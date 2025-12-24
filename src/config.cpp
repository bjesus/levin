#include "config.hpp"
#include <toml.hpp>
#include <stdexcept>
#include <algorithm>
#include <vector>
#include <cstdlib>
#include <cctype>

namespace levin {

namespace {
    /**
     * Parse a human-readable size string like "100mb", "5gb", "1tb" into bytes.
     * Supports: b, kb, mb, gb, tb (case-insensitive)
     * Binary units: 1KB = 1024 bytes, 1MB = 1024KB, etc.
     * 
     * @param size_str Size string (e.g., "100mb", "5GB", "1024")
     * @return Size in bytes
     * @throws std::runtime_error if format is invalid
     */
    uint64_t parse_size(const std::string& size_str) {
        if (size_str.empty()) {
            throw std::runtime_error("Empty size string");
        }
        
        // Find where the number ends and unit begins
        size_t pos = 0;
        while (pos < size_str.length() && 
               (std::isdigit(size_str[pos]) || size_str[pos] == '.' || size_str[pos] == ' ')) {
            pos++;
        }
        
        // Skip whitespace
        while (pos < size_str.length() && std::isspace(size_str[pos])) {
            pos++;
        }
        
        // Parse the numeric part
        double value;
        try {
            value = std::stod(size_str.substr(0, pos));
        } catch (...) {
            throw std::runtime_error("Invalid number in size string: " + size_str);
        }
        
        if (value < 0) {
            throw std::runtime_error("Size cannot be negative: " + size_str);
        }
        
        // Get the unit part (case-insensitive)
        std::string unit;
        if (pos < size_str.length()) {
            unit = size_str.substr(pos);
            std::transform(unit.begin(), unit.end(), unit.begin(), ::tolower);
        }
        
        // Calculate multiplier (binary: 1024-based)
        uint64_t multiplier = 1;
        if (unit.empty() || unit == "b") {
            multiplier = 1;
        } else if (unit == "kb") {
            multiplier = 1024ULL;
        } else if (unit == "mb") {
            multiplier = 1024ULL * 1024ULL;
        } else if (unit == "gb") {
            multiplier = 1024ULL * 1024ULL * 1024ULL;
        } else if (unit == "tb") {
            multiplier = 1024ULL * 1024ULL * 1024ULL * 1024ULL;
        } else {
            throw std::runtime_error("Unknown size unit: " + unit + " (valid: b, kb, mb, gb, tb)");
        }
        
        return static_cast<uint64_t>(value * multiplier);
    }
    
    std::string expand_path(const std::string& path) {
        std::string result = path;
        
        // Expand ~ to HOME
        if (!result.empty() && result[0] == '~') {
            const char* home = std::getenv("HOME");
            if (home) {
                result = std::string(home) + result.substr(1);
            }
        }
        
        // Expand $VAR and ${VAR}
        size_t pos = 0;
        while ((pos = result.find('$', pos)) != std::string::npos) {
            size_t end;
            std::string var_name;
            
            if (pos + 1 < result.length() && result[pos + 1] == '{') {
                end = result.find('}', pos + 2);
                if (end == std::string::npos) break;
                var_name = result.substr(pos + 2, end - pos - 2);
                end++; // include }
            } else {
                end = pos + 1;
                while (end < result.length() && 
                       (std::isalnum(result[end]) || result[end] == '_')) {
                    end++;
                }
                var_name = result.substr(pos + 1, end - pos - 1);
            }
            
            const char* value = std::getenv(var_name.c_str());
            if (value) {
                result.replace(pos, end - pos, value);
                pos += std::strlen(value);
            } else {
                // Variable not set, use XDG default
                const char* home = std::getenv("HOME");
                std::string default_val;
                
                if (var_name == "XDG_CONFIG_HOME" && home) {
                    default_val = std::string(home) + "/.config";
                } else if (var_name == "XDG_CACHE_HOME" && home) {
                    default_val = std::string(home) + "/.cache";
                } else if (var_name == "XDG_STATE_HOME" && home) {
                    default_val = std::string(home) + "/.local/state";
                } else {
                    pos = end;
                    continue;
                }
                
                result.replace(pos, end - pos, default_val);
                pos += default_val.length();
            }
        }
        
        return result;
    }
}

Config Config::load(const std::string& path) {
    Config config;

    try {
        // Parse TOML file
        const auto data = toml::parse(path);

        // Load daemon settings
        if (data.contains("daemon")) {
            const auto& daemon = toml::find(data, "daemon");
            config.daemon.pid_file = expand_path(toml::find<std::string>(daemon, "pid_file"));
            config.daemon.log_file = expand_path(toml::find<std::string>(daemon, "log_file"));
            config.daemon.log_level = toml::find<std::string>(daemon, "log_level");
            config.daemon.run_on_battery = daemon.contains("run_on_battery") ? 
                toml::find<bool>(daemon, "run_on_battery") : false;
        }

        // Load path settings
        if (data.contains("paths")) {
            const auto& paths = toml::find(data, "paths");
            config.paths.watch_directory = expand_path(toml::find<std::string>(paths, "watch_directory"));
            config.paths.data_directory = expand_path(toml::find<std::string>(paths, "data_directory"));
            config.paths.session_state = expand_path(toml::find<std::string>(paths, "session_state"));
            config.paths.statistics_file = expand_path(toml::find<std::string>(paths, "statistics_file"));
        }

        // Load disk settings
        if (data.contains("disk")) {
            const auto& disk = toml::find(data, "disk");
            
            // Support both old name (min_free_bytes) and new name (min_free_space)
            // Also support both integer and string (human-readable) formats
            if (disk.contains("min_free_space")) {
                const auto& value = disk.at("min_free_space");
                if (value.is_integer()) {
                    config.disk.min_free_bytes = value.as_integer();
                } else if (value.is_string()) {
                    config.disk.min_free_bytes = parse_size(value.as_string());
                } else {
                    throw std::runtime_error("min_free_space must be an integer or string");
                }
            } else if (disk.contains("min_free_bytes")) {
                // Backwards compatibility with old name
                const auto& value = disk.at("min_free_bytes");
                if (value.is_integer()) {
                    config.disk.min_free_bytes = value.as_integer();
                } else if (value.is_string()) {
                    config.disk.min_free_bytes = parse_size(value.as_string());
                } else {
                    throw std::runtime_error("min_free_bytes must be an integer or string");
                }
            } else {
                throw std::runtime_error("disk.min_free_space is required");
            }
            
            config.disk.min_free_percentage = toml::find<double>(disk, "min_free_percentage");
            config.disk.check_interval_seconds = toml::find<int>(disk, "check_interval_seconds");
        }

        // Load torrent settings
        if (data.contains("torrents")) {
            const auto& torrents = toml::find(data, "torrents");
            config.torrents.seeder_update_interval_minutes = 
                toml::find<int>(torrents, "seeder_update_interval_minutes");
            config.torrents.watch_directory_scan_interval_seconds = 
                toml::find<int>(torrents, "watch_directory_scan_interval_seconds");
            config.torrents.max_connections_per_torrent = 
                toml::find<int>(torrents, "max_connections_per_torrent");
            config.torrents.max_upload_slots_per_torrent = 
                toml::find<int>(torrents, "max_upload_slots_per_torrent");
        }

        // Load limits
        if (data.contains("limits")) {
            const auto& limits = toml::find(data, "limits");
            config.limits.max_download_rate_kbps = toml::find<int>(limits, "max_download_rate_kbps");
            config.limits.max_upload_rate_kbps = toml::find<int>(limits, "max_upload_rate_kbps");
            config.limits.max_total_connections = toml::find<int>(limits, "max_total_connections");
            config.limits.max_active_downloads = toml::find<int>(limits, "max_active_downloads");
            config.limits.max_active_seeds = toml::find<int>(limits, "max_active_seeds");
            config.limits.max_active_torrents = toml::find<int>(limits, "max_active_torrents");
        }

        // Load network settings
        if (data.contains("network")) {
            const auto& network = toml::find(data, "network");
            config.network.listen_port = toml::find<int>(network, "listen_port");
            config.network.enable_dht = toml::find<bool>(network, "enable_dht");
            config.network.enable_lsd = toml::find<bool>(network, "enable_lsd");
            config.network.enable_upnp = toml::find<bool>(network, "enable_upnp");
            config.network.enable_natpmp = toml::find<bool>(network, "enable_natpmp");
            config.network.enable_webrtc = toml::find<bool>(network, "enable_webrtc");
            config.network.webrtc_stun_server = toml::find<std::string>(network, "webrtc_stun_server");
        }

        // Load CLI settings
        if (data.contains("cli")) {
            const auto& cli = toml::find(data, "cli");
            config.cli.control_socket = expand_path(toml::find<std::string>(cli, "control_socket"));
        }

        // Load statistics settings
        if (data.contains("statistics")) {
            const auto& statistics = toml::find(data, "statistics");
            config.statistics.save_interval_minutes = toml::find<int>(statistics, "save_interval_minutes");
        }

    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to load configuration from '" + path + "': " + e.what());
    }

    return config;
}

bool Config::validate() const {
    // Validate daemon settings
    if (daemon.pid_file.empty()) {
        return false;
    }
    if (daemon.log_file.empty()) {
        return false;
    }

    // Validate log level
    const std::vector<std::string> valid_log_levels = {
        "trace", "debug", "info", "warn", "error", "critical"
    };
    if (std::find(valid_log_levels.begin(), valid_log_levels.end(), daemon.log_level) 
        == valid_log_levels.end()) {
        return false;
    }

    // Validate paths
    if (paths.watch_directory.empty() || paths.data_directory.empty()) {
        return false;
    }
    if (paths.session_state.empty() || paths.statistics_file.empty()) {
        return false;
    }

    // Validate disk settings
    if (disk.min_free_bytes == 0 && disk.min_free_percentage == 0.0) {
        return false; // Must have at least one minimum
    }
    if (disk.min_free_percentage < 0.0 || disk.min_free_percentage > 1.0) {
        return false;
    }
    if (disk.check_interval_seconds <= 0) {
        return false;
    }

    // Validate torrent settings
    if (torrents.seeder_update_interval_minutes <= 0) {
        return false;
    }
    if (torrents.watch_directory_scan_interval_seconds <= 0) {
        return false;
    }
    if (torrents.max_connections_per_torrent <= 0) {
        return false;
    }
    if (torrents.max_upload_slots_per_torrent <= 0) {
        return false;
    }

    // Validate limits (can be 0 for unlimited)
    if (limits.max_download_rate_kbps < 0 || limits.max_upload_rate_kbps < 0) {
        return false;
    }
    if (limits.max_total_connections <= 0) {
        return false;
    }
    if (limits.max_active_downloads <= 0) {
        return false;
    }
    // max_active_seeds can be -1 for unlimited
    if (limits.max_active_seeds < -1) {
        return false;
    }
    if (limits.max_active_torrents <= 0) {
        return false;
    }

    // Validate network settings
    if (network.listen_port <= 0 || network.listen_port > 65535) {
        return false;
    }
    if (network.enable_webrtc && network.webrtc_stun_server.empty()) {
        return false;
    }

    // Validate CLI settings
    if (cli.control_socket.empty()) {
        return false;
    }

    // Validate statistics settings
    if (statistics.save_interval_minutes <= 0) {
        return false;
    }

    return true;
}

uint64_t Config::get_effective_min_free_space(uint64_t total_disk_bytes) const {
    uint64_t absolute_min = disk.min_free_bytes;
    uint64_t percentage_min = static_cast<uint64_t>(total_disk_bytes * disk.min_free_percentage);
    return std::max(absolute_min, percentage_min);
}

} // namespace levin
