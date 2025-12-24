package com.yoavmoshe.levin.monitoring

import android.os.StatFs
import android.util.Log
import com.yoavmoshe.levin.data.LevinSettings
import java.io.File

/**
 * Monitor storage usage and enforce limits
 * Ported from desktop disk_monitor.cpp
 */
class StorageMonitor(private val settings: LevinSettings) {
    
    companion object {
        private const val TAG = "StorageMonitor"
    }
    
    /**
     * Storage status snapshot
     */
    data class StorageStatus(
        val totalBytes: Long,
        val freeBytes: Long,
        val usedByLevinBytes: Long,
        val budgetBytes: Long,
        val allowedBytes: Long
    ) {
        /**
         * Percentage of allowed storage used
         */
        val usagePercent: Float
            get() = if (allowedBytes > 0) {
                (usedByLevinBytes.toFloat() / allowedBytes.toFloat()) * 100f
            } else 0f
        
        /**
         * Is storage over budget?
         */
        val isOverBudget: Boolean
            get() = usedByLevinBytes >= allowedBytes || freeBytes < 100 * 1024 * 1024 // 100MB minimum
    }
    
    /**
     * Get current storage status
     */
    fun getStatus(): StorageStatus {
        val dataDir = settings.dataDirectory
        
        // Get filesystem stats
        val stat = StatFs(dataDir.path)
        val totalBytes = stat.totalBytes
        val freeBytes = stat.availableBytes
        
        // Calculate space used by Levin
        val usedByLevin = calculateDirectorySize(dataDir)
        
        // Calculate budget
        val allowedBytes = settings.allowedStorageBytes
        val minFreeBytes = settings.minFreeSpaceBytes
        val budgetBytes = minOf(
            allowedBytes - usedByLevin,
            freeBytes - minFreeBytes
        ).coerceAtLeast(0)
        
        return StorageStatus(
            totalBytes = totalBytes,
            freeBytes = freeBytes,
            usedByLevinBytes = usedByLevin,
            budgetBytes = budgetBytes,
            allowedBytes = allowedBytes
        )
    }
    
    /**
     * Check if there's space for a piece of given size
     */
    fun hasSpaceForPiece(pieceSize: Int): Boolean {
        val status = getStatus()
        return !status.isOverBudget && status.budgetBytes >= pieceSize
    }
    
    /**
     * Calculate total size of directory (recursively)
     */
    private fun calculateDirectorySize(dir: File): Long {
        if (!dir.exists()) {
            return 0
        }
        
        return try {
            dir.walkTopDown()
                .filter { it.isFile }
                .mapNotNull { file ->
                    try {
                        file.length()
                    } catch (e: SecurityException) {
                        Log.w(TAG, "Cannot read file size: ${file.path}")
                        null
                    }
                }
                .sum()
        } catch (e: Exception) {
            Log.e(TAG, "Error calculating directory size", e)
            0
        }
    }
    
    /**
     * Log current storage status
     */
    fun logStatus() {
        val status = getStatus()
        Log.i(TAG, "Storage: ${status.usedByLevinBytes / (1024 * 1024)} MB / " +
                  "${status.allowedBytes / (1024 * 1024)} MB " +
                  "(${String.format("%.1f", status.usagePercent)}%)")
    }
}
