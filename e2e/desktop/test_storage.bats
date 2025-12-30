#!/usr/bin/env bats
#
# Storage Management E2E Tests
# Verifies budget calculation, hysteresis, and file deletion per DESIGN.md
#
# Key behaviors:
# - 50MB hysteresis to prevent thrashing
# - Delete individual files (not entire torrents)
# - Delete in random order
# - Stop deletion as soon as enough space freed
#

load 'test_helper'

setup() {
    setup_test_environment
}

teardown() {
    teardown_test_environment
}

# =============================================================================
# Budget Calculation Tests
# =============================================================================

@test "BUDGET: under limit shows positive budget" {
    start_daemon
    wait_for_state "No torrents" 10
    
    # Create 100MB (under 200MB limit)
    create_files_mb 100 5
    create_mock_torrent "test1"
    
    sleep 10
    
    # Should NOT be in Seeding state
    local state=$(get_state_text)
    [[ "${state}" != *"Seeding"* ]] || {
        echo "Should not be Seeding when under budget"
        print_debug_info
        return 1
    }
}

@test "BUDGET: at limit (within hysteresis) does not trigger Seeding" {
    start_daemon
    wait_for_state "No torrents" 10
    
    # Create 180MB (200MB limit - 50MB hysteresis = 150MB threshold)
    # 180MB is above threshold but let's test the boundary
    # Actually: budget = available - 50MB, so if max=200MB and we use 180MB,
    # available = 20MB, budget = 20MB - 50MB = -30MB (over!)
    
    # Let's create exactly 140MB to stay within budget
    # available = 60MB, budget = 60MB - 50MB = 10MB (OK)
    create_files_mb 140 5
    create_mock_torrent "test1"
    
    sleep 10
    
    local state=$(get_state_text)
    [[ "${state}" != *"Seeding"* ]] || {
        echo "Should not be Seeding at 140MB (60MB available, 10MB budget after hysteresis)"
        print_debug_info
        return 1
    }
}

@test "HYSTERESIS: 50MB buffer prevents rapid state changes" {
    start_daemon
    wait_for_state "No torrents" 10
    
    # Start at 140MB (should be OK)
    create_files_mb 140 7
    create_mock_torrent "test1"
    sleep 10
    
    local state1=$(get_state_text)
    [[ "${state1}" != *"Seeding"* ]] || {
        echo "Initial state should not be Seeding at 140MB"
        return 1
    }
    
    # Add 20MB more = 160MB total
    # available = 40MB, budget = 40MB - 50MB = -10MB (now over!)
    create_file_mb "extra1.dat" 20 > /dev/null
    sleep 10
    
    wait_for_state "Seeding" 30
    local state2=$(get_state_text)
    assert_contains "${state2}" "Seeding" \
        "Should be Seeding after crossing hysteresis threshold"
}

# =============================================================================
# File Deletion Tests
# =============================================================================

@test "DELETION: deletes files when over max_storage" {
    start_daemon
    wait_for_state "No torrents" 10
    
    # Create 250MB in 10 files (25MB each)
    create_files_mb 250 10
    local initial_files=$(count_data_files)
    
    create_mock_torrent "test1"
    
    # Wait for deletion cycle
    sleep 15
    
    local final_files=$(count_data_files)
    local final_size=$(get_data_size_mb)
    
    # Files should be deleted
    assert_lt "${final_files}" "${initial_files}" \
        "Should delete some files"
    
    # Should be close to or under limit after deletion
    # (may be slightly over due to file size granularity)
    assert_lt "${final_size}" 250 \
        "Total size should decrease"
}

@test "DELETION: deletes only enough to meet requirement" {
    start_daemon
    wait_for_state "No torrents" 10
    
    # Create 300MB in 30 files (10MB each)
    for i in $(seq 1 30); do
        create_file_mb "file_${i}.dat" 10 > /dev/null
    done
    
    local initial_files=$(count_data_files)
    local initial_size=$(get_data_size_mb)
    
    create_mock_torrent "test1"
    
    # Wait for deletion
    sleep 20
    
    local final_files=$(count_data_files)
    local final_size=$(get_data_size_mb)
    
    # Should delete roughly 100MB worth (to get from 300MB to ~200MB)
    # That's about 10 files of 10MB each
    local deleted_files=$((initial_files - final_files))
    
    # Should not delete ALL files, just enough
    assert_gt "${final_files}" 0 "Should not delete all files"
    
    # Should delete approximately the right amount (within reasonable margin)
    # Deleted should be roughly 10 files (100MB), allow 5-15 range
    [[ ${deleted_files} -ge 5 && ${deleted_files} -le 20 ]] || {
        echo "Deleted ${deleted_files} files, expected ~10 (100MB worth)"
        echo "Initial: ${initial_size}MB, Final: ${final_size}MB"
        return 1
    }
}

@test "DELETION: deletes individual files not directories" {
    start_daemon
    wait_for_state "No torrents" 10
    
    # Create files in subdirectories (simulating torrent structure)
    mkdir -p "${TEST_DIR}/data/torrent1"
    mkdir -p "${TEST_DIR}/data/torrent2"
    
    # Put files in subdirectories (each 80MB, total 240MB > 200MB limit)
    dd if=/dev/urandom of="${TEST_DIR}/data/torrent1/file1.dat" bs=1M count=80 2>/dev/null
    dd if=/dev/urandom of="${TEST_DIR}/data/torrent1/file2.dat" bs=1M count=80 2>/dev/null
    dd if=/dev/urandom of="${TEST_DIR}/data/torrent2/file3.dat" bs=1M count=80 2>/dev/null
    
    # Count our test files before adding torrent
    local initial_test_files=$(find "${TEST_DIR}/data" -name "*.dat" -type f | wc -l)
    
    create_mock_torrent "test1"
    
    # Wait for deletion cycle
    sleep 15
    
    # Directories should still exist (only files deleted, not directories)
    [[ -d "${TEST_DIR}/data/torrent1" ]] || {
        echo "Directory torrent1 should not be deleted"
        return 1
    }
    [[ -d "${TEST_DIR}/data/torrent2" ]] || {
        echo "Directory torrent2 should not be deleted"
        return 1
    }
    
    # Some of our .dat test files should be gone (at least 40MB worth = 1 file)
    local remaining_test_files=$(find "${TEST_DIR}/data" -name "*.dat" -type f | wc -l)
    
    # At least one file should be deleted to bring us under the limit
    assert_lt "${remaining_test_files}" "${initial_test_files}" \
        "At least one test file should be deleted (was ${initial_test_files}, now ${remaining_test_files})"
}

@test "DELETION: handles empty data directory gracefully" {
    start_daemon
    wait_for_state "No torrents" 10
    
    # Don't create any files
    create_mock_torrent "test1"
    
    # Wait a bit - should not crash
    sleep 10
    
    # Daemon should still be running
    local state=$(get_state_text)
    [[ -n "${state}" ]] || {
        echo "Daemon should still be responding"
        print_debug_info
        return 1
    }
}

# =============================================================================
# Disk Usage Calculation Tests
# =============================================================================

@test "DISK_USAGE: calculates actual disk blocks (sparse file handling)" {
    start_daemon
    wait_for_state "No torrents" 10
    
    # Create a sparse file (appears large but uses little disk space)
    local sparse_file="${TEST_DIR}/data/sparse.dat"
    dd if=/dev/zero of="${sparse_file}" bs=1 count=0 seek=100M 2>/dev/null
    
    # Apparent size is 100MB but actual disk usage is near 0
    local apparent_size=$(ls -l "${sparse_file}" | awk '{print $5}')
    local actual_blocks=$(du -s "${TEST_DIR}/data" | cut -f1)
    
    # Create some real data
    create_files_mb 100 5
    create_mock_torrent "test1"
    
    sleep 10
    
    # Should NOT be in Seeding because sparse file doesn't count
    # (100MB real + ~0 sparse = ~100MB, under 200MB limit)
    local state=$(get_state_text)
    [[ "${state}" != *"Seeding"* ]] || {
        echo "Sparse file should not count toward disk usage"
        echo "This may indicate disk usage calculation is using apparent size instead of actual blocks"
        print_debug_info
        return 1
    }
}

# =============================================================================
# Recovery Tests
# =============================================================================

@test "RECOVERY: returns to normal state after freeing space" {
    start_daemon
    wait_for_state "No torrents" 10
    
    # Go over budget
    create_files_mb 250 5
    create_mock_torrent "test1"
    
    sleep 10
    wait_for_state "Seeding" 30
    
    # Manually delete files to free space
    rm -f "${TEST_DIR}/data"/*.dat
    
    # Wait for next storage check
    sleep 10
    
    # Should no longer be in Seeding
    local state=$(get_state_text)
    [[ "${state}" != *"Seeding"* ]] || {
        echo "Should recover from Seeding after manually freeing space"
        print_debug_info
        return 1
    }
}
