package com.yoavmoshe.levin.state

/**
 * Levin operational states
 * Unified state machine for both Android and Desktop
 * 
 * State transitions:
 * OFF ←→ PAUSED (user toggle)
 * PAUSED ←→ IDLE (conditions met)
 * IDLE ←→ DOWNLOADING (torrents added/removed)
 * DOWNLOADING ←→ SEEDING (storage limit)
 */
enum class LevinState {
    /**
     * User disabled Levin
     * - No service running (desktop: daemon stopped)
     * - No monitoring
     * - No network activity
     * - No notification
     */
    OFF,
    
    /**
     * Waiting for operating conditions
     * - Monitoring active (to detect when conditions improve)
     * - Session stopped
     * - No network activity
     * - No notification
     * Conditions: Battery/Network restrictions active
     */
    PAUSED,
    
    /**
     * Ready but no torrents to process
     * - Monitoring active
     * - Session running (ready for torrents)
     * - Minimal network (DHT/tracker announces only)
     * - Notification shown: "No torrents"
     */
    IDLE,
    
    /**
     * Storage limit reached, seeding only
     * - Monitoring active
     * - Session running
     * - Uploads active, downloads paused
     * - Notification shown: "Seeding"
     */
    SEEDING,
    
    /**
     * Normal operation
     * - Monitoring active
     * - Session running
     * - Full network activity (downloads + uploads)
     * - Notification shown: "Downloading"
     */
    DOWNLOADING;
    
    /**
     * Check if state allows network activity
     */
    fun allowsNetwork(): Boolean = this in setOf(IDLE, SEEDING, DOWNLOADING)
    
    /**
     * Check if state should show notification
     */
    fun showsNotification(): Boolean = this in setOf(IDLE, SEEDING, DOWNLOADING)
    
    /**
     * Check if session should be running
     */
    fun requiresSession(): Boolean = this in setOf(IDLE, SEEDING, DOWNLOADING)
    
    /**
     * Get display name for UI
     */
    fun displayName(): String = when(this) {
        OFF -> "Off"
        PAUSED -> "Paused"
        IDLE -> "No torrents"
        SEEDING -> "Seeding"
        DOWNLOADING -> "Downloading"
    }
    
    /**
     * Get notification title
     */
    fun notificationTitle(): String = when(this) {
        IDLE -> "Levin - No torrents"
        SEEDING -> "Levin - Seeding"
        DOWNLOADING -> "Levin - Downloading"
        else -> "Levin"
    }
}
