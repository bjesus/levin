# Levin v0.4.3 Release Notes

## WebTorrent Support for All Platforms

This release ensures WebTorrent support is available on **all platforms**, including Linux distribution packages.

### Changes

- **Bundled libtorrent**: All builds now compile libtorrent from source (master branch) with WebTorrent enabled
- **Automatic WebSocket tracker injection**: WebSocket trackers are automatically added to all torrents for WebTorrent peer discovery
- **Removed system libtorrent dependency**: No longer relies on distribution packages which lack WebTorrent support

### WebTorrent Trackers

The following trackers are automatically injected (tier 0, highest priority):
- `wss://tracker.openwebtorrent.com`
- `wss://tracker.webtorrent.dev`
- `wss://tracker.btorrent.xyz`

### Build Requirements

Since libtorrent is now built from source, the following are required:
- CMake >= 3.15
- C++17 compiler (GCC 9+ or Clang 10+)
- Boost >= 1.70
- OpenSSL
- libcurl
- GLib 2.0 (for power monitoring on Linux)

### Note on Build Time

Building from source takes longer (~2-5 minutes) as it compiles libtorrent and its WebRTC dependencies. This is a one-time cost for the benefit of WebTorrent support.

## Upgrade Notes

Simply update to the new version. No configuration changes required.
