package com.yoavmoshe.levin.ui

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import com.yoavmoshe.levin.R
import com.yoavmoshe.levin.data.StatisticsRepository
import com.yoavmoshe.levin.util.FormatUtils
import com.google.android.material.textview.MaterialTextView
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

/**
 * Fragment showing real-time statistics
 */
class StatsFragment : Fragment() {
    
    private lateinit var statsRepo: StatisticsRepository
    
    // View references
    private lateinit var downloadRate: MaterialTextView
    private lateinit var uploadRate: MaterialTextView
    private lateinit var downloaded: MaterialTextView
    private lateinit var uploaded: MaterialTextView
    private lateinit var lifetimeDownloaded: MaterialTextView
    private lateinit var lifetimeUploaded: MaterialTextView
    private lateinit var ratio: MaterialTextView
    private lateinit var activeTorrents: MaterialTextView
    private lateinit var paused: MaterialTextView
    
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
        
        // Bind views
        downloadRate = view.findViewById(R.id.download_rate)
        uploadRate = view.findViewById(R.id.upload_rate)
        downloaded = view.findViewById(R.id.downloaded)
        uploaded = view.findViewById(R.id.uploaded)
        lifetimeDownloaded = view.findViewById(R.id.lifetime_downloaded)
        lifetimeUploaded = view.findViewById(R.id.lifetime_uploaded)
        ratio = view.findViewById(R.id.ratio)
        activeTorrents = view.findViewById(R.id.active_torrents)
        paused = view.findViewById(R.id.paused)
        
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
        
        // Session stats
        downloadRate.text = FormatUtils.formatSpeed(stats.sessionDownloadRate)
        uploadRate.text = FormatUtils.formatSpeed(stats.sessionUploadRate)
        downloaded.text = FormatUtils.formatBytes(stats.sessionDownloaded)
        uploaded.text = FormatUtils.formatBytes(stats.sessionUploaded)
        
        // Lifetime stats
        lifetimeDownloaded.text = FormatUtils.formatBytes(stats.lifetimeDownloaded)
        lifetimeUploaded.text = FormatUtils.formatBytes(stats.lifetimeUploaded)
        ratio.text = FormatUtils.formatRatio(stats.lifetimeDownloaded, stats.lifetimeUploaded)
        
        // Status
        activeTorrents.text = stats.activeTorrents.toString()
        paused.text = if (stats.isPaused) "Yes" else "No"
    }
}
