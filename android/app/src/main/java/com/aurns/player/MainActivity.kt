package com.aurns.player

import android.Manifest
import android.content.ComponentName
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.*
import androidx.compose.animation.core.*
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.ContextCompat
import androidx.media3.common.MediaItem
import androidx.media3.common.Player
import androidx.media3.session.MediaController
import androidx.media3.session.SessionToken
import com.aurns.player.data.MusicScanner
import com.aurns.player.data.Track
import com.aurns.player.downloader.AudioDownloadManager
import com.aurns.player.downloader.InnertubeResolver
import com.aurns.player.downloader.SearchResult
import com.aurns.player.jam.JamClient
import com.aurns.player.service.PlaybackService
import com.aurns.player.ui.theme.*
import com.google.common.util.concurrent.ListenableFuture
import com.google.common.util.concurrent.MoreExecutors
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

class MainActivity : ComponentActivity() {

    private lateinit var downloadManager: AudioDownloadManager
    private val jamClient = JamClient()
    private var controllerFuture: ListenableFuture<MediaController>? = null
    private var mediaController: MediaController? = null

    // Şarkı Listesi ve Oynatma Durumları
    private val tracks = mutableStateListOf<Track>()
    private var currentTrackIndex by mutableStateOf(-1)
    private var isPlaying by mutableStateOf(false)
    private var currentPositionSec by mutableStateOf(0)
    private var durationSec by mutableStateOf(0)

    // İzin İstemcisi
    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { permissions ->
        if (permissions.values.any { it }) {
            loadLocalTracks()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        downloadManager = AudioDownloadManager(this)

        // İzinleri Kontrol Et ve Tara
        checkAndRequestPermissions()

        // Medya Kontrolcüsünü Başlat
        initMediaController()

        setContent {
            AurnsTheme {
                MainScreen(
                    tracks = tracks,
                    currentTrackIndex = currentTrackIndex,
                    isPlaying = isPlaying,
                    currentPos = currentPositionSec,
                    duration = durationSec,
                    downloadManager = downloadManager,
                    jamClient = jamClient,
                    onPlayTrack = { index -> playTrack(index) },
                    onTogglePlay = { togglePlayPause() },
                    onSeek = { sec -> seekTo(sec) },
                    onNext = { playNext() },
                    onPrev = { playPrev() },
                    onRefreshTracks = { loadLocalTracks() }
                )
            }
        }
    }

    private fun checkAndRequestPermissions() {
        val permissions = mutableListOf<String>()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            permissions.add(Manifest.permission.READ_MEDIA_AUDIO)
            permissions.add(Manifest.permission.POST_NOTIFICATIONS)
        } else {
            permissions.add(Manifest.permission.READ_EXTERNAL_STORAGE)
        }

        val needed = permissions.filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }

        if (needed.isEmpty()) {
            loadLocalTracks()
        } else {
            permissionLauncher.launch(needed.toTypedArray())
        }
    }

    private fun loadLocalTracks() {
        val list = MusicScanner.scanLocalTracks(this)
        tracks.clear()
        tracks.addAll(list)
    }

    private fun initMediaController() {
        val sessionToken = SessionToken(this, ComponentName(this, PlaybackService::class.java))
        controllerFuture = MediaController.Builder(this, sessionToken).buildAsync()
        controllerFuture?.addListener({
            mediaController = controllerFuture?.get()
            mediaController?.addListener(object : Player.Listener {
                override fun onIsPlayingChanged(playing: Boolean) {
                    isPlaying = playing
                }
                override fun onPlaybackStateChanged(playbackState: Int) {
                    if (playbackState == Player.STATE_ENDED) {
                        playNext()
                    }
                }
            })
            setupProgressTicker()
        }, MoreExecutors.directExecutor())
    }

    private fun setupProgressTicker() {
        val handler = android.os.Handler(android.os.Looper.getMainLooper())
        handler.post(object : Runnable {
            override fun run() {
                mediaController?.let { p ->
                    currentPositionSec = (p.currentPosition / 1000).toInt()
                    durationSec = if (p.duration > 0) (p.duration / 1000).toInt() else 0
                }
                handler.postDelayed(this, 1000)
            }
        })
    }

    private fun playTrack(index: Int) {
        if (index < 0 || index >= tracks.size) return
        currentTrackIndex = index
        val track = tracks[index]
        mediaController?.let { p ->
            p.setMediaItem(MediaItem.fromUri(track.uri))
            p.prepare()
            p.play()
        }
    }

    private fun togglePlayPause() {
        mediaController?.let { p ->
            if (p.isPlaying) p.pause() else p.play()
        }
    }

    private fun seekTo(sec: Int) {
        mediaController?.seekTo(sec * 1000L)
    }

    private fun playNext() {
        if (tracks.isEmpty()) return
        val next = (currentTrackIndex + 1) % tracks.size
        playTrack(next)
    }

    private fun playPrev() {
        if (tracks.isEmpty()) return
        val prev = if (currentTrackIndex - 1 < 0) tracks.size - 1 else currentTrackIndex - 1
        playTrack(prev)
    }

    override fun onDestroy() {
        controllerFuture?.let { MediaController.releaseFuture(it) }
        super.onDestroy()
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun MainScreen(
    tracks: List<Track>,
    currentTrackIndex: Int,
    isPlaying: Boolean,
    currentPos: Int,
    duration: Int,
    downloadManager: AudioDownloadManager,
    jamClient: JamClient,
    onPlayTrack: (Int) -> Unit,
    onTogglePlay: () -> Unit,
    onSeek: (Int) -> Unit,
    onNext: () -> Unit,
    onPrev: () -> Unit,
    onRefreshTracks: () -> Unit
) {
    val scope = rememberCoroutineScope()
    var isDrawerOpen by remember { mutableStateOf(false) }
    var isDownloadSheetOpen by remember { mutableStateOf(false) }
    var searchQuery by remember { mutableStateOf("") }

    val dlProgress by downloadManager.currentProgress.collectAsState()
    val jamState by jamClient.syncState.collectAsState()

    val filteredTracks = remember(tracks, searchQuery) {
        if (searchQuery.isBlank()) tracks
        else tracks.filter { it.title.contains(searchQuery, ignoreCase = true) || it.artist.contains(searchQuery, ignoreCase = true) }
    }

    Box(modifier = Modifier.fillMaxSize().background(BgObsidian)) {
        Column(modifier = Modifier.fillMaxSize().statusBarsPadding()) {

            // 1. ÜST ÇUBUK (HEADER)
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(60.dp)
                    .background(BgSurface)
                    .padding(horizontal = 14.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                // Menü Hamburger Düğmesi
                IconButton(onClick = { isDrawerOpen = true }) {
                    Icon(Icons.Default.Menu, contentDescription = "Menü", tint = TextWhite)
                }

                Spacer(modifier = Modifier.width(6.dp))

                // Logo ve İsim ("Λ U R N S")
                Text(
                    text = "Λ U R N S",
                    fontSize = 20.sp,
                    fontWeight = FontWeight.Bold,
                    color = TextWhite,
                    letterSpacing = 2.sp
                )

                Spacer(modifier = Modifier.weight(1f))

                // Hızlı "+ İndir" Butonu
                Button(
                    onClick = { isDownloadSheetOpen = true },
                    colors = ButtonDefaults.buttonColors(containerColor = BgCard),
                    shape = RoundedCornerShape(16.dp),
                    contentPadding = PaddingValues(horizontal = 12.dp, vertical = 6.dp)
                ) {
                    Icon(Icons.Default.Add, contentDescription = null, tint = AccentOrange, modifier = Modifier.size(16.dp))
                    Spacer(modifier = Modifier.width(4.dp))
                    Text(text = "İndir", color = TextWhite, fontSize = 13.sp, fontWeight = FontWeight.SemiBold)
                }
            }

            // Arama Kutusu
            OutlinedTextField(
                value = searchQuery,
                onValueChange = { searchQuery = it },
                placeholder = { Text("Parça veya sanatçı ara...", color = TextMuted, fontSize = 13.sp) },
                singleLine = true,
                leadingIcon = { Icon(Icons.Default.Search, contentDescription = null, tint = TextMuted) },
                trailingIcon = {
                    if (searchQuery.isNotEmpty()) {
                        IconButton(onClick = { searchQuery = "" }) {
                            Icon(Icons.Default.Close, contentDescription = "Temizle", tint = TextMuted)
                        }
                    }
                },
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 14.dp, vertical = 8.dp),
                shape = RoundedCornerShape(20.dp),
                colors = OutlinedTextFieldDefaults.colors(
                    focusedContainerColor = BgInput,
                    unfocusedContainerColor = BgInput,
                    focusedBorderColor = AccentOrange,
                    unfocusedBorderColor = BorderDark,
                    focusedTextColor = TextWhite,
                    unfocusedTextColor = TextWhite
                )
            )

            // 2. EVRENSEL CANLI İNDİRME ÇUBUĞU (Sunset-Neon Gradyan)
            AnimatedVisibility(visible = dlProgress.isRunning || dlProgress.isDone) {
                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(horizontal = 14.dp, vertical = 4.dp)
                        .clip(RoundedCornerShape(12.dp))
                        .background(BgCard)
                        .padding(12.dp)
                ) {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Column(modifier = Modifier.weight(1f)) {
                            Text(
                                text = if (dlProgress.isDone) "İNDİRME TAMAMLANDI" else "PARÇA İNDİRİLİYOR",
                                fontSize = 10.sp,
                                fontWeight = FontWeight.Bold,
                                color = if (dlProgress.isDone) AccentOrange else AccentPink
                            )
                            Text(
                                text = dlProgress.title,
                                fontSize = 13.sp,
                                fontWeight = FontWeight.SemiBold,
                                color = TextWhite,
                                maxLines = 1,
                                overflow = TextOverflow.Ellipsis
                            )
                        }
                        Text(
                            text = "%${dlProgress.percent}",
                            fontSize = 15.sp,
                            fontWeight = FontWeight.Bold,
                            color = AccentOrange
                        )
                        if (dlProgress.isRunning) {
                            Spacer(modifier = Modifier.width(8.dp))
                            IconButton(onClick = { downloadManager.cancelActiveDownload() }, modifier = Modifier.size(24.dp)) {
                                Icon(Icons.Default.Close, contentDescription = "İptal", tint = TextMuted)
                            }
                        }
                    }

                    Spacer(modifier = Modifier.height(8.dp))

                    // Sunset-Neon Gradyan İlerleme Çubuğu
                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .height(5.dp)
                            .clip(RoundedCornerShape(3.dp))
                            .background(Color(0xFF202020))
                    ) {
                        Box(
                            modifier = Modifier
                                .fillMaxWidth(dlProgress.percent / 100f)
                                .fillMaxHeight()
                                .background(Brush.horizontalGradient(listOf(AccentOrange, AccentPink, AccentPurple)))
                        )
                    }
                }
            }

            // 3. ŞARKI LİSTESİ & İNTERAKTİF NEON DURUM NOKTALARI
            LazyColumn(
                modifier = Modifier
                    .fillMaxWidth()
                    .weight(1f),
                contentPadding = PaddingValues(bottom = 120.dp, top = 4.dp)
            ) {
                itemsIndexed(filteredTracks) { index, track ->
                    val isCurrent = (index == currentTrackIndex)

                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clickable { onPlayTrack(index) }
                            .background(if (isCurrent) Color(0xFF1E1424) else Color.Transparent)
                            .padding(horizontal = 14.dp, vertical = 10.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        // Sol Numara veya Oynat İkonu
                        Text(
                            text = "${index + 1}",
                            fontSize = 13.sp,
                            color = if (isCurrent) AccentOrange else TextMuted,
                            modifier = Modifier.width(26.dp)
                        )

                        // İNTERAKTİF NEON DURUM NOKTASI (Çalan Şarkıda Dinamik Parlayan Nefes Haleli)
                        if (isCurrent && isPlaying) {
                            val infiniteTransition = rememberInfiniteTransition(label = "pulse")
                            val pulseAlpha by infiniteTransition.animateFloat(
                                initialValue = 0.3f,
                                targetValue = 1f,
                                animationSpec = infiniteRepeatable(
                                    animation = tween(800, easing = FastOutSlowInEasing),
                                    repeatMode = RepeatMode.Reverse
                                ),
                                label = "alpha"
                            )
                            Box(
                                modifier = Modifier
                                    .size(10.dp)
                                    .clip(CircleShape)
                                    .background(AccentPink.copy(alpha = pulseAlpha))
                            )
                        } else {
                            Box(
                                modifier = Modifier
                                    .size(6.dp)
                                    .clip(CircleShape)
                                    .background(if (isCurrent) AccentOrange else Color.Transparent)
                            )
                        }

                        Spacer(modifier = Modifier.width(12.dp))

                        // Şarkı Adı ve Sanatçı
                        Column(modifier = Modifier.weight(1f)) {
                            Text(
                                text = track.title,
                                fontSize = 14.sp,
                                fontWeight = if (isCurrent) FontWeight.Bold else FontWeight.Normal,
                                color = TextWhite,
                                maxLines = 1,
                                overflow = TextOverflow.Ellipsis
                            )
                            Text(
                                text = track.artist,
                                fontSize = 12.sp,
                                color = TextDim,
                                maxLines = 1,
                                overflow = TextOverflow.Ellipsis
                            )
                        }

                        // Süre
                        Text(
                            text = "%02d:%02d".format(track.durationSec / 60, track.durationSec % 60),
                            fontSize = 12.sp,
                            color = TextMuted
                        )
                    }
                }
            }
        }

        // 4. ALT YÜZEN CAM OYNATICI KARTI (FLOATING PLAYER DOCK)
        if (currentTrackIndex in tracks.indices) {
            val curTrack = tracks[currentTrackIndex]
            Box(
                modifier = Modifier
                    .align(Alignment.BottomCenter)
                    .fillMaxWidth()
                    .padding(horizontal = 10.dp, vertical = 12.dp)
                    .clip(RoundedCornerShape(20.dp))
                    .background(BgCard.copy(alpha = 0.95f))
                    .padding(14.dp)
            ) {
                Column(modifier = Modifier.fillMaxWidth()) {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Column(modifier = Modifier.weight(1f)) {
                            Text(
                                text = curTrack.title,
                                fontSize = 14.sp,
                                fontWeight = FontWeight.Bold,
                                color = TextWhite,
                                maxLines = 1,
                                overflow = TextOverflow.Ellipsis
                            )
                            Text(
                                text = curTrack.artist,
                                fontSize = 12.sp,
                                color = AccentOrange,
                                maxLines = 1,
                                overflow = TextOverflow.Ellipsis
                            )
                        }

                        // Kontrol Butonları
                        IconButton(onClick = onPrev) {
                            Icon(Icons.Default.SkipPrevious, contentDescription = "Önceki", tint = TextWhite)
                        }
                        IconButton(
                            onClick = onTogglePlay,
                            modifier = Modifier
                                .size(42.dp)
                                .clip(CircleShape)
                                .background(Brush.linearGradient(listOf(AccentOrange, AccentPink)))
                        ) {
                            Icon(
                                if (isPlaying) Icons.Default.Pause else Icons.Default.PlayArrow,
                                contentDescription = "Oynat/Duraklat",
                                tint = TextWhite
                            )
                        }
                        IconButton(onClick = onNext) {
                            Icon(Icons.Default.SkipNext, contentDescription = "Sonraki", tint = TextWhite)
                        }
                    }

                    Spacer(modifier = Modifier.height(8.dp))

                    // İlerleme Çubuğu ve Süreler
                    Slider(
                        value = if (duration > 0) currentPos.toFloat() / duration else 0f,
                        onValueChange = { onSeek((it * duration).toInt()) },
                        colors = SliderDefaults.colors(
                            thumbColor = AccentOrange,
                            activeTrackColor = AccentPink,
                            inactiveTrackColor = BorderDark
                        ),
                        modifier = Modifier.height(18.dp)
                    )

                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween
                    ) {
                        Text(
                            text = "%02d:%02d".format(currentPos / 60, currentPos % 60),
                            fontSize = 11.sp,
                            color = TextMuted
                        )
                        Text(
                            text = "%02d:%02d".format(duration / 60, duration % 60),
                            fontSize = 11.sp,
                            color = TextMuted
                        )
                    }
                }
            }
        }

        // 5. YOUTUBE İNDİRME ALTYAPI ARAMA PENCERESİ (BottomSheet)
        if (isDownloadSheetOpen) {
            YouTubeDownloadSheet(
                onDismiss = { isDownloadSheetOpen = false },
                onDownload = { res ->
                    downloadManager.startDownload(res.videoId, res.title, res.artist)
                    isDownloadSheetOpen = false
                }
            )
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun YouTubeDownloadSheet(
    onDismiss: () -> Unit,
    onDownload: (SearchResult) -> Unit
) {
    val scope = rememberCoroutineScope()
    var query by remember { mutableStateOf("") }
    var results by remember { mutableStateOf<List<SearchResult>>(emptyList()) }
    var isLoading by remember { mutableStateOf(false) }

    ModalBottomSheet(
        onDismissRequest = onDismiss,
        containerColor = BgSurface,
        dragHandle = { BottomSheetDefaults.DragHandle(color = BorderDark) }
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(16.dp)
                .fillMaxHeight(0.85f)
        ) {
            Text(
                text = "Müzik Ara ve İndir (YouTube Innertube)",
                fontSize = 17.sp,
                fontWeight = FontWeight.Bold,
                color = TextWhite
            )
            Spacer(modifier = Modifier.height(10.dp))

            Row(modifier = Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                OutlinedTextField(
                    value = query,
                    onValueChange = { query = it },
                    placeholder = { Text("Şarkı veya sanatçı adı...", color = TextMuted) },
                    singleLine = true,
                    modifier = Modifier.weight(1f),
                    colors = OutlinedTextFieldDefaults.colors(
                        focusedTextColor = TextWhite,
                        unfocusedTextColor = TextWhite,
                        focusedBorderColor = AccentOrange
                    )
                )
                Spacer(modifier = Modifier.width(8.dp))
                Button(
                    onClick = {
                        if (query.isNotBlank()) {
                            isLoading = true
                            scope.launch {
                                results = InnertubeResolver.search(query)
                                isLoading = false
                            }
                        }
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = AccentOrange)
                ) {
                    Text("Ara")
                }
            }

            Spacer(modifier = Modifier.height(12.dp))

            if (isLoading) {
                Box(modifier = Modifier.fillMaxWidth().height(100.dp), contentAlignment = Alignment.Center) {
                    CircularProgressIndicator(color = AccentOrange)
                }
            } else {
                LazyColumn(modifier = Modifier.fillMaxSize()) {
                    itemsIndexed(results) { _, item ->
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .clickable { onDownload(item) }
                                .padding(vertical = 10.dp),
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Column(modifier = Modifier.weight(1f)) {
                                Text(item.title, color = TextWhite, fontSize = 14.sp, fontWeight = FontWeight.SemiBold, maxLines = 1)
                                Text("${item.artist} • ${item.durationText}", color = TextDim, fontSize = 12.sp)
                            }
                            IconButton(onClick = { onDownload(item) }) {
                                Icon(Icons.Default.Download, contentDescription = "İndir", tint = AccentPink)
                            }
                        }
                        Divider(color = BorderDark, thickness = 0.5.dp)
                    }
                }
            }
        }
    }
}
