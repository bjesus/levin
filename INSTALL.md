# Installation Guide

This guide covers installation of Levin across different platforms and scenarios.

## Table of Contents

- [Quick Start](#quick-start)
- [Debian/Ubuntu](#debianubuntu)
- [Fedora/RHEL/CentOS](#fedorарhelcentos)
- [Arch Linux (AUR)](#arch-linux-aur)
- [macOS](#macos)
- [Building from Source](#building-from-source)
- [Configuration](#configuration)
- [Service Management](#service-management)
- [Troubleshooting](#troubleshooting)

## Quick Start

Levin uses **user-level installation** - all data and configuration live in your home directory:

- **Config**: `~/.config/levin/levin.toml`
- **Data**: `~/.cache/levin/data/`
- **Logs**: `~/.local/state/levin/levin.log`
- **Torrents**: `~/.config/levin/torrents/`

No root access required after initial package installation!

## Debian/Ubuntu

### Supported Versions

- Ubuntu 20.04 LTS (Focal Fossa)
- Ubuntu 22.04 LTS (Jammy Jellyfish)
- Ubuntu 24.04 LTS (Noble Numbat)
- Debian 11 (Bullseye)
- Debian 12 (Bookworm)

### Installation Steps

1. **Download the package** for your Ubuntu version from [Releases](https://github.com/bjesus/levin/releases):
   ```bash
   # Example for Ubuntu 22.04
   wget https://github.com/bjesus/levin/releases/download/v0.1.0/levin_0.1.0_ubuntu-22.04_amd64.deb
   ```

2. **Install the package**:
   ```bash
   sudo dpkg -i levin_0.1.0_ubuntu-22.04_amd64.deb
   sudo apt-get install -f  # Install dependencies if needed
   ```

3. **Create default configuration**:
   ```bash
   levind
   ```
   
   This creates:
   - `~/.config/levin/levin.toml` (config file)
   - `~/.config/levin/torrents/` (watch directory)

4. **Edit configuration**:
   ```bash
   nano ~/.config/levin/levin.toml
   ```
   
   At minimum, review:
   - `[disk]` section - adjust space limits
   - `[paths.watch_directory]` - where to put .torrent files

5. **Enable and start the service**:
   ```bash
   systemctl --user enable levin
   systemctl --user start levin
   ```

6. **Check status**:
   ```bash
   systemctl --user status levin
   levin status
   ```

### Uninstallation

```bash
# Stop and disable service
systemctl --user stop levin
systemctl --user disable levin

# Remove package
sudo apt-get remove levin

# Remove user data (optional)
rm -rf ~/.config/levin ~/.cache/levin ~/.local/state/levin
```

## Fedora/RHEL/CentOS

### Supported Versions

- Fedora 39, 40
- RHEL 9
- CentOS Stream 9

### Installation Steps

1. **Download the package** for your Fedora version from [Releases](https://github.com/bjesus/levin/releases):
   ```bash
   # Example for Fedora 40
   wget https://github.com/bjesus/levin/releases/download/v0.1.0/levin-0.1.0-fedora40.x86_64.rpm
   ```

2. **Install the package**:
   ```bash
   sudo dnf install levin-0.1.0-fedora40.x86_64.rpm
   ```

3. **Create default configuration**:
   ```bash
   levind
   ```

4. **Edit configuration**:
   ```bash
   nano ~/.config/levin/levin.toml
   ```

5. **Enable and start the service**:
   ```bash
   systemctl --user enable levin
   systemctl --user start levin
   ```

6. **Check status**:
   ```bash
   systemctl --user status levin
   levin status
   ```

### Uninstallation

```bash
# Stop and disable service
systemctl --user stop levin
systemctl --user disable levin

# Remove package
sudo dnf remove levin

# Remove user data (optional)
rm -rf ~/.config/levin ~/.cache/levin ~/.local/state/levin
```

## Arch Linux (AUR)

### Installation with AUR Helper

Using `yay`:
```bash
yay -S levin
```

Using `paru`:
```bash
paru -S levin
```

### Manual Installation from AUR

```bash
# Clone the AUR repository
git clone https://aur.archlinux.org/levin.git
cd levin

# Or use packaging files directly
# cd packaging/aur

# Build and install
makepkg -si
```

### Post-Installation

1. **Create default configuration**:
   ```bash
   levind
   ```

2. **Edit configuration**:
   ```bash
   nano ~/.config/levin/levin.toml
   ```

3. **Enable and start the service**:
   ```bash
   systemctl --user enable levin
   systemctl --user start levin
   ```

### Uninstallation

```bash
# Stop and disable service
systemctl --user stop levin
systemctl --user disable levin

# Remove package
sudo pacman -R levin

# Remove user data (optional)
rm -rf ~/.config/levin ~/.cache/levin ~/.local/state/levin
```

## macOS

### Using Homebrew (Recommended)

1. **Add the tap**:
   ```bash
   brew tap bjesus/levin
   ```

2. **Install levin**:
   ```bash
   brew install levin
   ```

3. **Create default configuration**:
   ```bash
   levind
   ```

4. **Edit configuration**:
   ```bash
   nano ~/.config/levin/levin.toml
   ```

5. **Start the service**:
   ```bash
   brew services start levin
   ```

6. **Check status**:
   ```bash
   brew services list
   levin status
   ```

### Manual Installation from Binary

1. **Download the tarball** from [Releases](https://github.com/bjesus/levin/releases):
   ```bash
   wget https://github.com/bjesus/levin/releases/download/v0.1.0/levin-0.1.0-macos-arm64.tar.gz
   tar xzf levin-0.1.0-macos-arm64.tar.gz
   ```

2. **Install binaries**:
   ```bash
   sudo cp levin-0.1.0-macos/bin/* /usr/local/bin/
   ```

3. **Follow steps 3-6 from Homebrew installation above**

### Uninstallation

```bash
# Stop service
brew services stop levin

# Uninstall
brew uninstall levin
brew untap bjesus/levin

# Remove user data (optional)
rm -rf ~/.config/levin ~/.cache/levin ~/.local/state/levin
```

## Building from Source

### Prerequisites

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    libboost-system-dev \
    libboost-filesystem-dev \
    libssl-dev \
    libtorrent-rasterbar-dev \
    pkg-config
```

**Fedora:**
```bash
sudo dnf install -y \
    gcc-c++ \
    cmake \
    git \
    boost-devel \
    openssl-devel \
    rb_libtorrent-devel \
    pkgconfig
```

**macOS:**
```bash
brew install cmake boost libtorrent-rasterbar openssl@3 pkg-config
```

### Build Steps

1. **Clone the repository**:
   ```bash
   git clone https://github.com/bjesus/levin.git
   cd levin
   ```

2. **Configure the build**:
   ```bash
   mkdir build && cd build
   cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF ..
   ```
   
   On macOS, you may need:
   ```bash
   cmake -DCMAKE_BUILD_TYPE=Release \
         -DBUILD_TESTS=OFF \
         -DOPENSSL_ROOT_DIR=/usr/local/opt/openssl@3 \
         ..
   ```

3. **Build**:
   ```bash
   make -j$(nproc)  # Linux
   make -j$(sysctl -n hw.ncpu)  # macOS
   ```

4. **Install** (optional):
   ```bash
   sudo make install
   ```
   
   Or run from build directory:
   ```bash
   ./levind --version
   ./cli/levin --version
   ```

5. **Create configuration**:
   ```bash
   levind  # or ./build/levind if not installed
   ```

6. **Enable service** (if installed):
   ```bash
   systemctl --user enable levin
   systemctl --user start levin
   ```

### Development Build

For development with debug symbols and tests:
```bash
mkdir build-debug && cd build-debug
cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON ..
make -j$(nproc)
./tests/levin_tests  # Run tests
```

## Configuration

### Default Configuration

On first run, Levin creates `~/.config/levin/levin.toml` with sensible defaults.

### Key Configuration Options

**Disk Space Management:**
```toml
[disk]
min_free_bytes = 1073741824       # 1GB minimum free
min_free_percentage = 0.05        # 5% of disk must stay free
check_interval_seconds = 60       # Check every minute
```

**Paths:**
```toml
[paths]
watch_directory = "$XDG_CONFIG_HOME/levin/torrents"
data_directory = "$XDG_CACHE_HOME/levin/data"
```

**Network:**
```toml
[network]
listen_port = 6881
enable_dht = true
enable_lsd = true
enable_upnp = true
```

**Limits:**
```toml
[limits]
max_download_rate_kbps = 0        # 0 = unlimited
max_upload_rate_kbps = 0          # 0 = unlimited
max_active_torrents = 8
```

### Environment Variables

The config file supports:
- `$VAR` or `${VAR}` expansion
- `~` for home directory
- XDG variables with automatic fallbacks:
  - `$XDG_CONFIG_HOME` (default: `~/.config`)
  - `$XDG_CACHE_HOME` (default: `~/.cache`)
  - `$XDG_STATE_HOME` (default: `~/.local/state`)

## Service Management

### Linux (systemd)

```bash
# Check status
systemctl --user status levin

# View logs (live)
journalctl --user -u levin -f

# View recent logs
journalctl --user -u levin -n 50

# Restart
systemctl --user restart levin

# Stop
systemctl --user stop levin

# Disable automatic start
systemctl --user disable levin

# Re-enable
systemctl --user enable levin
```

### macOS (Homebrew Services)

```bash
# Start
brew services start levin

# Check status
brew services list

# View logs
tail -f ~/.local/state/levin/stdout.log
tail -f ~/.local/state/levin/stderr.log

# Stop
brew services stop levin

# Restart
brew services restart levin
```

### Manual Execution

Run in foreground for testing:
```bash
levind --foreground
```

With custom config:
```bash
levind --config /path/to/config.toml --foreground
```

## Troubleshooting

### Service won't start

1. **Check logs**:
   ```bash
   journalctl --user -u levin -n 100
   # or
   cat ~/.local/state/levin/levin.log
   ```

2. **Check configuration**:
   ```bash
   levind --config ~/.config/levin/levin.toml --foreground
   ```
   
   This will show errors immediately.

3. **Verify directories exist**:
   ```bash
   ls -la ~/.config/levin/
   ls -la ~/.cache/levin/
   ls -la ~/.local/state/levin/
   ```

### CLI commands fail

1. **Check if daemon is running**:
   ```bash
   systemctl --user status levin
   ps aux | grep levind
   ```

2. **Check control socket**:
   ```bash
   ls -la ~/.local/state/levin/levin.sock
   ```

3. **Try with explicit socket path**:
   ```bash
   levin --socket ~/.local/state/levin/levin.sock status
   ```

### Disk space issues

1. **Check current usage**:
   ```bash
   levin status
   df -h ~/.cache/levin/
   ```

2. **Adjust disk limits** in `~/.config/levin/levin.toml`:
   ```toml
   [disk]
   min_free_bytes = 2147483648      # Increase to 2GB
   min_free_percentage = 0.10       # Increase to 10%
   ```

3. **Restart service**:
   ```bash
   systemctl --user restart levin
   ```

### Permission errors

Levin runs as your user - check file permissions:
```bash
# Fix permissions
chmod 700 ~/.config/levin
chmod 700 ~/.cache/levin
chmod 700 ~/.local/state/levin
chmod 600 ~/.config/levin/levin.toml
```

### Port conflicts

If port 6881 is in use, change it in config:
```toml
[network]
listen_port = 6882  # or another available port
```

### Dependencies missing

**Ubuntu/Debian:**
```bash
sudo apt-get install -y libboost-system1.74.0 libboost-filesystem1.74.0 \
    libssl3 libtorrent-rasterbar2.0
```

**Fedora:**
```bash
sudo dnf install boost-system boost-filesystem openssl rb_libtorrent
```

### Getting Help

- **Issues**: [GitHub Issues](https://github.com/bjesus/levin/issues)
- **Logs**: `~/.local/state/levin/levin.log`
- **Status**: `levin status`
- **Version**: `levind --version`

Include the following in bug reports:
- OS and version
- Levin version (`levind --version`)
- Relevant log output
- Configuration (sanitized)
