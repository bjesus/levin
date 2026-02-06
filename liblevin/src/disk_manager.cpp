#include "disk_manager.h"

#include <algorithm>
#include <random>
#include <vector>

namespace levin {

DiskManager::DiskManager(uint64_t min_free_bytes, double min_free_pct, uint64_t max_storage)
    : min_free_bytes_(min_free_bytes)
    , min_free_pct_(min_free_pct)
    , max_storage_(max_storage)
{
}

DiskBudgetResult DiskManager::calculate(uint64_t fs_total, uint64_t fs_free,
                                        uint64_t current_usage) const {
    // min_required = max(min_free_bytes, fs_total * min_free_percentage)
    uint64_t pct_bytes = static_cast<uint64_t>(static_cast<double>(fs_total) * min_free_pct_);
    uint64_t min_required = std::max(min_free_bytes_, pct_bytes);

    // available_space = max(0, fs_free - min_required)
    uint64_t available_space = (fs_free > min_required) ? (fs_free - min_required) : 0;

    uint64_t budget;
    bool over_budget;
    uint64_t deficit = 0;

    if (max_storage_ > 0) {
        // available_for_levin = max(0, max_storage - current_usage)
        uint64_t available_for_levin = (max_storage_ > current_usage)
            ? (max_storage_ - current_usage) : 0;
        budget = std::min(available_space, available_for_levin);
        over_budget = (current_usage > max_storage_) || (budget == 0);
        deficit = (current_usage > max_storage_) ? (current_usage - max_storage_) : 0;
    } else {
        budget = available_space;
        over_budget = (budget == 0);
        deficit = (min_required > fs_free) ? (min_required - fs_free) : 0;
    }

    // 50 MB hysteresis to prevent download-delete thrashing
    if (budget > HYSTERESIS) {
        budget -= HYSTERESIS;
    } else {
        budget = 0;
        over_budget = true;
    }

    return DiskBudgetResult{budget, deficit, over_budget};
}

uint64_t DiskManager::delete_to_free(const std::filesystem::path& dir, uint64_t deficit_bytes) {
    namespace fs = std::filesystem;

    if (deficit_bytes == 0) return 0;

    // Collect all regular files (recursive for multi-file torrents in subdirectories)
    std::vector<fs::path> files;
    std::error_code ec;
    for (auto& entry : fs::recursive_directory_iterator(dir, ec)) {
        if (entry.is_regular_file()) {
            files.push_back(entry.path());
        }
    }

    if (files.empty()) return 0;

    // Shuffle for random deletion order per design doc
    std::random_device rd;
    std::mt19937 rng(rd());
    std::shuffle(files.begin(), files.end(), rng);

    uint64_t freed = 0;
    for (const auto& f : files) {
        if (freed >= deficit_bytes) break;

        auto sz = fs::file_size(f, ec);
        if (ec) continue;

        if (fs::remove(f, ec) && !ec) {
            freed += sz;
        }
    }

    return freed;
}

} // namespace levin
