#include <catch2/catch_test_macros.hpp>
#include "statistics.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

TEST_CASE("Statistics save and load round-trip", "[statistics]") {
    auto tmp = fs::temp_directory_path() / "levin_test_stats";
    fs::create_directories(tmp);
    std::string path = (tmp / "stats.dat").string();

    // Clean up
    fs::remove(path);

    levin::Statistics stats;
    stats.total_downloaded = 123456789;
    stats.total_uploaded = 987654321;

    REQUIRE(stats.save(path));

    levin::Statistics loaded;
    REQUIRE(loaded.load(path));
    CHECK(loaded.total_downloaded == 123456789);
    CHECK(loaded.total_uploaded == 987654321);

    fs::remove_all(tmp);
}

TEST_CASE("Statistics load returns false for missing file", "[statistics]") {
    levin::Statistics stats;
    REQUIRE_FALSE(stats.load("/nonexistent/path/stats.dat"));
}

TEST_CASE("Statistics load returns false for corrupt file", "[statistics]") {
    auto tmp = fs::temp_directory_path() / "levin_test_stats_corrupt";
    fs::create_directories(tmp);
    std::string path = (tmp / "stats.dat").string();

    // Write garbage
    std::ofstream f(path, std::ios::binary);
    f << "garbage data that is not valid";
    f.close();

    levin::Statistics stats;
    REQUIRE_FALSE(stats.load(path));

    fs::remove_all(tmp);
}

TEST_CASE("Statistics update computes cumulative totals", "[statistics]") {
    levin::Statistics stats;
    stats.total_downloaded = 1000;
    stats.total_uploaded = 2000;

    // Simulate: previous sessions had 1000/2000, current session added 500/300
    stats.update(1000, 2000, 500, 300);

    CHECK(stats.total_downloaded == 1500);
    CHECK(stats.total_uploaded == 2300);
    CHECK(stats.session_downloaded == 500);
    CHECK(stats.session_uploaded == 300);
}

TEST_CASE("Statistics persist across simulated restarts", "[statistics]") {
    auto tmp = fs::temp_directory_path() / "levin_test_stats_restart";
    fs::create_directories(tmp);
    std::string path = (tmp / "stats.dat").string();
    fs::remove(path);

    // Session 1: download 1000, upload 2000
    {
        levin::Statistics stats;
        stats.load(path); // Should fail (no file), totals stay 0
        stats.update(0, 0, 1000, 2000);
        CHECK(stats.total_downloaded == 1000);
        CHECK(stats.total_uploaded == 2000);
        REQUIRE(stats.save(path));
    }

    // Session 2: load previous, add more
    {
        levin::Statistics stats;
        REQUIRE(stats.load(path));
        CHECK(stats.total_downloaded == 1000);
        CHECK(stats.total_uploaded == 2000);

        uint64_t base_dl = stats.total_downloaded;
        uint64_t base_ul = stats.total_uploaded;
        stats.update(base_dl, base_ul, 500, 300);
        CHECK(stats.total_downloaded == 1500);
        CHECK(stats.total_uploaded == 2300);
        REQUIRE(stats.save(path));
    }

    // Session 3: verify accumulated totals
    {
        levin::Statistics stats;
        REQUIRE(stats.load(path));
        CHECK(stats.total_downloaded == 1500);
        CHECK(stats.total_uploaded == 2300);
    }

    fs::remove_all(tmp);
}
