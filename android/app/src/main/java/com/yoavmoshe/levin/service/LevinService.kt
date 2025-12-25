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
        const val ACTION_RELOAD_TORRENTS = "com.yoavmoshe.levin.RELOAD_TORRENTS"
        
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
    private var updateCounter = 0  // Counter for periodic lifetime saves
    private var isStorageOverLimit = false  // Track if paused due to storage
    
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
            ACTION_RELOAD_TORRENTS -> handleReload()
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
        
        // Don't start libtorrent session yet - will start lazily when first torrent is added
        // This prevents unnecessary network connections when there are no torrents
        
        // Scan for existing torrents and add them
        // (This will start the session if torrents are found)
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
    
    private fun handleReload() {
        Log.i(TAG, "Manually reloading torrents from watch directory")
        scanAndAddTorrents()
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
        
        // Check storage limits - CRITICAL: Must enforce disk space limits
        val storageStatus = storageMonitor.getStatus()
        if (storageStatus.isOverBudget) {
            val deficitMb = storageStatus.deficitBytes / (1024 * 1024)
            val usedMb = storageStatus.usedByLevinBytes / (1024 * 1024)
            val maxMb = storageStatus.maxAllowedBytes?.let { it / (1024 * 1024) } ?: 0
            Log.w(TAG, "Storage budget exceeded! Used: $usedMb MB, Max: $maxMb MB, Over by: $deficitMb MB")
            
            // CRITICAL: Pause downloads immediately when storage limit is exceeded
            isStorageOverLimit = true
            if (!sessionManager.isPaused) {
                Log.w(TAG, "Pausing downloads due to storage limit violation")
                handlePause()
            }
        } else {
            // Storage is OK - clear the flag
            if (isStorageOverLimit) {
                Log.i(TAG, "Storage now within limits")
                isStorageOverLimit = false
                // Check if we should resume (only if all other conditions are met)
                if (sessionManager.isPaused && shouldBeRunning()) {
                    Log.i(TAG, "Storage OK and all conditions met - resuming downloads")
                    handleResume()
                }
            }
        }
        
        // Calculate piece statistics from all torrents
        val torrentStatuses = sessionManager.getTorrentStatuses()
        var totalPiecesHave = 0
        var totalPiecesTotal = 0
        try {
            torrentStatuses.forEach { status ->
                // Get the number of pieces in the torrent and how many we have
                val numPieces = status.numPieces()
                val progress = status.progress()
                totalPiecesTotal += numPieces
                // Calculate pieces we have from progress percentage
                totalPiecesHave += (numPieces * progress).toInt()
            }
        } catch (e: Exception) {
            Log.w(TAG, "Error calculating piece statistics", e)
        }
        
        // Update current stats
        currentStats = currentStats.updateSession(
            downloaded = sessionStats.totalDownloaded,
            uploaded = sessionStats.totalUploaded,
            downloadRate = sessionStats.downloadRate,
            uploadRate = sessionStats.uploadRate,
            torrents = sessionStats.activeTorrents,
            peers = sessionStats.peerCount,
            paused = sessionManager.isPaused,
            diskUsed = storageStatus.usedByLevinBytes,
            diskFree = storageStatus.freeBytes,
            piecesHave = totalPiecesHave,
            piecesTotal = totalPiecesTotal
        )
        
        // Save to repository every update
        statsRepo.save(currentStats)
        
        // Also save session to lifetime periodically (every 30 seconds)
        // This ensures stats persist even if app is force-killed
        updateCounter++
        if (updateCounter >= 30) {
            Log.d(TAG, "Periodic lifetime save (session -> lifetime)")
            statsRepo.saveSession(currentStats)
            // Reload to get updated lifetime values
            currentStats = statsRepo.load()
            updateCounter = 0
        }
    }
    
    private fun updateNotification() {
        if (!isRunning) return
        
        val notification = buildNotification()
        val notificationManager = getSystemService(NotificationManager::class.java)
        notificationManager?.notify(NotificationHelper.NOTIFICATION_ID, notification)
    }
    
    /**
     * Check if all operating conditions are met to run
     * Returns true if downloads should be active, false if should be paused
     */
    private fun shouldBeRunning(): Boolean {
        // Check power condition
        val isCharging = powerMonitor.isCharging()
        val powerOk = settings.runOnBattery || isCharging
        if (!powerOk) {
            Log.d(TAG, "Power condition not met: charging=$isCharging, runOnBattery=${settings.runOnBattery}")
            return false
        }
        
        // Check network condition (we don't have a simple getter, so skip this check for now)
        // The network monitor will call onNetworkTypeChanged when state changes
        
        // Check storage condition
        if (isStorageOverLimit) {
            Log.d(TAG, "Storage condition not met: over limit")
            return false
        }
        
        return true
    }
    
    private fun onPowerStateChanged(isCharging: Boolean) {
        // Check all conditions and pause/resume appropriately
        if (shouldBeRunning()) {
            if (sessionManager.isPaused) {
                Log.i(TAG, "All conditions met - resuming downloads")
                handleResume()
            }
        } else {
            if (!sessionManager.isPaused) {
                Log.i(TAG, "Conditions not met - pausing downloads")
                handlePause()
            }
        }
    }
    
    private fun onNetworkTypeChanged(isWifi: Boolean, isCellular: Boolean) {
        // Pause if on cellular-only and cellular not allowed
        val networkOk = isWifi || settings.runOnCellular
        
        if (!networkOk && !sessionManager.isPaused) {
            Log.i(TAG, "On cellular network (runOnCellular=false) - pausing downloads")
            handlePause()
        } else if (networkOk && sessionManager.isPaused && shouldBeRunning()) {
            Log.i(TAG, "Network conditions met and all conditions OK - resuming downloads")
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
