#pragma once

#include <string>
#include <cstdint>

namespace levin {

struct Config;

/**
 * Monitors disk space and enforces limits.
 */
class DiskMonitor {
public:
    struct SpaceStatus {
        uint64_t total_bytes;           // Total disk space
        uint64_t free_bytes;            // Free space available
        uint64_t min_required_bytes;    // Minimum required (max of absolute/percentage)
        uint64_t current_usage_bytes;   // Current actual disk usage
        uint64_t budget_bytes;          // Available budget for torrents
        bool over_budget;               // True if over the limit
        uint64_t deficit_bytes;         // How much over budget (0 if under)
    };

    explicit DiskMonitor(const Config& config);

    /**
     * Check current disk space status.
     * Calculates actual disk usage internally.
     * 
     * @return SpaceStatus with current disk space information
     */
    SpaceStatus check_space();

    /**
     * Get the data directory being monitored.
     */
    const std::string& get_data_directory() const { return data_directory_; }

private:
    const Config& config_;
    std::string data_directory_;

    /**
     * Get filesystem statistics using statvfs.
     */
    bool get_filesystem_stats(uint64_t& total_bytes, uint64_t& free_bytes);
    
    /**
     * Calculate actual disk usage of the data directory.
     * Uses stat() to get actual block usage, handling sparse files correctly.
     * 
     * @return Actual disk space consumed in bytes
     */
    uint64_t calculate_actual_disk_usage();
};

} // namespace levin
