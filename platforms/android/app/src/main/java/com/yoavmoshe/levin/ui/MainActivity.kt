package com.yoavmoshe.levin.ui

import android.Manifest
import android.content.pm.PackageManager
import android.net.ConnectivityManager
import android.net.NetworkCapabilities
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
            showPopulateDialog()
        }
    }

    private fun showPopulateDialog() {
        AlertDialog.Builder(this)
            .setTitle("Welcome to Levin")
            .setMessage(
                "Your torrent watch directory is empty. " +
                "Would you like to download torrent files from Anna's Archive?"
            )
            .setPositiveButton("Populate") { _, _ ->
                startPopulate()
            }
            .setNegativeButton("Skip") { _, _ ->
                // Only dismiss permanently when user explicitly skips
                getSharedPreferences(PREFS_NAME, MODE_PRIVATE)
                    .edit().putBoolean("first_run_dismissed", true).apply()
            }
            .setCancelable(false)
            .show()
    }

    private fun hasNetworkConnectivity(): Boolean {
        val cm = getSystemService(CONNECTIVITY_SERVICE) as ConnectivityManager
        val network = cm.activeNetwork ?: return false
        val caps = cm.getNetworkCapabilities(network) ?: return false
        return caps.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
    }

    private fun startPopulate() {
        if (!hasNetworkConnectivity()) {
            AlertDialog.Builder(this)
                .setTitle("No Internet Connection")
                .setMessage("Connect to the internet and try again.")
                .setPositiveButton("Retry") { _, _ -> startPopulate() }
                .setNegativeButton("Cancel") { _, _ -> }
                .show()
            return
        }

        val watchDir = File(filesDir, "watch")
        watchDir.mkdirs()

        // Show a progress dialog
        val progressDialog = AlertDialog.Builder(this)
            .setTitle("Downloading Torrents")
            .setMessage("Fetching torrent list from Anna's Archive...")
            .setCancelable(false)
            .setNegativeButton("Cancel") { dialog, _ ->
                dialog.dismiss()
            }
            .create()
        progressDialog.show()

        val thread = HandlerThread("PopulateWorker").also { it.start() }
        val handler = Handler(thread.looper)
        handler.post {
            val result = AnnaArchiveClient.populateTorrents(watchDir,
                object : AnnaArchiveClient.ProgressCallback {
                    override fun onProgress(current: Int, total: Int, message: String) {
                        runOnUiThread {
                            if (progressDialog.isShowing) {
                                if (total > 0) {
                                    progressDialog.setMessage("[$current/$total] $message")
                                } else {
                                    progressDialog.setMessage(message)
                                }
                            }
                        }
                    }
                })
            runOnUiThread {
                if (progressDialog.isShowing) {
                    progressDialog.dismiss()
                }

                if (result.downloaded > 0) {
                    // Success — dismiss the first-run flag
                    getSharedPreferences(PREFS_NAME, MODE_PRIVATE)
                        .edit().putBoolean("first_run_dismissed", true).apply()
                    Toast.makeText(this,
                        "Downloaded ${result.downloaded} torrents",
                        Toast.LENGTH_SHORT).show()
                } else if (result.downloaded == 0 && result.errorMessage == null) {
                    // All torrents already existed
                    getSharedPreferences(PREFS_NAME, MODE_PRIVATE)
                        .edit().putBoolean("first_run_dismissed", true).apply()
                    Toast.makeText(this,
                        "All torrents already present",
                        Toast.LENGTH_SHORT).show()
                } else {
                    // Failure — show error with retry option, don't dismiss first-run
                    val msg = result.errorMessage
                        ?: "Failed to download torrents."
                    AlertDialog.Builder(this)
                        .setTitle("Download Failed")
                        .setMessage(msg)
                        .setPositiveButton("Retry") { _, _ -> startPopulate() }
                        .setNegativeButton("Cancel") { _, _ -> }
                        .show()
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
