#!/usr/bin/env bats
# Android E2E tests: state machine transitions, condition updates

load helpers

@test "service logs state transitions via JNI callback" {
    start_service
    sleep 3

    # The jni_state_callback should have logged at least one state change
    # (initial state evaluation when setEnabled(true) fires)
    wait_for_log "State changed"
}

@test "service starts with watcher on correct directory" {
    start_service
    sleep 3

    wait_for_log "starting watcher on:"
}

@test "network monitor reports initial state" {
    start_service
    sleep 3

    # NetworkMonitor should report the initial wifi/cellular state
    wait_for_log "Network changed:"
}

@test "adding torrent triggers state update in scan_existing" {
    # Pre-populate a torrent
    run_as mkdir -p "$WATCH_DIR" "$DATA_DIR" "$STATE_DIR"
    push_to_app "$FIXTURE_TORRENT" "$WATCH_DIR/state_test.torrent"

    start_service
    sleep 3

    # Verify torrent was picked up
    wait_for_log "scan_existing complete, torrent_count=1"

    # With a torrent present and enabled=true, state should transition
    # The exact state depends on network/battery: downloading (4) if OK, seeding/paused if not
    wait_for_log "State changed"
}

@test "service handles rapid start-stop gracefully" {
    start_service
    sleep 1
    force_stop
    sleep 1
    start_service
    sleep 2

    is_service_running
    wait_for_log "levin_start: result=0"
}
