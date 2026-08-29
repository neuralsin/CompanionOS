package com.companionos.app

import android.util.Base64
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.FormBody
import okhttp3.OkHttpClient
import okhttp3.Request
import org.json.JSONObject
import java.util.concurrent.TimeUnit

object SpotifyWebApiClient {

    var clientId = "450a2e6ab1ed4ce4ac9738f654814240"
    var clientSecret = "78200536e7804c8ca427102a6ba2138f"

    private var accessToken: String? = null
    private var tokenExpiryTime: Long = 0L

    private val httpClient = OkHttpClient.Builder()
        .connectTimeout(5, TimeUnit.SECONDS)
        .readTimeout(10, TimeUnit.SECONDS)
        .build()

    suspend fun getAccessToken(): String? = withContext(Dispatchers.IO) {
        if (accessToken != null && System.currentTimeMillis() < tokenExpiryTime - 60000) {
            return@withContext accessToken
        }

        try {
            val authHeader = "Basic " + Base64.encodeToString(
                "$clientId:$clientSecret".toByteArray(Charsets.UTF_8),
                Base64.NO_WRAP
            )

            val body = FormBody.Builder()
                .add("grant_type", "client_credentials")
                .build()

            val request = Request.Builder()
                .url("https://accounts.spotify.com/api/token")
                .header("Authorization", authHeader)
                .post(body)
                .build()

            val response = httpClient.newCall(request).execute()
            if (response.isSuccessful) {
                val jsonStr = response.body?.string() ?: ""
                val obj = JSONObject(jsonStr)
                val token = obj.optString("access_token")
                val expiresIn = obj.optLong("expires_in", 3600L)
                if (token.isNotEmpty()) {
                    accessToken = token
                    tokenExpiryTime = System.currentTimeMillis() + (expiresIn * 1000L)
                    return@withContext token
                }
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
        return@withContext null
    }

    suspend fun searchTrackAlbumArtUrl(track: String, artist: String): String? = withContext(Dispatchers.IO) {
        try {
            val token = getAccessToken() ?: return@withContext null
            var cleanTrack = track.replace(Regex("""\(.*?\)|\[.*?\]|-.*"""), "").trim()
            if (cleanTrack.isEmpty()) cleanTrack = track.trim()
            var cleanArtist = artist.replace(Regex("""\(.*?\)|\[.*?\]|-.*"""), "").split(",")[0].trim()
            if (cleanArtist.isEmpty()) cleanArtist = artist.trim()

            val query = java.net.URLEncoder.encode("track:$cleanTrack artist:$cleanArtist", "UTF-8")
            val url = "https://api.spotify.com/v1/search?q=$query&type=track&limit=1"

            val request = Request.Builder()
                .url(url)
                .header("Authorization", "Bearer $token")
                .build()

            val response = httpClient.newCall(request).execute()
            if (response.isSuccessful) {
                val jsonStr = response.body?.string() ?: ""
                val obj = JSONObject(jsonStr)
                val tracks = obj.optJSONObject("tracks")
                val items = tracks?.optJSONArray("items")
                if (items != null && items.length() > 0) {
                    val firstTrack = items.getJSONObject(0)
                    val album = firstTrack.optJSONObject("album")
                    val images = album?.optJSONArray("images")
                    if (images != null && images.length() > 0) {
                        return@withContext images.getJSONObject(0).optString("url")
                    }
                }
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
        return@withContext null
    }
}
