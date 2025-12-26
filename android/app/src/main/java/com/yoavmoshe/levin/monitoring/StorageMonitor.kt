package com.yoavmoshe.levin.monitoring

import android.app.usage.StorageStatsManager
import android.content.Context
import android.os.Build
import android.os.StatFs
import android.os.storage.StorageManager
import android.os.storage.StorageVolume
import android.util.Log
import com.yoavmoshe.levin.data.LevinSettings
import java.io.File
import java.util.UUID
import java.util.concurrent.TimeUnit

/**
 * Monitor storage usage and enforce limits
 * Ported from desktop disk_monitor.cpp
 */
class StorageMonitor(
    private val context: Context,
    private val settings: LevinSettings
) {
    
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
        
        // Calculate actual space used by Levin (handles sparse files correctly)
        val usedByLevin = calculateActualDiskUsage(dataDir)
        
        // Calculate available space respecting minimum free
        val minRequired = settings.minFree
        val availableSpace = (freeBytes - minRequired).coerceAtLeast(0)
        
        // Calculate budget respecting both constraints
        val rawBudget = if (settings.maxStorage != null) {
            val availableForLevin = (settings.maxStorage - usedByLevin).coerceAtLeast(0)
            minOf(availableSpace, availableForLevin)
        } else {
            availableSpace
        }
        
        // Apply 50MB hysteresis to prevent download-delete thrashing
        val HYSTERESIS_BYTES = 50L * 1024 * 1024  // 50 MB
        val budget = if (rawBudget > HYSTERESIS_BYTES) {
            rawBudget - HYSTERESIS_BYTES
        } else if (rawBudget > 0) {
            0
        } else {
            rawBudget
        }
        
        // Determine if over budget
        val overBudget = when {
            budget <= 0 -> true
            settings.maxStorage != null && usedByLevin >= settings.maxStorage -> true
            freeBytes < minRequired -> true
            else -> false
        }
        
        val deficit = when {
            settings.maxStorage != null && usedByLevin > settings.maxStorage ->
                usedByLevin - settings.maxStorage
            freeBytes < minRequired ->
                minRequired - freeBytes
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
     * Calculate actual disk usage for external files directory
     * Uses StorageStatsManager API on Android 8.0+ for accurate stats (handles sparse files correctly)
     */
    private fun calculateActualDiskUsage(dir: File): Long {
        if (!dir.exists()) {
            Log.w(TAG, "Directory does not exist: ${dir.absolutePath}")
            return 0
        }
        
        Log.d(TAG, "Calculating disk usage for: ${dir.absolutePath}")
        
        // Use StorageStatsManager on API 26+ (Android 8.0+)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            try {
                val storageStatsManager = context.getSystemService(StorageStatsManager::class.java)
                val storageManager = context.getSystemService(StorageManager::class.java)
                
                // Get the storage volume for external files directory
                val storageVolume: StorageVolume? = storageManager?.getStorageVolume(dir)
                val uuid: UUID = if (storageVolume != null && storageVolume.uuid != null) {
                    UUID.fromString(storageVolume.uuid)
                } else {
                    StorageManager.UUID_DEFAULT
                }
                
                // Query external storage stats for our app
                val stats = storageStatsManager?.queryExternalStatsForUser(uuid, android.os.Process.myUserHandle())
                
                if (stats != null) {
                    Log.d(TAG, "External storage usage via StorageStatsManager: ${stats.totalBytes / (1024 * 1024)} MB")
                    return stats.totalBytes
                }
                
            } catch (e: Exception) {
                Log.e(TAG, "Failed to get storage stats via StorageStatsManager, falling back", e)
                // Fall through to fallback method
            }
        }
        
        // Fallback for API < 26 or if StorageStatsManager fails
        return calculateDiskUsageViaShell(dir)
    }
    
    /**
     * Fallback method using shell du command
     * Uses 'du -s' (without -b) to get actual block usage, not apparent size
     * Only used on API < 26 or if StorageStatsManager fails
     */
    private fun calculateDiskUsageViaShell(dir: File): Long {
        return try {
            // Use 'du -s' (without -b) to get actual disk blocks, not logical size
            // This handles sparse files correctly
            val process = Runtime.getRuntime().exec(arrayOf("du", "-s", dir.absolutePath))
            val output = process.inputStream.bufferedReader().use { it.readText() }
            val completed = process.waitFor(10, TimeUnit.SECONDS)
            
            if (!completed || process.exitValue() != 0) {
                Log.w(TAG, "du command failed or timed out")
                return 0L
            }
            
            // Parse: "12345\t/path" where 12345 is in KB
            val sizeInKB = output.trim().split(Regex("\\s+")).firstOrNull()?.toLongOrNull() ?: 0L
            val sizeInBytes = sizeInKB * 1024
            Log.d(TAG, "Disk usage via shell: ${sizeInBytes / (1024 * 1024)} MB")
            sizeInBytes
        } catch (e: Exception) {
            Log.e(TAG, "Failed to calculate disk usage via shell", e)
            0L
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
