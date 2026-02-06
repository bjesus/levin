#pragma once
#include <cstdint>
#include <string>

namespace levin {

/**
 * Persistent statistics that survive across restarts.
 * Stored as a simple binary file in the state directory.
 */
struct Statistics {
    uint64_t total_downloaded = 0;
    uint64_t total_uploaded = 0;
    uint64_t session_downloaded = 0;  // Current session only
    uint64_t session_uploaded = 0;    // Current session only

    // Load stats from file. Returns false if file doesn't exist or is corrupt.
    bool load(const std::string& path);

    // Save stats to file. Returns false on write error.
    bool save(const std::string& path) const;

    // Update session counters and recompute totals.
    // base_downloaded/uploaded are the cumulative values from before this session.
    void update(uint64_t base_downloaded, uint64_t base_uploaded,
                uint64_t current_session_downloaded, uint64_t current_session_uploaded);
};

} // namespace levin
