package com.companionos.app

import android.app.Notification
import android.content.ComponentName
import android.content.Context
import android.graphics.Bitmap
import android.graphics.drawable.BitmapDrawable
import android.graphics.drawable.Icon
import android.media.MediaMetadata
import android.media.session.MediaController
import android.media.session.MediaSessionManager
import android.media.session.PlaybackState
import android.os.Build
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

    private val serviceJob = Job()
    private val serviceScope = CoroutineScope(Dispatchers.IO + serviceJob)

    private var activeMediaController: MediaController? = null
    private var lastTrackTitle = ""
    private var lastArtist = ""
    private var lastAlbumArt: Bitmap? = null
    private var currentLyricsList: List<LrcLibLyricsService.LyricLine> = emptyList()

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

    override fun onListenerConnected() {
        super.onListenerConnected()
        registerMediaSessionListener()
        startPlaybackPositionTicker()
    }

    override fun onDestroy() {
        super.onDestroy()
        serviceJob.cancel()
        try {
            activeMediaController?.unregisterCallback(mediaCallback)
            val mm = getSystemService(Context.MEDIA_SESSION_SERVICE) as? MediaSessionManager
            sessionListener?.let { mm?.removeOnActiveSessionsChangedListener(it) }
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    private fun registerMediaSessionListener() {
        try {
            val mm = getSystemService(Context.MEDIA_SESSION_SERVICE) as? MediaSessionManager
            val component = ComponentName(this, CompanionNotificationListenerService::class.java)

            sessionListener = MediaSessionManager.OnActiveSessionsChangedListener { controllers ->
                if (!controllers.isNullOrEmpty()) {
                    activeMediaController?.unregisterCallback(mediaCallback)
                    activeMediaController = controllers[0]
                    activeMediaController?.registerCallback(mediaCallback)
                    processMediaMetadata(activeMediaController?.metadata, activeMediaController?.playbackState)
                }
            }
            sessionListener?.let { mm?.addOnActiveSessionsChangedListener(it, component) }

            val controllers = mm?.getActiveSessions(component)
            if (!controllers.isNullOrEmpty()) {
                activeMediaController?.unregisterCallback(mediaCallback)
                activeMediaController = controllers[0]
                activeMediaController?.registerCallback(mediaCallback)
                processMediaMetadata(activeMediaController?.metadata, activeMediaController?.playbackState)
            }
        } catch (e: SecurityException) {
            // Permission needs toggle in Settings -> Notification Access
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    private fun processMediaMetadata(metadata: MediaMetadata?, state: PlaybackState?) {
        if (metadata == null) return

        val track = metadata.getString(MediaMetadata.METADATA_KEY_TITLE) ?: ""
        val artist = metadata.getString(MediaMetadata.METADATA_KEY_ARTIST) ?: ""
        val album = metadata.getString(MediaMetadata.METADATA_KEY_ALBUM) ?: ""
        val durationMs = metadata.getLong(MediaMetadata.METADATA_KEY_DURATION)
        val durationSec = if (durationMs > 0) (durationMs / 1000).toInt() else 0

        val isPlaying = state?.state == PlaybackState.STATE_PLAYING
        val positionMs = state?.position ?: 0L
        val progressSec = if (positionMs > 0) (positionMs / 1000).toInt() else 0

        if (track.isEmpty()) return

        // 1. Send all data via SPOTIFY, TRACK, and STATE formats
        val spotifyPayload = JSONObject().apply {
            put("track", track)
            put("artist", artist)
            put("album", album)
            put("duration", durationSec)
            put("progress", progressSec)
            put("playing", isPlaying)
        }
        CompanionForegroundService.sendUdp("SPOTIFY:$spotifyPayload")
        CompanionForegroundService.sendUdp("TRACK:$spotifyPayload")
        CompanionForegroundService.sendUdp("STATE:$spotifyPayload")

        // 2. If track changed, fetch lyrics & stream album art
        if (track != lastTrackTitle || artist != lastArtist) {
            lastTrackTitle = track
            lastArtist = artist

            val gen = MediaArtProcessor.nextGeneration()

            // Fetch synced lyrics from LRCLIB
            serviceScope.launch {
                currentLyricsList = LrcLibLyricsService.fetchLyrics(track, artist)
            }

            // Extract or fetch album art bitmap
            serviceScope.launch {
                var artBitmap = metadata.getBitmap(MediaMetadata.METADATA_KEY_ALBUM_ART)
                    ?: metadata.getBitmap(MediaMetadata.METADATA_KEY_ART)

                if (artBitmap == null) {
                    val artUriStr = metadata.getString(MediaMetadata.METADATA_KEY_ALBUM_ART_URI)
                        ?: metadata.getString(MediaMetadata.METADATA_KEY_ART_URI)
                    if (!artUriStr.isNullOrEmpty()) {
                        artBitmap = MediaArtProcessor.downloadBitmap(artUriStr)
                    }
                }

                if (artBitmap == null) {
                    artBitmap = MediaArtProcessor.fetchAlbumArtOnline(track, artist)
                }

                if (artBitmap != null) {
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
                    val controller = activeMediaController
                    val state = controller?.playbackState
                    if (state != null && state.state == PlaybackState.STATE_PLAYING) {
                        val posMs = state.position
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

                        // Send Progress update
                        val progSec = (posMs / 1000).toInt()
                        val durSec = (durationMs / 1000).toInt()
                        val progPayload = JSONObject().apply {
                            put("progress", progSec)
                            put("duration", durSec)
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
                pkg.contains("spotify") || pkg.contains("music") || pkg.contains("audio") || pkg.contains("player")

        if (isMedia) {
            registerMediaSessionListener()

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

                    val gen = MediaArtProcessor.nextGeneration()
                    serviceScope.launch {
                        currentLyricsList = LrcLibLyricsService.fetchLyrics(title, text)
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

                        if (notifBmp != null) {
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
            pkg.contains("whatsapp") -> "WhatsApp"
            pkg.contains("telegram") -> "Telegram"
            pkg.contains("discord") -> "Discord"
            pkg.contains("mms") || pkg.contains("messaging") -> "SMS"
            pkg.contains("gmail") || pkg.contains("mail") -> "Gmail"
            pkg.contains("instagram") -> "Instagram"
            pkg.contains("twitter") || pkg.contains("x.android") -> "X"
            pkg.contains("slack") -> "Slack"
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
