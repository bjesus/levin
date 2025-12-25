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
        
        // Keys for session stats (current state)
        private const val KEY_SESSION_DOWNLOADED = "session_downloaded"
        private const val KEY_SESSION_UPLOADED = "session_uploaded"
        private const val KEY_SESSION_DOWNLOAD_RATE = "session_download_rate"
        private const val KEY_SESSION_UPLOAD_RATE = "session_upload_rate"
        private const val KEY_ACTIVE_TORRENTS = "active_torrents"
        private const val KEY_PEER_COUNT = "peer_count"
        private const val KEY_IS_PAUSED = "is_paused"
        private const val KEY_SESSION_START_TIME = "session_start_time"
        
        // Keys for disk usage stats
        private const val KEY_DISK_USED_BYTES = "disk_used_bytes"
        private const val KEY_DISK_FREE_BYTES = "disk_free_bytes"
        
        // Keys for piece stats
        private const val KEY_PIECES_HAVE = "pieces_have"
        private const val KEY_PIECES_TOTAL = "pieces_total"
    }
    
    /**
     * Load statistics from storage (both lifetime and session stats)
     */
    fun load(): Statistics {
        return Statistics(
            // Lifetime stats
            lifetimeDownloaded = prefs.getLong(KEY_LIFETIME_DOWNLOADED, 0),
            lifetimeUploaded = prefs.getLong(KEY_LIFETIME_UPLOADED, 0),
            lifetimeUptimeMs = prefs.getLong(KEY_LIFETIME_UPTIME_MS, 0),
            // Session stats (current state)
            sessionDownloaded = prefs.getLong(KEY_SESSION_DOWNLOADED, 0),
            sessionUploaded = prefs.getLong(KEY_SESSION_UPLOADED, 0),
            sessionDownloadRate = prefs.getLong(KEY_SESSION_DOWNLOAD_RATE, 0),
            sessionUploadRate = prefs.getLong(KEY_SESSION_UPLOAD_RATE, 0),
            activeTorrents = prefs.getInt(KEY_ACTIVE_TORRENTS, 0),
            peerCount = prefs.getInt(KEY_PEER_COUNT, 0),
            isPaused = prefs.getBoolean(KEY_IS_PAUSED, false),
            sessionStartTime = prefs.getLong(KEY_SESSION_START_TIME, System.currentTimeMillis()),
            // Disk usage stats
            diskUsedBytes = prefs.getLong(KEY_DISK_USED_BYTES, 0),
            diskFreeBytes = prefs.getLong(KEY_DISK_FREE_BYTES, 0),
            // Piece stats
            piecesHave = prefs.getInt(KEY_PIECES_HAVE, 0),
            piecesTotal = prefs.getInt(KEY_PIECES_TOTAL, 0)
        )
    }
    
    /**
     * Save statistics to storage (both lifetime and session stats)
     */
    fun save(stats: Statistics) {
        prefs.edit().apply {
            // Lifetime stats
            putLong(KEY_LIFETIME_DOWNLOADED, stats.lifetimeDownloaded)
            putLong(KEY_LIFETIME_UPLOADED, stats.lifetimeUploaded)
            putLong(KEY_LIFETIME_UPTIME_MS, stats.lifetimeUptimeMs)
            // Session stats (current state)
            putLong(KEY_SESSION_DOWNLOADED, stats.sessionDownloaded)
            putLong(KEY_SESSION_UPLOADED, stats.sessionUploaded)
            putLong(KEY_SESSION_DOWNLOAD_RATE, stats.sessionDownloadRate)
            putLong(KEY_SESSION_UPLOAD_RATE, stats.sessionUploadRate)
            putInt(KEY_ACTIVE_TORRENTS, stats.activeTorrents)
            putInt(KEY_PEER_COUNT, stats.peerCount)
            putBoolean(KEY_IS_PAUSED, stats.isPaused)
            putLong(KEY_SESSION_START_TIME, stats.sessionStartTime)
            // Disk usage stats
            putLong(KEY_DISK_USED_BYTES, stats.diskUsedBytes)
            putLong(KEY_DISK_FREE_BYTES, stats.diskFreeBytes)
            // Piece stats
            putInt(KEY_PIECES_HAVE, stats.piecesHave)
            putInt(KEY_PIECES_TOTAL, stats.piecesTotal)
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
