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
import com.yoavmoshe.levin.state.LevinState
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
     * Build notification with current stats based on state
     */
    fun buildNotification(
        state: LevinState,
        downloadRate: Long,
        uploadRate: Long,
        torrentCount: Int
    ): Notification {
        val title = state.notificationTitle()
        
        val text = when (state) {
            LevinState.OFF -> "Off"  // Should never show notification in OFF state
            LevinState.PAUSED -> "Paused"  // Should never show notification in PAUSED state
            LevinState.IDLE -> "No torrents"
            LevinState.SEEDING -> "⬆ ${FormatUtils.formatSpeed(uploadRate)} (storage limit reached)"
            LevinState.DOWNLOADING -> "⬇ ${FormatUtils.formatSpeed(downloadRate)}  ⬆ ${FormatUtils.formatSpeed(uploadRate)}  $torrentCount active"
        }
        
        // Intent to open main activity when notification is tapped
        val openIntent = Intent(context, MainActivity::class.java)
        val openPendingIntent = PendingIntent.getActivity(
            context,
            0,
            openIntent,
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT
        )
        
        return NotificationCompat.Builder(context, CHANNEL_ID)
            .setContentTitle(title)
            .setContentText(text)
            .setSmallIcon(R.drawable.ic_notification)
            .setOngoing(true)  // Can't be dismissed by swipe
            .setContentIntent(openPendingIntent)
            .setCategory(NotificationCompat.CATEGORY_SERVICE)
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .build()
    }
}
