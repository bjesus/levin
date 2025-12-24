#include "disk_monitor.hpp"
#include "config.hpp"
#include "logger.hpp"
#include "utils.hpp"
#include <sys/statvfs.h>
#include <algorithm>

namespace levin {

DiskMonitor::DiskMonitor(const Config& config)
    : config_(config)
    , data_directory_(config.paths.data_directory)
    , current_usage_bytes_(0) {
    LOG_INFO("DiskMonitor initialized for: {}", data_directory_);
}

bool DiskMonitor::get_filesystem_stats(uint64_t& total_bytes, uint64_t& free_bytes) {
    struct statvfs stat;
    
    if (statvfs(data_directory_.c_str(), &stat) != 0) {
        LOG_ERROR("Failed to get filesystem stats for: {}", data_directory_);
        return false;
    }
    
    total_bytes = static_cast<uint64_t>(stat.f_blocks) * stat.f_frsize;
    free_bytes = static_cast<uint64_t>(stat.f_bavail) * stat.f_frsize;  // Available to non-root
    
    return true;
}

void DiskMonitor::set_current_usage(uint64_t torrent_data_bytes) {
    current_usage_bytes_ = torrent_data_bytes;
}

DiskMonitor::SpaceStatus DiskMonitor::check_space() {
    SpaceStatus status{};
    
    // Get filesystem statistics
    if (!get_filesystem_stats(status.total_bytes, status.free_bytes)) {
        // If we can't get stats, assume we're over budget to be safe
        status.over_budget = true;
        return status;
    }
    
    // Calculate minimum required free space
    status.min_required_bytes = config_.get_effective_min_free_space(status.total_bytes);
    
    // Calculate budget (how much space we can use for torrents)
    if (status.free_bytes <= status.min_required_bytes) {
        status.budget_bytes = 0;
        status.over_budget = true;
        status.deficit_bytes = status.min_required_bytes - status.free_bytes;
    } else {
        status.budget_bytes = status.free_bytes - status.min_required_bytes;
        status.over_budget = false;
        status.deficit_bytes = 0;
    }
    
    status.current_usage_bytes = current_usage_bytes_;
    
    // Additional check: are we using more than our budget?
    if (current_usage_bytes_ > status.budget_bytes) {
        status.over_budget = true;
        status.deficit_bytes = current_usage_bytes_ - status.budget_bytes;
    }
    
    // Log status if over budget
    if (status.over_budget) {
        LOG_WARN("OVER BUDGET! Free: {}, Required: {}, Deficit: {}",
                 utils::format_bytes(status.free_bytes),
                 utils::format_bytes(status.min_required_bytes),
                 utils::format_bytes(status.deficit_bytes));
    }
    
    return status;
}

} // namespace levin
