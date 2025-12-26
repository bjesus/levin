package com.yoavmoshe.levin.service

import android.app.Notification
import android.app.NotificationManager
import android.app.Service
import android.content.Intent
import android.os.IBinder
import android.util.Log
import com.yoavmoshe.levin.data.LevinSettings
import com.yoavmoshe.levin.data.SettingsRepository
import com.yoavmoshe.levin.data.Statistics
import com.yoavmoshe.levin.data.StatisticsRepository
import com.yoavmoshe.levin.monitoring.NetworkMonitor
import com.yoavmoshe.levin.monitoring.PowerMonitor
import com.yoavmoshe.levin.monitoring.StorageMonitor
import com.yoavmoshe.levin.monitoring.TorrentWatcher
import com.yoavmoshe.levin.state.LevinState
import com.yoavmoshe.levin.state.LevinStateManager
import java.io.File

/**
 * Foreground service that manages BitTorrent archiving
 * Uses unified state machine for clean state management
 */
class LevinService : Service() {
    
    companion object {
        const val ACTION_START = "com.yoavmoshe.levin.START"
        const val ACTION_STOP = "com.yoavmoshe.levin.STOP"
        const val ACTION_ENABLE = "com.yoavmoshe.levin.ENABLE"
        const val ACTION_DISABLE = "com.yoavmoshe.levin.DISABLE"
        const val ACTION_RELOAD_TORRENTS = "com.yoavmoshe.levin.RELOAD_TORRENTS"
        
        private const val TAG = "LevinService"
    }
    
    // Core components
    private lateinit var settings: LevinSettings
    private lateinit var settingsRepo: SettingsRepository
    private lateinit var statsRepo: StatisticsRepository
    private lateinit var sessionManager: LevinSessionManager
    private lateinit var stateManager: LevinStateManager
    
    private lateinit var powerMonitor: PowerMonitor
    private lateinit var networkMonitor: NetworkMonitor
    private lateinit var storageMonitor: StorageMonitor
    private lateinit var torrentWatcher: TorrentWatcher
    
    private var isServiceRunning = false
    private var lastDeletionTime = 0L  // Track when we last deleted files
    private var recentlyDeletedBytes = 0L  // Track how much we deleted recently
    
    private lateinit var notificationHelper: NotificationHelper
    private lateinit var currentStats: Statistics
    private var updateCounter = 0
    
    // Broadcast receiver for settings changes
    private val settingsChangeReceiver = object : android.content.BroadcastReceiver() {
        override fun onReceive(context: android.content.Context, intent: android.content.Intent) {
            Log.i(TAG, "Settings changed broadcast received")
            settings = settingsRepo.load()
            updateAllConditions()
        }
    }
    
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
        storageMonitor = StorageMonitor(this, settings)
        torrentWatcher = TorrentWatcher(settings.watchDirectory) { file ->
            sessionManager.addTorrent(file)
            // Update torrent count after adding
            updateTorrentCount()
        }
        
        // Initialize state machine
        stateManager = LevinStateManager { oldState, newState ->
            handleStateTransition(oldState, newState)
        }
        
        // Set initial state based on settings
        stateManager.setEnabled(settings.enabled)
        
        // Listen for settings changes via broadcast
        val filter = android.content.IntentFilter("com.yoavmoshe.levin.SETTINGS_CHANGED")
        registerReceiver(settingsChangeReceiver, filter, android.content.Context.RECEIVER_NOT_EXPORTED)
    }
    
    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        Log.i(TAG, "onStartCommand: action=${intent?.action}")
        
        when (intent?.action) {
            ACTION_START -> {
                // Service started - ensure enabled state matches settings
                if (!isServiceRunning) {
                    isServiceRunning = true
                    initializeService()
                }
            }
            ACTION_STOP -> {
                // Stop service completely
                handleStopService()
            }
            ACTION_ENABLE -> {
                // User enabled via toggle
                stateManager.setEnabled(true)
                // Save to settings
                settingsRepo.save(settings.copy(enabled = true))
            }
            ACTION_DISABLE -> {
                // User disabled via toggle
                stateManager.setEnabled(false)
                // Save to settings
                settingsRepo.save(settings.copy(enabled = false))
            }
            ACTION_RELOAD_TORRENTS -> {
                scanAndAddTorrents()
            }
            else -> {
                // Default to START if no action specified
                if (!isServiceRunning) {
                    isServiceRunning = true
                    initializeService()
                }
            }
        }
        
        return START_STICKY  // Restart service if killed by system
    }
    
    /**
     * Initialize service and start monitoring
     */
    private fun initializeService() {
        Log.i(TAG, "Initializing service")
        
        // CRITICAL: Start foreground immediately to satisfy Android requirement
        // We'll update the notification later with actual state
        val initialNotification = buildNotification(LevinState.IDLE)
        startForeground(NotificationHelper.NOTIFICATION_ID, initialNotification)
        
        // Scan for existing torrents
        scanAndAddTorrents()
        
        // Start monitors
        startMonitoring()
        
        // Start periodic updates
        startPeriodicUpdates()
        
        // Update all conditions to trigger initial state computation
        updateAllConditions()
    }
    
    /**
     * Start all monitors
     */
    private fun startMonitoring() {
        // Start power monitor
        powerMonitor.start { isCharging ->
            val allows = settings.runOnBattery || isCharging
            stateManager.updateBatteryCondition(allows)
        }
        
        // Start network monitor
        networkMonitor.start { isWifi, isCellular ->
            val allows = isWifi || settings.runOnCellular
            stateManager.updateNetworkCondition(allows)
        }
        
        // Start file watcher
        torrentWatcher.start()
        
        // Log initial storage status
        storageMonitor.logStatus()
    }
    
    /**
     * Stop all monitors
     */
    private fun stopMonitoring() {
        powerMonitor.stop()
        networkMonitor.stop()
        torrentWatcher.stop()
    }
    
    /**
     * Update all conditions in state machine
     * Call this after settings change
     */
    private fun updateAllConditions() {
        // Battery condition
        val isCharging = powerMonitor.isCharging()
        val batteryAllows = settings.runOnBattery || isCharging
        stateManager.updateBatteryCondition(batteryAllows)
        
        // Network condition
        // We don't have a simple way to query current network state
        // The network monitor will update us via callback
        
        // Torrent count
        updateTorrentCount()
        
        // Storage condition
        val storageStatus = storageMonitor.getStatus()
        stateManager.updateStorageCondition(!storageStatus.isOverBudget)
        
        // Force recomputation
        stateManager.recompute()
    }
    
    /**
     * Update torrent count in state machine
     */
    private fun updateTorrentCount() {
        val hasTorrents = sessionManager.getTorrentStatuses().isNotEmpty()
        stateManager.updateHasTorrents(hasTorrents)
    }
    
    /**
     * Handle state transitions
     * This is where state changes trigger actual behavior
     */
    private fun handleStateTransition(oldState: LevinState, newState: LevinState) {
        Log.i(TAG, "Handling state transition: $oldState → $newState")
        
        when (newState) {
            LevinState.OFF -> {
                // User disabled - stop everything
                stopMonitoring()
                stopSession()
                hideNotification()
            }
            
            LevinState.PAUSED -> {
                // Waiting for conditions - keep monitoring but stop session
                ensureMonitoring()
                stopSession()
                hideNotification()
            }
            
            LevinState.IDLE -> {
                // No torrents - keep session running, show notification
                ensureMonitoring()
                ensureSessionRunning()
                resumeDownloadsIfPaused()
                showNotification(newState)
            }
            
            LevinState.SEEDING -> {
                // Storage full - pause downloads, keep uploads
                ensureMonitoring()
                ensureSessionRunning()
                pauseDownloads()
                showNotification(newState)
            }
            
            LevinState.DOWNLOADING -> {
                // Normal operation - full speed
                ensureMonitoring()
                ensureSessionRunning()
                resumeDownloadsIfPaused()
                showNotification(newState)
            }
        }
        
        // Update statistics
        updateStats()
    }
    
    /**
     * Ensure monitoring is active
     */
    private fun ensureMonitoring() {
        // Monitors are started once in initializeService
        // This is here for future dynamic start/stop if needed
    }
    
    /**
     * Ensure session is running
     */
    private fun ensureSessionRunning() {
        if (sessionManager.isPaused) {
            Log.i(TAG, "Resuming session for state")
            sessionManager.resume()
        }
    }
    
    /**
     * Stop session completely
     */
    private fun stopSession() {
        if (!sessionManager.isPaused) {
            Log.i(TAG, "Pausing session for state")
            sessionManager.pause()
        }
    }
    
    /**
     * Pause downloads only (for storage limit)
     */
    private fun pauseDownloads() {
        if (!sessionManager.isDownloadPaused) {
            Log.i(TAG, "Pausing downloads (storage limit)")
            sessionManager.pauseDownloads()
        }
    }
    
    /**
     * Resume downloads if they were paused
     */
    private fun resumeDownloadsIfPaused() {
        if (sessionManager.isDownloadPaused) {
            Log.i(TAG, "Resuming downloads")
            sessionManager.resumeDownloads()
        }
    }
    
    /**
     * Show notification for current state
     */
    private fun showNotification(state: LevinState) {
        val notification = buildNotification(state)
        startForeground(NotificationHelper.NOTIFICATION_ID, notification)
    }
    
    /**
     * Hide notification
     */
    private fun hideNotification() {
        stopForeground(STOP_FOREGROUND_REMOVE)
    }
    
    /**
     * Build notification for current state
     */
    private fun buildNotification(state: LevinState = stateManager.getState()): Notification {
        return notificationHelper.buildNotification(
            state = state,
            downloadRate = currentStats.sessionDownloadRate,
            uploadRate = currentStats.sessionUploadRate,
            torrentCount = currentStats.activeTorrents
        )
    }
    
    /**
     * Scan watch directory and add torrents
     */
    private fun scanAndAddTorrents() {
        val existingTorrents = torrentWatcher.scanExisting()
        Log.i(TAG, "Found ${existingTorrents.size} existing torrent files")
        existingTorrents.forEach { file ->
            sessionManager.addTorrent(file)
        }
        updateTorrentCount()
    }
    
    /**
     * Start periodic updates (stats + notification + storage check)
     */
    private fun startPeriodicUpdates() {
        Thread {
            while (isServiceRunning) {
                try {
                    // Update stats
                    updateStats()
                    
                    // Check storage and free up space if needed
                    val storageStatus = storageMonitor.getStatus()
                    if (storageStatus.isOverBudget && storageStatus.deficitBytes > 0) {
                        freeUpSpace(storageStatus.deficitBytes)
                    }
                    
                    // Update state machine (after potential cleanup)
                    val storageStatusAfterCleanup = storageMonitor.getStatus()
                    stateManager.updateStorageCondition(!storageStatusAfterCleanup.isOverBudget)
                    
                    // Update torrent count
                    updateTorrentCount()
                    
                    // Update notification if visible
                    val currentState = stateManager.getState()
                    if (currentState.showsNotification()) {
                        updateNotification()
                    }
                    
                    Thread.sleep(1000)  // Update every second
                } catch (e: InterruptedException) {
                    break
                } catch (e: Exception) {
                    Log.e(TAG, "Error in periodic update", e)
                }
            }
        }.start()
    }
    
    /**
     * Update statistics from session
     */
    private fun updateStats() {
        val sessionStats = sessionManager.getStats()
        val storageStatus = storageMonitor.getStatus()
        
        // Calculate piece statistics
        val torrentStatuses = sessionManager.getTorrentStatuses()
        var totalPiecesHave = 0
        var totalPiecesTotal = 0
        try {
            torrentStatuses.forEach { status ->
                val numPieces = status.numPieces()
                val progress = status.progress()
                totalPiecesTotal += numPieces
                totalPiecesHave += (numPieces * progress).toInt()
            }
        } catch (e: Exception) {
            Log.w(TAG, "Error calculating piece statistics", e)
        }
        
        // Update current stats with new state
        currentStats = currentStats.updateSession(
            downloaded = sessionStats.totalDownloaded,
            uploaded = sessionStats.totalUploaded,
            downloadRate = sessionStats.downloadRate,
            uploadRate = sessionStats.uploadRate,
            torrents = sessionStats.activeTorrents,
            peers = sessionStats.peerCount,
            currentState = stateManager.getState(),  // Use state machine state
            diskUsed = storageStatus.usedByLevinBytes,
            diskFree = storageStatus.freeBytes,
            piecesHave = totalPiecesHave,
            piecesTotal = totalPiecesTotal
        )
        
        // Save to repository
        statsRepo.save(currentStats)
        
        // Periodically save session to lifetime (every 30 seconds)
        updateCounter++
        if (updateCounter >= 30) {
            Log.d(TAG, "Periodic lifetime save (session → lifetime)")
            statsRepo.saveSession(currentStats)
            currentStats = statsRepo.load()
            updateCounter = 0
        }
    }
    
    /**
     * Free up disk space by deleting entire torrents (oldest first)
     * Android FUSE filesystem has delayed space accounting, so we track deletions
     * to avoid over-deleting
     */
    private fun freeUpSpace(bytesToFree: Long) {
        Log.w(TAG, "Need to free up ${bytesToFree / (1024 * 1024)} MB")
        
        // Check if we recently deleted files (within last 30 seconds)
        // Android FUSE has delayed space accounting, so we need a longer window
        val now = System.currentTimeMillis()
        val timeSinceLastDeletion = now - lastDeletionTime
        if (timeSinceLastDeletion < 30000 && recentlyDeletedBytes > 0) {
            // We deleted recently, but Android hasn't updated free space yet
            // Adjust the target by what we already deleted
            val adjustedTarget = bytesToFree - recentlyDeletedBytes
            if (adjustedTarget <= 0) {
                Log.i(TAG, "Recently deleted ${recentlyDeletedBytes / (1024 * 1024)} MB, " +
                          "waiting for filesystem to update (${(30000 - timeSinceLastDeletion) / 1000}s remaining)")
                return
            }
        }
        
        // Reset tracking after 30 seconds
        if (timeSinceLastDeletion >= 30000) {
            recentlyDeletedBytes = 0
        }
        
        val dataDir = settings.dataDirectory
        if (!dataDir.exists()) {
            Log.w(TAG, "Data directory doesn't exist")
            return
        }
        
        // Get all files in the data directory with their sizes
        val files = mutableListOf<Pair<File, Long>>()
        for (file in dataDir.walkTopDown()) {
            if (file.isFile) {
                files.add(Pair(file, file.length()))
            }
        }
        
        if (files.isEmpty()) {
            Log.w(TAG, "No files to delete in data directory")
            return
        }
        
        // Sort by modification time (oldest first)
        files.sortBy { it.first.lastModified() }
        
        var freed = 0L
        var deleted = 0
        
        for ((file, size) in files) {
            if (freed >= bytesToFree) break
            
            try {
                if (file.delete()) {
                    freed += size
                    deleted++
                    Log.d(TAG, "Deleted file: ${file.name} (${size / (1024 * 1024)} MB)")
                } else {
                    Log.e(TAG, "Failed to delete file: ${file.absolutePath}")
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error deleting file", e)
            }
        }
        
        // Track this deletion
        lastDeletionTime = now
        recentlyDeletedBytes += freed
        
        Log.w(TAG, "Deleted $deleted files, freed ${freed / (1024 * 1024)} MB (target: ${bytesToFree / (1024 * 1024)} MB)")
    }
    
    /**
     * Update notification
     */
    private fun updateNotification() {
        val currentState = stateManager.getState()
        if (currentState.showsNotification()) {
            val notification = buildNotification(currentState)
            val notificationManager = getSystemService(NotificationManager::class.java)
            notificationManager?.notify(NotificationHelper.NOTIFICATION_ID, notification)
        }
    }
    
    /**
     * Stop service completely
     */
    private fun handleStopService() {
        Log.i(TAG, "Stopping service")
        
        isServiceRunning = false
        
        // Stop monitors
        stopMonitoring()
        
        // Save session stats to lifetime
        statsRepo.saveSession(currentStats)
        
        // Stop session
        sessionManager.stop()
        
        // Stop foreground and remove notification
        stopForeground(STOP_FOREGROUND_REMOVE)
        stopSelf()
    }
    
    override fun onDestroy() {
        super.onDestroy()
        Log.i(TAG, "Service onDestroy")
        
        if (isServiceRunning) {
            handleStopService()
        }
        
        // Unregister settings receiver
        try {
            unregisterReceiver(settingsChangeReceiver)
        } catch (e: IllegalArgumentException) {
            // Receiver not registered, ignore
        }
    }
    
    override fun onBind(intent: Intent?): IBinder? = null
}
