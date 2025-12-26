package com.yoavmoshe.levin.ui

import android.content.Intent
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import com.google.android.material.switchmaterial.SwitchMaterial
import com.google.android.material.textview.MaterialTextView
import com.yoavmoshe.levin.R
import com.yoavmoshe.levin.data.SettingsRepository
import com.yoavmoshe.levin.data.StatisticsRepository
import com.yoavmoshe.levin.service.LevinService
import com.yoavmoshe.levin.state.LevinState
import com.yoavmoshe.levin.util.FormatUtils
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

/**
 * Fragment showing real-time statistics with controls
 */
class StatsFragment : Fragment() {
    
    private lateinit var statsRepo: StatisticsRepository
    private lateinit var settingsRepo: SettingsRepository
    
    // Status card views
    private lateinit var statusText: MaterialTextView
    private lateinit var currentSpeed: MaterialTextView
    private lateinit var activeTorrentsStatus: MaterialTextView
    private lateinit var enableToggle: SwitchMaterial
    
    // Stats views (combined table)
    private lateinit var downloaded: MaterialTextView
    private lateinit var uploaded: MaterialTextView
    private lateinit var lifetimeDownloaded: MaterialTextView
    private lateinit var lifetimeUploaded: MaterialTextView
    private lateinit var sessionRatio: MaterialTextView
    private lateinit var ratio: MaterialTextView
    private lateinit var diskUsed: MaterialTextView
    private lateinit var diskFree: MaterialTextView
    private lateinit var peers: MaterialTextView
    private lateinit var pieces: MaterialTextView
    
    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        return inflater.inflate(R.layout.fragment_stats, container, false)
    }
    
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        
        // Initialize repositories
        statsRepo = StatisticsRepository(requireContext())
        settingsRepo = SettingsRepository(requireContext())
        
        // Bind status card views
        statusText = view.findViewById(R.id.status_text)
        currentSpeed = view.findViewById(R.id.current_speed)
        activeTorrentsStatus = view.findViewById(R.id.active_torrents_status)
        
        // Bind toggle from toolbar (in parent activity)
        enableToggle = requireActivity().findViewById(R.id.toolbar_enable_toggle)
        
        // Bind stats views (combined table)
        downloaded = view.findViewById(R.id.downloaded)
        uploaded = view.findViewById(R.id.uploaded)
        lifetimeDownloaded = view.findViewById(R.id.lifetime_downloaded)
        lifetimeUploaded = view.findViewById(R.id.lifetime_uploaded)
        sessionRatio = view.findViewById(R.id.session_ratio)
        ratio = view.findViewById(R.id.ratio)
        diskUsed = view.findViewById(R.id.disk_used)
        diskFree = view.findViewById(R.id.disk_free)
        peers = view.findViewById(R.id.peers)
        pieces = view.findViewById(R.id.pieces)
        
        // Setup toggle listener
        enableToggle.setOnCheckedChangeListener { _, isChecked ->
            val action = if (isChecked) {
                LevinService.ACTION_ENABLE
            } else {
                LevinService.ACTION_DISABLE
            }
            val intent = Intent(requireContext(), LevinService::class.java).apply {
                this.action = action
            }
            requireContext().startService(intent)
        }
        
        // Start periodic updates
        startPeriodicUpdates()
    }
    
    private fun startPeriodicUpdates() {
        viewLifecycleOwner.lifecycleScope.launch {
            while (isActive) {
                updateStats()
                delay(1000) // Update every second
            }
        }
    }
    
    private suspend fun updateStats() {
        // Load data on background thread to avoid ANR
        val stats = kotlinx.coroutines.withContext(kotlinx.coroutines.Dispatchers.IO) {
            statsRepo.load()
        }
        val settings = kotlinx.coroutines.withContext(kotlinx.coroutines.Dispatchers.IO) {
            settingsRepo.load()
        }
        
        // Update toggle state (without triggering listener)
        enableToggle.setOnCheckedChangeListener(null)
        enableToggle.isChecked = settings.enabled
        enableToggle.setOnCheckedChangeListener { _, isChecked ->
            val action = if (isChecked) {
                LevinService.ACTION_ENABLE
            } else {
                LevinService.ACTION_DISABLE
            }
            val intent = Intent(requireContext(), LevinService::class.java).apply {
                this.action = action
            }
            requireContext().startService(intent)
        }
        
        // Update status text based on state
        statusText.text = when (stats.state) {
            LevinState.OFF -> "Off"
            LevinState.PAUSED -> "Paused"
            LevinState.IDLE -> "No Torrents"
            LevinState.SEEDING -> "Seeding (Storage Limit)"
            LevinState.DOWNLOADING -> "Downloading"
        }
        
        // Update speed display based on state
        currentSpeed.text = when (stats.state) {
            LevinState.OFF, LevinState.PAUSED -> ""
            LevinState.IDLE -> "No active torrents"
            LevinState.SEEDING -> "⬆ ${FormatUtils.formatSpeed(stats.sessionUploadRate)} (downloads paused)"
            LevinState.DOWNLOADING -> "⬇ ${FormatUtils.formatSpeed(stats.sessionDownloadRate)}  ⬆ ${FormatUtils.formatSpeed(stats.sessionUploadRate)}"
        }
        
        activeTorrentsStatus.text = "${stats.activeTorrents} active torrent${if (stats.activeTorrents != 1) "s" else ""}"
        
        // Session stats (combined table)
        downloaded.text = FormatUtils.formatSize(stats.sessionDownloaded)
        uploaded.text = FormatUtils.formatSize(stats.sessionUploaded)
        
        // Lifetime stats (combined table) - shows TOTAL (lifetime + session)
        lifetimeDownloaded.text = FormatUtils.formatSize(stats.totalDownloaded)
        lifetimeUploaded.text = FormatUtils.formatSize(stats.totalUploaded)
        
        // Ratios
        sessionRatio.text = FormatUtils.formatRatio(stats.sessionRatio)
        ratio.text = FormatUtils.formatRatio(stats.lifetimeRatio)
        
        // Disk usage stats
        diskUsed.text = FormatUtils.formatSize(stats.diskUsedBytes)
        diskFree.text = FormatUtils.formatSize(stats.diskFreeBytes)
        
        // Peer count
        peers.text = stats.peerCount.toString()
        
        // Pieces (have / total)
        pieces.text = "${stats.piecesHave} / ${stats.piecesTotal}"
    }
}
