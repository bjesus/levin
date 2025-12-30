#include "session.hpp"
#include "config.hpp"
#include "logger.hpp"
#include <libtorrent/session.hpp>
#include <libtorrent/session_params.hpp>
#include <libtorrent/session_stats.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/write_resume_data.hpp>
#include <libtorrent/read_resume_data.hpp>
#include <libtorrent/load_torrent.hpp>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace lt = libtorrent;

namespace levin {

Session::Session(const Config& config)
    : config_(config)
    , data_directory_(config.paths.data_directory)
    , session_state_file_(config.session_state()) {
}

Session::~Session() {
    stop();
}

bool Session::start() {
    LOG_INFO("Starting libtorrent session");

    try {
        // Create session parameters
        lt::settings_pack settings;

        configure_session();

        // Create session
        session_ = std::make_unique<lt::session>(settings);

        // Load session state if it exists
        load_session_state();

        LOG_INFO("Libtorrent session started successfully");
        LOG_INFO("DHT enabled: {}", Config::enable_dht);
        LOG_INFO("Listening on port: {}", Config::listen_port);
#ifdef WEBTORRENT_ENABLED
        LOG_INFO("WebTorrent support: ENABLED");
        LOG_INFO("WebTorrent STUN server: {}", config_.webtorrent.stun_server);
#else
        LOG_INFO("WebTorrent support: DISABLED");
#endif

        return true;

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to start libtorrent session: {}", e.what());
        return false;
    }
}

void Session::stop() {
    if (session_) {
        LOG_INFO("Stopping libtorrent session");

        // Save session state
        save_session_state();

        // Wait for alerts to be processed
        session_->pause();

        session_.reset();
        LOG_INFO("Libtorrent session stopped");
    }
}

void Session::configure_session() {
    lt::settings_pack settings;

    // Basic settings
    settings.set_int(lt::settings_pack::alert_mask,
                    lt::alert_category::error |
                    lt::alert_category::status |
                    lt::alert_category::storage);

    // Connection limits (from hardcoded constants)
    settings.set_int(lt::settings_pack::connections_limit, Config::max_total_connections);
    settings.set_int(lt::settings_pack::active_downloads, Config::max_active_downloads);
    settings.set_int(lt::settings_pack::active_seeds, Config::max_active_seeds);
    settings.set_int(lt::settings_pack::active_limit, Config::max_active_torrents);

    // Bandwidth limits (from config)
    if (config_.limits.max_download_rate_kbps > 0) {
        settings.set_int(lt::settings_pack::download_rate_limit, 
                        config_.limits.max_download_rate_kbps * 1024);
    }
    if (config_.limits.max_upload_rate_kbps > 0) {
        settings.set_int(lt::settings_pack::upload_rate_limit,
                        config_.limits.max_upload_rate_kbps * 1024);
    }

    // Port - use string format: "0.0.0.0:port" (hardcoded)
    std::string listen_interface = "0.0.0.0:" + std::to_string(Config::listen_port);
    settings.set_str(lt::settings_pack::listen_interfaces, listen_interface);

    // DHT (always enabled)
    settings.set_bool(lt::settings_pack::enable_dht, Config::enable_dht);

    // LSD (always enabled)
    settings.set_bool(lt::settings_pack::enable_lsd, Config::enable_lsd);

    // UPnP/NAT-PMP (always enabled)
    settings.set_bool(lt::settings_pack::enable_upnp, Config::enable_upnp);
    settings.set_bool(lt::settings_pack::enable_natpmp, Config::enable_natpmp);
    
#ifdef WEBTORRENT_ENABLED
    // WebTorrent STUN server for NAT traversal
    settings.set_str(lt::settings_pack::webtorrent_stun_server, config_.webtorrent.stun_server);
#endif

    // Apply to session
    if (session_) {
        session_->apply_settings(settings);
    }
}

void Session::load_session_state() {
    std::ifstream file(session_state_file_, std::ios::binary);
    if (!file.is_open()) {
        LOG_INFO("No existing session state found");
        return;
    }

    try {
        std::vector<char> buf((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
        
        lt::bdecode_node node;
        lt::error_code ec;
        lt::bdecode(buf.data(), buf.data() + buf.size(), node, ec);

        if (ec) {
            LOG_WARN("Failed to decode session state: {}", ec.message());
            return;
        }

        if (session_) {
            lt::session_params params = lt::read_session_params(node);
            session_->apply_settings(params.settings);
        }

        LOG_INFO("Loaded session state");

    } catch (const std::exception& e) {
        LOG_WARN("Failed to load session state: {}", e.what());
    }
}

void Session::save_session_state() {
    if (!session_) {
        return;
    }

    try {
        // Get session state as session_params
        lt::session_params params = session_->session_state();
        
        // Write session params to disk
        lt::entry e = lt::write_session_params(params);
        
        std::vector<char> buf;
        lt::bencode(std::back_inserter(buf), e);

        std::ofstream file(session_state_file_, std::ios::binary);
        if (!file.is_open()) {
            LOG_ERROR("Failed to open session state file for writing");
            return;
        }

        file.write(buf.data(), buf.size());
        LOG_INFO("Saved session state");

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to save session state: {}", e.what());
    }
}

// WebSocket trackers for WebTorrent support
static const std::vector<std::string> WEBTORRENT_TRACKERS = {
    "wss://tracker.openwebtorrent.com",
    "wss://tracker.webtorrent.dev",
    "wss://tracker.btorrent.xyz"
};

bool Session::add_torrent(const std::string& torrent_path) {
    if (!session_) {
        LOG_ERROR("Session not initialized");
        return false;
    }

    try {
        // Load torrent file using libtorrent's load_torrent_file function
        lt::add_torrent_params params = lt::load_torrent_file(torrent_path);
        params.save_path = data_directory_;

        lt::torrent_handle handle = session_->add_torrent(params);
        
        if (!handle.is_valid()) {
            LOG_ERROR("Failed to add torrent: invalid handle");
            return false;
        }

#ifdef WEBTORRENT_ENABLED
        // Inject WebSocket trackers for WebTorrent support
        for (const auto& tracker_url : WEBTORRENT_TRACKERS) {
            lt::announce_entry tracker(tracker_url);
            tracker.tier = 0;  // tier 0 = highest priority
            handle.add_tracker(tracker);
        }
        LOG_DEBUG("Added {} WebSocket trackers for WebTorrent", WEBTORRENT_TRACKERS.size());
#endif

        std::string info_hash = info_hash_to_string(handle.info_hash());
        torrents_[info_hash] = handle;

        LOG_INFO("Added torrent: {} ({})", params.ti->name(), info_hash);
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to add torrent {}: {}", torrent_path, e.what());
        return false;
    }
}

void Session::remove_torrent(const std::string& info_hash) {
    auto it = torrents_.find(info_hash);
    if (it == torrents_.end()) {
        LOG_WARN("Torrent not found: {}", info_hash);
        return;
    }

    if (session_) {
        session_->remove_torrent(it->second);
    }

    torrents_.erase(it);
    LOG_INFO("Removed torrent: {}", info_hash);
}

void Session::process_alerts() {
    if (!session_) {
        return;
    }

    std::vector<lt::alert*> alerts;
    session_->pop_alerts(&alerts);

    for (lt::alert* alert : alerts) {
        // Log important alerts
        if (lt::alert_cast<lt::torrent_error_alert>(alert)) {
            LOG_ERROR("Torrent error: {}", alert->message());
        } else if (lt::alert_cast<lt::torrent_finished_alert>(alert)) {
            LOG_INFO("Torrent finished: {}", alert->message());
        } else if (lt::alert_cast<lt::add_torrent_alert>(alert)) {
            LOG_DEBUG("Torrent added: {}", alert->message());
        }
    }
}

void Session::get_stats(uint64_t& downloaded, uint64_t& uploaded, int& num_peers) {
    downloaded = 0;
    uploaded = 0;
    num_peers = 0;

    if (!session_) {
        return;
    }

    // In libtorrent 2.x, use post_session_stats() and get from alerts
    // For now, aggregate from all torrents
    auto torrents = session_->get_torrents();
    for (const auto& handle : torrents) {
        if (!handle.is_valid()) continue;
        
        lt::torrent_status status = handle.status();
        downloaded += status.all_time_download;
        uploaded += status.all_time_upload;
        num_peers += status.num_peers;
    }
}

void Session::get_stats_with_rates(uint64_t& downloaded, uint64_t& uploaded, int& num_peers,
                                    int& download_rate, int& upload_rate) {
    downloaded = 0;
    uploaded = 0;
    num_peers = 0;
    download_rate = 0;
    upload_rate = 0;

    if (!session_) {
        return;
    }

    auto torrents = session_->get_torrents();
    for (const auto& handle : torrents) {
        if (!handle.is_valid()) continue;
        
        lt::torrent_status status = handle.status();
        downloaded += status.all_time_download;
        uploaded += status.all_time_upload;
        num_peers += status.num_peers;
        download_rate += status.download_rate;
        upload_rate += status.upload_rate;
    }
}

std::vector<lt::torrent_handle> Session::get_torrents() {
    std::vector<lt::torrent_handle> handles;
    if (session_) {
        handles = session_->get_torrents();
    }
    return handles;
}

void Session::pause() {
    if (!session_) {
        LOG_WARN("Cannot pause: session not initialized");
        return;
    }
    
    LOG_INFO("Pausing libtorrent session (all network activity will stop)");
    session_->pause();
}

void Session::resume() {
    if (!session_) {
        LOG_WARN("Cannot resume: session not initialized");
        return;
    }
    
    LOG_INFO("Resuming libtorrent session");
    session_->resume();
}

std::string Session::info_hash_to_string(const lt::sha1_hash& hash) {
    std::ostringstream oss;
    oss << hash;
    return oss.str();
}

void Session::set_download_rate_limit(int kbps) {
    if (!session_) return;
    
    lt::settings_pack settings;
    settings.set_int(lt::settings_pack::download_rate_limit, kbps > 0 ? kbps * 1024 : 0);
    session_->apply_settings(settings);
    
    LOG_INFO("Set download rate limit to {} KB/s", kbps);
}

void Session::set_upload_rate_limit(int kbps) {
    if (!session_) return;
    
    lt::settings_pack settings;
    settings.set_int(lt::settings_pack::upload_rate_limit, kbps > 0 ? kbps * 1024 : 0);
    session_->apply_settings(settings);
    
    LOG_INFO("Set upload rate limit to {} KB/s", kbps);
}

void Session::get_bandwidth_limits(int& download_kbps, int& upload_kbps) {
    if (!session_) {
        download_kbps = 0;
        upload_kbps = 0;
        return;
    }
    
    lt::settings_pack settings = session_->get_settings();
    int dl_limit = settings.get_int(lt::settings_pack::download_rate_limit);
    int ul_limit = settings.get_int(lt::settings_pack::upload_rate_limit);
    
    download_kbps = dl_limit > 0 ? dl_limit / 1024 : 0;
    upload_kbps = ul_limit > 0 ? ul_limit / 1024 : 0;
}

} // namespace levin
