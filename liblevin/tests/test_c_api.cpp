#include <catch2/catch_test_macros.hpp>
#include "liblevin.h"

#include <cstring>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

constexpr uint64_t GB = 1024ULL * 1024 * 1024;

// Test fixture: creates temp directories, provides a config
class TestFixture {
public:
    levin_config_t config;

    TestFixture() {
        base_dir_ = fs::temp_directory_path() / ("levin_capi_test_" + std::to_string(counter_++));
        watch_dir_ = (base_dir_ / "torrents").string();
        data_dir_ = (base_dir_ / "data").string();
        state_dir_ = (base_dir_ / "state").string();

        fs::create_directories(base_dir_);

        std::memset(&config, 0, sizeof(config));
        config.watch_directory = watch_dir_.c_str();
        config.data_directory = data_dir_.c_str();
        config.state_directory = state_dir_.c_str();
        config.min_free_bytes = 1 * GB;
        config.min_free_percentage = 0.05;
        config.max_storage_bytes = 100 * GB;
        config.run_on_battery = 0;
        config.run_on_cellular = 0;
        config.disk_check_interval_secs = 60;
        config.max_download_kbps = 0;
        config.max_upload_kbps = 0;
        config.stun_server = "stun.l.google.com:19302";
    }

    ~TestFixture() {
        std::error_code ec;
        fs::remove_all(base_dir_, ec);
    }

private:
    fs::path base_dir_;
    std::string watch_dir_;
    std::string data_dir_;
    std::string state_dir_;
    static inline int counter_ = 0;
};

TEST_CASE("Create and destroy context") {
    TestFixture f;
    levin_t* ctx = levin_create(&f.config);
    REQUIRE(ctx != nullptr);
    levin_destroy(ctx);
}

TEST_CASE("Initial state is OFF", "[capi]") {
    TestFixture f;
    levin_t* ctx = levin_create(&f.config);
    REQUIRE(levin_get_status(ctx).state == LEVIN_STATE_OFF);
    levin_destroy(ctx);
}

TEST_CASE("Full conditions with no torrents: IDLE", "[capi]") {
    TestFixture f;
    levin_t* ctx = levin_create(&f.config);
    levin_start(ctx);
    levin_set_enabled(ctx, 1);
    levin_update_battery(ctx, 1);
    levin_update_network(ctx, 1, 0);
    levin_update_storage(ctx, 500*GB, 400*GB);
    levin_tick(ctx);
    REQUIRE(levin_get_status(ctx).state == LEVIN_STATE_IDLE);
    levin_stop(ctx);
    levin_destroy(ctx);
}

TEST_CASE("State callback fires on transition", "[capi]") {
    TestFixture f;
    levin_t* ctx = levin_create(&f.config);
    levin_start(ctx);

    levin_state_t last = LEVIN_STATE_OFF;
    levin_set_state_callback(ctx, [](levin_state_t, levin_state_t n, void* ud) {
        *(levin_state_t*)ud = n;
    }, &last);

    levin_set_enabled(ctx, 1);
    levin_update_battery(ctx, 1);
    levin_update_network(ctx, 1, 0);
    levin_update_storage(ctx, 500*GB, 400*GB);
    levin_tick(ctx);
    REQUIRE(last == LEVIN_STATE_IDLE);

    levin_stop(ctx);
    levin_destroy(ctx);
}

TEST_CASE("Battery loss transitions to PAUSED", "[capi]") {
    TestFixture f;
    f.config.run_on_battery = 0;
    levin_t* ctx = levin_create(&f.config);
    levin_start(ctx);
    levin_set_enabled(ctx, 1);
    levin_update_battery(ctx, 1);
    levin_update_network(ctx, 1, 0);
    levin_update_storage(ctx, 500*GB, 400*GB);
    levin_tick(ctx);
    REQUIRE(levin_get_status(ctx).state == LEVIN_STATE_IDLE);

    levin_update_battery(ctx, 0);
    levin_tick(ctx);
    REQUIRE(levin_get_status(ctx).state == LEVIN_STATE_PAUSED);

    levin_stop(ctx);
    levin_destroy(ctx);
}

TEST_CASE("run_on_battery=true ignores battery state", "[capi]") {
    TestFixture f;
    f.config.run_on_battery = 1;
    levin_t* ctx = levin_create(&f.config);
    levin_start(ctx);
    levin_set_enabled(ctx, 1);
    levin_update_battery(ctx, 0);
    levin_update_network(ctx, 1, 0);
    levin_update_storage(ctx, 500*GB, 400*GB);
    levin_tick(ctx);
    REQUIRE(levin_get_status(ctx).state == LEVIN_STATE_IDLE);  // not PAUSED

    levin_stop(ctx);
    levin_destroy(ctx);
}

TEST_CASE("Torrent list is initially empty", "[capi]") {
    TestFixture f;
    levin_t* ctx = levin_create(&f.config);
    levin_start(ctx);
    int count = 0;
    auto* list = levin_get_torrents(ctx, &count);
    REQUIRE(count == 0);
    levin_free_torrents(list, count);
    levin_stop(ctx);
    levin_destroy(ctx);
}

TEST_CASE("Status reports disk usage and budget", "[capi]") {
    TestFixture f;
    levin_t* ctx = levin_create(&f.config);
    levin_start(ctx);
    levin_set_enabled(ctx, 1);
    levin_update_battery(ctx, 1);
    levin_update_network(ctx, 1, 0);
    levin_update_storage(ctx, 500*GB, 400*GB);
    levin_tick(ctx);

    auto s = levin_get_status(ctx);
    REQUIRE(s.disk_budget > 0);
    REQUIRE(s.over_budget == 0);

    levin_stop(ctx);
    levin_destroy(ctx);
}
