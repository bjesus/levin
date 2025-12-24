#include <catch2/catch_test_macros.hpp>
#include "../src/utils.hpp"

using namespace levin;

TEST_CASE("format_bytes formats byte sizes correctly", "[utils]") {
    SECTION("Bytes") {
        REQUIRE(utils::format_bytes(0) == "0.00 B");
        REQUIRE(utils::format_bytes(1) == "1.00 B");
        REQUIRE(utils::format_bytes(1023) == "1023.00 B");
    }
    
    SECTION("Kilobytes") {
        REQUIRE(utils::format_bytes(1024) == "1.00 KB");
        REQUIRE(utils::format_bytes(1536) == "1.50 KB");
        REQUIRE(utils::format_bytes(10240) == "10.00 KB");
    }
    
    SECTION("Megabytes") {
        REQUIRE(utils::format_bytes(1024 * 1024) == "1.00 MB");
        REQUIRE(utils::format_bytes(1024 * 1024 * 10) == "10.00 MB");
        REQUIRE(utils::format_bytes(1024 * 1024 * 100) == "100.00 MB");
    }
    
    SECTION("Gigabytes") {
        REQUIRE(utils::format_bytes(1024ULL * 1024 * 1024) == "1.00 GB");
        REQUIRE(utils::format_bytes(1024ULL * 1024 * 1024 * 5) == "5.00 GB");
        REQUIRE(utils::format_bytes(1024ULL * 1024 * 1024 * 100) == "100.00 GB");
    }
    
    SECTION("Terabytes") {
        REQUIRE(utils::format_bytes(1024ULL * 1024 * 1024 * 1024) == "1.00 TB");
        REQUIRE(utils::format_bytes(1024ULL * 1024 * 1024 * 1024 * 2) == "2.00 TB");
    }
}

TEST_CASE("format_duration formats time durations correctly", "[utils]") {
    SECTION("Seconds") {
        REQUIRE(utils::format_duration(0) == "0s");
        REQUIRE(utils::format_duration(1) == "1s");
        REQUIRE(utils::format_duration(59) == "59s");
    }
    
    SECTION("Minutes") {
        REQUIRE(utils::format_duration(60) == "1m 0s");
        REQUIRE(utils::format_duration(90) == "1m 30s");
        REQUIRE(utils::format_duration(3599) == "59m 59s");
    }
    
    SECTION("Hours") {
        REQUIRE(utils::format_duration(3600) == "1h 0m");
        REQUIRE(utils::format_duration(7200) == "2h 0m");
        REQUIRE(utils::format_duration(86399) == "23h 59m");
    }
    
    SECTION("Days") {
        REQUIRE(utils::format_duration(86400) == "1d 0h 0m");
        REQUIRE(utils::format_duration(172800) == "2d 0h 0m");
        REQUIRE(utils::format_duration(604800) == "7d 0h 0m");
    }
}
