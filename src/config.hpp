#pragma once

#include <string>
#include <cstdint>

namespace levin {

/**
 * Configuration structure for the Levin daemon.
 * Loads settings from TOML configuration file.
 * 
 * Simplified config - most settings have sensible defaults.
 * Only required settings: watch_directory, data_directory, min_free
 */
struct Config {
    // Paths (REQUIRED: watch_directory, data_directory)
    struct {
        std::string watch_directory;    // Directory to watch for .torrent files
        std::string data_directory;     // Directory to store downloaded data
        std::string state_directory;    // Directory for state files (pid, log, socket, etc.)
                                        // Default: $XDG_STATE_HOME/levin/
    } paths;

    // Disk management settings (REQUIRED: min_free)
    struct {
        uint64_t min_free;              // Minimum free space in bytes
        double min_free_percentage;     // Minimum free space as percentage (default: 0.05)
        uint64_t max_storage;           // Maximum storage Levin can use (0 = unlimited)
    } disk;

    // Daemon settings (all have defaults)
    struct {
        std::string log_level;          // Default: "info"
        bool run_on_battery;            // Default: false
    } daemon;

    // Bandwidth limits (all optional, 0 = unlimited)
    struct {
        int max_download_rate_kbps;     // Default: 0 (unlimited)
        int max_upload_rate_kbps;       // Default: 0 (unlimited)
    } limits;
    
    // WebTorrent settings (all optional)
    struct {
        std::string stun_server;        // Default: "stun.l.google.com:19302" (Google)
    } webtorrent;

    // Derived paths (computed from state_directory)
    std::string pid_file() const;
    std::string log_file() const;
    std::string control_socket() const;
    std::string session_state() const;
    std::string statistics_file() const;

    /**
     * Load configuration from a TOML file.
     * 
     * @param path Path to the TOML configuration file
     * @return Config object with loaded settings
     * @throws std::runtime_error if file cannot be read or parsed
     */
    static Config load(const std::string& path);

    /**
     * Validate configuration values.
     * 
     * @return true if configuration is valid, false otherwise
     */
    bool validate() const;

    /**
     * Get the effective minimum free space for a given total disk space.
     * Returns the maximum of absolute minimum and percentage-based minimum.
     * 
     * @param total_disk_bytes Total disk space in bytes
     * @return Effective minimum free space in bytes
     */
    uint64_t get_effective_min_free_space(uint64_t total_disk_bytes) const;
    
    // =========================================================================
    // Hardcoded constants (no config needed)
    // =========================================================================
    
    // Network settings - always enabled for best connectivity
    static constexpr int listen_port = 6881;
    static constexpr bool enable_dht = true;
    static constexpr bool enable_lsd = true;
    static constexpr bool enable_upnp = true;
    static constexpr bool enable_natpmp = true;
    
    // Torrent settings - sensible defaults
    static constexpr int max_connections_per_torrent = 50;
    static constexpr int max_upload_slots_per_torrent = 8;
    static constexpr int max_total_connections = 200;
    
    // All torrents active - no artificial limits
    static constexpr int max_active_downloads = -1;  // unlimited
    static constexpr int max_active_seeds = -1;      // unlimited
    static constexpr int max_active_torrents = -1;   // unlimited
    
    // Timing intervals
    static constexpr int disk_check_interval_seconds = 60;
    static constexpr int seeder_update_interval_minutes = 60;
    static constexpr int watch_scan_interval_seconds = 30;
    static constexpr int statistics_save_interval_minutes = 5;
};

} // namespace levin
