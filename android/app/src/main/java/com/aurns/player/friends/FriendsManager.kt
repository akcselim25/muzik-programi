package com.aurns.player.friends

import android.content.Context
import android.os.Build
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import org.json.JSONArray
import org.json.JSONObject
import java.io.InputStream
import java.io.OutputStream
import java.net.Socket
import java.util.*
import kotlin.random.Random

data class FriendItem(
    val tag: String,
    val name: String,
    val isOnline: Boolean = false,
    val isPlaying: Boolean = false,
    val currentTrack: String = "Çevrimdışı",
    val lastSeenTimestamp: Long = 0L
)

data class FriendRequest(
    val tag: String,
    val name: String
)

data class JamRoomState(
    val isActive: Boolean = false,
    val roomCode: String = "",
    val partnerTag: String = "",
    val currentTrackTitle: String = "",
    val isPlaying: Boolean = false,
    val positionSec: Int = 0
)

class FriendsManager(private val context: Context) {

    private val prefs = context.getSharedPreferences("aurns_friends_prefs", Context.MODE_PRIVATE)
    private val scope = CoroutineScope(Dispatchers.IO + SupervisorJob())

    // Kimlik Bilgileri
    private val _myTag = MutableStateFlow("")
    val myTag: StateFlow<String> = _myTag.asStateFlow()

    private val _myName = MutableStateFlow("")
    val myName: StateFlow<String> = _myName.asStateFlow()

    // Arkadaşlar ve Bekleyen İstekler
    private val _friends = MutableStateFlow<List<FriendItem>>(emptyList())
    val friends: StateFlow<List<FriendItem>> = _friends.asStateFlow()

    private val _pendingRequests = MutableStateFlow<List<FriendRequest>>(emptyList())
    val pendingRequests: StateFlow<List<FriendRequest>> = _pendingRequests.asStateFlow()

    // Jam Canlı Senkronizasyon Durumu
    private val _jamState = MutableStateFlow(JamRoomState())
    val jamState: StateFlow<JamRoomState> = _jamState.asStateFlow()

    // Ağ Bağlantı Durumu
    private val _isConnected = MutableStateFlow(false)
    val isConnected: StateFlow<Boolean> = _isConnected.asStateFlow()

    private var socket: Socket? = null
    private var isRunning = false
    private var outputStream: OutputStream? = null

    // Yerel oynatma durumu
    private var localIsPlaying = false
    private var localTrackTitle = ""

    // Jam dinleyici callback'i (MainActivity'de şarkıyı başlatmak için)
    var onRemoteJamPlayRequest: ((title: String, posSec: Int) -> Unit)? = null

    init {
        loadFromPrefs()
        startNetworkLoop()
    }

    private fun loadFromPrefs() {
        var tag = prefs.getString("my_tag", "") ?: ""
        var name = prefs.getString("my_name", "") ?: ""

        if (tag.isBlank()) {
            val deviceModel = Build.MODEL.replace(Regex("[^a-zA-Z0-9]"), "").take(10).ifEmpty { "Android" }
            val num = 1000 + Random.nextInt(9000)
            tag = "$deviceModel#$num"
            name = deviceModel
            prefs.edit().putString("my_tag", tag).putString("my_name", name).apply()
        }

        _myTag.value = tag
        _myName.value = name

        // Arkadaş listesini yükle
        val friendsJson = prefs.getString("friends_json", "[]") ?: "[]"
        val fList = mutableListOf<FriendItem>()
        try {
            val arr = JSONArray(friendsJson)
            for (i in 0 until arr.length()) {
                val obj = arr.getJSONObject(i)
                fList.add(FriendItem(obj.getString("tag"), obj.getString("name")))
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
        _friends.value = fList

        // Bekleyen istekleri yükle
        val pendingJson = prefs.getString("pending_json", "[]") ?: "[]"
        val pList = mutableListOf<FriendRequest>()
        try {
            val arr = JSONArray(pendingJson)
            for (i in 0 until arr.length()) {
                val obj = arr.getJSONObject(i)
                pList.add(FriendRequest(obj.getString("tag"), obj.getString("name")))
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
        _pendingRequests.value = pList
    }

    fun updateMyName(newName: String) {
        val clean = newName.trim().take(24)
        if (clean.isNotBlank()) {
            _myName.value = clean
            val tag = _myTag.value
            val hashPart = if (tag.contains("#")) "#" + tag.substringAfter("#") else "#${1000 + Random.nextInt(9000)}"
            val newTag = "$clean$hashPart"
            _myTag.value = newTag
            prefs.edit().putString("my_name", clean).putString("my_tag", newTag).apply()

            // Yeni gelen kutumuza abone ol
            subscribeToMyInbox()
            requestAllStatuses()
        }
    }

    private fun saveFriendsToPrefs() {
        val fArr = JSONArray()
        for (f in _friends.value) {
            val obj = JSONObject().apply {
                put("tag", f.tag)
                put("name", f.name)
            }
            fArr.put(obj)
        }

        val pArr = JSONArray()
        for (p in _pendingRequests.value) {
            val obj = JSONObject().apply {
                put("tag", p.tag)
                put("name", p.name)
            }
            pArr.put(obj)
        }

        prefs.edit()
            .putString("friends_json", fArr.toString())
            .putString("pending_json", pArr.toString())
            .apply()
    }

    private fun tagToTopicKey(tag: String): String {
        return tag.lowercase(Locale.ROOT)
            .replace('#', '_')
            .replace('+', '_')
            .replace('/', '_')
            .replace('$', '_')
            .replace(' ', '_')
    }

    private fun startNetworkLoop() {
        isRunning = true
        scope.launch {
            while (isRunning) {
                try {
                    // Windows ΛURNS ile birebir aynı ortak MQTT broker bağlantısı
                    socket = Socket("broker.emqx.io", 1883)
                    outputStream = socket?.getOutputStream()
                    val inputStream = socket?.getInputStream()

                    val clientId = "aurns_and_${System.currentTimeMillis() % 100000}"
                    sendMqttConnect(outputStream, clientId)

                    _isConnected.value = true
                    subscribeToMyInbox()

                    // Açılışta tüm arkadaşların durumunu iste
                    requestAllStatuses()

                    val buffer = ByteArray(4096)
                    while (isRunning && socket?.isConnected == true) {
                        val read = inputStream?.read(buffer) ?: break
                        if (read <= 0) break

                        handleIncomingMqttPacket(buffer, read)
                    }
                } catch (e: Exception) {
                    _isConnected.value = false
                    delay(3000)
                } finally {
                    _isConnected.value = false
                    try { socket?.close() } catch (ignored: Exception) {}
                }
            }
        }
    }

    private fun subscribeToMyInbox() {
        val myKey = tagToTopicKey(_myTag.value)
        if (myKey.isNotBlank()) {
            sendMqttSubscribe(outputStream, "seloplayer/u/$myKey")
        }
    }

    private fun handleIncomingMqttPacket(buf: ByteArray, len: Int) {
        val pktType = buf[0].toInt() and 0xF0
        if (pktType != 0x30) return // PUBLISH değilse atla

        var idx = 1
        var multiplier = 1
        var remLen = 0
        while (idx < len) {
            val digit = buf[idx].toInt() and 0xFF
            remLen += (digit and 0x7F) * multiplier
            multiplier *= 128
            idx++
            if ((digit and 0x80) == 0) break
        }

        if (idx + 2 > len) return
        val topLen = ((buf[idx].toInt() and 0xFF) shl 8) or (buf[idx + 1].toInt() and 0xFF)
        idx += 2
        if (idx + topLen > len) return

        val topic = String(buf, idx, topLen, Charsets.UTF_8)
        idx += topLen
        val payloadLen = len - idx
        val payload = if (payloadLen > 0) String(buf, idx, payloadLen, Charsets.UTF_8) else ""

        handleNetworkMessage(topic, payload)
    }

    private fun handleNetworkMessage(topic: String, payload: String) {
        val myKey = tagToTopicKey(_myTag.value)
        val myInbox = "seloplayer/u/$myKey"

        // 1. Kendi Gelen Kutusu Mesajları
        if (topic.equals(myInbox, ignoreCase = true)) {
            val parts = payload.split("|")
            if (parts.isEmpty()) return
            val action = parts[0]

            when (action) {
                // Arkadaşlık İsteği: REQ|fromTag|fromName
                "REQ" -> {
                    if (parts.size >= 3) {
                        val fromTag = parts[1]
                        val fromName = parts[2]
                        if (fromTag != _myTag.value && _friends.value.none { it.tag.equals(fromTag, ignoreCase = true) }) {
                            val cur = _pendingRequests.value.toMutableList()
                            if (cur.none { it.tag.equals(fromTag, ignoreCase = true) }) {
                                cur.add(FriendRequest(fromTag, fromName))
                                _pendingRequests.value = cur
                                saveFriendsToPrefs()
                            }
                        }
                    }
                }

                // İstek Kabul Edildi: ACC|fromTag|fromName
                "ACC" -> {
                    if (parts.size >= 3) {
                        val fromTag = parts[1]
                        val fromName = parts[2]
                        val cur = _friends.value.toMutableList()
                        if (cur.none { it.tag.equals(fromTag, ignoreCase = true) }) {
                            cur.add(FriendItem(fromTag, fromName, isOnline = true, currentTrack = "Çevrimiçi"))
                            _friends.value = cur
                            saveFriendsToPrefs()
                        }
                        sendPong(fromTag)
                        sendPing(fromTag)
                    }
                }

                // Durum Sorgusu: PING|fromTag|fromName
                "PING" -> {
                    if (parts.size >= 2) {
                        val fromTag = parts[1]
                        sendPong(fromTag)
                    }
                }

                // Durum Yanıtı: PONG|fromTag|fromName|isPlaying|currentTrack
                "PONG" -> {
                    if (parts.size >= 5) {
                        val fromTag = parts[1]
                        val fromName = parts[2]
                        val isPlay = parts[3] == "1"
                        val track = parts[4].ifEmpty { if (isPlay) "Müzik Çalıyor" else "Çevrimiçi" }

                        updateFriendStatus(fromTag, fromName, isOnline = true, isPlaying = isPlay, currentTrack = track)
                    }
                }

                // Çevrimdışı Bildirimi: BYE|fromTag
                "BYE" -> {
                    if (parts.size >= 2) {
                        val fromTag = parts[1]
                        updateFriendStatus(fromTag, "", isOnline = false, isPlaying = false, currentTrack = "Çevrimdışı")
                    }
                }

                // Jam Daveti: JAM_INVITE|fromTag|fromName|roomCode
                "JAM_INVITE" -> {
                    if (parts.size >= 4) {
                        val fromTag = parts[1]
                        val roomCode = parts[3]
                        joinJamRoom(roomCode, fromTag)
                    }
                }
            }
        }

        // 2. Canlı Jam Oda Mesajları (seloplayer/j/<roomCode>)
        if (topic.startsWith("seloplayer/j/") && _jamState.value.isActive) {
            try {
                val json = JSONObject(payload)
                val title = json.optString("title", "")
                val isPlay = json.optBoolean("playing", false)
                val pos = json.optInt("posSec", 0)

                _jamState.value = _jamState.value.copy(
                    currentTrackTitle = title,
                    isPlaying = isPlay,
                    positionSec = pos
                )

                if (title.isNotEmpty() && title != localTrackTitle) {
                    onRemoteJamPlayRequest?.invoke(title, pos)
                }
            } catch (e: Exception) {
                e.printStackTrace()
            }
        }
    }

    private fun updateFriendStatus(tag: String, name: String, isOnline: Boolean, isPlaying: Boolean, currentTrack: String) {
        val cur = _friends.value.toMutableList()
        val idx = cur.indexOfFirst { it.tag.equals(tag, ignoreCase = true) }
        if (idx != -1) {
            val old = cur[idx]
            cur[idx] = old.copy(
                name = if (name.isNotBlank()) name else old.name,
                isOnline = isOnline,
                isPlaying = isPlaying,
                currentTrack = currentTrack,
                lastSeenTimestamp = System.currentTimeMillis()
            )
            _friends.value = cur
        }
    }

    // ==================== ARKADAŞ EYLEMLERİ ====================

    fun sendFriendRequest(targetTag: String): Boolean {
        val clean = targetTag.trim()
        if (clean.isBlank() || !clean.contains("#") || clean.equals(_myTag.value, ignoreCase = true)) {
            return false
        }
        if (_friends.value.any { it.tag.equals(clean, ignoreCase = true) }) {
            return false
        }

        val targetKey = tagToTopicKey(clean)
        val topic = "seloplayer/u/$targetKey"
        val payload = "REQ|${_myTag.value}|${_myName.value}"

        scope.launch {
            sendMqttPublish(outputStream, topic, payload)
        }
        return true
    }

    fun acceptFriendRequest(targetTag: String) {
        val req = _pendingRequests.value.firstOrNull { it.tag.equals(targetTag, ignoreCase = true) } ?: return

        val curF = _friends.value.toMutableList()
        if (curF.none { it.tag.equals(req.tag, ignoreCase = true) }) {
            curF.add(FriendItem(req.tag, req.name, isOnline = true, currentTrack = "Çevrimiçi"))
            _friends.value = curF
        }

        val curP = _pendingRequests.value.toMutableList()
        curP.removeAll { it.tag.equals(targetTag, ignoreCase = true) }
        _pendingRequests.value = curP
        saveFriendsToPrefs()

        val targetKey = tagToTopicKey(req.tag)
        val topic = "seloplayer/u/$targetKey"
        val payload = "ACC|${_myTag.value}|${_myName.value}"

        scope.launch {
            sendMqttPublish(outputStream, topic, payload)
            sendPong(req.tag)
            sendPing(req.tag)
        }
    }

    fun rejectFriendRequest(targetTag: String) {
        val curP = _pendingRequests.value.toMutableList()
        curP.removeAll { it.tag.equals(targetTag, ignoreCase = true) }
        _pendingRequests.value = curP
        saveFriendsToPrefs()
    }

    fun removeFriend(targetTag: String) {
        val curF = _friends.value.toMutableList()
        curF.removeAll { it.tag.equals(targetTag, ignoreCase = true) }
        _friends.value = curF
        saveFriendsToPrefs()
    }

    fun requestAllStatuses() {
        for (f in _friends.value) {
            sendPing(f.tag)
        }
    }

    private fun sendPing(targetTag: String) {
        val targetKey = tagToTopicKey(targetTag)
        val topic = "seloplayer/u/$targetKey"
        val payload = "PING|${_myTag.value}|${_myName.value}"
        scope.launch {
            sendMqttPublish(outputStream, topic, payload)
        }
    }

    private fun sendPong(targetTag: String) {
        val targetKey = tagToTopicKey(targetTag)
        val topic = "seloplayer/u/$targetKey"
        val trackClean = localTrackTitle.replace('|', '-')
        val payload = "PONG|${_myTag.value}|${_myName.value}|${if (localIsPlaying) 1 else 0}|$trackClean"
        scope.launch {
            sendMqttPublish(outputStream, topic, payload)
        }
    }

    fun updateLocalPlaybackState(isPlaying: Boolean, trackTitle: String) {
        localIsPlaying = isPlaying
        localTrackTitle = trackTitle

        // Çevrimiçi tüm arkadaşlara anında yeni çalan şarkıyı bildir
        for (f in _friends.value) {
            sendPong(f.tag)
        }

        // Eğer Jam odasındaysak odaya da canlı durumu yayınla
        if (_jamState.value.isActive) {
            broadcastJamState(trackTitle, isPlaying, 0)
        }
    }

    // ==================== JAM CANLI SENKRONİZASYON ====================

    fun joinJamRoom(roomCode: String, partnerTag: String = "") {
        val cleanRoom = roomCode.trim().uppercase()
        if (cleanRoom.isEmpty()) return

        leaveJamRoom()
        _jamState.value = JamRoomState(isActive = true, roomCode = cleanRoom, partnerTag = partnerTag)

        scope.launch {
            sendMqttSubscribe(outputStream, "seloplayer/j/$cleanRoom")
        }
    }

    fun inviteFriendToJam(friendTag: String) {
        val myRoom = "JAM_${1000 + Random.nextInt(9000)}"
        joinJamRoom(myRoom, friendTag)

        val targetKey = tagToTopicKey(friendTag)
        val topic = "seloplayer/u/$targetKey"
        val payload = "JAM_INVITE|${_myTag.value}|${_myName.value}|$myRoom"

        scope.launch {
            sendMqttPublish(outputStream, topic, payload)
        }
    }

    fun broadcastJamState(title: String, isPlaying: Boolean, posSec: Int) {
        val room = _jamState.value.roomCode
        if (room.isEmpty() || !_jamState.value.isActive) return

        scope.launch {
            val json = JSONObject().apply {
                put("title", title)
                put("playing", isPlaying)
                put("posSec", posSec)
                put("time", System.currentTimeMillis())
            }
            sendMqttPublish(outputStream, "seloplayer/j/$room", json.toString())
        }
    }

    fun leaveJamRoom() {
        val room = _jamState.value.roomCode
        if (room.isNotEmpty()) {
            _jamState.value = JamRoomState()
        }
    }

    fun onDestroy() {
        isRunning = false
        // Çevrimdışı olduğunu arkadaşlara bildir
        for (f in _friends.value) {
            val targetKey = tagToTopicKey(f.tag)
            sendMqttPublish(outputStream, "seloplayer/u/$targetKey", "BYE|${_myTag.value}")
        }
        try { socket?.close() } catch (ignored: Exception) {}
    }

    // ==================== HAM MQTT PAKET FONKSİYONLARI ====================

    private fun sendMqttConnect(out: OutputStream?, clientId: String) {
        if (out == null) return
        val cidBytes = clientId.toByteArray(Charsets.UTF_8)
        val remLen = 10 + 2 + cidBytes.size
        val pkt = ByteArray(2 + remLen)
        var i = 0
        pkt[i++] = 0x10.toByte() // CONNECT
        pkt[i++] = remLen.toByte()
        pkt[i++] = 0x00.toByte(); pkt[i++] = 0x04.toByte()
        pkt[i++] = 'M'.code.toByte(); pkt[i++] = 'Q'.code.toByte(); pkt[i++] = 'T'.code.toByte(); pkt[i++] = 'T'.code.toByte()
        pkt[i++] = 0x04.toByte() // Level 4
        pkt[i++] = 0x02.toByte() // Clean Session
        pkt[i++] = 0x00.toByte(); pkt[i++] = 0x1E.toByte() // 30s KeepAlive
        pkt[i++] = (cidBytes.size shr 8).toByte()
        pkt[i++] = (cidBytes.size and 0xFF).toByte()
        System.arraycopy(cidBytes, 0, pkt, i, cidBytes.size)

        out.write(pkt)
        out.flush()
    }

    private fun sendMqttSubscribe(out: OutputStream?, topic: String) {
        if (out == null) return
        val topBytes = topic.toByteArray(Charsets.UTF_8)
        val remLen = 2 + 2 + topBytes.size + 1
        val pkt = ByteArray(2 + remLen)
        var i = 0
        pkt[i++] = 0x82.toByte() // SUBSCRIBE
        pkt[i++] = remLen.toByte()
        pkt[i++] = 0x00.toByte(); pkt[i++] = 0x01.toByte() // Packet ID
        pkt[i++] = (topBytes.size shr 8).toByte()
        pkt[i++] = (topBytes.size and 0xFF).toByte()
        System.arraycopy(topBytes, 0, pkt, i, topBytes.size)
        i += topBytes.size
        pkt[i] = 0x00.toByte() // QoS 0

        out.write(pkt)
        out.flush()
    }

    private fun sendMqttPublish(out: OutputStream?, topic: String, payload: String) {
        if (out == null) return
        try {
            val topBytes = topic.toByteArray(Charsets.UTF_8)
            val payBytes = payload.toByteArray(Charsets.UTF_8)
            val remLen = 2 + topBytes.size + payBytes.size
            if (remLen > 4000) return

            val pkt = ByteArray(2 + remLen)
            var i = 0
            pkt[i++] = 0x30.toByte() // PUBLISH QoS 0
            pkt[i++] = remLen.toByte()
            pkt[i++] = (topBytes.size shr 8).toByte()
            pkt[i++] = (topBytes.size and 0xFF).toByte()
            System.arraycopy(topBytes, 0, pkt, i, topBytes.size)
            i += topBytes.size
            System.arraycopy(payBytes, 0, pkt, i, payBytes.size)

            out.write(pkt)
            out.flush()
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }
}
