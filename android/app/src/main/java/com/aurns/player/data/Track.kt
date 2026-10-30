package com.aurns.player.data

import android.net.Uri

data class Track(
    val id: Long,
    val title: String,
    val artist: String,
    val durationSec: Int,
    val path: String,
    val uri: Uri,
    val albumArtUri: Uri? = null,
    val ytId: String? = null
)
