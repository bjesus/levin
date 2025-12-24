package com.yoavmoshe.levin.service

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.os.Build
import androidx.core.app.NotificationCompat
import com.yoavmoshe.levin.R
import com.yoavmoshe.levin.ui.MainActivity
import com.yoavmoshe.levin.util.FormatUtils

/**
 * Helper for building service notifications
 */
class NotificationHelper(private val context: Context) {
    
    companion object {
        const val CHANNEL_ID = "levin_service_channel"
        const val NOTIFICATION_ID = 1
    }
    
    /**
     * Create notification channel (required for Android 8.0+)
     */
    fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                CHANNEL_ID,
                context.getString(R.string.notification_channel_name),
                NotificationManager.IMPORTANCE_LOW  // Low importance = no sound
            ).apply {
                description = context.getString(R.string.notification_channel_description)
                setShowBadge(false)
                enableVibration(false)
                setSound(null, null)
            }
            
            val manager = context.getSystemService(NotificationManager::class.java)
            manager?.createNotificationChannel(channel)
        }
    }
    
    /**
     * Build notification with current stats
     */
    fun buildNotification(
        downloadRate: Long,
        uploadRate: Long,
        torrentCount: Int,
        isPaused: Boolean
    ): Notification {
        val title = if (isPaused) {
            context.getString(R.string.notification_title_paused)
        } else {
            context.getString(R.string.notification_title_running)
        }
        
        val text = if (isPaused) {
            "Paused"
        } else {
            "⬇ ${FormatUtils.formatSpeed(downloadRate)}  $torrentCount active"
        }
        
        // Intent to open main activity when notification is tapped
        val openIntent = Intent(context, MainActivity::class.java)
        val openPendingIntent = PendingIntent.getActivity(
            context,
            0,
            openIntent,
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT
        )
        
        // Intent for pause/resume action
        val actionIntent = Intent(context, LevinService::class.java).apply {
            action = if (isPaused) LevinService.ACTION_RESUME else LevinService.ACTION_PAUSE
        }
        val actionPendingIntent = PendingIntent.getService(
            context,
            1,
            actionIntent,
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT
        )
        
        val actionText = if (isPaused) {
            context.getString(R.string.action_resume)
        } else {
            context.getString(R.string.action_pause)
        }
        
        // TODO: Add proper icons for pause/resume actions
        val actionIcon = if (isPaused) {
            android.R.drawable.ic_media_play
        } else {
            android.R.drawable.ic_media_pause
        }
        
        return NotificationCompat.Builder(context, CHANNEL_ID)
            .setContentTitle(title)
            .setContentText(text)
            .setSmallIcon(android.R.drawable.stat_sys_download)  // TODO: Add custom icon
            .setOngoing(true)  // Can't be dismissed by swipe
            .setContentIntent(openPendingIntent)
            .addAction(actionIcon, actionText, actionPendingIntent)
            .setCategory(NotificationCompat.CATEGORY_SERVICE)
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .build()
    }
}
