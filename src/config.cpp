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
    
    std::string get_default_state_directory() {
        const char* state_home = std::getenv("XDG_STATE_HOME");
        if (state_home) {
            return std::string(state_home) + "/levin";
        }
        const char* home = std::getenv("HOME");
        if (home) {
            return std::string(home) + "/.local/state/levin";
        }
        return "/tmp/levin";
    }
}

// Derived path implementations
std::string Config::pid_file() const {
    return paths.state_directory + "/levin.pid";
}

std::string Config::log_file() const {
    return paths.state_directory + "/levin.log";
}

std::string Config::control_socket() const {
    return paths.state_directory + "/levin.sock";
}

std::string Config::session_state() const {
    return paths.state_directory + "/session.state";
}

std::string Config::statistics_file() const {
    return paths.state_directory + "/statistics.json";
}

Config Config::load(const std::string& path) {
    Config config;
    
    // Set defaults
    config.paths.state_directory = get_default_state_directory();
    config.disk.min_free = 0;
    config.disk.min_free_percentage = 0.05;  // 5% default
    config.disk.max_storage = 0;  // unlimited
    config.daemon.log_level = "info";
    config.daemon.run_on_battery = false;
    config.limits.max_download_rate_kbps = 0;  // unlimited
    config.limits.max_upload_rate_kbps = 0;    // unlimited
    config.webtorrent.stun_server = "stun.l.google.com:19302";  // Google's public STUN

    try {
        const auto data = toml::parse(path);

        // Load paths (REQUIRED: watch_directory, data_directory)
        if (data.contains("paths")) {
            const auto& paths_section = toml::find(data, "paths");
            
            if (paths_section.contains("watch_directory")) {
                config.paths.watch_directory = expand_path(toml::find<std::string>(paths_section, "watch_directory"));
            }
            if (paths_section.contains("data_directory")) {
                config.paths.data_directory = expand_path(toml::find<std::string>(paths_section, "data_directory"));
            }
            if (paths_section.contains("state_directory")) {
                config.paths.state_directory = expand_path(toml::find<std::string>(paths_section, "state_directory"));
            }
        }

        // Load disk settings (REQUIRED: min_free)
        if (data.contains("disk")) {
            const auto& disk_section = toml::find(data, "disk");
            
            // min_free (supports human-readable or bytes)
            if (disk_section.contains("min_free")) {
                const auto& value = disk_section.at("min_free");
                if (value.is_integer()) {
                    config.disk.min_free = value.as_integer();
                } else if (value.is_string()) {
                    config.disk.min_free = parse_size(value.as_string());
                }
            }
            
            // min_free_percentage (optional, default 5%)
            if (disk_section.contains("min_free_percentage")) {
                config.disk.min_free_percentage = toml::find<double>(disk_section, "min_free_percentage");
            }
            
            // max_storage (optional, 0 = unlimited)
            if (disk_section.contains("max_storage")) {
                const auto& value = disk_section.at("max_storage");
                if (value.is_integer()) {
                    config.disk.max_storage = value.as_integer();
                } else if (value.is_string()) {
                    config.disk.max_storage = parse_size(value.as_string());
                }
            }
        }

        // Load daemon settings (all optional)
        if (data.contains("daemon")) {
            const auto& daemon_section = toml::find(data, "daemon");
            
            if (daemon_section.contains("log_level")) {
                config.daemon.log_level = toml::find<std::string>(daemon_section, "log_level");
            }
            if (daemon_section.contains("run_on_battery")) {
                config.daemon.run_on_battery = toml::find<bool>(daemon_section, "run_on_battery");
            }
        }

        // Load bandwidth limits (all optional)
        if (data.contains("limits")) {
            const auto& limits_section = toml::find(data, "limits");
            
            if (limits_section.contains("max_download_rate_kbps")) {
                config.limits.max_download_rate_kbps = toml::find<int>(limits_section, "max_download_rate_kbps");
            }
            if (limits_section.contains("max_upload_rate_kbps")) {
                config.limits.max_upload_rate_kbps = toml::find<int>(limits_section, "max_upload_rate_kbps");
            }
        }
        
        // Load WebTorrent settings (all optional)
        if (data.contains("webtorrent")) {
            const auto& webtorrent_section = toml::find(data, "webtorrent");
            
            if (webtorrent_section.contains("stun_server")) {
                config.webtorrent.stun_server = toml::find<std::string>(webtorrent_section, "stun_server");
            }
        }

    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to load configuration from '" + path + "': " + e.what());
    }

    return config;
}

bool Config::validate() const {
    // Required paths
    if (paths.watch_directory.empty()) {
        return false;
    }
    if (paths.data_directory.empty()) {
        return false;
    }
    if (paths.state_directory.empty()) {
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

    // Validate disk settings - must have at least one minimum
    if (disk.min_free == 0 && disk.min_free_percentage == 0.0) {
        return false;
    }
    if (disk.min_free_percentage < 0.0 || disk.min_free_percentage > 1.0) {
        return false;
    }

    // Validate bandwidth limits (can be 0 for unlimited, but not negative)
    if (limits.max_download_rate_kbps < 0 || limits.max_upload_rate_kbps < 0) {
        return false;
    }

    return true;
}

uint64_t Config::get_effective_min_free_space(uint64_t total_disk_bytes) const {
    uint64_t absolute_min = disk.min_free;
    uint64_t percentage_min = static_cast<uint64_t>(total_disk_bytes * disk.min_free_percentage);
    return std::max(absolute_min, percentage_min);
}

} // namespace levin
