# Levin - Agent Notes

## Project Overview
Levin is a background torrent seeding client for Anna's Archive. It uses a shared C++ library (`liblevin`) with thin platform shells.

## Architecture
- `liblevin/` - Core C++ library with C API
  - `include/` - Public headers (state_machine.h, disk_manager.h, liblevin.h, torrent_session.h, torrent_watcher.h, annas_archive.h, statistics.h)
  - `src/` - Implementation files
  - `tests/` - Catch2 unit tests + test.torrent fixture
- `platforms/linux/` - Linux daemon + CLI (single binary)
- `platforms/android/` - Android app (Kotlin + JNI)
- `e2e/` - BATS end-to-end tests (28 Linux tests + 30 Android tests across 8 files)
- `packaging/` - systemd, deb, rpm, AUR packaging

## Build System
- CMake 3.20+ required
- Catch2 v3.5.2 fetched via FetchContent
- libtorrent master branch fetched via FetchContent when `LEVIN_USE_STUB_SESSION=OFF` (with WebRTC/libdatachannel)
- libcurl required (system package, Linux only)
- Build (stub mode, fast): `cmake -S . -B build -DLEVIN_USE_STUB_SESSION=ON && cmake --build build -j$(nproc)`
- Build (real libtorrent + WebRTC): `cmake -S . -B build-real -DLEVIN_USE_STUB_SESSION=OFF && cmake --build build-real -j$(nproc)`
- Tests: `ctest --output-on-failure --test-dir build` (or `--test-dir build-real`)
- E2E (Linux stub): `LEVIN_BIN=./build/platforms/linux/levin bats e2e/test_*.bats`
- E2E (Linux real): `LEVIN_BIN=./build-real/platforms/linux/levin bats e2e/test_*.bats`
- E2E (Android): `bats e2e/android/` (requires connected device + installed APK)
- Android deps: `cd platforms/android && ./build-deps.sh` (downloads Boost headers, builds OpenSSL for Android ABIs)
- Android build: `cd platforms/android && ./gradlew assembleDebug`

## Implementation Phases (TDD)
1. State Machine (pure logic) - DONE
2. Disk Budget Calculation (pure math) - DONE
3. File Deletion (filesystem ops) - DONE
4. C API (integration, stub session) - DONE
5. BitTorrent Session (libtorrent RC_2_0) - DONE
   - TorrentWatcher (inotify) - DONE
   - AnnaArchive client (libcurl) - DONE
   - Note: WebTorrent/WebRTC requires libtorrent master (not RC_2_0)
   - WebRTC migration to master branch - DONE (Linux + Android)
6. Linux Shell - DONE
   - Daemon (double-fork, PID file, signals)
   - CLI (start/stop/status/list/pause/resume/populate)
   - IPC (Unix socket, JSON messages)
   - Config (TOML parser, XDG paths, env expansion)
   - Power (sysfs /sys/class/power_supply)
   - Storage (statvfs + block-based du)
7. Linux Packaging - DONE (systemd, deb, rpm, AUR)
8. Android Shell - DONE
   - Gradle + CMake cross-compilation (arm64, x86_64)
   - JNI bridge (LevinNative.kt + jni_bridge.cpp)
   - Foreground service with 1s tick
   - Power/Network/Storage monitors
   - Stats + Settings UI
   - AnnaArchive client (pure Kotlin HttpURLConnection, no libcurl dependency)
   - BootReceiver for start-on-boot
   - Real libtorrent + WebRTC via libdatachannel (not stub session)

## Key Design Decisions
- `LEVIN_USE_STUB_SESSION` CMake option allows building/testing without libtorrent
- State machine uses priority-ordered evaluation (OFF > PAUSED > IDLE > SEEDING > DOWNLOADING)
- Disk budget has 50MB hysteresis to prevent download-delete thrashing
- All `levin_*` API calls must come from the same thread (Android uses HandlerThread)
- File deletion uses random order per design doc
- ITorrentSession interface enables stub/real session swapping
- TorrentWatcher uses inotify on Linux, no-op on other platforms (future: FSEvents, ReadDirectoryChangesW)
- TorrentWatcher only fires on_add for IN_CLOSE_WRITE and IN_MOVED_TO (not IN_CREATE) to prevent double-adding
- TorrentWatcher is now integrated into liblevin core (levin_start/levin_tick/levin_stop manage it)
- AnnaArchive uses libcurl with retry/backoff on Linux; Android uses pure Kotlin HttpURLConnection (no curl dependency)
- Linux CLI and daemon are a single binary (subcommand routing)
- IPC uses Unix domain sockets with hand-rolled JSON serialization (no dependency)
- Config parser handles TOML subset: key=value, strings, numbers, booleans, comments
- SIGHUP reloads config: bandwidth limits, run_on_battery, run_on_cellular
- Statistics module persists cumulative download/upload totals across sessions (binary format, saved every 5 min)
- Session state (libtorrent) persisted on stop, restored on start
- Disk usage uses stat() st_blocks*512 for accurate block-based measurement on Linux/macOS
- WebRTC migration: switched from libtorrent RC_2_0 to master branch for TORRENT_USE_RTC support
- libtorrent master with `deprecated-functions=OFF` sets TORRENT_ABI_VERSION=100, removing old constructors
- Use `lt::load_torrent_file()` instead of deprecated `lt::torrent_info(string)` constructor
- Android builds use prebuilt OpenSSL (cross-compiled via build-deps.sh) and downloaded Boost headers
- armeabi-v7a dropped from Android ABIs due to OpenSSL ARM assembly PIC relocation issues
- E2E test start_daemon waits for IPC socket (not just PID file) since real libtorrent takes longer to init

## Future Work
- Add `bandwidth` CLI command per design doc
- macOS shell (IOKit power, launchd, Homebrew formula)
- Windows shell (system tray, Windows Service)
- iOS shell (BGTaskScheduler, constrained background limits)

## Environment
- Ubuntu 24.04, g++ 13.3, cmake 3.28
- Android SDK at ~/Android/Sdk, NDK r27 (27.0.12077973), API 34
- Android device connected and authorized
- libcurl and openssl available as system packages
- BATS installed for E2E tests
