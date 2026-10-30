package com.aurns.player.jam

import kotlinx.coroutines.*
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import org.json.JSONObject
import java.io.InputStream
import java.io.OutputStream
import java.net.Socket

data class JamSyncState(
    val isConnected: Boolean = false,
    val roomCode: String = "",
    val partnerName: String = "",
    val currentTrackTitle: String = "",
    val currentYtId: String = "",
    val isPlaying: Boolean = false,
    val positionSec: Int = 0
)

class JamClient {

    private val scope = CoroutineScope(Dispatchers.IO + SupervisorJob())
    private var socket: Socket? = null
    private var isRunning = false

    private val _syncState = MutableStateFlow(JamSyncState())
    val syncState: StateFlow<JamSyncState> = _syncState.asStateFlow()

    fun joinRoom(roomCode: String, myName: String) {
        leaveRoom()
        _syncState.value = JamSyncState(isConnected = true, roomCode = roomCode)
        isRunning = true

        scope.launch {
            try {
                // Windows ΛURNS ile birebir aynı genel MQTT broker bağlantısı
                socket = Socket("broker.emqx.io", 1883)
                val out = socket?.getOutputStream()
                val input = socket?.getInputStream()

                // Basit MQTT CONNECT paketi
                sendMqttConnect(out, "aurns_android_${System.currentTimeMillis() % 10000}")

                // Oda konusuna abone ol (seloplayer/j/<roomCode>)
                sendMqttSubscribe(out, "seloplayer/j/$roomCode")

                val buffer = ByteArray(4096)
                while (isRunning && socket?.isConnected == true) {
                    val read = input?.read(buffer) ?: break
                    if (read <= 0) break

                    // Basit MQTT PUBLISH paketinden JSON yükünü ayrıştır
                    parseMqttPayload(buffer, read)?.let { jsonStr ->
                        try {
                            val json = JSONObject(jsonStr)
                            val title = json.optString("title", "")
                            val ytId = json.optString("ytId", "")
                            val isPlay = json.optBoolean("playing", false)
                            val pos = json.optInt("posSec", 0)

                            _syncState.value = _syncState.value.copy(
                                currentTrackTitle = title,
                                currentYtId = ytId,
                                isPlaying = isPlay,
                                positionSec = pos
                            )
                        } catch (e: Exception) {
                            e.printStackTrace()
                        }
                    }
                }
            } catch (e: Exception) {
                e.printStackTrace()
            } finally {
                _syncState.value = JamSyncState()
            }
        }
    }

    fun broadcastState(title: String, ytId: String, isPlaying: Boolean, posSec: Int) {
        val room = _syncState.value.roomCode
        if (room.isEmpty() || socket?.isConnected != true) return

        scope.launch {
            try {
                val json = JSONObject().apply {
                    put("title", title)
                    put("ytId", ytId)
                    put("playing", isPlaying)
                    put("posSec", posSec)
                    put("time", System.currentTimeMillis())
                }
                sendMqttPublish(socket?.getOutputStream(), "seloplayer/j/$room", json.toString())
            } catch (e: Exception) {
                e.printStackTrace()
            }
        }
    }

    fun leaveRoom() {
        isRunning = false
        try {
            socket?.close()
        } catch (e: Exception) {}
        socket = null
        _syncState.value = JamSyncState()
    }

    private fun sendMqttConnect(out: OutputStream?, clientId: String) {
        if (out == null) return
        val cidBytes = clientId.toByteArray()
        val varHeader = byteArrayOf(0x00, 0x04, 'M'.code.toByte(), 'Q'.code.toByte(), 'T'.code.toByte(), 'T'.code.toByte(), 0x04, 0x02, 0x00, 0x3C)
        val payloadLen = 2 + cidBytes.size
        val remLen = varHeader.size + payloadLen

        out.write(0x10) // CONNECT
        out.write(remLen)
        out.write(varHeader)
        out.write(byteArrayOf((cidBytes.size shr 8).toByte(), (cidBytes.size and 0xFF).toByte()))
        out.write(cidBytes)
        out.flush()
    }

    private fun sendMqttSubscribe(out: OutputStream?, topic: String) {
        if (out == null) return
        val tBytes = topic.toByteArray()
        val remLen = 2 + 2 + tBytes.size + 1
        out.write(0x82) // SUBSCRIBE
        out.write(remLen)
        out.write(byteArrayOf(0x00, 0x01)) // Packet ID
        out.write(byteArrayOf((tBytes.size shr 8).toByte(), (tBytes.size and 0xFF).toByte()))
        out.write(tBytes)
        out.write(0x00) // QoS 0
        out.flush()
    }

    private fun sendMqttPublish(out: OutputStream?, topic: String, payload: String) {
        if (out == null) return
        val tBytes = topic.toByteArray()
        val pBytes = payload.toByteArray()
        val remLen = 2 + tBytes.size + pBytes.size

        out.write(0x30) // PUBLISH
        out.write(remLen)
        out.write(byteArrayOf((tBytes.size shr 8).toByte(), (tBytes.size and 0xFF).toByte()))
        out.write(tBytes)
        out.write(pBytes)
        out.flush()
    }

    private fun parseMqttPayload(buffer: ByteArray, len: Int): String? {
        if (len < 4) return null
        val pktType = buffer[0].toInt() and 0xF0
        if (pktType != 0x30) return null // Sadece PUBLISH paketleri

        val remLen = buffer[1].toInt() and 0xFF
        val topicLen = ((buffer[2].toInt() and 0xFF) shl 8) or (buffer[3].toInt() and 0xFF)
        val payloadStart = 4 + topicLen
        if (payloadStart >= len) return null

        val payloadLen = len - payloadStart
        return String(buffer, payloadStart, payloadLen)
    }
}
