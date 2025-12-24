package com.yoavmoshe.levin.service

import android.app.Notification
import android.app.NotificationManager
import android.app.Service
import android.content.Intent
import android.os.IBinder
import android.util.Log
import androidx.lifecycle.LifecycleService
import androidx.lifecycle.lifecycleScope
import com.yoavmoshe.levin.data.LevinSettings
import com.yoavmoshe.levin.data.SettingsRepository
import com.yoavmoshe.levin.data.Statistics
import com.yoavmoshe.levin.data.StatisticsRepository
import com.yoavmoshe.levin.monitoring.NetworkMonitor
import com.yoavmoshe.levin.monitoring.PowerMonitor
import com.yoavmoshe.levin.monitoring.StorageMonitor
import com.yoavmoshe.levin.monitoring.TorrentWatcher
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import java.io.File

/**
 * Foreground service that manages BitTorrent archiving
 * This is the core of the Android app - runs continuously in the background
 */
class LevinService : Service() {
    
    companion object {
        const val ACTION_START = "com.yoavmoshe.levin.START"
        const val ACTION_STOP = "com.yoavmoshe.levin.STOP"
        const val ACTION_PAUSE = "com.yoavmoshe.levin.PAUSE"
        const val ACTION_RESUME = "com.yoavmoshe.levin.RESUME"
        
        private const val TAG = "LevinService"
    }
    
    // Core components
    private lateinit var settings: LevinSettings
    private lateinit var settingsRepo: SettingsRepository
    private lateinit var statsRepo: StatisticsRepository
    private lateinit var sessionManager: LevinSessionManager
    private lateinit var notificationHelper: NotificationHelper
    
    // Monitoring components
    private lateinit var powerMonitor: PowerMonitor
    private lateinit var networkMonitor: NetworkMonitor
    private lateinit var storageMonitor: StorageMonitor
    private lateinit var torrentWatcher: TorrentWatcher
    
    // Current stats
    private var currentStats = Statistics()
    
    // State
    private var isRunning = false
    
    override fun onCreate() {
        super.onCreate()
        Log.i(TAG, "Service onCreate")
        
        // Initialize repositories
        settingsRepo = SettingsRepository(this)
        statsRepo = StatisticsRepository(this)
        
        // Load settings and stats
        settings = settingsRepo.load()
        currentStats = statsRepo.load()
        
        // Ensure directories exist
        settings.createDirectories()
        
        // Initialize notification helper
        notificationHelper = NotificationHelper(this)
        notificationHelper.createNotificationChannel()
        
        // Initialize session manager
        sessionManager = LevinSessionManager(this, settings)
        
        // Initialize monitors
        powerMonitor = PowerMonitor(this)
        networkMonitor = NetworkMonitor(this)
        storageMonitor = StorageMonitor(settings)
        torrentWatcher = TorrentWatcher(settings.watchDirectory) { file ->
            sessionManager.addTorrent(file)
        }
    }
    
    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        Log.i(TAG, "onStartCommand: action=${intent?.action}")
        
        when (intent?.action) {
            ACTION_START -> handleStart()
            ACTION_STOP -> handleStop()
            ACTION_PAUSE -> handlePause()
            ACTION_RESUME -> handleResume()
            else -> {
                // Default to START if no action specified
                if (!isRunning) {
                    handleStart()
                }
            }
        }
        
        return START_STICKY  // Restart service if killed by system
    }
    
    private fun handleStart() {
        if (isRunning) {
            Log.w(TAG, "Service already running")
            return
        }
        
        Log.i(TAG, "Starting Levin service")
        isRunning = true
        
        // Start as foreground service (required for background operation)
        val notification = buildNotification()
        startForeground(NotificationHelper.NOTIFICATION_ID, notification)
        
        // Start libtorrent session
        sessionManager.start()
        
        // Scan for existing torrents and add them
        scanAndAddTorrents()
        
        // Start power monitor
        powerMonitor.start { isCharging ->
            onPowerStateChanged(isCharging)
        }
        
        // Start network monitor
        networkMonitor.start { isWifi, isCellular ->
            onNetworkTypeChanged(isWifi, isCellular)
        }
        
        // Start file watcher
        torrentWatcher.start()
        
        // Log initial storage status
        storageMonitor.logStatus()
        
        // Start periodic updates (stats + notification)
        startPeriodicUpdates()
        
        Log.i(TAG, "Levin service started successfully")
    }
    
    private fun handleStop() {
        Log.i(TAG, "Stopping service")
        
        isRunning = false
        
        // Stop monitors
        powerMonitor.stop()
        networkMonitor.stop()
        torrentWatcher.stop()
        
        // Save session stats to lifetime
        statsRepo.saveSession(currentStats)
        
        // Stop session
        sessionManager.stop()
        
        // Stop foreground and remove notification
        stopForeground(STOP_FOREGROUND_REMOVE)
        stopSelf()
    }
    
    private fun handlePause() {
        Log.i(TAG, "Pausing downloads")
        sessionManager.pause()
        updateNotification()
    }
    
    private fun handleResume() {
        Log.i(TAG, "Resuming downloads")
        sessionManager.resume()
        updateNotification()
    }
    
    private fun scanAndAddTorrents() {
        val existingTorrents = torrentWatcher.scanExisting()
        Log.i(TAG, "Found ${existingTorrents.size} existing torrent files")
        existingTorrents.forEach { file ->
            sessionManager.addTorrent(file)
        }
    }
    
    private fun startPeriodicUpdates() {
        // TODO: Use coroutines when we switch to LifecycleService
        // For now, use a simple thread
        Thread {
            while (isRunning) {
                try {
                    updateStats()
                    updateNotification()
                    Thread.sleep(1000)  // Update every second
                } catch (e: InterruptedException) {
                    break
                } catch (e: Exception) {
                    Log.e(TAG, "Error in periodic update", e)
                }
            }
        }.start()
    }
    
    private fun updateStats() {
        val sessionStats = sessionManager.getStats()
        
        // Check storage limits
        val storageStatus = storageMonitor.getStatus()
        if (storageStatus.isOverBudget) {
            Log.w(TAG, "Storage budget exceeded: ${storageStatus.usagePercentage}%")
            // TODO: Pause downloads or delete old torrents
        }
        
        // Update current stats
        currentStats = currentStats.updateSession(
            downloaded = sessionStats.totalDownloaded,
            uploaded = sessionStats.totalUploaded,
            downloadRate = sessionStats.downloadRate,
            uploadRate = sessionStats.uploadRate,
            torrents = sessionStats.activeTorrents,
            peers = sessionStats.peerCount,
            paused = sessionManager.isPaused
        )
        
        // Save to repository (every update - StatisticsRepository handles throttling if needed)
        statsRepo.save(currentStats)
    }
    
    private fun updateNotification() {
        if (!isRunning) return
        
        val notification = buildNotification()
        val notificationManager = getSystemService(NotificationManager::class.java)
        notificationManager?.notify(NotificationHelper.NOTIFICATION_ID, notification)
    }
    
    private fun onPowerStateChanged(isCharging: Boolean) {
        if (!settings.runOnBattery && !isCharging) {
            Log.i(TAG, "On battery - pausing downloads")
            handlePause()
        } else if (isCharging && sessionManager.isPaused) {
            Log.i(TAG, "On AC power - resuming downloads")
            handleResume()
        }
    }
    
    private fun onNetworkTypeChanged(isWifi: Boolean, isCellular: Boolean) {
        if (!settings.runOnCellular && isCellular) {
            Log.i(TAG, "On cellular - pausing downloads")
            handlePause()
        } else if (isWifi && sessionManager.isPaused) {
            Log.i(TAG, "On WiFi - resuming downloads")
            handleResume()
        }
    }
    
    private fun buildNotification(): Notification {
        return notificationHelper.buildNotification(
            downloadRate = currentStats.sessionDownloadRate,
            uploadRate = currentStats.sessionUploadRate,
            torrentCount = currentStats.activeTorrents,
            isPaused = sessionManager.isPaused
        )
    }
    
    override fun onDestroy() {
        super.onDestroy()
        Log.i(TAG, "Service onDestroy")
        
        if (isRunning) {
            handleStop()
        }
    }
    
    override fun onBind(intent: Intent?): IBinder? = null
}
