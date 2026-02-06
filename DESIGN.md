# Levin: Design & Implementation Guide

## What is Levin

Levin is a background torrent seeding client for [Anna's Archive](https://annas-archive.org). It runs quietly on your device, downloads archive torrents when there's disk space available, seeds them to others, and stops using resources when you're on battery or cellular. It supports WebTorrent so web browsers can download directly from Levin seeders.

## Core Principles

1. **Seed Anna's Archive torrents.** Fetch torrent files from Anna's Archive, download their data, seed continuously. WebTorrent support is mandatory and on by default -- browsers must be able to connect.

2. **Respect the device.** By default, only run when plugged in and on WiFi. On desktop, only run when on AC power. All overridable by the user.

3. **Never exceed disk limits.** If the user says "use at most 50 GB," the app must *never* use more. If external factors reduce available space, Levin deletes its own data immediately. When space becomes available again, it downloads more.

4. **One shared library, many platforms.** A single C++ library (`liblevin`) contains all business logic: state machine, disk management, torrent session, Anna's Archive integration. Platform shells (Android, Linux, macOS, Windows, iOS) are thin wrappers providing lifecycle, UI, and platform-specific monitoring.

5. **WebTorrent via libtorrent.** libtorrent's master branch supports WebTorrent through libdatachannel. We compile libtorrent from source -- never rely on system packages, which lack WebRTC support.

## Architecture

```
┌─────────────────────────────────────────────────┐
│              Platform Shells (thin)              │
│  ┌───────────┐ ┌─────────┐ ┌───────┐ ┌───────┐ │
│  │  Android   │ │  Linux  │ │ macOS │ │Windows│ │
│  │ Service+UI │ │ Daemon  │ │Launch │ │Service│ │
│  │  (Kotlin)  │ │  +CLI   │ │ Agent │ │(C#/C) │ │
│  └─────┬─────┘ └────┬────┘ └───┬───┘ └───┬───┘ │
│        │ JNI        │ direct   │ direct   │     │
├────────┴────────────┴──────────┴──────────┴─────┤
│                 liblevin (C API)                 │
│  ┌────────────────────────────────────────────┐  │
│  │           Core Logic (C++)                 │  │
│  │  State Machine · Disk Manager · Stats      │  │
│  │  Anna's Archive Client · Config Parser     │  │
│  └──────────────────┬─────────────────────────┘  │
│                     │ links                       │
│  ┌──────────────────┴─────────────────────────┐  │
│  │  libtorrent-rasterbar (compiled from src)  │  │
│  │  + libdatachannel (WebRTC/WebTorrent)      │  │
│  └────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────┘
```

**Platform shells** are responsible for:
- Starting/stopping `liblevin` based on platform lifecycle
- Providing battery, network, and storage status by calling `levin_update_*` functions
- Rendering UI (Android activities, desktop CLI output)
- Packaging and installation

**Platform shells must NOT contain:**
- State machine logic
- Disk budget calculations
- Torrent session management
- Anna's Archive fetching logic

## The C API (`liblevin.h`)

The shared library exposes a C API so it can be called from any language.

```c
// --- Lifecycle ---
levin_t* levin_create(const levin_config_t* config);
void     levin_destroy(levin_t* ctx);
int      levin_start(levin_t* ctx);
void     levin_stop(levin_t* ctx);
void     levin_tick(levin_t* ctx);  // shell calls this every ~1 second

// --- Condition Updates (called by platform shell) ---
void levin_update_battery(levin_t* ctx, int on_ac_power);
void levin_update_network(levin_t* ctx, int has_wifi, int has_cellular);
void levin_update_storage(levin_t* ctx, uint64_t fs_total, uint64_t fs_free);

// --- Torrent Management ---
int  levin_add_torrent(levin_t* ctx, const char* torrent_path);
void levin_remove_torrent(levin_t* ctx, const char* info_hash);

// --- Status ---
levin_status_t    levin_get_status(levin_t* ctx);
levin_torrent_t*  levin_get_torrents(levin_t* ctx, int* count);
void              levin_free_torrents(levin_torrent_t* list);

// --- Settings (runtime) ---
void levin_set_enabled(levin_t* ctx, int enabled);
void levin_set_download_limit(levin_t* ctx, int kbps);  // 0 = unlimited
void levin_set_upload_limit(levin_t* ctx, int kbps);

// --- Anna's Archive ---
int levin_populate_torrents(levin_t* ctx, levin_progress_cb cb, void* userdata);

// --- Callbacks ---
typedef void (*levin_state_cb)(levin_state_t old_state, levin_state_t new_state, void* userdata);
void levin_set_state_callback(levin_t* ctx, levin_state_cb cb, void* userdata);
```

### Key types

```c
typedef enum {
    LEVIN_STATE_OFF,
    LEVIN_STATE_PAUSED,
    LEVIN_STATE_IDLE,
    LEVIN_STATE_SEEDING,
    LEVIN_STATE_DOWNLOADING
} levin_state_t;

typedef struct {
    const char* watch_directory;
    const char* data_directory;
    const char* state_directory;
    uint64_t min_free_bytes;
    double   min_free_percentage;   // 0.05 = 5%
    uint64_t max_storage_bytes;     // 0 = unlimited
    int run_on_battery;             // default: 0
    int run_on_cellular;            // default: 0
    int disk_check_interval_secs;   // default: 60
    int max_download_kbps;          // 0 = unlimited
    int max_upload_kbps;            // 0 = unlimited
    const char* stun_server;        // default: "stun.l.google.com:19302"
} levin_config_t;

typedef struct {
    levin_state_t state;
    int           torrent_count;
    int           peer_count;
    int           download_rate;    // bytes/sec
    int           upload_rate;      // bytes/sec
    uint64_t      total_downloaded;
    uint64_t      total_uploaded;
    uint64_t      disk_usage;
    uint64_t      disk_budget;
    int           over_budget;
} levin_status_t;
```

### Design rules

- All strings passed in are copied internally; the caller owns the original.
- `levin_tick()` is the heartbeat. All periodic work (disk checks, torrent scanning, stats saving) runs inside `tick` based on internal timers.
- `levin_update_*` functions are called by the shell whenever conditions change. Redundant calls are deduplicated internally.
- The library is single-threaded from the caller's perspective. All `levin_*` calls must come from the same thread. libtorrent's internal threads are managed by the library.

## State Machine

Five states, five conditions, evaluated in priority order:

| Priority | Condition                     | Result State   |
|----------|-------------------------------|----------------|
| 1        | `!enabled`                    | OFF            |
| 2        | `!battery_ok OR !network_ok`  | PAUSED         |
| 3        | `!has_torrents`               | IDLE           |
| 4        | `!storage_ok`                 | SEEDING        |
| 5        | *(default)*                   | DOWNLOADING    |

### Condition definitions

| Condition     | True when                                                        |
|---------------|------------------------------------------------------------------|
| `enabled`     | User has not disabled the app. Desktop: always true while daemon is running. Android: UI toggle. |
| `battery_ok`  | On AC power, OR `run_on_battery` is set                          |
| `network_ok`  | On WiFi/Ethernet, OR `run_on_cellular` is set. Desktop: always true. |
| `has_torrents` | At least one torrent is loaded in the session                   |
| `storage_ok`  | Disk budget > 0 (see below)                                     |

### Actions on state transitions

| Entering State | Action                                                         |
|----------------|----------------------------------------------------------------|
| OFF            | Pause libtorrent session entirely (zero network activity)      |
| PAUSED         | Pause libtorrent session entirely                              |
| IDLE           | Resume session (DHT stays alive, ready for torrents)           |
| SEEDING        | Resume session, set download rate limit to 1 byte/sec          |
| DOWNLOADING    | Resume session, restore configured download rate limit         |

## Disk Space Management

The invariant: **Levin must never use more disk space than permitted.**

### Budget calculation

Runs every `disk_check_interval_secs` and also when a torrent is added:

```
min_required     = max(min_free_bytes, fs_total * min_free_percentage)
available_space  = max(0, fs_free - min_required)

if max_storage > 0:
    available_for_levin = max(0, max_storage - current_usage)
    budget = min(available_space, available_for_levin)
    over_budget = (current_usage > max_storage) OR (budget == 0)
    deficit = max(0, current_usage - max_storage)
else:
    budget = available_space
    over_budget = (budget == 0)
    deficit = max(0, min_required - fs_free)

# 50 MB hysteresis to prevent download-delete thrashing
budget = max(0, budget - 50MB)
if budget == 0: over_budget = true
```

### When over budget

1. Set `storage_ok = false` → state machine transitions to SEEDING → downloads limited to 1 byte/sec.
2. Delete files from `data_directory` in random order until `deficit` bytes are freed.
3. On next tick, recalculate. If budget > 0, set `storage_ok = true` → transitions to DOWNLOADING.

### On torrent add

Check disk budget before adding a torrent. If already over budget, set download rate to 1 byte/sec before the torrent starts. This prevents a burst of downloads before the next disk check.

### Measuring usage

Use actual disk blocks consumed, not apparent file size. On Linux/macOS: equivalent of `du -s`. On Android: `StorageStatsManager` with `du -s` fallback. This correctly handles sparse files.

## BitTorrent Configuration

### libtorrent setup

Build from source (master branch) with WebTorrent enabled via libdatachannel. Session settings:

- Listen on `0.0.0.0:6881`
- Enable: DHT, LSD, UPnP, NAT-PMP
- Max 50 connections per torrent, 200 total
- Alert mask: error, status, storage
- STUN server: configurable, default `stun.l.google.com:19302`
- Save/restore session state (DHT table, etc.) across restarts

### WebTorrent tracker injection

Every torrent gets these WebSocket trackers at tier 0:

```
wss://tracker.openwebtorrent.com
wss://tracker.webtorrent.dev
wss://tracker.btorrent.xyz
```

### Torrent watching

Monitor `watch_directory` for `.torrent` file changes using the platform's filesystem notification API (`inotify` on Linux, `FileObserver` on Android, `FSEvents` on macOS, `ReadDirectoryChangesW` on Windows). Add/remove torrents from session accordingly.

## Anna's Archive Integration

### Torrent fetching API

```
GET https://annas-archive.org/dyn/generate_torrents?max_tb=1&format=url
```

Returns newline-separated `.torrent` file URLs. Download each to `watch_directory`, skipping existing files. Retry up to 3 times with exponential backoff. Timeout: 30 seconds per request.

### First-run population

On first launch (empty `watch_directory`), prompt the user to fetch torrents. Desktop: terminal prompt. Android: dialog. This is the primary onboarding path.

## Platform Shells

### Linux

- **Daemon:** double-fork daemonization, PID file, SIGTERM/SIGINT for shutdown, SIGHUP for reload.
- **CLI:** same binary, IPC over Unix socket with JSON protocol. Commands: `start`, `stop`, `status`, `list`, `pause`, `resume`, `bandwidth`.
- **Config:** TOML file at `$XDG_CONFIG_HOME/levin/levin.toml`. Supports `~` and `$VAR` expansion, human-readable sizes.
- **Power:** DBus/UPower: subscribe to `PropertiesChanged` on `org.freedesktop.UPower` DisplayDevice. State 1 (charging) or 4 (fully-charged) = AC.
- **Network:** Always true.
- **Storage:** `statvfs()` + `du -s` equivalent.
- **Packaging:** deb, rpm, AUR PKGBUILD, systemd user service.

### macOS

Same as Linux except: IOKit `IOPSCopyPowerSourcesInfo` for power, launchd instead of systemd, Homebrew formula for packaging.

### Android

- **Service:** foreground service (`START_STICKY`). Calls `levin_tick()` every 1 second via JNI. The JNI layer is minimal type marshaling.
- **Monitoring:** BroadcastReceiver for power. `ConnectivityManager.NetworkCallback` for WiFi/cellular. `StatFs` for storage. Each calls `levin_update_*` via JNI.
- **Config:** SharedPreferences, exposed through settings UI.
- **UI:** Two screens:
  - **Stats:** state, speeds, totals, disk usage/budget, torrent count, peer count. Enable/disable toggle.
  - **Settings:** min_free (GB), max_storage (GB), run on battery, run on cellular, run on startup, populate torrents.
- **Build:** Gradle builds Kotlin shell. CMake cross-compiles `liblevin.so` for arm64-v8a, armeabi-v7a, x86_64 via NDK.

### Windows / iOS (future)

Not specified. Windows: system tray app or service. iOS: constrained by background limits, likely BGTaskScheduler.

## Configuration Reference

| Option                     | Type   | Default                        | Description                            |
|----------------------------|--------|--------------------------------|----------------------------------------|
| `watch_directory`          | path   | `~/.config/levin/torrents`     | Where `.torrent` files go              |
| `data_directory`           | path   | `~/.cache/levin/data`          | Where downloaded data is stored        |
| `state_directory`          | path   | `~/.local/state/levin`         | PID, log, socket, session state, stats |
| `min_free_bytes`           | size   | `1 GB`                         | Minimum free space to preserve         |
| `min_free_percentage`      | float  | `0.05`                         | Min free as fraction of total disk     |
| `max_storage_bytes`        | size   | `0` (unlimited)                | Max space Levin may use                |
| `run_on_battery`           | bool   | `false`                        | Run when on battery power              |
| `run_on_cellular`          | bool   | `false`                        | Run on cellular (Android)              |
| `disk_check_interval_secs` | int    | `60`                           | Seconds between disk checks            |
| `max_download_kbps`        | int    | `0` (unlimited)                | Download rate limit in KB/s            |
| `max_upload_kbps`          | int    | `0` (unlimited)                | Upload rate limit in KB/s              |
| `stun_server`              | string | `stun.l.google.com:19302`      | STUN server for WebRTC                 |
| `log_level`                | string | `info`                         | trace/debug/info/warn/error/critical   |

Desktop: TOML file with human-readable sizes (`"10gb"`, `"500mb"`). Android: SharedPreferences.

## Build System

### Directory structure

```
liblevin/
├── CMakeLists.txt           # fetches libtorrent, builds liblevin
├── include/
│   └── liblevin.h           # public C API
├── src/
│   ├── levin.cpp            # C API implementation
│   ├── state_machine.cpp
│   ├── disk_manager.cpp
│   ├── torrent_session.cpp  # wraps libtorrent
│   ├── torrent_watcher.cpp
│   ├── annas_archive.cpp
│   ├── statistics.cpp
│   └── config.cpp
└── tests/
    ├── test_state_machine.cpp
    ├── test_disk_manager.cpp
    ├── test_disk_deletion.cpp
    ├── test_torrent_session.cpp
    ├── test_c_api.cpp
    └── fixtures/
        └── test.torrent
```

libtorrent is fetched via CMake `FetchContent` from the master branch, built as a static library with `-Dwebtorrent=ON`. libdatachannel is pulled transitively.

### Android cross-compilation

Android NDK toolchain with CMake. Build `liblevin.so` per ABI. Gradle invokes CMake via `externalNativeBuild`.

### Desktop

CMake builds `liblevin` as a static library, links into the `levin` binary with platform-specific daemon/CLI/monitor code.

## Testing Strategy: Test-Driven Development

Tests are organized in phases. Each phase builds on the previous. **Write the tests first, then implement until they pass.**

Core tests use Catch2. E2E tests use BATS. Implement in order:

1. State machine (pure logic, no dependencies)
2. Disk budget calculation (pure math)
3. File deletion (filesystem only)
4. C API (with stub session initially)
5. libtorrent session integration (WebTorrent, trackers, rate limits)
6. Desktop E2E (daemon + CLI)
7. Android E2E (app + adb)

### Phase 1: State Machine

Pure logic, no I/O. The foundation everything else depends on.

```cpp
// tests/test_state_machine.cpp

TEST_CASE("Initial state is OFF") {
    StateMachine sm;
    REQUIRE(sm.state() == State::OFF);
}

TEST_CASE("Enabling with no conditions met goes to PAUSED") {
    StateMachine sm;
    sm.update_enabled(true);
    REQUIRE(sm.state() == State::PAUSED);
}

TEST_CASE("All conditions met but no torrents goes to IDLE") {
    StateMachine sm;
    sm.update_enabled(true);
    sm.update_battery(true);
    sm.update_network(true);
    REQUIRE(sm.state() == State::IDLE);
}

TEST_CASE("All conditions met with torrents and storage goes to DOWNLOADING") {
    StateMachine sm;
    sm.update_enabled(true);
    sm.update_battery(true);
    sm.update_network(true);
    sm.update_has_torrents(true);
    sm.update_storage(true);
    REQUIRE(sm.state() == State::DOWNLOADING);
}

TEST_CASE("Storage full with torrents goes to SEEDING") {
    StateMachine sm;
    sm.update_enabled(true);
    sm.update_battery(true);
    sm.update_network(true);
    sm.update_has_torrents(true);
    sm.update_storage(false);
    REQUIRE(sm.state() == State::SEEDING);
}

TEST_CASE("Disabling always goes to OFF regardless of other conditions") {
    StateMachine sm;
    sm.update_enabled(true);
    sm.update_battery(true);
    sm.update_network(true);
    sm.update_has_torrents(true);
    sm.update_storage(true);
    REQUIRE(sm.state() == State::DOWNLOADING);

    sm.update_enabled(false);
    REQUIRE(sm.state() == State::OFF);
}

TEST_CASE("Battery loss overrides torrent and storage conditions") {
    StateMachine sm;
    sm.update_enabled(true);
    sm.update_battery(true);
    sm.update_network(true);
    sm.update_has_torrents(true);
    sm.update_storage(true);

    sm.update_battery(false);
    REQUIRE(sm.state() == State::PAUSED);
}

TEST_CASE("Network loss transitions to PAUSED") {
    StateMachine sm;
    sm.update_enabled(true);
    sm.update_battery(true);
    sm.update_network(true);
    sm.update_has_torrents(true);
    sm.update_storage(true);

    sm.update_network(false);
    REQUIRE(sm.state() == State::PAUSED);
}

TEST_CASE("State callback fires on transition") {
    StateMachine sm;
    std::vector<std::pair<State, State>> transitions;
    sm.set_callback([&](State o, State n) { transitions.push_back({o, n}); });

    sm.update_enabled(true);
    sm.update_battery(true);
    sm.update_network(true);

    REQUIRE(transitions.size() >= 1);
    REQUIRE(transitions.back().second == State::IDLE);
}

TEST_CASE("Redundant updates do not fire callback") {
    StateMachine sm;
    int count = 0;
    sm.set_callback([&](State, State) { count++; });

    sm.update_enabled(true);
    sm.update_battery(true);
    sm.update_network(true);
    int after_setup = count;

    sm.update_battery(true);  // same value
    sm.update_network(true);  // same value
    REQUIRE(count == after_setup);
}

TEST_CASE("SEEDING to DOWNLOADING when storage freed") {
    StateMachine sm;
    sm.update_enabled(true);
    sm.update_battery(true);
    sm.update_network(true);
    sm.update_has_torrents(true);
    sm.update_storage(false);
    REQUIRE(sm.state() == State::SEEDING);

    sm.update_storage(true);
    REQUIRE(sm.state() == State::DOWNLOADING);
}

TEST_CASE("Removing all torrents goes to IDLE even if storage ok") {
    StateMachine sm;
    sm.update_enabled(true);
    sm.update_battery(true);
    sm.update_network(true);
    sm.update_has_torrents(true);
    sm.update_storage(true);
    REQUIRE(sm.state() == State::DOWNLOADING);

    sm.update_has_torrents(false);
    REQUIRE(sm.state() == State::IDLE);
}
```

### Phase 2: Disk Budget Calculation

Pure math. No filesystem access -- takes numbers in, returns a budget.

```cpp
// tests/test_disk_manager.cpp

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
```

### Phase 3: File Deletion

Tests filesystem operations -- creating temp files, deleting them.

```cpp
// tests/test_disk_deletion.cpp

// Helper: TempDir creates a temp directory, cleans up on destruction.
// Helper: create_file(path, size_bytes) writes a file of given size.
// Helper: dir_size(path) returns total bytes of regular files.

TEST_CASE("delete_to_free removes enough data to meet deficit") {
    TempDir dir;
    for (int i = 0; i < 10; i++)
        create_file(dir / ("f" + std::to_string(i)), 10*MB);
    REQUIRE(dir_size(dir) == 100*MB);

    DiskManager dm;
    uint64_t freed = dm.delete_to_free(dir, 30*MB);
    REQUIRE(freed >= 30*MB);
    REQUIRE(dir_size(dir) <= 70*MB);
}

TEST_CASE("delete_to_free removes nothing when deficit is zero") {
    TempDir dir;
    create_file(dir / "keep.dat", 10*MB);

    DiskManager dm;
    uint64_t freed = dm.delete_to_free(dir, 0);
    REQUIRE(freed == 0);
    REQUIRE(fs::exists(dir / "keep.dat"));
}

TEST_CASE("delete_to_free handles empty directory") {
    TempDir dir;
    DiskManager dm;
    uint64_t freed = dm.delete_to_free(dir, 10*MB);
    REQUIRE(freed == 0);
}

TEST_CASE("delete_to_free does not delete more than necessary") {
    TempDir dir;
    for (int i = 0; i < 5; i++)
        create_file(dir / ("f" + std::to_string(i)), 20*MB);

    DiskManager dm;
    dm.delete_to_free(dir, 25*MB);

    int remaining = 0;
    for (auto& e : fs::directory_iterator(dir))
        if (e.is_regular_file()) remaining++;
    REQUIRE(remaining >= 3);  // deleted at most 2 files (40MB >= 25MB)
}
```

### Phase 4: C API

Tests the public C API as a black box. This is the contract all platform shells depend on. Initially use a stub torrent session, upgrade to real session after Phase 5.

```cpp
// tests/test_c_api.cpp

// Helper: TestFixture creates temp dirs, returns a levin_config_t.

TEST_CASE("Create and destroy context") {
    TestFixture f;
    levin_t* ctx = levin_create(&f.config);
    REQUIRE(ctx != nullptr);
    levin_destroy(ctx);
}

TEST_CASE("Initial state is OFF") {
    TestFixture f;
    levin_t* ctx = levin_create(&f.config);
    REQUIRE(levin_get_status(ctx).state == LEVIN_STATE_OFF);
    levin_destroy(ctx);
}

TEST_CASE("Full conditions with no torrents: IDLE") {
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

TEST_CASE("State callback fires on transition") {
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

TEST_CASE("Battery loss transitions to PAUSED") {
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

TEST_CASE("run_on_battery=true ignores battery state") {
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

TEST_CASE("Torrent list is initially empty") {
    TestFixture f;
    levin_t* ctx = levin_create(&f.config);
    levin_start(ctx);
    int count = 0;
    auto* list = levin_get_torrents(ctx, &count);
    REQUIRE(count == 0);
    levin_free_torrents(list);
    levin_stop(ctx);
    levin_destroy(ctx);
}

TEST_CASE("Status reports disk usage and budget") {
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
```

### Phase 5: BitTorrent Session

Integration tests requiring a running libtorrent session. Use a small test `.torrent` committed to the repo.

```cpp
// tests/test_torrent_session.cpp

TEST_CASE("Session starts and stops cleanly") {
    TorrentSession session;
    session.configure(6881, "stun.l.google.com:19302");
    session.start("/tmp/levin_test_data");
    REQUIRE(session.is_running());
    session.stop();
    REQUIRE(!session.is_running());
}

TEST_CASE("WebTorrent is enabled") {
    TorrentSession session;
    session.configure(6881, "stun.l.google.com:19302");
    session.start("/tmp/levin_test_data");
    REQUIRE(session.is_webtorrent_enabled());
    session.stop();
}

TEST_CASE("Added torrent has WebSocket trackers") {
    TorrentSession session;
    session.configure(6881, "stun.l.google.com:19302");
    session.start("/tmp/levin_test_data");

    auto handle = session.add_torrent("tests/fixtures/test.torrent");
    REQUIRE(handle.has_value());

    auto trackers = session.get_trackers(*handle);
    bool has_wss = false;
    for (auto& t : trackers)
        if (t.rfind("wss://", 0) == 0) { has_wss = true; break; }
    REQUIRE(has_wss);
    session.stop();
}

TEST_CASE("pause_downloads sets rate limit to 1") {
    TorrentSession session;
    session.configure(6881, "stun.l.google.com:19302");
    session.start("/tmp/levin_test_data");
    session.pause_downloads();
    REQUIRE(session.get_download_rate_limit() == 1);
    session.resume_downloads();
    REQUIRE(session.get_download_rate_limit() == 0);
    session.stop();
}

TEST_CASE("pause_session stops all activity") {
    TorrentSession session;
    session.configure(6881, "stun.l.google.com:19302");
    session.start("/tmp/levin_test_data");
    session.pause_session();
    REQUIRE(session.is_paused());
    session.resume_session();
    REQUIRE(!session.is_paused());
    session.stop();
}
```

### Phase 6: Desktop E2E

Full stack tests using the real daemon binary.

```bash
# e2e/test_lifecycle.bats

@test "Daemon starts and creates PID file" {
    start_daemon
    [ -f "${STATE_DIR}/levin.pid" ]
    kill -0 "$(cat ${STATE_DIR}/levin.pid)"
}

@test "CLI status returns valid output" {
    start_daemon
    run levin_cmd status
    [[ "$output" == *"State:"* ]]
}

@test "Daemon starts in IDLE with no torrents" {
    start_daemon
    wait_for_state "Idle" 10
}

@test "Clean shutdown on SIGTERM" {
    start_daemon
    local pid=$(cat "${STATE_DIR}/levin.pid")
    kill "$pid"
    sleep 2
    ! kill -0 "$pid" 2>/dev/null
    [ ! -f "${STATE_DIR}/levin.pid" ]
}
```

```bash
# e2e/test_storage.bats

@test "Over budget triggers SEEDING" {
    start_daemon
    wait_for_state "Idle" 10
    create_files_mb 250  # over 200MB test limit
    add_test_torrent
    wait_for_state "Seeding" 30
}

@test "Files deleted when over budget" {
    start_daemon
    wait_for_state "Idle" 10
    create_files_mb 250
    add_test_torrent
    wait_for_state "Seeding" 30
    sleep 10
    [ "$(get_data_size_mb)" -lt 250 ]
}

@test "Freeing space restores DOWNLOADING" {
    start_daemon
    wait_for_state "Idle" 10
    create_files_mb 250
    add_test_torrent
    wait_for_state "Seeding" 30
    rm -f "${DATA_DIR}"/*.dat
    sleep 15
    wait_for_state "Downloading" 30
}
```

```bash
# e2e/test_power.bats

@test "run_on_battery=true keeps running on battery" {
    set_config "run_on_battery" "true"
    start_daemon
    wait_for_state "Idle" 10
    [[ "$(get_state)" != *"Paused"* ]]
}
```

### Phase 7: Android E2E (via adb)

```bash
# e2e/android/test_app.bats

@test "App installs and launches" {
    adb install -r "$APK_PATH"
    adb shell am start -n com.yoavmoshe.levin/.ui.MainActivity
    sleep 3
    [[ "$(adb shell dumpsys window | grep mCurrentFocus)" == *"levin"* ]]
}

@test "Service starts as foreground" {
    adb shell am startservice -n com.yoavmoshe.levin/.service.LevinService
    sleep 2
    [[ -n "$(adb shell dumpsys activity services | grep LevinService)" ]]
}

@test "Pauses on battery when run_on_battery=false" {
    adb shell dumpsys battery set ac 0
    adb shell dumpsys battery set usb 0
    sleep 10
    [[ "$(get_android_state)" == "Paused" ]]
    adb shell dumpsys battery reset
}

@test "Resumes when power reconnected" {
    adb shell dumpsys battery set ac 0
    sleep 10
    adb shell dumpsys battery set ac 1
    sleep 10
    [[ "$(get_android_state)" != "Paused" ]]
    adb shell dumpsys battery reset
}
```

### Test execution order

Each phase must be fully green before starting the next:

| Phase | Tests | Dependencies |
|-------|-------|--------------|
| 1. State machine | Pure logic | None |
| 2. Disk budget | Pure math | None |
| 3. File deletion | Filesystem | Phase 2 |
| 4. C API | Integration | Phases 1-3 |
| 5. Torrent session | libtorrent | CMake build working |
| 6. Desktop E2E | Full stack | Phases 4-5 + daemon binary |
| 7. Android E2E | Full stack | Phases 4-5 + APK |

Phase 4 can initially use a stub torrent session returning canned data, then switch to the real session after Phase 5 passes.
