# Levin - BitTorrent Archive Daemon

A high-performance C++ BitTorrent client designed for archival purposes. Levin intelligently manages disk space by downloading and seeding pieces from torrents with the fewest seeders, ensuring rare data is preserved while respecting strict disk space limits.

## Features

- **Piece-Level Management**: Operates at BitTorrent piece granularity, allowing partial torrent seeding
- **Smart Prioritization**: Downloads rare pieces from under-seeded torrents first
- **Strict Disk Management**: Never exceeds configured disk space limits (absolute + percentage)
- **WebRTC Support**: Compatible with browser-based BitTorrent clients
- **Low Resource Usage**: Optimized daemon for 24/7 operation
- **CLI Interface**: Monitor and control the daemon via command-line interface
- **Statistics Tracking**: Session and lifetime upload/download statistics

## Architecture

Levin prioritizes data based on:
1. **Torrent Priority**: `1 / (seeders + 1)` - fewer seeders = higher priority
2. **Piece Rarity**: Within each torrent, rarer pieces are prioritized
3. **Combined Score**: `priority = torrent_priority × piece_rarity`

When disk space is limited, the daemon removes the most common pieces from well-seeded torrents first, ensuring rare data is preserved.

## Requirements

### System Requirements
- Linux (tested on Ubuntu 24.04) or macOS
- C++17 compiler (GCC 7+, Clang 5+)
- CMake 3.15 or later
- 100MB+ disk space for daemon binaries

### Dependencies
- **libtorrent-rasterbar** (>= 2.0.0 for WebRTC support)
- **Boost** (system, filesystem)
- **OpenSSL** (for libtorrent)
- **toml11** (fetched automatically by CMake)
- **spdlog** (fetched automatically by CMake)
- **nlohmann/json** (fetched automatically by CMake)

## Installation

Levin uses user-level installation - no root required after package installation!

### Debian/Ubuntu

```bash
# Download the .deb for your Ubuntu version from releases
wget https://github.com/bjesus/levin/releases/download/v0.1.0/levin_0.1.0_ubuntu-22.04_amd64.deb

# Install the package
sudo dpkg -i levin_0.1.0_ubuntu-22.04_amd64.deb
sudo apt-get install -f  # Install dependencies if needed

# Start the daemon (creates default config automatically)
levin start

# Or start in foreground to see output
levin start --foreground

# Check status
levin status

# Enable automatic start on login
systemctl --user enable levin
```

### Fedora/RHEL

```bash
# Download the .rpm for your Fedora version from releases
wget https://github.com/bjesus/levin/releases/download/v0.1.0/levin-0.1.0-fedora40.x86_64.rpm

# Install the package
sudo dnf install levin-0.1.0-fedora40.x86_64.rpm

# Start the daemon (creates default config automatically)
levin start

# Or start in foreground to see output
levin start --foreground

# Check status
levin status

# Enable automatic start on login
systemctl --user enable levin
```

### Arch Linux (AUR)

```bash
# Install from AUR
yay -S levin
# or
paru -S levin

# Start the daemon (creates default config automatically)
levin start

# Or start in foreground to see output
levin start --foreground

# Check status
levin status

# Enable automatic start on login
systemctl --user enable levin
```

### macOS (Homebrew)

```bash
# Add the tap
brew tap bjesus/levin

# Install levin
brew install levin

# First run creates default config
levin

# Edit configuration
nano ~/.config/levin/levin.toml

# Start the service
brew services start levin
```

### Build from Source

```bash
# Clone the repository
git clone https://github.com/bjesus/levin.git
cd levin

# Install dependencies (Ubuntu/Debian)
sudo apt-get install -y cmake g++ libboost-system-dev \
  libboost-filesystem-dev libssl-dev libtorrent-rasterbar-dev

# Or on macOS
brew install cmake boost openssl@3 libtorrent-rasterbar

# Build
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF ..
make -j$(nproc)

# Install binaries and service file
sudo make install

# Start the daemon (creates default config automatically)
levin start

# Or start in foreground to see output
levin start --foreground

# Check status
levin status

# Enable automatic start on login
systemctl --user enable levin
```

## Configuration

On first run, Levin creates a default configuration at `~/.config/levin/levin.toml`:

```toml
[daemon]
# User-level directories using XDG Base Directory specification
pid_file = "$XDG_STATE_HOME/levin/levin.pid"       # ~/.local/state/levin/levin.pid
log_file = "$XDG_STATE_HOME/levin/levin.log"       # ~/.local/state/levin/levin.log
log_level = "info"

[paths]
watch_directory = "$XDG_CONFIG_HOME/levin/torrents"  # ~/.config/levin/torrents
data_directory = "$XDG_CACHE_HOME/levin/data"        # ~/.cache/levin/data
session_state = "$XDG_STATE_HOME/levin/session.state"  # ~/.local/state/levin/session.state
statistics_file = "$XDG_STATE_HOME/levin/statistics.json"  # ~/.local/state/levin/statistics.json

[disk]
min_free_space = "1gb"           # Human-readable: "100mb", "5gb", "1tb" or bytes
min_free_percentage = 0.05       # 5%
check_interval_seconds = 60

[torrents]
seeder_update_interval_minutes = 60
watch_directory_scan_interval_seconds = 30
max_connections_per_torrent = 50
max_upload_slots_per_torrent = 8

[limits]
max_download_rate_kbps = 0       # 0 = unlimited
max_upload_rate_kbps = 0         # 0 = unlimited
max_total_connections = 200
max_active_downloads = 4
max_active_seeds = -1            # -1 = unlimited
max_active_torrents = 8

[network]
listen_port = 6881
enable_dht = true
enable_lsd = true
enable_upnp = true
enable_natpmp = true
enable_webrtc = false
webrtc_stun_server = "stun:stun.l.google.com:19302"

[cli]
control_socket = "$XDG_STATE_HOME/levin/levin.sock"  # ~/.local/state/levin/levin.sock

[statistics]
save_interval_minutes = 5
```

The config file supports:
- Environment variables: `$VAR` or `${VAR}`
- Tilde expansion: `~/data`
- XDG defaults if variables not set

## Usage

### Service Management

```bash
# Check service status
systemctl --user status levin

# View logs
journalctl --user -u levin -f

# Restart service
systemctl --user restart levin

# Stop service
systemctl --user stop levin

# Disable automatic start
systemctl --user disable levin
```

On macOS with Homebrew:
```bash
# Start service
brew services start levin

# Check status
brew services list

# Stop service
brew services stop levin
```

### Manual Execution

```bash
# Start in foreground (for testing)
levin start --foreground

# With custom config
levin start --config /path/to/config.toml --foreground

# Check version
levin --version

# Stop the daemon
levin terminate
```

### Monitor Status

```bash
# View overall status
levin status

# List all torrents
levin list

# View statistics
levin stats
```

### Control the Daemon

```bash
# Pause all activity
levin pause

# Resume activity
levin resume

# Set bandwidth limits (KB/s, 0 = unlimited)
levin bandwidth --download 1024 --upload 2048
levin bandwidth --download 0     # Remove limit

# View current limits
levin bandwidth
```

## How It Works

1. **Watch Directory**: Levin monitors `~/.config/levin/torrents/` for `.torrent` files
2. **Load Torrents**: When a `.torrent` file appears, it's loaded into the session
3. **Query Seeders**: Every 60 minutes (configurable), the daemon queries DHT/trackers for seeder counts
4. **Calculate Priorities**: Pieces are prioritized by rarity and torrent seeder count
5. **Download Pieces**: Available disk space (in `~/.cache/levin/data/`) is filled with highest-priority pieces
6. **Monitor Space**: Every 60 seconds, disk space is checked and metrics are updated
7. **Rebalance**: If over budget, lowest-priority pieces are deleted; if under budget, new pieces are downloaded
8. **Emergency Mode**: If severely over budget (>100MB), all downloads are paused until space is freed
9. **Seed**: All pieces we have are seeded to the swarm

### Disk Space Management

Levin strictly enforces disk space limits:
- **Minimum Free Space**: `max(min_free_space, min_free_percentage × total_disk_space)`
- **Budget**: Amount of space available for torrent data = `free_space - min_free_space`
- **Emergency Mode**: Triggered when current usage exceeds budget by >100MB
  - All downloads are immediately paused
  - Lowest-priority pieces are deleted until under budget
  - Downloads automatically resume when space is available

## CLI Commands

| Command | Description |
|---------|-------------|
| `levin status` | Show daemon status, disk usage, network stats |
| `levin list` | List all loaded torrents with priorities |
| `levin stats` | Show session and lifetime statistics |
| `levin pause` | Pause all torrent activity |
| `levin resume` | Resume torrent activity |
| `levin bandwidth` | View or set bandwidth limits |
| `levin terminate` | Stop the daemon gracefully |
| `levin --version` | Show version information |
| `levin --help` | Show help message |

## Testing

Levin has been thoroughly tested with:
- ✅ 8 real torrents from Anna's Archive (940GB total)
- ✅ Disk space management with simulated disk fill/drain
- ✅ Emergency mode triggering and recovery
- ✅ Bandwidth limiting (download/upload)
- ✅ Pause/resume functionality
- ✅ CLI interface (status, list, stats, bandwidth commands)
- ✅ Statistics persistence across restarts
- ✅ Piece prioritization based on seeder counts

Test results:
- Successfully managed 5.42GB of torrent data with strict 3.71GB budget
- Emergency mode correctly triggered when >100MB over budget
- Automatic recovery and download resumption when space became available
- All CLI commands functional and properly formatted

## Development

### Project Structure

```
Levin/
├── src/               # Daemon source code (2,467 lines)
│   ├── main.cpp           # Entry point
│   ├── daemon.cpp         # Main daemon loop
│   ├── config.cpp         # TOML configuration parser
│   ├── logger.cpp         # Logging (spdlog wrapper)
│   ├── session.cpp        # libtorrent session wrapper
│   ├── torrent_watcher.cpp  # inotify-based file watcher
│   ├── piece_manager.cpp   # Core prioritization logic (323 lines)
│   ├── disk_monitor.cpp    # Disk space management
│   ├── statistics.cpp      # JSON persistence for stats
│   └── cli_server.cpp      # Unix socket server (291 lines)
├── cli_client.cpp         # CLI client functionality (integrated)
├── config/            # Example configuration files
├── scripts/           # Installation and service scripts
│   ├── Levin.service   # systemd unit file
│   └── install.sh         # Installation script
├── CMakeLists.txt     # Root build configuration
└── PROGRESS.md        # Implementation progress tracking
```

### Building for Development

```bash
# Debug build with symbols
mkdir build-debug && cd build-debug
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)

# Run tests
ctest

# Memory leak detection
valgrind --leak-check=full ./levin start --config ../config/levin.toml.example --foreground
```

### Key Files

- **piece_manager.cpp**: Core intelligence - implements piece prioritization algorithms
- **disk_monitor.cpp**: Enforces strict disk space limits with emergency mode
- **cli_server.cpp**: JSON-based IPC via Unix domain sockets
- **daemon.cpp**: Event loop with 60-second rebalance cycle

## Contributing

Contributions are welcome! Please see PROGRESS.md for current implementation status and open tasks.

## License

MIT License - see [LICENSE](LICENSE) file for details.

## Acknowledgments

- [libtorrent-rasterbar](https://libtorrent.org/) - The excellent BitTorrent library powering this project
- WebRTC support via [PR #4123](https://github.com/arvidn/libtorrent/pull/4123)
