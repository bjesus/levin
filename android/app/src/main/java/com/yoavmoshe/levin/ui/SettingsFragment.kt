package com.yoavmoshe.levin.ui

import android.os.Bundle
import android.os.StatFs
import android.widget.Toast
import androidx.preference.EditTextPreference
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.SwitchPreferenceCompat
import com.yoavmoshe.levin.R
import com.yoavmoshe.levin.data.SettingsRepository

/**
 * Fragment showing app settings
 */
class SettingsFragment : PreferenceFragmentCompat() {
    
    private lateinit var settingsRepo: SettingsRepository
    
    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.preferences, rootKey)
        
        // Initialize repository
        settingsRepo = SettingsRepository(requireContext())
        
        // Load current settings
        val settings = settingsRepo.load()
        
        // Helper function to format GB values
        fun formatGb(gb: Double): String {
            return if (gb == gb.toLong().toDouble()) {
                gb.toLong().toString()
            } else {
                String.format("%.1f", gb)
            }
        }
        
        // Minimum Free Space preference (REQUIRED)
        findPreference<EditTextPreference>("min_free")?.apply {
            val currentGb = settings.minFree / (1024.0 * 1024.0 * 1024.0)
            text = formatGb(currentGb)
            summary = "${text} GB"
            
            setOnPreferenceChangeListener { _, newValue ->
                val inputStr = (newValue as String).trim()
                
                // Validation 1: Required - cannot be empty
                if (inputStr.isEmpty()) {
                    Toast.makeText(
                        requireContext(),
                        "Minimum free space is required",
                        Toast.LENGTH_SHORT
                    ).show()
                    return@setOnPreferenceChangeListener false
                }
                
                // Validation 2: Must be a valid number
                val gbValue = inputStr.toDoubleOrNull()
                if (gbValue == null) {
                    Toast.makeText(
                        requireContext(),
                        "Please enter a valid number (e.g., 5 or 1.5)",
                        Toast.LENGTH_SHORT
                    ).show()
                    return@setOnPreferenceChangeListener false
                }
                
                // Validation 3: Must be positive
                if (gbValue <= 0) {
                    Toast.makeText(
                        requireContext(),
                        "Minimum free space must be greater than 0 GB",
                        Toast.LENGTH_SHORT
                    ).show()
                    return@setOnPreferenceChangeListener false
                }
                
                // Validation 4: Cannot exceed total disk space
                val currentSettings = settingsRepo.load()
                val dataDir = currentSettings.dataDirectory
                val stat = StatFs(dataDir.path)
                val totalGb = stat.totalBytes / (1024.0 * 1024.0 * 1024.0)
                
                if (gbValue > totalGb) {
                    Toast.makeText(
                        requireContext(),
                        String.format(
                            "Cannot exceed total disk space (%.1f GB)",
                            totalGb
                        ),
                        Toast.LENGTH_LONG
                    ).show()
                    return@setOnPreferenceChangeListener false
                }
                
                // All validations passed - save the value
                val bytes = (gbValue * 1024.0 * 1024.0 * 1024.0).toLong()
                settingsRepo.save(currentSettings.copy(minFree = bytes))
                
                summary = "${formatGb(gbValue)} GB"
                Toast.makeText(
                    requireContext(),
                    "Minimum free space updated to ${formatGb(gbValue)} GB",
                    Toast.LENGTH_SHORT
                ).show()
                
                true
            }
        }
        
        // Maximum Storage preference (OPTIONAL)
        findPreference<EditTextPreference>("max_storage")?.apply {
            if (settings.maxStorage != null) {
                val currentGb = settings.maxStorage / (1024.0 * 1024.0 * 1024.0)
                text = formatGb(currentGb)
                summary = "${text} GB"
            } else {
                text = ""
                summary = "Unlimited"
            }
            
            setOnPreferenceChangeListener { _, newValue ->
                val inputStr = (newValue as String).trim()
                
                // Empty = unlimited (valid)
                if (inputStr.isEmpty()) {
                    val currentSettings = settingsRepo.load()
                    settingsRepo.save(currentSettings.copy(maxStorage = null))
                    summary = "Unlimited"
                    Toast.makeText(
                        requireContext(),
                        "Maximum storage set to unlimited",
                        Toast.LENGTH_SHORT
                    ).show()
                    return@setOnPreferenceChangeListener true
                }
                
                // Validation 1: Must be a valid number
                val gbValue = inputStr.toDoubleOrNull()
                if (gbValue == null) {
                    Toast.makeText(
                        requireContext(),
                        "Please enter a valid number or leave empty for unlimited",
                        Toast.LENGTH_SHORT
                    ).show()
                    return@setOnPreferenceChangeListener false
                }
                
                // Validation 2: Must be positive
                if (gbValue <= 0) {
                    Toast.makeText(
                        requireContext(),
                        "Maximum storage must be greater than 0 GB",
                        Toast.LENGTH_SHORT
                    ).show()
                    return@setOnPreferenceChangeListener false
                }
                
                // Validation 3: Cannot exceed available disk space
                val currentSettings = settingsRepo.load()
                val dataDir = currentSettings.dataDirectory
                val stat = StatFs(dataDir.path)
                val availableGb = stat.availableBytes / (1024.0 * 1024.0 * 1024.0)
                
                if (gbValue > availableGb) {
                    Toast.makeText(
                        requireContext(),
                        String.format(
                            "Cannot exceed available disk space (%.1f GB)",
                            availableGb
                        ),
                        Toast.LENGTH_LONG
                    ).show()
                    return@setOnPreferenceChangeListener false
                }
                
                // All validations passed - save the value
                val bytes = (gbValue * 1024.0 * 1024.0 * 1024.0).toLong()
                settingsRepo.save(currentSettings.copy(maxStorage = bytes))
                
                summary = "${formatGb(gbValue)} GB"
                Toast.makeText(
                    requireContext(),
                    "Maximum storage updated to ${formatGb(gbValue)} GB",
                    Toast.LENGTH_SHORT
                ).show()
                
                true
            }
        }
        
        // Run on Startup preference
        findPreference<SwitchPreferenceCompat>("run_on_startup")?.apply {
            isChecked = settings.runOnStartup
            setOnPreferenceChangeListener { _, newValue ->
                val currentSettings = settingsRepo.load()
                settingsRepo.save(currentSettings.copy(runOnStartup = newValue as Boolean))
                true
            }
        }
        
        // Run on Battery preference
        findPreference<SwitchPreferenceCompat>("run_on_battery")?.apply {
            isChecked = settings.runOnBattery
            setOnPreferenceChangeListener { _, newValue ->
                val currentSettings = settingsRepo.load()
                settingsRepo.save(currentSettings.copy(runOnBattery = newValue as Boolean))
                true
            }
        }
        
        // Run on Cellular preference
        findPreference<SwitchPreferenceCompat>("run_on_cellular")?.apply {
            isChecked = settings.runOnCellular
            setOnPreferenceChangeListener { _, newValue ->
                val currentSettings = settingsRepo.load()
                settingsRepo.save(currentSettings.copy(runOnCellular = newValue as Boolean))
                true
            }
        }
    }
}
