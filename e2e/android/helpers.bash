# Common helpers for Android E2E tests (adb + BATS)
#
# Prerequisites:
#   - adb in PATH with exactly one device/emulator connected
#   - APK built: platforms/android/app/build/outputs/apk/debug/app-debug.apk
#   - App is debuggable (run-as works)

PKG="com.yoavmoshe.levin"
SERVICE="${PKG}/.service.LevinService"
ACTIVITY="${PKG}/.ui.MainActivity"
ACTION_START="${PKG}.action.START"
ACTION_STOP="${PKG}.action.STOP"
APK_PATH="${LEVIN_APK:-platforms/android/app/build/outputs/apk/debug/app-debug.apk}"
FIXTURE_TORRENT="${BATS_TEST_DIRNAME}/../../liblevin/tests/fixtures/test.torrent"

# App-internal paths (relative to run-as cwd = /data/data/$PKG)
APP_FILES="files"
WATCH_DIR="files/watch"
DATA_DIR="files/data"
STATE_DIR="files/state"

setup() {
    # Ensure device is connected
    local devices
    devices="$(adb devices | grep -w device | grep -v "List")"
    if [ -z "$devices" ]; then
        skip "No Android device connected"
    fi

    # Clear logcat so each test gets fresh logs
    adb logcat -c 2>/dev/null || true
}

teardown() {
    # Force-stop the app (kills service + activity reliably on all API levels)
    adb shell am force-stop "$PKG" 2>/dev/null || true
    sleep 1

    # Clean up app data directories (but keep the app installed)
    run_as rm -rf "$WATCH_DIR" "$DATA_DIR" "$STATE_DIR" 2>/dev/null || true

    # Clear shared preferences
    run_as rm -rf shared_prefs 2>/dev/null || true
}

# --- Service Control ---

# Start the foreground service by launching the activity.
# On API 31+, `am start-foreground-service` from the shell fails with
# "Requires permission not exported from uid". The activity's onCreate()
# calls LevinService.start(), so launching the activity is the reliable way.
start_service() {
    adb shell am start -n "$ACTIVITY" 2>&1
    # Wait for service to appear in dumpsys
    wait_for_service_running 15
}

# Stop the service by sending the STOP action via broadcast-like intent.
# On API 35 we can't use am startservice either, so we force-stop instead.
stop_service() {
    adb shell am force-stop "$PKG" 2>&1
    sleep 1
}

# Force-stop the entire app (kills service + activity)
force_stop() {
    adb shell am force-stop "$PKG" 2>&1
}

# --- Queries ---

# Run a command as the app user (run-as)
run_as() {
    adb shell run-as "$PKG" "$@"
}

# Check if service is currently running via dumpsys
is_service_running() {
    local output
    output="$(adb shell dumpsys activity services "$SERVICE" 2>&1)"
    echo "$output" | grep -q "ServiceRecord.*LevinService"
}

# Wait for service to appear in dumpsys
wait_for_service_running() {
    local timeout="${1:-10}"
    local count=0
    while [ "$count" -lt "$timeout" ]; do
        if is_service_running; then
            return 0
        fi
        sleep 1
        count=$((count + 1))
    done
    echo "Timed out waiting for service to start"
    return 1
}

# Wait for service to stop
wait_for_service_stopped() {
    local timeout="${1:-10}"
    local count=0
    while [ "$count" -lt "$timeout" ]; do
        if ! is_service_running; then
            return 0
        fi
        sleep 1
        count=$((count + 1))
    done
    echo "Timed out waiting for service to stop"
    return 1
}

# Get logcat lines matching a tag (from current test only, since we cleared in setup)
get_logcat() {
    local tag="${1:-LevinJNI}"
    adb logcat -d -s "${tag}:*" 2>/dev/null
}

# Get all levin-related logcat lines (LevinJNI, LevinCore, LevinService tags)
get_levin_logs() {
    adb logcat -d -s "LevinJNI:*" "LevinCore:*" "LevinService:*" "NetworkMonitor:*" "PowerMonitor:*" 2>/dev/null || true
}

# Wait for a specific string to appear in logcat
wait_for_log() {
    local pattern="$1"
    local timeout="${2:-10}"
    local count=0
    while [ "$count" -lt "$timeout" ]; do
        if get_levin_logs | grep -q "$pattern"; then
            return 0
        fi
        sleep 1
        count=$((count + 1))
    done
    echo "Timed out waiting for log pattern: $pattern"
    echo "Recent logs:"
    get_levin_logs | tail -20
    return 1
}

# Check if a file exists in the app's data directory
app_file_exists() {
    run_as test -f "$1" 2>/dev/null
}

# Check if a directory exists in the app's data directory
app_dir_exists() {
    run_as test -d "$1" 2>/dev/null
}

# List files in an app directory
app_ls() {
    run_as ls "$@" 2>/dev/null
}

# Count files in an app directory
app_file_count() {
    local dir="$1"
    local count
    count="$(run_as ls "$dir" 2>/dev/null | wc -l)"
    echo "$count"
}

# Push a file into the app's data directory
# Since adb push can't write to /data/data directly, we use a temp location
push_to_app() {
    local local_path="$1"
    local app_rel_path="$2"
    local filename
    filename="$(basename "$app_rel_path")"
    local dir
    dir="$(dirname "$app_rel_path")"

    # Ensure directory exists
    run_as mkdir -p "$dir" 2>/dev/null || true

    # Push to /data/local/tmp first, then copy via run-as
    adb push "$local_path" "/data/local/tmp/_levin_e2e_${filename}" >/dev/null 2>&1
    adb shell "cat /data/local/tmp/_levin_e2e_${filename} | run-as $PKG sh -c 'cat > ${app_rel_path}'" 2>&1
    adb shell rm -f "/data/local/tmp/_levin_e2e_${filename}" 2>/dev/null || true
}

# Install the APK (idempotent)
install_apk() {
    if [ ! -f "$APK_PATH" ]; then
        skip "APK not found at: $APK_PATH"
    fi
    adb install -r "$APK_PATH" 2>&1
}

# Check foreground service notification
has_foreground_notification() {
    local output
    output="$(adb shell dumpsys notification 2>&1)"
    echo "$output" | grep -q "$PKG"
}
