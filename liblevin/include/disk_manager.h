#pragma once

#include <cstdint>
#include <string>
#include <filesystem>

namespace levin {

struct DiskBudgetResult {
    uint64_t budget_bytes;
    uint64_t deficit_bytes;
    bool over_budget;
};

class DiskManager {
public:
    // Constructor for budget calculation
    // min_free_bytes: absolute minimum free space to preserve
    // min_free_pct: minimum free space as fraction of total (e.g. 0.05 = 5%)
    // max_storage: maximum bytes levin may use (0 = unlimited)
    DiskManager(uint64_t min_free_bytes = 0,
                double min_free_pct = 0.0,
                uint64_t max_storage = 0);

    // Pure calculation: given filesystem stats and current usage, compute budget
    DiskBudgetResult calculate(uint64_t fs_total, uint64_t fs_free,
                               uint64_t current_usage) const;

    // Delete files from directory until at least deficit_bytes are freed.
    // Returns actual bytes freed.
    uint64_t delete_to_free(const std::filesystem::path& dir, uint64_t deficit_bytes);

private:
    uint64_t min_free_bytes_;
    double min_free_pct_;
    uint64_t max_storage_;

    static constexpr uint64_t HYSTERESIS = 50ULL * 1024 * 1024; // 50 MB
};

} // namespace levin
