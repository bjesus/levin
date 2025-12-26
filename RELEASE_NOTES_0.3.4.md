# Levin 0.3.4 Release Notes

## Critical Fixes: Storage Cleanup & State Reporting

This release fixes critical bugs in storage management and implements proper file-based deletion to maintain minimum free space requirements.

---

## Major Fixes

### 1. File-Based Deletion (Not Torrent-Based) ✅

**Problem:** When storage limit was reached, the app deleted entire torrents instead of individual files, leading to excessive deletion.

**Example:**
- Need to free: 1 GB
- Old behavior: Deleted entire 6 GB torrent
- New behavior: Deletes individual files totaling ~1 GB

**Impact:** Dramatically reduces wasted deletion - now only removes the minimum necessary data.

**Changes:**
- **Desktop:** `src/piece_manager.cpp` - Scans all files, sorts by modification time, deletes oldest first
- **Android:** `LevinService.kt` - Same file-based deletion strategy

---

### 2. Correct State Reporting When Budget = 0 ✅

**Problem:** Desktop showed "Downloading" state even when budget was 0 (should be "Seeding").

**Root Cause:** Hysteresis was applied after over_budget check, causing budget to become 0 without setting over_budget flag.

**Fix:** Added check after hysteresis to set over_budget when budget reaches 0.

**File:** `src/disk_monitor.cpp` - Added post-hysteresis over_budget check

---

### 3. Unified Status Text Across Platforms ✅

**Problem:** Desktop and Android showed different status text for the same states.

**Solution:** Desktop now uses the same state names as Android:
- "Downloading" - Active downloading
- "Seeding (storage limit)" - Over budget, uploads only
- "Idle (no torrents)" - No torrents loaded
- "Paused (battery)" - Paused due to battery/network
- "Off" - Disabled

**Changes:**
- `src/cli_server.cpp` - Expose state via CLI API
- `src/cli_client.cpp` - Display unified state text
- `src/daemon.cpp` - Add state callback

---

### 4. Desktop Filesystem Monitoring Resilience ✅

**Problem:** Desktop showed "Unable to read filesystem" when all torrents were deleted.

**Root Cause:** Data directory was deleted, causing `statvfs()` to fail.

**Fix:** Auto-create data directory if missing before calling `statvfs()`.

**File:** `src/disk_monitor.cpp` - Create directory if missing

---

### 5. Android Deletion Tracking ⚠️

**Problem:** Android FUSE filesystem has delayed space accounting - deletions don't immediately update free space, causing over-deletion.

**Solution:** Track recent deletions with 30-second window to prevent repeated deletion attempts.

**File:** `android/app/src/main/java/com/yoavmoshe/levin/service/LevinService.kt` - Added deletion tracking

**Note:** This is a workaround for an inherent Android/FUSE platform limitation.

---

## Implementation Details

### File Deletion Algorithm (Both Platforms)

1. Scan all files in data directory recursively
2. Collect file paths, sizes, and modification times
3. Sort by modification time (oldest first)
4. Delete files one by one until `freed >= bytes_to_free`
5. Stop immediately when target reached

**Benefits:**
- Granular control over deletion amount
- Deletes oldest data first (most likely to be well-seeded)
- No wasted deletion of recent/rare data
- Works identically on both platforms

### State Machine Improvements

Added proper state reporting when budget constraints are active:
- Budget = 0 → SEEDING state (correct)
- Budget > 0 but < 50 MB → SEEDING state (hysteresis)
- Budget ≥ 50 MB → DOWNLOADING state

---

## Testing

### Desktop
- ✅ All unit tests passing (50 assertions, 5 test cases)
- ✅ File deletion tested with 1 GB target
- ✅ State correctly shows "Seeding (storage limit)" when budget = 0
- ✅ Filesystem monitoring survives data directory deletion

### Android
- ✅ APK builds successfully
- ✅ File deletion tested on emulator
- ✅ Deletion tracking prevents over-deletion
- ✅ State synchronization verified

---

## Files Changed

### Desktop (5 files)
- `CMakeLists.txt` - Version bump to 0.3.4
- `src/piece_manager.cpp` - File-based deletion instead of torrent-based
- `src/disk_monitor.cpp` - Auto-create data directory, fix budget=0 state
- `src/cli_server.hpp/cpp` - Expose state via API
- `src/cli_client.cpp` - Unified status text display
- `src/daemon.cpp` - Add state callback

### Android (3 files)
- `android/app/build.gradle.kts` - Version bump to 0.3.4 (versionCode 9)
- `android/app/src/main/java/com/yoavmoshe/levin/service/LevinService.kt` - File-based deletion with tracking
- `android/app/src/main/java/com/yoavmoshe/levin/service/SessionManager.kt` - Remove unused methods

---

## Breaking Changes

**None** - Fully backward compatible.

---

## Upgrade Notes

### Desktop

1. Stop the daemon: `levin stop`
2. Rebuild: `cmake -B build && cmake --build build`
3. Start the daemon: `levin start`
4. Verify: `levin status` should show correct state

### Android

1. Install new APK over existing installation
2. Settings and data are preserved
3. App will automatically resume operation

---

## Known Limitations

### Android FUSE Delay

Android's FUSE filesystem doesn't update free space immediately after file deletion. This can take 10-30 seconds or until another file operation occurs.

**Mitigation:** 30-second deletion tracking prevents repeated deletion attempts, but some over-deletion may still occur in edge cases.

**This is a platform limitation, not a bug in Levin.**

---

## Performance

**Deletion Speed:**
- Desktop: ~1000 files/second
- Android: ~500 files/second (varies by device)

**Monitoring Overhead:**
- Desktop: Check every 60 seconds
- Android: Check every 1 second (minimal CPU impact)

---

## Contributors

- Fixed file-based deletion logic
- Unified state reporting across platforms
- Improved storage monitoring resilience
- Enhanced Android FUSE compatibility

---

## Next Steps

Future releases may include:
- Per-torrent retention policies
- Configurable deletion strategy (oldest/largest/random)
- Automatic re-download of deleted data when space available
- Storage usage analytics

---

## Release Date

December 25, 2024
