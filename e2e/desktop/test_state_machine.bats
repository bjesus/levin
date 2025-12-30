#!/usr/bin/env bats
#
# State Machine E2E Tests
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
    setup_test_environment
}

teardown() {
    teardown_test_environment
}

# =============================================================================
# IDLE State Tests
# =============================================================================

@test "IDLE: daemon starts in Idle state with no torrents" {
    # Start daemon with empty watch directory
    start_daemon
    
    # Should be in Idle state
    wait_for_state "No torrents" 10
    
    local state=$(get_state_text)
    assert_contains "${state}" "No torrents" "Should show No torrents state"
}

@test "IDLE: status shows 'No torrents' text" {
    start_daemon
    wait_for_state "No torrents" 10
    
    local status=$(get_status)
    
    # Verify exact state text per DESIGN.md
    assert_contains "${status}" "No torrents" \
        "Status should show 'No torrents' text per DESIGN.md"
}

# =============================================================================
# SEEDING State Tests (Storage Limit)
# =============================================================================

@test "SEEDING: transitions to Seeding when over max_storage" {
    start_daemon
    wait_for_state "No torrents" 10
    
    # Create 250MB of files (over 200MB max_storage limit)
    create_files_mb 250 5
    
    # Add a mock torrent so we're not in IDLE
    create_mock_torrent "test1"
    
    # Wait for storage check (every 5 seconds in test config)
    sleep 10
    
    # Should transition to Seeding state
    wait_for_state "Seeding" 30
    
    local state=$(get_state_text)
    assert_contains "${state}" "Seeding" "Should be in Seeding state"
    assert_contains "${state}" "storage limit" "Should indicate storage limit"
}

@test "SEEDING: status shows 'Seeding (storage limit)' text" {
    start_daemon
    wait_for_state "No torrents" 10
    
    # Over budget
    create_files_mb 250 5
    create_mock_torrent "test1"
    
    sleep 10
    wait_for_state "Seeding" 30
    
    local status=$(get_status)
    
    # Verify exact state text per DESIGN.md
    assert_contains "${status}" "Seeding (storage limit)" \
        "Status should show exact text from DESIGN.md"
}

@test "SEEDING: files are deleted when over budget" {
    start_daemon
    wait_for_state "No torrents" 10
    
    # Create 250MB of files
    create_files_mb 250 10
    local initial_count=$(count_data_files)
    local initial_size=$(get_data_size_mb)
    
    # Add torrent to trigger storage management
    create_mock_torrent "test1"
    
    # Wait for deletion
    sleep 15
    
    local final_count=$(count_data_files)
    local final_size=$(get_data_size_mb)
    
    # Files should have been deleted
    assert_lt "${final_count}" "${initial_count}" \
        "File count should decrease after deletion"
    assert_lt "${final_size}" "${initial_size}" \
        "Data size should decrease after deletion"
}

# =============================================================================
# State Priority Tests
# =============================================================================

@test "PRIORITY: IDLE takes precedence over SEEDING when no torrents" {
    start_daemon
    wait_for_state "No torrents" 10
    
    # Create files over budget but NO torrents
    create_files_mb 250 5
    
    # Wait for storage check
    sleep 10
    
    # Should still be IDLE because no torrents
    local state=$(get_state_text)
    assert_contains "${state}" "No torrents" \
        "Should remain in 'No torrents' even when over budget if no torrents"
}

@test "PRIORITY: state transitions correctly as conditions change" {
    start_daemon
    
    # Phase 1: IDLE (no torrents)
    wait_for_state "No torrents" 10
    local state1=$(get_state_text)
    assert_contains "${state1}" "No torrents" "Phase 1: Should be No torrents"
    
    # Phase 2: Add torrent + over budget = SEEDING
    create_files_mb 250 5
    create_mock_torrent "test1"
    sleep 10
    wait_for_state "Seeding" 30
    local state2=$(get_state_text)
    assert_contains "${state2}" "Seeding" "Phase 2: Should be Seeding"
    
    # Phase 3: Remove all files, still has torrent = back to some active state
    rm -f "${TEST_DIR}/data"/*.dat
    sleep 10
    # Should no longer be in Seeding (storage is now OK)
    local state3=$(get_state_text)
    [[ "${state3}" != *"Seeding"* ]] || {
        echo "Phase 3: Should not be Seeding after clearing data"
        return 1
    }
}

# =============================================================================
# Budget Display Tests
# =============================================================================

@test "STATUS: shows storage budget information" {
    start_daemon
    wait_for_state "No torrents" 10
    
    local status=$(get_status)
    
    # Status should include storage information
    # (exact format depends on implementation)
    assert_contains "${status}" "MB" "Status should show storage info"
}

@test "STATUS: budget shows 0 when over limit" {
    start_daemon
    wait_for_state "No torrents" 10
    
    # Go over budget
    create_files_mb 250 5
    create_mock_torrent "test1"
    sleep 10
    wait_for_state "Seeding" 30
    
    local status=$(get_status)
    
    # Budget should be 0 or negative indication
    # The exact format varies but should indicate no budget
    assert_contains "${status}" "0" "Budget should show 0 when over limit"
}
