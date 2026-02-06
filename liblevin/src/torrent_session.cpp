#include "torrent_session.h"

#ifndef LEVIN_USE_STUB_SESSION

#include <libtorrent/session.hpp>
#include <libtorrent/session_params.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/write_resume_data.hpp>
#include <libtorrent/read_resume_data.hpp>
#include <libtorrent/entry.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/torrent_status.hpp>
#include <libtorrent/session_stats.hpp>

#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <unordered_map>

namespace lt = libtorrent;
namespace fs = std::filesystem;

namespace levin {

// WebSocket trackers to inject per design doc
static const std::vector<std::string> WSS_TRACKERS = {
    "wss://tracker.openwebtorrent.com",
    "wss://tracker.webtorrent.dev",
    "wss://tracker.btorrent.xyz"
};

class RealTorrentSession : public ITorrentSession {
public:
    RealTorrentSession() = default;
    ~RealTorrentSession() override { stop(); }

    void configure(int port, const std::string& stun_server) override {
        port_ = port;
        stun_server_ = stun_server;
    }

    void start(const std::string& data_directory) override {
        if (running_) return;

        data_dir_ = data_directory;

        lt::settings_pack sp;
        sp.set_str(lt::settings_pack::listen_interfaces,
                   "0.0.0.0:" + std::to_string(port_));
        sp.set_bool(lt::settings_pack::enable_dht, true);
        sp.set_bool(lt::settings_pack::enable_lsd, true);
        sp.set_bool(lt::settings_pack::enable_upnp, true);
        sp.set_bool(lt::settings_pack::enable_natpmp, true);
        sp.set_int(lt::settings_pack::connections_limit, 200);
        sp.set_int(lt::settings_pack::max_connections_per_torrent, 50);

        // Alert mask
        sp.set_int(lt::settings_pack::alert_mask,
                   lt::alert_category::error
                   | lt::alert_category::status
                   | lt::alert_category::storage);

        // Note: WebTorrent/WebRTC settings (webtorrent_stun_server) are only
        // available on libtorrent master branch with -Dwebtorrent=ON.
        // When building with RC_2_0, we still inject WSS trackers on every
        // torrent so that WebTorrent-capable peers can find us via the tracker,
        // but true WebRTC data channel support requires master.
        // TODO: Enable when building from master:
        // sp.set_str(lt::settings_pack::webtorrent_stun_server, stun_server_);

        // Try to restore saved session state (DHT nodes, etc.)
        if (!pending_state_path_.empty() && fs::exists(pending_state_path_)) {
            try {
                std::ifstream f(pending_state_path_, std::ios::binary);
                std::string buf((std::istreambuf_iterator<char>(f)),
                                 std::istreambuf_iterator<char>());
                if (!buf.empty()) {
                    lt::session_params params = lt::read_session_params_buf(
                        lt::span<char const>(buf.data(), static_cast<int>(buf.size())));
                    // Merge our settings on top of the restored state
                    params.settings = sp;
                    session_ = std::make_unique<lt::session>(std::move(params));
                    running_ = true;
                    paused_ = false;
                    return;
                }
            } catch (const std::exception&) {
                // Fall through to fresh session creation
            }
        }

        session_ = std::make_unique<lt::session>(sp);
        running_ = true;
        paused_ = false;
    }

    void stop() override {
        if (!running_) return;
        session_.reset();
        running_ = false;
        paused_ = false;
        torrents_.clear();
    }

    bool is_running() const override { return running_; }

    std::optional<std::string> add_torrent(const std::string& torrent_path) override {
        if (!running_ || !session_) return std::nullopt;

        try {
            lt::add_torrent_params atp;
            auto ti = std::make_shared<lt::torrent_info>(torrent_path);
            atp.ti = ti;
            atp.save_path = data_dir_;

            // Inject WebSocket trackers at tier 0
            for (const auto& tracker : WSS_TRACKERS) {
                atp.trackers.push_back(tracker);
                atp.tracker_tiers.push_back(0);
            }

            lt::torrent_handle h = session_->add_torrent(atp);
            std::string hash = to_hex(h.info_hash());
            torrents_[hash] = h;
            return hash;
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }

    void remove_torrent(const std::string& info_hash) override {
        auto it = torrents_.find(info_hash);
        if (it != torrents_.end() && session_) {
            session_->remove_torrent(it->second);
            torrents_.erase(it);
        }
    }

    int torrent_count() const override {
        return static_cast<int>(torrents_.size());
    }

    std::vector<TorrentInfo> get_torrent_list() const override {
        std::vector<TorrentInfo> result;
        if (!session_) return result;
        for (const auto& [hash, handle] : torrents_) {
            if (!handle.is_valid()) continue;
            auto st = handle.status(lt::torrent_handle::query_name);
            TorrentInfo ti;
            ti.info_hash = hash;
            ti.name = st.name;
            ti.size = st.total_wanted;
            ti.downloaded = st.total_done;
            ti.uploaded = st.total_upload;
            ti.download_rate = st.download_rate;
            ti.upload_rate = st.upload_rate;
            ti.num_peers = st.num_peers;
            ti.progress = static_cast<double>(st.progress);
            ti.is_seed = st.is_seeding;
            result.push_back(std::move(ti));
        }
        return result;
    }

    void pause_session() override {
        if (session_) {
            session_->pause();
            paused_ = true;
        }
    }

    void resume_session() override {
        if (session_) {
            session_->resume();
            paused_ = false;
        }
    }

    bool is_paused() const override { return paused_; }

    void pause_downloads() override {
        set_download_rate_limit(1);
    }

    void resume_downloads() override {
        set_download_rate_limit(0);
    }

    void set_download_rate_limit(int bytes_per_sec) override {
        if (!session_) return;
        lt::settings_pack sp;
        sp.set_int(lt::settings_pack::download_rate_limit, bytes_per_sec);
        session_->apply_settings(sp);
        download_rate_limit_ = bytes_per_sec;
    }

    void set_upload_rate_limit(int bytes_per_sec) override {
        if (!session_) return;
        lt::settings_pack sp;
        sp.set_int(lt::settings_pack::upload_rate_limit, bytes_per_sec);
        session_->apply_settings(sp);
    }

    int get_download_rate_limit() const override {
        return download_rate_limit_;
    }

    int peer_count() const override {
        if (!session_) return 0;
        int total = 0;
        for (const auto& [hash, handle] : torrents_) {
            if (!handle.is_valid()) continue;
            auto st = handle.status(lt::torrent_handle::query_name);
            total += st.num_peers;
        }
        return total;
    }

    int download_rate() const override {
        if (!session_) return 0;
        int total = 0;
        for (const auto& [hash, handle] : torrents_) {
            if (!handle.is_valid()) continue;
            auto st = handle.status();
            total += st.download_rate;
        }
        return total;
    }

    int upload_rate() const override {
        if (!session_) return 0;
        int total = 0;
        for (const auto& [hash, handle] : torrents_) {
            if (!handle.is_valid()) continue;
            auto st = handle.status();
            total += st.upload_rate;
        }
        return total;
    }

    uint64_t total_downloaded() const override {
        if (!session_) return 0;
        uint64_t total = 0;
        for (const auto& [hash, handle] : torrents_) {
            if (!handle.is_valid()) continue;
            auto st = handle.status();
            total += st.total_download;
        }
        return total;
    }

    uint64_t total_uploaded() const override {
        if (!session_) return 0;
        uint64_t total = 0;
        for (const auto& [hash, handle] : torrents_) {
            if (!handle.is_valid()) continue;
            auto st = handle.status();
            total += st.total_upload;
        }
        return total;
    }

    bool is_webtorrent_enabled() const override {
        // On RC_2_0, WebTorrent is not natively supported.
        // We still inject WSS trackers, but true WebRTC requires master branch.
        // Return true if we have WSS trackers configured (our injection logic).
        return true;  // We always inject WSS trackers
    }

    std::vector<std::string> get_trackers(const std::string& info_hash) const override {
        auto it = torrents_.find(info_hash);
        if (it == torrents_.end()) return {};

        std::vector<std::string> result;
        auto trackers = it->second.trackers();
        for (const auto& t : trackers) {
            result.push_back(t.url);
        }
        return result;
    }

    void save_state(const std::string& path) override {
        if (!session_) return;
        auto params = session_->session_state();
        auto buf = lt::write_session_params_buf(params);
        std::ofstream f(path, std::ios::binary);
        if (f.is_open()) {
            f.write(buf.data(), static_cast<std::streamsize>(buf.size()));
        }
    }

    void load_state(const std::string& path) override {
        // Store the path so start() can load saved state at session creation time.
        // For RC_2_0, state must be loaded via session_params at construction.
        pending_state_path_ = path;
    }

private:
    static std::string to_hex(const lt::sha1_hash& hash) {
        std::ostringstream oss;
        oss << hash;
        return oss.str();
    }

    std::unique_ptr<lt::session> session_;
    std::unordered_map<std::string, lt::torrent_handle> torrents_;
    std::string data_dir_;
    int port_ = 6881;
    std::string stun_server_ = "stun.l.google.com:19302";
    bool running_ = false;
    bool paused_ = false;
    int download_rate_limit_ = 0;
    std::string pending_state_path_;
};

// Factory function to create the real session
std::unique_ptr<ITorrentSession> create_real_torrent_session() {
    return std::make_unique<RealTorrentSession>();
}

} // namespace levin

#endif // LEVIN_USE_STUB_SESSION
