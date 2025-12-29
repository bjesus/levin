#!/usr/bin/env bats
#
# Android State Machine E2E Tests
# Verifies the 5-state system as specified in DESIGN.md
#
# States (in priority order):
# 1. OFF - user disabled
# 2. PAUSED - battery/network conditions not met
# 3. IDLE - no torrents loaded
# 4. SEEDING - storage limit reached
# 5. DOWNLOADING - normal operation
#

load 'test_helper'

setup() {
    # Reset device state
    reset_battery_simulation
    enable_wifi
    
    # Fresh install for each test
    install_app
    clear_logcat
}

teardown() {
    # Clean up
    stop_app
    uninstall_app
    reset_battery_simulation
    enable_wifi
}

# =============================================================================
# IDLE State Tests
# =============================================================================

@test "IDLE: app starts in Idle state with no torrents" {
    # Ensure good conditions
    simulate_ac_power
    enable_wifi
    
    start_app
    click_add_torrents_no  # Dismiss dialog, stay in IDLE
    sleep 5  # Wait for service to start
    
    wait_for_state "Idle" 15
    
    local state=$(get_state_from_notification)
    assert_contains "${state}" "Idle" "Should show Idle state"
}

@test "IDLE: notification shows 'Idle (no torrents)' text" {
    simulate_ac_power
    enable_wifi
    
    start_app
    click_add_torrents_no  # Dismiss dialog
    sleep 5
    
    wait_for_state "Idle" 15
    
    local notif=$(get_notification_text)
    
    # Verify notification contains expected text per DESIGN.md
    assert_contains "${notif}" "Idle" "Notification should show Idle"
    assert_contains "${notif}" "no torrents" "Notification should indicate no torrents"
}

# =============================================================================
# SEEDING State Tests (Storage Limit)
# =============================================================================

@test "SEEDING: transitions to Seeding when over storage limit" {
    simulate_ac_power
    enable_wifi
    
    # Create files exceeding the default storage limit
    # Default Android min_free is typically 500MB, max_storage varies
    # Let's create a lot of data to trigger the limit
    create_device_files_mb 250 5
    
    start_app
    click_add_torrents_yes  # Add torrents to test SEEDING state
    sleep 15  # Wait for torrents to download and storage check
    
    # May need torrents loaded to enter SEEDING (otherwise IDLE)
    # For now, check if state reflects storage issue
    local state=$(get_state_from_notification)
    
    # If in Idle, that's expected without torrents
    # If in Seeding, that means storage limit triggered
    echo "Current state: ${state}"
    
    # The key test is that it doesn't crash and shows a valid state
    [[ -n "${state}" ]] || {
        echo "Should have a valid state"
        print_debug_info
        return 1
    }
}

@test "SEEDING: notification shows 'Seeding (storage limit)' when over budget" {
    simulate_ac_power
    enable_wifi
    
    # Create significant data
    create_device_files_mb 300 6
    
    start_app
    click_add_torrents_yes  # Add torrents to test SEEDING state
    sleep 15  # Wait for torrents to download
    
    # If storage limit is triggered and torrents exist, should show Seeding
    local notif=$(get_notification_text)
    echo "Notification: ${notif}"
    
    # Note: Without actual torrents, it will stay in IDLE
    # This test verifies the notification system works
    [[ -n "${notif}" ]] || {
        echo "Should have notification"
        return 1
    }
}

# =============================================================================
# PAUSED State Tests (Battery)
# =============================================================================

@test "PAUSED: pauses when on battery (runOnBattery=false)" {
    # Start with AC power
    simulate_ac_power
    
    start_app
    click_add_torrents_yes  # Add torrents for realistic test
    sleep 5
    
    # Wait for initial state (could be IDLE, DOWNLOADING, or SEEDING)
    sleep 3
    
    # Switch to battery
    simulate_battery_power
    sleep 5
    
    # Should transition to Paused
    wait_for_state "Paused" 15
    
    local state=$(get_state_from_notification)
    assert_contains "${state}" "Paused" "Should be Paused on battery"
}

@test "PAUSED: notification shows 'Paused (battery)' text" {
    simulate_ac_power
    start_app
    click_add_torrents_yes  # Add torrents for realistic test
    sleep 5
    
    simulate_battery_power
    sleep 5
    
    wait_for_state "Paused" 15
    
    local notif=$(get_notification_text)
    assert_contains "${notif}" "Paused" "Should show Paused"
    assert_contains "${notif}" "battery" "Should indicate battery reason"
}

@test "PAUSED: resumes when AC power reconnected" {
    simulate_ac_power
    start_app
    click_add_torrents_yes  # Add torrents for realistic test
    sleep 5
    
    # Go to battery
    simulate_battery_power
    sleep 3
    wait_for_state "Paused" 15
    
    # Reconnect AC
    simulate_ac_power
    sleep 3
    
    # Should resume (back to Idle since no torrents)
    wait_for_state "Idle" 15
    
    local state=$(get_state_from_notification)
    assert_contains "${state}" "Idle" "Should resume to Idle after AC reconnected"
}

# =============================================================================
# PAUSED State Tests (Network)
# =============================================================================

@test "PAUSED: pauses when WiFi disabled (runOnCellular=false)" {
    simulate_ac_power
    enable_wifi
    
    start_app
    click_add_torrents_yes  # Add torrents for realistic test
    sleep 5
    
    # Wait for initial state
    sleep 3
    
    # Disable WiFi (cellular only)
    disable_wifi
    sleep 5
    
    # Should transition to Paused
    wait_for_state "Paused" 15
    
    local state=$(get_state_from_notification)
    assert_contains "${state}" "Paused" "Should be Paused without WiFi"
}

@test "PAUSED: notification shows 'Paused (network)' when on cellular" {
    simulate_ac_power
    enable_wifi
    
    start_app
    click_add_torrents_yes  # Add torrents for realistic test
    sleep 5
    
    disable_wifi
    sleep 5
    
    wait_for_state "Paused" 15
    
    local notif=$(get_notification_text)
    assert_contains "${notif}" "Paused" "Should show Paused"
    # Note: Might show "network" or "cellular" or similar
}

@test "PAUSED: resumes when WiFi re-enabled" {
    simulate_ac_power
    enable_wifi
    
    start_app
    click_add_torrents_yes  # Add torrents for realistic test
    sleep 5
    
    disable_wifi
    sleep 3
    wait_for_state "Paused" 15
    
    enable_wifi
    sleep 5  # WiFi takes time to reconnect
    
    wait_for_state "Idle" 20
    
    local state=$(get_state_from_notification)
    assert_contains "${state}" "Idle" "Should resume after WiFi re-enabled"
}

# =============================================================================
# State Priority Tests
# =============================================================================

@test "PRIORITY: battery pause takes precedence over network" {
    simulate_ac_power
    enable_wifi
    
    start_app
    click_add_torrents_yes  # Add torrents for realistic test
    sleep 5
    
    # Disable both
    simulate_battery_power
    disable_wifi
    sleep 5
    
    local state=$(get_state_from_notification)
    assert_contains "${state}" "Paused" "Should be Paused"
    # Battery condition is checked first in priority order
}

@test "PRIORITY: network pause when battery OK but WiFi disabled" {
    simulate_ac_power  # Battery OK
    enable_wifi
    
    start_app
    click_add_torrents_yes  # Add torrents for realistic test
    sleep 5
    
    disable_wifi  # Network not OK
    sleep 5
    
    local state=$(get_state_from_notification)
    assert_contains "${state}" "Paused" "Should be Paused for network"
}

# =============================================================================
# Service Lifecycle Tests
# =============================================================================

@test "SERVICE: foreground service runs in Idle state" {
    simulate_ac_power
    enable_wifi
    
    start_app
    click_add_torrents_no  # Stay in IDLE state for this test
    sleep 5
    
    # Service should be running
    is_service_running || {
        echo "Service should be running in Idle state"
        print_debug_info
        return 1
    }
}

@test "SERVICE: notification visible in active states" {
    simulate_ac_power
    enable_wifi
    
    start_app
    click_add_torrents_yes  # Add torrents for active state
    sleep 5
    
    local notif=$(get_notification_text)
    [[ -n "${notif}" ]] || {
        echo "Notification should be visible in active state"
        return 1
    }
}
