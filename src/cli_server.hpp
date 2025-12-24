#pragma once

#include <string>
#include <functional>
#include <nlohmann/json.hpp>

namespace levin {

class Config;
class Session;
class Statistics;
class PieceManager;
class DiskMonitor;

using json = nlohmann::json;

/**
 * Unix socket server for CLI communication.
 * Handles commands from the CLI client.
 */
class CLIServer {
public:
    CLIServer(const Config& config, 
              Session& session,
              Statistics& statistics,
              PieceManager& piece_manager,
              DiskMonitor& disk_monitor);
    ~CLIServer();

    /**
     * Start the Unix socket server.
     */
    bool start();

    /**
     * Stop the server.
     */
    void stop();

    /**
     * Process incoming commands (non-blocking).
     * Call this regularly from the main loop.
     */
    void process_commands();

    /**
     * Set callback for pause command.
     */
    void set_pause_callback(std::function<void()> callback);

    /**
     * Set callback for resume command.
     */
    void set_resume_callback(std::function<void()> callback);
    
    /**
     * Set callback to check if paused for battery.
     */
    void set_paused_for_battery_callback(std::function<bool()> callback);
    
    /**
     * Set callback for terminate command.
     */
    void set_terminate_callback(std::function<void()> callback);

private:
    const Config& config_;
    Session& session_;
    Statistics& statistics_;
    PieceManager& piece_manager_;
    DiskMonitor& disk_monitor_;

    std::string socket_path_;
    int server_fd_;
    bool running_;

    std::function<void()> pause_callback_;
    std::function<void()> resume_callback_;
    std::function<bool()> paused_for_battery_callback_;
    std::function<void()> terminate_callback_;

    /**
     * Handle a command from a client.
     */
    json handle_command(const json& request);

    /**
     * Command handlers
     */
    json handle_status();
    json handle_list();
    json handle_stats();
    json handle_pause();
    json handle_resume();
    json handle_bandwidth(const json& args);
    json handle_terminate();

    /**
     * Accept and handle a client connection.
     */
    void handle_client(int client_fd);
};

} // namespace levin
