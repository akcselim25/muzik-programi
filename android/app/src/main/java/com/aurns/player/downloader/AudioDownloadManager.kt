package com.aurns.player.downloader

import android.app.DownloadManager
import android.content.Context
import android.net.Uri
import android.os.Environment
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import java.io.File

data class DownloadProgress(
    val title: String,
    val percent: Int,
    val isRunning: Boolean,
    val isDone: Boolean = false,
    val error: String? = null
)

class AudioDownloadManager(private val context: Context) {

    private val dm = context.getSystemService(Context.DOWNLOAD_SERVICE) as DownloadManager
    private val scope = CoroutineScope(Dispatchers.IO + SupervisorJob())

    private val _currentProgress = MutableStateFlow(DownloadProgress("", 0, false))
    val currentProgress: StateFlow<DownloadProgress> = _currentProgress.asStateFlow()

    private var activeDownloadId: Long = -1L
    private var pollJob: Job? = null

    fun startDownload(videoId: String, title: String, artist: String) {
        scope.launch {
            _currentProgress.value = DownloadProgress(title, 0, isRunning = true)

            // 1. YouTube Doğrudan Ses Linkini Çöz (0.1 sn)
            val streamInfo = InnertubeResolver.resolveAudioStream(videoId)
            if (streamInfo == null || streamInfo.url.isEmpty()) {
                _currentProgress.value = DownloadProgress(title, 0, isRunning = false, error = "Ses akışı alınamadı")
                return@launch
            }

            // 2. Güvenli Dosya İsmi Hazırla
            val safeFileName = title.replace(Regex("[^a-zA-Z0-9çğıöşüÇĞİÖŞÜ\\s-_.]"), "")
                .trim()
                .take(64) + ".m4a"

            // 3. Android Sistem İndiricisine (DownloadManager) Teslim Et (Sıfır RAM Harcar)
            val request = DownloadManager.Request(Uri.parse(streamInfo.url)).apply {
                setTitle(title)
                setDescription("ΛURNS • $artist")
                setNotificationVisibility(DownloadManager.Request.VISIBILITY_VISIBLE_NOTIFY_COMPLETED)
                setAllowedOverMetered(true)
                setAllowedOverRoaming(true)
                setDestinationInExternalPublicDir(Environment.DIRECTORY_MUSIC, "Aurns/$safeFileName")
            }

            activeDownloadId = dm.enqueue(request)

            // 4. Canlı İlerleme Çubuğu Takibi (%0 - %100)
            startProgressPolling(activeDownloadId, title)
        }
    }

    fun cancelActiveDownload() {
        if (activeDownloadId != -1L) {
            dm.remove(activeDownloadId)
            activeDownloadId = -1L
            pollJob?.cancel()
            _currentProgress.value = DownloadProgress("", 0, isRunning = false)
        }
    }

    private fun startProgressPolling(downloadId: Long, title: String) {
        pollJob?.cancel()
        pollJob = scope.launch {
            var downloading = true
            while (downloading && isActive) {
                val query = DownloadManager.Query().setFilterById(downloadId)
                dm.query(query)?.use { cursor ->
                    if (cursor.moveToFirst()) {
                        val bytesDownloaded = cursor.getInt(cursor.getColumnIndexOrThrow(DownloadManager.COLUMN_BYTES_DOWNLOADED_SO_FAR))
                        val bytesTotal = cursor.getInt(cursor.getColumnIndexOrThrow(DownloadManager.COLUMN_TOTAL_SIZE_BYTES))
                        val status = cursor.getInt(cursor.getColumnIndexOrThrow(DownloadManager.COLUMN_STATUS))

                        when (status) {
                            DownloadManager.STATUS_RUNNING -> {
                                val pct = if (bytesTotal > 0) ((bytesDownloaded.toDouble() / bytesTotal) * 100).toInt() else 0
                                _currentProgress.value = DownloadProgress(title, pct.coerceIn(0, 100), isRunning = true)
                            }
                            DownloadManager.STATUS_SUCCESSFUL -> {
                                _currentProgress.value = DownloadProgress(title, 100, isRunning = false, isDone = true)
                                downloading = false
                            }
                            DownloadManager.STATUS_FAILED -> {
                                _currentProgress.value = DownloadProgress(title, 0, isRunning = false, error = "İndirme başarısız oldu")
                                downloading = false
                            }
                        }
                    }
                }
                delay(250)
            }
        }
    }
}
