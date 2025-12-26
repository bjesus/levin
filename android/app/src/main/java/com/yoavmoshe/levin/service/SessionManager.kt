package com.yoavmoshe.levin.service

import android.content.Context
import android.util.Log
import com.yoavmoshe.levin.data.LevinSettings
import org.libtorrent4j.AlertListener
import org.libtorrent4j.SessionManager
import org.libtorrent4j.SessionParams
import org.libtorrent4j.TorrentHandle
import org.libtorrent4j.TorrentInfo
import org.libtorrent4j.TorrentStatus
import org.libtorrent4j.alerts.Alert
import org.libtorrent4j.alerts.AlertType
import org.libtorrent4j.alerts.TorrentFinishedAlert
import org.libtorrent4j.swig.settings_pack
import java.io.File

/**
 * Manages libtorrent session
 * Ported from desktop session.cpp
 */
class LevinSessionManager(
    private val context: Context,
    private val settings: LevinSettings
) {
    
    private var session: SessionManager? = null
    private val torrents = mutableMapOf<String, TorrentHandle>()
    
    // Track added torrent files so we can re-add them after pause/resume
    private val addedTorrentFiles = mutableListOf<File>()
    
    var isPaused = false
        private set
    
    var isDownloadPaused = false
        private set
    
    companion object {
        private const val TAG = "LevinSessionManager"
    }
    
    /**
     * Session statistics
     */
    data class Stats(
        val downloadRate: Long = 0,
        val uploadRate: Long = 0,
        val totalDownloaded: Long = 0,
        val totalUploaded: Long = 0,
        val activeTorrents: Int = 0,
        val peerCount: Int = 0
    )
    
    /**
     * Start the libtorrent session
     */
    fun start() {
        if (session != null) {
            Log.w(TAG, "Session already started")
            return
        }
        
        Log.i(TAG, "Starting libtorrent session")
        
        // Create session parameters (ported from desktop session.cpp)
        val params = SessionParams()
        
        // Create session with default parameters for now
        // TODO: Configure settings pack when we understand the API better
        session = SessionManager().apply {
            // Add alert listener for torrent events
            addListener(object : AlertListener {
                override fun alert(alert: Alert<*>) {
                    handleAlert(alert)
                }
                
                override fun types(): IntArray? = null  // Receive all alert types
            })
            
            start()
        }
        
        Log.i(TAG, "libtorrent session started successfully")
    }
    
    /**
     * Add a torrent from file
     */
    fun addTorrent(torrentFile: File): Boolean {
        // Lazy-start session on first torrent add
        if (session == null) {
            Log.i(TAG, "First torrent detected, starting session now")
            start()
        }
        
        val currentSession = session ?: run {
            Log.e(TAG, "Cannot add torrent: session failed to start")
            return false
        }
        
        return try {
            Log.i(TAG, "Adding torrent: ${torrentFile.name}")
            
            // Load torrent info
            val ti = TorrentInfo(torrentFile)
            val infoHash = ti.infoHash().toHex()
            
            // Check if already added (only check if not paused, since pause clears torrents map)
            if (!isPaused && torrents.containsKey(infoHash)) {
                Log.w(TAG, "Torrent already added: ${torrentFile.name}")
                return false
            }
            
            // Add torrent to session with save path
            currentSession.download(ti, settings.dataDirectory)
            
            // Find the torrent handle by info hash
            val handle = currentSession.find(ti.infoHash())
            torrents[infoHash] = handle
            
            // Track this torrent file for pause/resume (avoid duplicates)
            if (!addedTorrentFiles.any { it.absolutePath == torrentFile.absolutePath }) {
                addedTorrentFiles.add(torrentFile)
                Log.d(TAG, "Tracking torrent file: ${torrentFile.name}")
            }
            
            Log.i(TAG, "Torrent added successfully: ${torrentFile.name}")
            true
        } catch (e: Exception) {
            Log.e(TAG, "Failed to add torrent: ${torrentFile.name}", e)
            false
        }
    }
    
    /**
     * Pause - completely stop the session to use 0% resources
     * All torrent state is lost but will be re-added on resume
     */
    fun pause() {
        if (isPaused) {
            Log.w(TAG, "Already paused")
            return
        }
        
        Log.i(TAG, "Pausing - stopping session completely for 0% resource usage")
        isPaused = true
        
        // Save resume data for all torrents before stopping
        torrents.values.forEach { handle ->
            try {
                if (handle.isValid && handle.needSaveResumeData()) {
                    handle.saveResumeData()
                }
            } catch (e: Exception) {
                Log.w(TAG, "Failed to save resume data during pause", e)
            }
        }
        
        // Completely stop the session to free all resources
        session?.stop()
        session = null
        torrents.clear()
        
        Log.i(TAG, "Session stopped - 0% resource usage")
    }
    
    /**
     * Resume - restart the session and re-add all torrents
     */
    fun resume() {
        if (!isPaused) {
            Log.w(TAG, "Already running")
            return
        }
        
        Log.i(TAG, "Resuming - restarting session and re-adding ${addedTorrentFiles.size} torrents")
        isPaused = false
        
        // Restart the session
        start()
        
        // Re-add all previously added torrents
        val torrentFilesToAdd = addedTorrentFiles.toList()  // Copy to avoid modification during iteration
        torrentFilesToAdd.forEach { torrentFile ->
            if (torrentFile.exists()) {
                addTorrent(torrentFile)
            } else {
                Log.w(TAG, "Torrent file no longer exists: ${torrentFile.name}")
                addedTorrentFiles.remove(torrentFile)
            }
        }
        
        Log.i(TAG, "Session resumed with ${torrents.size} torrents")
    }
    
    /**
     * Get current session statistics
     */
    fun getStats(): Stats {
        val currentSession = session ?: return Stats()
        
        val sessionStats = currentSession.stats()
        
        return Stats(
            downloadRate = sessionStats.downloadRate(),
            uploadRate = sessionStats.uploadRate(),
            totalDownloaded = sessionStats.totalDownload(),
            totalUploaded = sessionStats.totalUpload(),
            activeTorrents = torrents.size,
            peerCount = sessionStats.dhtNodes().toInt()  // Use DHT nodes as proxy for peer count
        )
    }
    
    /**
     * Get list of all torrents with their status
     */
    fun getTorrentStatuses(): List<TorrentStatus> {
        return torrents.values.mapNotNull { handle ->
            try {
                if (handle.isValid) {
                    handle.status()
                } else {
                    null
                }
            } catch (e: Exception) {
                Log.w(TAG, "Failed to get torrent status", e)
                null
            }
        }
    }
    
    /**
     * Pause downloads only (for storage limit) - uploads continue
     * Sets download rate limit to 1 byte/sec (effectively 0)
     */
    fun pauseDownloads() {
        if (isDownloadPaused) {
            Log.w(TAG, "Downloads already paused")
            return
        }
        
        val currentSession = session
        if (currentSession == null) {
            Log.w(TAG, "Cannot pause downloads: session not started")
            return
        }
        
        Log.i(TAG, "Pausing downloads only (uploads continue)")
        isDownloadPaused = true
        
        // Set download rate limit to 1 byte/sec (effectively stops downloads)
        // Upload limit remains unlimited
        val pack = settings_pack()
        pack.set_int(settings_pack.int_types.download_rate_limit.swigValue(), 1)
        currentSession.swig().apply_settings(pack)
        
        Log.i(TAG, "Downloads paused (rate limit = 1 byte/sec) - uploads still active")
    }
    
    /**
     * Resume downloads (for storage limit)
     */
    fun resumeDownloads() {
        if (!isDownloadPaused) {
            Log.w(TAG, "Downloads not paused")
            return
        }
        
        val currentSession = session
        if (currentSession == null) {
            Log.w(TAG, "Cannot resume downloads: session not started")
            return
        }
        
        Log.i(TAG, "Resuming downloads")
        isDownloadPaused = false
        
        // Set download rate limit back to unlimited (0 = unlimited in libtorrent)
        val pack = settings_pack()
        pack.set_int(settings_pack.int_types.download_rate_limit.swigValue(), 0)
        currentSession.swig().apply_settings(pack)
        
        Log.i(TAG, "Downloads resumed (rate limit = unlimited)")
    }
    
    /**
     * Remove torrent by its data directory
     * The directory name is expected to be the torrent info hash
     */
    fun removeTorrentByPath(dir: File) {
        // Directory name should match the info hash
        val dirName = dir.name
        val handle = torrents[dirName]
        
        if (handle != null) {
            Log.i(TAG, "Removing torrent from session: $dirName")
            try {
                session?.remove(handle)
                torrents.remove(dirName)
            } catch (e: Exception) {
                Log.e(TAG, "Error removing torrent", e)
            }
        }
    }
    
    /**
     * Stop the session and cleanup
     */
    fun stop() {
        Log.i(TAG, "Stopping session")
        
        // Save resume data for all torrents
        torrents.values.forEach { handle ->
            try {
                if (handle.isValid && handle.needSaveResumeData()) {
                    handle.saveResumeData()
                }
            } catch (e: Exception) {
                Log.w(TAG, "Failed to save resume data", e)
            }
        }
        
        // Stop session
        session?.stop()
        session = null
        torrents.clear()
        addedTorrentFiles.clear()
        
        Log.i(TAG, "Session stopped")
    }
    
    /**
     * Handle libtorrent alerts
     */
    private fun handleAlert(alert: Alert<*>) {
        when (alert.type()) {
            AlertType.TORRENT_FINISHED -> {
                val finishedAlert = alert as TorrentFinishedAlert
                val handle = finishedAlert.handle()
                val name = handle.torrentFile()?.name() ?: "unknown"
                Log.i(TAG, "Torrent finished: $name")
                // TODO: Optionally remove .torrent file from watch directory
            }
            
            AlertType.TORRENT_ERROR -> {
                Log.e(TAG, "Torrent error: ${alert.message()}")
            }
            
            else -> {
                // Log other alerts at debug level if needed
                // Log.d(TAG, "Alert: ${alert.type()} - ${alert.message()}")
            }
        }
    }
}
