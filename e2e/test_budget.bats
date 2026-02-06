#!/usr/bin/env bats
# E2E tests for disk budget behavior: over-budget detection, file deletion,
# space recovery, and state transitions.

load helpers

# Helper: get a specific field from status output
get_field() {
    local field="$1"
    "$LEVIN_BIN" status 2>&1 | grep "$field" | head -1
}

@test "adding a torrent transitions to downloading state" {
    start_daemon

    # Copy a torrent file into the watch directory; the internal watcher
    # should detect it and add it to the session.
    cp "${BATS_TEST_DIRNAME}/../liblevin/tests/fixtures/test.torrent" "$WATCH_DIR/"

    # Give the daemon time to detect the file and transition
    sleep 3

    run levin_cmd status
    [ "$status" -eq 0 ]
    # With torrents present and not over budget, state should be downloading
    [[ "$output" == *"State:"*"downloading"* ]]
    [[ "$output" == *"Torrents:"*"1"* ]]
}

@test "over budget reports yes when max_storage is exceeded" {
    # Write a config with very low max_storage_bytes (1 byte)
    local config_dir="${XDG_CONFIG_HOME}/levin"
    cat > "${config_dir}/levin.toml" <<EOF
watch_directory = ${WATCH_DIR}
data_directory = ${DATA_DIR}
state_directory = ${STATE_DIR}
max_storage_bytes = 1
EOF

    # Create a file in the data directory to exceed budget
    dd if=/dev/zero of="${DATA_DIR}/dummy.bin" bs=1024 count=1 2>/dev/null

    start_daemon

    # Wait for disk check to fire (first tick)
    sleep 3

    run levin_cmd status
    [ "$status" -eq 0 ]
    [[ "$output" == *"Over budget:"*"yes"* ]]
}

@test "files deleted when over budget" {
    local config_dir="${XDG_CONFIG_HOME}/levin"
    cat > "${config_dir}/levin.toml" <<EOF
watch_directory = ${WATCH_DIR}
data_directory = ${DATA_DIR}
state_directory = ${STATE_DIR}
max_storage_bytes = 1
disk_check_interval_secs = 1
EOF

    # Create a file in data directory
    dd if=/dev/zero of="${DATA_DIR}/to_delete.bin" bs=4096 count=1 2>/dev/null
    [ -f "${DATA_DIR}/to_delete.bin" ]

    start_daemon

    # Wait for disk check and deletion
    sleep 5

    # The file should be deleted because we're over budget
    [ ! -f "${DATA_DIR}/to_delete.bin" ]
}

@test "torrent with over-budget transitions to seeding" {
    local config_dir="${XDG_CONFIG_HOME}/levin"
    cat > "${config_dir}/levin.toml" <<EOF
watch_directory = ${WATCH_DIR}
data_directory = ${DATA_DIR}
state_directory = ${STATE_DIR}
max_storage_bytes = 1
EOF

    start_daemon

    # Copy torrent after daemon is running so inotify catches it
    cp "${BATS_TEST_DIRNAME}/../liblevin/tests/fixtures/test.torrent" "$WATCH_DIR/"

    # Wait for watcher detection, disk check, and state transitions
    sleep 5

    run levin_cmd status
    [ "$status" -eq 0 ]

    # Should have the torrent registered
    [[ "$output" == *"Torrents:"*"1"* ]]

    # With max_storage_bytes=1 (essentially 0 budget) and a torrent,
    # state should be seeding (over budget prevents downloading)
    [[ "$output" == *"State:"*"seeding"* ]]
}
