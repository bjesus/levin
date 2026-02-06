#!/usr/bin/env bats
# Android E2E tests: app installation, service lifecycle, foreground service

load helpers

@test "APK exists and is a valid file" {
    [ -f "$APK_PATH" ]
}

@test "device is connected and authorized" {
    run adb devices
    [ "$status" -eq 0 ]
    [[ "$output" == *"device"* ]]
    # Should not contain "unauthorized" or "offline"
    ! echo "$output" | grep -qE "unauthorized|offline"
}

@test "app can be installed" {
    install_apk
    # Verify package is listed
    run adb shell pm list packages "$PKG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"$PKG"* ]]
}

@test "app is debuggable (run-as works)" {
    run run_as id
    [ "$status" -eq 0 ]
    [[ "$output" == *"$PKG"* ]] || [[ "$output" == *"uid="* ]]
}

@test "service starts as foreground service via activity" {
    start_service
    is_service_running

    # Check it's a foreground service via dumpsys
    local output
    output="$(adb shell dumpsys activity services "$SERVICE" 2>&1)"
    [[ "$output" == *"isForeground=true"* ]]
}

@test "service creates app directories on start" {
    start_service
    sleep 2

    app_dir_exists "$WATCH_DIR"
    app_dir_exists "$DATA_DIR"
    app_dir_exists "$STATE_DIR"
}

@test "levin_create and levin_start succeed (JNI logs)" {
    start_service
    sleep 2

    wait_for_log "levin_create succeeded"
    wait_for_log "levin_start: result=0"
}

@test "force-stop kills the service" {
    start_service
    is_service_running

    force_stop
    sleep 2
    ! is_service_running
}

@test "service can be restarted after force-stop" {
    start_service
    is_service_running

    force_stop
    sleep 2
    ! is_service_running

    start_service
    is_service_running
    wait_for_log "levin_start: result=0"
}

@test "activity can be launched" {
    run adb shell am start -n "$ACTIVITY"
    [ "$status" -eq 0 ]
    sleep 2

    # Check the activity is in the foreground
    local output
    output="$(adb shell dumpsys activity activities 2>&1)"
    echo "$output" | grep -q "MainActivity"
}

@test "service survives multiple start-stop cycles" {
    for i in 1 2 3; do
        start_service
        is_service_running
        sleep 2

        force_stop
        sleep 2
        ! is_service_running
    done
}
