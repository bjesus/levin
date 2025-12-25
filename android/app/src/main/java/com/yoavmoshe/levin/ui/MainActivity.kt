package com.yoavmoshe.levin.ui

import android.Manifest
import android.app.ProgressDialog
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.util.Log
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.NavHostFragment
import androidx.navigation.ui.setupWithNavController
import com.google.android.material.appbar.MaterialToolbar
import com.google.android.material.bottomnavigation.BottomNavigationView
import com.yoavmoshe.levin.R
import com.yoavmoshe.levin.data.SettingsRepository
import com.yoavmoshe.levin.service.LevinService
import com.yoavmoshe.levin.util.AnnasArchivePopulator
import kotlinx.coroutines.launch

/**
 * Main activity with bottom navigation
 */
class MainActivity : AppCompatActivity() {
    
    companion object {
        private const val TAG = "MainActivity"
        private const val REQUEST_NOTIFICATION_PERMISSION = 1
    }
    
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        
        // Setup toolbar
        val toolbar = findViewById<MaterialToolbar>(R.id.toolbar)
        setSupportActionBar(toolbar)
        
        // Setup navigation
        val navHostFragment = supportFragmentManager
            .findFragmentById(R.id.nav_host_fragment) as NavHostFragment
        val navController = navHostFragment.navController
        
        val bottomNav = findViewById<BottomNavigationView>(R.id.bottom_navigation)
        bottomNav.setupWithNavController(navController)
        
        // Request notification permission (Android 13+)
        requestNotificationPermission()
        
        // Start the service
        startLevinService()
        
        // Check if we need to populate torrents from Anna's Archive
        checkAndPopulateTorrents()
    }
    
    private fun requestNotificationPermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            if (ContextCompat.checkSelfPermission(
                    this,
                    Manifest.permission.POST_NOTIFICATIONS
                ) != PackageManager.PERMISSION_GRANTED
            ) {
                ActivityCompat.requestPermissions(
                    this,
                    arrayOf(Manifest.permission.POST_NOTIFICATIONS),
                    REQUEST_NOTIFICATION_PERMISSION
                )
            }
        }
    }
    
    private fun startLevinService() {
        val intent = Intent(this, LevinService::class.java).apply {
            action = LevinService.ACTION_START
        }
        
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            startForegroundService(intent)
        } else {
            startService(intent)
        }
    }
    
    /**
     * Check if torrents directory is empty and prompt user to populate it
     */
    private fun checkAndPopulateTorrents() {
        val settingsRepo = SettingsRepository(this)
        val settings = settingsRepo.load()
        
        // Check if watch directory is empty (no .torrent files)
        val torrentFiles = settings.watchDirectory.listFiles { file ->
            file.extension == "torrent"
        } ?: emptyArray()
        
        if (torrentFiles.isEmpty()) {
            Log.i(TAG, "No torrents found, showing populate dialog")
            showPopulateTorrentsDialog()
        }
    }
    
    /**
     * Show dialog asking user if they want to populate torrents
     */
    private fun showPopulateTorrentsDialog() {
        AlertDialog.Builder(this)
            .setTitle("Add Torrents?")
            .setMessage("Would you like to automatically add torrents from Anna's Archive to get started?")
            .setPositiveButton("Yes") { _, _ ->
                startPopulatingTorrents()
            }
            .setNegativeButton("No", null)
            .setCancelable(false)
            .show()
    }
    
    /**
     * Start downloading and populating torrents from Anna's Archive.
     * Shows progress dialog while downloading.
     */
    fun startPopulatingTorrents() {
        val settingsRepo = SettingsRepository(this)
        val settings = settingsRepo.load()
        val populator = AnnasArchivePopulator(settings.watchDirectory)
        
        // Show progress dialog
        @Suppress("DEPRECATION")
        val progressDialog = ProgressDialog(this).apply {
            setTitle("Downloading Torrents")
            setMessage("Preparing...")
            setProgressStyle(ProgressDialog.STYLE_HORIZONTAL)
            setCancelable(false)
            show()
        }
        
        lifecycleScope.launch {
            try {
                val result = populator.populate { current, total ->
                    runOnUiThread {
                        progressDialog.max = total
                        progressDialog.progress = current
                        progressDialog.setMessage("Downloading: $current/$total")
                    }
                }
                
                progressDialog.dismiss()
                
                // Show result
                val message = buildString {
                    append("Download complete!\n\n")
                    append("Successfully downloaded: ${result.successful} torrents")
                    if (result.failed > 0) {
                        append("\nFailed: ${result.failed} torrents")
                    }
                }
                
                AlertDialog.Builder(this@MainActivity)
                    .setTitle("Success")
                    .setMessage(message)
                    .setPositiveButton("OK") { _, _ ->
                        // Reload torrents in service
                        val intent = Intent(this@MainActivity, LevinService::class.java).apply {
                            action = LevinService.ACTION_RELOAD_TORRENTS
                        }
                        startService(intent)
                    }
                    .show()
                    
            } catch (e: Exception) {
                Log.e(TAG, "Failed to populate torrents", e)
                progressDialog.dismiss()
                
                AlertDialog.Builder(this@MainActivity)
                    .setTitle("Error")
                    .setMessage("Failed to download torrents: ${e.message}\n\nYou can try again from Settings.")
                    .setPositiveButton("OK", null)
                    .show()
            }
        }
    }
}
