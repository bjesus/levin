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
            CREATE or MOVED_TO or CLOSE_WRITE
        ) {
            override fun onEvent(event: Int, path: String?) {
                Log.d(TAG, "FileObserver event: ${eventToString(event)}, path=$path")
                
                if (path == null) return
                
                // Only process .torrent files
                if (!path.endsWith(".torrent", ignoreCase = true)) {
                    Log.d(TAG, "Ignoring non-torrent file: $path")
                    return
                }
                
                val file = File(watchDirectory, path)
                
                // Small delay to ensure file is fully written
                Thread.sleep(100)
                
                // Verify file is readable and non-empty
                if (file.exists() && file.canRead() && file.length() > 0) {
                    Log.i(TAG, "New torrent file detected: $path (${file.length()} bytes)")
                    onTorrentAdded(file)
                } else {
                    Log.w(TAG, "Torrent file not ready: $path (exists=${file.exists()}, canRead=${file.canRead()}, size=${file.length()})")
                }
            }
            
            private fun eventToString(event: Int): String {
                return when (event) {
                    CREATE -> "CREATE"
                    MOVED_TO -> "MOVED_TO"
                    CLOSE_WRITE -> "CLOSE_WRITE"
                    else -> "UNKNOWN($event)"
                }
            }
        }.also { 
            it.startWatching()
            Log.i(TAG, "FileObserver.startWatching() called")
        }
        
        Log.i(TAG, "TorrentWatcher started successfully, watching events: CREATE, MOVED_TO, CLOSE_WRITE")
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
