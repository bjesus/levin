package com.yoavmoshe.levin.ui

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import com.yoavmoshe.levin.service.LevinService

/**
 * Main activity - entry point of the app
 * For now, just starts the service and shows a simple UI
 */
class MainActivity : AppCompatActivity() {
    
    companion object {
        private const val REQUEST_NOTIFICATION_PERMISSION = 1
    }
    
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        
        // Simple temporary UI
        val textView = TextView(this).apply {
            text = buildString {
                appendLine("Levin - BitTorrent Archiver")
                appendLine()
                appendLine("Service is running in the background.")
                appendLine()
                appendLine("Check your notification panel for status.")
                appendLine()
                appendLine("Torrent files: /Android/data/com.yoavmoshe.levin/files/torrents/")
                appendLine("Downloaded data: /Android/data/com.yoavmoshe.levin/files/data/")
                appendLine()
                appendLine("Note: Full UI coming in Phase 5!")
            }
            textSize = 16f
            setPadding(32, 32, 32, 32)
        }
        
        setContentView(textView)
        
        // Request notification permission (Android 13+)
        requestNotificationPermission()
        
        // Start the service
        startLevinService()
    }
    
    private fun requestNotificationPermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            if (ContextCompat.checkSelfPermission(
                    this,
                    Manifest.permission.POST_NOTIFICATIONS
                ) != PackageManager.PERMISSION_GRANTED
            ) {
                ActivityCompat.requestPermissions(
                    this,
                    arrayOf(Manifest.permission.POST_NOTIFICATIONS),
                    REQUEST_NOTIFICATION_PERMISSION
                )
            }
        }
    }
    
    private fun startLevinService() {
        val intent = Intent(this, LevinService::class.java).apply {
            action = LevinService.ACTION_START
        }
        
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            startForegroundService(intent)
        } else {
            startService(intent)
        }
    }
}
