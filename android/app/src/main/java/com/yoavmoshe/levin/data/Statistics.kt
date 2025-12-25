package com.yoavmoshe.levin.data

import kotlin.time.Duration
import kotlin.time.Duration.Companion.milliseconds

/**
 * Statistics tracking - ported from desktop statistics.cpp
 * Tracks both session (current run) and lifetime (all-time) stats
 */
data class Statistics(
    // Session stats (reset on app restart)
    val sessionDownloaded: Long = 0,
    val sessionUploaded: Long = 0,
    val sessionStartTime: Long = System.currentTimeMillis(),
    
    // Lifetime stats (persisted)
    val lifetimeDownloaded: Long = 0,
    val lifetimeUploaded: Long = 0,
    val lifetimeUptimeMs: Long = 0,
    
    // Current state
    val sessionDownloadRate: Long = 0, // bytes/sec
    val sessionUploadRate: Long = 0,   // bytes/sec
    val activeTorrents: Int = 0,
    val peerCount: Int = 0,
    val isPaused: Boolean = false,
    
    // Disk usage stats
    val diskUsedBytes: Long = 0,
    val diskFreeBytes: Long = 0,
    
    // Piece stats
    val piecesHave: Int = 0,
    val piecesTotal: Int = 0
) {
    /**
     * Session uptime
     */
    val sessionUptime: Duration
        get() = (System.currentTimeMillis() - sessionStartTime).milliseconds
    
    /**
     * Total lifetime uptime including current session
     */
    val totalUptime: Duration
        get() = (lifetimeUptimeMs + sessionUptime.inWholeMilliseconds).milliseconds
    
    /**
     * Total downloaded (lifetime + current session)
     */
    val totalDownloaded: Long
        get() = lifetimeDownloaded + sessionDownloaded
    
    /**
     * Total uploaded (lifetime + current session)
     */
    val totalUploaded: Long
        get() = lifetimeUploaded + sessionUploaded
    
    /**
     * Session ratio (uploaded / downloaded)
     */
    val sessionRatio: Double
        get() = if (sessionDownloaded > 0) {
            sessionUploaded.toDouble() / sessionDownloaded.toDouble()
        } else {
            0.0
        }
    
    /**
     * Lifetime ratio (total uploaded / total downloaded)
     */
    val lifetimeRatio: Double
        get() = if (totalDownloaded > 0) {
            totalUploaded.toDouble() / totalDownloaded.toDouble()
        } else {
            0.0
        }
    
    /**
     * Update with new session data from libtorrent
     */
    fun updateSession(
        downloaded: Long,
        uploaded: Long,
        downloadRate: Long,
        uploadRate: Long,
        torrents: Int,
        peers: Int,
        paused: Boolean,
        diskUsed: Long = diskUsedBytes,
        diskFree: Long = diskFreeBytes,
        piecesHave: Int = this.piecesHave,
        piecesTotal: Int = this.piecesTotal
    ): Statistics {
        return copy(
            sessionDownloaded = downloaded,
            sessionUploaded = uploaded,
            sessionDownloadRate = downloadRate,
            sessionUploadRate = uploadRate,
            activeTorrents = torrents,
            peerCount = peers,
            isPaused = paused,
            diskUsedBytes = diskUsed,
            diskFreeBytes = diskFree,
            piecesHave = piecesHave,
            piecesTotal = piecesTotal
        )
    }
    
    /**
     * Save session stats to lifetime on app shutdown
     */
    fun saveToLifetime(): Statistics {
        return copy(
            lifetimeDownloaded = lifetimeDownloaded + sessionDownloaded,
            lifetimeUploaded = lifetimeUploaded + sessionUploaded,
            lifetimeUptimeMs = lifetimeUptimeMs + sessionUptime.inWholeMilliseconds,
            sessionDownloaded = 0,
            sessionUploaded = 0,
            sessionStartTime = System.currentTimeMillis()
        )
    }
}
