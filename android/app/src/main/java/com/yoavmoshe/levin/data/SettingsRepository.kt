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
        private const val KEY_DATA_DIR = "data_directory"
        private const val KEY_WATCH_DIR = "watch_directory"
        private const val KEY_ALLOWED_STORAGE = "allowed_storage_bytes"
        private const val KEY_MIN_FREE_SPACE = "min_free_space_bytes"
        private const val KEY_RUN_ON_BATTERY = "run_on_battery"
        private const val KEY_RUN_ON_CELLULAR = "run_on_cellular"
        private const val KEY_LISTEN_PORT = "listen_port"
        private const val KEY_MAX_CONNECTIONS = "max_connections"
        private const val KEY_ENABLE_DHT = "enable_dht"
        private const val KEY_ENABLE_UPNP = "enable_upnp"
        private const val KEY_ENABLE_LSD = "enable_lsd"
    }
    
    /**
     * Save settings to SharedPreferences
     */
    fun save(settings: LevinSettings) {
        prefs.edit().apply {
            putString(KEY_DATA_DIR, settings.dataDirectory.absolutePath)
            putString(KEY_WATCH_DIR, settings.watchDirectory.absolutePath)
            putLong(KEY_ALLOWED_STORAGE, settings.allowedStorageBytes)
            putLong(KEY_MIN_FREE_SPACE, settings.minFreeSpaceBytes)
            putBoolean(KEY_RUN_ON_BATTERY, settings.runOnBattery)
            putBoolean(KEY_RUN_ON_CELLULAR, settings.runOnCellular)
            putInt(KEY_LISTEN_PORT, settings.listenPort)
            putInt(KEY_MAX_CONNECTIONS, settings.maxConnections)
            putBoolean(KEY_ENABLE_DHT, settings.enableDht)
            putBoolean(KEY_ENABLE_UPNP, settings.enableUpnp)
            putBoolean(KEY_ENABLE_LSD, settings.enableLsd)
            apply()
        }
    }
    
    /**
     * Load settings from SharedPreferences.
     * Returns default settings if none saved yet.
     */
    fun load(): LevinSettings {
        val defaultSettings = LevinSettings.default(context)
        
        return LevinSettings(
            dataDirectory = File(prefs.getString(KEY_DATA_DIR, 
                defaultSettings.dataDirectory.absolutePath)!!),
            watchDirectory = File(prefs.getString(KEY_WATCH_DIR,
                defaultSettings.watchDirectory.absolutePath)!!),
            allowedStorageBytes = prefs.getLong(KEY_ALLOWED_STORAGE,
                defaultSettings.allowedStorageBytes),
            minFreeSpaceBytes = prefs.getLong(KEY_MIN_FREE_SPACE,
                defaultSettings.minFreeSpaceBytes),
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
