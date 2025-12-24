package com.yoavmoshe.levin.monitoring

import android.os.FileObserver
import android.util.Log
import java.io.File

/**
 * Watch directory for new .torrent files
 * Much simpler than desktop version - Android's FileObserver is easier than Linux inotify
 */
class TorrentWatcher(
    private val watchDirectory: File,
    private val onTorrentAdded: (File) -> Unit
) {
    
    private var observer: FileObserver? = null
    private var isStarted = false
    
    companion object {
        private const val TAG = "TorrentWatcher"
    }
    
    /**
     * Start watching for new torrent files
     */
    fun start() {
        if (isStarted) {
            Log.w(TAG, "TorrentWatcher already started")
            return
        }
        
        Log.i(TAG, "Starting TorrentWatcher for: ${watchDirectory.absolutePath}")
        isStarted = true
        
        // Ensure directory exists
        if (!watchDirectory.exists()) {
            watchDirectory.mkdirs()
            Log.i(TAG, "Created watch directory")
        }
        
        // Create and start FileObserver
        observer = object : FileObserver(
            watchDirectory,
            CREATE or MOVED_TO
        ) {
            override fun onEvent(event: Int, path: String?) {
                if (path == null) return
                
                // Only process .torrent files
                if (!path.endsWith(".torrent", ignoreCase = true)) {
                    return
                }
                
                val file = File(watchDirectory, path)
                
                // Small delay to ensure file is fully written
                Thread.sleep(100)
                
                // Verify file is readable and non-empty
                if (file.exists() && file.canRead() && file.length() > 0) {
                    Log.i(TAG, "New torrent file detected: $path")
                    onTorrentAdded(file)
                } else {
                    Log.w(TAG, "Torrent file not ready: $path")
                }
            }
        }.also { it.startWatching() }
        
        Log.i(TAG, "TorrentWatcher started successfully")
    }
    
    /**
     * Stop watching
     */
    fun stop() {
        if (!isStarted) return
        
        Log.i(TAG, "Stopping TorrentWatcher")
        isStarted = false
        
        observer?.stopWatching()
        observer = null
    }
    
    /**
     * Scan for existing torrent files (call once on startup)
     */
    fun scanExisting(): List<File> {
        if (!watchDirectory.exists()) {
            watchDirectory.mkdirs()
            return emptyList()
        }
        
        val torrents = watchDirectory.listFiles { file ->
            file.isFile && file.extension.equals("torrent", ignoreCase = true)
        }?.toList() ?: emptyList()
        
        Log.i(TAG, "Scan found ${torrents.size} existing torrent files")
        return torrents
    }
}
