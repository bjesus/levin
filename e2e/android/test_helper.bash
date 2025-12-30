#!/usr/bin/env bash
#
# Shared test utilities for Android E2E tests
#

# Cache directory for downloaded torrents
TORRENT_CACHE_DIR="${TORRENT_CACHE_DIR:-/tmp/levin_e2e_cache}"

# ADB wrapper with optional serial
adb_cmd() {
    if [[ -n "${ANDROID_SERIAL:-}" ]]; then
        adb -s "${ANDROID_SERIAL}" "$@"
    else
        adb "$@"
    fi
}

# Download torrent files for testing (cached)
download_test_torrents() {
    local count="${1:-3}"
    
    mkdir -p "${TORRENT_CACHE_DIR}"
    
    # Check if we already have enough cached torrents
    local cached=$(find "${TORRENT_CACHE_DIR}" -name "*.torrent" -type f 2>/dev/null | wc -l)
    if [[ $cached -ge $count ]]; then
        echo "Using $cached cached torrent files"
        return 0
    fi
    
    echo "Downloading torrent files from Anna's Archive..."
    
    # Get list of torrent URLs - use data torrents which are smaller
    local urls=$(curl -skL "https://annas-archive.org/torrents" 2>/dev/null | \
        grep -oE '/dyn/small_file/torrents/managed_by_aa/annas_archive_data__aacid/[^"]+\.torrent' | \
        head -10)
    
    local downloaded=0
    while IFS= read -r url; do
        [[ -z "$url" ]] && continue
        
        local filename=$(basename "$url")
        local filepath="${TORRENT_CACHE_DIR}/${filename}"
        
        if [[ ! -f "$filepath" ]]; then
            echo "Downloading: $filename"
            if curl -skL "https://annas-archive.org${url}" -o "$filepath" 2>/dev/null; then
                # Verify it's a valid torrent file (at least 1KB)
                local size=$(stat -c%s "$filepath" 2>/dev/null || echo "0")
                if [[ $size -gt 1000 ]] && file "$filepath" | grep -q "BitTorrent"; then
                    downloaded=$((downloaded + 1))
                    echo "Successfully downloaded: $filename ($size bytes)"
                else
                    echo "Invalid torrent file, removing: $filepath"
                    rm -f "$filepath"
                fi
            fi
        else
            downloaded=$((downloaded + 1))
        fi
        
        if [[ $downloaded -ge $count ]]; then
            break
        fi
    done <<< "$urls"
    
    echo "Downloaded $downloaded torrent files"
}

# Push test torrents to device
push_test_torrents() {
    local count="${1:-1}"
    local torrents_dir=$(get_torrents_dir)
    
    # Ensure we have torrents to push
    download_test_torrents "$count"
    
    # Create directory on device (app must have run at least once to create base dir)
    # Note: CI uses API 29 which doesn't have scoped storage restrictions
    adb_cmd shell "mkdir -p ${torrents_dir}" || {
        echo "Warning: Could not create torrents directory"
        return 1
    }
    
    # Push torrent files
    local pushed=0
    for torrent in "${TORRENT_CACHE_DIR}"/*.torrent; do
        if [[ -f "$torrent" ]]; then
            if ! adb_cmd push "$torrent" "${torrents_dir}/"; then
                echo "Warning: Failed to push $torrent"
                continue
            fi
            pushed=$((pushed + 1))
            if [[ $pushed -ge $count ]]; then
                break
            fi
        fi
    done
    
    echo "Pushed $pushed torrent files to device"
    
    # Verify files were pushed
    local file_count=$(adb_cmd shell "ls ${torrents_dir}/*.torrent 2>/dev/null | wc -l" | tr -d '\r')
    echo "Verified $file_count torrent files on device"
    
    if [[ $pushed -eq 0 ]]; then
        echo "ERROR: No torrent files were pushed!"
        return 1
    fi
}

# Install the app (uninstall first for clean state)
install_app() {
    echo "Installing app..."
    
    # Uninstall if present (ignore errors)
    adb_cmd uninstall "${PACKAGE_NAME}" 2>/dev/null || true
    
    # Install fresh
    adb_cmd install -r "${APK_PATH}"
    
    # Grant permissions BEFORE starting app
    adb_cmd shell pm grant "${PACKAGE_NAME}" android.permission.POST_NOTIFICATIONS 2>/dev/null || true
    adb_cmd shell pm grant "${PACKAGE_NAME}" android.permission.FOREGROUND_SERVICE 2>/dev/null || true
    
    # Set battery to charging so app doesn't immediately pause
    adb_cmd shell "dumpsys battery set ac 1" 2>/dev/null || true
    adb_cmd shell "dumpsys battery set status 2" 2>/dev/null || true
    
    echo "App installed: ${PACKAGE_NAME}"
}

# Uninstall the app
uninstall_app() {
    adb_cmd uninstall "${PACKAGE_NAME}" 2>/dev/null || true
}

# Start the app
start_app() {
    adb_cmd shell am start -n "${MAIN_ACTIVITY}"
    sleep 3  # Wait for app to start and service to initialize
}

# Start app with pre-populated torrents (skips "Add Torrents?" dialog)
start_app_with_torrents() {
    local count="${1:-1}"
    
    # Start app briefly to create directories
    adb_cmd shell am start -n "${MAIN_ACTIVITY}"
    sleep 3
    
    # Dismiss any permission dialogs
    dismiss_permission_dialogs
    
    # Click No on Add Torrents dialog to continue without downloading
    click_add_torrents_no || true
    dismiss_error_dialogs || true
    
    # Force stop to prepare for torrent push
    adb_cmd shell am force-stop "${PACKAGE_NAME}"
    sleep 1
    
    # Now directories should exist - push torrents
    push_test_torrents "$count"
    
    # Now start app again - should skip the dialog since we have torrents
    adb_cmd shell am start -n "${MAIN_ACTIVITY}"
    sleep 3
}

# Click on "Add Torrents?" dialog - Yes button
# This dialog appears on first launch when no torrents exist
click_add_torrents_yes() {
    echo "Clicking 'Yes' on Add Torrents dialog..."
    sleep 2  # Wait for dialog to appear
    
    # Try to find and click the "Yes" button using UI dump
    local yes_bounds=$(adb_cmd exec-out uiautomator dump /dev/stdout 2>/dev/null | \
        grep -oP 'text="Yes"[^>]*bounds="\[\d+,\d+\]\[\d+,\d+\]"' | \
        grep -oP 'bounds="\[\d+,\d+\]\[\d+,\d+\]"' | head -1)
    
    if [[ -n "$yes_bounds" ]]; then
        # Extract coordinates from bounds="[x1,y1][x2,y2]"
        local coords=$(echo "$yes_bounds" | grep -oP '\d+' | head -4)
        local x1=$(echo "$coords" | sed -n '1p')
        local y1=$(echo "$coords" | sed -n '2p')
        local x2=$(echo "$coords" | sed -n '3p')
        local y2=$(echo "$coords" | sed -n '4p')
        local tap_x=$(( (x1 + x2) / 2 ))
        local tap_y=$(( (y1 + y2) / 2 ))
        
        echo "Found Yes button at ($tap_x, $tap_y)"
        adb_cmd shell "input tap ${tap_x} ${tap_y}"
    else
        echo "Yes button not found, using fallback coordinates"
        # Fallback: calculate based on screen size
        local size=$(adb_cmd shell "wm size" | grep -oP '\d+x\d+' | tr -d '\r')
        local width=$(echo $size | cut -d'x' -f1)
        local height=$(echo $size | cut -d'x' -f2)
        local yes_x=$((width * 82 / 100))  # Adjusted for typical dialog
        local yes_y=$((height * 58 / 100))
        adb_cmd shell "input tap ${yes_x} ${yes_y}"
    fi
    
    # Wait for torrent download to begin (this can take time)
    echo "Waiting for torrents to download..."
    sleep 10
}

# Click on "Add Torrents?" dialog - No button
click_add_torrents_no() {
    echo "Clicking 'No' on Add Torrents dialog..."
    sleep 2  # Wait for dialog to appear
    
    # Try to find and click the "No" button using UI dump
    local no_bounds=$(adb_cmd exec-out uiautomator dump /dev/stdout 2>/dev/null | \
        grep -oP 'text="No"[^>]*bounds="\[\d+,\d+\]\[\d+,\d+\]"' | \
        grep -oP 'bounds="\[\d+,\d+\]\[\d+,\d+\]"' | head -1)
    
    if [[ -n "$no_bounds" ]]; then
        # Extract coordinates from bounds="[x1,y1][x2,y2]"
        local coords=$(echo "$no_bounds" | grep -oP '\d+' | head -4)
        local x1=$(echo "$coords" | sed -n '1p')
        local y1=$(echo "$coords" | sed -n '2p')
        local x2=$(echo "$coords" | sed -n '3p')
        local y2=$(echo "$coords" | sed -n '4p')
        local tap_x=$(( (x1 + x2) / 2 ))
        local tap_y=$(( (y1 + y2) / 2 ))
        
        echo "Found No button at ($tap_x, $tap_y)"
        adb_cmd shell "input tap ${tap_x} ${tap_y}"
    else
        echo "No button not found, using fallback coordinates"
        # Fallback: calculate based on screen size
        local size=$(adb_cmd shell "wm size" | grep -oP '\d+x\d+' | tr -d '\r')
        local width=$(echo $size | cut -d'x' -f1)
        local height=$(echo $size | cut -d'x' -f2)
        local no_x=$((width * 66 / 100))  # Adjusted for typical dialog
        local no_y=$((height * 58 / 100))
        adb_cmd shell "input tap ${no_x} ${no_y}"
    fi
    
    sleep 2
}

# Dismiss any permission dialogs that might appear
dismiss_permission_dialogs() {
    echo "Dismissing permission dialogs..."
    
    # Check for notification permission dialog and click Allow
    local allow_bounds=$(adb_cmd exec-out uiautomator dump /dev/stdout 2>/dev/null | \
        grep -oP 'text="Allow"[^>]*bounds="\[\d+,\d+\]\[\d+,\d+\]"' | \
        grep -oP 'bounds="\[\d+,\d+\]\[\d+,\d+\]"' | head -1)
    
    if [[ -n "$allow_bounds" ]]; then
        local coords=$(echo "$allow_bounds" | grep -oP '\d+' | head -4)
        local x1=$(echo "$coords" | sed -n '1p')
        local y1=$(echo "$coords" | sed -n '2p')
        local x2=$(echo "$coords" | sed -n '3p')
        local y2=$(echo "$coords" | sed -n '4p')
        local tap_x=$(( (x1 + x2) / 2 ))
        local tap_y=$(( (y1 + y2) / 2 ))
        
        echo "Found Allow button, clicking it"
        adb_cmd shell "input tap ${tap_x} ${tap_y}"
        sleep 1
    fi
}

# Dismiss error dialogs (SSL errors, etc.)
dismiss_error_dialogs() {
    echo "Dismissing error dialogs..."
    
    # Look for OK button in error dialogs
    local ok_bounds=$(adb_cmd exec-out uiautomator dump /dev/stdout 2>/dev/null | \
        grep -oP 'text="OK"[^>]*bounds="\[\d+,\d+\]\[\d+,\d+\]"' | \
        grep -oP 'bounds="\[\d+,\d+\]\[\d+,\d+\]"' | head -1)
    
    if [[ -n "$ok_bounds" ]]; then
        local coords=$(echo "$ok_bounds" | grep -oP '\d+' | head -4)
        local x1=$(echo "$coords" | sed -n '1p')
        local y1=$(echo "$coords" | sed -n '2p')
        local x2=$(echo "$coords" | sed -n '3p')
        local y2=$(echo "$coords" | sed -n '4p')
        local tap_x=$(( (x1 + x2) / 2 ))
        local tap_y=$(( (y1 + y2) / 2 ))
        
        echo "Found OK button, clicking it"
        adb_cmd shell "input tap ${tap_x} ${tap_y}"
        sleep 1
    fi
}

# Stop the app
stop_app() {
    adb_cmd shell am force-stop "${PACKAGE_NAME}"
}

# Get the app's external files directory
get_data_dir() {
    echo "/sdcard/Android/data/${PACKAGE_NAME}/files"
}

# Get the app's torrents directory
get_torrents_dir() {
    echo "$(get_data_dir)/torrents"
}

# Create test files on device
create_device_file_mb() {
    local filename="$1"
    local size_mb="$2"
    local data_dir=$(get_data_dir)
    
    # Create directory if needed
    adb_cmd shell "mkdir -p ${data_dir}"
    
    # Create file using dd
    adb_cmd shell "dd if=/dev/urandom of=${data_dir}/${filename} bs=1048576 count=${size_mb}" 2>/dev/null
}

# Create multiple files totaling specified size
create_device_files_mb() {
    local total_mb="$1"
    local num_files="${2:-5}"
    local per_file_mb=$((total_mb / num_files))
    local data_dir=$(get_data_dir)
    
    adb_cmd shell "mkdir -p ${data_dir}"
    
    for i in $(seq 1 $num_files); do
        adb_cmd shell "dd if=/dev/urandom of=${data_dir}/testfile_${i}.dat bs=1048576 count=${per_file_mb}" 2>/dev/null
    done
}

# Get data directory size in MB
get_device_data_size_mb() {
    local data_dir=$(get_data_dir)
    local size_kb=$(adb_cmd shell "du -s ${data_dir} 2>/dev/null | cut -f1" | tr -d '\r')
    echo $((size_kb / 1024))
}

# Count files in data directory
count_device_files() {
    local data_dir=$(get_data_dir)
    adb_cmd shell "find ${data_dir} -type f 2>/dev/null | wc -l" | tr -d '\r'
}

# Clear all data files
clear_device_data() {
    local data_dir=$(get_data_dir)
    adb_cmd shell "rm -rf ${data_dir}/*" 2>/dev/null || true
}

# Get notification text
get_notification_text() {
    # Use dumpsys to get notification content
    # First get the full NotificationRecord block, then extract title/text
    local output=$(adb_cmd shell "dumpsys notification --noredact" 2>/dev/null)
    
    # Check if notification exists for our package
    if echo "$output" | grep -q "NotificationRecord.*pkg=${PACKAGE_NAME}"; then
        # Extract title and text from the notification record
        # Use awk to get only unique lines while preserving order (title first)
        echo "$output" | grep -A 60 "NotificationRecord.*pkg=${PACKAGE_NAME}" | \
            grep -E "android\.(title|text)=String" | \
            awk '!seen[$0]++' | \
            head -2
    fi
}

# Get the current state from notification title
get_state_from_notification() {
    local notif=$(get_notification_text)
    # Parse "Levin - <state>" from title
    # Format is: android.title=String (Levin - Downloading)
    echo "${notif}" | grep "android.title" | sed 's/.*Levin - //' | sed 's/)$//' | tr -d '\r'
}

# Wait for a specific state (with timeout)
wait_for_state() {
    local expected_state="$1"
    local timeout="${2:-30}"
    local waited=0
    
    while [[ $waited -lt $timeout ]]; do
        local current_state=$(get_state_from_notification 2>/dev/null || echo "unknown")
        if [[ "${current_state}" == *"${expected_state}"* ]]; then
            return 0
        fi
        sleep 1
        waited=$((waited + 1))
    done
    
    echo "Timeout waiting for state '${expected_state}'. Current: '$(get_state_from_notification)'"
    return 1
}

# Check if service is running
is_service_running() {
    adb_cmd shell "dumpsys activity services ${PACKAGE_NAME}" | grep -q "ServiceRecord"
}

# =============================================================================
# Battery Simulation
# =============================================================================

# Simulate AC power connected (charging)
simulate_ac_power() {
    echo "Simulating AC power connected..."
    adb_cmd shell "dumpsys battery set ac 1"
    adb_cmd shell "dumpsys battery set status 2"  # 2 = CHARGING
}

# Simulate battery power (unplugged)
simulate_battery_power() {
    echo "Simulating battery power (unplugged)..."
    adb_cmd shell "dumpsys battery set ac 0"
    adb_cmd shell "dumpsys battery set status 3"  # 3 = DISCHARGING
}

# Reset battery simulation
reset_battery_simulation() {
    echo "Resetting battery simulation..."
    adb_cmd shell "dumpsys battery reset"
}

# =============================================================================
# Network Simulation
# =============================================================================

# Disable WiFi
disable_wifi() {
    echo "Disabling WiFi..."
    adb_cmd shell "svc wifi disable"
}

# Enable WiFi
enable_wifi() {
    echo "Enabling WiFi..."
    adb_cmd shell "svc wifi enable"
    # Wait for WiFi to connect
    sleep 5
}

# Check if WiFi is connected
is_wifi_connected() {
    adb_cmd shell "dumpsys connectivity" | grep -q "WIFI.*CONNECTED"
}

# Enable mobile data only (disable WiFi)
enable_cellular_only() {
    disable_wifi
    adb_cmd shell "svc data enable"
}

# =============================================================================
# Settings Manipulation
# =============================================================================

# Set a shared preference value
# Note: This requires the app to read these at runtime
set_preference() {
    local key="$1"
    local value="$2"
    local prefs_file="/data/data/${PACKAGE_NAME}/shared_prefs/levin_preferences.xml"
    
    # This would need root access or the app to expose a way to change settings
    echo "Warning: Direct preference modification requires root or app support"
}

# Get app logcat output
get_app_logs() {
    local lines="${1:-100}"
    adb_cmd logcat -d -t "${lines}" --pid=$(adb_cmd shell pidof "${PACKAGE_NAME}" | tr -d '\r') 2>/dev/null || \
        adb_cmd logcat -d -t "${lines}" | grep -E "(LevinService|SessionManager|StorageMonitor|PowerMonitor)"
}

# Clear logcat
clear_logcat() {
    adb_cmd logcat -c
}

# =============================================================================
# Assertions
# =============================================================================

assert_contains() {
    local haystack="$1"
    local needle="$2"
    local message="${3:-String does not contain expected substring}"
    
    if [[ "${haystack}" != *"${needle}"* ]]; then
        echo "FAIL: ${message}"
        echo "  Expected to contain: ${needle}"
        echo "  Actual: ${haystack}"
        return 1
    fi
}

assert_equals() {
    local expected="$1"
    local actual="$2"
    local message="${3:-Values are not equal}"
    
    if [[ "${expected}" != "${actual}" ]]; then
        echo "FAIL: ${message}"
        echo "  Expected: ${expected}"
        echo "  Actual: ${actual}"
        return 1
    fi
}

assert_gt() {
    local actual="$1"
    local threshold="$2"
    local message="${3:-Value not greater than threshold}"
    
    if [[ ${actual} -le ${threshold} ]]; then
        echo "FAIL: ${message}"
        echo "  Expected > ${threshold}, got ${actual}"
        return 1
    fi
}

assert_lt() {
    local actual="$1"
    local threshold="$2"
    local message="${3:-Value not less than threshold}"
    
    if [[ ${actual} -ge ${threshold} ]]; then
        echo "FAIL: ${message}"
        echo "  Expected < ${threshold}, got ${actual}"
        return 1
    fi
}

# Print debug info on failure
print_debug_info() {
    echo "=== Debug Info ==="
    echo "Package: ${PACKAGE_NAME}"
    echo "Data size: $(get_device_data_size_mb) MB"
    echo "File count: $(count_device_files)"
    echo ""
    echo "=== Notification ==="
    get_notification_text || echo "(no notification)"
    echo ""
    echo "=== App Logs (last 50 lines) ==="
    get_app_logs 50
}
