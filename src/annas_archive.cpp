#include "annas_archive.hpp"
#include "logger.hpp"
#include "utils.hpp"
#include <curl/curl.h>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <algorithm>

namespace levin {
namespace annas_archive {

namespace {
    const char* ANNAS_ARCHIVE_URL = "https://annas-archive.org/dyn/generate_torrents?max_tb=1&format=url";
    const int INITIAL_BACKOFF_MS = 1000;  // 1 second
    const int TIMEOUT_SECONDS = 30;       // 30 second timeout per request

    /**
     * Curl write callback for capturing response data
     */
    size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
        size_t total_size = size * nmemb;
        std::string* response = static_cast<std::string*>(userp);
        response->append(static_cast<char*>(contents), total_size);
        return total_size;
    }

    /**
     * Curl write callback for writing to file
     */
    size_t write_file_callback(void* contents, size_t size, size_t nmemb, void* userp) {
        size_t total_size = size * nmemb;
        std::ofstream* file = static_cast<std::ofstream*>(userp);
        file->write(static_cast<char*>(contents), total_size);
        return total_size;
    }

    /**
     * Perform HTTP GET request with curl
     */
    bool http_get(const std::string& url, std::string& response, std::string& error) {
        CURL* curl = curl_easy_init();
        if (!curl) {
            error = "Failed to initialize CURL";
            return false;
        }

        response.clear();
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, TIMEOUT_SECONDS);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);  // Follow redirects
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Levin/" PROJECT_VERSION);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

        CURLcode res = curl_easy_perform(curl);
        
        if (res != CURLE_OK) {
            error = std::string("CURL error: ") + curl_easy_strerror(res);
            curl_easy_cleanup(curl);
            return false;
        }

        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        curl_easy_cleanup(curl);

        if (http_code != 200) {
            error = "HTTP error: " + std::to_string(http_code);
            return false;
        }

        return true;
    }

    /**
     * Download file from URL to path
     */
    bool http_download_file(const std::string& url, const std::string& path, std::string& error) {
        CURL* curl = curl_easy_init();
        if (!curl) {
            error = "Failed to initialize CURL";
            return false;
        }

        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) {
            error = "Failed to open output file: " + path;
            curl_easy_cleanup(curl);
            return false;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, TIMEOUT_SECONDS);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Levin/" PROJECT_VERSION);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

        CURLcode res = curl_easy_perform(curl);
        
        file.close();

        if (res != CURLE_OK) {
            error = std::string("CURL error: ") + curl_easy_strerror(res);
            curl_easy_cleanup(curl);
            // Remove partial file
            std::filesystem::remove(path);
            return false;
        }

        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        curl_easy_cleanup(curl);

        if (http_code != 200) {
            error = "HTTP error: " + std::to_string(http_code);
            // Remove partial file
            std::filesystem::remove(path);
            return false;
        }

        return true;
    }

    /**
     * Extract filename from URL (last path component)
     */
    std::string extract_filename(const std::string& url) {
        size_t last_slash = url.find_last_of('/');
        if (last_slash != std::string::npos && last_slash + 1 < url.length()) {
            return url.substr(last_slash + 1);
        }
        // Fallback: generate filename from hash
        return "torrent_" + std::to_string(std::hash<std::string>{}(url)) + ".torrent";
    }

    /**
     * Split string by newlines
     */
    std::vector<std::string> split_lines(const std::string& text) {
        std::vector<std::string> lines;
        std::istringstream stream(text);
        std::string line;
        
        while (std::getline(stream, line)) {
            // Trim whitespace
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);
            
            if (!line.empty()) {
                lines.push_back(line);
            }
        }
        
        return lines;
    }

} // anonymous namespace

std::vector<std::string> fetch_torrent_urls(int max_retries) {
    LOG_INFO("Fetching torrent list from Anna's Archive");
    
    std::string response;
    std::string error;
    int backoff_ms = INITIAL_BACKOFF_MS;
    
    for (int attempt = 1; attempt <= max_retries; attempt++) {
        if (attempt > 1) {
            LOG_WARN("Retry attempt {}/{} after {}ms", attempt, max_retries, backoff_ms);
            std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms));
            backoff_ms *= 2;  // Exponential backoff
        }
        
        if (http_get(ANNAS_ARCHIVE_URL, response, error)) {
            // Success!
            auto urls = split_lines(response);
            LOG_INFO("Fetched {} torrent URLs from Anna's Archive", urls.size());
            return urls;
        }
        
        LOG_WARN("Failed to fetch torrent list (attempt {}/{}): {}", 
                 attempt, max_retries, error);
    }
    
    throw std::runtime_error("Failed to fetch torrent list from Anna's Archive after " + 
                           std::to_string(max_retries) + " attempts: " + error);
}

bool download_torrent(const std::string& url, const std::string& output_path, int max_retries) {
    std::string error;
    int backoff_ms = INITIAL_BACKOFF_MS;
    
    for (int attempt = 1; attempt <= max_retries; attempt++) {
        if (attempt > 1) {
            LOG_DEBUG("Retry downloading {} (attempt {}/{})", url, attempt, max_retries);
            std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms));
            backoff_ms *= 2;  // Exponential backoff
        }
        
        if (http_download_file(url, output_path, error)) {
            return true;
        }
        
        LOG_DEBUG("Failed to download {} (attempt {}/{}): {}", 
                  url, attempt, max_retries, error);
    }
    
    LOG_ERROR("Failed to download {} after {} attempts: {}", url, max_retries, error);
    return false;
}

DownloadResult populate_torrents(
    const std::string& watch_directory,
    std::function<void(int current, int total)> progress_callback
) {
    // Ensure watch directory exists
    if (!utils::ensure_directory(watch_directory)) {
        throw std::runtime_error("Failed to create watch directory: " + watch_directory);
    }
    
    // Fetch list of torrent URLs
    std::vector<std::string> urls = fetch_torrent_urls();
    
    DownloadResult result;
    result.total_urls = urls.size();
    result.successful = 0;
    result.failed = 0;
    
    LOG_INFO("Downloading {} torrents to {}", urls.size(), watch_directory);
    
    // Download each torrent
    for (size_t i = 0; i < urls.size(); i++) {
        const auto& url = urls[i];
        
        // Extract filename from URL
        std::string filename = extract_filename(url);
        std::filesystem::path output_path = std::filesystem::path(watch_directory) / filename;
        
        // Skip if already exists
        if (std::filesystem::exists(output_path)) {
            LOG_DEBUG("Torrent already exists, skipping: {}", filename);
            result.successful++;
            if (progress_callback) {
                progress_callback(i + 1, urls.size());
            }
            continue;
        }
        
        // Download
        if (download_torrent(url, output_path.string())) {
            result.successful++;
        } else {
            result.failed++;
            result.failed_urls.push_back(url);
        }
        
        // Progress callback
        if (progress_callback) {
            progress_callback(i + 1, urls.size());
        }
    }
    
    LOG_INFO("Download complete: {}/{} successful, {} failed", 
             result.successful, result.total_urls, result.failed);
    
    return result;
}

bool prompt_user_to_populate() {
    std::cout << "\nWould you like to automatically add torrents from Anna's Archive? (y/n): " << std::flush;
    
    std::string response;
    std::getline(std::cin, response);
    
    // Trim whitespace
    response.erase(0, response.find_first_not_of(" \t\r\n"));
    response.erase(response.find_last_not_of(" \t\r\n") + 1);
    
    // Convert to lowercase for comparison
    std::transform(response.begin(), response.end(), response.begin(), ::tolower);
    
    return (response == "y" || response == "yes");
}

} // namespace annas_archive
} // namespace levin
