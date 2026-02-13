package com.yoavmoshe.levin

/**
 * JNI bridge to liblevin C API.
 *
 * All methods must be called from the same thread (the service's Handler thread).
 */
object LevinNative {
    init {
        System.loadLibrary("levin")
    }

    /**
     * Status snapshot returned by [getStatus].
     */
    data class StatusData(
        val state: Int,
        val torrentCount: Int,
        val peerCount: Int,
        val downloadRate: Int,
        val uploadRate: Int,
        val reserved: Int,
        val totalDownloaded: Long,
        val totalUploaded: Long,
        val diskUsage: Long,
        val diskBudget: Long,
        val overBudget: Boolean
    ) {
        val stateName: String
            get() = when (state) {
                0 -> "OFF"
                1 -> "PAUSED"
                2 -> "IDLE"
                3 -> "SEEDING"
                4 -> "DOWNLOADING"
                else -> "UNKNOWN"
            }
    }

    /**
     * Torrent info returned by [getTorrents].
     */
    data class TorrentData(
        val infoHash: String,
        val name: String,
        val size: Long,
        val downloaded: Long,
        val uploaded: Long,
        val downloadRate: Int,
        val uploadRate: Int,
        val numPeers: Int,
        val progress: Double,
        val isSeed: Boolean
    )

    // --- Lifecycle ---
    external fun create(
        watchDir: String,
        dataDir: String,
        stateDir: String,
        minFreeBytes: Long,
        minFreePercentage: Double,
        maxStorageBytes: Long,
        runOnBattery: Boolean,
        runOnCellular: Boolean,
        diskCheckIntervalSecs: Int,
        maxDownloadKbps: Int,
        maxUploadKbps: Int
    ): Long

    external fun destroy(handle: Long)
    external fun start(handle: Long): Int
    external fun stop(handle: Long)
    external fun tick(handle: Long)

    // --- Condition Updates ---
    external fun setEnabled(handle: Long, enabled: Boolean)
    external fun updateBattery(handle: Long, onAcPower: Boolean)
    external fun updateNetwork(handle: Long, hasWifi: Boolean, hasCellular: Boolean)
    external fun updateStorage(handle: Long, fsTotal: Long, fsFree: Long)

    // --- Torrent Management ---
    external fun addTorrent(handle: Long, torrentPath: String): Int
    external fun removeTorrent(handle: Long, infoHash: String)
    external fun getTorrents(handle: Long): Array<TorrentData>

    // --- Status ---
    external fun getStatus(handle: Long): StatusData

    // --- Settings ---
    external fun setDownloadLimit(handle: Long, kbps: Int)
    external fun setUploadLimit(handle: Long, kbps: Int)
    external fun setRunOnBattery(handle: Long, runOnBattery: Boolean)
    external fun setRunOnCellular(handle: Long, runOnCellular: Boolean)
    external fun setDiskLimits(handle: Long, minFreeBytes: Long, minFreePct: Double, maxStorageBytes: Long)

    // --- Anna's Archive ---
    external fun populateTorrents(handle: Long): Int

    // --- Callbacks ---
    external fun setStateCallback(handle: Long)
}
