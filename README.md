<p align="center"><img width="100"  alt="levin" src="https://github.com/user-attachments/assets/cc1c141b-edb7-4bdf-930b-24c3d8329359" /></p>
<h1 align="center">Levin</h1>
<p align="center"><strong>Anna archives, Levin seeds</strong></p>

Levin is the easiest way to support Anna's Archive using resources you already have and aren't using — your idle phone or laptop, spare disk space, and network connection. It quietly seeds archive torrents in the background, while completely pausing on battery or cellular, and deleting its own data if the available diskspace goes below the limit you set. It costs you nothing, and helps keep the world's largest open library available to everyone.

## Android

Your phone is one of the best places to run Levin, because unlike your laptop, it is often on while you're not using it. Levin defaults to running only when being charged and on a WiFi.

Download the APK from the [releases page](https://github.com/bjesus/levin/releases) and install it. On first launch, the app will offer to fetch torrent files from Anna's Archive.


## Linux

### Install

Download the `.deb` package or the standalone binary from the [releases page](https://github.com/bjesus/levin/releases).

```sh
# Debian/Ubuntu
sudo dpkg -i levin_0.0.3_amd64.deb

# Or just copy the binary
chmod +x levin-linux-x86_64
sudo cp levin-linux-x86_64 /usr/local/bin/levin
```

If you installed the `.deb`, a systemd user service is included:

```sh
systemctl --user enable --now levin
```

## macOS

Install via Homebrew:

```sh
brew install bjesus/levin/levin
```

Or download `levin-macos.zip` from the [releases page](https://github.com/bjesus/levin/releases), unzip, and move `Levin.app` to your Applications folder. On first launch, right-click the app and select "Open" to bypass Gatekeeper (one-time only).

## Usage

```
levin start      Start the daemon
levin stop       Stop the daemon
levin status     Show daemon status
levin list       List active torrents
levin pause      Pause all seeding/downloading
levin resume     Resume seeding/downloading
levin populate   Fetch torrents from Anna's Archive
```

## Configuration

The config file is at `~/.config/levin/levin.toml`. All fields are optional.

```toml
# Directories
watch_directory = "~/.config/levin/torrents"
data_directory = "~/.cache/levin/data"
state_directory = "~/.local/state/levin"

# Disk limits
min_free_bytes = "1GB"       # minimum free space to keep on disk
min_free_percentage = 0.05   # minimum free space as fraction of total
max_storage_bytes = "0"      # max space levin may use (0 = unlimited)

# Conditions
run_on_battery = false
run_on_cellular = false

# Bandwidth limits (0 = unlimited)
max_download_kbps = 0
max_upload_kbps = 0

# Network
stun_server = "stun.l.google.com:19302"
```

Changes to `run_on_battery`, `run_on_cellular`, and bandwidth limits are picked up on `SIGHUP`:

```sh
kill -HUP $(cat ~/.local/state/levin/levin.pid)
```

## Building from source

Requires CMake 3.20+, a C++17 compiler, libcurl, Boost, and OpenSSL.

```sh
cmake -S . -B build -DLEVIN_USE_STUB_SESSION=OFF
cmake --build build -j$(nproc)
```

The binary is at `build/platforms/linux/levin`.

For Android, see `platforms/android/build-deps.sh` for prerequisite setup, then build with Gradle:

```sh
cd platforms/android
./build-deps.sh
./gradlew assembleDebug
```

