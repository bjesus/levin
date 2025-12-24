#pragma once

#include <string>
#include <functional>
#include <vector>

namespace levin {

struct Config;

/**
 * Watches a directory for .torrent files using inotify (Linux).
 * Notifies when new files are added or removed.
 */
class TorrentWatcher {
public:
    // Callback types
    using TorrentAddedCallback = std::function<void(const std::string& path)>;
    using TorrentRemovedCallback = std::function<void(const std::string& path)>;

    explicit TorrentWatcher(const Config& config);
    ~TorrentWatcher();

    /**
     * Set callback for when a torrent file is added.
     */
    void set_torrent_added_callback(TorrentAddedCallback callback);

    /**
     * Set callback for when a torrent file is removed.
     */
    void set_torrent_removed_callback(TorrentRemovedCallback callback);

    /**
     * Initialize inotify and start watching.
     */
    bool start();

    /**
     * Stop watching.
     */
    void stop();

    /**
     * Process pending inotify events (non-blocking).
     * Call this regularly from the main loop.
     */
    void process_events();

    /**
     * Scan directory for existing .torrent files.
     * Useful for initial load.
     */
    std::vector<std::string> scan_existing_torrents();

    /**
     * Check if a filename ends with .torrent
     */
    static bool is_torrent_file(const std::string& filename);

private:
    const Config& config_;
    std::string watch_directory_;
    int inotify_fd_;
    int watch_descriptor_;
    bool running_;

    TorrentAddedCallback torrent_added_callback_;
    TorrentRemovedCallback torrent_removed_callback_;
};

} // namespace levin
