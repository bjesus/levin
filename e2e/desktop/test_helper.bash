#!/usr/bin/env bash
#
# Shared test utilities for Desktop E2E tests
#

# Create isolated test environment
setup_test_environment() {
    local config_template="${1:-test_config.toml.example}"
    local run_on_battery="${2:-true}"
    local max_storage_mb="${3:-200}"
    
    # Create a fresh test directory
    export TEST_DIR=$(mktemp -d /tmp/levin-e2e-XXXXXX)
    export TEST_DATA_DIR="${TEST_DIR}/data"
    export TEST_TORRENTS_DIR="${TEST_DIR}/torrents"
    export TEST_SOCKET="${TEST_DIR}/levin.sock"
    export TEST_CONFIG="${TEST_DIR}/levin.toml"
    
    # Create required directories
    mkdir -p "${TEST_DATA_DIR}"
    mkdir -p "${TEST_TORRENTS_DIR}"
    
    # Get script directory
    local SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
    
    # Generate config from template
    sed "s|\${TEST_DIR}|${TEST_DIR}|g" "${SCRIPT_DIR}/${config_template}" > "${TEST_CONFIG}"
    
    echo "Test environment: ${TEST_DIR}"
}

# Clean up test environment
teardown_test_environment() {
    # Stop daemon if running
    stop_daemon || true
    
    # Remove test directory
    if [[ -n "${TEST_DIR:-}" && -d "${TEST_DIR}" ]]; then
        rm -rf "${TEST_DIR}"
    fi
}

# Start the daemon in foreground (background process)
start_daemon() {
    local extra_args="${1:-}"
    
    "${LEVIN_BINARY}" start -c "${TEST_CONFIG}" -f ${extra_args} &
    export DAEMON_PID=$!
    
    # Wait for socket to be ready
    local max_wait=30
    local waited=0
    while [[ ! -S "${TEST_SOCKET}" && $waited -lt $max_wait ]]; do
        sleep 0.5
        waited=$((waited + 1))
        
        # Check if daemon crashed
        if ! kill -0 ${DAEMON_PID} 2>/dev/null; then
            echo "Daemon crashed during startup. Log:"
            cat "${TEST_DIR}/levin.log" || true
            return 1
        fi
    done
    
    if [[ ! -S "${TEST_SOCKET}" ]]; then
        echo "Timeout waiting for daemon socket"
        return 1
    fi
    
    echo "Daemon started (PID: ${DAEMON_PID})"
}

# Stop the daemon
stop_daemon() {
    if [[ -n "${DAEMON_PID:-}" ]]; then
        # Try graceful shutdown first
        "${LEVIN_BINARY}" terminate --socket "${TEST_SOCKET}" 2>/dev/null || true
        
        # Wait for process to exit
        local max_wait=10
        local waited=0
        while kill -0 ${DAEMON_PID} 2>/dev/null && [[ $waited -lt $max_wait ]]; do
            sleep 0.5
            waited=$((waited + 1))
        done
        
        # Force kill if still running
        if kill -0 ${DAEMON_PID} 2>/dev/null; then
            kill -9 ${DAEMON_PID} 2>/dev/null || true
        fi
        
        wait ${DAEMON_PID} 2>/dev/null || true
        unset DAEMON_PID
    fi
}

# Get daemon status
get_status() {
    "${LEVIN_BINARY}" status --socket "${TEST_SOCKET}" 2>/dev/null
}

# Get daemon status as JSON (for parsing)
get_status_json() {
    # The CLI outputs human-readable format, we need to parse it
    # For now, just return the text output
    get_status
}

# Get the current state text from status output
get_state_text() {
    get_status | head -1 | sed 's/^State: //'
}

# Wait for a specific state (with timeout)
wait_for_state() {
    local expected_state="$1"
    local timeout="${2:-30}"
    local waited=0
    
    while [[ $waited -lt $timeout ]]; do
        local current_state=$(get_state_text 2>/dev/null || echo "unknown")
        if [[ "${current_state}" == *"${expected_state}"* ]]; then
            return 0
        fi
        sleep 1
        waited=$((waited + 1))
    done
    
    echo "Timeout waiting for state '${expected_state}'. Current: '$(get_state_text)'"
    return 1
}

# Create a file of specified size (in MB)
create_file_mb() {
    local filename="$1"
    local size_mb="$2"
    local filepath="${TEST_DIR}/data/${filename}"
    
    dd if=/dev/urandom of="${filepath}" bs=1M count="${size_mb}" 2>/dev/null
    echo "${filepath}"
}

# Create multiple files totaling specified size
create_files_mb() {
    local total_mb="$1"
    local num_files="${2:-5}"
    local per_file_mb=$((total_mb / num_files))
    
    for i in $(seq 1 $num_files); do
        create_file_mb "testfile_${i}.dat" "${per_file_mb}" > /dev/null
    done
}

# Get total size of data directory (in bytes)
get_data_size() {
    du -sb "${TEST_DIR}/data" 2>/dev/null | cut -f1
}

# Get total size of data directory (in MB)
get_data_size_mb() {
    local bytes=$(get_data_size)
    echo $((bytes / 1024 / 1024))
}

# Count files in data directory
count_data_files() {
    find "${TEST_DIR}/data" -type f 2>/dev/null | wc -l
}

# Cache directory for downloaded torrents (shared across tests)
TORRENT_CACHE_DIR="/tmp/levin-e2e-torrent-cache"

# Ensure torrent cache exists and has at least one torrent
ensure_torrent_cache() {
    mkdir -p "${TORRENT_CACHE_DIR}"
    
    # Check if we already have torrents cached
    local torrent_count=$(find "${TORRENT_CACHE_DIR}" -name "*.torrent" -type f -size +0 2>/dev/null | wc -l)
    
    if [[ ${torrent_count} -ge 1 ]]; then
        return 0
    fi
    
    echo "Downloading torrents from Anna's Archive..."
    
    # Try to fetch torrent URLs from Anna's Archive with retries
    # Using -k flag to allow insecure connections (for corporate proxies/VPNs)
    local urls_response=""
    for attempt in 1 2 3; do
        urls_response=$(curl -skL --connect-timeout 15 --max-time 45 \
            "https://annas-archive.org/dyn/generate_torrents?max_tb=1&format=url" 2>/dev/null | head -5)
        if [[ -n "${urls_response}" ]]; then
            break
        fi
        echo "Attempt ${attempt} failed, retrying..."
        sleep 3
    done
    
    if [[ -z "${urls_response}" ]]; then
        echo "Failed to fetch torrent URLs from Anna's Archive"
        echo "Tests requiring torrents will be skipped"
        return 1
    fi
    
    # Download first few torrents
    local count=0
    while IFS= read -r url; do
        if [[ -z "${url}" || "${url}" != http* ]]; then
            continue
        fi
        
        local filename=$(basename "${url}")
        local dest="${TORRENT_CACHE_DIR}/${filename}"
        
        if curl -skL --connect-timeout 15 --max-time 90 -o "${dest}" "${url}" 2>/dev/null; then
            # Verify it's a valid torrent (starts with 'd' for bencode dict)
            if [[ -s "${dest}" && $(head -c1 "${dest}") == "d" ]]; then
                echo "Downloaded: ${filename}"
                count=$((count + 1))
            else
                rm -f "${dest}"
            fi
        fi
        
        # Stop after getting 3 torrents
        if [[ ${count} -ge 3 ]]; then
            break
        fi
    done <<< "${urls_response}"
    
    if [[ ${count} -eq 0 ]]; then
        echo "Failed to download any valid torrents"
        return 1
    fi
    
    echo "Cached ${count} torrents"
    return 0
}

# Copy a torrent from cache to the test watch directory
# Returns the path to the copied torrent
copy_cached_torrent() {
    local name="${1:-test}"
    local dest="${TEST_DIR}/torrents/${name}.torrent"
    
    # Ensure cache exists
    ensure_torrent_cache || return 1
    
    # Get first cached torrent
    local source=$(find "${TORRENT_CACHE_DIR}" -name "*.torrent" -type f 2>/dev/null | head -1)
    
    if [[ -z "${source}" ]]; then
        echo "No cached torrents available"
        return 1
    fi
    
    cp "${source}" "${dest}"
    echo "${dest}"
}

# Create a mock torrent file (for state testing without real downloads)
# This creates a properly formatted bencode torrent that libtorrent will accept
create_mock_torrent() {
    local name="${1:-test}"
    local dest="${TEST_DIR}/torrents/${name}.torrent"
    
    # Use a cached real torrent instead of a mock
    # This ensures libtorrent can actually load it
    copy_cached_torrent "${name}"
}

# Simulate AC power connected (for power monitoring tests)
# This requires dbus-send and UPower
simulate_ac_power() {
    # This would need to mock the DBus UPower interface
    # For now, we'll use the run_on_battery config option
    echo "Note: Power simulation requires DBus mock - using config override"
}

# Simulate battery power (unplugged)
simulate_battery_power() {
    echo "Note: Power simulation requires DBus mock - using config override"
}

# Assert that a condition is true
assert() {
    local condition="$1"
    local message="${2:-Assertion failed}"
    
    if ! eval "$condition"; then
        echo "FAIL: ${message}"
        echo "  Condition: ${condition}"
        return 1
    fi
}

# Assert string contains substring
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

# Assert string equals
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

# Assert numeric comparison
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

# Print test log on failure (for debugging)
print_debug_info() {
    echo "=== Debug Info ==="
    echo "Test directory: ${TEST_DIR}"
    echo "Data size: $(get_data_size_mb) MB"
    echo "File count: $(count_data_files)"
    echo ""
    echo "=== Daemon Log (last 50 lines) ==="
    tail -50 "${TEST_DIR}/levin.log" 2>/dev/null || echo "(no log)"
    echo ""
    echo "=== Status ==="
    get_status 2>/dev/null || echo "(daemon not running)"
}
