package com.yoavmoshe.levin.data

import android.content.Context
import android.content.SharedPreferences
import java.io.File

/**
 * Repository for persisting Levin settings using SharedPreferences.
 * This replaces the TOML config file approach from desktop.
 */
class SettingsRepository(private val context: Context) {
    
    private val prefs: SharedPreferences = 
        context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
    
    companion object {
        private const val PREFS_NAME = "levin_settings"
        
        // Keys
        private const val KEY_ENABLED = "enabled"
        private const val KEY_DATA_DIR = "data_directory"
        private const val KEY_WATCH_DIR = "watch_directory"
        private const val KEY_MIN_FREE = "min_free"
        private const val KEY_MAX_STORAGE = "max_storage"
        private const val KEY_RUN_ON_STARTUP = "run_on_startup"
        private const val KEY_RUN_ON_BATTERY = "run_on_battery"
        private const val KEY_RUN_ON_CELLULAR = "run_on_cellular"
        private const val KEY_LISTEN_PORT = "listen_port"
        private const val KEY_MAX_CONNECTIONS = "max_connections"
        private const val KEY_ENABLE_DHT = "enable_dht"
        private const val KEY_ENABLE_UPNP = "enable_upnp"
        private const val KEY_ENABLE_LSD = "enable_lsd"
    }
    
    /**
     * Save settings to SharedPreferences and broadcast change notification
     */
    fun save(settings: LevinSettings) {
        prefs.edit().apply {
            putBoolean(KEY_ENABLED, settings.enabled)
            putString(KEY_DATA_DIR, settings.dataDirectory.absolutePath)
            putString(KEY_WATCH_DIR, settings.watchDirectory.absolutePath)
            putLong(KEY_MIN_FREE, settings.minFree)
            if (settings.maxStorage != null) {
                putLong(KEY_MAX_STORAGE, settings.maxStorage)
            } else {
                remove(KEY_MAX_STORAGE)
            }
            putBoolean(KEY_RUN_ON_STARTUP, settings.runOnStartup)
            putBoolean(KEY_RUN_ON_BATTERY, settings.runOnBattery)
            putBoolean(KEY_RUN_ON_CELLULAR, settings.runOnCellular)
            putInt(KEY_LISTEN_PORT, settings.listenPort)
            putInt(KEY_MAX_CONNECTIONS, settings.maxConnections)
            putBoolean(KEY_ENABLE_DHT, settings.enableDht)
            putBoolean(KEY_ENABLE_UPNP, settings.enableUpnp)
            putBoolean(KEY_ENABLE_LSD, settings.enableLsd)
            apply()
        }
        
        // Broadcast settings change to service
        android.util.Log.i("SettingsRepository", "Broadcasting settings change")
        val intent = android.content.Intent("com.yoavmoshe.levin.SETTINGS_CHANGED")
        intent.setPackage(context.packageName)
        context.sendBroadcast(intent)
        android.util.Log.i("SettingsRepository", "Broadcast sent")
    }
    
    /**
     * Load settings from SharedPreferences.
     * Returns default settings if none saved yet.
     */
    fun load(): LevinSettings {
        val defaultSettings = LevinSettings.default(context)
        
        // Load max_storage (optional - null if not set)
        val maxStorage = if (prefs.contains(KEY_MAX_STORAGE)) {
            prefs.getLong(KEY_MAX_STORAGE, -1).takeIf { it > 0 }
        } else {
            null
        }
        
        return LevinSettings(
            enabled = prefs.getBoolean(KEY_ENABLED,
                defaultSettings.enabled),
            dataDirectory = File(prefs.getString(KEY_DATA_DIR, 
                defaultSettings.dataDirectory.absolutePath)!!),
            watchDirectory = File(prefs.getString(KEY_WATCH_DIR,
                defaultSettings.watchDirectory.absolutePath)!!),
            minFree = prefs.getLong(KEY_MIN_FREE,
                defaultSettings.minFree),
            maxStorage = maxStorage,
            runOnStartup = prefs.getBoolean(KEY_RUN_ON_STARTUP,
                defaultSettings.runOnStartup),
            runOnBattery = prefs.getBoolean(KEY_RUN_ON_BATTERY,
                defaultSettings.runOnBattery),
            runOnCellular = prefs.getBoolean(KEY_RUN_ON_CELLULAR,
                defaultSettings.runOnCellular),
            listenPort = prefs.getInt(KEY_LISTEN_PORT,
                defaultSettings.listenPort),
            maxConnections = prefs.getInt(KEY_MAX_CONNECTIONS,
                defaultSettings.maxConnections),
            enableDht = prefs.getBoolean(KEY_ENABLE_DHT,
                defaultSettings.enableDht),
            enableUpnp = prefs.getBoolean(KEY_ENABLE_UPNP,
                defaultSettings.enableUpnp),
            enableLsd = prefs.getBoolean(KEY_ENABLE_LSD,
                defaultSettings.enableLsd)
        )
    }
    
    /**
     * Register a listener for settings changes
     */
    fun registerListener(listener: SharedPreferences.OnSharedPreferenceChangeListener) {
        prefs.registerOnSharedPreferenceChangeListener(listener)
    }
    
    /**
     * Unregister settings change listener
     */
    fun unregisterListener(listener: SharedPreferences.OnSharedPreferenceChangeListener) {
        prefs.unregisterOnSharedPreferenceChangeListener(listener)
    }
}
