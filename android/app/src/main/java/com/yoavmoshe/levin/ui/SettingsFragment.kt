package com.yoavmoshe.levin.ui

import android.os.Bundle
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
        
        // Update preferences with current values
        findPreference<SwitchPreferenceCompat>("run_on_battery")?.apply {
            isChecked = settings.runOnBattery
            setOnPreferenceChangeListener { _, newValue ->
                settingsRepo.save(settings.copy(runOnBattery = newValue as Boolean))
                true
            }
        }
        
        findPreference<SwitchPreferenceCompat>("run_on_cellular")?.apply {
            isChecked = settings.runOnCellular
            setOnPreferenceChangeListener { _, newValue ->
                settingsRepo.save(settings.copy(runOnCellular = newValue as Boolean))
                true
            }
        }
    }
}
