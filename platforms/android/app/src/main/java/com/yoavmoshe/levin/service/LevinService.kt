package com.yoavmoshe.levin.service

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.os.Handler
import android.os.HandlerThread
import android.os.IBinder
import android.os.Looper
import android.util.Log
import androidx.core.app.NotificationCompat
import com.yoavmoshe.levin.LevinNative
import com.yoavmoshe.levin.R
import com.yoavmoshe.levin.monitor.NetworkMonitor
import com.yoavmoshe.levin.monitor.PowerMonitor
import com.yoavmoshe.levin.monitor.StorageMonitor
import com.yoavmoshe.levin.ui.MainActivity

/**
 * Foreground service that runs the levin seeding engine.
 *
 * All levin_* calls are dispatched onto a dedicated HandlerThread so they
 * always execute on the same thread, matching the liblevin threading contract.
 */
class LevinService : Service() {

    companion object {
        private const val TAG = "LevinService"
        private const val CHANNEL_ID = "levin_service"
        private const val NOTIFICATION_ID = 1
        private const val TICK_INTERVAL_MS = 1000L
        private const val PREFS_NAME = "levin_prefs"

        const val ACTION_START = "com.yoavmoshe.levin.action.START"
        const val ACTION_STOP = "com.yoavmoshe.levin.action.STOP"

        /** Last status for UI polling. */
        @Volatile
        var lastStatus: LevinNative.StatusData? = null
            private set

        /** Whether the service is currently running. */
        @Volatile
        var isRunning = false
            private set

        fun start(context: Context) {
            val intent = Intent(context, LevinService::class.java).apply {
                action = ACTION_START
            }
            context.startForegroundService(intent)
        }

        fun stop(context: Context) {
            val intent = Intent(context, LevinService::class.java).apply {
                action = ACTION_STOP
            }
            context.startService(intent)
        }
    }

    private var levinHandle: Long = 0
    private lateinit var workerThread: HandlerThread
    private lateinit var workerHandler: Handler

    private lateinit var powerMonitor: PowerMonitor
    private lateinit var networkMonitor: NetworkMonitor
    private lateinit var storageMonitor: StorageMonitor

    private val tickRunnable = object : Runnable {
        override fun run() {
            if (levinHandle != 0L) {
                LevinNative.tick(levinHandle)
                lastStatus = LevinNative.getStatus(levinHandle)
                updateNotification()
            }
            workerHandler.postDelayed(this, TICK_INTERVAL_MS)
        }
    }

    override fun onCreate() {
        super.onCreate()
        createNotificationChannel()

        workerThread = HandlerThread("LevinWorker").also { it.start() }
        workerHandler = Handler(workerThread.looper)
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_STOP -> {
                stopSelf()
                return START_NOT_STICKY
            }
            else -> {
                startForeground(NOTIFICATION_ID, buildNotification("Starting..."))
                workerHandler.post { initLevin() }
            }
        }
        return START_STICKY
    }

    private fun initLevin() {
        if (levinHandle != 0L) return

        val prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE)
        val filesDir = filesDir.absolutePath
        val watchDir = "$filesDir/watch"
        val dataDir = "$filesDir/data"
        val stateDir = "$filesDir/state"

        val minFreeGb = prefs.getFloat("min_free_gb", 2.0f)
        val maxStorageGb = prefs.getFloat("max_storage_gb", 0.0f)
        val runOnBattery = prefs.getBoolean("run_on_battery", false)
        val runOnCellular = prefs.getBoolean("run_on_cellular", false)

        levinHandle = LevinNative.create(
            watchDir = watchDir,
            dataDir = dataDir,
            stateDir = stateDir,
            minFreeBytes = (minFreeGb * 1_073_741_824L).toLong(),
            minFreePercentage = 0.05,
            maxStorageBytes = if (maxStorageGb > 0f) (maxStorageGb * 1_073_741_824L).toLong() else 0L,
            runOnBattery = runOnBattery,
            runOnCellular = runOnCellular,
            diskCheckIntervalSecs = 60,
            maxDownloadKbps = 0,
            maxUploadKbps = 0
        )

        if (levinHandle == 0L) {
            Log.e(TAG, "Failed to create levin context")
            stopSelf()
            return
        }

        val rc = LevinNative.start(levinHandle)
        if (rc != 0) {
            Log.e(TAG, "Failed to start levin: $rc")
            LevinNative.destroy(levinHandle)
            levinHandle = 0
            stopSelf()
            return
        }

        LevinNative.setEnabled(levinHandle, true)

        // Start monitors
        startMonitors()

        // Begin tick loop
        isRunning = true
        workerHandler.post(tickRunnable)

        Log.i(TAG, "Levin service started")
    }

    private fun startMonitors() {
        powerMonitor = PowerMonitor { onAcPower ->
            workerHandler.post {
                if (levinHandle != 0L) {
                    LevinNative.updateBattery(levinHandle, onAcPower)
                }
            }
        }
        powerMonitor.register(this)

        networkMonitor = NetworkMonitor { hasWifi, hasCellular ->
            workerHandler.post {
                if (levinHandle != 0L) {
                    LevinNative.updateNetwork(levinHandle, hasWifi, hasCellular)
                }
            }
        }
        networkMonitor.register(this)

        storageMonitor = StorageMonitor(filesDir.absolutePath) { total, free ->
            workerHandler.post {
                if (levinHandle != 0L) {
                    LevinNative.updateStorage(levinHandle, total, free)
                }
            }
        }
        storageMonitor.start(workerHandler)
    }

    private fun stopMonitors() {
        if (::powerMonitor.isInitialized) powerMonitor.unregister(this)
        if (::networkMonitor.isInitialized) networkMonitor.unregister(this)
        if (::storageMonitor.isInitialized) storageMonitor.stop()
    }

    override fun onDestroy() {
        isRunning = false
        workerHandler.removeCallbacks(tickRunnable)

        workerHandler.post {
            stopMonitors()
            if (levinHandle != 0L) {
                LevinNative.stop(levinHandle)
                LevinNative.destroy(levinHandle)
                levinHandle = 0
            }
        }

        // Give the worker a moment to clean up, then quit
        workerThread.quitSafely()
        lastStatus = null

        Log.i(TAG, "Levin service destroyed")
        super.onDestroy()
    }

    override fun onBind(intent: Intent?): IBinder? = null

    // --- Notification ---

    private fun createNotificationChannel() {
        val channel = NotificationChannel(
            CHANNEL_ID,
            "Levin Seeding Service",
            NotificationManager.IMPORTANCE_LOW
        ).apply {
            description = "Background torrent seeding for Anna's Archive"
        }
        val nm = getSystemService(NotificationManager::class.java)
        nm.createNotificationChannel(channel)
    }

    private fun buildNotification(text: String): Notification {
        val pendingIntent = PendingIntent.getActivity(
            this, 0,
            Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )

        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle("Levin")
            .setContentText(text)
            .setSmallIcon(R.drawable.ic_notification)
            .setOngoing(true)
            .setContentIntent(pendingIntent)
            .build()
    }

    private fun updateNotification() {
        val status = lastStatus ?: return
        val text = "${status.stateName} | " +
                "Up: ${formatRate(status.uploadRate)} | " +
                "Down: ${formatRate(status.downloadRate)} | " +
                "Peers: ${status.peerCount}"
        val nm = getSystemService(NotificationManager::class.java)
        nm.notify(NOTIFICATION_ID, buildNotification(text))
    }

    private fun formatRate(bytesPerSec: Int): String {
        return when {
            bytesPerSec >= 1_048_576 -> "${bytesPerSec / 1_048_576} MB/s"
            bytesPerSec >= 1024 -> "${bytesPerSec / 1024} KB/s"
            else -> "$bytesPerSec B/s"
        }
    }
}
