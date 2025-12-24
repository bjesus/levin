#include "torrent_watcher.hpp"
#include "config.hpp"
#include "logger.hpp"
#include "utils.hpp"
#include <unistd.h>
#include <dirent.h>
#include <cstring>

#ifdef __linux__
#include <sys/inotify.h>
#define EVENT_SIZE (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (1024 * (EVENT_SIZE + 16))
#endif

namespace levin {

TorrentWatcher::TorrentWatcher(const Config& config)
    : config_(config)
    , watch_directory_(config.paths.watch_directory)
    , inotify_fd_(-1)
    , watch_descriptor_(-1)
    , running_(false) {
}

TorrentWatcher::~TorrentWatcher() {
    stop();
}

void TorrentWatcher::set_torrent_added_callback(TorrentAddedCallback callback) {
    torrent_added_callback_ = callback;
}

void TorrentWatcher::set_torrent_removed_callback(TorrentRemovedCallback callback) {
    torrent_removed_callback_ = callback;
}

bool TorrentWatcher::start() {
    LOG_INFO("Starting TorrentWatcher for directory: {}", watch_directory_);

    // Check if directory exists
    if (!utils::directory_exists(watch_directory_)) {
        LOG_ERROR("Watch directory does not exist: {}", watch_directory_);
        return false;
    }

#ifdef __linux__
    // Initialize inotify
    inotify_fd_ = inotify_init1(IN_NONBLOCK);
    if (inotify_fd_ < 0) {
        LOG_ERROR("Failed to initialize inotify: {}", std::strerror(errno));
        return false;
    }

    // Add watch for directory
    watch_descriptor_ = inotify_add_watch(inotify_fd_, watch_directory_.c_str(),
                                          IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM);
    if (watch_descriptor_ < 0) {
        LOG_ERROR("Failed to add inotify watch: {}", std::strerror(errno));
        close(inotify_fd_);
        inotify_fd_ = -1;
        return false;
    }
#else
    // On non-Linux platforms, use polling instead of inotify
    LOG_WARN("File system notifications not available on this platform, using polling");
#endif

    running_ = true;
    LOG_INFO("TorrentWatcher started successfully");
    return true;
}

void TorrentWatcher::stop() {
    if (running_) {
        LOG_INFO("Stopping TorrentWatcher");
        running_ = false;

#ifdef __linux__
        if (watch_descriptor_ >= 0) {
            inotify_rm_watch(inotify_fd_, watch_descriptor_);
            watch_descriptor_ = -1;
        }

        if (inotify_fd_ >= 0) {
            close(inotify_fd_);
            inotify_fd_ = -1;
        }
#endif
    }
}

void TorrentWatcher::process_events() {
    if (!running_) {
        return;
    }

#ifdef __linux__
    if (inotify_fd_ < 0) {
        return;
    }

    char buffer[EVENT_BUF_LEN];
    ssize_t length = read(inotify_fd_, buffer, EVENT_BUF_LEN);

    if (length < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            LOG_ERROR("Error reading inotify events: {}", std::strerror(errno));
        }
        return;
    }

    // Process all events in the buffer
    ssize_t i = 0;
    while (i < length) {
        struct inotify_event* event = (struct inotify_event*)&buffer[i];

        if (event->len > 0) {
            std::string filename = event->name;

            // Only process .torrent files
            if (!is_torrent_file(filename)) {
                i += EVENT_SIZE + event->len;
                continue;
            }

            std::string full_path = watch_directory_ + "/" + filename;

            // File created or moved into directory
            if (event->mask & (IN_CREATE | IN_MOVED_TO)) {
                LOG_INFO("Torrent file added: {}", filename);
                if (torrent_added_callback_) {
                    torrent_added_callback_(full_path);
                }
            }

            // File deleted or moved out of directory
            if (event->mask & (IN_DELETE | IN_MOVED_FROM)) {
                LOG_INFO("Torrent file removed: {}", filename);
                if (torrent_removed_callback_) {
                    torrent_removed_callback_(full_path);
                }
            }
        }

        i += EVENT_SIZE + event->len;
    }
#else
    // On non-Linux platforms, process_events() is a no-op
    // The daemon will rely on periodic scanning via scan_existing_torrents()
#endif
}

std::vector<std::string> TorrentWatcher::scan_existing_torrents() {
    std::vector<std::string> torrents;

    DIR* dir = opendir(watch_directory_.c_str());
    if (!dir) {
        LOG_ERROR("Failed to open watch directory: {}", watch_directory_);
        return torrents;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string filename = entry->d_name;

        // Skip . and ..
        if (filename == "." || filename == "..") {
            continue;
        }

        // Only process .torrent files
        if (is_torrent_file(filename)) {
            std::string full_path = watch_directory_ + "/" + filename;
            torrents.push_back(full_path);
            LOG_DEBUG("Found existing torrent: {}", filename);
        }
    }

    closedir(dir);

    LOG_INFO("Found {} existing torrent files", torrents.size());
    return torrents;
}

bool TorrentWatcher::is_torrent_file(const std::string& filename) {
    if (filename.length() < 8) {  // Minimum: "a.torrent"
        return false;
    }

    return filename.substr(filename.length() - 8) == ".torrent";
}

} // namespace levin
