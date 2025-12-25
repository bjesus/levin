# Levin 0.2.0 Release Notes

## Major Changes: Unified Storage Management

This release introduces a unified storage management system across both desktop and Android platforms, providing more flexible and consistent control over disk usage.

### New Storage Configuration Model

Both platforms now support **two independent storage constraints**:

1. **Minimum Free Space** (REQUIRED) - Ensures the disk never drops below a specified threshold
2. **Maximum Storage** (OPTIONAL) - Caps how much space Levin can use

Both constraints are respected simultaneously, and Levin will stop downloading when either limit is reached.

---

## Desktop Changes

### Configuration File Updates

**New simplified key names:**
- `min_free` - Minimum free space (replaces `min_free_bytes`)
- `max_storage` - Maximum storage Levin can use (NEW, optional)

**Backward compatibility:**
- Old `min_free_bytes` key still supported as an alias
- Existing configs will continue to work

**New human-readable format support:**
```toml
[disk]
# REQUIRED: Minimum free space (supports human-readable format)
min_free = "10gb"              # or 10737418240 (bytes)
min_free_percentage = 0.10

# OPTIONAL: Maximum storage (0 or omit for unlimited)
max_storage = "100gb"          # NEW: caps Levin data at 100GB

check_interval_seconds = 60
```

**Example use cases:**
- Set `max_storage = "100gb"` to cap Levin's archive at 100GB
- Omit `max_storage` for unlimited storage (respecting only `min_free`)
- Use both constraints together for fine-grained control

### Implementation Changes

- Updated `Config` struct with new field names
- Enhanced `DiskMonitor` to respect both constraints
- Improved logging to show both constraints
- All tests updated and passing

---

## Android Changes

### Settings UI Improvements

**Two new settings replace the old "Storage Limit":**

1. **Minimum Free Space (Required)** ⚠️
   - Always keep at least this much space free on device
   - Default: 1 GB
   - Supports decimal values (e.g., 1.5 GB)
   - Cannot be empty

2. **Maximum Storage (Optional)** ℹ️
   - Limit Levin data to this size
   - Default: Unlimited
   - Leave empty for unlimited storage
   - Supports decimal values (e.g., 50.5 GB)

### Enhanced Validation

Both settings now include comprehensive validation:
- ✅ Must be valid numbers (decimal supported)
- ✅ Must be positive
- ✅ Cannot exceed available/total disk space
- ✅ Clear error messages with disk space info
- ✅ Success toasts when settings are updated

### Implementation Changes

- Updated `LevinSettings` data class
  - `minFree: Long` (REQUIRED, replaces `minFreeSpaceBytes`)
  - `maxStorage: Long?` (OPTIONAL, replaces `allowedStorageBytes`)
- Simplified SharedPreferences keys: `min_free`, `max_storage`
- Enhanced `StorageMonitor` with unified constraint logic
- Improved status logging showing both constraints

---

## Breaking Changes

### Desktop
- **None** - Fully backward compatible
- Old `min_free_bytes` key still works
- No migration required

### Android
- **Field names changed** - Old installs will use new defaults
- `allowedStorageBytes` → `maxStorage` (now optional, defaults to unlimited)
- `minFreeSpaceBytes` → `minFree` (now properly exposed in UI)
- No migration logic - treated as fresh install with safe defaults

---

## Upgrade Notes

### Desktop

1. **Recommended:** Update your config file to use new keys:
   ```toml
   [disk]
   min_free = "10gb"           # Cleaner than min_free_bytes = 10737418240
   min_free_percentage = 0.10
   max_storage = "100gb"       # NEW: Optional cap on Levin storage
   ```

2. **Optional:** Keep using old `min_free_bytes` - still works

### Android

1. Install new version
2. Review new settings in Settings → Storage Limits
3. Set "Minimum Free Space" (defaults to 1GB)
4. Optionally set "Maximum Storage" (defaults to unlimited)

---

## Technical Details

### Budget Calculation Logic

```
effective_min_free = max(min_free, total_disk * min_free_percentage)  # Desktop only
available_space = free_bytes - effective_min_free

if max_storage is set:
    available_for_levin = max_storage - current_usage
    budget = min(available_space, available_for_levin)
else:
    budget = available_space

over_budget = (budget <= 0) OR (current_usage > max_storage if set)
```

### Key Benefits

1. **Flexibility**: Choose between capping Levin's usage OR ensuring minimum free space OR both
2. **Consistency**: Same model on desktop and Android
3. **Safety**: Always respect disk space constraints
4. **Clarity**: Simplified key names, human-readable formats

---

## Testing

### Desktop
- ✅ All unit tests passing
- ✅ Config parsing with new keys
- ✅ Config parsing with old keys (backward compatibility)
- ✅ Budget calculation with both constraints
- ✅ Budget calculation with unlimited max_storage

### Android
- ✅ APK builds successfully
- ✅ Both settings functional in UI
- ✅ Validation working correctly
- ✅ Budget calculation respecting both constraints

---

## Files Changed

### Desktop
- `src/config.hpp` - Updated Config struct
- `src/config.cpp` - Enhanced parsing with backward compatibility
- `src/disk_monitor.cpp` - Unified constraint logic
- `config/levin.toml.example` - Updated example with documentation
- `tests/test_config.cpp` - Updated test configs
- `tests/test_disk_monitor.cpp` - Updated test configs

### Android
- `android/app/src/main/java/com/yoavmoshe/levin/data/Settings.kt`
- `android/app/src/main/java/com/yoavmoshe/levin/data/SettingsRepository.kt`
- `android/app/src/main/java/com/yoavmoshe/levin/monitoring/StorageMonitor.kt`
- `android/app/src/main/java/com/yoavmoshe/levin/ui/SettingsFragment.kt`
- `android/app/src/main/java/com/yoavmoshe/levin/service/LevinService.kt`
- `android/app/src/main/res/values/strings.xml`
- `android/app/src/main/res/xml/preferences.xml`
- `android/app/build.gradle.kts` - Version bump
- `CMakeLists.txt` - Version bump

---

## Contributors

- Implemented unified storage model across both platforms
- Enhanced validation and user feedback
- Maintained backward compatibility on desktop
- Comprehensive testing on both platforms

---

## Next Steps

Future releases may include:
- Automatic cleanup when over budget
- Storage usage statistics in UI
- Per-torrent storage limits
- Percentage-based min_free on Android
