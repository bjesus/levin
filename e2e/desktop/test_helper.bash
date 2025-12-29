#!/usr/bin/env bash
#
# Shared test utilities for Desktop E2E tests
#

# Create isolated test environment
setup_test_environment() {
    # Create unique test directory
    export TEST_DIR=$(mktemp -d "/tmp/levin-e2e-XXXXXX")
    export TEST_SOCKET="${TEST_DIR}/levin.sock"
    
    # Create subdirectories
    mkdir -p "${TEST_DIR}/torrents"
    mkdir -p "${TEST_DIR}/data"
    
    # Generate config from template
    local config_template="${E2E_DIR}/test_config.toml.example"
    export TEST_CONFIG="${TEST_DIR}/levin.toml"
    
    # Replace ${TEST_DIR} placeholder in config
    sed "s|\${TEST_DIR}|${TEST_DIR}|g" "${config_template}" > "${TEST_CONFIG}"
    
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

# Download a torrent for testing
# Uses Anna's Archive functionality to get real torrents
download_test_torrent() {
    local dest="${TEST_DIR}/torrents/test.torrent"
    
    # For now, create a minimal valid torrent file for testing
    # In production, this would use Anna's Archive API
    # We'll implement this properly in the integration
    
    echo "TODO: Download torrent from Anna's Archive"
    return 1
}

# Create a mock torrent file (for state testing without real downloads)
create_mock_torrent() {
    local name="${1:-test}"
    local dest="${TEST_DIR}/torrents/${name}.torrent"
    
    # Create a minimal bencoded torrent file
    # This won't actually download anything but will be loaded by libtorrent
    cat > "${dest}" << 'EOF'
d8:announce35:udp://tracker.example.com:6969/4:infod6:lengthi1024e4:name8:testfile12:piece lengthi16384e6:pieces20:xxxxxxxxxxxxxxxxxxxx7:privatei0eee
EOF
    
    echo "${dest}"
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
