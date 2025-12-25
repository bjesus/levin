#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../src/disk_monitor.hpp"
#include "../src/config.hpp"
#include "../src/logger.hpp"
#include <fstream>
#include <filesystem>

using namespace levin;
using Catch::Approx;

// Initialize logger once for all tests
struct TestSetup {
    TestSetup() {
        Logger::init("/tmp/levin_test.log", "error");
    }
};
static TestSetup test_setup;

// Helper to create a minimal config for testing
Config create_test_config(uint64_t min_bytes, double min_percentage) {
    std::string temp_file = "/tmp/test_disk_config.toml";
    std::ostringstream oss;
    oss << R"(
[daemon]
pid_file = "/var/run/test.pid"
log_file = "/var/log/test.log"
log_level = "error"

[paths]
watch_directory = "/tmp/torrents"
data_directory = "/tmp"
session_state = "/tmp/session.state"
statistics_file = "/tmp/stats.json"

[disk]
min_free = )" << min_bytes << R"(
min_free_percentage = )" << min_percentage << R"(
check_interval_seconds = 60

[torrents]
seeder_update_interval_minutes = 15
watch_directory_scan_interval_seconds = 30
max_connections_per_torrent = 50
max_upload_slots_per_torrent = 20

[limits]
max_download_rate_kbps = 0
max_upload_rate_kbps = 0
max_total_connections = 200
max_active_downloads = 5
max_active_seeds = -1
max_active_torrents = 10

[network]
listen_port = 6881
enable_dht = true
enable_lsd = true
enable_upnp = true
enable_natpmp = true
enable_webrtc = true
webrtc_stun_server = "stun:stun.l.google.com:19302"

[cli]
control_socket = "/tmp/test.sock"

[statistics]
save_interval_minutes = 5
)";

    std::ofstream ofs(temp_file);
    ofs << oss.str();
    ofs.close();
    
    Config config = Config::load(temp_file);
    std::filesystem::remove(temp_file);
    return config;
}

TEST_CASE("DiskMonitor calculates space status correctly", "[disk_monitor]") {
    SECTION("Under budget scenario") {
        Config config = create_test_config(1024 * 1024 * 100, 0.01); // 100MB or 1%
        DiskMonitor monitor(config);
        
        // Set usage to 1GB
        monitor.set_current_usage(1024ULL * 1024 * 1024);
        
        auto status = monitor.check_space();
        
        // We should have plenty of budget (depends on actual disk size)
        REQUIRE(status.total_bytes > 0);
        REQUIRE(status.free_bytes > 0);
        REQUIRE(status.min_required_bytes > 0);
        
        // Check that budget is calculated correctly
        uint64_t expected_budget = (status.free_bytes > status.min_required_bytes) 
            ? (status.free_bytes - status.min_required_bytes) 
            : 0;
        REQUIRE(status.budget_bytes == expected_budget);
    }
    
    SECTION("Over budget detection") {
        Config config = create_test_config(1024 * 1024 * 100, 0.01);
        DiskMonitor monitor(config);
        
        auto status = monitor.check_space();
        
        // Set usage to more than budget
        uint64_t excessive_usage = status.budget_bytes + (200 * 1024 * 1024); // Budget + 200MB
        monitor.set_current_usage(excessive_usage);
        
        status = monitor.check_space();
        
        // Should be over budget
        REQUIRE(status.over_budget == true);
        REQUIRE(status.deficit_bytes >= (200 * 1024 * 1024));
    }
    
    SECTION("Emergency mode threshold") {
        Config config = create_test_config(1024 * 1024 * 100, 0.01);
        DiskMonitor monitor(config);
        
        auto status = monitor.check_space();
        
        // Set usage to 150MB over budget (should trigger emergency mode)
        uint64_t excessive_usage = status.budget_bytes + (150 * 1024 * 1024);
        monitor.set_current_usage(excessive_usage);
        
        status = monitor.check_space();
        
        REQUIRE(status.over_budget == true);
        REQUIRE(status.deficit_bytes > (100 * 1024 * 1024)); // > 100MB over
    }
}

TEST_CASE("DiskMonitor respects minimum free space", "[disk_monitor]") {
    SECTION("Absolute minimum") {
        uint64_t min_bytes = 5ULL * 1024 * 1024 * 1024; // 5GB
        Config config = create_test_config(min_bytes, 0.001); // Very low percentage
        DiskMonitor monitor(config);
        
        auto status = monitor.check_space();
        
        // Min required should be at least the absolute minimum
        REQUIRE(status.min_required_bytes >= min_bytes);
    }
    
    SECTION("Percentage minimum") {
        uint64_t small_absolute = 100 * 1024 * 1024; // 100MB
        double large_percentage = 0.20; // 20%
        Config config = create_test_config(small_absolute, large_percentage);
        DiskMonitor monitor(config);
        
        auto status = monitor.check_space();
        
        // Min required should be close to 20% of total
        uint64_t expected_min = uint64_t(status.total_bytes * large_percentage);
        REQUIRE(status.min_required_bytes == Approx(expected_min).margin(1024));
    }
}
