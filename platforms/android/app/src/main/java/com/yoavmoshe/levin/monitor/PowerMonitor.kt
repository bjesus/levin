package com.yoavmoshe.levin.monitor

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.BatteryManager
import android.util.Log

/**
 * Monitors power state (AC vs battery) via BroadcastReceiver.
 *
 * Calls [onPowerChanged] with `true` when on AC power, `false` when on battery.
 */
class PowerMonitor(
    private val onPowerChanged: (onAcPower: Boolean) -> Unit
) : BroadcastReceiver() {

    companion object {
        private const val TAG = "PowerMonitor"
    }

    override fun onReceive(context: Context, intent: Intent) {
        val status = intent.getIntExtra(BatteryManager.EXTRA_STATUS, -1)
        val plugged = intent.getIntExtra(BatteryManager.EXTRA_PLUGGED, 0)

        val onAcPower = plugged == BatteryManager.BATTERY_PLUGGED_AC ||
                plugged == BatteryManager.BATTERY_PLUGGED_USB ||
                plugged == BatteryManager.BATTERY_PLUGGED_WIRELESS ||
                status == BatteryManager.BATTERY_STATUS_CHARGING ||
                status == BatteryManager.BATTERY_STATUS_FULL

        Log.d(TAG, "Power changed: onAcPower=$onAcPower (status=$status, plugged=$plugged)")
        onPowerChanged(onAcPower)
    }

    fun register(context: Context) {
        val filter = IntentFilter().apply {
            addAction(Intent.ACTION_BATTERY_CHANGED)
            addAction(Intent.ACTION_POWER_CONNECTED)
            addAction(Intent.ACTION_POWER_DISCONNECTED)
        }
        // ACTION_BATTERY_CHANGED is a sticky broadcast; registerReceiver returns
        // the current state immediately.
        val stickyIntent = context.registerReceiver(this, filter)
        if (stickyIntent != null) {
            onReceive(context, stickyIntent)
        }
    }

    fun unregister(context: Context) {
        try {
            context.unregisterReceiver(this)
        } catch (e: IllegalArgumentException) {
            Log.w(TAG, "Receiver was not registered", e)
        }
    }
}
