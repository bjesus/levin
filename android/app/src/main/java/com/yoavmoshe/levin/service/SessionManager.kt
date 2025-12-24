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
    
    var isPaused = false
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
        val currentSession = session ?: run {
            Log.e(TAG, "Cannot add torrent: session not started")
            return false
        }
        
        return try {
            Log.i(TAG, "Adding torrent: ${torrentFile.name}")
            
            // Load torrent info
            val ti = TorrentInfo(torrentFile)
            val infoHash = ti.infoHash().toHex()
            
            // Check if already added
            if (torrents.containsKey(infoHash)) {
                Log.w(TAG, "Torrent already added: ${torrentFile.name}")
                return false
            }
            
            // Add torrent to session with save path
            val handle = currentSession.download(ti, settings.dataDirectory)
            if (handle != null) {
                torrents[infoHash] = handle
            } else {
                Log.e(TAG, "Failed to get torrent handle")
                return false
            }
            
            Log.i(TAG, "Torrent added successfully: ${torrentFile.name}")
            true
        } catch (e: Exception) {
            Log.e(TAG, "Failed to add torrent: ${torrentFile.name}", e)
            false
        }
    }
    
    /**
     * Pause all torrents (battery/cellular mode)
     */
    fun pause() {
        Log.i(TAG, "Pausing session")
        isPaused = true
        session?.pause()
        torrents.values.forEach { it.pause() }
    }
    
    /**
     * Resume all torrents
     */
    fun resume() {
        Log.i(TAG, "Resuming session")
        isPaused = false
        session?.resume()
        torrents.values.forEach { it.resume() }
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
