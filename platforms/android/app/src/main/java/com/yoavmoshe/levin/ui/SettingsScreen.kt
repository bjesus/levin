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
import android.widget.Switch
import android.widget.Toast
import androidx.fragment.app.Fragment
import com.yoavmoshe.levin.LevinNative
import com.yoavmoshe.levin.R
import com.yoavmoshe.levin.service.LevinService

class SettingsFragment : Fragment() {

    companion object {
        private const val PREFS_NAME = "levin_prefs"
    }

    private lateinit var minFreeEdit: EditText
    private lateinit var maxStorageEdit: EditText
    private lateinit var batterySwitch: Switch
    private lateinit var cellularSwitch: Switch
    private lateinit var startupSwitch: Switch
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
        batterySwitch = view.findViewById(R.id.switch_battery)
        cellularSwitch = view.findViewById(R.id.switch_cellular)
        startupSwitch = view.findViewById(R.id.switch_startup)
        saveButton = view.findViewById(R.id.button_save)
        populateButton = view.findViewById(R.id.button_populate)

        loadSettings()

        saveButton.setOnClickListener {
            saveSettings()
            Toast.makeText(
                requireContext(),
                "Settings saved. Restart service to apply.",
                Toast.LENGTH_SHORT
            ).show()
        }

        populateButton.setOnClickListener {
            populateButton.isEnabled = false
            populateButton.text = "Populating..."
            Toast.makeText(requireContext(), "Fetching torrents from Anna's Archive...", Toast.LENGTH_SHORT).show()

            // Run populate on a background thread
            val thread = HandlerThread("PopulateWorker").also { it.start() }
            val handler = Handler(thread.looper)
            handler.post {
                // Note: This requires the service to be running (needs a levin handle).
                // For now, show a message that this feature requires libcurl on Android.
                val result = -1 // Stub: Android doesn't have libcurl yet
                requireActivity().runOnUiThread {
                    populateButton.isEnabled = true
                    populateButton.text = "Populate Torrents from Anna's Archive"
                    if (result >= 0) {
                        Toast.makeText(requireContext(), "Downloaded $result torrents", Toast.LENGTH_SHORT).show()
                    } else {
                        Toast.makeText(requireContext(), "Populate not available yet (requires libcurl)", Toast.LENGTH_LONG).show()
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
        batterySwitch.isChecked = prefs.getBoolean("run_on_battery", false)
        cellularSwitch.isChecked = prefs.getBoolean("run_on_cellular", false)
        startupSwitch.isChecked = prefs.getBoolean("run_on_startup", false)
    }

    private fun saveSettings() {
        val prefs = requireContext().getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        val runOnStartup = startupSwitch.isChecked
        prefs.edit().apply {
            putFloat("min_free_gb", minFreeEdit.text.toString().toFloatOrNull() ?: 2.0f)
            putFloat("max_storage_gb", maxStorageEdit.text.toString().toFloatOrNull() ?: 0.0f)
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
