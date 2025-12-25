#pragma once

#include <string>
#include <vector>
#include <functional>

namespace levin {
namespace annas_archive {

/**
 * Result of torrent download operation
 */
struct DownloadResult {
    int total_urls;        // Total number of torrent URLs
    int successful;        // Number successfully downloaded
    int failed;            // Number that failed
    std::vector<std::string> failed_urls;  // URLs that failed to download
};

/**
 * Fetch list of torrent URLs from Anna's Archive.
 * 
 * @param max_retries Maximum number of retry attempts (default: 3)
 * @return Vector of torrent URLs
 * @throws std::runtime_error if fetch fails after all retries
 */
std::vector<std::string> fetch_torrent_urls(int max_retries = 3);

/**
 * Download a single torrent file with retry logic.
 * 
 * @param url URL of the torrent file
 * @param output_path Path where to save the downloaded torrent file
 * @param max_retries Maximum number of retry attempts (default: 3)
 * @return true if download succeeded, false if failed after all retries
 */
bool download_torrent(
    const std::string& url, 
    const std::string& output_path,
    int max_retries = 3
);

/**
 * Populate watch directory with torrents from Anna's Archive.
 * 
 * @param watch_directory Directory where to save torrent files
 * @param progress_callback Optional callback for progress updates (current, total)
 * @return DownloadResult with statistics
 * @throws std::runtime_error if unable to fetch torrent list or create directory
 */
DownloadResult populate_torrents(
    const std::string& watch_directory,
    std::function<void(int current, int total)> progress_callback = nullptr
);

/**
 * Interactive prompt asking user if they want to populate torrents.
 * Reads from stdin.
 * 
 * @return true if user wants to populate, false otherwise
 */
bool prompt_user_to_populate();

} // namespace annas_archive
} // namespace levin
