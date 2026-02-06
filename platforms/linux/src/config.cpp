#include "config.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace levin::linux_shell {

namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string to_lower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return out;
}

// Expand ~ to $HOME and $VAR / ${VAR} environment variables in a string.
std::string expand_path(const std::string& raw) {
    std::string result;
    result.reserve(raw.size());

    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '~' && (i == 0) &&
            (i + 1 == raw.size() || raw[i + 1] == '/')) {
            const char* home = std::getenv("HOME");
            result += home ? home : "~";
        } else if (raw[i] == '$') {
            // Environment variable
            ++i;
            bool braced = false;
            if (i < raw.size() && raw[i] == '{') {
                braced = true;
                ++i;
            }
            std::string var_name;
            while (i < raw.size()) {
                if (braced && raw[i] == '}') {
                    ++i;
                    break;
                }
                if (!braced && !(std::isalnum(static_cast<unsigned char>(raw[i])) || raw[i] == '_'))
                    break;
                var_name += raw[i];
                ++i;
            }
            --i; // will be incremented by the for loop

            const char* val = std::getenv(var_name.c_str());
            if (val) result += val;
        } else {
            result += raw[i];
        }
    }
    return result;
}

// Parse a human-readable byte size string: "1gb", "500mb", "10tb", "1024", etc.
// Returns 0 on parse failure.
uint64_t parse_byte_size(const std::string& raw) {
    std::string s = trim(raw);
    if (s.empty()) return 0;

    // Find where the numeric part ends
    size_t num_end = 0;
    bool has_dot = false;
    while (num_end < s.size()) {
        char c = s[num_end];
        if (std::isdigit(static_cast<unsigned char>(c))) {
            ++num_end;
        } else if (c == '.' && !has_dot) {
            has_dot = true;
            ++num_end;
        } else {
            break;
        }
    }
    if (num_end == 0) return 0;

    double value = std::stod(s.substr(0, num_end));
    std::string suffix = to_lower(trim(s.substr(num_end)));

    uint64_t multiplier = 1;
    if (suffix.empty() || suffix == "b") {
        multiplier = 1;
    } else if (suffix == "kb" || suffix == "k") {
        multiplier = 1024ULL;
    } else if (suffix == "mb" || suffix == "m") {
        multiplier = 1024ULL * 1024;
    } else if (suffix == "gb" || suffix == "g") {
        multiplier = 1024ULL * 1024 * 1024;
    } else if (suffix == "tb" || suffix == "t") {
        multiplier = 1024ULL * 1024 * 1024 * 1024;
    } else if (suffix == "pb" || suffix == "p") {
        multiplier = 1024ULL * 1024 * 1024 * 1024 * 1024;
    }

    return static_cast<uint64_t>(value * static_cast<double>(multiplier));
}

// Remove surrounding quotes from a string value (single or double).
std::string unquote(const std::string& s) {
    if (s.size() >= 2) {
        char front = s.front();
        char back = s.back();
        if ((front == '"' && back == '"') || (front == '\'' && back == '\'')) {
            return s.substr(1, s.size() - 2);
        }
    }
    return s;
}

// Determine the default config file path via XDG_CONFIG_HOME.
std::string default_config_path() {
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0] != '\0') {
        return std::string(xdg) + "/levin/levin.toml";
    }
    const char* home = std::getenv("HOME");
    if (home) {
        return std::string(home) + "/.config/levin/levin.toml";
    }
    return "/etc/levin/levin.toml";
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// load_config
// ---------------------------------------------------------------------------

ShellConfig load_config(const std::string& config_path) {
    ShellConfig cfg{};

    // Sensible defaults (matching DESIGN.md)
    const char* home = std::getenv("HOME");
    std::string home_str = home ? home : "/tmp";

    cfg.watch_dir  = home_str + "/.config/levin/torrents";
    cfg.data_dir   = home_str + "/.cache/levin/data";
    cfg.state_dir  = home_str + "/.local/state/levin";
    cfg.stun       = "stun.l.google.com:19302";
    cfg.log_level  = "info";

    cfg.lib_config.min_free_bytes          = 1ULL * 1024 * 1024 * 1024; // 1 GB
    cfg.lib_config.min_free_percentage     = 0.05;
    cfg.lib_config.max_storage_bytes       = 50ULL * 1024 * 1024 * 1024; // 50 GB
    cfg.lib_config.run_on_battery          = 0;
    cfg.lib_config.run_on_cellular         = 0;
    cfg.lib_config.disk_check_interval_secs = 60;
    cfg.lib_config.max_download_kbps       = 0;
    cfg.lib_config.max_upload_kbps         = 0;

    // Open config file
    std::string path = config_path.empty() ? default_config_path() : config_path;
    std::ifstream file(path);
    if (!file.is_open()) {
        // No config file -- use defaults.  Wire up pointers and return.
        cfg.lib_config.watch_directory = cfg.watch_dir.c_str();
        cfg.lib_config.data_directory  = cfg.data_dir.c_str();
        cfg.lib_config.state_directory = cfg.state_dir.c_str();
        cfg.lib_config.stun_server     = cfg.stun.c_str();
        return cfg;
    }

    // Parse line by line
    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key   = trim(line.substr(0, eq));
        std::string value = trim(line.substr(eq + 1));

        if (key == "watch_directory") {
            cfg.watch_dir = expand_path(unquote(value));
        } else if (key == "data_directory") {
            cfg.data_dir = expand_path(unquote(value));
        } else if (key == "state_directory") {
            cfg.state_dir = expand_path(unquote(value));
        } else if (key == "min_free_bytes") {
            cfg.lib_config.min_free_bytes = parse_byte_size(unquote(value));
        } else if (key == "min_free_percentage") {
            cfg.lib_config.min_free_percentage = std::stod(value);
        } else if (key == "max_storage_bytes") {
            cfg.lib_config.max_storage_bytes = parse_byte_size(unquote(value));
        } else if (key == "run_on_battery") {
            std::string v = to_lower(value);
            cfg.lib_config.run_on_battery = (v == "true" || v == "1") ? 1 : 0;
        } else if (key == "run_on_cellular") {
            std::string v = to_lower(value);
            cfg.lib_config.run_on_cellular = (v == "true" || v == "1") ? 1 : 0;
        } else if (key == "disk_check_interval_secs") {
            cfg.lib_config.disk_check_interval_secs = std::stoi(value);
        } else if (key == "max_download_kbps") {
            cfg.lib_config.max_download_kbps = std::stoi(value);
        } else if (key == "max_upload_kbps") {
            cfg.lib_config.max_upload_kbps = std::stoi(value);
        } else if (key == "stun_server") {
            cfg.stun = unquote(value);
        } else if (key == "log_level") {
            cfg.log_level = to_lower(unquote(value));
        }
        // Unknown keys are silently ignored.
    }

    // Wire up const char* pointers into owned strings
    cfg.lib_config.watch_directory = cfg.watch_dir.c_str();
    cfg.lib_config.data_directory  = cfg.data_dir.c_str();
    cfg.lib_config.state_directory = cfg.state_dir.c_str();
    cfg.lib_config.stun_server     = cfg.stun.c_str();

    return cfg;
}

} // namespace levin::linux_shell
