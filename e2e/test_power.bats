#!/usr/bin/env bats
# E2E tests for power state reporting
#
# On a desktop/CI machine without battery, the daemon assumes AC power.
# These tests verify the daemon correctly starts and reports status
# under that assumption.

load helpers

@test "daemon starts successfully on AC power" {
    start_daemon
    is_daemon_running
}

@test "daemon enters idle state on AC power with no torrents" {
    start_daemon
    run levin_cmd status
    [ "$status" -eq 0 ]
    # On AC power with no torrents, state should be idle (not off/paused)
    [[ "$output" == *"State:"*"idle"* ]]
}

@test "daemon does not enter off state on AC power" {
    start_daemon
    run levin_cmd status
    [ "$status" -eq 0 ]
    # State should NOT be "off" -- AC power means battery_ok is true
    local state
    state="$(echo "$output" | grep "State:" | awk '{print $2}')"
    [ "$state" != "off" ]
}

@test "daemon remains responsive after multiple status queries" {
    start_daemon
    for i in 1 2 3 4 5; do
        run levin_cmd status
        [ "$status" -eq 0 ]
        [[ "$output" == *"State:"* ]]
    done
}

@test "daemon survives pause/resume cycle on AC power" {
    start_daemon

    levin_cmd pause
    run levin_cmd status
    [ "$status" -eq 0 ]
    # Pause sets enabled=false -> state OFF (PAUSED is for battery/network)
    [[ "$output" == *"State:"*"off"* ]]

    levin_cmd resume
    run levin_cmd status
    [ "$status" -eq 0 ]
    # After resume on AC with no torrents, should be idle
    [[ "$output" == *"State:"*"idle"* ]]

    # Daemon should still be running
    is_daemon_running
}
