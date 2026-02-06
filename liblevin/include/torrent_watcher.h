#pragma once

#include <functional>
#include <memory>
#include <string>

namespace levin {

// Callback types
using TorrentAddedCallback = std::function<void(const std::string& path)>;
using TorrentRemovedCallback = std::function<void(const std::string& path)>;

class TorrentWatcher {
public:
    TorrentWatcher();
    ~TorrentWatcher();

    // Non-copyable
    TorrentWatcher(const TorrentWatcher&) = delete;
    TorrentWatcher& operator=(const TorrentWatcher&) = delete;

    void set_callbacks(TorrentAddedCallback on_add, TorrentRemovedCallback on_remove);

    // Start watching directory. Returns 0 on success.
    int start(const std::string& directory);
    void stop();

    // Call this periodically (e.g. from tick) to process pending events.
    // This is non-blocking.
    void poll();

    // Scan the directory for existing .torrent files and call on_add for each.
    void scan_existing();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace levin
