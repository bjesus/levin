package com.yoavmoshe.levin.monitoring

import android.content.Context
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkRequest
import android.util.Log

/**
 * Monitor network type (WiFi vs Cellular)
 * Used to pause downloads on cellular if configured
 */
class NetworkMonitor(private val context: Context) {
    
    private var callback: ((isWifi: Boolean, isCellular: Boolean) -> Unit)? = null
    private var isStarted = false
    
    private val connectivityManager = 
        context.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
    
    companion object {
        private const val TAG = "NetworkMonitor"
    }
    
    /**
     * NetworkCallback for monitoring network changes
     */
    private val networkCallback = object : ConnectivityManager.NetworkCallback() {
        override fun onCapabilitiesChanged(
            network: Network,
            capabilities: NetworkCapabilities
        ) {
            val isWifi = capabilities.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) ||
                        capabilities.hasTransport(NetworkCapabilities.TRANSPORT_ETHERNET)
            val isCellular = capabilities.hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR)
            
            Log.d(TAG, "Network changed: WiFi=$isWifi, Cellular=$isCellular")
            callback?.invoke(isWifi, isCellular)
        }
        
        override fun onLost(network: Network) {
            Log.i(TAG, "Network lost")
            callback?.invoke(false, false)
        }
    }
    
    /**
     * Start monitoring network changes
     * @param callback Called with (isWifi, isCellular) when network changes
     */
    fun start(callback: (isWifi: Boolean, isCellular: Boolean) -> Unit) {
        if (isStarted) {
            Log.w(TAG, "NetworkMonitor already started")
            return
        }
        
        Log.i(TAG, "Starting NetworkMonitor")
        this.callback = callback
        isStarted = true
        
        // Register network callback
        val request = NetworkRequest.Builder()
            .addCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
            .build()
        
        connectivityManager.registerNetworkCallback(request, networkCallback)
        
        // Report initial state
        val (isWifi, isCellular) = getCurrentNetworkType()
        Log.i(TAG, "Initial network: WiFi=$isWifi, Cellular=$isCellular")
        callback(isWifi, isCellular)
    }
    
    /**
     * Stop monitoring
     */
    fun stop() {
        if (!isStarted) return
        
        Log.i(TAG, "Stopping NetworkMonitor")
        isStarted = false
        
        try {
            connectivityManager.unregisterNetworkCallback(networkCallback)
        } catch (e: IllegalArgumentException) {
            // Callback not registered, ignore
            Log.w(TAG, "Callback already unregistered")
        }
        
        callback = null
    }
    
    /**
     * Get current network type
     */
    private fun getCurrentNetworkType(): Pair<Boolean, Boolean> {
        val capabilities = connectivityManager.getNetworkCapabilities(
            connectivityManager.activeNetwork
        ) ?: return Pair(false, false)
        
        val isWifi = capabilities.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) ||
                    capabilities.hasTransport(NetworkCapabilities.TRANSPORT_ETHERNET)
        val isCellular = capabilities.hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR)
        
        return Pair(isWifi, isCellular)
    }
    
    /**
     * Check if on WiFi (can be called without starting monitor)
     */
    fun isWifi(): Boolean = getCurrentNetworkType().first
    
    /**
     * Check if on cellular (can be called without starting monitor)
     */
    fun isCellular(): Boolean = getCurrentNetworkType().second
}
