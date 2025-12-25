package com.yoavmoshe.levin.util

import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import okhttp3.Request
import java.io.File
import java.io.IOException
import java.util.concurrent.TimeUnit

/**
 * Result of torrent population operation
 */
data class PopulateResult(
    val total: Int,
    val successful: Int,
    val failed: Int,
    val failedUrls: List<String> = emptyList()
)

/**
 * Utility for populating Levin's watch directory with torrents from Anna's Archive.
 * 
 * Usage:
 *   val populator = AnnasArchivePopulator(watchDirectory)
 *   val result = populator.populate { current, total ->
 *       // Update UI with progress
 *   }
 */
class AnnasArchivePopulator(
    private val watchDirectory: File
) {
    companion object {
        private const val TAG = "AnnasArchivePopulator"
        private const val ANNAS_ARCHIVE_URL = 
            "https://annas-archive.org/dyn/generate_torrents?max_tb=1&format=url"
        private const val MAX_RETRIES = 3
        private const val INITIAL_BACKOFF_MS = 1000L  // 1 second
        private const val TIMEOUT_SECONDS = 30L
    }
    
    private val client = OkHttpClient.Builder()
        .connectTimeout(TIMEOUT_SECONDS, TimeUnit.SECONDS)
        .readTimeout(TIMEOUT_SECONDS, TimeUnit.SECONDS)
        .writeTimeout(TIMEOUT_SECONDS, TimeUnit.SECONDS)
        .build()
    
    /**
     * Fetch list of torrent URLs from Anna's Archive.
     * Retries up to MAX_RETRIES times with exponential backoff.
     * 
     * @throws IOException if fetch fails after all retries
     */
    suspend fun fetchTorrentUrls(): List<String> = withContext(Dispatchers.IO) {
        Log.i(TAG, "Fetching torrent list from Anna's Archive")
        
        var lastException: IOException? = null
        var backoffMs = INITIAL_BACKOFF_MS
        
        for (attempt in 1..MAX_RETRIES) {
            try {
                if (attempt > 1) {
                    Log.w(TAG, "Retry attempt $attempt/$MAX_RETRIES after ${backoffMs}ms")
                    delay(backoffMs)
                    backoffMs *= 2  // Exponential backoff
                }
                
                val request = Request.Builder()
                    .url(ANNAS_ARCHIVE_URL)
                    .header("User-Agent", "Levin/${com.yoavmoshe.levin.BuildConfig.VERSION_NAME}")
                    .build()
                
                client.newCall(request).execute().use { response ->
                    if (!response.isSuccessful) {
                        throw IOException("HTTP error: ${response.code}")
                    }
                    
                    val body = response.body?.string() 
                        ?: throw IOException("Empty response body")
                    
                    // Split by newlines and filter empty lines
                    val urls = body.lines()
                        .map { it.trim() }
                        .filter { it.isNotEmpty() }
                    
                    Log.i(TAG, "Fetched ${urls.size} torrent URLs from Anna's Archive")
                    return@withContext urls
                }
            } catch (e: IOException) {
                Log.w(TAG, "Failed to fetch torrent list (attempt $attempt/$MAX_RETRIES): ${e.message}")
                lastException = e
            }
        }
        
        throw lastException 
            ?: IOException("Failed to fetch torrent list after $MAX_RETRIES attempts")
    }
    
    /**
     * Download a single torrent file with retry logic.
     * 
     * @param url URL of the torrent file
     * @param outputFile File where to save the torrent
     * @return true if download succeeded, false otherwise
     */
    private suspend fun downloadTorrent(url: String, outputFile: File): Boolean = withContext(Dispatchers.IO) {
        var backoffMs = INITIAL_BACKOFF_MS
        
        for (attempt in 1..MAX_RETRIES) {
            try {
                if (attempt > 1) {
                    Log.d(TAG, "Retry downloading $url (attempt $attempt/$MAX_RETRIES)")
                    delay(backoffMs)
                    backoffMs *= 2  // Exponential backoff
                }
                
                val request = Request.Builder()
                    .url(url)
                    .header("User-Agent", "Levin/${com.yoavmoshe.levin.BuildConfig.VERSION_NAME}")
                    .build()
                
                val response = client.newCall(request).execute()
                
                if (!response.isSuccessful) {
                    Log.w(TAG, "HTTP error ${response.code} for $url")
                    response.close()
                    continue
                }
                
                val body = response.body
                if (body == null) {
                    Log.w(TAG, "Empty response body for $url")
                    response.close()
                    continue
                }
                
                // Write to file
                try {
                    outputFile.outputStream().use { output ->
                        body.byteStream().copyTo(output)
                    }
                    
                    Log.d(TAG, "Successfully downloaded: ${outputFile.name}")
                    return@withContext true
                } finally {
                    response.close()
                }
            } catch (e: Exception) {
                Log.w(TAG, "Failed to download $url (attempt $attempt/$MAX_RETRIES): ${e.message}")
            }
        }
        
        Log.e(TAG, "Failed to download $url after $MAX_RETRIES attempts")
        return@withContext false
    }
    
    /**
     * Extract filename from URL (last path segment).
     * Falls back to hash-based name if extraction fails.
     */
    private fun extractFilename(url: String): String {
        return try {
            val lastSlash = url.lastIndexOf('/')
            if (lastSlash >= 0 && lastSlash + 1 < url.length) {
                url.substring(lastSlash + 1)
            } else {
                "torrent_${url.hashCode()}.torrent"
            }
        } catch (e: Exception) {
            "torrent_${url.hashCode()}.torrent"
        }
    }
    
    /**
     * Populate watch directory with torrents from Anna's Archive.
     * 
     * @param onProgress Callback for progress updates (current, total)
     * @return PopulateResult with download statistics
     * @throws IOException if unable to fetch torrent list
     */
    suspend fun populate(
        onProgress: (current: Int, total: Int) -> Unit
    ): PopulateResult = withContext(Dispatchers.IO) {
        // Ensure watch directory exists
        if (!watchDirectory.exists()) {
            watchDirectory.mkdirs()
        }
        
        // Fetch list of torrent URLs
        val urls = fetchTorrentUrls()
        
        var successful = 0
        var failed = 0
        val failedUrls = mutableListOf<String>()
        
        Log.i(TAG, "Downloading ${urls.size} torrents to ${watchDirectory.absolutePath}")
        
        // Download each torrent
        for (index in urls.indices) {
            val url = urls[index]
            val filename = extractFilename(url)
            val outputFile = File(watchDirectory, filename)
            
            // Skip if already exists
            if (outputFile.exists()) {
                Log.d(TAG, "Torrent already exists, skipping: $filename")
                successful++
                onProgress(index + 1, urls.size)
                continue
            }
            
            // Download
            if (downloadTorrent(url, outputFile)) {
                successful++
            } else {
                failed++
                failedUrls.add(url)
            }
            
            // Progress callback
            onProgress(index + 1, urls.size)
        }
        
        Log.i(TAG, "Download complete: $successful/${urls.size} successful, $failed failed")
        
        PopulateResult(
            total = urls.size,
            successful = successful,
            failed = failed,
            failedUrls = failedUrls
        )
    }
}
