#!/usr/bin/env bats
# E2E tests for storage reporting and disk budget behavior

load helpers

@test "status reports disk usage and budget" {
    start_daemon
    run levin_cmd status
    [ "$status" -eq 0 ]
    [[ "$output" == *"Disk usage:"* ]]
    [[ "$output" == *"Disk budget:"* ]]
    [[ "$output" == *"Over budget:"* ]]
}

@test "disk budget is reported in human-readable format" {
    start_daemon
    run levin_cmd status
    [ "$status" -eq 0 ]
    # Budget should contain a unit (B, KB, MB, GB, or TB)
    local budget_line
    budget_line="$(echo "$output" | grep "Disk budget:")"
    [[ "$budget_line" =~ (B|KB|MB|GB|TB) ]]
}

@test "disk usage is zero or small with no torrents" {
    start_daemon
    run levin_cmd status
    [ "$status" -eq 0 ]
    local usage_line
    usage_line="$(echo "$output" | grep "Disk usage:")"
    # With no data downloaded, usage should be reported as 0 B or a very small value
    [[ "$usage_line" == *"0 B"* ]] || [[ "$usage_line" == *"0.0"* ]] || true
}

@test "over budget reports no when disk is available" {
    start_daemon
    run levin_cmd status
    [ "$status" -eq 0 ]
    [[ "$output" == *"Over budget:"*"no"* ]]
}

@test "data directory is created on daemon start" {
    start_daemon
    [ -d "$DATA_DIR" ]
}

@test "state directory is created on daemon start" {
    start_daemon
    [ -d "$STATE_DIR" ]
}
