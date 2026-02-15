#include "liblevin.h"
#include "annas_archive.h"
#include "config.h"
#include "daemon.h"
#include "ipc.h"
#include "storage.h"
#include "power.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cinttypes>
#include <string>
#include <unistd.h>
#include <csignal>
#include <sys/stat.h>

// ---------------------------------------------------------------------------
// Paths
// ---------------------------------------------------------------------------

static std::string default_runtime_dir() {
    const char* xdg = std::getenv("XDG_RUNTIME_DIR");
    if (xdg && xdg[0]) return std::string(xdg) + "/levin";
    return "/tmp/levin-" + std::to_string(::getuid());
}

static std::string default_state_dir() {
    const char* xdg = std::getenv("XDG_STATE_HOME");
    if (xdg && xdg[0]) return std::string(xdg) + "/levin";
    const char* home = std::getenv("HOME");
    if (home && home[0]) return std::string(home) + "/.local/state/levin";
    return "/var/lib/levin";
}

static std::string socket_path() {
    return default_runtime_dir() + "/levin.sock";
}

static std::string pid_path() {
    return default_runtime_dir() + "/levin.pid";
}

// ---------------------------------------------------------------------------
// Ensure runtime directory exists
// ---------------------------------------------------------------------------

static void ensure_dir(const std::string& path) {
    // Simple mkdir -p for a single directory (no recursive creation needed for
    // typical XDG_RUNTIME_DIR/levin)
    ::mkdir(path.c_str(), 0700);
}

// ---------------------------------------------------------------------------
// Utility: format bytes as human-readable
// ---------------------------------------------------------------------------

static std::string format_bytes(uint64_t bytes) {
    char buf[64];
    if (bytes >= 1024ULL * 1024 * 1024) {
        std::snprintf(buf, sizeof(buf), "%.1f GB",
                      static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0));
    } else if (bytes >= 1024ULL * 1024) {
        std::snprintf(buf, sizeof(buf), "%.1f MB",
                      static_cast<double>(bytes) / (1024.0 * 1024.0));
    } else if (bytes >= 1024) {
        std::snprintf(buf, sizeof(buf), "%.1f KB",
                      static_cast<double>(bytes) / 1024.0);
    } else {
        std::snprintf(buf, sizeof(buf), "%" PRIu64 " B", bytes);
    }
    return buf;
}

static std::string format_rate(int bytes_per_sec) {
    return format_bytes(static_cast<uint64_t>(bytes_per_sec)) + "/s";
}

static std::string format_number(const std::string& s) {
    // Insert commas as thousands separators: "13194" -> "13,194"
    std::string result;
    int n = static_cast<int>(s.size());
    for (int i = 0; i < n; ++i) {
        if (i > 0 && (n - i) % 3 == 0) result += ',';
        result += s[i];
    }
    return result;
}

// ---------------------------------------------------------------------------
// State name helper
// ---------------------------------------------------------------------------

static const char* state_name(levin_state_t s) {
    switch (s) {
        case LEVIN_STATE_OFF:         return "off";
        case LEVIN_STATE_PAUSED:      return "paused";
        case LEVIN_STATE_IDLE:        return "idle";
        case LEVIN_STATE_SEEDING:     return "seeding";
        case LEVIN_STATE_DOWNLOADING: return "downloading";
    }
    return "unknown";
}

// ---------------------------------------------------------------------------
// IPC message handler (runs inside daemon)
// ---------------------------------------------------------------------------

static levin::linux_shell::Message handle_ipc(
    levin_t* ctx,
    const levin::linux_shell::Message& req)
{
    using levin::linux_shell::Message;

    auto it = req.find("command");
    if (it == req.end()) {
        return {{"error", "missing command"}};
    }
    const std::string& cmd = it->second;

    if (cmd == "status") {
        levin_status_t st = levin_get_status(ctx);
        Message reply;
        reply["state"]            = state_name(st.state);
        reply["torrent_count"]    = std::to_string(st.torrent_count);
        reply["peer_count"]       = std::to_string(st.peer_count);
        reply["download_rate"]    = std::to_string(st.download_rate);
        reply["upload_rate"]      = std::to_string(st.upload_rate);
        reply["total_downloaded"] = std::to_string(st.total_downloaded);
        reply["total_uploaded"]   = std::to_string(st.total_uploaded);
        reply["disk_usage"]       = std::to_string(st.disk_usage);
        reply["disk_budget"]      = std::to_string(st.disk_budget);
        reply["over_budget"]      = std::to_string(st.over_budget);
        reply["file_count"]       = std::to_string(st.file_count);
        return reply;
    }

    if (cmd == "list") {
        int count = 0;
        levin_torrent_t* torrents = levin_get_torrents(ctx, &count);
        Message reply;
        reply["count"] = std::to_string(count);
        for (int i = 0; i < count; ++i) {
            std::string prefix = "t" + std::to_string(i) + "_";
            reply[prefix + "hash"]     = torrents[i].info_hash;
            reply[prefix + "name"]     = torrents[i].name ? torrents[i].name : "";
            reply[prefix + "size"]     = std::to_string(torrents[i].size);
            reply[prefix + "downloaded"] = std::to_string(torrents[i].downloaded);
            reply[prefix + "uploaded"] = std::to_string(torrents[i].uploaded);
            reply[prefix + "down_rate"]= std::to_string(torrents[i].download_rate);
            reply[prefix + "up_rate"]  = std::to_string(torrents[i].upload_rate);
            reply[prefix + "peers"]    = std::to_string(torrents[i].num_peers);
            reply[prefix + "progress"] = std::to_string(torrents[i].progress);
            reply[prefix + "seed"]     = std::to_string(torrents[i].is_seed);
        }
        levin_free_torrents(torrents, count);
        return reply;
    }

    if (cmd == "pause") {
        levin_set_enabled(ctx, 0);
        return {{"ok", "1"}};
    }

    if (cmd == "resume") {
        levin_set_enabled(ctx, 1);
        return {{"ok", "1"}};
    }

    return {{"error", "unknown command: " + cmd}};
}

// ---------------------------------------------------------------------------
// Daemon main loop
// ---------------------------------------------------------------------------

static int run_daemon() {
    using namespace levin::linux_shell;

    ensure_dir(default_runtime_dir());

    // Check if already running
    pid_t existing = read_pid_file(pid_path());
    if (existing > 0 && is_process_running(existing)) {
        std::fprintf(stderr, "levin: daemon already running (pid %d)\n",
                     static_cast<int>(existing));
        return 1;
    }

    // Daemonize
    int ret = daemonize(pid_path());
    if (ret < 0) {
        std::fprintf(stderr, "levin: daemonization failed\n");
        return 1;
    }

    // From here, we are the daemon process (stdin/stdout/stderr -> /dev/null)
    install_signal_handlers();

    // Load configuration
    ShellConfig cfg = load_config();

    // Create levin context
    levin_t* ctx = levin_create(&cfg.lib_config);
    if (!ctx) {
        remove_pid_file(pid_path());
        return 1;
    }

    // Start the torrent session
    if (levin_start(ctx) != 0) {
        levin_destroy(ctx);
        remove_pid_file(pid_path());
        return 1;
    }

    // Set up IPC server
    IpcServer ipc;
    if (ipc.start(socket_path(), [ctx](const Message& req) {
            return handle_ipc(ctx, req);
        }) != 0) {
        levin_stop(ctx);
        levin_destroy(ctx);
        remove_pid_file(pid_path());
        return 1;
    }

    // Torrent watcher is now managed internally by liblevin (levin_start/levin_tick)

    // Desktop assumption: always on AC, always has network
    levin_update_battery(ctx, 1);
    levin_update_network(ctx, 1, 0);

    // Tick counter for periodic tasks
    int disk_check_counter = 0;
    int disk_interval = cfg.lib_config.disk_check_interval_secs;
    if (disk_interval <= 0) disk_interval = 60;

    // Initial storage update
    {
        StorageInfo si = get_storage_info(cfg.data_dir);
        levin_update_storage(ctx, si.fs_total, si.fs_free);
    }

    // Enable seeding
    levin_set_enabled(ctx, 1);

    // -----------------------------------------------------------------------
    // Main loop: tick once per second
    // -----------------------------------------------------------------------
    while (!shutdown_requested()) {
        levin_tick(ctx);
        ipc.poll();

        // Periodic storage check
        ++disk_check_counter;
        if (disk_check_counter >= disk_interval) {
            disk_check_counter = 0;
            StorageInfo si = get_storage_info(cfg.data_dir);
            levin_update_storage(ctx, si.fs_total, si.fs_free);

            // Also refresh power status
            int on_ac = is_on_ac_power() ? 1 : 0;
            levin_update_battery(ctx, on_ac);
        }

        // Config reload on SIGHUP
        if (reload_requested()) {
            cfg = load_config();
            levin_set_download_limit(ctx, cfg.lib_config.max_download_kbps);
            levin_set_upload_limit(ctx, cfg.lib_config.max_upload_kbps);
            levin_set_run_on_battery(ctx, cfg.lib_config.run_on_battery);
            levin_set_run_on_cellular(ctx, cfg.lib_config.run_on_cellular);
        }

        ::sleep(1);
    }

    // -----------------------------------------------------------------------
    // Shutdown
    // -----------------------------------------------------------------------
    ipc.stop();
    levin_stop(ctx);
    levin_destroy(ctx);
    remove_pid_file(pid_path());

    return 0;
}

// ---------------------------------------------------------------------------
// CLI commands
// ---------------------------------------------------------------------------

static int cmd_stop() {
    using namespace levin::linux_shell;
    pid_t pid = read_pid_file(pid_path());
    if (pid <= 0 || !is_process_running(pid)) {
        std::fprintf(stderr, "levin: daemon is not running\n");
        return 1;
    }
    ::kill(pid, SIGTERM);
    std::printf("levin: sent shutdown signal to pid %d\n", static_cast<int>(pid));
    return 0;
}

static int cmd_status() {
    using namespace levin::linux_shell;
    Message reply = IpcClient::send(socket_path(), {{"command", "status"}});
    if (reply.empty()) {
        std::fprintf(stderr, "levin: daemon is not running or not responding\n");
        return 1;
    }
    auto get = [&](const std::string& k) -> std::string {
        auto it = reply.find(k);
        return it != reply.end() ? it->second : "";
    };

    std::printf("State:       %s\n", get("state").c_str());
    std::printf("Torrents:    %s\n", get("torrent_count").c_str());
    std::printf("Books:       %s\n", format_number(get("file_count")).c_str());
    std::printf("Peers:       %s\n", get("peer_count").c_str());
    std::printf("Download:    %s\n",
                format_rate(std::atoi(get("download_rate").c_str())).c_str());
    std::printf("Upload:      %s\n",
                format_rate(std::atoi(get("upload_rate").c_str())).c_str());
    std::printf("Downloaded:  %s\n",
                format_bytes(std::strtoull(get("total_downloaded").c_str(),
                                           nullptr, 10)).c_str());
    std::printf("Uploaded:    %s\n",
                format_bytes(std::strtoull(get("total_uploaded").c_str(),
                                           nullptr, 10)).c_str());
    std::printf("Disk usage:  %s\n",
                format_bytes(std::strtoull(get("disk_usage").c_str(),
                                           nullptr, 10)).c_str());
    std::printf("Disk budget: %s\n",
                format_bytes(std::strtoull(get("disk_budget").c_str(),
                                           nullptr, 10)).c_str());
    std::printf("Over budget: %s\n",
                get("over_budget") == "1" ? "yes" : "no");
    return 0;
}

static int cmd_list() {
    using namespace levin::linux_shell;
    Message reply = IpcClient::send(socket_path(), {{"command", "list"}});
    if (reply.empty()) {
        std::fprintf(stderr, "levin: daemon is not running or not responding\n");
        return 1;
    }

    auto get = [&](const std::string& k) -> std::string {
        auto it = reply.find(k);
        return it != reply.end() ? it->second : "";
    };

    int count = std::atoi(get("count").c_str());
    if (count == 0) {
        std::printf("No torrents.\n");
        return 0;
    }

    for (int i = 0; i < count; ++i) {
        std::string p = "t" + std::to_string(i) + "_";
        std::string name = get(p + "name");
        if (name.empty()) name = get(p + "hash");
        double progress = std::strtod(get(p + "progress").c_str(), nullptr);
        int peers = std::atoi(get(p + "peers").c_str());
        bool seed = get(p + "seed") == "1";

        std::printf("%-40s  %5.1f%%  %s  %d peer%s  D:%s  U:%s\n",
            name.c_str(),
            progress * 100.0,
            seed ? "seed" : "    ",
            peers,
            peers == 1 ? "" : "s",
            format_rate(std::atoi(get(p + "down_rate").c_str())).c_str(),
            format_rate(std::atoi(get(p + "up_rate").c_str())).c_str());
    }
    return 0;
}

static int cmd_pause() {
    using namespace levin::linux_shell;
    Message reply = IpcClient::send(socket_path(), {{"command", "pause"}});
    if (reply.empty()) {
        std::fprintf(stderr, "levin: daemon is not running or not responding\n");
        return 1;
    }
    std::printf("levin: paused\n");
    return 0;
}

static int cmd_resume() {
    using namespace levin::linux_shell;
    Message reply = IpcClient::send(socket_path(), {{"command", "resume"}});
    if (reply.empty()) {
        std::fprintf(stderr, "levin: daemon is not running or not responding\n");
        return 1;
    }
    std::printf("levin: resumed\n");
    return 0;
}

static int cmd_populate() {
    // Runs in foreground (not in daemon). Fetches torrents from Anna's Archive.
    using namespace levin::linux_shell;
    ShellConfig cfg = load_config();

    std::printf("Fetching torrents from Anna's Archive into %s ...\n",
                cfg.watch_dir.c_str());

    int result = levin::AnnaArchive::populate_torrents(
        cfg.watch_dir,
        [](int current, int total, const std::string& message) {
            std::printf("[%d/%d] %s\n", current, total, message.c_str());
            std::fflush(stdout);
        });

    if (result < 0) {
        std::fprintf(stderr, "levin: populate failed\n");
        return 1;
    }
    std::printf("Done. %d torrents downloaded.\n", result);
    return 0;
}

// ---------------------------------------------------------------------------
// Usage
// ---------------------------------------------------------------------------

static void print_usage() {
    std::printf(
        "Usage: levin <command>\n"
        "\n"
        "Commands:\n"
        "  start      Start the daemon\n"
        "  stop       Stop the daemon\n"
        "  status     Show daemon status\n"
        "  list       List active torrents\n"
        "  pause      Pause all seeding/downloading\n"
        "  resume     Resume seeding/downloading\n"
        "  populate   Fetch torrents from Anna's Archive (foreground)\n"
        "\n");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    const std::string cmd = argv[1];

    if (cmd == "start")    return run_daemon();
    if (cmd == "stop")     return cmd_stop();
    if (cmd == "status")   return cmd_status();
    if (cmd == "list")     return cmd_list();
    if (cmd == "pause")    return cmd_pause();
    if (cmd == "resume")   return cmd_resume();
    if (cmd == "populate") return cmd_populate();

    if (cmd == "help" || cmd == "--help" || cmd == "-h") {
        print_usage();
        return 0;
    }

    std::fprintf(stderr, "levin: unknown command '%s'\n", cmd.c_str());
    print_usage();
    return 1;
}
