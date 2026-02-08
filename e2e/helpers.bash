# Common helpers for E2E tests
LEVIN_BIN="${LEVIN_BIN:-./build/platforms/linux/levin}"
TEST_BASE_DIR=""

setup() {
    TEST_BASE_DIR="$(mktemp -d)"
    export XDG_CONFIG_HOME="${TEST_BASE_DIR}/config"
    export XDG_CACHE_HOME="${TEST_BASE_DIR}/cache"
    export XDG_STATE_HOME="${TEST_BASE_DIR}/state"
    export XDG_RUNTIME_DIR="${TEST_BASE_DIR}/run"
    WATCH_DIR="${XDG_CONFIG_HOME}/levin/torrents"
    DATA_DIR="${XDG_CACHE_HOME}/levin/data"
    STATE_DIR="${XDG_STATE_HOME}/levin"
    RUN_DIR="${XDG_RUNTIME_DIR}/levin"
    mkdir -p "$WATCH_DIR" "$DATA_DIR" "$STATE_DIR" "$RUN_DIR"

    # Write a config file that forces the daemon to use our isolated dirs.
    # Without this, load_config() falls back to $HOME/.config/... defaults
    # and may pick up real torrent files from the host.
    local config_dir="${XDG_CONFIG_HOME}/levin"
    mkdir -p "$config_dir"
    cat > "${config_dir}/levin.toml" <<EOF
watch_directory = ${WATCH_DIR}
data_directory = ${DATA_DIR}
state_directory = ${STATE_DIR}
EOF
}

teardown() {
    # Stop daemon if running
    "$LEVIN_BIN" stop 2>/dev/null || true
    # Give it a moment to clean up
    sleep 1
    # Kill any lingering daemon processes from our runtime dir
    local pidfile="${TEST_BASE_DIR}/run/levin/levin.pid"
    if [ -f "$pidfile" ]; then
        local pid
        pid="$(cat "$pidfile" 2>/dev/null)" || true
        if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
            kill -9 "$pid" 2>/dev/null || true
        fi
    fi
    # Clean up temp dirs
    rm -rf "$TEST_BASE_DIR" 2>/dev/null || true
}

start_daemon() {
    "$LEVIN_BIN" start
    # The daemon double-forks, so 'start' returns immediately.
    # Wait for the PID file to appear first.
    local pidfile="${XDG_RUNTIME_DIR}/levin/levin.pid"
    local count=0
    while [ "$count" -lt 10 ]; do
        if [ -f "$pidfile" ]; then
            local pid
            pid="$(cat "$pidfile" 2>/dev/null)" || true
            if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
                break
            fi
        fi
        sleep 0.5
        count=$((count + 1))
    done
    if [ "$count" -ge 10 ]; then
        echo "Timed out waiting for PID file"
        return 1
    fi
    # Now wait for the IPC socket to become responsive.
    # With real libtorrent + WebRTC, session init takes longer than stub mode,
    # so the IPC socket may not be ready immediately after PID file appears.
    count=0
    while [ "$count" -lt 20 ]; do
        if "$LEVIN_BIN" status >/dev/null 2>&1; then
            return 0
        fi
        sleep 0.5
        count=$((count + 1))
    done
    echo "Timed out waiting for IPC socket to become responsive"
    return 1
}

levin_cmd() {
    "$LEVIN_BIN" "$@"
}

wait_for_state() {
    local target="$1"
    local timeout="${2:-10}"
    local count=0
    while [ "$count" -lt "$timeout" ]; do
        local output
        output="$("$LEVIN_BIN" status 2>&1)" || true
        if echo "$output" | grep -qi "State:.*${target}"; then
            return 0
        fi
        sleep 1
        count=$((count + 1))
    done
    echo "Timed out waiting for state: $target"
    echo "Last output: $output"
    return 1
}

get_state() {
    "$LEVIN_BIN" status 2>&1 | grep -i "State:" | awk '{print $2}'
}

pid_file() {
    echo "${XDG_RUNTIME_DIR}/levin/levin.pid"
}

daemon_pid() {
    cat "$(pid_file)" 2>/dev/null
}

is_daemon_running() {
    local pid
    pid="$(daemon_pid)" || return 1
    [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null
}
