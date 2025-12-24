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
    val allowedStorageBytes: Long = 1L * 1024 * 1024 * 1024, // 1 GB default
    val minFreeSpaceBytes: Long = 100L * 1024 * 1024, // 100 MB minimum free
    
    // Power & Network
    val runOnBattery: Boolean = true,  // Changed default to true for better UX
    val runOnCellular: Boolean = true,  // Changed default to true for better UX
    
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
