package com.yoavmoshe.levin.ui

import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.CompoundButton
import android.widget.TextView
import com.google.android.material.materialswitch.MaterialSwitch
import androidx.fragment.app.Fragment
import com.yoavmoshe.levin.R
import com.yoavmoshe.levin.service.LevinService

class StatsFragment : Fragment() {

    private val handler = Handler(Looper.getMainLooper())
    private val refreshInterval = 1000L

    private lateinit var stateText: TextView
    private lateinit var torrentsText: TextView
    private lateinit var peersText: TextView
    private lateinit var downloadText: TextView
    private lateinit var uploadText: TextView
    private lateinit var totalDownText: TextView
    private lateinit var totalUpText: TextView
    private lateinit var diskUsageText: TextView
    private lateinit var diskBudgetText: TextView
    private lateinit var enableSwitch: MaterialSwitch
    private lateinit var serviceStatusText: TextView

    /** Suppress listener when updating the switch programmatically. */
    private var suppressSwitchListener = false

    private val switchListener = CompoundButton.OnCheckedChangeListener { _, isChecked ->
        if (suppressSwitchListener) return@OnCheckedChangeListener
        val ctx = requireContext()
        if (isChecked) {
            LevinService.start(ctx)
        } else {
            LevinService.stop(ctx)
        }
    }

    private val refreshRunnable = object : Runnable {
        override fun run() {
            updateUI()
            handler.postDelayed(this, refreshInterval)
        }
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        return inflater.inflate(R.layout.fragment_stats, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        stateText = view.findViewById(R.id.text_state)
        torrentsText = view.findViewById(R.id.text_torrents)
        peersText = view.findViewById(R.id.text_peers)
        downloadText = view.findViewById(R.id.text_download_rate)
        uploadText = view.findViewById(R.id.text_upload_rate)
        totalDownText = view.findViewById(R.id.text_total_downloaded)
        totalUpText = view.findViewById(R.id.text_total_uploaded)
        diskUsageText = view.findViewById(R.id.text_disk_usage)
        diskBudgetText = view.findViewById(R.id.text_disk_budget)
        enableSwitch = view.findViewById(R.id.switch_enable)
        serviceStatusText = view.findViewById(R.id.text_service_status)

        enableSwitch.setOnCheckedChangeListener(switchListener)
    }

    override fun onResume() {
        super.onResume()
        handler.post(refreshRunnable)
    }

    override fun onPause() {
        super.onPause()
        handler.removeCallbacks(refreshRunnable)
    }

    private fun updateUI() {
        val running = LevinService.isRunning
        serviceStatusText.text = if (running) "Running" else "Stopped"

        // Update switch without triggering listener
        suppressSwitchListener = true
        enableSwitch.isChecked = running
        suppressSwitchListener = false

        val status = LevinService.lastStatus
        if (status != null) {
            stateText.text = status.stateName
            torrentsText.text = "${status.torrentCount}"
            peersText.text = "${status.peerCount}"
            downloadText.text = formatRate(status.downloadRate)
            uploadText.text = formatRate(status.uploadRate)
            totalDownText.text = formatBytes(status.totalDownloaded)
            totalUpText.text = formatBytes(status.totalUploaded)
            diskUsageText.text = formatBytes(status.diskUsage) +
                    if (status.overBudget) " (!)" else ""
            diskBudgetText.text = formatBytes(status.diskBudget)
        } else {
            stateText.text = "--"
            torrentsText.text = "--"
            peersText.text = "--"
            downloadText.text = "--"
            uploadText.text = "--"
            totalDownText.text = "--"
            totalUpText.text = "--"
            diskUsageText.text = "--"
            diskBudgetText.text = "--"
        }
    }

    private fun formatRate(bytesPerSec: Int): String {
        return when {
            bytesPerSec >= 1_048_576 -> String.format("%.1f MB/s", bytesPerSec / 1_048_576.0)
            bytesPerSec >= 1024 -> String.format("%.1f KB/s", bytesPerSec / 1024.0)
            else -> "$bytesPerSec B/s"
        }
    }

    private fun formatBytes(bytes: Long): String {
        return when {
            bytes >= 1_073_741_824L -> String.format("%.2f GB", bytes / 1_073_741_824.0)
            bytes >= 1_048_576L -> String.format("%.1f MB", bytes / 1_048_576.0)
            bytes >= 1024L -> String.format("%.1f KB", bytes / 1024.0)
            else -> "$bytes B"
        }
    }
}
