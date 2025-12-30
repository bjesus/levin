# Levin v0.4.2 Release Notes

## Bug Fixes

### CI/Build Fixes
- **Fixed CMake install rules**: The install rules for the `levin` binary were not being generated when building libtorrent from source, causing package installation tests to fail
- **Fixed debian package build**: Added `USE_SYSTEM_LIBTORRENT=ON` to debian packaging rules so packages correctly use the system libtorrent library
- **Fixed systemd service install path**: Changed from absolute `/usr/lib/systemd/user` to relative `lib/systemd/user` for proper prefix-based installation

### WebTorrent Improvements
- **Automatic WebSocket tracker injection**: WebSocket trackers are now automatically added to all torrents on both desktop and Android platforms, ensuring WebTorrent peers can discover and connect without manual configuration

## Technical Details

### WebSocket Trackers Added Automatically
The following trackers are injected with tier 0 (highest priority):
- `wss://tracker.openwebtorrent.com`
- `wss://tracker.webtorrent.dev`
- `wss://tracker.btorrent.xyz`

### Build System Changes
- Libtorrent's install/export rules are now properly disabled when building from source to prevent CMake configuration errors
- This fixes the "datachannel-static not in export set" error that was causing CI failures

## Upgrade Notes

This is a bug-fix release with no breaking changes. Simply update to benefit from the fixes.

### Desktop
```bash
# Build from source
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

### Android
Update to the new APK version (0.4.2, versionCode 12).
