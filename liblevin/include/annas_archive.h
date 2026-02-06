#pragma once

#include <functional>
#include <string>
#include <vector>

namespace levin {

using ProgressCallback = std::function<void(int current, int total, const std::string& message)>;

class AnnaArchive {
public:
    // Fetch torrent URLs from Anna's Archive and download .torrent files
    // to the given directory. Calls progress_cb for each file downloaded.
    // Returns number of torrents downloaded, or -1 on error.
    static int populate_torrents(const std::string& watch_directory,
                                 ProgressCallback progress_cb = nullptr);

private:
    // Fetch the list of torrent URLs from the API
    static std::vector<std::string> fetch_torrent_urls();

    // Download a single file from URL to the given path.
    // Returns true on success.
    static bool download_file(const std::string& url, const std::string& dest_path);
};

} // namespace levin
