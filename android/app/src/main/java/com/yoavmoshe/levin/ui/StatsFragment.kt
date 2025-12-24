package com.yoavmoshe.levin.ui

import android.content.Intent
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import com.google.android.material.button.MaterialButton
import com.google.android.material.textview.MaterialTextView
import com.yoavmoshe.levin.R
import com.yoavmoshe.levin.data.StatisticsRepository
import com.yoavmoshe.levin.service.LevinService
import com.yoavmoshe.levin.util.FormatUtils
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

/**
 * Fragment showing real-time statistics with controls
 */
class StatsFragment : Fragment() {
    
    private lateinit var statsRepo: StatisticsRepository
    
    // Status card views
    private lateinit var statusText: MaterialTextView
    private lateinit var currentSpeed: MaterialTextView
    private lateinit var activeTorrentsStatus: MaterialTextView
    private lateinit var pauseResumeButton: MaterialButton
    private lateinit var reloadButton: MaterialButton
    
    // Stats views (combined table)
    private lateinit var downloaded: MaterialTextView
    private lateinit var uploaded: MaterialTextView
    private lateinit var lifetimeDownloaded: MaterialTextView
    private lateinit var lifetimeUploaded: MaterialTextView
    private lateinit var sessionRatio: MaterialTextView
    private lateinit var ratio: MaterialTextView
    
    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        return inflater.inflate(R.layout.fragment_stats, container, false)
    }
    
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        
        // Initialize repository
        statsRepo = StatisticsRepository(requireContext())
        
        // Bind status card views
        statusText = view.findViewById(R.id.status_text)
        currentSpeed = view.findViewById(R.id.current_speed)
        activeTorrentsStatus = view.findViewById(R.id.active_torrents_status)
        pauseResumeButton = view.findViewById(R.id.pause_resume_button)
        reloadButton = view.findViewById(R.id.reload_button)
        
        // Bind stats views (combined table)
        downloaded = view.findViewById(R.id.downloaded)
        uploaded = view.findViewById(R.id.uploaded)
        lifetimeDownloaded = view.findViewById(R.id.lifetime_downloaded)
        lifetimeUploaded = view.findViewById(R.id.lifetime_uploaded)
        sessionRatio = view.findViewById(R.id.session_ratio)
        ratio = view.findViewById(R.id.ratio)
        
        // Setup button listeners
        pauseResumeButton.setOnClickListener {
            val stats = statsRepo.load()
            val action = if (stats.isPaused) {
                LevinService.ACTION_RESUME
            } else {
                LevinService.ACTION_PAUSE
            }
            val intent = Intent(requireContext(), LevinService::class.java).apply {
                this.action = action
            }
            requireContext().startService(intent)
        }
        
        reloadButton.setOnClickListener {
            // Send intent to service to reload torrents
            val intent = Intent(requireContext(), LevinService::class.java).apply {
                action = "com.yoavmoshe.levin.RELOAD_TORRENTS"
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
    
    private fun updateStats() {
        val stats = statsRepo.load()
        
        // Update status card
        // Priority: 1) Check if paused first, 2) Then check if no torrents
        if (stats.isPaused) {
            statusText.text = "Paused"
        } else if (stats.activeTorrents == 0) {
            statusText.text = "No Active Torrents"
        } else {
            statusText.text = "Running"
        }
        
        currentSpeed.text = "⬇ ${FormatUtils.formatSpeed(stats.sessionDownloadRate)}  ⬆ ${FormatUtils.formatSpeed(stats.sessionUploadRate)}"
        
        activeTorrentsStatus.text = "${stats.activeTorrents} active torrent${if (stats.activeTorrents != 1) "s" else ""}"
        
        pauseResumeButton.text = if (stats.isPaused) "Resume" else "Pause"
        
        // Session stats (combined table)
        downloaded.text = FormatUtils.formatSize(stats.sessionDownloaded)
        uploaded.text = FormatUtils.formatSize(stats.sessionUploaded)
        
        // Lifetime stats (combined table)
        lifetimeDownloaded.text = FormatUtils.formatSize(stats.lifetimeDownloaded)
        lifetimeUploaded.text = FormatUtils.formatSize(stats.lifetimeUploaded)
        
        // Session ratio
        sessionRatio.text = FormatUtils.formatRatio(stats.sessionRatio)
        
        // Lifetime ratio
        ratio.text = FormatUtils.formatRatio(stats.lifetimeRatio)
    }
}
