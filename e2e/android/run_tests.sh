#!/usr/bin/env bash
#
# Android E2E Test Runner
# Runs all E2E tests on an Android emulator or device
#
# Usage: ./run_tests.sh [test_file.bats]
#
# Requirements:
# - Android SDK with platform-tools (adb)
# - Android emulator running OR physical device connected
# - APK built (./gradlew assembleDebug)
# - bats-core installed
#
# Environment variables:
# - ANDROID_SERIAL: Specific device/emulator to use (optional)
# - SKIP_INSTALL: Skip APK installation if already installed
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
ANDROID_DIR="${PROJECT_ROOT}/android"

# Package and activity names
export PACKAGE_NAME="com.yoavmoshe.levin"
export MAIN_ACTIVITY="${PACKAGE_NAME}/.ui.MainActivity"

# Check if bats is installed
if ! command -v bats &> /dev/null; then
    echo "Error: bats-core is not installed"
    echo "Install with:"
    echo "  Ubuntu/Debian: sudo apt-get install bats"
    echo "  macOS: brew install bats-core"
    exit 1
fi

# Check if adb is available
if ! command -v adb &> /dev/null; then
    echo "Error: adb not found. Install Android SDK platform-tools"
    exit 1
fi

# Check for connected device/emulator
check_device() {
    local devices=$(adb devices | grep -v "List of devices" | grep -v "^$" | wc -l)
    if [[ $devices -eq 0 ]]; then
        echo "Error: No Android device/emulator connected"
        echo ""
        echo "Start an emulator with:"
        echo "  \$ANDROID_SDK_ROOT/emulator/emulator -avd <avd_name>"
        echo ""
        echo "Or connect a physical device with USB debugging enabled"
        exit 1
    fi
    
    if [[ $devices -gt 1 && -z "${ANDROID_SERIAL:-}" ]]; then
        echo "Warning: Multiple devices connected. Set ANDROID_SERIAL to select one:"
        adb devices
        echo ""
        echo "Example: ANDROID_SERIAL=emulator-5554 ./run_tests.sh"
        exit 1
    fi
    
    echo "Using device: $(adb get-serialno)"
}

# Build APK if needed
build_apk() {
    local apk_path="${ANDROID_DIR}/app/build/outputs/apk/debug/app-debug.apk"
    
    if [[ ! -f "${apk_path}" ]]; then
        echo "Building debug APK..."
        (cd "${ANDROID_DIR}" && ./gradlew assembleDebug --quiet)
    fi
    
    export APK_PATH="${apk_path}"
    echo "APK: ${APK_PATH}"
}

# Export paths for tests
export PROJECT_ROOT
export ANDROID_DIR
export E2E_DIR="${SCRIPT_DIR}"

# Main
echo "================================"
echo "Android E2E Test Runner"
echo "================================"
echo ""

check_device
build_apk

echo ""
echo "Running tests..."
echo "================================"

# Run specific test or all tests
if [[ $# -gt 0 ]]; then
    bats "$@"
else
    bats "${SCRIPT_DIR}"/*.bats
fi
