#include "daemon.hpp"
#include "config.hpp"
#include "logger.hpp"
#include "session.hpp"
#include "torrent_watcher.hpp"
#include "disk_monitor.hpp"
#include "statistics.hpp"
#include "piece_manager.hpp"
#include "cli_server.hpp"
#include "power_monitor.hpp"
#include "utils.hpp"
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <fstream>
#include <stdexcept>
#include <csignal>
#include <cstdlib>

namespace levin {

// Global pointer for signal handler access
static Daemon* g_daemon_instance = nullptr;

// Signal handler
static void signal_handler(int signum) {
    if (g_daemon_instance) {
        // Don't use LOG_INFO here - might not be safe in signal handler
        (void)signum;  // Avoid unused parameter warning
        g_daemon_instance->shutdown();
    }
}

Daemon::Daemon(const Config& config)
    : config_(config)
    , running_(false)
    , pid_file_(config.daemon.pid_file) {
    g_daemon_instance = this;
}

Daemon::~Daemon() {
    if (running_) {
        shutdown();
    }
    remove_pid_file();
    g_daemon_instance = nullptr;
}

void Daemon::daemonize() {
    LOG_INFO("Daemonizing process...");

    // Fork the parent process
    pid_t pid = fork();
    if (pid < 0) {
        throw std::runtime_error("Failed to fork process");
    }
    
    // Exit parent process
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    // Child process continues here
    
    // Create new session and become session leader
    if (setsid() < 0) {
        throw std::runtime_error("Failed to create new session");
    }

    // Fork again to ensure we're not session leader (prevents acquiring controlling terminal)
    pid = fork();
    if (pid < 0) {
        throw std::runtime_error("Failed to fork second time");
    }
    
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    // Set file mode creation mask
    umask(0);

    // Change working directory to root
    if (chdir("/") < 0) {
        throw std::runtime_error("Failed to change working directory to /");
    }

    // Close standard file descriptors
    close_standard_fds();

    LOG_INFO("Daemonization complete, PID: {}", getpid());
}

void Daemon::close_standard_fds() {
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    // Redirect to /dev/null
    int fd = open("/dev/null", O_RDWR);
    if (fd != -1) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > STDERR_FILENO) {
            close(fd);
        }
    }
}

void Daemon::create_pid_file() {
    std::ofstream pid_file(pid_file_);
    if (!pid_file.is_open()) {
        throw std::runtime_error("Failed to create PID file: " + pid_file_);
    }
    
    pid_file << getpid() << std::endl;
    pid_file.close();
    
    LOG_INFO("Created PID file: {}", pid_file_);
}

void Daemon::remove_pid_file() {
    if (!pid_file_.empty()) {
        unlink(pid_file_.c_str());
        LOG_INFO("Removed PID file: {}", pid_file_);
    }
}

void Daemon::setup_signal_handlers() {
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    // Handle termination signals
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGHUP, &sa, nullptr);

    // Ignore SIGPIPE (broken pipe)
    signal(SIGPIPE, SIG_IGN);

    LOG_INFO("Signal handlers installed");
}

bool Daemon::initialize_components() {
    LOG_INFO("Initializing components...");

    // Initialize statistics
    statistics_ = std::make_unique<Statistics>(config_);
    statistics_->load();

    // Initialize disk monitor
    disk_monitor_ = std::make_unique<DiskMonitor>(config_);

    // Initialize libtorrent session
    session_ = std::make_unique<Session>(config_);
    if (!session_->start()) {
        LOG_ERROR("Failed to start libtorrent session");
        return false;
    }

    // Initialize torrent watcher
    watcher_ = std::make_unique<TorrentWatcher>(config_);
    
    // Set up callbacks for torrent events
    watcher_->set_torrent_added_callback([this](const std::string& path) {
        LOG_INFO("New torrent detected: {}", path);
        session_->add_torrent(path);
    });

    watcher_->set_torrent_removed_callback([this](const std::string& path) {
        LOG_INFO("Torrent removed: {}", path);
        // TODO: Remove from session (need to track path -> info_hash mapping)
    });

    if (!watcher_->start()) {
        LOG_ERROR("Failed to start torrent watcher");
        return false;
    }

    // Initialize piece manager (after session and disk monitor)
    piece_manager_ = std::make_unique<PieceManager>(config_, *session_, *disk_monitor_);

    // Load existing torrents
    auto existing_torrents = watcher_->scan_existing_torrents();
    LOG_INFO("Loading {} existing torrents", existing_torrents.size());
    for (const auto& torrent_path : existing_torrents) {
        session_->add_torrent(torrent_path);
    }

    // Initial metrics update
    piece_manager_->update_metrics();
    piece_manager_->rebuild_queues();

    // Initialize CLI server
    cli_server_ = std::make_unique<CLIServer>(config_, *session_, *statistics_, 
                                               *piece_manager_, *disk_monitor_);
    
    // Set up pause/resume callbacks
    cli_server_->set_pause_callback([this]() {
        LOG_INFO("Pausing all downloads (CLI command)");
        piece_manager_->emergency_pause_downloads();
    });
    
    cli_server_->set_resume_callback([this]() {
        LOG_INFO("Resuming downloads (CLI command)");
        piece_manager_->rebuild_queues();
    });
    
    cli_server_->set_paused_for_battery_callback([this]() {
        return paused_for_battery_.load();
    });
    
    cli_server_->set_terminate_callback([this]() {
        LOG_INFO("Terminate command received");
        shutdown();
    });
    
    if (!cli_server_->start()) {
        LOG_WARN("Failed to start CLI server (non-fatal)");
    }

    // Initialize timers
    last_disk_check_ = std::chrono::steady_clock::now();
    last_stats_save_ = std::chrono::steady_clock::now();
    last_metrics_update_ = std::chrono::steady_clock::now();
    last_rebalance_ = std::chrono::steady_clock::now();

    // Initialize power monitor if run_on_battery is false
    if (!config_.daemon.run_on_battery) {
        power_monitor_ = std::make_unique<PowerMonitor>();
        power_monitor_->start([this](bool on_ac_power) {
            if (on_ac_power) {
                // Plugged into AC - resume if we were paused for battery
                if (paused_for_battery_) {
                    LOG_INFO("AC power detected - resuming downloads");
                    paused_for_battery_ = false;
                    piece_manager_->rebuild_queues();
                }
            } else {
                // Running on battery - pause
                if (!paused_for_battery_) {
                    LOG_INFO("Battery power detected - pausing downloads");
                    paused_for_battery_ = true;
                    piece_manager_->emergency_pause_downloads("battery power");
                }
            }
        });
        
        // Check initial state and pause if needed
        if (!power_monitor_->is_on_ac_power()) {
            LOG_INFO("Starting on battery power - pausing downloads");
            paused_for_battery_ = true;
            piece_manager_->emergency_pause_downloads("battery power");
        }
    }

    LOG_INFO("All components initialized successfully");
    return true;
}

void Daemon::run() {
    LOG_INFO("Starting Levin daemon...");
    
    running_ = true;
    
    // Create PID file
    create_pid_file();
    
    // Setup signal handlers
    setup_signal_handlers();

    // Initialize components
    if (!initialize_components()) {
        LOG_CRITICAL("Failed to initialize components, shutting down");
        running_ = false;
        return;
    }
    
    LOG_INFO("Daemon is running");
    
    // Main event loop
    while (running_) {
        auto now = std::chrono::steady_clock::now();

        // Process libtorrent alerts
        session_->process_alerts();

        // Process torrent watcher events
        watcher_->process_events();

        // Process CLI commands
        cli_server_->process_commands();

        // Update torrent metrics periodically (every 15 minutes)
        auto metrics_interval = std::chrono::minutes(config_.torrents.seeder_update_interval_minutes);
        if (now - last_metrics_update_ >= metrics_interval) {
            piece_manager_->update_metrics();
            piece_manager_->rebuild_queues();
            last_metrics_update_ = now;
        }

        // Rebalance disk usage periodically (every minute)
        auto rebalance_interval = std::chrono::seconds(60);
        if (now - last_rebalance_ >= rebalance_interval) {
            // Update metrics to get accurate disk usage (but don't rebuild queues)
            piece_manager_->update_metrics();
            
            // Update current disk usage
            uint64_t total_data = piece_manager_->get_total_data_size();
            disk_monitor_->set_current_usage(total_data);
            
            // Check space and rebalance
            auto status = disk_monitor_->check_space();
            LOG_INFO("Disk usage: {} / {} (budget: {})",
                     utils::format_bytes(total_data),
                     utils::format_bytes(status.total_bytes),
                     utils::format_bytes(status.budget_bytes));
            
            piece_manager_->rebalance_disk_usage();
            last_rebalance_ = now;
        }

        // Save statistics periodically
        auto stats_interval = std::chrono::minutes(config_.statistics.save_interval_minutes);
        if (now - last_stats_save_ >= stats_interval) {
            uint64_t downloaded, uploaded;
            int peers;
            session_->get_stats(downloaded, uploaded, peers);
            
            // Update torrent/piece counts
            const auto& metrics = piece_manager_->get_metrics();
            int total_pieces = 0, pieces_we_have = 0;
            for (const auto& [hash, m] : metrics) {
                total_pieces += m.total_pieces;
                pieces_we_have += m.pieces_we_have;
            }
            
            statistics_->update(downloaded, uploaded);
            statistics_->update_state(metrics.size(), pieces_we_have, total_pieces, peers);
            statistics_->save();
            last_stats_save_ = now;
        }

        // Sleep to avoid busy waiting
        usleep(100000);  // 100ms
    }
    
    LOG_INFO("Daemon shutting down");

    // Save final state
    statistics_->save();
}

void Daemon::shutdown() {
    LOG_INFO("Shutdown requested");
    running_ = false;
}

} // namespace levin
