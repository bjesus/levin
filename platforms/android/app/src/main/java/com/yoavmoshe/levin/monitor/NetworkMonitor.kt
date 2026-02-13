package com.yoavmoshe.levin.monitor

import android.content.Context
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkRequest
import android.util.Log

/**
 * Monitors network connectivity via ConnectivityManager.NetworkCallback.
 *
 * Calls [onNetworkChanged] with (hasWifi, hasCellular) whenever the state changes.
 */
class NetworkMonitor(
    private val onNetworkChanged: (hasWifi: Boolean, hasCellular: Boolean) -> Unit
) {

    companion object {
        private const val TAG = "NetworkMonitor"
    }

    private var connectivityManager: ConnectivityManager? = null

    /** Track last reported values to avoid spamming the callback. */
    private var lastWifi: Boolean? = null
    private var lastCellular: Boolean? = null

    private val networkCallback = object : ConnectivityManager.NetworkCallback() {
        override fun onAvailable(network: Network) {
            updateState()
        }

        override fun onLost(network: Network) {
            updateState()
        }

        override fun onCapabilitiesChanged(
            network: Network,
            capabilities: NetworkCapabilities
        ) {
            updateState()
        }
    }

    fun register(context: Context) {
        connectivityManager =
            context.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager

        val request = NetworkRequest.Builder()
            .addCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
            .build()

        connectivityManager?.registerNetworkCallback(request, networkCallback)

        // Emit initial state
        updateState()
    }

    fun unregister(context: Context) {
        try {
            connectivityManager?.unregisterNetworkCallback(networkCallback)
        } catch (e: IllegalArgumentException) {
            Log.w(TAG, "Callback was not registered", e)
        }
        connectivityManager = null
    }

    private fun updateState() {
        val cm = connectivityManager ?: return
        val activeNetwork = cm.activeNetwork
        val caps = if (activeNetwork != null) cm.getNetworkCapabilities(activeNetwork) else null

        val hasWifi = caps?.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) == true
        val hasCellular = caps?.hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR) == true

        if (hasWifi != lastWifi || hasCellular != lastCellular) {
            lastWifi = hasWifi
            lastCellular = hasCellular
            Log.d(TAG, "Network changed: wifi=$hasWifi cellular=$hasCellular")
            onNetworkChanged(hasWifi, hasCellular)
        }
    }
}
