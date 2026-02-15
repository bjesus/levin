#include "liblevin.h"
#include "state_machine.h"
#include "disk_manager.h"
#include "torrent_session.h"
#include "torrent_watcher.h"
#include "annas_archive.h"
#include "statistics.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <memory>
#include <string>
#include <filesystem>

#if defined(__linux__) || defined(__APPLE__)
#include <sys/stat.h>
#endif

#include "levin_log.h"

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
    std::unique_ptr<levin::TorrentWatcher> watcher;
    levin::Statistics stats;
    uint64_t stats_base_downloaded = 0; // Cumulative total before this session
    uint64_t stats_base_uploaded = 0;

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
    int file_count = 0;

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

// Calculate current disk usage and count non-empty files in data directory
struct DiskScan {
    uint64_t usage;
    int file_count;
};

static DiskScan calculate_disk_usage(const std::string& data_dir) {
    DiskScan result{0, 0};
    std::error_code ec;
    if (!fs::exists(data_dir, ec)) return result;
    for (auto& entry : fs::recursive_directory_iterator(data_dir, ec)) {
        if (entry.is_regular_file()) {
#if defined(__linux__) || defined(__APPLE__)
            struct stat st;
            if (::stat(entry.path().c_str(), &st) == 0) {
                uint64_t size = static_cast<uint64_t>(st.st_blocks) * 512;
                result.usage += size;
                if (st.st_size > 0) result.file_count++;
            }
#else
            auto sz = entry.file_size(ec);
            result.usage += sz;
            if (sz > 0) result.file_count++;
#endif
        }
    }
    return result;
}

static void do_disk_check(levin_t* ctx) {
    auto scan = calculate_disk_usage(ctx->data_directory);
    ctx->disk_usage = scan.usage;
    ctx->file_count = scan.file_count;
    auto result = ctx->disk_manager.calculate(ctx->fs_total, ctx->fs_free, ctx->disk_usage);
    ctx->disk_budget = result.budget_bytes;
    ctx->over_budget = result.over_budget ? 1 : 0;

    ctx->state_machine.update_storage(!result.over_budget);

    // Set per-file download priorities so we never download more than the budget allows.
    // Files that don't fit get priority 0 (don't download).
    if (ctx->session) {
        ctx->session->apply_budget_priorities(result.budget_bytes);
    }

    // Safety net: if somehow over budget (e.g. files added externally), delete to recover
    if (result.over_budget && result.deficit_bytes > 0) {
        uint64_t freed = ctx->disk_manager.delete_to_free(ctx->data_directory, result.deficit_bytes);
        // Update fs_free to reflect freed space so recalculation is accurate
        ctx->fs_free += freed;
        // Recalculate after deletion
        auto scan2 = calculate_disk_usage(ctx->data_directory);
        ctx->disk_usage = scan2.usage;
        ctx->file_count = scan2.file_count;
        auto r2 = ctx->disk_manager.calculate(ctx->fs_total, ctx->fs_free, ctx->disk_usage);
        ctx->disk_budget = r2.budget_bytes;
        ctx->over_budget = r2.over_budget ? 1 : 0;
        ctx->state_machine.update_storage(!r2.over_budget);

        // Re-apply priorities with the updated budget
        if (ctx->session) {
            ctx->session->apply_budget_priorities(r2.budget_bytes);
        }
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

#ifdef LEVIN_USE_STUB_SESSION
    ctx->session = std::make_unique<levin::StubTorrentSession>();
#else
    ctx->session = levin::create_real_torrent_session();
#endif

    // Create torrent watcher
    ctx->watcher = std::make_unique<levin::TorrentWatcher>();

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

    // Load persistent statistics
    ctx->stats.load(ctx->state_directory + "/stats.dat");
    ctx->stats_base_downloaded = ctx->stats.total_downloaded;
    ctx->stats_base_uploaded = ctx->stats.total_uploaded;

    // Start session (with state restoration)
    ctx->session->configure(6881, ctx->stun_server);
    ctx->session->load_state(ctx->state_directory + "/session.state");
    ctx->session->start(ctx->data_directory);

    // Configure and start torrent watcher
    ctx->watcher->set_callbacks(
        [ctx](const std::string& path) {
            levin_add_torrent(ctx, path.c_str());
        },
        [ctx](const std::string& path) {
            // Extract filename to use as info_hash key for removal
            // (the remove_torrent API expects an info_hash, but the watcher
            // only knows the file path; for now we pass the path and the
            // session can look it up)
            auto pos = path.find_last_of('/');
            std::string name = (pos != std::string::npos) ? path.substr(pos + 1) : path;
            (void)name;
            // Note: remove_torrent requires an info_hash which we don't have
            // from just the file path. This is a best-effort notification.
        }
    );
    ctx->started = true;

    if (!ctx->watch_directory.empty()) {
        LEVIN_LOG("starting watcher on: %s", ctx->watch_directory.c_str());
        ctx->watcher->start(ctx->watch_directory);
        ctx->watcher->scan_existing();
        LEVIN_LOG("scan_existing complete, torrent_count=%d", ctx->session->torrent_count());
    }
    return 0;
}

void levin_stop(levin_t* ctx) {
    if (!ctx || !ctx->started) return;
    ctx->watcher->stop();

    // Update and save statistics before stopping
    ctx->stats.update(ctx->stats_base_downloaded, ctx->stats_base_uploaded,
                      ctx->session->total_downloaded(), ctx->session->total_uploaded());
    ctx->stats.save(ctx->state_directory + "/stats.dat");

    ctx->session->save_state(ctx->state_directory + "/session.state");
    ctx->session->stop();
    ctx->started = false;
}

void levin_tick(levin_t* ctx) {
    if (!ctx || !ctx->started) return;

    ctx->tick_count++;

    // Poll watcher for new/removed torrent files
    ctx->watcher->poll();

    // Update has_torrents based on session
    ctx->state_machine.update_has_torrents(ctx->session->torrent_count() > 0);

    // Periodic disk check
    if (ctx->tick_count % ctx->disk_check_interval_secs == 0 || ctx->tick_count == 1) {
        if (ctx->fs_total > 0) {
            do_disk_check(ctx);
        }
    }

    // Periodic stats save (every 5 minutes)
    static const int STATS_SAVE_INTERVAL = 300;
    if (ctx->tick_count % STATS_SAVE_INTERVAL == 0) {
        ctx->stats.update(ctx->stats_base_downloaded, ctx->stats_base_uploaded,
                          ctx->session->total_downloaded(), ctx->session->total_uploaded());
        ctx->stats.save(ctx->state_directory + "/stats.dat");
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
        LEVIN_LOG("torrent added: %s (count=%d)", torrent_path, ctx->session->torrent_count());
        return 0;
    }
    LEVIN_LOG("torrent add failed: %s", torrent_path);
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
    // Cumulative totals: base (from previous sessions) + current session
    status.total_downloaded = ctx->stats_base_downloaded +
        (ctx->session ? ctx->session->total_downloaded() : 0);
    status.total_uploaded = ctx->stats_base_uploaded +
        (ctx->session ? ctx->session->total_uploaded() : 0);
    status.disk_usage = ctx->disk_usage;
    status.disk_budget = ctx->disk_budget;
    status.over_budget = ctx->over_budget;
    status.file_count = ctx->file_count;

    return status;
}

levin_torrent_t* levin_get_torrents(levin_t* ctx, int* count) {
    if (!ctx || !count) {
        if (count) *count = 0;
        return nullptr;
    }

    auto torrents = ctx->session->get_torrent_list();
    int n = static_cast<int>(torrents.size());
    if (n == 0) {
        *count = 0;
        return nullptr;
    }

    auto* list = new levin_torrent_t[n];
    for (int i = 0; i < n; i++) {
        const auto& t = torrents[i];
        // Copy info_hash (truncate/pad to 40 chars)
        std::memset(list[i].info_hash, 0, sizeof(list[i].info_hash));
        std::strncpy(list[i].info_hash, t.info_hash.c_str(), 40);
        list[i].name = strdup(t.name.c_str());
        list[i].size = t.size;
        list[i].downloaded = t.downloaded;
        list[i].uploaded = t.uploaded;
        list[i].download_rate = t.download_rate;
        list[i].upload_rate = t.upload_rate;
        list[i].num_peers = t.num_peers;
        list[i].progress = t.progress;
        list[i].is_seed = t.is_seed ? 1 : 0;
    }

    *count = n;
    return list;
}

void levin_free_torrents(levin_torrent_t* list, int count) {
    if (list) {
        for (int i = 0; i < count; i++) {
            free(const_cast<char*>(list[i].name));
        }
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

void levin_set_run_on_battery(levin_t* ctx, int run_on_battery) {
    if (!ctx) return;
    ctx->run_on_battery = run_on_battery;
    // Re-evaluate battery condition with current power state
    bool battery_ok = ctx->on_ac_power || ctx->run_on_battery;
    ctx->state_machine.update_battery(battery_ok);
}

void levin_set_disk_limits(levin_t* ctx, uint64_t min_free_bytes, double min_free_pct, uint64_t max_storage_bytes) {
    if (!ctx) return;
    ctx->min_free_bytes = min_free_bytes;
    ctx->min_free_percentage = min_free_pct;
    ctx->max_storage_bytes = max_storage_bytes;
    ctx->disk_manager = levin::DiskManager(min_free_bytes, min_free_pct, max_storage_bytes);
    // Trigger immediate re-evaluation
    if (ctx->started && ctx->fs_total > 0) {
        do_disk_check(ctx);
    }
}

void levin_set_run_on_cellular(levin_t* ctx, int run_on_cellular) {
    if (!ctx) return;
    ctx->run_on_cellular = run_on_cellular;
    // Re-evaluate network condition with current state
    bool network_ok = ctx->has_wifi || (ctx->has_cellular && ctx->run_on_cellular);
    ctx->state_machine.update_network(network_ok);
}

int levin_populate_torrents(levin_t* ctx, levin_progress_cb cb, void* userdata) {
    if (!ctx) return -1;

    levin::ProgressCallback progress;
    if (cb) {
        progress = [cb, userdata](int current, int total, const std::string& message) {
            cb(current, total, message.c_str(), userdata);
        };
    }

    return levin::AnnaArchive::populate_torrents(ctx->watch_directory, progress);
}

void levin_set_state_callback(levin_t* ctx, levin_state_cb cb, void* userdata) {
    if (!ctx) return;
    ctx->state_cb = cb;
    ctx->state_cb_userdata = userdata;
}
