# End-to-End Tests

This directory contains E2E tests that verify Levin's behavior according to the [DESIGN.md](../DESIGN.md) specification. Tests are black-box style - they interact with the app through its public interfaces (CLI for Desktop, ADB for Android) rather than testing internal code.

## Test Framework

Both platforms use [BATS](https://github.com/bats-core/bats-core) (Bash Automated Testing System), the industry standard for shell-based integration testing.

## Directory Structure

```
e2e/
├── desktop/
│   ├── run_tests.sh           # Test runner
│   ├── test_helper.bash       # Shared utilities
│   ├── test_config.toml       # Test configuration (200MB limits)
│   ├── test_state_machine.bats # State machine tests
│   ├── test_storage.bats      # Storage management tests
│   └── test_power.bats        # Power monitoring tests
├── android/
│   ├── run_tests.sh           # Test runner
│   ├── test_helper.bash       # Shared utilities
│   └── test_state_machine.bats # State/network/battery tests
└── README.md                  # This file
```

## Test Categories

Tests verify behavior specified in DESIGN.md:

### State Machine Tests
- **IDLE**: No torrents loaded
- **SEEDING**: Storage limit reached (downloads paused, uploads continue)
- **PAUSED**: Battery or network conditions not met
- **DOWNLOADING**: Normal operation (not tested without real torrents)
- **Priority order**: Verifies state transitions follow correct precedence

### Storage Management Tests
- Budget calculation with 50MB hysteresis
- Over-budget detection
- File deletion (random order, individual files)
- Sparse file handling (actual disk blocks, not logical size)

### Platform-Specific Tests
- **Desktop**: Power monitoring via DBus/UPower
- **Android**: Battery simulation (`adb shell dumpsys battery`)
- **Android**: Network simulation (WiFi enable/disable)

## Running Tests

### Prerequisites

**Both platforms:**
```bash
# Install bats-core
# Ubuntu/Debian:
sudo apt-get install bats

# macOS:
brew install bats-core

# Or via npm:
npm install -g bats
```

### Desktop

```bash
# Build levin first
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run all desktop E2E tests
./e2e/desktop/run_tests.sh

# Run specific test file
./e2e/desktop/run_tests.sh e2e/desktop/test_storage.bats

# Run specific test by name
bats e2e/desktop/test_storage.bats --filter "BUDGET"
```

**Requirements:**
- Linux (Ubuntu 20.04+) or macOS
- levin binary built in `build/`
- ~500MB free disk space for test files

### Android

```bash
# Start emulator (or connect device)
$ANDROID_SDK_ROOT/emulator/emulator -avd <avd_name> &

# Wait for boot
adb wait-for-device
adb shell "while [[ -z \$(getprop sys.boot_completed) ]]; do sleep 1; done"

# Build APK
cd android && ./gradlew assembleDebug && cd ..

# Run all Android E2E tests
./e2e/android/run_tests.sh
```

**Requirements:**
- Android SDK with platform-tools (adb)
- Android emulator running OR physical device connected
- APK built (`android/app/build/outputs/apk/debug/app-debug.apk`)

## Test Configuration

### Desktop

Tests use `e2e/desktop/test_config.toml` with:
- `max_storage = "200mb"` - Small limit for fast testing
- `min_free = "10mb"` - Minimal disk reservation
- `check_interval_seconds = 5` - Frequent storage checks
- Isolated directories in `/tmp/levin-e2e-*/`

### Android

Tests use the app's default settings but:
- Each test run installs/uninstalls the app for clean state
- Battery and network conditions are simulated via ADB
- Data files are created in the app's external storage directory

## CI Integration

Tests run automatically on GitHub Actions:

- **Desktop E2E**: Runs on `ubuntu-latest` after build passes
- **Android E2E**: Runs on emulator using `reactivecircus/android-emulator-runner`

See `.github/workflows/ci.yml` and `.github/workflows/android-ci.yml` for details.

## Writing New Tests

### Test Structure (BATS)

```bash
#!/usr/bin/env bats

load 'test_helper'

setup() {
    setup_test_environment  # Create isolated test dir
}

teardown() {
    teardown_test_environment  # Clean up
}

@test "descriptive test name" {
    # Arrange
    start_daemon
    create_files_mb 100 5
    
    # Act
    sleep 5  # Wait for storage check
    
    # Assert
    local state=$(get_state_text)
    assert_contains "${state}" "expected" "Error message"
}
```

### Best Practices

1. **Test the contract, not implementation**: Use CLI output and observable behavior
2. **Isolate tests**: Each test gets fresh directories
3. **Use descriptive names**: `@test "SEEDING: transitions when over max_storage"`
4. **Clean up**: Always implement `teardown()` to stop daemons and remove files
5. **Handle timing**: Use `wait_for_state` with timeouts instead of fixed sleeps
6. **Debug helpers**: Call `print_debug_info` on failures

## Troubleshooting

### Desktop

**"Failed to connect to daemon"**
- Daemon may have crashed during startup
- Check `${TEST_DIR}/levin.log` for errors

**"Timeout waiting for state"**
- Storage check interval may be too long
- Check if daemon is actually running: `ps aux | grep levin`

### Android

**"No Android device/emulator connected"**
- Start emulator: `$ANDROID_SDK_ROOT/emulator/emulator -avd <name>`
- Check connection: `adb devices`

**"Multiple devices connected"**
- Set `ANDROID_SERIAL=emulator-5554` before running tests

**State not changing after battery simulation**
- Some emulators don't respond to `dumpsys battery set`
- Try resetting first: `adb shell dumpsys battery reset`

## Test Results

On success:
```
1..10
ok 1 IDLE: daemon starts in Idle state with no torrents
ok 2 IDLE: status shows 'Idle (no torrents)' text
...
```

On failure:
```
not ok 3 SEEDING: transitions to Seeding when over max_storage
# (in test file test_state_machine.bats, line 45)
#   `wait_for_state "Seeding" 30' failed
# Timeout waiting for state 'Seeding'. Current: 'Idle (no torrents)'
```

Logs are uploaded as artifacts on CI failures.
