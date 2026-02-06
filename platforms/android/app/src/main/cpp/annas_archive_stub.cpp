/**
 * Stub implementation of AnnaArchive for Android builds without libcurl.
 *
 * Replaces liblevin/src/annas_archive.cpp which depends on libcurl.
 * When a cross-compiled libcurl is available for Android, this stub
 * should be removed and the real implementation used instead.
 */
#include "annas_archive.h"

namespace levin {

std::vector<std::string> AnnaArchive::fetch_torrent_urls() {
    return {};
}

bool AnnaArchive::download_file(const std::string& /* url */,
                                const std::string& /* dest_path */) {
    return false;
}

int AnnaArchive::populate_torrents(const std::string& /* watch_directory */,
                                   ProgressCallback /* progress_cb */) {
    return -1;
}

} // namespace levin
