package com.yoavmoshe.levin

import android.util.Log
import java.io.File
import java.io.FileOutputStream
import java.net.HttpURLConnection
import java.net.URL

/**
 * Kotlin implementation of the Anna's Archive torrent fetcher.
 * Uses Android's built-in HttpURLConnection instead of libcurl.
 *
 * Mirrors the logic in liblevin/src/annas_archive.cpp but runs in pure Kotlin.
 */
object AnnaArchiveClient {

    private const val TAG = "AnnaArchive"
    private const val TORRENT_LIST_URL =
        "https://annas-archive.li/dyn/generate_torrents?max_tb=1&format=url"
    private const val USER_AGENT = "levin/0.1"
    private const val TIMEOUT_MS = 30_000
    private const val MAX_RETRIES = 3

    interface ProgressCallback {
        fun onProgress(current: Int, total: Int, message: String)
    }

    /**
     * Fetch the list of torrent URLs from Anna's Archive.
     * Returns an empty list on failure.
     */
    fun fetchTorrentUrls(): List<String> {
        for (attempt in 0 until MAX_RETRIES) {
            try {
                val conn = URL(TORRENT_LIST_URL).openConnection() as HttpURLConnection
                conn.requestMethod = "GET"
                conn.connectTimeout = TIMEOUT_MS
                conn.readTimeout = TIMEOUT_MS
                conn.setRequestProperty("User-Agent", USER_AGENT)
                conn.instanceFollowRedirects = true

                val code = conn.responseCode
                if (code in 200..299) {
                    val body = conn.inputStream.bufferedReader().readText()
                    conn.disconnect()
                    return body.lines()
                        .map { it.trim() }
                        .filter { it.isNotEmpty() }
                }
                conn.disconnect()
                Log.w(TAG, "fetchTorrentUrls: HTTP $code on attempt ${attempt + 1}")
            } catch (e: Exception) {
                Log.w(TAG, "fetchTorrentUrls: attempt ${attempt + 1} failed: ${e.message}")
            }

            // Exponential backoff: 1s, 2s, 4s
            if (attempt + 1 < MAX_RETRIES) {
                Thread.sleep((1L shl attempt) * 1000)
            }
        }
        return emptyList()
    }

    /**
     * Download a file from [url] to [destFile].
     * Returns true on success.
     */
    fun downloadFile(url: String, destFile: File): Boolean {
        for (attempt in 0 until MAX_RETRIES) {
            try {
                val conn = URL(url).openConnection() as HttpURLConnection
                conn.requestMethod = "GET"
                conn.connectTimeout = TIMEOUT_MS
                conn.readTimeout = TIMEOUT_MS
                conn.setRequestProperty("User-Agent", USER_AGENT)
                conn.instanceFollowRedirects = true

                val code = conn.responseCode
                if (code in 200..299) {
                    FileOutputStream(destFile).use { fos ->
                        conn.inputStream.copyTo(fos)
                    }
                    conn.disconnect()
                    return true
                }
                conn.disconnect()
                Log.w(TAG, "downloadFile: HTTP $code for $url on attempt ${attempt + 1}")
            } catch (e: Exception) {
                Log.w(TAG, "downloadFile: attempt ${attempt + 1} failed for $url: ${e.message}")
            }

            // Remove partial download
            destFile.delete()

            // Exponential backoff
            if (attempt + 1 < MAX_RETRIES) {
                Thread.sleep((1L shl attempt) * 1000)
            }
        }
        return false
    }

    /**
     * Extract filename from a URL (last path component, without query params).
     */
    private fun filenameFromUrl(url: String): String? {
        val path = URL(url).path ?: return null
        val name = path.substringAfterLast('/')
        return if (name.isNotEmpty()) name else null
    }

    /**
     * Fetch torrent URLs from Anna's Archive and download .torrent files
     * into [watchDirectory].
     *
     * @return number of newly downloaded torrents, or -1 on failure.
     */
    fun populateTorrents(watchDirectory: File, callback: ProgressCallback? = null): Int {
        // Ensure directory exists
        if (!watchDirectory.exists() && !watchDirectory.mkdirs()) {
            Log.e(TAG, "Cannot create watch directory: $watchDirectory")
            return -1
        }

        // Fetch URL list
        val urls = fetchTorrentUrls()
        if (urls.isEmpty()) {
            Log.e(TAG, "No torrent URLs returned from Anna's Archive")
            return -1
        }

        val total = urls.size
        var downloaded = 0

        for ((i, url) in urls.withIndex()) {
            val filename = filenameFromUrl(url) ?: continue
            val destFile = File(watchDirectory, filename)

            // Skip existing files
            if (destFile.exists()) {
                callback?.onProgress(i + 1, total, "skipped (exists): $filename")
                continue
            }

            callback?.onProgress(i + 1, total, "downloading: $filename")

            if (downloadFile(url, destFile)) {
                downloaded++
            } else {
                callback?.onProgress(i + 1, total, "failed: $filename")
            }
        }

        return downloaded
    }
}
