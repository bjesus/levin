package com.yoavmoshe.levin.ui

import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.HandlerThread
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import com.google.android.material.bottomnavigation.BottomNavigationView
import com.yoavmoshe.levin.AnnaArchiveClient
import com.yoavmoshe.levin.R
import com.yoavmoshe.levin.service.LevinService
import java.io.File

class MainActivity : AppCompatActivity() {

    companion object {
        private const val REQUEST_NOTIFICATION_PERMISSION = 100
        private const val PREFS_NAME = "levin_prefs"
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        requestNotificationPermission()

        val bottomNav = findViewById<BottomNavigationView>(R.id.bottom_navigation)
        bottomNav.setOnItemSelectedListener { item ->
            when (item.itemId) {
                R.id.nav_stats -> {
                    showFragment(StatsFragment())
                    true
                }
                R.id.nav_settings -> {
                    showFragment(SettingsFragment())
                    true
                }
                else -> false
            }
        }

        // Show stats by default
        if (savedInstanceState == null) {
            showFragment(StatsFragment())
            bottomNav.selectedItemId = R.id.nav_stats
        }

        // Start the service
        LevinService.start(this)

        // Check for first run (empty watch directory)
        if (savedInstanceState == null) {
            checkFirstRun()
        }
    }

    private fun checkFirstRun() {
        val watchDir = File(filesDir, "watch")
        val hasExistingTorrents = watchDir.exists() &&
                (watchDir.listFiles()?.any { it.extension == "torrent" } == true)

        val prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE)
        val firstRunDismissed = prefs.getBoolean("first_run_dismissed", false)

        if (!hasExistingTorrents && !firstRunDismissed) {
            AlertDialog.Builder(this)
                .setTitle("Welcome to Levin")
                .setMessage(
                    "Your torrent watch directory is empty. " +
                    "Would you like to download torrent files from Anna's Archive?"
                )
                .setPositiveButton("Populate") { _, _ ->
                    prefs.edit().putBoolean("first_run_dismissed", true).apply()
                    startPopulate()
                }
                .setNegativeButton("Later") { _, _ ->
                    prefs.edit().putBoolean("first_run_dismissed", true).apply()
                }
                .setCancelable(false)
                .show()
        }
    }

    private fun startPopulate() {
        val watchDir = File(filesDir, "watch")
        watchDir.mkdirs()

        Toast.makeText(this, "Fetching torrents from Anna's Archive...", Toast.LENGTH_SHORT).show()

        val thread = HandlerThread("PopulateWorker").also { it.start() }
        val handler = Handler(thread.looper)
        handler.post {
            val result = AnnaArchiveClient.populateTorrents(watchDir,
                object : AnnaArchiveClient.ProgressCallback {
                    override fun onProgress(current: Int, total: Int, message: String) {
                        // Progress updates not shown from MainActivity
                    }
                })
            runOnUiThread {
                if (result >= 0) {
                    Toast.makeText(this, "Downloaded $result torrents", Toast.LENGTH_SHORT).show()
                } else {
                    Toast.makeText(this, "Failed to fetch torrents. You can try again in Settings.", Toast.LENGTH_LONG).show()
                }
                thread.quitSafely()
            }
        }
    }

    private fun showFragment(fragment: androidx.fragment.app.Fragment) {
        supportFragmentManager.beginTransaction()
            .replace(R.id.fragment_container, fragment)
            .commit()
    }

    private fun requestNotificationPermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            if (ContextCompat.checkSelfPermission(
                    this, Manifest.permission.POST_NOTIFICATIONS
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
}
