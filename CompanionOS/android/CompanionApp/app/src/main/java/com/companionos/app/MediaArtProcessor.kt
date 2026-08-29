package com.companionos.app

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import okhttp3.Request
import java.io.InputStream
import java.util.concurrent.TimeUnit

object MediaArtProcessor {

    private val httpClient = OkHttpClient.Builder()
        .connectTimeout(5, TimeUnit.SECONDS)
        .readTimeout(10, TimeUnit.SECONDS)
        .build()

    @Volatile
    private var currentGeneration = 0

    fun nextGeneration(): Int {
        return ++currentGeneration
    }

    fun isCurrentGeneration(generation: Int): Boolean {
        return generation == currentGeneration
    }

    suspend fun loadBitmapFromUri(context: android.content.Context, uriStr: String): Bitmap? = withContext(Dispatchers.IO) {
        try {
            if (uriStr.startsWith("content://") || uriStr.startsWith("file://") || uriStr.startsWith("android.resource://")) {
                val uri = android.net.Uri.parse(uriStr)
                context.contentResolver.openInputStream(uri)?.use {
                    BitmapFactory.decodeStream(it)
                }
            } else {
                downloadBitmap(uriStr)
            }
        } catch (e: Exception) {
            e.printStackTrace()
            null
        }
    }

    suspend fun downloadBitmap(url: String): Bitmap? = withContext(Dispatchers.IO) {
        try {
            val request = Request.Builder().url(url).build()
            val response = httpClient.newCall(request).execute()
            if (response.isSuccessful) {
                val inputStream: InputStream? = response.body?.byteStream()
                inputStream?.use { BitmapFactory.decodeStream(it) }
            } else null
        } catch (e: Exception) {
            null
        }
    }

    suspend fun fetchAlbumArtOnline(track: String, artist: String): Bitmap? = withContext(Dispatchers.IO) {
        try {
            var cleanTrack = track.replace(Regex("""\(.*?\)|\[.*?\]|-.*"""), "").trim()
            if (cleanTrack.isEmpty()) cleanTrack = track.trim()
            var cleanArtist = artist.replace(Regex("""\(.*?\)|\[.*?\]|-.*"""), "").split(",")[0].trim()
            if (cleanArtist.isEmpty()) cleanArtist = artist.trim()
            
            val query = java.net.URLEncoder.encode("$cleanTrack $cleanArtist".trim(), "UTF-8")
            if (query.isEmpty()) return@withContext null

            // 1. Try Spotify Official Web API
            try {
                val spotifyArtUrl = SpotifyWebApiClient.searchTrackAlbumArtUrl(cleanTrack, cleanArtist)
                if (!spotifyArtUrl.isNullOrEmpty()) {
                    val bmp = downloadBitmap(spotifyArtUrl)
                    if (bmp != null) return@withContext bmp
                }
            } catch (e: Exception) {}

            // 2. Fallback to iTunes Search API (fast, high resolution, zero keys required)
            val itunesUrl = "https://itunes.apple.com/search?term=$query&entity=song&limit=1"
            val req1 = Request.Builder().url(itunesUrl).header("User-Agent", "CompanionOS").build()
            val resp1 = httpClient.newCall(req1).execute()
            if (resp1.isSuccessful) {
                val jsonStr = resp1.body?.string() ?: ""
                val obj = org.json.JSONObject(jsonStr)
                val results = obj.optJSONArray("results")
                if (results != null && results.length() > 0) {
                    val first = results.getJSONObject(0)
                    var artUrl = first.optString("artworkUrl100", "")
                    if (artUrl.isNotEmpty()) {
                        artUrl = artUrl.replace("100x100bb.jpg", "600x600bb.jpg")
                        val bmp = downloadBitmap(artUrl)
                        if (bmp != null) return@withContext bmp
                    }
                }
            }

            // 3. Fallback to Deezer Search API
            val deezerUrl = "https://api.deezer.com/search?q=$query"
            val req2 = Request.Builder().url(deezerUrl).header("User-Agent", "CompanionOS").build()
            val resp2 = httpClient.newCall(req2).execute()
            if (resp2.isSuccessful) {
                val jsonStr = resp2.body?.string() ?: ""
                val obj = org.json.JSONObject(jsonStr)
                val data = obj.optJSONArray("data")
                if (data != null && data.length() > 0) {
                    val first = data.getJSONObject(0)
                    val album = first.optJSONObject("album")
                    val artUrl = album?.optString("cover_medium", "") ?: ""
                    if (artUrl.isNotEmpty()) {
                        val bmp = downloadBitmap(artUrl)
                        if (bmp != null) return@withContext bmp
                    }
                }
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
        return@withContext null
    }

    fun convertToRgb565(bitmap: Bitmap, size: Int = 64): ByteArray {
        val scaled = if (bitmap.width != size || bitmap.height != size) {
            Bitmap.createScaledBitmap(bitmap, size, size, true)
        } else {
            bitmap
        }

        val byteCount = size * size * 2
        val bytes = ByteArray(byteCount)
        val pixels = IntArray(size * size)
        scaled.getPixels(pixels, 0, size, 0, 0, size, size)

        var idx = 0
        for (pixel in pixels) {
            val r = (pixel shr 16) and 0xFF
            val g = (pixel shr 8) and 0xFF
            val b = pixel and 0xFF

            // Exact 565 formula matching Spotify integration
            val rgb565 = ((r and 0xF8) shl 8) or ((g and 0xFC) shl 3) or (b shr 3)
            bytes[idx++] = ((rgb565 shr 8) and 0xFF).toByte() // High byte
            bytes[idx++] = (rgb565 and 0xFF).toByte()        // Low byte
        }
        return bytes
    }

    @Volatile
    private var lastAlbumArtBytes: ByteArray? = null
    @Volatile
    private var lastCustomPhotoBytes: ByteArray? = null

    suspend fun streamAlbumArt(bitmap: Bitmap, generation: Int) = withContext(Dispatchers.IO) {
        try {
            if (!isCurrentGeneration(generation)) return@withContext

            val rgb565Bytes = convertToRgb565(bitmap, 64)
            lastAlbumArtBytes = rgb565Bytes
            if (!isCurrentGeneration(generation)) return@withContext

            CompanionForegroundService.sendUdpDirect("ART_START:")
            delay(100) // Allow ESP SPI/memory clear margin

            if (!isCurrentGeneration(generation)) return@withContext

            // 4 rows of 64 pixels = 256 pixels = 512 bytes per chunk
            val pixelsPerChunk = 64 * 4
            val chunkSize = pixelsPerChunk * 2

            for (i in 0 until rgb565Bytes.size step chunkSize) {
                if (!isCurrentGeneration(generation)) return@withContext

                val end = minOf(i + chunkSize, rgb565Bytes.size)
                val chunkData = rgb565Bytes.copyOfRange(i, end)
                val rowIdx = i / (64 * 2)

                // Custom Binary Packet Header: 0xFE identifies raw binary over UDP
                val packet = ByteArray(3 + chunkData.size)
                packet[0] = 0xFE.toByte()
                packet[1] = ((rowIdx shr 8) and 0xFF).toByte()
                packet[2] = (rowIdx and 0xFF).toByte()
                System.arraycopy(chunkData, 0, packet, 3, chunkData.size)

                CompanionForegroundService.sendUdpBytesDirect(packet)
                delay(20) // Give ESP32 time to render via SPI without dropping packets
            }

            if (isCurrentGeneration(generation)) {
                CompanionForegroundService.sendUdpDirect("ART_COMPLETE:")
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    fun retransmitAlbumArtChunks(rows: List<Int>) {
        val bytes = lastAlbumArtBytes ?: return
        val pixelsPerChunk = 64 * 4
        val chunkSize = pixelsPerChunk * 2

        for (rowIdx in rows) {
            val i = rowIdx * (64 * 2)
            if (i >= 0 && i + chunkSize <= bytes.size) {
                val chunkData = bytes.copyOfRange(i, i + chunkSize)
                val packet = ByteArray(3 + chunkData.size)
                packet[0] = 0xFE.toByte()
                packet[1] = ((rowIdx shr 8) and 0xFF).toByte()
                packet[2] = (rowIdx and 0xFF).toByte()
                System.arraycopy(chunkData, 0, packet, 3, chunkData.size)

                CompanionForegroundService.sendUdpBytesDirect(packet)
            }
        }
        CompanionForegroundService.sendUdpDirect("ART_COMPLETE:")
    }

    suspend fun streamCustomEyeImage(bitmap: Bitmap) = withContext(Dispatchers.IO) {
        try {
            val width = 160
            val height = 128
            val scaled = if (bitmap.width != width || bitmap.height != height) {
                Bitmap.createScaledBitmap(bitmap, width, height, true)
            } else {
                bitmap
            }

            val byteCount = width * height * 2
            val bytes = ByteArray(byteCount)
            val pixels = IntArray(width * height)
            scaled.getPixels(pixels, 0, width, 0, 0, width, height)

            var idx = 0
            for (pixel in pixels) {
                val r = (pixel shr 16) and 0xFF
                val g = (pixel shr 8) and 0xFF
                val b = pixel and 0xFF
                val rgb565 = ((r and 0xF8) shl 8) or ((g and 0xFC) shl 3) or (b shr 3)
                bytes[idx++] = ((rgb565 shr 8) and 0xFF).toByte()
                bytes[idx++] = (rgb565 and 0xFF).toByte()
            }

            lastCustomPhotoBytes = bytes
            CompanionForegroundService.sendUdpDirect("CUSTEYE:START")
            delay(100)

            // Stream 1 row (160 pixels = 320 bytes) per packet with 0xFD header
            val rowBytes = width * 2
            for (row in 0 until height) {
                val chunkData = bytes.copyOfRange(row * rowBytes, (row + 1) * rowBytes)
                val packet = ByteArray(3 + chunkData.size)
                packet[0] = 0xFD.toByte()
                packet[1] = ((row shr 8) and 0xFF).toByte()
                packet[2] = (row and 0xFF).toByte()
                System.arraycopy(chunkData, 0, packet, 3, chunkData.size)

                CompanionForegroundService.sendUdpBytesDirect(packet)
                delay(12)
            }

            // Verify with ESP that all 128 rows were received with ZERO loss
            CompanionForegroundService.sendUdpDirect("CUSTEYE:VERIFY")
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    fun retransmitCustomEyeRows(rows: List<Int>) {
        val bytes = lastCustomPhotoBytes ?: return
        val rowBytes = 160 * 2
        for (row in rows) {
            if (row in 0 until 128) {
                val chunkData = bytes.copyOfRange(row * rowBytes, (row + 1) * rowBytes)
                val packet = ByteArray(3 + chunkData.size)
                packet[0] = 0xFD.toByte()
                packet[1] = ((row shr 8) and 0xFF).toByte()
                packet[2] = (row and 0xFF).toByte()
                System.arraycopy(chunkData, 0, packet, 3, chunkData.size)

                CompanionForegroundService.sendUdpBytesDirect(packet)
            }
        }
        CompanionForegroundService.sendUdpDirect("CUSTEYE:VERIFY")
    }
}
