#pragma once

#include <string>
#include <cstdint>
#include <chrono>

namespace levin {

struct Config;

/**
 * Tracks and persists statistics (session and lifetime).
 */
class Statistics {
public:
    struct Stats {
        // Lifetime stats (persisted)
        uint64_t lifetime_downloaded_bytes;
        uint64_t lifetime_uploaded_bytes;
        uint64_t lifetime_uptime_seconds;
        std::chrono::system_clock::time_point first_start_time;
        int lifetime_session_count;
        
        // Session stats (reset on restart)
        uint64_t session_downloaded_bytes;
        uint64_t session_uploaded_bytes;
        std::chrono::system_clock::time_point session_start_time;
        
        // Current state
        int torrents_loaded;
        int pieces_have;
        int pieces_total;
        int peers_connected;
    };

    explicit Statistics(const Config& config);
    ~Statistics();

    /**
     * Load lifetime statistics from disk.
     */
    void load();

    /**
     * Save lifetime statistics to disk.
     */
    void save();

    /**
     * Update statistics from current session data.
     * 
     * @param downloaded Total downloaded in this session
     * @param uploaded Total uploaded in this session
     */
    void update(uint64_t downloaded, uint64_t uploaded);

    /**
     * Update current state counters.
     */
    void update_state(int torrents, int pieces_have, int pieces_total, int peers);

    /**
     * Get current statistics.
     */
    const Stats& get_stats() const { return stats_; }

    /**
     * Calculate session uptime in seconds.
     */
    uint64_t get_session_uptime_seconds() const;

private:
    const Config& config_;
    std::string stats_file_;
    Stats stats_;
    uint64_t last_reported_download_;
    uint64_t last_reported_upload_;
};

} // namespace levin
