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
#elif defined(__APPLE__)
#include <CoreServices/CoreServices.h>
#include <dispatch/dispatch.h>
#include <mutex>
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

#elif defined(__APPLE__) // macOS: FSEvents

// FSEvents delivers events asynchronously on a dispatch queue.  We accumulate
// them in a thread-safe queue and drain during poll() so that callbacks
// always fire on the levin tick thread (matching the single-thread contract).

struct FsEvent {
    std::string path;
    bool is_remove;
};

// Shared state between the FSEvents callback and TorrentWatcher.
// Defined at namespace scope so the free callback function can access it.
struct FsWatcherState {
    std::mutex mu;
    std::vector<FsEvent> pending;
};

struct TorrentWatcher::Impl {
    std::string directory;
    TorrentAddedCallback on_add;
    TorrentRemovedCallback on_remove;

    FSEventStreamRef stream = nullptr;
    dispatch_queue_t queue = nullptr;

    FsWatcherState shared;
};

// FSEvents callback — runs on the dispatch queue, NOT the tick thread.
static void fsevents_callback(
        ConstFSEventStreamRef /*stream*/,
        void* context,
        size_t count,
        void* event_paths,
        const FSEventStreamEventFlags flags[],
        const FSEventStreamEventId /*ids*/[]) {
    auto* state = static_cast<FsWatcherState*>(context);
    auto** paths = static_cast<char**>(event_paths);

    std::lock_guard<std::mutex> lock(state->mu);
    for (size_t i = 0; i < count; ++i) {
        std::string p(paths[i]);
        if (!has_torrent_extension(p)) continue;

        bool removed = (flags[i] & kFSEventStreamEventFlagItemRemoved) != 0;
        bool created = (flags[i] & kFSEventStreamEventFlagItemCreated) != 0;
        bool renamed = (flags[i] & kFSEventStreamEventFlagItemRenamed) != 0;
        bool modified = (flags[i] & kFSEventStreamEventFlagItemModified) != 0;

        if (removed) {
            state->pending.push_back({std::move(p), true});
        } else if (created || renamed || modified) {
            // Check that the file actually exists (renames can mean source or dest)
            namespace fs = std::filesystem;
            std::error_code ec;
            if (fs::exists(p, ec) && fs::is_regular_file(p, ec)) {
                state->pending.push_back({std::move(p), false});
            }
        }
    }
}

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

    CFStringRef path = CFStringCreateWithCString(
        kCFAllocatorDefault, directory.c_str(), kCFStringEncodingUTF8);
    if (!path) return -1;

    CFArrayRef paths = CFArrayCreate(
        kCFAllocatorDefault, (const void**)&path, 1, &kCFTypeArrayCallBacks);
    CFRelease(path);
    if (!paths) return -1;

    FSEventStreamContext ctx{};
    ctx.info = &impl_->shared;

    impl_->stream = FSEventStreamCreate(
        kCFAllocatorDefault,
        fsevents_callback,
        &ctx,
        paths,
        kFSEventStreamEventIdSinceNow,
        0.3,  // 300ms latency — events batched for efficiency
        kFSEventStreamCreateFlagFileEvents | kFSEventStreamCreateFlagNoDefer);

    CFRelease(paths);
    if (!impl_->stream) return -1;

    impl_->queue = dispatch_queue_create("com.yoavmoshe.levin.fswatcher", DISPATCH_QUEUE_SERIAL);
    FSEventStreamSetDispatchQueue(impl_->stream, impl_->queue);
    FSEventStreamStart(impl_->stream);

    return 0;
}

void TorrentWatcher::stop() {
    if (impl_->stream) {
        FSEventStreamStop(impl_->stream);
        FSEventStreamInvalidate(impl_->stream);
        FSEventStreamRelease(impl_->stream);
        impl_->stream = nullptr;
    }
    if (impl_->queue) {
        dispatch_release(impl_->queue);
        impl_->queue = nullptr;
    }
}

void TorrentWatcher::poll() {
    // Drain pending events accumulated by the FSEvents callback.
    std::vector<FsEvent> events;
    {
        std::lock_guard<std::mutex> lock(impl_->shared.mu);
        events.swap(impl_->shared.pending);
    }

    for (const auto& ev : events) {
        if (ev.is_remove) {
            if (impl_->on_remove) impl_->on_remove(ev.path);
        } else {
            if (impl_->on_add) impl_->on_add(ev.path);
        }
    }
}

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

#else // Fallback (no-op) for platforms without file watching

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

#endif

} // namespace levin
