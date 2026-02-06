#include <catch2/catch_test_macros.hpp>
#include "disk_manager.h"

using namespace levin;

constexpr uint64_t GB = 1024ULL * 1024 * 1024;
constexpr uint64_t MB = 1024ULL * 1024;

TEST_CASE("Under limit: budget is positive") {
    DiskManager dm(1*GB, 0.05, 100*GB);
    auto r = dm.calculate(500*GB, 400*GB, 10*GB);
    REQUIRE(!r.over_budget);
    REQUIRE(r.budget_bytes > 0);
}

TEST_CASE("Over max_storage: over budget with correct deficit") {
    DiskManager dm(1*GB, 0.05, 100*GB);
    auto r = dm.calculate(500*GB, 400*GB, 120*GB);
    REQUIRE(r.over_budget);
    REQUIRE(r.deficit_bytes == 20*GB);
}

TEST_CASE("Filesystem nearly full: over budget even if under max_storage") {
    DiskManager dm(10*GB, 0.05, 100*GB);
    // min_required = max(10GB, 500GB*5%) = 25GB. Only 5GB free.
    auto r = dm.calculate(500*GB, 5*GB, 50*GB);
    REQUIRE(r.over_budget);
}

TEST_CASE("Unlimited max_storage: only min_free matters") {
    DiskManager dm(1*GB, 0.05, 0);
    auto r = dm.calculate(500*GB, 400*GB, 200*GB);
    REQUIRE(!r.over_budget);
    REQUIRE(r.budget_bytes > 0);
}

TEST_CASE("Hysteresis subtracted from budget") {
    DiskManager dm(1*GB, 0.0, 100*GB);
    auto r = dm.calculate(500*GB, 400*GB, 80*GB);
    // available_for_levin = 20GB, available_space = 399GB
    // budget = 20GB - 50MB
    REQUIRE(r.budget_bytes == 20*GB - 50*MB);
}

TEST_CASE("Within hysteresis buffer: budget zero, over_budget true") {
    DiskManager dm(1*GB, 0.0, 100*GB);
    auto r = dm.calculate(500*GB, 400*GB, 100*GB - 30*MB);
    REQUIRE(r.over_budget);
    REQUIRE(r.budget_bytes == 0);
}

TEST_CASE("Budget is minimum of both constraints") {
    DiskManager dm(1*GB, 0.0, 100*GB);
    // 10GB free, 50GB used: fs constraint = 9GB, storage constraint = 50GB
    auto r = dm.calculate(500*GB, 10*GB, 50*GB);
    REQUIRE(r.budget_bytes < 10*GB);
    REQUIRE(r.budget_bytes > 8*GB);
}

TEST_CASE("Zero usage: full budget available") {
    DiskManager dm(1*GB, 0.0, 100*GB);
    auto r = dm.calculate(500*GB, 400*GB, 0);
    REQUIRE(!r.over_budget);
    REQUIRE(r.budget_bytes == 100*GB - 50*MB);
}
