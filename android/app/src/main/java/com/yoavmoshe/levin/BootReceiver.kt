package com.yoavmoshe.levin

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.util.Log
import com.yoavmoshe.levin.data.SettingsRepository
import com.yoavmoshe.levin.service.LevinService

/**
 * Broadcast receiver that starts Levin service on device boot
 * if "Run on Startup" is enabled in settings.
 */
class BootReceiver : BroadcastReceiver() {
    
    companion object {
        private const val TAG = "BootReceiver"
    }
    
    override fun onReceive(context: Context, intent: Intent) {
        if (intent.action == Intent.ACTION_BOOT_COMPLETED) {
            Log.d(TAG, "Boot completed, checking if should start Levin service")
            
            // Load settings
            val settingsRepo = SettingsRepository(context)
            val settings = settingsRepo.load()
            
            // Start service if enabled
            if (settings.runOnStartup) {
                Log.i(TAG, "Starting Levin service on boot")
                val serviceIntent = Intent(context, LevinService::class.java)
                context.startForegroundService(serviceIntent)
            } else {
                Log.d(TAG, "Run on startup is disabled, not starting service")
            }
        }
    }
}
