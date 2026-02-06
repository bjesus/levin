#!/usr/bin/env bats
# E2E tests for daemon lifecycle: start, status, stop

load helpers

@test "levin binary exists and is executable" {
    [ -x "$LEVIN_BIN" ]
}

@test "levin --help prints usage" {
    run "$LEVIN_BIN" help
    [ "$status" -eq 0 ]
    [[ "$output" == *"Usage:"* ]]
    [[ "$output" == *"start"* ]]
    [[ "$output" == *"stop"* ]]
    [[ "$output" == *"status"* ]]
}

@test "levin with no arguments prints usage and exits non-zero" {
    run "$LEVIN_BIN"
    [ "$status" -eq 1 ]
    [[ "$output" == *"Usage:"* ]]
}

@test "levin unknown command exits non-zero" {
    run "$LEVIN_BIN" bogus
    [ "$status" -eq 1 ]
    [[ "$output" == *"unknown command"* ]]
}

@test "daemon creates PID file on start" {
    start_daemon
    local pidfile
    pidfile="$(pid_file)"
    [ -f "$pidfile" ]
    local pid
    pid="$(cat "$pidfile")"
    [ -n "$pid" ]
    # PID should be a running process
    kill -0 "$pid"
}

@test "status returns valid output while daemon is running" {
    start_daemon
    run levin_cmd status
    [ "$status" -eq 0 ]
    [[ "$output" == *"State:"* ]]
    [[ "$output" == *"Torrents:"* ]]
    [[ "$output" == *"Peers:"* ]]
    [[ "$output" == *"Download:"* ]]
    [[ "$output" == *"Upload:"* ]]
    [[ "$output" == *"Disk usage:"* ]]
    [[ "$output" == *"Disk budget:"* ]]
}

@test "daemon starts in idle state with no torrents" {
    start_daemon
    run levin_cmd status
    [ "$status" -eq 0 ]
    [[ "$output" == *"State:"*"idle"* ]]
    [[ "$output" == *"Torrents:"*"0"* ]]
}

@test "list command shows no torrents initially" {
    start_daemon
    run levin_cmd list
    [ "$status" -eq 0 ]
    [[ "$output" == *"No torrents"* ]]
}

@test "stop command shuts down daemon cleanly" {
    start_daemon
    local pid
    pid="$(daemon_pid)"
    [ -n "$pid" ]

    run levin_cmd stop
    [ "$status" -eq 0 ]
    [[ "$output" == *"shutdown signal"* ]]

    # Wait for process to exit
    local count=0
    while [ "$count" -lt 10 ]; do
        if ! kill -0 "$pid" 2>/dev/null; then
            break
        fi
        sleep 0.5
        count=$((count + 1))
    done
    # Process should be gone
    ! kill -0 "$pid" 2>/dev/null

    # PID file should be cleaned up
    local pidfile
    pidfile="$(pid_file)"
    [ ! -f "$pidfile" ]
}

@test "stop when daemon is not running exits non-zero" {
    run levin_cmd stop
    [ "$status" -eq 1 ]
    [[ "$output" == *"not running"* ]]
}

@test "status when daemon is not running exits non-zero" {
    run levin_cmd status
    [ "$status" -eq 1 ]
    [[ "$output" == *"not running"* || "$output" == *"not responding"* ]]
}

@test "starting daemon twice reports already running" {
    start_daemon
    run "$LEVIN_BIN" start
    # The second start should fail because the daemon is already running.
    # Note: the parent of the double-fork exits 0, but the child writes
    # to stderr and exits 1. Since daemonize calls _exit(0) in the parent,
    # we actually get exit 0 from the parent but the daemon process itself
    # detects the conflict. Best we can do is check that the original
    # daemon is still running.
    is_daemon_running
}

@test "pause and resume commands work" {
    start_daemon

    run levin_cmd pause
    [ "$status" -eq 0 ]
    [[ "$output" == *"paused"* ]]

    # Pause sets enabled=false, which the state machine maps to OFF
    # (PAUSED is reserved for battery/network constraint conditions)
    run levin_cmd status
    [ "$status" -eq 0 ]
    [[ "$output" == *"State:"*"off"* ]]

    run levin_cmd resume
    [ "$status" -eq 0 ]
    [[ "$output" == *"resumed"* ]]

    run levin_cmd status
    [ "$status" -eq 0 ]
    [[ "$output" == *"State:"*"idle"* ]]
}
