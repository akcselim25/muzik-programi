package com.aurns.player

import android.Manifest
import android.content.ComponentName
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.BackHandler
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.*
import androidx.compose.animation.core.*
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
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
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.ContextCompat
import androidx.media3.common.MediaItem
import androidx.media3.common.Player
import androidx.media3.session.MediaController
import androidx.media3.session.SessionToken
import coil.compose.AsyncImage
import com.aurns.player.data.MusicScanner
import com.aurns.player.data.Track
import com.aurns.player.downloader.AudioDownloadManager
import com.aurns.player.downloader.DownloadProgress
import com.aurns.player.downloader.InnertubeResolver
import com.aurns.player.downloader.SearchResult
import com.aurns.player.jam.JamClient
import com.aurns.player.jam.JamSyncState
import com.aurns.player.service.PlaybackService
import com.aurns.player.ui.theme.*
import com.google.common.util.concurrent.ListenableFuture
import com.google.common.util.concurrent.MoreExecutors
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.launch

class MainActivity : ComponentActivity() {

    private lateinit var downloadManager: AudioDownloadManager
    private val jamClient = JamClient()
    private var controllerFuture: ListenableFuture<MediaController>? = null
    private var mediaController: MediaController? = null

    // Parça Listesi ve Durumları
    private val tracks = mutableStateListOf<Track>()
    private var currentTrackIndex by mutableStateOf(-1)
    private var isPlaying by mutableStateOf(false)
    private var isShuffle by mutableStateOf(false)
    private var isRepeat by mutableStateOf(false)

    // Sadece Dock'un yeniden çizilmesi için akışlar (Ana ekranın gereksiz recomposition'ını engeller)
    private val currentPositionFlow = MutableStateFlow(0)
    private val durationFlow = MutableStateFlow(0)

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

        downloadManager.onDownloadFinishedListener = {
            loadLocalTracks()
            Toast.makeText(this, "Şarkı arşive eklendi!", Toast.LENGTH_SHORT).show()
        }

        checkAndRequestPermissions()
        initMediaController()

        setContent {
            AurnsTheme {
                MainScreen(
                    tracks = tracks,
                    currentTrackIndex = currentTrackIndex,
                    isPlaying = isPlaying,
                    isShuffle = isShuffle,
                    isRepeat = isRepeat,
                    currentPositionFlow = currentPositionFlow,
                    durationFlow = durationFlow,
                    downloadManager = downloadManager,
                    jamClient = jamClient,
                    onPlayTrack = { index -> playTrack(index) },
                    onTogglePlay = { togglePlayPause() },
                    onSeek = { sec -> seekTo(sec) },
                    onNext = { playNext() },
                    onPrev = { playPrev() },
                    onToggleShuffle = { isShuffle = !isShuffle },
                    onToggleRepeat = { isRepeat = !isRepeat },
                    onRefreshTracks = { loadLocalTracks() },
                    onClearTracks = { tracks.clear(); currentTrackIndex = -1; mediaController?.stop() },
                    onSharePlaylist = { sharePlaylist() }
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
                        if (isRepeat) {
                            playTrack(currentTrackIndex)
                        } else {
                            playNext()
                        }
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
                    currentPositionFlow.value = (p.currentPosition / 1000).toInt()
                    durationFlow.value = if (p.duration > 0) (p.duration / 1000).toInt() else 0
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
        jamClient.broadcastState(track.title, "", true, 0)
    }

    private fun togglePlayPause() {
        mediaController?.let { p ->
            if (p.isPlaying) {
                p.pause()
                if (currentTrackIndex in tracks.indices) {
                    jamClient.broadcastState(tracks[currentTrackIndex].title, "", false, (p.currentPosition / 1000).toInt())
                }
            } else {
                p.play()
                if (currentTrackIndex in tracks.indices) {
                    jamClient.broadcastState(tracks[currentTrackIndex].title, "", true, (p.currentPosition / 1000).toInt())
                }
            }
        }
    }

    private fun seekTo(sec: Int) {
        mediaController?.seekTo(sec * 1000L)
    }

    private fun playNext() {
        if (tracks.isEmpty()) return
        val next = if (isShuffle) {
            (tracks.indices).random()
        } else {
            (currentTrackIndex + 1) % tracks.size
        }
        playTrack(next)
    }

    private fun playPrev() {
        if (tracks.isEmpty()) return
        val prev = if (currentTrackIndex - 1 < 0) tracks.size - 1 else currentTrackIndex - 1
        playTrack(prev)
    }

    private fun sharePlaylist() {
        if (tracks.isEmpty()) {
            Toast.makeText(this, "Paylaşılacak parça yok!", Toast.LENGTH_SHORT).show()
            return
        }
        val text = buildString {
            append("🎶 ΛURNS Çalma Listem (${tracks.size} parça):\n\n")
            tracks.take(20).forEachIndexed { i, t ->
                append("${i + 1}. ${t.title} - ${t.artist}\n")
            }
            if (tracks.size > 20) append("...ve ${tracks.size - 20} parça daha!")
        }
        val sendIntent = Intent().apply {
            action = Intent.ACTION_SEND
            putExtra(Intent.EXTRA_TEXT, text)
            type = "text/plain"
        }
        startActivity(Intent.createChooser(sendIntent, "Çalma Listesini Paylaş"))
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
    isShuffle: Boolean,
    isRepeat: Boolean,
    currentPositionFlow: MutableStateFlow<Int>,
    durationFlow: MutableStateFlow<Int>,
    downloadManager: AudioDownloadManager,
    jamClient: JamClient,
    onPlayTrack: (Int) -> Unit,
    onTogglePlay: () -> Unit,
    onSeek: (Int) -> Unit,
    onNext: () -> Unit,
    onPrev: () -> Unit,
    onToggleShuffle: () -> Unit,
    onToggleRepeat: () -> Unit,
    onRefreshTracks: () -> Unit,
    onClearTracks: () -> Unit,
    onSharePlaylist: () -> Unit
) {
    var isDrawerOpen by remember { mutableStateOf(false) }
    var isDownloadSheetOpen by remember { mutableStateOf(false) }
    var isJamDialogOpen by remember { mutableStateOf(false) }
    var isUpdateDialogOpen by remember { mutableStateOf(false) }
    var searchQuery by remember { mutableStateOf("") }

    val dlProgress by downloadManager.currentProgress.collectAsState()
    val jamState by jamClient.syncState.collectAsState()

    // Geri tuşuna basıldığında önce menüyü kapat
    BackHandler(enabled = isDrawerOpen) {
        isDrawerOpen = false
    }

    // Arama filtrelemesi (Performans için derivedStateOf)
    val filteredTracks by remember(tracks, searchQuery) {
        derivedStateOf {
            if (searchQuery.isBlank()) tracks
            else tracks.filter {
                it.title.contains(searchQuery, ignoreCase = true) ||
                it.artist.contains(searchQuery, ignoreCase = true)
            }
        }
    }

    Box(modifier = Modifier.fillMaxSize().background(BgObsidian)) {

        // ANA İÇERİK
        Column(modifier = Modifier.fillMaxSize().statusBarsPadding()) {

            // 1. ÜST ÇUBUK (HEADER - Windows ΛURNS ile %100 Birebir)
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

                // Λ U R N S Logosu
                Text(
                    text = "Λ U R N S",
                    fontSize = 20.sp,
                    fontWeight = FontWeight.Bold,
                    color = TextWhite,
                    letterSpacing = 2.sp
                )

                Spacer(modifier = Modifier.weight(1f))

                // [Arkadaşlar & Jam] Düğmesi
                Surface(
                    onClick = { isJamDialogOpen = true },
                    shape = RoundedCornerShape(16.dp),
                    color = BgCard,
                    border = BorderStroke(1.dp, if (jamState.isConnected) AccentPink else BorderDark),
                    modifier = Modifier.padding(end = 8.dp)
                ) {
                    Row(
                        modifier = Modifier.padding(horizontal = 10.dp, vertical = 6.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Icon(
                            Icons.Default.Groups,
                            contentDescription = null,
                            tint = if (jamState.isConnected) AccentPink else AccentOrange,
                            modifier = Modifier.size(16.dp)
                        )
                        Spacer(modifier = Modifier.width(4.dp))
                        Text(
                            text = if (jamState.isConnected) "Jam Aktif" else "Jam",
                            color = TextWhite,
                            fontSize = 12.sp,
                            fontWeight = FontWeight.SemiBold
                        )
                    }
                }

                // [+ İndir] Düğmesi (Sunset-Neon Gradyan Vurgulu)
                Surface(
                    onClick = { isDownloadSheetOpen = true },
                    shape = RoundedCornerShape(16.dp),
                    color = BgCard,
                    border = BorderStroke(1.dp, Brush.horizontalGradient(listOf(AccentOrange, AccentPink)))
                ) {
                    Row(
                        modifier = Modifier.padding(horizontal = 10.dp, vertical = 6.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Icon(
                            Icons.Default.Add,
                            contentDescription = null,
                            tint = AccentOrange,
                            modifier = Modifier.size(16.dp)
                        )
                        Spacer(modifier = Modifier.width(4.dp))
                        Text(
                            text = "İndir",
                            color = TextWhite,
                            fontSize = 12.sp,
                            fontWeight = FontWeight.SemiBold
                        )
                    }
                }
            }

            // ARAMA KUTUSU (Search Pill)
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
                DownloadProgressBanner(dlProgress = dlProgress, onCancel = { downloadManager.cancelActiveDownload() })
            }

            // 3. ŞARKI LİSTESİ & İNTERAKTİF NEON DURUM NOKTALARI (60 FPS Sıfır Kasıntı)
            if (filteredTracks.isEmpty()) {
                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .weight(1f),
                    contentAlignment = Alignment.Center
                ) {
                    Column(horizontalAlignment = Alignment.CenterHorizontally) {
                        Icon(Icons.Default.MusicNote, contentDescription = null, tint = TextMuted, modifier = Modifier.size(48.dp))
                        Spacer(modifier = Modifier.height(10.dp))
                        Text("Müzik bulunamadı", color = TextDim, fontSize = 15.sp, fontWeight = FontWeight.SemiBold)
                        Spacer(modifier = Modifier.height(4.dp))
                        Text("Sol üst menüden klasör tarayabilir veya + İndir'e basabilirsiniz.", color = TextMuted, fontSize = 12.sp)
                    }
                }
            } else {
                LazyColumn(
                    modifier = Modifier
                        .fillMaxWidth()
                        .weight(1f),
                    contentPadding = PaddingValues(bottom = 140.dp, top = 4.dp)
                ) {
                    itemsIndexed(filteredTracks, key = { _, track -> track.id }) { index, track ->
                        val isCurrent = (index == currentTrackIndex)
                        TrackRowItem(
                            index = index,
                            track = track,
                            isCurrent = isCurrent,
                            isPlaying = isPlaying,
                            onClick = { onPlayTrack(index) }
                        )
                    }
                }
            }
        }

        // 4. ALT YÜZEN CAM OYNATICI KARTI (FLOATING PLAYER DOCK)
        if (currentTrackIndex in tracks.indices) {
            val curTrack = tracks[currentTrackIndex]
            FloatingPlayerDock(
                track = curTrack,
                isPlaying = isPlaying,
                isShuffle = isShuffle,
                isRepeat = isRepeat,
                currentPositionFlow = currentPositionFlow,
                durationFlow = durationFlow,
                onTogglePlay = onTogglePlay,
                onNext = onNext,
                onPrev = onPrev,
                onToggleShuffle = onToggleShuffle,
                onToggleRepeat = onToggleRepeat,
                onSeek = onSeek,
                modifier = Modifier
                    .align(Alignment.BottomCenter)
                    .navigationBarsPadding()
                    .padding(horizontal = 12.dp, vertical = 10.dp)
            )
        }

        // 5. APPLE YAY FİZİKLİ SLIDE-OUT MENÜ (DRAWER)
        if (isDrawerOpen) {
            // Karartıcı Arka Plan (Scrim)
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .background(Color.Black.copy(alpha = 0.65f))
                    .clickable(
                        interactionSource = remember { MutableInteractionSource() },
                        indication = null
                    ) { isDrawerOpen = false }
            )
        }

        AnimatedVisibility(
            visible = isDrawerOpen,
            enter = slideInHorizontally(
                initialOffsetX = { -it },
                animationSpec = spring(dampingRatio = 0.82f, stiffness = 420f)
            ) + fadeIn(animationSpec = tween(200)),
            exit = slideOutHorizontally(
                targetOffsetX = { -it },
                animationSpec = spring(dampingRatio = 0.82f, stiffness = 420f)
            ) + fadeOut(animationSpec = tween(200)),
            modifier = Modifier.fillMaxHeight().width(284.dp)
        ) {
            DrawerContent(
                trackCount = tracks.size,
                onScanFolder = { isDrawerOpen = false; onRefreshTracks() },
                onOpenDownload = { isDrawerOpen = false; isDownloadSheetOpen = true },
                onOpenJam = { isDrawerOpen = false; isJamDialogOpen = true },
                onImportPlaylist = {
                    isDrawerOpen = false
                    Toast.makeText(it, "Çalma listesi içe aktarılıyor...", Toast.LENGTH_SHORT).show()
                },
                onExportPlaylist = { isDrawerOpen = false; onSharePlaylist() },
                onClearList = { isDrawerOpen = false; onClearTracks() },
                onCheckUpdates = { isDrawerOpen = false; isUpdateDialogOpen = true },
                onClose = { isDrawerOpen = false }
            )
        }

        // 6. YOUTUBE İNDİRME PENCERESİ (BottomSheet)
        if (isDownloadSheetOpen) {
            YouTubeDownloadSheet(
                onDismiss = { isDownloadSheetOpen = false },
                onDownload = { res ->
                    downloadManager.startDownload(res.videoId, res.title, res.artist)
                    isDownloadSheetOpen = false
                }
            )
        }

        // 7. ARKADAŞLAR VE JAM CANLI DİNLEME DİYALOĞU
        if (isJamDialogOpen) {
            JamRoomDialog(
                jamClient = jamClient,
                onDismiss = { isJamDialogOpen = false }
            )
        }

        // 8. GÜNCELLEMELERİ DENETLE DİYALOĞU
        if (isUpdateDialogOpen) {
            AlertDialog(
                onDismissRequest = { isUpdateDialogOpen = false },
                containerColor = BgCard,
                title = {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Icon(Icons.Default.SystemUpdate, contentDescription = null, tint = AccentOrange)
                        Spacer(modifier = Modifier.width(8.dp))
                        Text("Güncellemeleri Denetle", color = TextWhite, fontSize = 17.sp, fontWeight = FontWeight.Bold)
                    }
                },
                text = {
                    Column {
                        Text("ΛURNS Android v1.1.0", color = AccentPink, fontWeight = FontWeight.SemiBold, fontSize = 14.sp)
                        Spacer(modifier = Modifier.height(4.dp))
                        Text(
                            "Tebrikler! En güncel ve optimize edilmiş sürümü kullanıyorsunuz.\n• YouTube Doğrudan Ses İndirici\n• Apple Yay Fiziği Menü\n• 60 FPS Ultra Akıcı Obsidian Arayüz",
                            color = TextDim,
                            fontSize = 13.sp
                        )
                    }
                },
                confirmButton = {
                    TextButton(onClick = { isUpdateDialogOpen = false }) {
                        Text("Harika", color = AccentOrange, fontWeight = FontWeight.Bold)
                    }
                }
            )
        }
    }
}

// ==================== BİLEŞENLER ====================

@Composable
fun DrawerContent(
    trackCount: Int,
    onScanFolder: () -> Unit,
    onOpenDownload: () -> Unit,
    onOpenJam: () -> Unit,
    onImportPlaylist: (android.content.Context) -> Unit,
    onExportPlaylist: () -> Unit,
    onClearList: () -> Unit,
    onCheckUpdates: () -> Unit,
    onClose: () -> Unit
) {
    val context = androidx.compose.ui.platform.LocalContext.current

    Surface(
        modifier = Modifier.fillMaxSize(),
        color = BgSurface,
        border = BorderStroke(1.dp, BorderDark)
    ) {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .statusBarsPadding()
                .navigationBarsPadding()
                .padding(horizontal = 14.dp, vertical = 16.dp)
        ) {
            // Üst Başlık
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(
                    text = "Λ U R N S",
                    fontSize = 22.sp,
                    fontWeight = FontWeight.ExtraBold,
                    color = TextWhite,
                    letterSpacing = 3.sp
                )
                Spacer(modifier = Modifier.weight(1f))
                IconButton(onClick = onClose) {
                    Icon(Icons.Default.Close, contentDescription = "Kapat", tint = TextMuted)
                }
            }

            Text(
                text = "$trackCount yerel parça yüklü",
                fontSize = 12.sp,
                color = AccentOrange,
                modifier = Modifier.padding(start = 2.dp, bottom = 18.dp)
            )

            HorizontalDivider(color = BorderDark, thickness = 0.5.dp)
            Spacer(modifier = Modifier.height(14.dp))

            // 7 Menü Maddesi (Windows ΛURNS ile Birebir Sıralama ve Tasarım)
            DrawerMenuItem(
                icon = Icons.Default.FolderOpen,
                title = "Müzik Klasörü Seç",
                subtitle = "Yerel arşivi tara",
                onClick = onScanFolder
            )

            Spacer(modifier = Modifier.height(8.dp))

            // Özel Neon Çerçeveli: Müzik Ara ve İndir
            DrawerMenuItem(
                icon = Icons.Default.CloudDownload,
                title = "Müzik Ara ve İndir",
                subtitle = "YouTube'dan yüksek kalite",
                isSpecial = true,
                onClick = onOpenDownload
            )

            Spacer(modifier = Modifier.height(8.dp))

            // Özel Neon Çerçeveli: Arkadaşlar ve Jam
            DrawerMenuItem(
                icon = Icons.Default.Groups,
                title = "Arkadaşlar ve Jam",
                subtitle = "Birlikte canlı dinle",
                isSpecial = true,
                onClick = onOpenJam
            )

            Spacer(modifier = Modifier.height(8.dp))

            DrawerMenuItem(
                icon = Icons.Default.FileUpload,
                title = "Oynatma Listesini İçe Aktar",
                subtitle = "Paylaşılan listeyi yükle ve indir",
                onClick = { onImportPlaylist(context) }
            )

            Spacer(modifier = Modifier.height(8.dp))

            DrawerMenuItem(
                icon = Icons.Default.FileDownload,
                title = "Oynatma Listesini Dışa Aktar",
                subtitle = "Arkadaşlarınla paylaşmak için",
                onClick = onExportPlaylist
            )

            Spacer(modifier = Modifier.height(8.dp))

            DrawerMenuItem(
                icon = Icons.Default.DeleteSweep,
                title = "Listeyi Temizle",
                subtitle = "Ekranı sıfırla",
                onClick = onClearList
            )

            Spacer(modifier = Modifier.weight(1f))

            HorizontalDivider(color = BorderDark, thickness = 0.5.dp)
            Spacer(modifier = Modifier.height(10.dp))

            DrawerMenuItem(
                icon = Icons.Default.SystemUpdate,
                title = "Güncellemeleri Denetle",
                subtitle = "v1.1.0 sürümü",
                onClick = onCheckUpdates
            )
        }
    }
}

@Composable
fun DrawerMenuItem(
    icon: ImageVector,
    title: String,
    subtitle: String,
    isSpecial: Boolean = false,
    onClick: () -> Unit
) {
    Surface(
        onClick = onClick,
        shape = RoundedCornerShape(12.dp),
        color = if (isSpecial) Color(0xFF18121E) else BgCard,
        border = if (isSpecial) BorderStroke(1.dp, Brush.horizontalGradient(listOf(AccentOrange, AccentPink)))
                 else BorderStroke(0.5.dp, BorderDark),
        modifier = Modifier.fillMaxWidth()
    ) {
        Row(
            modifier = Modifier.padding(horizontal = 12.dp, vertical = 10.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Box(
                modifier = Modifier
                    .size(34.dp)
                    .clip(RoundedCornerShape(8.dp))
                    .background(if (isSpecial) AccentPink.copy(alpha = 0.15f) else Color(0xFF1E1E1E)),
                contentAlignment = Alignment.Center
            ) {
                Icon(
                    icon,
                    contentDescription = null,
                    tint = if (isSpecial) AccentOrange else TextWhite,
                    modifier = Modifier.size(18.dp)
                )
            }

            Spacer(modifier = Modifier.width(12.dp))

            Column(modifier = Modifier.weight(1f)) {
                Text(
                    text = title,
                    color = TextWhite,
                    fontSize = 13.sp,
                    fontWeight = FontWeight.SemiBold
                )
                Text(
                    text = subtitle,
                    color = TextDim,
                    fontSize = 11.sp
                )
            }
        }
    }
}

@Composable
fun TrackRowItem(
    index: Int,
    track: Track,
    isCurrent: Boolean,
    isPlaying: Boolean,
    onClick: () -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clickable(onClick = onClick)
            .background(if (isCurrent) Color(0xFF1B1422) else Color.Transparent)
            .padding(horizontal = 14.dp, vertical = 10.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        // Sol İndeks Numarası
        Text(
            text = "${index + 1}",
            fontSize = 13.sp,
            color = if (isCurrent) AccentOrange else TextMuted,
            modifier = Modifier.width(26.dp)
        )

        // İNTERAKTİF NEON DURUM NOKTASI (Aura Dots)
        AuraPulseDot(isCurrent = isCurrent, isPlaying = isPlaying)

        Spacer(modifier = Modifier.width(12.dp))

        // Parça Başlığı ve Sanatçı
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
                color = if (isCurrent) AccentOrange else TextDim,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
        }

        // Parça Süresi
        Text(
            text = "%02d:%02d".format(track.durationSec / 60, track.durationSec % 60),
            fontSize = 12.sp,
            color = TextMuted
        )
    }
}

@Composable
fun AuraPulseDot(isCurrent: Boolean, isPlaying: Boolean) {
    if (isCurrent && isPlaying) {
        val infiniteTransition = rememberInfiniteTransition(label = "pulse")
        val alpha by infiniteTransition.animateFloat(
            initialValue = 0.35f,
            targetValue = 1f,
            animationSpec = infiniteRepeatable(
                animation = tween(800, easing = FastOutSlowInEasing),
                repeatMode = RepeatMode.Reverse
            ),
            label = "pulseAlpha"
        )
        Box(
            modifier = Modifier
                .size(10.dp)
                .clip(CircleShape)
                .background(AccentPink.copy(alpha = alpha))
        )
    } else {
        Box(
            modifier = Modifier
                .size(6.dp)
                .clip(CircleShape)
                .background(if (isCurrent) AccentOrange else Color.Transparent)
        )
    }
}

@Composable
fun FloatingPlayerDock(
    track: Track,
    isPlaying: Boolean,
    isShuffle: Boolean,
    isRepeat: Boolean,
    currentPositionFlow: MutableStateFlow<Int>,
    durationFlow: MutableStateFlow<Int>,
    onTogglePlay: () -> Unit,
    onNext: () -> Unit,
    onPrev: () -> Unit,
    onToggleShuffle: () -> Unit,
    onToggleRepeat: () -> Unit,
    onSeek: (Int) -> Unit,
    modifier: Modifier = Modifier
) {
    val currentPos by currentPositionFlow.collectAsState()
    val duration by durationFlow.collectAsState()

    Surface(
        modifier = modifier.fillMaxWidth(),
        shape = RoundedCornerShape(20.dp),
        color = BgCard.copy(alpha = 0.96f),
        border = BorderStroke(1.dp, BorderDark),
        shadowElevation = 12.dp
    ) {
        Column(modifier = Modifier.padding(14.dp)) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically
            ) {
                // Albüm Kapağı Kutusu
                Box(
                    modifier = Modifier
                        .size(46.dp)
                        .clip(RoundedCornerShape(10.dp))
                        .background(Color(0xFF222222)),
                    contentAlignment = Alignment.Center
                ) {
                    AsyncImage(
                        model = track.albumArtUri,
                        contentDescription = null,
                        contentScale = ContentScale.Crop,
                        modifier = Modifier.fillMaxSize()
                    )
                }

                Spacer(modifier = Modifier.width(12.dp))

                // Şarkı ve Sanatçı Bilgisi
                Column(modifier = Modifier.weight(1f)) {
                    Text(
                        text = track.title,
                        fontSize = 14.sp,
                        fontWeight = FontWeight.Bold,
                        color = TextWhite,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis
                    )
                    Text(
                        text = track.artist,
                        fontSize = 12.sp,
                        color = AccentOrange,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis
                    )
                }

                // Karışık Çal (Shuffle)
                IconButton(onClick = onToggleShuffle, modifier = Modifier.size(32.dp)) {
                    Icon(
                        Icons.Default.Shuffle,
                        contentDescription = "Karışık",
                        tint = if (isShuffle) AccentOrange else TextMuted,
                        modifier = Modifier.size(18.dp)
                    )
                }

                // Önceki Parça
                IconButton(onClick = onPrev, modifier = Modifier.size(36.dp)) {
                    Icon(Icons.Default.SkipPrevious, contentDescription = "Önceki", tint = TextWhite)
                }

                // Oynat / Duraklat (Neon Sunset Gradyan Buton)
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
                        tint = TextWhite,
                        modifier = Modifier.size(24.dp)
                    )
                }

                // Sonraki Parça
                IconButton(onClick = onNext, modifier = Modifier.size(36.dp)) {
                    Icon(Icons.Default.SkipNext, contentDescription = "Sonraki", tint = TextWhite)
                }

                // Tekrar Çal (Repeat)
                IconButton(onClick = onToggleRepeat, modifier = Modifier.size(32.dp)) {
                    Icon(
                        Icons.Default.Repeat,
                        contentDescription = "Tekrar",
                        tint = if (isRepeat) AccentPink else TextMuted,
                        modifier = Modifier.size(18.dp)
                    )
                }
            }

            Spacer(modifier = Modifier.height(8.dp))

            // İlerleme Kaydırıcısı (Slider)
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

@Composable
fun DownloadProgressBanner(dlProgress: DownloadProgress, onCancel: () -> Unit) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 14.dp, vertical = 4.dp)
            .clip(RoundedCornerShape(12.dp))
            .background(BgCard)
            .border(0.5.dp, BorderDark, RoundedCornerShape(12.dp))
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
                IconButton(onClick = onCancel, modifier = Modifier.size(24.dp)) {
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
    var hasSearched by remember { mutableStateOf(false) }

    ModalBottomSheet(
        onDismissRequest = onDismiss,
        containerColor = BgSurface,
        dragHandle = { BottomSheetDefaults.DragHandle(color = BorderDark) }
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 16.dp, vertical = 8.dp)
                .fillMaxHeight(0.85f)
        ) {
            Text(
                text = "Müzik Ara ve İndir (YouTube Innertube)",
                fontSize = 18.sp,
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
                    shape = RoundedCornerShape(12.dp),
                    colors = OutlinedTextFieldDefaults.colors(
                        focusedTextColor = TextWhite,
                        unfocusedTextColor = TextWhite,
                        focusedBorderColor = AccentOrange,
                        unfocusedBorderColor = BorderDark,
                        focusedContainerColor = BgInput,
                        unfocusedContainerColor = BgInput
                    )
                )
                Spacer(modifier = Modifier.width(8.dp))
                Button(
                    onClick = {
                        if (query.isNotBlank()) {
                            isLoading = true
                            hasSearched = true
                            scope.launch {
                                results = InnertubeResolver.search(query)
                                isLoading = false
                            }
                        }
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = AccentOrange),
                    shape = RoundedCornerShape(12.dp)
                ) {
                    Text("Ara", fontWeight = FontWeight.Bold)
                }
            }

            Spacer(modifier = Modifier.height(14.dp))

            if (isLoading) {
                Box(
                    modifier = Modifier.fillMaxWidth().height(160.dp),
                    contentAlignment = Alignment.Center
                ) {
                    CircularProgressIndicator(color = AccentOrange)
                }
            } else if (hasSearched && results.isEmpty()) {
                Box(
                    modifier = Modifier.fillMaxWidth().height(160.dp),
                    contentAlignment = Alignment.Center
                ) {
                    Text("Sonuç bulunamadı veya bağlantı kurulamadı.", color = TextDim, fontSize = 14.sp)
                }
            } else {
                LazyColumn(
                    modifier = Modifier.fillMaxSize(),
                    contentPadding = PaddingValues(bottom = 30.dp)
                ) {
                    itemsIndexed(results, key = { _, item -> item.videoId }) { _, item ->
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .clickable { onDownload(item) }
                                .padding(vertical = 10.dp),
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            // Video Küçük Resmi (Thumbnail)
                            AsyncImage(
                                model = item.thumbnailUrl,
                                contentDescription = null,
                                contentScale = ContentScale.Crop,
                                modifier = Modifier
                                    .size(54.dp, 40.dp)
                                    .clip(RoundedCornerShape(6.dp))
                                    .background(Color(0xFF222222))
                            )

                            Spacer(modifier = Modifier.width(12.dp))

                            Column(modifier = Modifier.weight(1f)) {
                                Text(
                                    text = item.title,
                                    color = TextWhite,
                                    fontSize = 14.sp,
                                    fontWeight = FontWeight.SemiBold,
                                    maxLines = 1,
                                    overflow = TextOverflow.Ellipsis
                                )
                                Text(
                                    text = "${item.artist} • ${item.durationText}",
                                    color = TextDim,
                                    fontSize = 12.sp,
                                    maxLines = 1,
                                    overflow = TextOverflow.Ellipsis
                                )
                            }

                            IconButton(onClick = { onDownload(item) }) {
                                Icon(Icons.Default.CloudDownload, contentDescription = "İndir", tint = AccentPink)
                            }
                        }
                        HorizontalDivider(color = BorderDark, thickness = 0.5.dp)
                    }
                }
            }
        }
    }
}

@Composable
fun JamRoomDialog(
    jamClient: JamClient,
    onDismiss: () -> Unit
) {
    val syncState by jamClient.syncState.collectAsState()
    var roomInput by remember { mutableStateOf(syncState.roomCode) }

    AlertDialog(
        onDismissRequest = onDismiss,
        containerColor = BgCard,
        title = {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Icon(Icons.Default.Groups, contentDescription = null, tint = AccentPink)
                Spacer(modifier = Modifier.width(8.dp))
                Text("Arkadaşlar & Jam Canlı Dinle", color = TextWhite, fontSize = 17.sp, fontWeight = FontWeight.Bold)
            }
        },
        text = {
            Column {
                Text(
                    text = "Aynı oda koduna sahip tüm arkadaşlarınızla (Windows & Android) canlı şarkı ve süre senkronizasyonu yapılır.",
                    color = TextDim,
                    fontSize = 12.sp
                )

                Spacer(modifier = Modifier.height(14.dp))

                OutlinedTextField(
                    value = roomInput,
                    onValueChange = { roomInput = it.take(12) },
                    label = { Text("Oda Kodu (Örn: SELO1)", color = TextMuted) },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                    shape = RoundedCornerShape(10.dp),
                    colors = OutlinedTextFieldDefaults.colors(
                        focusedTextColor = TextWhite,
                        unfocusedTextColor = TextWhite,
                        focusedBorderColor = AccentPink,
                        unfocusedBorderColor = BorderDark,
                        focusedContainerColor = BgInput,
                        unfocusedContainerColor = BgInput
                    )
                )

                if (syncState.isConnected) {
                    Spacer(modifier = Modifier.height(10.dp))
                    Text(
                        text = "● Odaya Bağlı: ${syncState.roomCode}",
                        color = Color(0xFF34C759),
                        fontWeight = FontWeight.SemiBold,
                        fontSize = 12.sp
                    )
                }
            }
        },
        confirmButton = {
            if (syncState.isConnected) {
                TextButton(onClick = { jamClient.leaveRoom() }) {
                    Text("Odadan Ayrıl", color = TextDanger, fontWeight = FontWeight.Bold)
                }
            } else {
                Button(
                    onClick = {
                        if (roomInput.isNotBlank()) {
                            jamClient.joinRoom(roomInput.trim(), "Android_${Build.MODEL}")
                            onDismiss()
                        }
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = AccentPink),
                    shape = RoundedCornerShape(10.dp)
                ) {
                    Text("Odaya Katıl", color = Color.White, fontWeight = FontWeight.Bold)
                }
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) {
                Text("Kapat", color = TextMuted)
            }
        }
    )
}
