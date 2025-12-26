#pragma once

#include <mutex>
#include <functional>

namespace levin {

/**
 * Levin application states
 * These states determine what the application does and what network activity is allowed
 */
enum class LevinState {
    OFF = 0,          // User disabled - no activity, no monitoring, no network
    PAUSED = 1,       // Waiting for conditions (battery/network) - monitoring active, no network
    IDLE = 2,         // No torrents - session running, monitoring active, no downloads/uploads
    SEEDING = 3,      // Storage limit reached - downloads paused, uploads continue
    DOWNLOADING = 4   // Normal operation - full network activity
};

/**
 * Convert state to string for logging
 */
inline const char* state_to_string(LevinState state) {
    switch (state) {
        case LevinState::OFF: return "OFF";
        case LevinState::PAUSED: return "PAUSED";
        case LevinState::IDLE: return "IDLE";
        case LevinState::SEEDING: return "SEEDING";
        case LevinState::DOWNLOADING: return "DOWNLOADING";
        default: return "UNKNOWN";
    }
}

/**
 * State machine for managing Levin application state
 * 
 * Single source of truth for application state
 * Thread-safe, event-driven state machine
 * 
 * State transitions are triggered by condition changes:
 * - User enable/disable (master on/off toggle)
 * - Battery condition (on AC power OR runOnBattery enabled)
 * - Network condition (always true on desktop, used on Android)
 * - Torrent availability (are there torrents to download/seed?)
 * - Storage condition (is storage under limit?)
 * 
 * Logic mirrors Android implementation in LevinStateManager.kt
 */
class StateMachine {
public:
    /**
     * Callback type for state change notifications
     * Called with (old_state, new_state) when state changes
     */
    using StateChangeCallback = std::function<void(LevinState old_state, LevinState new_state)>;
    
    /**
     * Create state machine with state change callback
     * Initial state is OFF
     */
    explicit StateMachine(StateChangeCallback callback);
    
    /**
     * Get current state (thread-safe)
     */
    LevinState get_state() const;
    
    /**
     * User toggled enabled/disabled (master on/off switch)
     * Desktop: daemon running = enabled (no separate flag needed)
     */
    void set_enabled(bool enabled);
    
    /**
     * Battery condition changed (Desktop: on AC power OR runOnBattery enabled)
     * @param allows true if battery condition allows running
     */
    void update_battery_condition(bool allows);
    
    /**
     * Network condition changed (Desktop: always true, Android: WiFi OR runOnCellular)
     * @param allows true if network condition allows running
     */
    void update_network_condition(bool allows);
    
    /**
     * Torrent availability changed
     * @param has true if there are torrents to download/seed
     */
    void update_has_torrents(bool has);
    
    /**
     * Storage condition changed
     * @param allows true if storage is under limit
     */
    void update_storage_condition(bool allows);
    
    /**
     * Force state recomputation
     * Useful after multiple condition updates
     */
    void recompute();
    
    /**
     * Check if state allows network activity
     */
    bool allows_network() const;
    
    /**
     * Check if state requires session to be running
     */
    bool requires_session() const;

private:
    mutable std::mutex mutex_;
    LevinState current_state_;
    StateChangeCallback callback_;
    
    // Conditions (all start as "not restricting")
    bool user_enabled_;
    bool battery_allows_;
    bool network_allows_;
    bool has_torrents_;
    bool storage_allows_;
    
    /**
     * Determine new state based on current conditions
     * This is the core state machine logic
     */
    LevinState determine_state() const;
    
    /**
     * Recompute state and trigger callback if changed
     */
    void recompute_state();
};

} // namespace levin
