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
    
    // Calculate available space respecting minimum free space
    uint64_t available_space = (status.free_bytes > status.min_required_bytes) 
        ? status.free_bytes - status.min_required_bytes 
        : 0;
    
    status.current_usage_bytes = current_usage_bytes_;
    
    // Apply max_storage constraint if set (0 = unlimited)
    if (config_.disk.max_storage > 0) {
        // Calculate how much more we can use within the max_storage limit
        uint64_t available_for_levin = (current_usage_bytes_ < config_.disk.max_storage)
            ? config_.disk.max_storage - current_usage_bytes_
            : 0;
        
        // Budget is the minimum of both constraints
        status.budget_bytes = std::min(available_space, available_for_levin);
        
        // Over budget if exceeded max storage
        if (current_usage_bytes_ > config_.disk.max_storage) {
            status.over_budget = true;
            status.deficit_bytes = current_usage_bytes_ - config_.disk.max_storage;
        } else if (status.budget_bytes == 0 && available_space > 0) {
            // We hit the max_storage limit but still have disk space
            status.over_budget = true;
            status.deficit_bytes = 0;
        } else {
            status.over_budget = false;
            status.deficit_bytes = 0;
        }
    } else {
        // Unlimited storage mode - only respect min_free constraint
        status.budget_bytes = available_space;
        
        if (available_space == 0) {
            status.over_budget = true;
            status.deficit_bytes = status.min_required_bytes - status.free_bytes;
        } else if (current_usage_bytes_ > 0 && current_usage_bytes_ > status.budget_bytes) {
            // Current usage exceeds budget calculated from min_free constraint
            status.over_budget = true;
            status.deficit_bytes = current_usage_bytes_ - status.budget_bytes;
        } else {
            status.over_budget = false;
            status.deficit_bytes = 0;
        }
    }
    
    // Log status if over budget
    if (status.over_budget) {
        if (config_.disk.max_storage > 0 && current_usage_bytes_ > config_.disk.max_storage) {
            LOG_WARN("OVER MAX STORAGE! Usage: {}, Max: {}, Deficit: {}",
                     utils::format_bytes(current_usage_bytes_),
                     utils::format_bytes(config_.disk.max_storage),
                     utils::format_bytes(status.deficit_bytes));
        } else {
            LOG_WARN("OVER BUDGET! Free: {}, Required: {}, Deficit: {}",
                     utils::format_bytes(status.free_bytes),
                     utils::format_bytes(status.min_required_bytes),
                     utils::format_bytes(status.deficit_bytes));
        }
    }
    
    return status;
}

} // namespace levin
