#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace levin {

struct TorrentInfo {
    std::string info_hash;
    std::string name;
    uint64_t size;
    uint64_t downloaded;
    uint64_t uploaded;
    int download_rate;
    int upload_rate;
    int num_peers;
    double progress;
    bool is_seed;
};

// Abstract interface for torrent session -- allows stub and real implementations
class ITorrentSession {
public:
    virtual ~ITorrentSession() = default;

    virtual void configure(int port, const std::string& stun_server) = 0;
    virtual void start(const std::string& data_directory) = 0;
    virtual void stop() = 0;
    virtual bool is_running() const = 0;

    // Torrent management
    virtual std::optional<std::string> add_torrent(const std::string& torrent_path) = 0;
    virtual void remove_torrent(const std::string& info_hash) = 0;
    virtual int torrent_count() const = 0;

    // Torrent listing
    virtual std::vector<TorrentInfo> get_torrent_list() const = 0;

    // Session control
    virtual void pause_session() = 0;
    virtual void resume_session() = 0;
    virtual bool is_paused() const = 0;

    // Download rate control
    virtual void pause_downloads() = 0;   // set download rate to 1 byte/sec
    virtual void resume_downloads() = 0;  // restore configured rate
    virtual void set_download_rate_limit(int bytes_per_sec) = 0;
    virtual void set_upload_rate_limit(int bytes_per_sec) = 0;
    virtual int get_download_rate_limit() const = 0;

    // Stats
    virtual int peer_count() const = 0;
    virtual int download_rate() const = 0;
    virtual int upload_rate() const = 0;
    virtual uint64_t total_downloaded() const = 0;
    virtual uint64_t total_uploaded() const = 0;

    // WebTorrent
    virtual bool is_webtorrent_enabled() const = 0;
    virtual std::vector<std::string> get_trackers(const std::string& info_hash) const = 0;

    // Session state persistence
    virtual void save_state(const std::string& path) = 0;
    virtual void load_state(const std::string& path) = 0;
};

// Stub implementation for testing without libtorrent
class StubTorrentSession : public ITorrentSession {
public:
    void configure(int port, const std::string& stun_server) override;
    void start(const std::string& data_directory) override;
    void stop() override;
    bool is_running() const override;

    std::optional<std::string> add_torrent(const std::string& torrent_path) override;
    void remove_torrent(const std::string& info_hash) override;
    int torrent_count() const override;

    std::vector<TorrentInfo> get_torrent_list() const override;

    void pause_session() override;
    void resume_session() override;
    bool is_paused() const override;

    void pause_downloads() override;
    void resume_downloads() override;
    void set_download_rate_limit(int bytes_per_sec) override;
    void set_upload_rate_limit(int bytes_per_sec) override;
    int get_download_rate_limit() const override;

    int peer_count() const override;
    int download_rate() const override;
    int upload_rate() const override;
    uint64_t total_downloaded() const override;
    uint64_t total_uploaded() const override;

    bool is_webtorrent_enabled() const override;
    std::vector<std::string> get_trackers(const std::string& info_hash) const override;

    void save_state(const std::string& path) override;
    void load_state(const std::string& path) override;

private:
    bool running_ = false;
    bool paused_ = false;
    int download_rate_limit_ = 0;
    int upload_rate_limit_ = 0;
    int torrent_count_ = 0;
};

// Factory for real libtorrent session (only available when built with libtorrent)
#ifndef LEVIN_USE_STUB_SESSION
std::unique_ptr<ITorrentSession> create_real_torrent_session();
#endif

} // namespace levin
