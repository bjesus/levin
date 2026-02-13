package com.yoavmoshe.levin.ui

import android.content.ComponentName
import android.content.Context
import android.content.pm.PackageManager
import android.os.Bundle
import android.os.Handler
import android.os.HandlerThread
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.EditText
import android.widget.Toast
import androidx.appcompat.widget.SwitchCompat
import androidx.fragment.app.Fragment
import com.yoavmoshe.levin.AnnaArchiveClient
import com.yoavmoshe.levin.R
import com.yoavmoshe.levin.service.LevinService
import java.io.File

class SettingsFragment : Fragment() {

    companion object {
        private const val PREFS_NAME = "levin_prefs"
    }

    private lateinit var minFreeEdit: EditText
    private lateinit var maxStorageEdit: EditText
    private lateinit var maxDownloadEdit: EditText
    private lateinit var maxUploadEdit: EditText
    private lateinit var batterySwitch: SwitchCompat
    private lateinit var cellularSwitch: SwitchCompat
    private lateinit var startupSwitch: SwitchCompat
    private lateinit var saveButton: Button
    private lateinit var populateButton: Button

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        return inflater.inflate(R.layout.fragment_settings, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        minFreeEdit = view.findViewById(R.id.edit_min_free)
        maxStorageEdit = view.findViewById(R.id.edit_max_storage)
        maxDownloadEdit = view.findViewById(R.id.edit_max_download)
        maxUploadEdit = view.findViewById(R.id.edit_max_upload)
        batterySwitch = view.findViewById(R.id.switch_battery)
        cellularSwitch = view.findViewById(R.id.switch_cellular)
        startupSwitch = view.findViewById(R.id.switch_startup)
        saveButton = view.findViewById(R.id.button_save)
        populateButton = view.findViewById(R.id.button_populate)

        loadSettings()

        saveButton.setOnClickListener {
            saveSettings()
            // Apply runtime-changeable settings immediately
            LevinService.applySettings(requireContext())
            Toast.makeText(
                requireContext(),
                "Settings saved and applied.",
                Toast.LENGTH_SHORT
            ).show()
        }

        populateButton.setOnClickListener {
            populateButton.isEnabled = false
            populateButton.text = "Populating..."

            val watchDir = File(requireContext().filesDir, "watch")

            // Run populate on a background thread
            val thread = HandlerThread("PopulateWorker").also { it.start() }
            val handler = Handler(thread.looper)
            handler.post {
                val result = AnnaArchiveClient.populateTorrents(watchDir,
                    object : AnnaArchiveClient.ProgressCallback {
                        override fun onProgress(current: Int, total: Int, message: String) {
                            activity?.runOnUiThread {
                                if (total > 0) {
                                    populateButton.text = "[$current/$total] $message"
                                } else {
                                    populateButton.text = message
                                }
                            }
                        }
                    })

                activity?.runOnUiThread {
                    populateButton.isEnabled = true
                    populateButton.text = "Populate Torrents from Anna's Archive"
                    if (result.downloaded >= 0 && result.errorMessage == null) {
                        Toast.makeText(requireContext(),
                            "Downloaded ${result.downloaded} torrents",
                            Toast.LENGTH_SHORT).show()
                    } else {
                        val msg = result.errorMessage ?: "Failed to fetch torrents."
                        Toast.makeText(requireContext(), msg, Toast.LENGTH_LONG).show()
                    }
                    thread.quitSafely()
                }
            }
        }
    }

    private fun loadSettings() {
        val prefs = requireContext().getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        minFreeEdit.setText(String.format("%.1f", prefs.getFloat("min_free_gb", 2.0f)))
        maxStorageEdit.setText(String.format("%.1f", prefs.getFloat("max_storage_gb", 0.0f)))
        maxDownloadEdit.setText(prefs.getInt("max_download_kbps", 0).toString())
        maxUploadEdit.setText(prefs.getInt("max_upload_kbps", 0).toString())
        batterySwitch.isChecked = prefs.getBoolean("run_on_battery", false)
        cellularSwitch.isChecked = prefs.getBoolean("run_on_cellular", false)
        startupSwitch.isChecked = prefs.getBoolean("run_on_startup", false)
    }

    private fun saveSettings() {
        val prefs = requireContext().getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        val runOnStartup = startupSwitch.isChecked

        // Validate numeric inputs (clamp to non-negative)
        val minFreeGb = (minFreeEdit.text.toString().toFloatOrNull() ?: 2.0f).coerceAtLeast(0f)
        val maxStorageGb = (maxStorageEdit.text.toString().toFloatOrNull() ?: 0.0f).coerceAtLeast(0f)
        val maxDownloadKbps = (maxDownloadEdit.text.toString().toIntOrNull() ?: 0).coerceAtLeast(0)
        val maxUploadKbps = (maxUploadEdit.text.toString().toIntOrNull() ?: 0).coerceAtLeast(0)

        prefs.edit().apply {
            putFloat("min_free_gb", minFreeGb)
            putFloat("max_storage_gb", maxStorageGb)
            putInt("max_download_kbps", maxDownloadKbps)
            putInt("max_upload_kbps", maxUploadKbps)
            putBoolean("run_on_battery", batterySwitch.isChecked)
            putBoolean("run_on_cellular", cellularSwitch.isChecked)
            putBoolean("run_on_startup", runOnStartup)
            apply()
        }

        // Enable/disable the BootReceiver based on run_on_startup setting
        val receiver = ComponentName(requireContext(), "com.yoavmoshe.levin.BootReceiver")
        val newState = if (runOnStartup)
            PackageManager.COMPONENT_ENABLED_STATE_ENABLED
        else
            PackageManager.COMPONENT_ENABLED_STATE_DISABLED
        requireContext().packageManager.setComponentEnabledSetting(
            receiver, newState, PackageManager.DONT_KILL_APP
        )
    }
}
