package com.yoavmoshe.levin.service

import android.app.Service
import android.content.Intent
import android.os.IBinder

class LevinService : Service() {
    
    companion object {
        const val ACTION_START = "com.yoavmoshe.levin.START"
        const val ACTION_STOP = "com.yoavmoshe.levin.STOP"
        const val ACTION_PAUSE = "com.yoavmoshe.levin.PAUSE"
        const val ACTION_RESUME = "com.yoavmoshe.levin.RESUME"
    }
    
    override fun onCreate() {
        super.onCreate()
        // TODO: Initialize service components
    }
    
    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        // TODO: Handle service commands
        return START_STICKY
    }
    
    override fun onDestroy() {
        super.onDestroy()
        // TODO: Cleanup
    }
    
    override fun onBind(intent: Intent?): IBinder? = null
}
