#include "cli_client.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <iomanip>
#include <sstream>

using json = nlohmann::json;

namespace levin {
namespace cli {

static std::string get_default_socket_path() {
    // Try XDG_STATE_HOME first
    const char* state_home = std::getenv("XDG_STATE_HOME");
    if (state_home) {
        return std::string(state_home) + "/levin/levin.sock";
    }
    
    // Fall back to HOME
    const char* home = std::getenv("HOME");
    if (home) {
        return std::string(home) + "/.local/state/levin/levin.sock";
    }
    
    // Last resort
    return "/tmp/levin.sock";
}

static std::string send_command(const std::string& socket_path, const json& request) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        throw std::runtime_error("Failed to create socket");
    }

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        throw std::runtime_error("Failed to connect to daemon. Is it running?");
    }

    std::string request_str = request.dump();
    if (write(sock, request_str.c_str(), request_str.length()) < 0) {
        close(sock);
        throw std::runtime_error("Failed to send command");
    }

    char buffer[8192];
    ssize_t n = read(sock, buffer, sizeof(buffer) - 1);
    close(sock);

    if (n < 0) {
        throw std::runtime_error("Failed to read response");
    }

    buffer[n] = '\0';
    return std::string(buffer);
}

static std::string format_bytes(uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024.0 && unit < 4) {
        size /= 1024.0;
        unit++;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " " << units[unit];
    return oss.str();
}

static std::string format_rate(int bytes_per_second) {
    if (bytes_per_second == 0) {
        return "0 B/s";
    }
    
    const char* units[] = {"B/s", "KB/s", "MB/s", "GB/s"};
    int unit = 0;
    double rate = static_cast<double>(bytes_per_second);

    while (rate >= 1024.0 && unit < 3) {
        rate /= 1024.0;
        unit++;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << rate << " " << units[unit];
    return oss.str();
}

static std::string format_duration(uint64_t seconds) {
    if (seconds < 60) {
        return std::to_string(seconds) + "s";
    }
    uint64_t minutes = seconds / 60;
    if (minutes < 60) {
        return std::to_string(minutes) + "m";
    }
    uint64_t hours = minutes / 60;
    minutes = minutes % 60;
    if (hours < 24) {
        return std::to_string(hours) + "h " + std::to_string(minutes) + "m";
    }
    uint64_t days = hours / 24;
    hours = hours % 24;
    return std::to_string(days) + "d " + std::to_string(hours) + "h";
}

static void print_status(const json& data) {
    // Status line
    auto disk = data["disk"];
    bool over_budget = disk["over_budget"];
    uint64_t total = disk["total_bytes"];
    
    bool paused_for_battery = data.contains("paused_for_battery") && data["paused_for_battery"].get<bool>();
    
    // Get state if available
    std::string state_str = "Running";
    if (data.contains("state")) {
        std::string state = data["state"];
        // Match Android's display format
        if (state == "OFF") {
            state_str = "Off";
        } else if (state == "PAUSED") {
            state_str = "Paused";
            if (paused_for_battery) {
                state_str = "Paused (battery)";
            }
        } else if (state == "IDLE") {
            state_str = "Idle (no torrents)";
        } else if (state == "SEEDING") {
            state_str = "Seeding (storage limit)";
        } else if (state == "DOWNLOADING") {
            state_str = "Downloading";
        }
    } else if (total == 0) {
        state_str = "Unable to read filesystem";
    } else if (over_budget) {
        state_str = "⚠ OVER BUDGET";
    } else if (paused_for_battery) {
        state_str = "Paused (battery)";
    }
    
    std::cout << "Status: " << state_str << "\n";
    std::cout << "\n";

    // Disk
    std::cout << "Disk:\n";
    std::cout << "  Total:        " << format_bytes(disk["total_bytes"]) << "\n";
    std::cout << "  Free:         " << format_bytes(disk["free_bytes"]) << "\n";
    std::cout << "  Disk Used:    " << format_bytes(disk["used_bytes"]) << "\n";
    std::cout << "  Budget:       " << format_bytes(disk["budget_bytes"]) << "\n";
    std::cout << "\n";

    // Torrents
    auto torrents = data["torrents"];
    std::cout << "Torrents:\n";
    std::cout << "  Loaded:       " << torrents["total_loaded"] << "\n";
    std::cout << "  Total Pieces: " << torrents["total_pieces"] << "\n";
    std::cout << "  We Have:      " << torrents["pieces_we_have"];
    
    if (torrents["total_pieces"].get<int>() > 0) {
        double percent = 100.0 * torrents["pieces_we_have"].get<int>() / torrents["total_pieces"].get<int>();
        std::cout << " (" << std::fixed << std::setprecision(1) << percent << "%)";
    }
    std::cout << "\n\n";

    // Network with rates and session (lifetime) format
    auto network = data["network"];
    uint64_t session_down = network["session_downloaded"];
    uint64_t session_up = network["session_uploaded"];
    uint64_t lifetime_down = network["lifetime_downloaded"];
    uint64_t lifetime_up = network["lifetime_uploaded"];
    int download_rate = network["download_rate"];
    int upload_rate = network["upload_rate"];
    
    std::cout << "Network:\n";
    std::cout << "  Downloading: " << format_rate(download_rate) << "\n";
    std::cout << "  Uploading:   " << format_rate(upload_rate) << "\n";
    std::cout << "  Downloaded:  " << format_bytes(session_down) 
              << " (" << format_bytes(lifetime_down) << ")\n";
    std::cout << "  Uploaded:    " << format_bytes(session_up) 
              << " (" << format_bytes(lifetime_up) << ")\n";
    
    // Calculate ratios
    if (session_down > 0 && session_up > 0) {
        double sess_ratio = static_cast<double>(session_up) / session_down;
        double life_ratio = lifetime_down > 0 ? static_cast<double>(lifetime_up) / lifetime_down : 0.0;
        std::cout << "  Ratio:       " << std::fixed << std::setprecision(2) << sess_ratio 
                  << " (" << std::fixed << std::setprecision(2) << life_ratio << ")\n";
    } else if (lifetime_down > 0 && lifetime_up > 0) {
        double life_ratio = static_cast<double>(lifetime_up) / lifetime_down;
        std::cout << "  Ratio:       - (" << std::fixed << std::setprecision(2) << life_ratio << ")\n";
    }
    
    std::cout << "  Peers:       " << network["peers_connected"] << "\n";
    std::cout << "\n";
    
    // Uptime
    auto uptime = data["uptime"];
    std::cout << "Uptime: " << format_duration(uptime["session_seconds"]) 
              << " (" << format_duration(uptime["lifetime_seconds"]) << ")\n";
    std::cout << "\n";
}

static void print_list(const json& data) {
    auto torrents = data["torrents"];
    
    std::cout << "Torrents (" << torrents.size() << " loaded):\n\n";
    
    std::cout << std::left 
              << std::setw(12) << "HASH"
              << std::setw(40) << "NAME"
              << std::setw(10) << "SEEDERS"
              << std::setw(15) << "PIECES"
              << std::setw(12) << "SIZE"
              << "\n";
    std::cout << std::string(89, '-') << "\n";

    for (const auto& t : torrents) {
        std::string hash = t["info_hash"];
        std::string name = t["name"];
        if (name.length() > 37) {
            name = name.substr(0, 34) + "...";
        }

        int pieces_have = t["pieces_have"];
        int pieces_total = t["pieces_total"];
        std::string pieces_str = std::to_string(pieces_have) + "/" + std::to_string(pieces_total);

        std::cout << std::left
                  << std::setw(12) << hash.substr(0, 10)
                  << std::setw(40) << name
                  << std::setw(10) << t["seeders"]
                  << std::setw(15) << pieces_str
                  << std::setw(12) << format_bytes(t["size_total"])
                  << "\n";
    }
    std::cout << "\n";
}

static void print_bandwidth(const json& data) {
    int download_kbps = data["download_limit_kbps"].get<int>();
    int upload_kbps = data["upload_limit_kbps"].get<int>();
    
    std::cout << "Bandwidth Limits:\n";
    std::cout << "  Download: ";
    if (download_kbps == 0) {
        std::cout << "unlimited\n";
    } else {
        std::cout << download_kbps << " KB/s\n";
    }
    
    std::cout << "  Upload:   ";
    if (upload_kbps == 0) {
        std::cout << "unlimited\n";
    } else {
        std::cout << upload_kbps << " KB/s\n";
    }
    std::cout << "\n";
}

static void print_usage() {
    std::cout << "Usage: levin COMMAND [OPTIONS]\n\n"
              << "Commands:\n"
              << "  start [OPTIONS]       Start the daemon\n"
              << "    -c, --config FILE   Configuration file path\n"
              << "    -f, --foreground    Run in foreground (don't daemonize)\n"
              << "  status                Show daemon status and statistics\n"
              << "  list                  List all loaded torrents\n"
              << "  pause                 Pause all torrent activity\n"
              << "  resume                Resume torrent activity\n"
              << "  bandwidth             Show current bandwidth limits\n"
              << "  bandwidth --download KBPS   Set download limit (0 = unlimited)\n"
              << "  bandwidth --upload KBPS     Set upload limit (0 = unlimited)\n"
              << "  terminate             Stop the daemon\n"
              << "\n"
              << "Global options:\n"
              << "  --socket PATH         Path to control socket (default: $XDG_STATE_HOME/levin/levin.sock)\n"
              << "  --version             Show version information\n"
              << "  --help                Show this help message\n"
              << std::endl;
}

int run_client(int argc, char** argv) {
    std::string socket_path = get_default_socket_path();
    std::string command;
    json args = json::object();

    // Parse arguments
    for (int i = 0; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--socket" && i + 1 < argc) {
            socket_path = argv[++i];
        } else if (arg == "--help") {
            print_usage();
            return 0;
        } else if (command.empty()) {
            command = arg;
        } else if (command == "bandwidth") {
            // Parse bandwidth command arguments
            if (arg == "--download" && i + 1 < argc) {
                args["download"] = std::stoi(argv[++i]);
            } else if (arg == "--upload" && i + 1 < argc) {
                args["upload"] = std::stoi(argv[++i]);
            }
        }
    }

    if (command.empty()) {
        print_usage();
        return 1;
    }

    try {
        std::string server_command = command;
        
        json request = {
            {"command", server_command},
            {"args", args}
        };

        std::string response_str = send_command(socket_path, request);
        json response = json::parse(response_str);

        if (!response["success"]) {
            std::cerr << "Error: " << response["error"] << std::endl;
            return 1;
        }

        if (command == "status") {
            print_status(response["data"]);
        } else if (command == "list") {
            print_list(response["data"]);
        } else if (command == "pause" || command == "resume") {
            std::cout << response["message"] << std::endl;
        } else if (command == "terminate") {
            std::cout << response["message"] << std::endl;
        } else if (command == "bandwidth") {
            // Check if we're just querying or setting bandwidth
            if (response.contains("message")) {
                std::cout << response["message"] << std::endl;
            } else {
                print_bandwidth(response);
            }
        } else {
            std::cout << response.dump(2) << std::endl;
        }

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}

} // namespace cli
} // namespace levin
