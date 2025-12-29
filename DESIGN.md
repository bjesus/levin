# Levin Design Document

This document describes the core logic and algorithms that both Desktop (C++) and Android (Kotlin) implementations must follow. When making changes to either platform, ensure consistency with this specification.

---

## Table of Contents

1. [State Machine](#state-machine)
2. [Storage Management](#storage-management)
3. [File Deletion Algorithm](#file-deletion-algorithm)
4. [State Display](#state-display)
5. [Platform-Specific Behaviors](#platform-specific-behaviors)

---

## State Machine

### States

Levin operates in exactly 5 states:

| State         | Numeric Value | Description                                            |
| ------------- | ------------- | ------------------------------------------------------ |
| `OFF`         | 0             | User disabled - no activity                            |
| `PAUSED`      | 1             | Conditions not met (battery/network) - session paused  |
| `IDLE`        | 2             | No torrents loaded - session running but inactive      |
| `SEEDING`     | 3             | Storage limit reached - uploads only, downloads paused |
| `DOWNLOADING` | 4             | Normal operation - full network activity               |

### Conditions

The state machine tracks these boolean conditions:

- **`user_enabled`**: User's master on/off toggle
- **`battery_allows`**: True if on AC power OR `runOnBattery` setting enabled
- **`network_allows`**: True if on WiFi OR `runOnCellular` setting enabled (Desktop: always true)
- **`has_torrents`**: True if any torrents are loaded
- **`storage_allows`**: True if NOT over budget (budget > 0 after hysteresis)

### State Determination Logic

States are determined by priority (checked in order):

```
IF !user_enabled THEN
    RETURN OFF

IF !battery_allows OR !network_allows THEN
    RETURN PAUSED

IF !has_torrents THEN
    RETURN IDLE

IF !storage_allows THEN
    RETURN SEEDING

RETURN DOWNLOADING
```

### State Transitions

States change only when conditions change. Each condition has an update method:

- `set_enabled(bool)`
- `update_battery_condition(bool)`
- `update_network_condition(bool)`
- `update_has_torrents(bool)`
- `update_storage_condition(bool)`

When any condition changes, the state is recomputed and a callback is triggered if the state changed.

### State-Specific Behaviors

| State       | Session | Foreground Service | Notification | Downloads | Uploads |
| ----------- | ------- | ------------------ | ------------ | --------- | ------- |
| OFF         | Stopped | Stopped            | Hidden       | No        | No      |
| PAUSED      | Paused  | Stopped            | Hidden       | No        | No      |
| IDLE        | Running | Running            | Visible      | No        | No      |
| SEEDING     | Running | Running            | Visible      | Paused    | Yes     |
| DOWNLOADING | Running | Running            | Visible      | Yes       | Yes     |

**Implementation Notes:**

- PAUSED: Desktop/Android completely pause the libtorrent session
- SEEDING: Set all pieces we don't have to IGNORE/dont_download priority (no downloads, uploads continue)
- Session state and notification state must update together atomically

---

## Storage Management

### Budget Calculation

Storage monitoring checks every:

- **Desktop:** 60 seconds
- **Android:** 1 second (lightweight check)

#### Algorithm

```
1. Get filesystem stats:
   - total_bytes = total filesystem size
   - free_bytes = available bytes for user

2. Calculate minimum required free space:
   - Desktop: min_required = max(min_free_bytes, total_bytes * min_free_percentage)
   - Android: min_required = min_free_bytes (no percentage support)

3. Calculate available space:
   - available_space = max(0, free_bytes - min_required)

4. If max_storage is set (optional cap):
   - current_usage = calculate_actual_disk_usage()
   - available_for_levin = max(0, max_storage - current_usage)
   - budget = min(available_space, available_for_levin)
   ELSE:
   - budget = available_space

5. Apply 50MB hysteresis to prevent thrashing:
   - IF budget > 50MB THEN budget -= 50MB
   - ELSE IF budget > 0 THEN budget = 0

6. Determine over_budget:
   - over_budget = (budget <= 0) OR
                   (max_storage set AND current_usage >= max_storage) OR
                   (free_bytes < min_required)
```

### Disk Usage Calculation

**Critical:** Must calculate actual disk usage, not logical file size (handles sparse files correctly).

#### Both Platforms

Both Desktop and Android use the `du -s` shell command for consistency:

```
command: du -s <data_directory>
output:  12345\t/path  (where 12345 is in KB)
usage:   sizeInKB * 1024 = bytes
```

**Desktop (C++):**
```cpp
FILE* pipe = popen("du -s " + data_directory, "r");
// Parse output, extract KB, multiply by 1024
```

**Android (Kotlin):**
```kotlin
// Fallback if StorageStatsManager unavailable
val process = Runtime.exec(["du", "-s", data_directory])
val output = process.inputStream.readText()
val sizeInKB = output.trim().split(regex)[0].toLong()
return sizeInKB * 1024
```

**Why `du -s`:**
- Handles sparse files correctly (counts actual blocks, not logical size)
- Consistent behavior across platforms
- Simpler than platform-specific stat() implementations

**Note:** Android's FUSE filesystem has delayed space accounting (10-30 seconds after deletion).

### Over Budget Handling

When `over_budget == true`:

1. **Immediately** set `storage_allows = false`
2. State machine transitions to SEEDING (if has torrents)
3. Trigger file deletion to free space
4. Repeat check after 1-60 seconds (platform dependent)

---

## File Deletion Algorithm

### When to Delete

Delete files when:

```
over_budget == true AND deficit_bytes > 0
```

Where `deficit_bytes` is calculated as:

```
IF max_storage set AND current_usage > max_storage THEN
    deficit = current_usage - max_storage
ELSE IF free_bytes < min_required THEN
    deficit = min_required - free_bytes
ELSE
    deficit = 0
```

### Deletion Strategy

**Goal:** Delete exactly enough files to meet requirements, no more.

**Algorithm:**

```
1. Scan data_directory recursively for all files
   - Collect: file_path, file_size

2. Shuffle files to random order
   - No assumption that older files are better seeded

3. freed = 0
   deleted_count = 0

4. FOR EACH file IN shuffled_files:
       IF freed >= deficit_bytes THEN BREAK

       DELETE file
       freed += file_size
       deleted_count += 1

5. LOG: "Deleted {deleted_count} files, freed {freed} bytes (target: {deficit_bytes})"
```

**Important Rules:**

- Delete **individual files**, not entire torrent directories
- Delete in **random order** (no assumption about seeding quality)
- Stop **as soon as** enough space is freed
- Do **not** interact with libtorrent session (files may be in use)

### Android-Specific: Deletion Tracking

Android FUSE has delayed space accounting (10-30 seconds). To prevent over-deletion:

```
last_deletion_time = 0
recently_deleted_bytes = 0

BEFORE deleting:
    IF (now - last_deletion_time) < 30 seconds THEN
        adjusted_target = deficit_bytes - recently_deleted_bytes
        IF adjusted_target <= 0 THEN
            LOG "Waiting for filesystem to update"
            RETURN  // Don't delete more yet

AFTER deleting:
    last_deletion_time = now
    recently_deleted_bytes += freed

IF (now - last_deletion_time) >= 30 seconds THEN
    recently_deleted_bytes = 0  // Reset tracking
```

**Desktop does not need this** - Linux ext4/btrfs/xfs update space immediately.

---

## State Display

### Status Text

Both platforms display the same human-readable state text:

| State            | Display Text              |
| ---------------- | ------------------------- |
| OFF              | "Off"                     |
| PAUSED (battery) | "Paused (battery)"        |
| PAUSED (network) | "Paused (network)"        |
| IDLE             | "Idle (no torrents)"      |
| SEEDING          | "Seeding (storage limit)" |
| DOWNLOADING      | "Downloading"             |

**Implementation:**

- **Desktop:** CLI status command shows this on first line
- **Android:** Notification title shows "Levin - {state_text}"

### Notification Details

Additional information shown alongside state:

**IDLE:**

```
Text: "No torrents"
```

**SEEDING:**

```
Text: "⬆ {upload_rate} (storage limit reached)"
```

**DOWNLOADING:**

```
Text: "⬇ {download_rate}  ⬆ {upload_rate}  {torrent_count} active"
```

Format rates using:

- < 1 KB/s: "{bytes} B/s"
- < 1 MB/s: "{kilobytes} KB/s"
- > = 1 MB/s: "{megabytes} MB/s"

---

## Platform-Specific Behaviors

### Desktop Only

**Power Monitoring:**

- Uses DBus/UPower event-driven monitoring (no polling)
- Subscribes to PropertiesChanged signals on `/org/freedesktop/UPower/devices/DisplayDevice`
- Immediate notification when power state changes
- If `runOnBattery == false` and unplugged → PAUSED state

**Daemon:**

- Runs as systemd service
- Socket-based CLI for status/control
- Logs to `~/.cache/levin/levin.log`

**Rebalancing:**

- Runs every 60 seconds
- Checks storage and calls delete_pieces if needed

### Android Only

**Power Monitoring:**

- Uses `BroadcastReceiver` for `ACTION_POWER_CONNECTED/DISCONNECTED`
- Immediate notification of power changes

**Network Monitoring:**

- Uses `ConnectivityManager.NetworkCallback`
- Distinguishes WiFi vs Cellular

**Service:**

- Foreground service with notification (required for background operation)
- Notification is mandatory when running (OFF/PAUSED hide it)

**Monitoring:**

- Checks every 1 second (storage, torrent count)
- Updates notification every 1 second

**Torrent Watching:**

- Scans watch directory every 30 seconds
- Uses `File.listFiles()` and tracks by filename

### Shared Behavior

**Both platforms:**

- Use 50MB hysteresis on budget to prevent download-delete thrashing
- Delete files (not torrents) when over budget
- Show unified state text
- Maintain session statistics (lifetime + session)
- Auto-load torrents from watch directory

---

## Implementation Checklist

When implementing a feature on one platform, verify the other platform:

- [ ] State machine logic matches (5 states, priority order)
- [ ] Storage budget calculation is identical (including hysteresis)
- [ ] File deletion uses same algorithm
- [ ] State display text is consistent
- [ ] Over budget triggers same behavior (SEEDING state, delete files)
- [ ] Tests verify behavior matches specification

---

## Testing Guidelines

### State Machine Tests

For each platform, verify:

1. User disable → OFF (regardless of other conditions)
2. Battery/network restriction → PAUSED
3. No torrents → IDLE
4. Over budget → SEEDING
5. All conditions met → DOWNLOADING

### Storage Tests

1. Budget = 0 → storage_allows = false → SEEDING state
2. Budget < 50MB → storage_allows = false (hysteresis)
3. Budget >= 50MB → storage_allows = true
4. Over max_storage → over_budget = true
5. Below min_free → over_budget = true

### Deletion Tests

1. Delete exactly enough files (not entire torrents)
2. Stop as soon as target reached
3. Verify disk space actually freed (may be delayed on Android)

---

## References

- State Machine: `src/state_machine.cpp`, `android/.../state/LevinStateManager.kt`
- Storage: `src/disk_monitor.cpp`, `android/.../monitoring/StorageMonitor.kt`
- Deletion: `src/piece_manager.cpp::delete_pieces()`, `android/.../service/LevinService.kt::freeUpSpace()`
- Display: `src/cli_client.cpp`, `android/.../state/LevinState.kt::notificationTitle()`
