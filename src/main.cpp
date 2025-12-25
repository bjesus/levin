#include "config.hpp"
#include "logger.hpp"
#include "daemon.hpp"
#include "utils.hpp"
#include "cli_client.hpp"
#include "annas_archive.hpp"
#include <iostream>
#include <cstdlib>
#include <getopt.h>
#include <filesystem>
#include <fstream>
#include <cstring>

using namespace levin;

std::string get_default_config_path() {
    return utils::get_xdg_path("XDG_CONFIG_HOME", ".config", "levin/levin.toml");
}

std::string get_default_config_content() {
    return R"([daemon]
# PID file location (default: $XDG_STATE_HOME/levin/levin.pid or ~/.local/state/levin/levin.pid)
pid_file = "$XDG_STATE_HOME/levin/levin.pid"

# Log file location (default: $XDG_STATE_HOME/levin/levin.log or ~/.local/state/levin/levin.log)
log_file = "$XDG_STATE_HOME/levin/levin.log"

# Log level: trace, debug, info, warn, error, critical
log_level = "info"

# Run on battery power (false = pause when on battery, resume when plugged in)
run_on_battery = false

[paths]
# Directory to watch for .torrent files (default: $XDG_CONFIG_HOME/levin/torrents or ~/.config/levin/torrents)
watch_directory = "$XDG_CONFIG_HOME/levin/torrents"

# Directory to store downloaded data (default: $XDG_CACHE_HOME/levin/data or ~/.cache/levin/data)
data_directory = "$XDG_CACHE_HOME/levin/data"

# Session state file (default: $XDG_STATE_HOME/levin/session.state or ~/.local/state/levin/session.state)
session_state = "$XDG_STATE_HOME/levin/session.state"

# Statistics file (default: $XDG_STATE_HOME/levin/statistics.json or ~/.local/state/levin/statistics.json)
statistics_file = "$XDG_STATE_HOME/levin/statistics.json"

[disk]
# Minimum free space (supports: "100mb", "5gb", "1tb" or raw bytes)
min_free_space = "1gb"

# Minimum free space as percentage (0.05 = 5%)
min_free_percentage = 0.05

# How often to check disk space (seconds)
check_interval_seconds = 60

[torrents]
# How often to update seeder counts (minutes)
seeder_update_interval_minutes = 60

# How often to scan watch directory (seconds)
watch_directory_scan_interval_seconds = 30

# Maximum connections per torrent
max_connections_per_torrent = 50

# Maximum upload slots per torrent
max_upload_slots_per_torrent = 8

[limits]
# Maximum download rate in KB/s (0 = unlimited)
max_download_rate_kbps = 0

# Maximum upload rate in KB/s (0 = unlimited)
max_upload_rate_kbps = 0

# Maximum total connections
max_total_connections = 200

# Maximum active downloads
max_active_downloads = 4

# Maximum active seeds (-1 = unlimited)
max_active_seeds = -1

# Maximum active torrents total
max_active_torrents = 8

[network]
# Port to listen on
listen_port = 6881

# Enable DHT (Distributed Hash Table)
enable_dht = true

# Enable LSD (Local Service Discovery)
enable_lsd = true

# Enable UPnP (Universal Plug and Play)
enable_upnp = true

# Enable NAT-PMP
enable_natpmp = true

# Enable WebRTC for WebTorrent support
enable_webrtc = false

# STUN server for WebRTC (required if enable_webrtc = true)
webrtc_stun_server = "stun:stun.l.google.com:19302"

[cli]
# Unix socket for CLI communication (default: $XDG_STATE_HOME/levin/levin.sock or ~/.local/state/levin/levin.sock)
control_socket = "$XDG_STATE_HOME/levin/levin.sock"

[statistics]
# How often to save statistics to disk (minutes)
save_interval_minutes = 5
)";
}

bool create_default_config() {
    std::string config_path = get_default_config_path();
    std::filesystem::path config_file(config_path);
    
    // Create parent directories
    if (!utils::ensure_directory(config_file.parent_path().string())) {
        std::cerr << "Error: Failed to create config directory: " 
                  << config_file.parent_path() << std::endl;
        return false;
    }
    
    // Create torrents subdirectory
    std::string torrents_dir = utils::get_xdg_path("XDG_CONFIG_HOME", ".config", "levin/torrents");
    if (!utils::ensure_directory(torrents_dir)) {
        std::cerr << "Error: Failed to create torrents directory: " 
                  << torrents_dir << std::endl;
        return false;
    }
    
    // Write config file
    std::ofstream file(config_path);
    if (!file) {
        std::cerr << "Error: Failed to create config file: " << config_path << std::endl;
        return false;
    }
    
    file << get_default_config_content();
    file.close();
    
    std::cout << "Created default configuration at: " << config_path << std::endl;
    
    // Ask user if they want to populate torrents from Anna's Archive
    if (annas_archive::prompt_user_to_populate()) {
        std::cout << "\nFetching torrent list from Anna's Archive..." << std::endl;
        
        try {
            auto result = annas_archive::populate_torrents(
                torrents_dir,
                [](int current, int total) {
                    std::cout << "\rDownloading torrents: " << current << "/" << total << std::flush;
                }
            );
            
            std::cout << "\n\nDownload complete!" << std::endl;
            std::cout << "Successfully downloaded: " << result.successful << " torrents" << std::endl;
            
            if (result.failed > 0) {
                std::cout << "Failed downloads: " << result.failed << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "\nError: " << e.what() << std::endl;
            std::cerr << "You can manually add torrent files to: " << torrents_dir << std::endl;
        }
    }
    
    std::cout << "\nPlease review and customize the configuration, then start the daemon:\n"
              << "  levin start\n"
              << std::endl;
    
    return true;
}

void print_usage() {
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

int run_daemon(int argc, char** argv) {
    std::string config_file;
    bool foreground = false;

    // Parse command line arguments for daemon mode
    // Skip first arg which is "start"
    optind = 0;  // Reset getopt
    static struct option long_options[] = {
        {"config",     required_argument, 0, 'c'},
        {"foreground", no_argument,       0, 'f'},
        {"help",       no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "c:fh", long_options, nullptr)) != -1) {
        switch (opt) {
            case 'c':
                config_file = optarg;
                break;
            case 'f':
                foreground = true;
                break;
            case 'h':
                print_usage();
                return EXIT_SUCCESS;
            default:
                print_usage();
                return EXIT_FAILURE;
        }
    }

    // Use default config path if not specified
    if (config_file.empty()) {
        config_file = get_default_config_path();
    }
    
    // If config doesn't exist, create it and exit
    if (!std::filesystem::exists(config_file)) {
        if (!create_default_config()) {
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }

    try {
        // Load configuration
        Config config = Config::load(config_file);
        
        if (!config.validate()) {
            std::cerr << "Error: Invalid configuration" << std::endl;
            return EXIT_FAILURE;
        }

        // Initialize logger
        Logger::init(config.daemon.log_file, config.daemon.log_level);
        
        LOG_INFO("=================================================");
        LOG_INFO("Levin v{} starting", PROJECT_VERSION);
        LOG_INFO("Configuration: {}", config_file);
        LOG_INFO("=================================================");

        // Create daemon
        Daemon daemon(config);

        // Daemonize if not in foreground mode
        if (!foreground) {
            LOG_INFO("Daemonizing...");
            daemon.daemonize();
        } else {
            LOG_INFO("Running in foreground mode");
        }

        // Run the daemon
        daemon.run();

        LOG_INFO("Levin shutdown complete");
        Logger::shutdown();

        return EXIT_SUCCESS;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        LOG_CRITICAL("Fatal error: {}", e.what());
        Logger::shutdown();
        return EXIT_FAILURE;
    }
}

int main(int argc, char** argv) {
    // Check if there are no arguments or if first arg is a flag
    if (argc < 2 || argv[1][0] == '-') {
        // Check for global flags
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
                std::cout << "Levin v" << PROJECT_VERSION << std::endl;
                return EXIT_SUCCESS;
            } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
                print_usage();
                return EXIT_SUCCESS;
            }
        }
        
        // No valid command
        print_usage();
        return EXIT_FAILURE;
    }

    std::string command = argv[1];
    
    // Dispatch to daemon or client mode
    if (command == "start") {
        // Daemon mode - pass remaining args
        return run_daemon(argc - 1, &argv[1]);
    } else {
        // Client mode - pass command and remaining args
        return cli::run_client(argc - 1, &argv[1]);
    }
}
