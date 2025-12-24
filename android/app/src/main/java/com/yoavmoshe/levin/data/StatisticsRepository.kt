package com.yoavmoshe.levin.data

import android.content.Context
import android.content.SharedPreferences

/**
 * Repository for persisting statistics.
 * Ported from desktop statistics.cpp
 */
class StatisticsRepository(private val context: Context) {
    
    private val prefs: SharedPreferences =
        context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
    
    companion object {
        private const val PREFS_NAME = "levin_statistics"
        
        // Keys for lifetime stats
        private const val KEY_LIFETIME_DOWNLOADED = "lifetime_downloaded"
        private const val KEY_LIFETIME_UPLOADED = "lifetime_uploaded"
        private const val KEY_LIFETIME_UPTIME_MS = "lifetime_uptime_ms"
    }
    
    /**
     * Load statistics from storage
     */
    fun load(): Statistics {
        return Statistics(
            lifetimeDownloaded = prefs.getLong(KEY_LIFETIME_DOWNLOADED, 0),
            lifetimeUploaded = prefs.getLong(KEY_LIFETIME_UPLOADED, 0),
            lifetimeUptimeMs = prefs.getLong(KEY_LIFETIME_UPTIME_MS, 0)
        )
    }
    
    /**
     * Save statistics to storage (typically only lifetime stats)
     */
    fun save(stats: Statistics) {
        prefs.edit().apply {
            putLong(KEY_LIFETIME_DOWNLOADED, stats.lifetimeDownloaded)
            putLong(KEY_LIFETIME_UPLOADED, stats.lifetimeUploaded)
            putLong(KEY_LIFETIME_UPTIME_MS, stats.lifetimeUptimeMs)
            apply()
        }
    }
    
    /**
     * Save session stats to lifetime (called on shutdown)
     */
    fun saveSession(stats: Statistics) {
        val updated = stats.saveToLifetime()
        save(updated)
    }
}
