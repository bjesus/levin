# Release Notes - Levin v0.4.1

**Release Date:** December 30, 2024

## 🎉 Major Feature: WebTorrent Support!

Levin can now seed torrents to **web browsers** using WebTorrent! This allows Levin to act as a hybrid peer, connecting to both:
- Traditional BitTorrent clients (uTP/TCP)  
- Web browsers via WebRTC Data Channels

### Platform Support
- **Desktop (Linux/macOS):** ✅ Enabled (builds libtorrent master with WebTorrent)
- **Android:** ✅ Enabled (libtorrent4j v2.1.0+ includes WebTorrent)

### How It Works
WebTorrent uses WebRTC for peer-to-peer connections in browsers. Levin connects to WebSocket trackers (`wss://`) and exchanges data via WebRTC Data Channels, making torrents accessible directly in web browsers without plugins.

See **[WEBTORRENT.md](WEBTORRENT.md)** for the complete guide.

---

## ✨ New Features

### WebTorrent Integration
- **WebSocket tracker support** (`wss://` URLs)
- **WebRTC peer connections** via Data Channels  
- **STUN server configuration** (default: `stun.l.google.com:19302`)
- **Hybrid peer mode** - seeds to both traditional clients and browsers
- **Automatic NAT traversal** via STUN/ICE

### Helper Scripts
- `scripts/check_torrent.sh` - Check if a torrent has WebSocket trackers
- `scripts/add_websocket_tracker.sh` - Add WebSocket trackers to existing torrents

### Configuration

**New WebTorrent section:**
```toml
[webtorrent]
stun_server = "stun.l.google.com:19302"  # Default, no config needed
```

**Simplified config format:**
- Removed deprecated sections: `[network]`, `[torrents]`, `[cli]`, `[statistics]`
- All network features now hardcoded as enabled (DHT, LSD, UPnP, NAT-PMP)
- Consolidated paths: use `state_directory` instead of individual files
- Unlimited torrent limits by default

**Old config format is NO LONGER supported** - update your configs!

---

## 🔧 Technical Changes

### Build System
- **Desktop:** Now builds libtorrent from source (master branch) with WebTorrent enabled
- Bundles libdatachannel for WebRTC support
- CMake option: `-DUSE_SYSTEM_LIBTORRENT=ON` to use system libtorrent (no WebTorrent)

### API Updates (libtorrent master compatibility)
- Use `piece_index_t` strong typedef for piece indices
- Use `load_torrent_file()` instead of `torrent_info(string)` constructor
- Use `session_state()` and `write_session_params()` instead of `save_state()`

### Config Simplification
**Before (v0.4.0):**
```toml
[daemon]
pid_file = "/var/run/levin.pid"
log_file = "/var/log/levin.log"

[network]
listen_port = 6881
enable_dht = true
enable_lsd = true
# ... many options
```

**Now (v0.4.1):**
```toml
[paths]
watch_directory = "~/.config/levin/torrents"
data_directory = "~/.cache/levin/data"
state_directory = "~/.local/state/levin"  # Optional, has default

[disk]
min_free = "1gb"

[daemon]
log_level = "info"  # Optional
run_on_battery = false  # Optional
```

All network/torrent settings are now hardcoded with sensible defaults.

---

## 📦 Installation

### Desktop (Linux/macOS)

**From Source:**
```bash
git clone https://github.com/bjesus/levin.git
cd levin
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
sudo cmake --install build
```

**Dependencies:**
- libtorrent master (bundled automatically)
- libdatachannel (bundled automatically)
- Boost >= 1.70
- OpenSSL
- CMake >= 3.15

### Android

Download the APK from the [Releases page](https://github.com/bjesus/levin/releases/tag/v0.4.1).

---

## 🧪 Testing

### Unit Tests
```bash
./build/tests/levin_tests
```
**Result:** ✅ 67 assertions, all passing

### E2E Tests
```bash
cd e2e/desktop && ./run_tests.sh
```

---

## 📚 Documentation

- **[WEBTORRENT.md](WEBTORRENT.md)** - Complete WebTorrent guide
  - Requirements & setup
  - Checking/adding WebSocket trackers
  - STUN server configuration
  - Testing with browsers
  - Troubleshooting

- **[config/levin.toml.example](config/levin.toml.example)** - Updated config template
- **[DESIGN.md](DESIGN.md)** - Architecture documentation

---

## 🐛 Bug Fixes

- Fixed config validation for new simplified format
- Updated E2E test configs to new format
- Fixed API compatibility with libtorrent master

---

## ⚠️ Breaking Changes

### Configuration Format Changed
The config format has been **significantly simplified**. Old configs from v0.4.0 and earlier **will NOT work**.

**Migration steps:**
1. Backup your old config
2. Use the new template: `config/levin.toml.example`
3. Only set required fields: `paths.*` and `disk.min_free`

**What was removed:**
- `[daemon].pid_file`, `log_file` → derived from `state_directory`
- `[network].*` → all hardcoded as enabled
- `[torrents].*` → sensible defaults, can't be changed
- `[cli].control_socket` → derived from `state_directory`
- `[statistics].save_interval_minutes` → hardcoded

**What stays:**
- `[paths].*` - watch/data directories (required)
- `[disk].*` - storage limits (min_free required)
- `[daemon].log_level`, `run_on_battery` (optional)
- `[limits].*` - bandwidth limits (optional)

---

## 🔗 Links

- **Repository:** https://github.com/bjesus/levin
- **Issues:** https://github.com/bjesus/levin/issues
- **WebTorrent Guide:** [WEBTORRENT.md](WEBTORRENT.md)
- **Anna's Archive:** https://annas-archive.org

---

## 🙏 Acknowledgments

- **WebTorrent** by @feross and contributors
- **libtorrent** WebTorrent support by @paullouisageneau ([PR #4123](https://github.com/arvidn/libtorrent/pull/4123))
- **libdatachannel** by @paullouisageneau

---

## 📝 Changelog

**Full Changelog:** https://github.com/bjesus/levin/compare/v0.4.0...v0.4.1

**Files Changed:**
- 19 files changed, 936 insertions(+), 583 deletions(-)
- New: WEBTORRENT.md, scripts/check_torrent.sh, scripts/add_websocket_tracker.sh
- Modified: CMakeLists.txt, config system, libtorrent API usage

---

**What's Next?**
- Try WebTorrent: Add WebSocket trackers to your torrents and seed to browsers!
- Report issues: https://github.com/bjesus/levin/issues
- Contribute: PRs welcome!
