#pragma once

#include <string>
#include <cstdint>

namespace levin {

class Config;

/**
 * Monitors disk space and enforces limits.
 */
class DiskMonitor {
public:
    struct SpaceStatus {
        uint64_t total_bytes;           // Total disk space
        uint64_t free_bytes;            // Free space available
        uint64_t min_required_bytes;    // Minimum required (max of absolute/percentage)
        uint64_t current_usage_bytes;   // Current torrent data usage
        uint64_t budget_bytes;          // Available budget for torrents
        bool over_budget;               // True if over the limit
        uint64_t deficit_bytes;         // How much over budget (0 if under)
    };

    explicit DiskMonitor(const Config& config);

    /**
     * Check current disk space status.
     * 
     * @return SpaceStatus with current disk space information
     */
    SpaceStatus check_space();

    /**
     * Calculate how much torrent data we currently have.
     * This should be called with actual data from the piece manager.
     * 
     * @param torrent_data_bytes Current torrent data size
     */
    void set_current_usage(uint64_t torrent_data_bytes);

    /**
     * Get the data directory being monitored.
     */
    const std::string& get_data_directory() const { return data_directory_; }

private:
    const Config& config_;
    std::string data_directory_;
    uint64_t current_usage_bytes_;

    /**
     * Get filesystem statistics using statvfs.
     */
    bool get_filesystem_stats(uint64_t& total_bytes, uint64_t& free_bytes);
};

} // namespace levin
