package com.yoavmoshe.levin

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.util.Log
import com.yoavmoshe.levin.service.LevinService

/**
 * Starts the LevinService on device boot when "run on startup" is enabled.
 *
 * This receiver is disabled by default in the manifest and gets enabled/disabled
 * via PackageManager when the user toggles the setting.
 */
class BootReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        if (intent.action == Intent.ACTION_BOOT_COMPLETED) {
            Log.i("BootReceiver", "Boot completed, starting LevinService")
            LevinService.start(context)
        }
    }
}
