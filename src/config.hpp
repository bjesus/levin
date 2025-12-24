#pragma once

#include <string>
#include <cstdint>

namespace levin {

/**
 * Configuration structure for the Levin daemon.
 * Loads settings from TOML configuration file.
 */
struct Config {
    // Daemon settings
    struct {
        std::string pid_file;
        std::string log_file;
        std::string log_level;
    } daemon;

    // Path settings
    struct {
        std::string watch_directory;
        std::string data_directory;
        std::string session_state;
        std::string statistics_file;
    } paths;

    // Disk management settings
    struct {
        uint64_t min_free_bytes;
        double min_free_percentage;
        int check_interval_seconds;
    } disk;

    // Torrent settings
    struct {
        int seeder_update_interval_minutes;
        int watch_directory_scan_interval_seconds;
        int max_connections_per_torrent;
        int max_upload_slots_per_torrent;
    } torrents;

    // Bandwidth and connection limits
    struct {
        int max_download_rate_kbps;
        int max_upload_rate_kbps;
        int max_total_connections;
        int max_active_downloads;
        int max_active_seeds;
        int max_active_torrents;
    } limits;

    // Network settings
    struct {
        int listen_port;
        bool enable_dht;
        bool enable_lsd;
        bool enable_upnp;
        bool enable_natpmp;
        bool enable_webrtc;
        std::string webrtc_stun_server;
    } network;

    // CLI settings
    struct {
        std::string control_socket;
    } cli;

    // Statistics settings
    struct {
        int save_interval_minutes;
    } statistics;

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
};

} // namespace levin
