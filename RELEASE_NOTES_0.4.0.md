# Levin 0.4.0 Release Notes

## E2E Testing & Stability Improvements

This release adds comprehensive end-to-end testing for both platforms and fixes critical stability issues.

---

## Major Changes

### 1. Comprehensive E2E Testing Framework

**Added full E2E test suites for both platforms:**

- **Desktop:** 24 tests covering state machine, storage management, power handling
- **Android:** 14 tests covering state transitions, battery simulation, notifications

**Docker support for reproducible desktop testing:**
```bash
docker build -f Dockerfile.e2e-desktop -t levin-e2e-desktop .
docker run --rm levin-e2e-desktop
```

**Test coverage includes:**
- State machine transitions (IDLE, SEEDING, DOWNLOADING, PAUSED)
- Storage budget enforcement and hysteresis
- File deletion when over budget
- Power/battery condition handling
- Notification state display (Android)

---

### 2. Thread-Safety Fix in Logger (Critical)

**Problem:** Logger could crash during shutdown due to race condition between logging threads and cleanup.

**Symptoms:**
- `terminate called without an active exception`
- Segfaults during daemon shutdown
- Crashes in E2E tests

**Fix:** Added proper thread-safe initialization and shutdown:
- `std::atomic<bool>` flags for initialization state
- `std::mutex` protection during init/shutdown
- Graceful handling of logging after shutdown

**Files changed:**
- `src/logger.hpp` - Added atomic flags and mutex
- `src/logger.cpp` - Thread-safe init/shutdown logic

---

### 3. Unified State Display

**Changed IDLE state display from "Idle (no torrents)" to "No torrents"**

This provides a cleaner, more consistent display across both platforms:

| State | Display Text |
|-------|-------------|
| IDLE | No torrents |
| DOWNLOADING | Downloading |
| SEEDING | Seeding (storage limit) |
| PAUSED | Paused (battery) |
| OFF | Off |

**Files changed:**
- `src/cli_client.cpp` - Updated display format
- `DESIGN.md` - Updated documentation

---

## Testing

### Desktop E2E Tests (24 tests)
```
ok 1 POWER: starts normally when on AC power (charging)
ok 2 POWER: starts normally when fully charged
ok 3 POWER: pauses when on battery (run_on_battery=false)
ok 4 POWER: runs on battery when run_on_battery=true
ok 5 POWER_CONFIG: run_on_battery=true allows operation
ok 6 POWER_CONFIG: status shows run_on_battery setting
ok 7 IDLE: daemon starts in Idle state with no torrents
ok 8 IDLE: status shows 'No torrents' text
ok 9 SEEDING: transitions to Seeding when over max_storage
ok 10 SEEDING: status shows 'Seeding (storage limit)' text
ok 11 SEEDING: files are deleted when over budget
ok 12 PRIORITY: IDLE takes precedence over SEEDING when no torrents
ok 13 PRIORITY: state transitions correctly as conditions change
ok 14 STATUS: shows storage budget information
ok 15 STATUS: budget shows 0 when over limit
ok 16 BUDGET: under limit shows positive budget
ok 17 BUDGET: at limit (within hysteresis) does not trigger Seeding
ok 18 HYSTERESIS: 50MB buffer prevents rapid state changes
ok 19 DELETION: deletes files when over max_storage
ok 20 DELETION: deletes only enough to meet requirement
ok 21 DELETION: deletes individual files not directories
ok 22 DELETION: handles empty data directory gracefully
ok 23 DISK_USAGE: calculates actual disk blocks (sparse file handling)
ok 24 RECOVERY: returns to normal state after freeing space
```

### Android E2E Tests (14 tests)
```
ok 1 STATE: app starts in No torrents state
ok 2 STATE: notification shows No torrents
ok 3 STATE: app loads pre-existing torrents
ok 4 BATTERY: pauses when unplugged (runOnBattery=false)
ok 5 BATTERY: resumes when plugged in
ok 6 BATTERY: runs on battery when runOnBattery=true
ok 7 STORAGE: transitions to Seeding when over limit
ok 8 STORAGE: continues seeding when over limit
ok 9 NETWORK: handles WiFi state changes
ok 10 LIFECYCLE: survives app restart
ok 11 LIFECYCLE: service persists after app close
ok 12 SETTINGS: respects storage limit changes
ok 13 SETTINGS: respects battery preference changes
ok 14 NOTIFICATION: shows correct state information
```

---

## Files Changed

### New Files
- `e2e/` - Complete E2E testing framework
  - `android/test_state_machine.bats` - Android test suite
  - `android/test_helper.bash` - Android test utilities
  - `desktop/test_state_machine.bats` - Desktop state tests
  - `desktop/test_storage.bats` - Desktop storage tests
  - `desktop/test_power.bats` - Desktop power tests
  - `desktop/test_helper.bash` - Desktop test utilities
  - `README.md` - Testing documentation
- `Dockerfile.e2e-desktop` - Docker image for desktop E2E tests
- `docker-compose.e2e.yml` - Docker compose for E2E testing

### Modified Files
- `CMakeLists.txt` - Version bump to 0.4.0
- `android/app/build.gradle.kts` - Version bump to 0.4.0 (versionCode 10)
- `src/logger.hpp` - Thread-safe logging
- `src/logger.cpp` - Thread-safe init/shutdown
- `src/cli_client.cpp` - Updated state display
- `DESIGN.md` - Updated documentation

---

## Breaking Changes

**None** - Fully backward compatible.

---

## Upgrade Notes

### Desktop

1. Stop the daemon: `levin stop`
2. Rebuild: `cmake -B build && cmake --build build`
3. Start the daemon: `levin start`
4. Verify: `levin status` should show "No torrents" when idle

### Android

1. Install new APK over existing installation
2. Settings and data are preserved
3. Notification will now show "No torrents" instead of "Idle"

---

## Running E2E Tests

### Desktop (Docker)
```bash
docker build -f Dockerfile.e2e-desktop -t levin-e2e-desktop .
docker run --rm levin-e2e-desktop
```

### Android (requires emulator)
```bash
# Start emulator
$ANDROID_SDK_ROOT/emulator/emulator -avd levin_test -no-snapshot-load &
adb wait-for-device

# Run tests
cd e2e/android
./run_tests.sh
```

---

## Release Date

December 29, 2024
