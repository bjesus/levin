#pragma once

#include <libtorrent/session.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <string>
#include <memory>
#include <unordered_map>

namespace levin {

class Config;

/**
 * Wrapper around libtorrent::session.
 * Manages torrent session with WebRTC support.
 */
class Session {
public:
    explicit Session(const Config& config);
    ~Session();

    /**
     * Start the session.
     */
    bool start();

    /**
     * Stop the session and save state.
     */
    void stop();

    /**
     * Add a torrent from a .torrent file.
     * 
     * @param torrent_path Path to .torrent file
     * @return true if added successfully
     */
    bool add_torrent(const std::string& torrent_path);

    /**
     * Remove a torrent by info hash.
     * 
     * @param info_hash Info hash (hex string)
     */
    void remove_torrent(const std::string& info_hash);

    /**
     * Process libtorrent alerts.
     * Call this regularly from the main loop.
     */
    void process_alerts();

    /**
     * Get session statistics.
     */
    void get_stats(uint64_t& downloaded, uint64_t& uploaded, int& num_peers);
    
    /**
     * Get session statistics with rates.
     */
    void get_stats_with_rates(uint64_t& downloaded, uint64_t& uploaded, int& num_peers,
                               int& download_rate, int& upload_rate);

    /**
     * Get all torrent handles.
     */
    std::vector<lt::torrent_handle> get_torrents();

    /**
     * Get data directory.
     */
    const std::string& get_data_directory() const { return data_directory_; }

    /**
     * Convert info_hash to hex string (public for PieceManager).
     */
    static std::string info_hash_to_string(const lt::sha1_hash& hash);

    /**
     * Set download rate limit (in KB/s, 0 = unlimited).
     */
    void set_download_rate_limit(int kbps);

    /**
     * Set upload rate limit (in KB/s, 0 = unlimited).
     */
    void set_upload_rate_limit(int kbps);

    /**
     * Get current bandwidth limits (in KB/s).
     */
    void get_bandwidth_limits(int& download_kbps, int& upload_kbps);

private:
    const Config& config_;
    std::string data_directory_;
    std::string session_state_file_;
    
    std::unique_ptr<lt::session> session_;
    std::unordered_map<std::string, lt::torrent_handle> torrents_;  // info_hash -> handle

    /**
     * Configure session settings.
     */
    void configure_session();

    /**
     * Load session state from disk.
     */
    void load_session_state();

    /**
     * Save session state to disk.
     */
    void save_session_state();
};

} // namespace levin
