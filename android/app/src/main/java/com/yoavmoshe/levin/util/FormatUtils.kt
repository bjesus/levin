package com.yoavmoshe.levin.util

import kotlin.time.Duration

/**
 * Utility functions for formatting numbers, bytes, and durations
 */
object FormatUtils {
    
    /**
     * Format bytes/second as human-readable speed
     */
    fun formatSpeed(bytesPerSecond: Long): String {
        return when {
            bytesPerSecond < 1024 -> "$bytesPerSecond B/s"
            bytesPerSecond < 1024 * 1024 -> "${bytesPerSecond / 1024} KB/s"
            bytesPerSecond < 1024 * 1024 * 1024 -> 
                String.format("%.1f MB/s", bytesPerSecond / (1024.0 * 1024.0))
            else -> 
                String.format("%.2f GB/s", bytesPerSecond / (1024.0 * 1024.0 * 1024.0))
        }
    }
    
    /**
     * Format bytes as human-readable size
     */
    fun formatSize(bytes: Long): String {
        return when {
            bytes < 0 -> "0 B"
            bytes < 1024 -> "$bytes B"
            bytes < 1024 * 1024 -> String.format("%.1f KB", bytes / 1024.0)
            bytes < 1024 * 1024 * 1024 -> 
                String.format("%.1f MB", bytes / (1024.0 * 1024.0))
            bytes < 1024L * 1024 * 1024 * 1024 ->
                String.format("%.2f GB", bytes / (1024.0 * 1024.0 * 1024.0))
            else ->
                String.format("%.2f TB", bytes / (1024.0 * 1024.0 * 1024.0 * 1024.0))
        }
    }
    
    /**
     * Format duration as human-readable time
     */
    fun formatDuration(duration: Duration): String {
        val days = duration.inWholeDays
        val hours = (duration.inWholeHours % 24)
        val minutes = (duration.inWholeMinutes % 60)
        
        return when {
            days > 0 -> "${days}d ${hours}h"
            hours > 0 -> "${hours}h ${minutes}m"
            else -> "${minutes}m"
        }
    }
    
    /**
     * Format ratio with 2 decimal places
     */
    fun formatRatio(ratio: Double): String {
        return String.format("%.2f", ratio)
    }
    
    /**
     * Format percentage
     */
    fun formatPercent(value: Float): String {
        return String.format("%.1f%%", value)
    }
}
