package com.companionos.app

import android.app.Notification
import android.content.BroadcastReceiver
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.graphics.Bitmap
import android.graphics.drawable.BitmapDrawable
import android.graphics.drawable.Icon
import android.media.MediaMetadata
import android.media.session.MediaController
import android.media.session.MediaSessionManager
import android.media.session.PlaybackState
import android.os.Build
import android.os.SystemClock
import android.service.notification.NotificationListenerService
import android.service.notification.StatusBarNotification
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import org.json.JSONObject
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

class CompanionNotificationListenerService : NotificationListenerService() {

    companion object {
        var instance: CompanionNotificationListenerService? = null
    }

    private val serviceJob = Job()
    private val serviceScope = CoroutineScope(Dispatchers.IO + serviceJob)

    private var activeMediaController: MediaController? = null
    private var lastTrackTitle = ""
    private var lastArtist = ""
    private var lastAlbumArt: Bitmap? = null
    private var currentLyricsList: List<LrcLibLyricsService.LyricLine> = emptyList()
    private var isSpotifyReceiverRegistered = false

    private val mediaCallback = object : MediaController.Callback() {
        override fun onMetadataChanged(metadata: MediaMetadata?) {
            super.onMetadataChanged(metadata)
            processMediaMetadata(metadata, activeMediaController?.playbackState)
        }

        override fun onPlaybackStateChanged(state: PlaybackState?) {
            super.onPlaybackStateChanged(state)
            processMediaMetadata(activeMediaController?.metadata, state)
        }
    }

    private var sessionListener: MediaSessionManager.OnActiveSessionsChangedListener? = null

    // Native Spotify Broadcast Receiver for instant zero-latency tracking
    private val spotifyBroadcastReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            if (intent == null) return
            try {
                val action = intent.action ?: ""
                val isPlaying = intent.getBooleanExtra("playing", false)
                val track = intent.getStringExtra("track") ?: ""
                val artist = intent.getStringExtra("artist") ?: ""
                val album = intent.getStringExtra("album") ?: ""
                val duration = intent.getIntExtra("length", 0).toLong()
                val position = intent.getIntExtra("playbackPosition", 0).toLong()

                if (track.isNotEmpty()) {
                    handleDirectTrackUpdate(track, artist, album, duration, position, isPlaying)
                }
            } catch (e: Exception) {
                e.printStackTrace()
            }
        }
    }

    override fun onCreate() {
        super.onCreate()
        instance = this
        registerSpotifyBroadcasts()
    }

    override fun onListenerConnected() {
        super.onListenerConnected()
        instance = this
        registerSpotifyBroadcasts()
        registerMediaSessionListener()
        startPlaybackPositionTicker()
    }

    private fun registerSpotifyBroadcasts() {
        if (isSpotifyReceiverRegistered) return
        try {
            val filter = IntentFilter().apply {
                addAction("com.spotify.music.playbackstatechanged")
                addAction("com.spotify.music.metadatachanged")
                addAction("com.spotify.music.queuechanged")
            }
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                registerReceiver(spotifyBroadcastReceiver, filter, Context.RECEIVER_EXPORTED)
            } else {
                registerReceiver(spotifyBroadcastReceiver, filter)
            }
            isSpotifyReceiverRegistered = true
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        if (instance == this) instance = null
        serviceJob.cancel()
        try {
            if (isSpotifyReceiverRegistered) {
                unregisterReceiver(spotifyBroadcastReceiver)
                isSpotifyReceiverRegistered = false
            }
            activeMediaController?.unregisterCallback(mediaCallback)
            val mm = getSystemService(Context.MEDIA_SESSION_SERVICE) as? MediaSessionManager
            sessionListener?.let { mm?.removeOnActiveSessionsChangedListener(it) }
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    fun handleMediaAction(action: String) {
        try {
            // Tier 1: Active MediaSession Controller
            val controls = activeMediaController?.transportControls
            if (controls != null) {
                when (action.uppercase()) {
                    "PLAY_PAUSE", "TOGGLE", "PLAY", "PAUSE" -> {
                        val state = activeMediaController?.playbackState?.state
                        if (state == PlaybackState.STATE_PLAYING) {
                            controls.pause()
                        } else {
                            controls.play()
                        }
                        return
                    }
                    "NEXT", "SKIP_NEXT" -> {
                        controls.skipToNext()
                        return
                    }
                    "PREV", "SKIP_PREV", "PREVIOUS" -> {
                        controls.skipToPrevious()
                        return
                    }
                    "STOP" -> {
                        controls.stop()
                        return
                    }
                }
            }

            // Tier 2: Spotify Direct Broadcast Intents
            val spotifyIntent = when (action.uppercase()) {
                "PLAY_PAUSE", "TOGGLE", "PLAY", "PAUSE" -> Intent("com.spotify.music.playbackcontrol.togglepause")
                "NEXT", "SKIP_NEXT" -> Intent("com.spotify.mobile.android.ui.widget.NEXT")
                "PREV", "SKIP_PREV", "PREVIOUS" -> Intent("com.spotify.mobile.android.ui.widget.PREVIOUS")
                else -> null
            }
            if (spotifyIntent != null) {
                spotifyIntent.setPackage("com.spotify.music")
                sendBroadcast(spotifyIntent)
            }

            // Tier 3: Universal System Media Key Events
            val audioManager = getSystemService(Context.AUDIO_SERVICE) as? android.media.AudioManager
            val keyCode = when (action.uppercase()) {
                "PLAY_PAUSE", "TOGGLE", "PLAY", "PAUSE" -> android.view.KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE
                "NEXT", "SKIP_NEXT" -> android.view.KeyEvent.KEYCODE_MEDIA_NEXT
                "PREV", "SKIP_PREV", "PREVIOUS" -> android.view.KeyEvent.KEYCODE_MEDIA_PREVIOUS
                "STOP" -> android.view.KeyEvent.KEYCODE_MEDIA_STOP
                else -> null
            }
            if (audioManager != null && keyCode != null) {
                val downEvent = android.view.KeyEvent(android.view.KeyEvent.ACTION_DOWN, keyCode)
                val upEvent = android.view.KeyEvent(android.view.KeyEvent.ACTION_UP, keyCode)
                audioManager.dispatchMediaKeyEvent(downEvent)
                audioManager.dispatchMediaKeyEvent(upEvent)
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    fun scanAndBindActiveMediaController() {
        try {
            val mm = getSystemService(Context.MEDIA_SESSION_SERVICE) as? MediaSessionManager ?: return
            val component = ComponentName(this, CompanionNotificationListenerService::class.java)
            val controllers = mm.getActiveSessions(component)

            if (!controllers.isNullOrEmpty()) {
                var bestController: MediaController? = null
                // Priority 1: Controller currently in STATE_PLAYING
                for (c in controllers) {
                    if (c.playbackState?.state == PlaybackState.STATE_PLAYING) {
                        bestController = c
                        break
                    }
                }
                // Priority 2: Spotify package
                if (bestController == null) {
                    for (c in controllers) {
                        if (c.packageName.contains("spotify", ignoreCase = true)) {
                            bestController = c
                            break
                        }
                    }
                }
                // Priority 3: First available
                if (bestController == null) {
                    bestController = controllers[0]
                }

                if (bestController != activeMediaController) {
                    activeMediaController?.unregisterCallback(mediaCallback)
                    activeMediaController = bestController
                    activeMediaController?.registerCallback(mediaCallback)
                }

                processMediaMetadata(activeMediaController?.metadata, activeMediaController?.playbackState)
            }
        } catch (e: SecurityException) {
            // Needs Notification Access permission
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    private fun registerMediaSessionListener() {
        try {
            val mm = getSystemService(Context.MEDIA_SESSION_SERVICE) as? MediaSessionManager
            val component = ComponentName(this, CompanionNotificationListenerService::class.java)

            sessionListener = MediaSessionManager.OnActiveSessionsChangedListener { controllers ->
                scanAndBindActiveMediaController()
            }
            sessionListener?.let { mm?.addOnActiveSessionsChangedListener(it, component) }

            scanAndBindActiveMediaController()
        } catch (e: SecurityException) {
            // Permission needs toggle in Settings -> Notification Access
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    private fun handleDirectTrackUpdate(track: String, artist: String, album: String, durationMs: Long, positionMs: Long, isPlaying: Boolean) {
        val spotifyPayload = JSONObject().apply {
            put("track", track)
            put("artist", artist)
            put("album", album)
            put("duration", durationMs)
            put("progress", positionMs)
            put("playing", isPlaying)
        }
        CompanionForegroundService.sendUdp("SPOTIFY:$spotifyPayload")
        CompanionForegroundService.sendUdp("TRACK:$spotifyPayload")
        CompanionForegroundService.sendUdp("STATE:$spotifyPayload")

        if (track != lastTrackTitle || artist != lastArtist) {
            lastTrackTitle = track
            lastArtist = artist

            currentLyricsList = emptyList()
            CompanionForegroundService.sendUdp("LYRICS:{\"line1\":\"\",\"line2\":\"\",\"prev\":\"\"}")

            val gen = MediaArtProcessor.nextGeneration()
            serviceScope.launch {
                val lyrics = LrcLibLyricsService.fetchLyrics(track, artist)
                if (MediaArtProcessor.isCurrentGeneration(gen)) {
                    currentLyricsList = lyrics
                }
            }

            serviceScope.launch {
                val artBitmap = MediaArtProcessor.fetchAlbumArtOnline(track, artist)
                if (artBitmap != null && MediaArtProcessor.isCurrentGeneration(gen)) {
                    lastAlbumArt = artBitmap
                    MediaArtProcessor.streamAlbumArt(artBitmap, gen)
                }
            }
        }
    }

    private fun processMediaMetadata(metadata: MediaMetadata?, state: PlaybackState?) {
        if (metadata == null) return

        val track = metadata.getString(MediaMetadata.METADATA_KEY_TITLE) ?: ""
        val artist = metadata.getString(MediaMetadata.METADATA_KEY_ARTIST) ?: ""
        val album = metadata.getString(MediaMetadata.METADATA_KEY_ALBUM) ?: ""
        val durationMs = metadata.getLong(MediaMetadata.METADATA_KEY_DURATION)

        val isPlaying = state?.state == PlaybackState.STATE_PLAYING
        val basePos = state?.position ?: 0L
        val lastUpdate = state?.lastPositionUpdateTime ?: 0L
        val currentPos = if (isPlaying && lastUpdate > 0) {
            val delta = SystemClock.elapsedRealtime() - lastUpdate
            val speed = if (state?.playbackSpeed != null && state.playbackSpeed > 0) state.playbackSpeed else 1.0f
            basePos + (delta * speed).toLong()
        } else {
            basePos
        }

        if (track.isEmpty()) return

        // 1. Send all data via SPOTIFY, TRACK, and STATE formats
        val spotifyPayload = JSONObject().apply {
            put("track", track)
            put("artist", artist)
            put("album", album)
            put("duration", durationMs)
            put("progress", currentPos)
            put("playing", isPlaying)
        }
        CompanionForegroundService.sendUdp("SPOTIFY:$spotifyPayload")
        CompanionForegroundService.sendUdp("TRACK:$spotifyPayload")
        CompanionForegroundService.sendUdp("STATE:$spotifyPayload")

        // 2. If track changed, fetch lyrics & stream album art
        if (track != lastTrackTitle || artist != lastArtist) {
            lastTrackTitle = track
            lastArtist = artist

            // Instantly clear old lyrics and wipe screen
            currentLyricsList = emptyList()
            CompanionForegroundService.sendUdp("LYRICS:{\"line1\":\"\",\"line2\":\"\",\"prev\":\"\"}")

            val gen = MediaArtProcessor.nextGeneration()

            // Fetch synced lyrics from LRCLIB
            serviceScope.launch {
                val lyrics = LrcLibLyricsService.fetchLyrics(track, artist)
                if (MediaArtProcessor.isCurrentGeneration(gen)) {
                    currentLyricsList = lyrics
                }
            }

            // Extract or fetch album art bitmap
            serviceScope.launch {
                var artBitmap = metadata.getBitmap(MediaMetadata.METADATA_KEY_ALBUM_ART)
                    ?: metadata.getBitmap(MediaMetadata.METADATA_KEY_ART)

                if (artBitmap == null) {
                    val artUriStr = metadata.getString(MediaMetadata.METADATA_KEY_ALBUM_ART_URI)
                        ?: metadata.getString(MediaMetadata.METADATA_KEY_ART_URI)
                    if (artUriStr != null) {
                        artBitmap = MediaArtProcessor.loadBitmapFromUri(applicationContext, artUriStr)
                    }
                }

                if (artBitmap == null) {
                    artBitmap = MediaArtProcessor.fetchAlbumArtOnline(track, artist)
                }

                if (artBitmap != null && MediaArtProcessor.isCurrentGeneration(gen)) {
                    lastAlbumArt = artBitmap
                    MediaArtProcessor.streamAlbumArt(artBitmap, gen)
                }
            }
        }
    }

    private fun startPlaybackPositionTicker() {
        serviceScope.launch {
            while (isActive) {
                try {
                    // Continuous session scan if controller is null or idle
                    if (activeMediaController == null || activeMediaController?.playbackState?.state != PlaybackState.STATE_PLAYING) {
                        scanAndBindActiveMediaController()
                    }

                    val controller = activeMediaController
                    val state = controller?.playbackState
                    if (state != null && state.state == PlaybackState.STATE_PLAYING) {
                        val basePos = state.position
                        val lastUpdate = state.lastPositionUpdateTime
                        val posMs = if (lastUpdate > 0) {
                            val delta = SystemClock.elapsedRealtime() - lastUpdate
                            val speed = if (state.playbackSpeed > 0) state.playbackSpeed else 1.0f
                            basePos + (delta * speed).toLong()
                        } else {
                            basePos
                        }
                        val durationMs = controller.metadata?.getLong(MediaMetadata.METADATA_KEY_DURATION) ?: 0L

                        // Tick Synced Lyrics
                        if (currentLyricsList.isNotEmpty()) {
                            val (prev, cur, next) = LrcLibLyricsService.getLyricsAtPosition(currentLyricsList, posMs)
                            val lyricsPayload = JSONObject().apply {
                                put("line1", cur)
                                put("line2", next)
                                put("prev", prev)
                            }
                            CompanionForegroundService.sendUdp("LYRICS:$lyricsPayload")
                        }

                        // Send Progress update with millisecond precision
                        val progPayload = JSONObject().apply {
                            put("progress", posMs)
                            put("duration", durationMs)
                        }
                        CompanionForegroundService.sendUdp("PROGRESS:$progPayload")
                    }
                } catch (e: Exception) {
                    // Resilient ticker
                }
                delay(500) // 500ms lyric/progress sync rate
            }
        }
    }

    override fun onNotificationPosted(sbn: StatusBarNotification?) {
        super.onNotificationPosted(sbn)
        if (sbn == null) return

        val pkg = sbn.packageName ?: ""
        val extras = sbn.notification.extras ?: return

        // 1. Check if media notification (Spotify, YouTube Music, Apple Music, etc.)
        val isMedia = sbn.notification.category == Notification.CATEGORY_TRANSPORT ||
                pkg.contains("spotify", ignoreCase = true) || pkg.contains("music", ignoreCase = true) ||
                pkg.contains("audio", ignoreCase = true) || pkg.contains("player", ignoreCase = true)

        if (isMedia) {
            scanAndBindActiveMediaController()

            val title = extras.getCharSequence(Notification.EXTRA_TITLE)?.toString() ?: ""
            val text = extras.getCharSequence(Notification.EXTRA_TEXT)?.toString() ?: ""
            val subText = extras.getCharSequence(Notification.EXTRA_SUB_TEXT)?.toString() ?: ""

            if (title.isNotEmpty()) {
                val mediaPayload = JSONObject().apply {
                    put("track", title)
                    put("artist", text)
                    put("album", subText)
                    put("playing", true)
                }
                CompanionForegroundService.sendUdp("SPOTIFY:$mediaPayload")
                CompanionForegroundService.sendUdp("TRACK:$mediaPayload")

                // Extract artwork from notification icon if not yet set
                if (title != lastTrackTitle || text != lastArtist) {
                    lastTrackTitle = title
                    lastArtist = text

                    // Instantly clear old lyrics
                    currentLyricsList = emptyList()
                    CompanionForegroundService.sendUdp("LYRICS:{\"line1\":\"\",\"line2\":\"\",\"prev\":\"\"}")

                    val gen = MediaArtProcessor.nextGeneration()
                    serviceScope.launch {
                        val fetched = LrcLibLyricsService.fetchLyrics(title, text)
                        if (MediaArtProcessor.isCurrentGeneration(gen)) {
                            currentLyricsList = fetched
                        }
                    }

                    serviceScope.launch {
                        var notifBmp: Bitmap? = null
                        try {
                            val largeIcon = extras.get(Notification.EXTRA_LARGE_ICON)
                            if (largeIcon is Bitmap) {
                                notifBmp = largeIcon
                            } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M && largeIcon is Icon) {
                                val drawable = largeIcon.loadDrawable(applicationContext)
                                if (drawable is BitmapDrawable) {
                                    notifBmp = drawable.bitmap
                                }
                            }
                        } catch (e: Exception) {}

                        if (notifBmp == null) {
                            notifBmp = MediaArtProcessor.fetchAlbumArtOnline(title, text)
                        }

                        if (notifBmp != null && MediaArtProcessor.isCurrentGeneration(gen)) {
                            lastAlbumArt = notifBmp
                            MediaArtProcessor.streamAlbumArt(notifBmp, gen)
                        }
                    }
                }
            }
            return
        }

        // 2. Map incoming chat / app notifications
        val title = extras.getString(Notification.EXTRA_TITLE) ?: ""
        val text = extras.getCharSequence(Notification.EXTRA_TEXT)?.toString() ?: ""

        if (title.isEmpty() || text.isEmpty()) return

        val appName = when {
            pkg.contains("whatsapp", ignoreCase = true) -> "WhatsApp"
            pkg.contains("telegram", ignoreCase = true) -> "Telegram"
            pkg.contains("discord", ignoreCase = true) -> "Discord"
            pkg.contains("mms", ignoreCase = true) || pkg.contains("messaging", ignoreCase = true) -> "SMS"
            pkg.contains("gmail", ignoreCase = true) || pkg.contains("mail", ignoreCase = true) -> "Gmail"
            pkg.contains("instagram", ignoreCase = true) -> "Instagram"
            pkg.contains("twitter", ignoreCase = true) || pkg.contains("x.android", ignoreCase = true) -> "X"
            pkg.contains("slack", ignoreCase = true) -> "Slack"
            else -> pkg.split(".").lastOrNull()?.replaceFirstChar { it.uppercase() } ?: "Alert"
        }

        val timeStr = SimpleDateFormat("HH:mm", Locale.getDefault()).format(Date())

        val notifPayload = JSONObject().apply {
            put("app", appName)
            put("title", title)
            put("text", text)
            put("time", timeStr)
        }

        CompanionForegroundService.sendUdp("NOTIF:$notifPayload")
    }
}
