package com.yoavmoshe.levin.state

import android.util.Log

/**
 * Manages Levin state transitions
 * Single source of truth for application state
 * Thread-safe, event-driven state machine
 * 
 * This logic is shared conceptually with desktop implementation
 */
class LevinStateManager(
    private val onStateChanged: (old: LevinState, new: LevinState) -> Unit
) {
    companion object {
        private const val TAG = "LevinStateManager"
    }
    
    private val lock = Any()
    private var currentState: LevinState = LevinState.OFF
    
    // Conditions (all start as "not restricting")
    private var userEnabled = false
    private var batteryAllows = true  // true = on AC OR runOnBattery enabled
    private var networkAllows = true  // true = on WiFi OR runOnCellular enabled
    private var hasTorrents = false
    private var storageAllows = true  // true = under storage limit
    
    /**
     * Get current state (thread-safe)
     */
    fun getState(): LevinState = synchronized(lock) { currentState }
    
    /**
     * User toggled enabled/disabled
     * This is the master on/off switch
     */
    fun setEnabled(enabled: Boolean) {
        synchronized(lock) {
            if (userEnabled != enabled) {
                Log.i(TAG, "User enabled: $userEnabled → $enabled")
                userEnabled = enabled
                recomputeState()
            }
        }
    }
    
    /**
     * Battery condition changed (Android only)
     * @param allows true if on AC power OR runOnBattery setting is true
     */
    fun updateBatteryCondition(allows: Boolean) {
        synchronized(lock) {
            if (batteryAllows != allows) {
                Log.d(TAG, "Battery condition: $batteryAllows → $allows")
                batteryAllows = allows
                recomputeState()
            }
        }
    }
    
    /**
     * Network condition changed (Android only)
     * @param allows true if on WiFi OR runOnCellular setting is true
     */
    fun updateNetworkCondition(allows: Boolean) {
        synchronized(lock) {
            if (networkAllows != allows) {
                Log.d(TAG, "Network condition: $networkAllows → $allows")
                networkAllows = allows
                recomputeState()
            }
        }
    }
    
    /**
     * Torrent availability changed
     * @param has true if there are torrents to download/seed
     */
    fun updateHasTorrents(has: Boolean) {
        synchronized(lock) {
            if (hasTorrents != has) {
                Log.d(TAG, "Has torrents: $hasTorrents → $has")
                hasTorrents = has
                recomputeState()
            }
        }
    }
    
    /**
     * Storage condition changed
     * @param allows true if under storage limit
     */
    fun updateStorageCondition(allows: Boolean) {
        synchronized(lock) {
            if (storageAllows != allows) {
                Log.d(TAG, "Storage condition: $storageAllows → $allows")
                storageAllows = allows
                recomputeState()
            }
        }
    }
    
    /**
     * Force state recomputation (call after settings reload)
     */
    fun recompute() {
        synchronized(lock) {
            Log.d(TAG, "Forcing state recomputation")
            recomputeState()
        }
    }
    
    /**
     * Core state machine logic
     * IMPORTANT: Must match desktop implementation exactly
     */
    private fun recomputeState() {
        val oldState = currentState
        val newState = determineState()
        
        if (oldState != newState) {
            Log.i(TAG, "STATE TRANSITION: $oldState → $newState [${getConditions()}]")
            currentState = newState
            
            // Notify outside synchronized block to avoid deadlocks
            try {
                onStateChanged(oldState, newState)
            } catch (e: Exception) {
                Log.e(TAG, "Error in state change callback", e)
            }
        }
    }
    
    /**
     * Determine target state based on conditions
     * This is the core state machine logic
     * 
     * State transition rules:
     * 1. User disabled → OFF
     * 2. Battery or network restriction → PAUSED
     * 3. No torrents → IDLE
     * 4. Storage limit reached → SEEDING
     * 5. All conditions met → DOWNLOADING
     */
    private fun determineState(): LevinState {
        // Rule 1: User disabled = OFF
        if (!userEnabled) {
            return LevinState.OFF
        }
        
        // Rule 2: Battery or network restriction = PAUSED
        if (!batteryAllows || !networkAllows) {
            return LevinState.PAUSED
        }
        
        // Rule 3: No torrents = IDLE
        if (!hasTorrents) {
            return LevinState.IDLE
        }
        
        // Rule 4: Storage limit reached = SEEDING (upload only)
        if (!storageAllows) {
            return LevinState.SEEDING
        }
        
        // Rule 5: All conditions met = DOWNLOADING
        return LevinState.DOWNLOADING
    }
    
    /**
     * Get current conditions for debugging
     */
    fun getConditions(): String {
        return synchronized(lock) {
            "enabled=$userEnabled, battery=$batteryAllows, network=$networkAllows, " +
            "torrents=$hasTorrents, storage=$storageAllows"
        }
    }
}
