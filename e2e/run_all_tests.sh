#!/usr/bin/env bash
#
# Unified E2E Test Runner
# Runs all E2E tests for both Desktop and Android
#
# Usage: ./run_all_tests.sh [--desktop-only|--android-only]
#
# Requirements:
# - bats-core installed
# - For Desktop: levin binary built
# - For Android: emulator or device connected, APK built
# - Environment variables: ANDROID_SDK_ROOT, ANDROID_HOME
#
# The script will:
# 1. Check prerequisites
# 2. Build Desktop if needed
# 3. Build Android APK if needed
# 4. Start Android emulator if needed
# 5. Run all tests
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Parse arguments
RUN_DESKTOP=true
RUN_ANDROID=true

while [[ $# -gt 0 ]]; do
    case $1 in
        --desktop-only)
            RUN_ANDROID=false
            shift
            ;;
        --android-only)
            RUN_DESKTOP=false
            shift
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [--desktop-only|--android-only]"
            exit 1
            ;;
    esac
done

# Helper functions
log_section() {
    echo ""
    echo -e "${BLUE}================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}================================${NC}"
}

log_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

log_error() {
    echo -e "${RED}✗ $1${NC}"
}

log_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

log_info() {
    echo -e "${NC}  $1${NC}"
}

# Check prerequisites
check_prerequisites() {
    log_section "Checking Prerequisites"
    
    # Check bats
    if ! command -v bats &> /dev/null; then
        log_error "bats-core is not installed"
        log_info "Install with:"
        log_info "  Ubuntu/Debian: sudo apt-get install bats"
        log_info "  macOS: brew install bats-core"
        exit 1
    fi
    log_success "bats-core installed"
    
    # Check cmake
    if ! command -v cmake &> /dev/null; then
        log_error "cmake is not installed"
        exit 1
    fi
    log_success "cmake installed"
    
    # Check Android SDK for Android tests
    if [[ "$RUN_ANDROID" == true ]]; then
        if [[ -z "${ANDROID_SDK_ROOT:-}" ]]; then
            log_error "ANDROID_SDK_ROOT environment variable not set"
            log_info "Set it with: export ANDROID_SDK_ROOT=\$HOME/Android/Sdk"
            exit 1
        fi
        log_success "ANDROID_SDK_ROOT set: $ANDROID_SDK_ROOT"
        
        if ! command -v adb &> /dev/null; then
            log_error "adb not found in PATH"
            log_info "Add to PATH: export PATH=\$PATH:\$ANDROID_SDK_ROOT/platform-tools"
            exit 1
        fi
        log_success "adb available"
    fi
}

# Build Desktop
build_desktop() {
    log_section "Building Desktop"
    
    if [[ -x "${PROJECT_ROOT}/build/levin" ]]; then
        log_info "Binary already exists, skipping build"
        log_success "Desktop binary ready"
        return 0
    fi
    
    log_info "Building levin..."
    if [[ ! -d "${PROJECT_ROOT}/build" ]]; then
        cmake -B "${PROJECT_ROOT}/build" -DCMAKE_BUILD_TYPE=Release
    fi
    cmake --build "${PROJECT_ROOT}/build"
    
    log_success "Desktop build complete"
}

# Build Android APK
build_android() {
    log_section "Building Android APK"
    
    local apk_path="${PROJECT_ROOT}/android/app/build/outputs/apk/debug/app-debug.apk"
    
    if [[ -f "${apk_path}" ]]; then
        log_info "APK already exists, skipping build"
        log_success "Android APK ready"
        return 0
    fi
    
    log_info "Building debug APK..."
    (cd "${PROJECT_ROOT}/android" && ./gradlew assembleDebug --quiet)
    
    log_success "Android build complete"
}

# Start Android emulator if needed
start_emulator() {
    log_section "Checking Android Device"
    
    # Check if device is already connected
    local devices=$(adb devices | grep -v "List of devices" | grep -v "^$" | wc -l)
    
    if [[ $devices -gt 0 ]]; then
        log_success "Device already connected: $(adb get-serialno)"
        return 0
    fi
    
    log_info "No device connected, starting emulator..."
    
    # Find available AVD
    local avd=$($ANDROID_SDK_ROOT/emulator/emulator -list-avds | head -n 1)
    
    if [[ -z "$avd" ]]; then
        log_error "No Android Virtual Device (AVD) found"
        log_info "Create one with Android Studio or avdmanager"
        exit 1
    fi
    
    log_info "Starting emulator: $avd"
    $ANDROID_SDK_ROOT/emulator/emulator -avd "$avd" -dns-server 8.8.8.8,8.8.4.4 -no-snapshot-load > /tmp/emulator.log 2>&1 &
    
    log_info "Waiting for device to boot..."
    adb wait-for-device
    adb shell "while [[ -z \$(getprop sys.boot_completed) ]]; do sleep 1; done" 2>/dev/null
    
    log_success "Emulator ready"
}

# Run Desktop tests
run_desktop_tests() {
    log_section "Running Desktop E2E Tests"
    
    if "${SCRIPT_DIR}/desktop/run_tests.sh"; then
        log_success "Desktop tests passed"
        return 0
    else
        log_error "Desktop tests failed"
        return 1
    fi
}

# Run Android tests
run_android_tests() {
    log_section "Running Android E2E Tests"
    
    if "${SCRIPT_DIR}/android/run_tests.sh"; then
        log_success "Android tests passed"
        return 0
    else
        log_error "Android tests failed"
        return 1
    fi
}

# Main execution
main() {
    log_section "Levin E2E Test Suite"
    
    local exit_code=0
    
    check_prerequisites
    
    if [[ "$RUN_DESKTOP" == true ]]; then
        build_desktop
        if ! run_desktop_tests; then
            exit_code=1
        fi
    fi
    
    if [[ "$RUN_ANDROID" == true ]]; then
        build_android
        start_emulator
        if ! run_android_tests; then
            exit_code=1
        fi
    fi
    
    # Summary
    echo ""
    log_section "Test Summary"
    
    if [[ $exit_code -eq 0 ]]; then
        log_success "All tests passed!"
    else
        log_error "Some tests failed"
    fi
    
    exit $exit_code
}

main
