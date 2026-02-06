#include "annas_archive.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

#include <curl/curl.h>

namespace levin {

namespace {

constexpr int MAX_RETRIES = 3;
constexpr long TIMEOUT_SECONDS = 30;
constexpr const char* TORRENT_LIST_URL =
    "https://annas-archive.li/dyn/generate_torrents?max_tb=1&format=url";

// libcurl write callback: appends data to a std::string
size_t write_string_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    size_t total = size * nmemb;
    out->append(ptr, total);
    return total;
}

// libcurl write callback: writes data to a FILE*
size_t write_file_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* fp = static_cast<FILE*>(userdata);
    return fwrite(ptr, size, nmemb, fp);
}

// Extract filename from URL (last path component)
std::string filename_from_url(const std::string& url) {
    auto pos = url.find_last_of('/');
    if (pos == std::string::npos || pos + 1 >= url.size()) {
        return {};
    }
    std::string name = url.substr(pos + 1);
    // Strip query parameters if present
    auto qpos = name.find('?');
    if (qpos != std::string::npos) {
        name = name.substr(0, qpos);
    }
    return name;
}

// Perform an HTTP GET with retries and exponential backoff.
// On success, returns the response body. On failure, returns empty string and sets ok=false.
std::string http_get(const std::string& url, bool& ok) {
    ok = false;
    std::string response;

    for (int attempt = 0; attempt < MAX_RETRIES; ++attempt) {
        response.clear();

        CURL* curl = curl_easy_init();
        if (!curl) return {};

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_string_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, TIMEOUT_SECONDS);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "levin/0.1");

        CURLcode res = curl_easy_perform(curl);
        long http_code = 0;
        if (res == CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        }
        curl_easy_cleanup(curl);

        if (res == CURLE_OK && http_code >= 200 && http_code < 300) {
            ok = true;
            return response;
        }

        // Exponential backoff: 1s, 2s, 4s
        if (attempt + 1 < MAX_RETRIES) {
            int delay_sec = 1 << attempt;
            std::this_thread::sleep_for(std::chrono::seconds(delay_sec));
        }
    }

    return {};
}

} // anonymous namespace

std::vector<std::string> AnnaArchive::fetch_torrent_urls() {
    std::vector<std::string> urls;

    bool ok = false;
    std::string body = http_get(TORRENT_LIST_URL, ok);
    if (!ok) return urls;

    std::istringstream stream(body);
    std::string line;
    while (std::getline(stream, line)) {
        // Trim trailing \r if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        // Skip empty lines
        if (!line.empty()) {
            urls.push_back(std::move(line));
        }
    }

    return urls;
}

bool AnnaArchive::download_file(const std::string& url, const std::string& dest_path) {
    for (int attempt = 0; attempt < MAX_RETRIES; ++attempt) {
        FILE* fp = fopen(dest_path.c_str(), "wb");
        if (!fp) return false;

        CURL* curl = curl_easy_init();
        if (!curl) {
            fclose(fp);
            return false;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, TIMEOUT_SECONDS);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "levin/0.1");

        CURLcode res = curl_easy_perform(curl);
        long http_code = 0;
        if (res == CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        }
        curl_easy_cleanup(curl);
        fclose(fp);

        if (res == CURLE_OK && http_code >= 200 && http_code < 300) {
            return true;
        }

        // Remove partial download
        std::error_code ec;
        std::filesystem::remove(dest_path, ec);

        // Exponential backoff: 1s, 2s, 4s
        if (attempt + 1 < MAX_RETRIES) {
            int delay_sec = 1 << attempt;
            std::this_thread::sleep_for(std::chrono::seconds(delay_sec));
        }
    }

    return false;
}

int AnnaArchive::populate_torrents(const std::string& watch_directory,
                                   ProgressCallback progress_cb) {
    namespace fs = std::filesystem;

    // Ensure the watch directory exists
    std::error_code ec;
    fs::create_directories(watch_directory, ec);
    if (ec) return -1;

    // Fetch the list of torrent URLs
    auto urls = fetch_torrent_urls();
    if (urls.empty()) return -1;

    int total = static_cast<int>(urls.size());
    int downloaded = 0;

    for (int i = 0; i < total; ++i) {
        const auto& url = urls[i];
        std::string filename = filename_from_url(url);
        if (filename.empty()) continue;

        std::string dest_path = watch_directory + "/" + filename;

        // Skip if file already exists
        if (fs::exists(dest_path, ec)) {
            if (progress_cb) {
                progress_cb(i + 1, total, "skipped (exists): " + filename);
            }
            continue;
        }

        if (progress_cb) {
            progress_cb(i + 1, total, "downloading: " + filename);
        }

        if (download_file(url, dest_path)) {
            ++downloaded;
        } else {
            if (progress_cb) {
                progress_cb(i + 1, total, "failed: " + filename);
            }
        }
    }

    return downloaded;
}

} // namespace levin
