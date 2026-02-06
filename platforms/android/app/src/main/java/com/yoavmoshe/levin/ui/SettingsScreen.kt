package com.yoavmoshe.levin.ui

import android.content.Context
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.EditText
import android.widget.Switch
import android.widget.Toast
import androidx.fragment.app.Fragment
import com.yoavmoshe.levin.R

class SettingsFragment : Fragment() {

    companion object {
        private const val PREFS_NAME = "levin_prefs"
    }

    private lateinit var minFreeEdit: EditText
    private lateinit var maxStorageEdit: EditText
    private lateinit var batterySwitch: Switch
    private lateinit var cellularSwitch: Switch
    private lateinit var saveButton: Button

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
        saveButton = view.findViewById(R.id.button_save)

        loadSettings()

        saveButton.setOnClickListener {
            saveSettings()
            Toast.makeText(
                requireContext(),
                "Settings saved. Restart service to apply.",
                Toast.LENGTH_SHORT
            ).show()
        }
    }

    private fun loadSettings() {
        val prefs = requireContext().getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        minFreeEdit.setText(String.format("%.1f", prefs.getFloat("min_free_gb", 2.0f)))
        maxStorageEdit.setText(String.format("%.1f", prefs.getFloat("max_storage_gb", 0.0f)))
        batterySwitch.isChecked = prefs.getBoolean("run_on_battery", false)
        cellularSwitch.isChecked = prefs.getBoolean("run_on_cellular", false)
    }

    private fun saveSettings() {
        val prefs = requireContext().getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        prefs.edit().apply {
            putFloat("min_free_gb", minFreeEdit.text.toString().toFloatOrNull() ?: 2.0f)
            putFloat("max_storage_gb", maxStorageEdit.text.toString().toFloatOrNull() ?: 0.0f)
            putBoolean("run_on_battery", batterySwitch.isChecked)
            putBoolean("run_on_cellular", cellularSwitch.isChecked)
            apply()
        }
    }
}
