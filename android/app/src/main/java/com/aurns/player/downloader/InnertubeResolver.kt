package com.aurns.player.downloader

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.Cookie
import okhttp3.CookieJar
import okhttp3.HttpUrl
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import org.json.JSONArray
import org.json.JSONObject
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.TimeUnit
import java.util.regex.Pattern

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

    private val cookieStore = ConcurrentHashMap<String, MutableList<Cookie>>()

    private val client: OkHttpClient = OkHttpClient.Builder()
        .cookieJar(object : CookieJar {
            override fun saveFromResponse(url: HttpUrl, cookies: List<Cookie>) {
                val host = url.host
                val existing = cookieStore.getOrPut(host) { mutableListOf() }
                synchronized(existing) {
                    for (c in cookies) {
                        existing.removeAll { it.name == c.name }
                        existing.add(c)
                    }
                }
            }

            override fun loadForRequest(url: HttpUrl): List<Cookie> {
                val host = url.host
                val list = mutableListOf<Cookie>()
                cookieStore[host]?.let { synchronized(it) { list.addAll(it) } }
                if (host.endsWith("youtube.com") && host != "youtube.com") {
                    cookieStore["youtube.com"]?.let { synchronized(it) { list.addAll(it) } }
                }
                return list
            }
        })
        .connectTimeout(10, TimeUnit.SECONDS)
        .readTimeout(15, TimeUnit.SECONDS)
        .build()

    private val JSON_MEDIA = "application/json; charset=utf-8".toMediaType()

    private const val UA_DESKTOP = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0.0.0 Safari/537.36"
    private const val UA_VISIONOS = "Mozilla/5.0 (Macintosh; Intel Mac OS X 15_7_3) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/26.0 Safari/605.1.15"

    @Volatile
    private var cachedVisitorData: String? = null

    /**
     * 1. YouTube İç Web API'sinden (Innertube WEB) Şarkı Arama
     * Güvenilir, sınırsız ve 0.1 saniye yanıt süreli.
     */
    suspend fun search(query: String): List<SearchResult> = withContext(Dispatchers.IO) {
        val results = mutableListOf<SearchResult>()
        try {
            val reqBody = JSONObject().apply {
                put("query", query)
                put("context", JSONObject().apply {
                    put("client", JSONObject().apply {
                        put("clientName", "WEB")
                        put("clientVersion", "2.20250101.00.00")
                        put("hl", "tr")
                        put("gl", "TR")
                    })
                })
            }

            val request = Request.Builder()
                .url("https://www.youtube.com/youtubei/v1/search")
                .header("User-Agent", UA_DESKTOP)
                .header("Content-Type", "application/json")
                .header("Origin", "https://www.youtube.com")
                .header("Referer", "https://www.youtube.com/")
                .post(reqBody.toString().toRequestBody(JSON_MEDIA))
                .build()

            val response = client.newCall(request).execute()
            val bodyStr = response.body?.string() ?: return@withContext emptyList()
            val json = JSONObject(bodyStr)

            // Visitor Data yakala (İlerideki indirme istekleri için)
            val vis = json.optJSONObject("responseContext")?.optString("visitorData")
            if (!vis.isNullOrEmpty()) {
                cachedVisitorData = vis
            }

            // Özyinelemeli (Recursive) Video Arama - Hiçbir sonucu kaçırmaz
            val renderers = mutableListOf<JSONObject>()
            findVideoRenderers(json, renderers)

            for (vr in renderers) {
                val videoId = vr.optString("videoId")
                if (videoId.isEmpty()) continue

                // Başlık
                val titleObj = vr.optJSONObject("title")
                val title = titleObj?.optJSONArray("runs")?.optJSONObject(0)?.optString("text")
                    ?: titleObj?.optString("simpleText") ?: "Bilinmeyen Parça"

                // Sanatçı / Kanal
                val ownerObj = vr.optJSONObject("ownerText") ?: vr.optJSONObject("shortBylineText") ?: vr.optJSONObject("longBylineText")
                val artist = ownerObj?.optJSONArray("runs")?.optJSONObject(0)?.optString("text") ?: "YouTube"

                // Süre
                val lengthObj = vr.optJSONObject("lengthText")
                val duration = lengthObj?.optString("simpleText")
                    ?: lengthObj?.optJSONArray("runs")?.optJSONObject(0)?.optString("text") ?: ""

                // Küçük Resim (Thumbnail)
                val thumbs = vr.optJSONObject("thumbnail")?.optJSONArray("thumbnails")
                val thumbUrl = if (thumbs != null && thumbs.length() > 0) {
                    thumbs.getJSONObject(thumbs.length() - 1).optString("url")
                } else {
                    "https://i.ytimg.com/vi/$videoId/hqdefault.jpg"
                }

                results.add(SearchResult(videoId, title, artist, duration, thumbUrl))
                if (results.size >= 30) break
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
        return@withContext results
    }

    /**
     * 2. Doğrudan Ses Akışı Linkini Çözümleme (Direct Audio Stream Extractor)
     * VisionOS Client + Session Cookie + Visitor ID ile doğrudan Google CDN HTTPS linki üretir.
     */
    suspend fun resolveAudioStream(videoId: String): StreamInfo? = withContext(Dispatchers.IO) {
        try {
            // Oturum çerezleri veya Visitor ID eksikse hızlı bir GET ile oturum aç
            if (cachedVisitorData.isNullOrEmpty()) {
                refreshSession(videoId)
            }

            val reqBody = JSONObject().apply {
                put("videoId", videoId)
                put("context", JSONObject().apply {
                    put("client", JSONObject().apply {
                        put("clientName", "VISIONOS")
                        put("clientVersion", "1.02")
                        put("deviceMake", "Apple")
                        put("deviceModel", "RealityDevice17,1")
                        put("userAgent", UA_VISIONOS)
                        put("osName", "visionOS")
                        put("osVersion", "26.5.23O471")
                        put("hl", "tr")
                        put("gl", "TR")
                    })
                })
            }

            val reqBuilder = Request.Builder()
                .url("https://www.youtube.com/youtubei/v1/player?prettyPrint=false")
                .header("User-Agent", UA_VISIONOS)
                .header("Content-Type", "application/json")
                .header("X-Youtube-Client-Name", "101")
                .header("X-Youtube-Client-Version", "1.02")
                .header("Origin", "https://www.youtube.com")

            cachedVisitorData?.let {
                reqBuilder.header("X-Goog-Visitor-Id", it)
            }

            val response = client.newCall(reqBuilder.post(reqBody.toString().toRequestBody(JSON_MEDIA)).build()).execute()
            val bodyStr = response.body?.string() ?: return@withContext null
            val json = JSONObject(bodyStr)

            val playability = json.optJSONObject("playabilityStatus")
            val status = playability?.optString("status")

            // Eğer LOGIN_REQUIRED verirse oturumu yenileyip bir kez daha dene
            if (status != "OK") {
                refreshSession(videoId)
                val retryResponse = client.newCall(reqBuilder.post(reqBody.toString().toRequestBody(JSON_MEDIA)).build()).execute()
                val retryBody = retryResponse.body?.string() ?: return@withContext null
                val retryJson = JSONObject(retryBody)
                return@withContext extractBestAudio(retryJson)
            }

            return@withContext extractBestAudio(json)
        } catch (e: Exception) {
            e.printStackTrace()
            return@withContext null
        }
    }

    private fun extractBestAudio(json: JSONObject): StreamInfo? {
        val adaptive = json.optJSONObject("streamingData")?.optJSONArray("adaptiveFormats")
            ?: return null

        var bestAudio: StreamInfo? = null
        var maxBitrate = 0

        for (i in 0 until adaptive.length()) {
            val fmt = adaptive.getJSONObject(i)
            val mime = fmt.optString("mimeType", "")
            if (!mime.startsWith("audio/")) continue

            val url = fmt.optString("url")
            if (url.isEmpty()) continue // Şifresiz doğrudan Google CDN linki olanları seç

            val bitrate = fmt.optInt("bitrate", 0)
            val contentLength = fmt.optLong("contentLength", 0L)

            // En kaliteli sesi seç (M4A 130k / 160k Opus)
            if (bitrate > maxBitrate) {
                maxBitrate = bitrate
                bestAudio = StreamInfo(url, mime, bitrate, contentLength)
            }
        }
        return bestAudio
    }

    private fun refreshSession(videoId: String) {
        try {
            val watchUrl = "https://www.youtube.com/watch?v=$videoId&bpctr=9999999999&has_verified=1"
            val req = Request.Builder()
                .url(watchUrl)
                .header("User-Agent", UA_VISIONOS)
                .header("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8")
                .header("Accept-Language", "en-US,en;q=0.9")
                .build()

            val resp = client.newCall(req).execute()
            val html = resp.body?.string().orEmpty()
            val matcher = Pattern.compile("\"visitorData\":\\s*\"([^\"]+)\"").matcher(html)
            if (matcher.find()) {
                cachedVisitorData = matcher.group(1)
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    private fun findVideoRenderers(obj: Any?, list: MutableList<JSONObject>) {
        when (obj) {
            is JSONObject -> {
                if (obj.has("videoRenderer")) {
                    obj.optJSONObject("videoRenderer")?.let { list.add(it) }
                }
                val keys = obj.keys()
                while (keys.hasNext()) {
                    val key = keys.next()
                    findVideoRenderers(obj.opt(key), list)
                }
            }
            is JSONArray -> {
                for (i in 0 until obj.length()) {
                    findVideoRenderers(obj.opt(i), list)
                }
            }
        }
    }
}
