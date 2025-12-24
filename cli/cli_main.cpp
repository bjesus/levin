#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <iomanip>

using json = nlohmann::json;

// Default socket path
const char* DEFAULT_SOCKET = "/var/run/archiver.sock";

std::string send_command(const std::string& socket_path, const json& request) {
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

std::string format_bytes(uint64_t bytes) {
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

std::string format_duration(uint64_t seconds) {
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

void print_status(const json& data) {
    std::cout << "\n=== Archiver Status ===\n\n";

    // Uptime
    uint64_t uptime = data["uptime_seconds"];
    std::cout << "Uptime: " << format_duration(uptime) << "\n\n";

    // Disk
    auto disk = data["disk"];
    std::cout << "Disk Usage:\n";
    std::cout << "  Total:    " << format_bytes(disk["total_bytes"]) << "\n";
    std::cout << "  Free:     " << format_bytes(disk["free_bytes"]) << "\n";
    std::cout << "  Used:     " << format_bytes(disk["used_bytes"]) << "\n";
    std::cout << "  Min Free: " << format_bytes(disk["min_required_bytes"]) << "\n";
    std::cout << "  Budget:   " << format_bytes(disk["budget_bytes"]) << "\n";
    
    bool over_budget = disk["over_budget"];
    if (over_budget) {
        std::cout << "  Status:   ⚠ OVER BUDGET\n";
    } else {
        std::cout << "  Status:   ✓ OK\n";
    }
    std::cout << "\n";

    // Torrents
    auto torrents = data["torrents"];
    std::cout << "Torrents:\n";
    std::cout << "  Loaded:        " << torrents["total_loaded"] << "\n";
    std::cout << "  Total Pieces:  " << torrents["total_pieces"] << "\n";
    std::cout << "  Pieces We Have: " << torrents["pieces_we_have"] << "\n";
    
    if (torrents["total_pieces"].get<int>() > 0) {
        double percent = 100.0 * torrents["pieces_we_have"].get<int>() / torrents["total_pieces"].get<int>();
        std::cout << "  Completion:    " << std::fixed << std::setprecision(1) << percent << "%\n";
    }
    std::cout << "\n";

    // Network
    auto network = data["network"];
    std::cout << "Network:\n";
    std::cout << "  Downloaded: " << format_bytes(network["total_downloaded"]) << "\n";
    std::cout << "  Uploaded:   " << format_bytes(network["total_uploaded"]) << "\n";
    std::cout << "  Peers:      " << network["peers_connected"] << "\n";
    std::cout << "\n";
}

void print_list(const json& data) {
    auto torrents = data["torrents"];
    
    std::cout << "\n=== Torrent List (" << torrents.size() << " torrents) ===\n\n";
    
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

void print_stats(const json& data) {
    std::cout << "\n=== Statistics ===\n\n";

    auto session = data["session"];
    std::cout << "Session (current):\n";
    std::cout << "  Downloaded: " << format_bytes(session["downloaded"]) << "\n";
    std::cout << "  Uploaded:   " << format_bytes(session["uploaded"]) << "\n";
    std::cout << "  Uptime:     " << format_duration(session["uptime"]) << "\n";
    
    if (session["downloaded"].get<uint64_t>() > 0 && session["uploaded"].get<uint64_t>() > 0) {
        double ratio = static_cast<double>(session["uploaded"].get<uint64_t>()) / session["downloaded"].get<uint64_t>();
        std::cout << "  Ratio:      " << std::fixed << std::setprecision(2) << ratio << "\n";
    }
    std::cout << "\n";

    auto lifetime = data["lifetime"];
    std::cout << "Lifetime (all sessions):\n";
    std::cout << "  Downloaded: " << format_bytes(lifetime["downloaded"]) << "\n";
    std::cout << "  Uploaded:   " << format_bytes(lifetime["uploaded"]) << "\n";
    std::cout << "  Uptime:     " << format_duration(lifetime["uptime"]) << "\n";
    std::cout << "  Sessions:   " << lifetime["sessions"] << "\n";
    
    if (lifetime["downloaded"].get<uint64_t>() > 0 && lifetime["uploaded"].get<uint64_t>() > 0) {
        double ratio = static_cast<double>(lifetime["uploaded"].get<uint64_t>()) / lifetime["downloaded"].get<uint64_t>();
        std::cout << "  Ratio:      " << std::fixed << std::setprecision(2) << ratio << "\n";
    }
    std::cout << "\n";
}

void print_bandwidth(const json& data) {
    std::cout << "=== Bandwidth Limits ===\n\n";
    
    int download_kbps = data["download_limit_kbps"].get<int>();
    int upload_kbps = data["upload_limit_kbps"].get<int>();
    
    std::cout << "Download: ";
    if (download_kbps == 0) {
        std::cout << "unlimited\n";
    } else {
        std::cout << download_kbps << " KB/s\n";
    }
    
    std::cout << "Upload:   ";
    if (upload_kbps == 0) {
        std::cout << "unlimited\n";
    } else {
        std::cout << upload_kbps << " KB/s\n";
    }
    std::cout << "\n";
}

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [OPTIONS] COMMAND [ARGS]\n\n"
              << "Commands:\n"
              << "  status                Show daemon status and statistics\n"
              << "  list                  List all loaded torrents\n"
              << "  stats                 Show detailed statistics\n"
              << "  pause                 Pause all torrent activity\n"
              << "  resume                Resume torrent activity\n"
              << "  bandwidth             Show current bandwidth limits\n"
              << "  bandwidth --download KBPS   Set download limit (0 = unlimited)\n"
              << "  bandwidth --upload KBPS     Set upload limit (0 = unlimited)\n"
              << "\n"
              << "Options:\n"
              << "  --socket PATH   Path to control socket (default: " << DEFAULT_SOCKET << ")\n"
              << "  --version       Show version information\n"
              << "  --help          Show this help message\n"
              << std::endl;
}

int main(int argc, char** argv) {
    std::string socket_path = DEFAULT_SOCKET;
    std::string command;
    json args = json::object();

    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--socket" && i + 1 < argc) {
            socket_path = argv[++i];
        } else if (arg == "--version") {
            std::cout << "Levin v" << PROJECT_VERSION << std::endl;
            return 0;
        } else if (arg == "--help") {
            print_usage(argv[0]);
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
        print_usage(argv[0]);
        return 1;
    }

    try {
        json request = {
            {"command", command},
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
        } else if (command == "stats") {
            print_stats(response["data"]);
        } else if (command == "pause" || command == "resume") {
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
