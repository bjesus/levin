#include "liblevin.h"
#include "state_machine.h"
#include "disk_manager.h"
#include "torrent_session.h"

#include <cstring>
#include <memory>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

// Internal context structure
struct levin_ctx {
    // Config (owned copies)
    std::string watch_directory;
    std::string data_directory;
    std::string state_directory;
    std::string stun_server;
    uint64_t min_free_bytes;
    double min_free_percentage;
    uint64_t max_storage_bytes;
    int run_on_battery;
    int run_on_cellular;
    int disk_check_interval_secs;
    int max_download_kbps;
    int max_upload_kbps;

    // Core components
    levin::StateMachine state_machine;
    levin::DiskManager disk_manager;
    std::unique_ptr<levin::ITorrentSession> session;

    // State tracking
    bool started = false;
    bool enabled = false;
    int on_ac_power = 0;
    int has_wifi = 0;
    int has_cellular = 0;
    uint64_t fs_total = 0;
    uint64_t fs_free = 0;

    // Callback
    levin_state_cb state_cb = nullptr;
    void* state_cb_userdata = nullptr;

    // Cached status
    uint64_t disk_usage = 0;
    uint64_t disk_budget = 0;
    int over_budget = 0;

    // Tick counter for periodic disk checks
    int tick_count = 0;
};

// Map internal state to C API state
static levin_state_t to_c_state(levin::State s) {
    switch (s) {
        case levin::State::OFF:         return LEVIN_STATE_OFF;
        case levin::State::PAUSED:      return LEVIN_STATE_PAUSED;
        case levin::State::IDLE:        return LEVIN_STATE_IDLE;
        case levin::State::SEEDING:     return LEVIN_STATE_SEEDING;
        case levin::State::DOWNLOADING: return LEVIN_STATE_DOWNLOADING;
    }
    return LEVIN_STATE_OFF;
}

// Apply session actions based on new state
static void apply_state_actions(levin_t* ctx, levin::State new_state) {
    if (!ctx->session || !ctx->session->is_running()) return;

    switch (new_state) {
        case levin::State::OFF:
        case levin::State::PAUSED:
            ctx->session->pause_session();
            break;
        case levin::State::IDLE:
            ctx->session->resume_session();
            break;
        case levin::State::SEEDING:
            ctx->session->resume_session();
            ctx->session->pause_downloads();
            break;
        case levin::State::DOWNLOADING:
            ctx->session->resume_session();
            if (ctx->max_download_kbps > 0) {
                ctx->session->set_download_rate_limit(ctx->max_download_kbps * 1024);
            } else {
                ctx->session->resume_downloads();
            }
            break;
    }
}

// Calculate current disk usage of data directory
static uint64_t calculate_disk_usage(const std::string& data_dir) {
    uint64_t total = 0;
    std::error_code ec;
    if (!fs::exists(data_dir, ec)) return 0;
    for (auto& entry : fs::recursive_directory_iterator(data_dir, ec)) {
        if (entry.is_regular_file()) {
            total += entry.file_size(ec);
        }
    }
    return total;
}

static void do_disk_check(levin_t* ctx) {
    ctx->disk_usage = calculate_disk_usage(ctx->data_directory);
    auto result = ctx->disk_manager.calculate(ctx->fs_total, ctx->fs_free, ctx->disk_usage);
    ctx->disk_budget = result.budget_bytes;
    ctx->over_budget = result.over_budget ? 1 : 0;

    ctx->state_machine.update_storage(!result.over_budget);

    // If over budget, delete files to free space
    if (result.over_budget && result.deficit_bytes > 0) {
        ctx->disk_manager.delete_to_free(ctx->data_directory, result.deficit_bytes);
        // Recalculate after deletion
        ctx->disk_usage = calculate_disk_usage(ctx->data_directory);
        auto r2 = ctx->disk_manager.calculate(ctx->fs_total, ctx->fs_free, ctx->disk_usage);
        ctx->disk_budget = r2.budget_bytes;
        ctx->over_budget = r2.over_budget ? 1 : 0;
        ctx->state_machine.update_storage(!r2.over_budget);
    }
}

// --- C API Implementation ---

levin_t* levin_create(const levin_config_t* config) {
    if (!config) return nullptr;

    auto* ctx = new levin_ctx();

    // Copy strings
    ctx->watch_directory = config->watch_directory ? config->watch_directory : "";
    ctx->data_directory = config->data_directory ? config->data_directory : "";
    ctx->state_directory = config->state_directory ? config->state_directory : "";
    ctx->stun_server = config->stun_server ? config->stun_server : "stun.l.google.com:19302";

    // Copy numeric config
    ctx->min_free_bytes = config->min_free_bytes;
    ctx->min_free_percentage = config->min_free_percentage;
    ctx->max_storage_bytes = config->max_storage_bytes;
    ctx->run_on_battery = config->run_on_battery;
    ctx->run_on_cellular = config->run_on_cellular;
    ctx->disk_check_interval_secs = config->disk_check_interval_secs > 0 ? config->disk_check_interval_secs : 60;
    ctx->max_download_kbps = config->max_download_kbps;
    ctx->max_upload_kbps = config->max_upload_kbps;

    // Initialize disk manager
    ctx->disk_manager = levin::DiskManager(ctx->min_free_bytes, ctx->min_free_percentage, ctx->max_storage_bytes);

    // Create stub session (will be swapped for real session in Phase 5)
    ctx->session = std::make_unique<levin::StubTorrentSession>();

    // Wire up state machine callback
    ctx->state_machine.set_callback([ctx](levin::State old_s, levin::State new_s) {
        apply_state_actions(ctx, new_s);
        if (ctx->state_cb) {
            ctx->state_cb(to_c_state(old_s), to_c_state(new_s), ctx->state_cb_userdata);
        }
    });

    return ctx;
}

void levin_destroy(levin_t* ctx) {
    if (!ctx) return;
    if (ctx->started) {
        levin_stop(ctx);
    }
    delete ctx;
}

int levin_start(levin_t* ctx) {
    if (!ctx || ctx->started) return -1;

    // Create directories
    std::error_code ec;
    fs::create_directories(ctx->watch_directory, ec);
    fs::create_directories(ctx->data_directory, ec);
    fs::create_directories(ctx->state_directory, ec);

    // Start session
    ctx->session->configure(6881, ctx->stun_server);
    ctx->session->start(ctx->data_directory);

    ctx->started = true;
    return 0;
}

void levin_stop(levin_t* ctx) {
    if (!ctx || !ctx->started) return;
    ctx->session->stop();
    ctx->started = false;
}

void levin_tick(levin_t* ctx) {
    if (!ctx || !ctx->started) return;

    ctx->tick_count++;

    // Update has_torrents based on session
    ctx->state_machine.update_has_torrents(ctx->session->torrent_count() > 0);

    // Periodic disk check
    if (ctx->tick_count % ctx->disk_check_interval_secs == 0 || ctx->tick_count == 1) {
        if (ctx->fs_total > 0) {
            do_disk_check(ctx);
        }
    }
}

void levin_update_battery(levin_t* ctx, int on_ac_power) {
    if (!ctx) return;
    ctx->on_ac_power = on_ac_power;
    bool battery_ok = on_ac_power || ctx->run_on_battery;
    ctx->state_machine.update_battery(battery_ok);
}

void levin_update_network(levin_t* ctx, int has_wifi, int has_cellular) {
    if (!ctx) return;
    ctx->has_wifi = has_wifi;
    ctx->has_cellular = has_cellular;
    bool network_ok = has_wifi || (has_cellular && ctx->run_on_cellular);
    ctx->state_machine.update_network(network_ok);
}

void levin_update_storage(levin_t* ctx, uint64_t fs_total, uint64_t fs_free) {
    if (!ctx) return;
    ctx->fs_total = fs_total;
    ctx->fs_free = fs_free;
    // Disk check will happen on next tick or we can do it immediately
    if (ctx->started) {
        do_disk_check(ctx);
    }
}

int levin_add_torrent(levin_t* ctx, const char* torrent_path) {
    if (!ctx || !ctx->started || !torrent_path) return -1;
    auto result = ctx->session->add_torrent(torrent_path);
    if (result) {
        ctx->state_machine.update_has_torrents(ctx->session->torrent_count() > 0);
        return 0;
    }
    return -1;
}

void levin_remove_torrent(levin_t* ctx, const char* info_hash) {
    if (!ctx || !ctx->started || !info_hash) return;
    ctx->session->remove_torrent(info_hash);
    ctx->state_machine.update_has_torrents(ctx->session->torrent_count() > 0);
}

levin_status_t levin_get_status(levin_t* ctx) {
    levin_status_t status = {};
    if (!ctx) return status;

    status.state = to_c_state(ctx->state_machine.state());
    status.torrent_count = ctx->session ? ctx->session->torrent_count() : 0;
    status.peer_count = ctx->session ? ctx->session->peer_count() : 0;
    status.download_rate = ctx->session ? ctx->session->download_rate() : 0;
    status.upload_rate = ctx->session ? ctx->session->upload_rate() : 0;
    status.total_downloaded = ctx->session ? ctx->session->total_downloaded() : 0;
    status.total_uploaded = ctx->session ? ctx->session->total_uploaded() : 0;
    status.disk_usage = ctx->disk_usage;
    status.disk_budget = ctx->disk_budget;
    status.over_budget = ctx->over_budget;

    return status;
}

levin_torrent_t* levin_get_torrents(levin_t* ctx, int* count) {
    if (!ctx || !count) {
        if (count) *count = 0;
        return nullptr;
    }
    // Stub: no torrents to report details on yet
    *count = 0;
    return nullptr;
}

void levin_free_torrents(levin_torrent_t* list) {
    if (list) {
        delete[] list;
    }
}

void levin_set_enabled(levin_t* ctx, int enabled) {
    if (!ctx) return;
    ctx->enabled = (enabled != 0);
    ctx->state_machine.update_enabled(ctx->enabled);
}

void levin_set_download_limit(levin_t* ctx, int kbps) {
    if (!ctx) return;
    ctx->max_download_kbps = kbps;
    if (ctx->session && ctx->session->is_running()) {
        if (kbps > 0) {
            ctx->session->set_download_rate_limit(kbps * 1024);
        } else {
            ctx->session->set_download_rate_limit(0);
        }
    }
}

void levin_set_upload_limit(levin_t* ctx, int kbps) {
    if (!ctx) return;
    ctx->max_upload_kbps = kbps;
    if (ctx->session && ctx->session->is_running()) {
        if (kbps > 0) {
            ctx->session->set_upload_rate_limit(kbps * 1024);
        } else {
            ctx->session->set_upload_rate_limit(0);
        }
    }
}

int levin_populate_torrents(levin_t* /*ctx*/, levin_progress_cb /*cb*/, void* /*userdata*/) {
    // TODO: Phase 5d - Anna's Archive integration
    return -1;
}

void levin_set_state_callback(levin_t* ctx, levin_state_cb cb, void* userdata) {
    if (!ctx) return;
    ctx->state_cb = cb;
    ctx->state_cb_userdata = userdata;
}
