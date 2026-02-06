#include "torrent_watcher.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#ifdef __linux__
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/inotify.h>
#include <unistd.h>
#endif

namespace levin {

namespace {

bool has_torrent_extension(const std::string& path) {
    const std::string ext = ".torrent";
    if (path.size() < ext.size()) return false;
    return path.compare(path.size() - ext.size(), ext.size(), ext) == 0;
}

// Extract filename from a full path
std::string filename_from_path(const std::string& path) {
    auto pos = path.find_last_of('/');
    if (pos == std::string::npos) return path;
    return path.substr(pos + 1);
}

} // anonymous namespace

#ifdef __linux__

struct TorrentWatcher::Impl {
    int inotify_fd = -1;
    int watch_fd = -1;
    std::string directory;
    TorrentAddedCallback on_add;
    TorrentRemovedCallback on_remove;
};

TorrentWatcher::TorrentWatcher() : impl_(std::make_unique<Impl>()) {}

TorrentWatcher::~TorrentWatcher() {
    stop();
}

void TorrentWatcher::set_callbacks(TorrentAddedCallback on_add, TorrentRemovedCallback on_remove) {
    impl_->on_add = std::move(on_add);
    impl_->on_remove = std::move(on_remove);
}

int TorrentWatcher::start(const std::string& directory) {
    stop();

    impl_->directory = directory;

    impl_->inotify_fd = inotify_init();
    if (impl_->inotify_fd < 0) {
        return -1;
    }

    // Set non-blocking
    int flags = fcntl(impl_->inotify_fd, F_GETFL, 0);
    if (flags < 0 || fcntl(impl_->inotify_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        close(impl_->inotify_fd);
        impl_->inotify_fd = -1;
        return -1;
    }

    uint32_t mask = IN_CREATE | IN_CLOSE_WRITE | IN_MOVED_TO | IN_DELETE | IN_MOVED_FROM;
    impl_->watch_fd = inotify_add_watch(impl_->inotify_fd, directory.c_str(), mask);
    if (impl_->watch_fd < 0) {
        close(impl_->inotify_fd);
        impl_->inotify_fd = -1;
        return -1;
    }

    return 0;
}

void TorrentWatcher::stop() {
    if (impl_->inotify_fd >= 0) {
        if (impl_->watch_fd >= 0) {
            inotify_rm_watch(impl_->inotify_fd, impl_->watch_fd);
            impl_->watch_fd = -1;
        }
        close(impl_->inotify_fd);
        impl_->inotify_fd = -1;
    }
}

void TorrentWatcher::poll() {
    if (impl_->inotify_fd < 0) return;

    // Buffer large enough for several events
    alignas(struct inotify_event) char buf[4096];

    for (;;) {
        ssize_t len = read(impl_->inotify_fd, buf, sizeof(buf));
        if (len <= 0) {
            // EAGAIN means no more events (non-blocking)
            break;
        }

        const char* ptr = buf;
        while (ptr < buf + len) {
            const auto* event = reinterpret_cast<const struct inotify_event*>(ptr);

            if (event->len > 0) {
                std::string name(event->name);
                if (has_torrent_extension(name)) {
                    std::string full_path = impl_->directory + "/" + name;

                    if ((event->mask & (IN_CLOSE_WRITE | IN_MOVED_TO)) && impl_->on_add) {
                        impl_->on_add(full_path);
                    }
                    if ((event->mask & (IN_DELETE | IN_MOVED_FROM)) && impl_->on_remove) {
                        impl_->on_remove(full_path);
                    }
                }
            }

            ptr += sizeof(struct inotify_event) + event->len;
        }
    }
}

void TorrentWatcher::scan_existing() {
    if (impl_->directory.empty()) return;

    namespace fs = std::filesystem;
    std::error_code ec;

    // Collect and sort for deterministic ordering
    std::vector<std::string> paths;
    for (const auto& entry : fs::directory_iterator(impl_->directory, ec)) {
        if (ec) break;
        if (entry.is_regular_file() && has_torrent_extension(entry.path().string())) {
            paths.push_back(entry.path().string());
        }
    }

    std::sort(paths.begin(), paths.end());

    for (const auto& path : paths) {
        if (impl_->on_add) {
            impl_->on_add(path);
        }
    }
}

#else // Non-Linux fallback (no-op)

struct TorrentWatcher::Impl {
    std::string directory;
    TorrentAddedCallback on_add;
    TorrentRemovedCallback on_remove;
};

TorrentWatcher::TorrentWatcher() : impl_(std::make_unique<Impl>()) {}

TorrentWatcher::~TorrentWatcher() {
    stop();
}

void TorrentWatcher::set_callbacks(TorrentAddedCallback on_add, TorrentRemovedCallback on_remove) {
    impl_->on_add = std::move(on_add);
    impl_->on_remove = std::move(on_remove);
}

int TorrentWatcher::start(const std::string& directory) {
    impl_->directory = directory;
    return 0;
}

void TorrentWatcher::stop() {}

void TorrentWatcher::poll() {}

void TorrentWatcher::scan_existing() {
    if (impl_->directory.empty()) return;

    namespace fs = std::filesystem;
    std::error_code ec;

    std::vector<std::string> paths;
    for (const auto& entry : fs::directory_iterator(impl_->directory, ec)) {
        if (ec) break;
        if (entry.is_regular_file() && has_torrent_extension(entry.path().string())) {
            paths.push_back(entry.path().string());
        }
    }

    std::sort(paths.begin(), paths.end());

    for (const auto& path : paths) {
        if (impl_->on_add) {
            impl_->on_add(path);
        }
    }
}

#endif // __linux__

} // namespace levin
