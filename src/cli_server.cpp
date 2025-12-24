#include "cli_server.hpp"
#include "config.hpp"
#include "session.hpp"
#include "statistics.hpp"
#include "piece_manager.hpp"
#include "disk_monitor.hpp"
#include "logger.hpp"
#include "utils.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

namespace levin {

CLIServer::CLIServer(const Config& config,
                     Session& session,
                     Statistics& statistics,
                     PieceManager& piece_manager,
                     DiskMonitor& disk_monitor)
    : config_(config)
    , session_(session)
    , statistics_(statistics)
    , piece_manager_(piece_manager)
    , disk_monitor_(disk_monitor)
    , socket_path_(config.cli.control_socket)
    , server_fd_(-1)
    , running_(false) {
}

CLIServer::~CLIServer() {
    stop();
}

bool CLIServer::start() {
    LOG_INFO("Starting CLI server on: {}", socket_path_);

    // Remove old socket if it exists
    unlink(socket_path_.c_str());

    // Create Unix socket
    server_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        LOG_ERROR("Failed to create Unix socket: {}", std::strerror(errno));
        return false;
    }

    // Set non-blocking
    int flags = fcntl(server_fd_, F_GETFL, 0);
    fcntl(server_fd_, F_SETFL, flags | O_NONBLOCK);

    // Bind to socket path
    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("Failed to bind socket: {}", std::strerror(errno));
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    // Listen
    if (listen(server_fd_, 5) < 0) {
        LOG_ERROR("Failed to listen on socket: {}", std::strerror(errno));
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    running_ = true;
    LOG_INFO("CLI server started successfully");
    return true;
}

void CLIServer::stop() {
    if (running_) {
        running_ = false;
        if (server_fd_ >= 0) {
            close(server_fd_);
            server_fd_ = -1;
        }
        unlink(socket_path_.c_str());
        LOG_INFO("CLI server stopped");
    }
}

void CLIServer::process_commands() {
    if (!running_ || server_fd_ < 0) return;

    // Accept client connection (non-blocking)
    int client_fd = accept(server_fd_, nullptr, nullptr);
    if (client_fd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            LOG_ERROR("Error accepting client: {}", std::strerror(errno));
        }
        return;
    }

    handle_client(client_fd);
}

void CLIServer::handle_client(int client_fd) {
    // Read request
    char buffer[4096];
    ssize_t n = read(client_fd, buffer, sizeof(buffer) - 1);
    
    if (n <= 0) {
        close(client_fd);
        return;
    }
    
    buffer[n] = '\0';

    try {
        json request = json::parse(buffer);
        json response = handle_command(request);
        
        std::string response_str = response.dump();
        write(client_fd, response_str.c_str(), response_str.length());
        
    } catch (const std::exception& e) {
        json error = {
            {"success", false},
            {"error", e.what()}
        };
        std::string error_str = error.dump();
        write(client_fd, error_str.c_str(), error_str.length());
    }

    close(client_fd);
}

json CLIServer::handle_command(const json& request) {
    std::string command = request.value("command", "");
    json args = request.value("args", json::object());

    LOG_DEBUG("CLI command: {}", command);

    if (command == "status") {
        return handle_status();
    } else if (command == "list") {
        return handle_list();
    } else if (command == "stats") {
        return handle_stats();
    } else if (command == "pause") {
        return handle_pause();
    } else if (command == "resume") {
        return handle_resume();
    } else if (command == "bandwidth") {
        return handle_bandwidth(args);
    } else {
        return {
            {"success", false},
            {"error", "Unknown command: " + command}
        };
    }
}

json CLIServer::handle_status() {
    auto space_status = disk_monitor_.check_space();
    const auto& stats = statistics_.get_stats();
    const auto& metrics = piece_manager_.get_metrics();

    uint64_t downloaded, uploaded;
    int peers;
    session_.get_stats(downloaded, uploaded, peers);

    int total_pieces = 0, pieces_we_have = 0;
    for (const auto& [hash, m] : metrics) {
        total_pieces += m.total_pieces;
        pieces_we_have += m.pieces_we_have;
    }

    return {
        {"success", true},
        {"data", {
            {"uptime_seconds", statistics_.get_session_uptime_seconds()},
            {"disk", {
                {"total_bytes", space_status.total_bytes},
                {"free_bytes", space_status.free_bytes},
                {"used_bytes", space_status.current_usage_bytes},
                {"min_required_bytes", space_status.min_required_bytes},
                {"budget_bytes", space_status.budget_bytes},
                {"over_budget", space_status.over_budget}
            }},
            {"torrents", {
                {"total_loaded", metrics.size()},
                {"total_pieces", total_pieces},
                {"pieces_we_have", pieces_we_have}
            }},
            {"network", {
                {"download_rate", 0},  // TODO: Calculate rate
                {"upload_rate", 0},
                {"total_downloaded", downloaded},
                {"total_uploaded", uploaded},
                {"peers_connected", peers}
            }}
        }}
    };
}

json CLIServer::handle_list() {
    const auto& metrics = piece_manager_.get_metrics();
    json torrents = json::array();

    for (const auto& [hash, m] : metrics) {
        torrents.push_back({
            {"info_hash", hash},
            {"name", m.name},
            {"seeders", m.total_seeders},
            {"pieces_total", m.total_pieces},
            {"pieces_have", m.pieces_we_have},
            {"size_total", m.total_size},
            {"size_have", m.size_we_have},
            {"priority", m.torrent_priority}
        });
    }

    return {
        {"success", true},
        {"data", {
            {"torrents", torrents}
        }}
    };
}

json CLIServer::handle_stats() {
    const auto& stats = statistics_.get_stats();

    return {
        {"success", true},
        {"data", {
            {"session", {
                {"downloaded", stats.session_downloaded_bytes},
                {"uploaded", stats.session_uploaded_bytes},
                {"uptime", statistics_.get_session_uptime_seconds()}
            }},
            {"lifetime", {
                {"downloaded", stats.lifetime_downloaded_bytes},
                {"uploaded", stats.lifetime_uploaded_bytes},
                {"uptime", stats.lifetime_uptime_seconds},
                {"sessions", stats.lifetime_session_count}
            }}
        }}
    };
}

json CLIServer::handle_pause() {
    if (pause_callback_) {
        pause_callback_();
    }
    return {
        {"success", true},
        {"message", "Paused all torrent activity"}
    };
}

json CLIServer::handle_resume() {
    if (resume_callback_) {
        resume_callback_();
    }
    return {
        {"success", true},
        {"message", "Resumed torrent activity"}
    };
}

json CLIServer::handle_bandwidth(const json& args) {
    // Check if we're setting or getting bandwidth limits
    bool has_download = args.contains("download");
    bool has_upload = args.contains("upload");
    
    if (has_download || has_upload) {
        // Set bandwidth limits
        if (has_download) {
            int download_kbps = args["download"].get<int>();
            session_.set_download_rate_limit(download_kbps);
        }
        
        if (has_upload) {
            int upload_kbps = args["upload"].get<int>();
            session_.set_upload_rate_limit(upload_kbps);
        }
        
        return {
            {"success", true},
            {"message", "Bandwidth limits updated"}
        };
    } else {
        // Get current bandwidth limits
        int download_kbps = 0;
        int upload_kbps = 0;
        session_.get_bandwidth_limits(download_kbps, upload_kbps);
        
        return {
            {"success", true},
            {"download_limit_kbps", download_kbps},
            {"upload_limit_kbps", upload_kbps}
        };
    }
}

void CLIServer::set_pause_callback(std::function<void()> callback) {
    pause_callback_ = callback;
}

void CLIServer::set_resume_callback(std::function<void()> callback) {
    resume_callback_ = callback;
}

} // namespace levin
