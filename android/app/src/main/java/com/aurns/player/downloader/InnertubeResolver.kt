package com.aurns.player.downloader

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import org.json.JSONObject
import java.util.concurrent.TimeUnit

data class SearchResult(
    val videoId: String,
    val title: String,
    val artist: String,
    val durationText: String,
    val thumbnailUrl: String
)

data class StreamInfo(
    val url: String,
    val mimeType: String,
    val bitrate: Int,
    val contentLength: Long
)

object InnertubeResolver {

    private val client = OkHttpClient.Builder()
        .connectTimeout(5, TimeUnit.SECONDS)
        .readTimeout(8, TimeUnit.SECONDS)
        .build()

    private const val INNERTUBE_CLIENT_VERSION = "19.29.35"
    private const val USER_AGENT = "com.google.android.youtube/$INNERTUBE_CLIENT_VERSION (Linux; U; Android 14; tr_TR; Pixel 8 Pro)"
    private val JSON_MEDIA = "application/json; charset=utf-8".toMediaType()

    /**
     * 1. YouTube İç Mobil API'sinden (Innertube) Şarkı Arama
     * Sunucu gerektirmez, doğrudan YouTube sunucusundan 0.1 sn içinde sonuç çeker.
     */
    suspend fun search(query: String): List<SearchResult> = withContext(Dispatchers.IO) {
        val results = mutableListOf<SearchResult>()
        try {
            val reqBody = JSONObject().apply {
                put("query", query)
                put("context", JSONObject().apply {
                    put("client", JSONObject().apply {
                        put("clientName", "ANDROID")
                        put("clientVersion", INNERTUBE_CLIENT_VERSION)
                        put("hl", "tr")
                        put("gl", "TR")
                    })
                })
            }

            val request = Request.Builder()
                .url("https://www.youtube.com/youtubei/v1/search")
                .header("User-Agent", USER_AGENT)
                .header("Content-Type", "application/json")
                .post(reqBody.toString().toRequestBody(JSON_MEDIA))
                .build()

            val response = client.newCall(request).execute()
            val bodyStr = response.body?.string() ?: return@withContext emptyList()
            val json = JSONObject(bodyStr)

            val sectionList = json.optJSONObject("contents")
                ?.optJSONObject("sectionListRenderer")
                ?.optJSONArray("contents") ?: return@withContext emptyList()

            for (i in 0 until sectionList.length()) {
                val itemSection = sectionList.getJSONObject(i).optJSONObject("itemSectionRenderer") ?: continue
                val contents = itemSection.optJSONArray("contents") ?: continue

                for (j in 0 until contents.length()) {
                    val vr = contents.getJSONObject(j).optJSONObject("videoRenderer") ?: continue
                    val videoId = vr.optString("videoId")
                    if (videoId.isEmpty()) continue

                    val title = vr.optJSONObject("title")?.optJSONArray("runs")?.optJSONObject(0)?.optString("text")
                        ?: vr.optJSONObject("title")?.optString("simpleText") ?: "Bilinmeyen Parça"

                    val author = vr.optJSONObject("ownerText")?.optJSONArray("runs")?.optJSONObject(0)?.optString("text")
                        ?: vr.optJSONObject("shortBylineText")?.optJSONArray("runs")?.optJSONObject(0)?.optString("text")
                        ?: "YouTube"

                    val duration = vr.optJSONObject("lengthText")?.optString("simpleText") ?: ""

                    val thumbs = vr.optJSONObject("thumbnail")?.optJSONArray("thumbnails")
                    val thumbUrl = if (thumbs != null && thumbs.length() > 0) {
                        thumbs.getJSONObject(thumbs.length() - 1).optString("url")
                    } else "https://i.ytimg.com/vi/$videoId/hqdefault.jpg"

                    results.add(SearchResult(videoId, title, author, duration, thumbUrl))
                    if (results.size >= 25) break
                }
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
        return@withContext results
    }

    /**
     * 2. Doğrudan Ses Akışı Linkini Çözümleme (Direct Audio Stream Extractor)
     * YouTube'dan doğrudan 160 kbps M4A/Opus doğrudan Google CDN indirme linkini alır.
     */
    suspend fun resolveAudioStream(videoId: String): StreamInfo? = withContext(Dispatchers.IO) {
        try {
            val reqBody = JSONObject().apply {
                put("videoId", videoId)
                put("context", JSONObject().apply {
                    put("client", JSONObject().apply {
                        put("clientName", "ANDROID")
                        put("clientVersion", INNERTUBE_CLIENT_VERSION)
                        put("hl", "tr")
                        put("gl", "TR")
                    })
                })
            }

            val request = Request.Builder()
                .url("https://www.youtube.com/youtubei/v1/player")
                .header("User-Agent", USER_AGENT)
                .header("Content-Type", "application/json")
                .post(reqBody.toString().toRequestBody(JSON_MEDIA))
                .build()

            val response = client.newCall(request).execute()
            val bodyStr = response.body?.string() ?: return@withContext null
            val json = JSONObject(bodyStr)

            val adaptive = json.optJSONObject("streamingData")?.optJSONArray("adaptiveFormats")
                ?: return@withContext null

            var bestAudio: StreamInfo? = null
            var maxBitrate = 0

            for (i in 0 until adaptive.length()) {
                val fmt = adaptive.getJSONObject(i)
                val mime = fmt.optString("mimeType", "")
                if (!mime.startsWith("audio/")) continue

                val url = fmt.optString("url")
                if (url.isEmpty()) continue // Şifresiz doğrudan CDN linki olanları seç

                val bitrate = fmt.optInt("bitrate", 0)
                val contentLength = fmt.optLong("contentLength", 0L)

                // En yüksek ses kalitesini (tercihen m4a / 160k) seç
                if (bitrate > maxBitrate) {
                    maxBitrate = bitrate
                    bestAudio = StreamInfo(url, mime, bitrate, contentLength)
                }
            }

            return@withContext bestAudio
        } catch (e: Exception) {
            e.printStackTrace()
            return@withContext null
        }
    }
}
