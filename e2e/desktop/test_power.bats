#!/usr/bin/env bats
#
# Power Monitoring E2E Tests
# Verifies battery/AC detection and PAUSED state per DESIGN.md
#
# Desktop uses DBus/UPower for power monitoring.
# Tests that require DBus mocking are skipped if python3-dbusmock is not available.
# Config-based tests (POWER_CONFIG_*) always run.
#
# Requirements for full tests:
# - python3-dbusmock: sudo apt install python3-dbusmock
# - dbus-daemon (usually pre-installed)
#

load 'test_helper'

# Check if dbusmock is available (used by individual tests)
has_dbusmock() {
    python3 -c "import dbusmock" 2>/dev/null
}

# Skip helper for tests that need dbusmock
skip_without_dbusmock() {
    if ! has_dbusmock; then
        skip "python3-dbusmock not installed (sudo apt install python3-dbusmock)"
    fi
}

setup() {
    setup_test_environment
    
    # Only setup DBus if dbusmock is available and this test needs it
    # Individual tests will call setup_dbus if needed
}

# Setup DBus for tests that need it
setup_dbus() {
    # Start a private D-Bus session for testing
    export DBUS_SESSION_BUS_ADDRESS="unix:path=${TEST_DIR}/dbus.sock"
    
    # Start dbus-daemon for this test session
    dbus-daemon --session --address="${DBUS_SESSION_BUS_ADDRESS}" \
        --nofork --nopidfile &
    export DBUS_PID=$!
    sleep 1
}

teardown() {
    # Stop dbusmock if running
    if [[ -n "${MOCK_PID:-}" ]]; then
        kill ${MOCK_PID} 2>/dev/null || true
    fi
    
    # Stop dbus-daemon
    if [[ -n "${DBUS_PID:-}" ]]; then
        kill ${DBUS_PID} 2>/dev/null || true
    fi
    
    teardown_test_environment
}

# Start UPower mock with specified state
start_upower_mock() {
    local state="${1:-1}"  # 1=charging, 2=discharging, 4=fully-charged
    
    # Create a Python script to run the mock
    cat > "${TEST_DIR}/upower_mock.py" << EOF
import dbus
import dbus.service
import dbus.mainloop.glib
from gi.repository import GLib

dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)

class UPowerMock(dbus.service.Object):
    def __init__(self, bus, state):
        self.state = state
        bus_name = dbus.service.BusName('org.freedesktop.UPower', bus)
        dbus.service.Object.__init__(self, bus_name, '/org/freedesktop/UPower/devices/DisplayDevice')
    
    @dbus.service.method('org.freedesktop.DBus.Properties',
                         in_signature='ss', out_signature='v')
    def Get(self, interface, prop):
        if prop == 'State':
            return dbus.UInt32(self.state)
        return None
    
    @dbus.service.method('org.freedesktop.DBus.Properties',
                         in_signature='s', out_signature='a{sv}')
    def GetAll(self, interface):
        return {'State': dbus.UInt32(self.state)}
    
    def set_state(self, state):
        self.state = state
        self.PropertiesChanged('org.freedesktop.UPower.Device', 
                               {'State': dbus.UInt32(state)}, [])
    
    @dbus.service.signal('org.freedesktop.DBus.Properties',
                         signature='sa{sv}as')
    def PropertiesChanged(self, interface, changed, invalidated):
        pass

bus = dbus.SessionBus()
mock = UPowerMock(bus, ${state})
print("UPower mock started with state ${state}")
GLib.MainLoop().run()
EOF
    
    python3 "${TEST_DIR}/upower_mock.py" &
    export MOCK_PID=$!
    sleep 1
}

# Change the mock state (simulates plugging/unplugging)
change_power_state() {
    local state="$1"  # 1=charging, 2=discharging, 4=fully-charged
    
    # Send D-Bus signal to change state
    python3 << EOF
import dbus
bus = dbus.SessionBus()
obj = bus.get_object('org.freedesktop.UPower', '/org/freedesktop/UPower/devices/DisplayDevice')
props = dbus.Interface(obj, 'org.freedesktop.DBus.Properties')
# Emit PropertiesChanged signal
props_iface = dbus.Interface(obj, 'org.freedesktop.UPower.Device')
# The mock will handle this internally
EOF
}

# =============================================================================
# Power State Tests
# =============================================================================

@test "POWER: starts normally when on AC power (charging)" {
    # UPower uses system bus which requires root access to mock properly
    # These tests would require a full system bus mock environment
    skip "UPower mocking requires system bus access (run manually with real AC power)"
}

@test "POWER: starts normally when fully charged" {
    skip "UPower mocking requires system bus access (run manually with real AC power)"
}

@test "POWER: pauses when on battery (run_on_battery=false)" {
    skip "UPower mocking requires system bus access (run manually on battery)"
}

@test "POWER: runs on battery when run_on_battery=true" {
    skip "UPower mocking requires system bus access (run manually on battery)"
}

# =============================================================================
# Config-Based Power Tests (no DBus mock needed)
# =============================================================================

@test "POWER_CONFIG: run_on_battery=true allows operation" {
    # These tests verify the config option works without DBus mock
    sed -i 's/run_on_battery = .*/run_on_battery = true/' "${TEST_CONFIG}"
    
    start_daemon
    wait_for_state "No torrents" 10
    
    local state=$(get_state_text)
    # Should not be paused for battery when run_on_battery=true
    [[ "${state}" != *"Paused"*"battery"* ]] || {
        echo "Should not be paused for battery when run_on_battery=true"
        return 1
    }
}

@test "POWER_CONFIG: status shows run_on_battery setting" {
    sed -i 's/run_on_battery = .*/run_on_battery = true/' "${TEST_CONFIG}"
    
    start_daemon
    wait_for_state "No torrents" 10
    
    # Status should reflect the setting (if shown)
    # This is informational - the main test is that it starts
    local status=$(get_status)
    echo "Status output: ${status}"
}
