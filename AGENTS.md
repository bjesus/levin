# Levin - Agent Notes

## Project Overview
Levin is a background torrent seeding client for Anna's Archive. It uses a shared C++ library (`liblevin`) with thin platform shells.

## Architecture
- `liblevin/` - Core C++ library with C API
  - `include/` - Public headers (state_machine.h, disk_manager.h, liblevin.h)
  - `src/` - Implementation files
  - `tests/` - Catch2 unit tests
- `platforms/linux/` - Linux daemon + CLI
- `platforms/android/` - Android app (Kotlin + JNI)
- `e2e/` - BATS end-to-end tests

## Build System
- CMake 3.20+ required
- Catch2 v3.5.2 fetched via FetchContent
- libtorrent will be fetched from master with `-Dwebtorrent=ON` (Phase 5)
- Build: `mkdir build && cd build && cmake .. && make`
- Tests: `cd build && ctest --output-on-failure`

## Implementation Phases (TDD)
1. State Machine (pure logic) - DONE
2. Disk Budget Calculation (pure math) - DONE
3. File Deletion (filesystem ops) - DONE
4. C API (integration, stub session first)
5. BitTorrent Session (libtorrent + WebTorrent)
6. Linux Shell (daemon, CLI, config, power, storage)
7. Linux Packaging (systemd, deb, rpm, AUR)
8. Android Shell (Gradle, JNI, service, UI)

## Key Design Decisions
- `LEVIN_USE_STUB_SESSION` CMake option allows building/testing without libtorrent
- State machine uses priority-ordered evaluation (OFF > PAUSED > IDLE > SEEDING > DOWNLOADING)
- Disk budget has 50MB hysteresis to prevent download-delete thrashing
- All `levin_*` API calls must come from the same thread
- File deletion uses random order per design doc
- Platform deps (power, network, fs watching) should be behind abstract interfaces

## Environment
- Ubuntu 24.04, g++ 13.3, cmake 3.28
- Android device available (may need USB debugging authorization)
- libcurl and openssl available as system packages
