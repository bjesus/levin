package com.yoavmoshe.levin.monitoring

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.BatteryManager
import android.util.Log

/**
 * Monitor power state (AC/battery)
 * Replaces desktop power_monitor.cpp (which used DBus/UPower on Linux)
 */
class PowerMonitor(private val context: Context) {
    
    private var callback: ((Boolean) -> Unit)? = null
    private var isStarted = false
    
    companion object {
        private const val TAG = "PowerMonitor"
    }
    
    /**
     * BroadcastReceiver for power connection changes
     */
    private val powerReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            when (intent.action) {
                Intent.ACTION_POWER_CONNECTED -> {
                    Log.i(TAG, "Power connected (AC)")
                    callback?.invoke(true)
                }
                Intent.ACTION_POWER_DISCONNECTED -> {
                    Log.i(TAG, "Power disconnected (Battery)")
                    callback?.invoke(false)
                }
            }
        }
    }
    
    /**
     * Start monitoring power state
     * @param callback Called with true when on AC power, false when on battery
     */
    fun start(callback: (Boolean) -> Unit) {
        if (isStarted) {
            Log.w(TAG, "PowerMonitor already started")
            return
        }
        
        Log.i(TAG, "Starting PowerMonitor")
        this.callback = callback
        isStarted = true
        
        // Register receiver for power connection changes
        val filter = IntentFilter().apply {
            addAction(Intent.ACTION_POWER_CONNECTED)
            addAction(Intent.ACTION_POWER_DISCONNECTED)
        }
        context.registerReceiver(powerReceiver, filter)
        
        // Query and report initial state
        val isCharging = isCurrentlyCharging()
        Log.i(TAG, "Initial power state: ${if (isCharging) "AC" else "Battery"}")
        callback(isCharging)
    }
    
    /**
     * Stop monitoring
     */
    fun stop() {
        if (!isStarted) return
        
        Log.i(TAG, "Stopping PowerMonitor")
        isStarted = false
        
        try {
            context.unregisterReceiver(powerReceiver)
        } catch (e: IllegalArgumentException) {
            // Receiver not registered, ignore
            Log.w(TAG, "Receiver already unregistered")
        }
        
        callback = null
    }
    
    /**
     * Check if device is currently charging
     */
    private fun isCurrentlyCharging(): Boolean {
        val batteryStatus = IntentFilter(Intent.ACTION_BATTERY_CHANGED).let { filter ->
            context.registerReceiver(null, filter)
        }
        
        val status = batteryStatus?.getIntExtra(BatteryManager.EXTRA_STATUS, -1) ?: -1
        return status == BatteryManager.BATTERY_STATUS_CHARGING ||
               status == BatteryManager.BATTERY_STATUS_FULL
    }
    
    /**
     * Get current charging state (can be called without starting monitor)
     */
    fun isCharging(): Boolean = isCurrentlyCharging()
}
