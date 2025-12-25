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
        val minRequiredFreeBytes: Long,
        val maxAllowedBytes: Long?,  // null = unlimited
        val isOverBudget: Boolean,
        val deficitBytes: Long = 0
    )
    
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
        
        // Calculate available space respecting minimum free
        val minRequired = settings.minFree
        val availableSpace = (freeBytes - minRequired).coerceAtLeast(0)
        
        // Calculate budget respecting both constraints
        val budget = if (settings.maxStorage != null) {
            val availableForLevin = (settings.maxStorage - usedByLevin).coerceAtLeast(0)
            minOf(availableSpace, availableForLevin)
        } else {
            availableSpace
        }
        
        // Determine if over budget
        val overBudget = when {
            budget <= 0 -> true
            settings.maxStorage != null && usedByLevin >= settings.maxStorage -> true
            freeBytes < minRequired -> true
            usedByLevin > 0 && usedByLevin > budget -> true
            else -> false
        }
        
        val deficit = when {
            settings.maxStorage != null && usedByLevin > settings.maxStorage ->
                usedByLevin - settings.maxStorage
            freeBytes < minRequired ->
                minRequired - freeBytes
            usedByLevin > budget ->
                usedByLevin - budget
            else -> 0L
        }
        
        return StorageStatus(
            totalBytes = totalBytes,
            freeBytes = freeBytes,
            usedByLevinBytes = usedByLevin,
            budgetBytes = budget,
            minRequiredFreeBytes = minRequired,
            maxAllowedBytes = settings.maxStorage,
            isOverBudget = overBudget,
            deficitBytes = deficit
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
        val maxStr = if (status.maxAllowedBytes != null) {
            "${status.maxAllowedBytes / (1024 * 1024)} MB"
        } else {
            "unlimited"
        }
        Log.i(TAG, "Storage: ${status.usedByLevinBytes / (1024 * 1024)} MB used, " +
                  "Budget: ${status.budgetBytes / (1024 * 1024)} MB, " +
                  "Max: $maxStr, " +
                  "Min Free: ${status.minRequiredFreeBytes / (1024 * 1024)} MB")
    }
}
