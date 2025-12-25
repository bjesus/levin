package com.yoavmoshe.levin.data

import android.content.Context
import java.io.File

/**
 * Levin app settings - mirrors desktop config.hpp structure
 */
data class LevinSettings(
    // Storage
    val dataDirectory: File,
    val watchDirectory: File,
    val minFree: Long = 1L * 1024 * 1024 * 1024, // REQUIRED - 1 GB default (minimum free space)
    val maxStorage: Long? = null, // OPTIONAL - null = unlimited (maximum storage Levin can use)
    
    // Power & Network
    val runOnStartup: Boolean = true,   // Run on device boot (default enabled)
    val runOnBattery: Boolean = false,  // Don't run on battery by default
    val runOnCellular: Boolean = false, // Don't run on cellular by default
    
    // BitTorrent settings (from desktop, not exposed in UI yet)
    val listenPort: Int = 6881,
    val maxConnections: Int = 200,
    val enableDht: Boolean = true,
    val enableUpnp: Boolean = true,
    val enableLsd: Boolean = true
) {
    companion object {
        /**
         * Create default settings for the app.
         * Uses app-specific external storage (no permissions needed on Android 10+)
         */
        fun default(context: Context): LevinSettings {
            val externalDir = context.getExternalFilesDir(null)
                ?: throw IllegalStateException("External storage not available")
            
            return LevinSettings(
                dataDirectory = File(externalDir, "data"),
                watchDirectory = File(externalDir, "torrents")
            )
        }
    }
    
    /**
     * Ensure directories exist
     */
    fun createDirectories() {
        dataDirectory.mkdirs()
        watchDirectory.mkdirs()
    }
}
