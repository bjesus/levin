#include <catch2/catch_test_macros.hpp>
#include "../src/config.hpp"
#include <fstream>
#include <filesystem>

using namespace levin;

TEST_CASE("Config loads valid TOML file with new simplified format", "[config]") {
    // Create a temporary config file using the new simplified format
    const char* test_config = R"(
[paths]
watch_directory = "/tmp/torrents"
data_directory = "/tmp/data"
state_directory = "/tmp/levin-state"

[disk]
min_free = "1gb"
min_free_percentage = 0.10
max_storage = "50gb"

[daemon]
log_level = "debug"
run_on_battery = true

[limits]
max_download_rate_kbps = 1000
max_upload_rate_kbps = 500
)";

    const char* temp_file = "/tmp/test_config.toml";
    std::ofstream ofs(temp_file);
    ofs << test_config;
    ofs.close();
    
    Config config = Config::load(temp_file);
    
    SECTION("Paths configuration") {
        REQUIRE(config.paths.watch_directory == "/tmp/torrents");
        REQUIRE(config.paths.data_directory == "/tmp/data");
        REQUIRE(config.paths.state_directory == "/tmp/levin-state");
    }
    
    SECTION("Derived paths") {
        REQUIRE(config.pid_file() == "/tmp/levin-state/levin.pid");
        REQUIRE(config.log_file() == "/tmp/levin-state/levin.log");
        REQUIRE(config.control_socket() == "/tmp/levin-state/levin.sock");
        REQUIRE(config.session_state() == "/tmp/levin-state/session.state");
        REQUIRE(config.statistics_file() == "/tmp/levin-state/statistics.json");
    }
    
    SECTION("Daemon configuration") {
        REQUIRE(config.daemon.log_level == "debug");
        REQUIRE(config.daemon.run_on_battery == true);
    }
    
    SECTION("Disk configuration") {
        REQUIRE(config.disk.min_free == 1024ULL * 1024 * 1024);  // 1GB
        REQUIRE(config.disk.min_free_percentage == 0.10);
        REQUIRE(config.disk.max_storage == 50ULL * 1024 * 1024 * 1024);  // 50GB
    }
    
    SECTION("Limits configuration") {
        REQUIRE(config.limits.max_download_rate_kbps == 1000);
        REQUIRE(config.limits.max_upload_rate_kbps == 500);
    }
    
    SECTION("Hardcoded constants") {
        // These should be constants, not loaded from config
        REQUIRE(Config::listen_port == 6881);
        REQUIRE(Config::enable_dht == true);
        REQUIRE(Config::enable_lsd == true);
        REQUIRE(Config::enable_upnp == true);
        REQUIRE(Config::enable_natpmp == true);
        REQUIRE(Config::max_active_downloads == -1);
        REQUIRE(Config::max_active_seeds == -1);
        REQUIRE(Config::max_active_torrents == -1);
        REQUIRE(Config::max_total_connections == 200);
        REQUIRE(Config::disk_check_interval_seconds == 60);
        REQUIRE(Config::statistics_save_interval_minutes == 5);
    }
    
    // Cleanup
    std::filesystem::remove(temp_file);
}

TEST_CASE("Config uses default state directory when not specified", "[config]") {
    const char* test_config = R"(
[paths]
watch_directory = "/tmp/torrents"
data_directory = "/tmp/data"

[disk]
min_free = "1gb"
)";

    const char* temp_file = "/tmp/test_config_defaults.toml";
    std::ofstream ofs(temp_file);
    ofs << test_config;
    ofs.close();
    
    Config config = Config::load(temp_file);
    
    SECTION("State directory defaults to XDG_STATE_HOME/levin") {
        // Should use default state directory
        std::string expected = std::string(std::getenv("HOME")) + "/.local/state/levin";
        REQUIRE(config.paths.state_directory == expected);
    }
    
    SECTION("Paths are loaded correctly") {
        REQUIRE(config.paths.watch_directory == "/tmp/torrents");
        REQUIRE(config.paths.data_directory == "/tmp/data");
    }
    
    // Cleanup
    std::filesystem::remove(temp_file);
}

TEST_CASE("Config calculates effective min free space correctly", "[config]") {
    const char* temp_file = "/tmp/test_config_calc.toml";
    const char* test_config = R"(
[paths]
watch_directory = "/tmp/torrents"
data_directory = "/tmp/data"

[disk]
min_free = "1gb"
min_free_percentage = 0.10
)";

    std::ofstream ofs(temp_file);
    ofs << test_config;
    ofs.close();
    
    Config config = Config::load(temp_file);
    
    SECTION("Absolute minimum is larger") {
        // Total disk: 1GB, 10% = 100MB, absolute = 1GB
        // Should return 1GB
        uint64_t total = 1024ULL * 1024 * 1024;
        REQUIRE(config.get_effective_min_free_space(total) == 1024ULL * 1024 * 1024);
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
        REQUIRE(config.get_effective_min_free_space(total) == 1024ULL * 1024 * 1024);
    }
    
    // Cleanup
    std::filesystem::remove(temp_file);
}

TEST_CASE("Config parses human-readable sizes", "[config]") {
    const char* temp_file = "/tmp/test_config_sizes.toml";
    const char* test_config = R"(
[paths]
watch_directory = "/tmp/torrents"
data_directory = "/tmp/data"

[disk]
min_free = "500mb"
max_storage = "2tb"
)";

    std::ofstream ofs(temp_file);
    ofs << test_config;
    ofs.close();
    
    Config config = Config::load(temp_file);
    
    REQUIRE(config.disk.min_free == 500ULL * 1024 * 1024);  // 500MB
    REQUIRE(config.disk.max_storage == 2ULL * 1024 * 1024 * 1024 * 1024);  // 2TB
    
    // Cleanup
    std::filesystem::remove(temp_file);
}

TEST_CASE("Config validation", "[config]") {
    SECTION("Valid config passes validation") {
        const char* temp_file = "/tmp/test_config_valid.toml";
        const char* test_config = R"(
[paths]
watch_directory = "/tmp/torrents"
data_directory = "/tmp/data"

[disk]
min_free = "1gb"
)";
        std::ofstream ofs(temp_file);
        ofs << test_config;
        ofs.close();
        
        Config config = Config::load(temp_file);
        REQUIRE(config.validate() == true);
        
        std::filesystem::remove(temp_file);
    }
    
    SECTION("Invalid log level fails validation") {
        const char* temp_file = "/tmp/test_config_invalid.toml";
        const char* test_config = R"(
[paths]
watch_directory = "/tmp/torrents"
data_directory = "/tmp/data"

[disk]
min_free = "1gb"

[daemon]
log_level = "invalid_level"
)";
        std::ofstream ofs(temp_file);
        ofs << test_config;
        ofs.close();
        
        Config config = Config::load(temp_file);
        REQUIRE(config.validate() == false);
        
        std::filesystem::remove(temp_file);
    }
}
