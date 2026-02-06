package com.yoavmoshe.levin.monitor

import android.os.Handler
import android.os.StatFs
import android.util.Log

/**
 * Periodically checks storage via [StatFs] on the data directory.
 *
 * Calls [onStorageChanged] with (totalBytes, freeBytes).
 */
class StorageMonitor(
    private val path: String,
    private val onStorageChanged: (totalBytes: Long, freeBytes: Long) -> Unit
) {

    companion object {
        private const val TAG = "StorageMonitor"
        private const val CHECK_INTERVAL_MS = 30_000L // every 30 seconds
    }

    private var handler: Handler? = null
    private var running = false

    private val checkRunnable = object : Runnable {
        override fun run() {
            if (!running) return
            try {
                val stat = StatFs(path)
                val total = stat.totalBytes
                val free = stat.availableBytes
                Log.d(TAG, "Storage: total=${total / (1024*1024)}MB free=${free / (1024*1024)}MB")
                onStorageChanged(total, free)
            } catch (e: Exception) {
                Log.e(TAG, "Failed to check storage", e)
            }
            handler?.postDelayed(this, CHECK_INTERVAL_MS)
        }
    }

    fun start(handler: Handler) {
        this.handler = handler
        running = true
        handler.post(checkRunnable)
    }

    fun stop() {
        running = false
        handler?.removeCallbacks(checkRunnable)
        handler = null
    }
}
