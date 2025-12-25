#include <catch2/catch_test_macros.hpp>
#include "../src/config.hpp"
#include <fstream>
#include <filesystem>

using namespace levin;

TEST_CASE("Config loads valid TOML file", "[config]") {
    // Create a temporary config file
    const char* test_config = R"(
[daemon]
pid_file = "/var/run/test.pid"
log_file = "/var/log/test.log"
log_level = "debug"

[paths]
watch_directory = "/tmp/torrents"
data_directory = "/tmp/data"
session_state = "/tmp/session.state"
statistics_file = "/tmp/stats.json"

[disk]
min_free = 1073741824
min_free_percentage = 0.10
check_interval_seconds = 60

[torrents]
seeder_update_interval_minutes = 15
watch_directory_scan_interval_seconds = 30
max_connections_per_torrent = 50
max_upload_slots_per_torrent = 20

[limits]
max_download_rate_kbps = 1000
max_upload_rate_kbps = 500
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

    const char* temp_file = "/tmp/test_config.toml";
    std::ofstream ofs(temp_file);
    ofs << test_config;
    ofs.close();
    
    Config config = Config::load(temp_file);
    
    SECTION("Daemon configuration") {
        REQUIRE(config.daemon.pid_file == "/var/run/test.pid");
        REQUIRE(config.daemon.log_file == "/var/log/test.log");
        REQUIRE(config.daemon.log_level == "debug");
    }
    
    SECTION("Paths configuration") {
        REQUIRE(config.paths.watch_directory == "/tmp/torrents");
        REQUIRE(config.paths.data_directory == "/tmp/data");
        REQUIRE(config.paths.session_state == "/tmp/session.state");
        REQUIRE(config.paths.statistics_file == "/tmp/stats.json");
    }
    
    SECTION("Disk configuration") {
        REQUIRE(config.disk.min_free == 1073741824);
        REQUIRE(config.disk.min_free_percentage == 0.10);
        REQUIRE(config.disk.check_interval_seconds == 60);
    }
    
    SECTION("Network configuration") {
        REQUIRE(config.network.listen_port == 6881);
        REQUIRE(config.network.enable_dht == true);
        REQUIRE(config.network.enable_webrtc == true);
    }
    
    SECTION("Limits configuration") {
        REQUIRE(config.limits.max_download_rate_kbps == 1000);
        REQUIRE(config.limits.max_upload_rate_kbps == 500);
        REQUIRE(config.limits.max_active_downloads == 5);
    }
    
    // Cleanup
    std::filesystem::remove(temp_file);
}

TEST_CASE("Config calculates effective min free space correctly", "[config]") {
    const char* temp_file = "/tmp/test_config_calc.toml";
    const char* test_config = R"(
[daemon]
pid_file = "/var/run/test.pid"
log_file = "/var/log/test.log"
log_level = "info"

[paths]
watch_directory = "/tmp/torrents"
data_directory = "/tmp/data"
session_state = "/tmp/session.state"
statistics_file = "/tmp/stats.json"

[disk]
min_free = 1073741824
min_free_percentage = 0.10
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
    ofs << test_config;
    ofs.close();
    
    Config config = Config::load(temp_file);
    
    SECTION("Absolute minimum is larger") {
        // Total disk: 1GB, 10% = 100MB, absolute = 1GB
        // Should return 1GB
        uint64_t total = 1024ULL * 1024 * 1024;
        REQUIRE(config.get_effective_min_free_space(total) == 1073741824);
    }
    
    SECTION("Percentage minimum is larger") {
        // Total disk: 100GB, 10% = 10GB, absolute = 1GB
        // Should return 10GB
        uint64_t total = 100ULL * 1024 * 1024 * 1024;
        uint64_t expected = uint64_t(total * 0.10);
        REQUIRE(config.get_effective_min_free_space(total) == expected);
    }
    
    SECTION("Equal values") {
        // Total disk: 10GB, 10% = 1GB, absolute = 1GB
        // Should return 1GB
        uint64_t total = 10ULL * 1024 * 1024 * 1024;
        REQUIRE(config.get_effective_min_free_space(total) == 1073741824);
    }
    
    // Cleanup
    std::filesystem::remove(temp_file);
}
