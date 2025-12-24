#pragma once

#include <string>
#include <atomic>
#include <memory>
#include <chrono>

namespace levin {

struct Config;
class Session;
class TorrentWatcher;
class DiskMonitor;
class Statistics;
class PieceManager;
class CLIServer;
class PowerMonitor;

/**
 * Main daemon class that handles:
 * - Daemonization (fork, setsid, etc.)
 * - PID file management
 * - Signal handling
 * - Main event loop
 */
class Daemon {
public:
    explicit Daemon(const Config& config);
    ~Daemon();

    /**
     * Daemonize the process (fork, close fds, etc.)
     * Call this before run() if you want to run as a daemon.
     */
    void daemonize();

    /**
     * Run the main event loop.
     * This blocks until shutdown is requested.
     */
    void run();

    /**
     * Request graceful shutdown.
     * Can be called from signal handlers.
     */
    void shutdown();

    /**
     * Check if daemon is running.
     */
    bool is_running() const { return running_; }

private:
    const Config& config_;
    std::atomic<bool> running_;
    std::string pid_file_;

    // Core components
    std::unique_ptr<Session> session_;
    std::unique_ptr<TorrentWatcher> watcher_;
    std::unique_ptr<DiskMonitor> disk_monitor_;
    std::unique_ptr<Statistics> statistics_;
    std::unique_ptr<PieceManager> piece_manager_;
    std::unique_ptr<CLIServer> cli_server_;
    std::unique_ptr<PowerMonitor> power_monitor_;
    
    // Power state
    std::atomic<bool> paused_for_battery_{false};

    // Timers for periodic tasks
    std::chrono::steady_clock::time_point last_disk_check_;
    std::chrono::steady_clock::time_point last_stats_save_;
    std::chrono::steady_clock::time_point last_metrics_update_;
    std::chrono::steady_clock::time_point last_rebalance_;

    /**
     * Initialize all components.
     */
    bool initialize_components();

    /**
     * Create PID file with current process ID.
     */
    void create_pid_file();

    /**
     * Remove PID file.
     */
    void remove_pid_file();

    /**
     * Setup signal handlers (SIGTERM, SIGINT, SIGHUP).
     */
    void setup_signal_handlers();

    /**
     * Close standard file descriptors (stdin, stdout, stderr).
     */
    void close_standard_fds();
};

} // namespace levin
