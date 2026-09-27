package com.companionos.app

import android.content.Context
import android.content.SharedPreferences
import android.util.Base64
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.FormBody
import okhttp3.OkHttpClient
import okhttp3.Request
import org.json.JSONObject
import java.net.URLEncoder
import java.util.concurrent.TimeUnit

data class SpotifyPlaybackInfo(
    val trackId: String,
    val title: String,
    val artist: String,
    val album: String,
    val durationMs: Long,
    val progressMs: Long,
    val isPlaying: Boolean,
    val albumArtUrl: String?
)

object SpotifyWebApiClient {

    var clientId = "450a2e6ab1ed4ce4ac9738f654814240"
    var clientSecret = "78200536e7804c8ca427102a6ba2138f"
    const val REDIRECT_URI = "companionos://callback"
    const val SCOPES = "user-read-playback-state user-modify-playback-state user-read-currently-playing"

    const val DEFAULT_REFRESH_TOKEN = "AQCDzzl6TnG-FpdJKuWddUHoX_wTx2jA1mMHJR3okGYjllb_DnifVtaplec0Do9EJWTZippa7R5DCejRbR_eJgexej9BEmsFbCdE3o6zwgXCs4tXHOth3eOT6NksbNi1lSs"
    const val DEFAULT_ACCESS_TOKEN = "BQCNa_jBDQmPUroD1WVTgAyt9Eq60wJ7tXGqgElBSDFZywXo11mKCBQ7NcpsK8QT4jIJzlQNPjMi8Lgno601-FU_9bD1o6KMa9REzSVxqqXtLMSoiYNKju3jBPH-5vg-sC8vhTakaClrVipZjpzT6d0cw4mGvqid7LFqOH_rfJwL-2K7f99CQvYL5dKUhcM85adLeBXLdCjhng8A27oI0PyC0VsUVa2x9LEKRkZguylaiOKqqwGFq4tG-oU2gQxmrU7RJOhNHVJh8TdkDF2tZUCwxVdQ7w"

    private var userAccessToken: String? = null
    private var userRefreshToken: String? = null
    private var userTokenExpiry: Long = 0L

    private var clientCredsToken: String? = null
    private var clientCredsExpiry: Long = 0L

    private val httpClient = OkHttpClient.Builder()
        .connectTimeout(5, TimeUnit.SECONDS)
        .readTimeout(8, TimeUnit.SECONDS)
        .build()

    private var prefs: SharedPreferences? = null

    fun init(context: Context) {
        prefs = context.getSharedPreferences("spotify_auth_prefs", Context.MODE_PRIVATE)
        userAccessToken = prefs?.getString("access_token", DEFAULT_ACCESS_TOKEN) ?: DEFAULT_ACCESS_TOKEN
        userRefreshToken = prefs?.getString("refresh_token", DEFAULT_REFRESH_TOKEN) ?: DEFAULT_REFRESH_TOKEN
        userTokenExpiry = prefs?.getLong("token_expiry", 0L) ?: 0L
    }

    fun isUserAuthenticated(): Boolean {
        return !userRefreshToken.isNullOrEmpty()
    }

    fun getRefreshToken(): String? = userRefreshToken
    fun getCurrentAccessToken(): String? = userAccessToken

    fun getAuthorizationUrl(): String {
        val encodedScopes = URLEncoder.encode(SCOPES, "UTF-8")
        val encodedRedirect = URLEncoder.encode(REDIRECT_URI, "UTF-8")
        return "https://accounts.spotify.com/authorize?client_id=$clientId&response_type=code&redirect_uri=$encodedRedirect&scope=$encodedScopes"
    }

    suspend fun exchangeCodeForToken(code: String): Boolean = withContext(Dispatchers.IO) {
        try {
            val authHeader = "Basic " + Base64.encodeToString(
                "$clientId:$clientSecret".toByteArray(Charsets.UTF_8),
                Base64.NO_WRAP
            )

            val body = FormBody.Builder()
                .add("grant_type", "authorization_code")
                .add("code", code)
                .add("redirect_uri", REDIRECT_URI)
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
                val access = obj.optString("access_token")
                val refresh = obj.optString("refresh_token", userRefreshToken ?: "")
                val expiresIn = obj.optLong("expires_in", 3600L)

                if (access.isNotEmpty()) {
                    userAccessToken = access
                    userRefreshToken = refresh
                    userTokenExpiry = System.currentTimeMillis() + (expiresIn * 1000L)

                    prefs?.edit()
                        ?.putString("access_token", access)
                        ?.putString("refresh_token", refresh)
                        ?.putLong("token_expiry", userTokenExpiry)
                        ?.apply()

                    return@withContext true
                }
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
        return@withContext false
    }

    private suspend fun refreshUserToken(): String? = withContext(Dispatchers.IO) {
        val refresh = userRefreshToken ?: return@withContext null
        try {
            val authHeader = "Basic " + Base64.encodeToString(
                "$clientId:$clientSecret".toByteArray(Charsets.UTF_8),
                Base64.NO_WRAP
            )

            val body = FormBody.Builder()
                .add("grant_type", "refresh_token")
                .add("refresh_token", refresh)
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
                val access = obj.optString("access_token")
                val newRefresh = obj.optString("refresh_token", refresh)
                val expiresIn = obj.optLong("expires_in", 3600L)

                if (access.isNotEmpty()) {
                    userAccessToken = access
                    userRefreshToken = newRefresh
                    userTokenExpiry = System.currentTimeMillis() + (expiresIn * 1000L)

                    prefs?.edit()
                        ?.putString("access_token", access)
                        ?.putString("refresh_token", newRefresh)
                        ?.putLong("token_expiry", userTokenExpiry)
                        ?.apply()

                    return@withContext access
                }
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
        return@withContext null
    }

    suspend fun getUserAccessToken(): String? {
        if (!userAccessToken.isNullOrEmpty() && System.currentTimeMillis() < userTokenExpiry - 60000) {
            return userAccessToken
        }
        return refreshUserToken()
    }

    suspend fun getClientCredentialsToken(): String? = withContext(Dispatchers.IO) {
        if (clientCredsToken != null && System.currentTimeMillis() < clientCredsExpiry - 60000) {
            return@withContext clientCredsToken
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
                    clientCredsToken = token
                    clientCredsExpiry = System.currentTimeMillis() + (expiresIn * 1000L)
                    return@withContext token
                }
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
        return@withContext null
    }

    suspend fun fetchCurrentPlayback(): SpotifyPlaybackInfo? = withContext(Dispatchers.IO) {
        val token = getUserAccessToken() ?: return@withContext null
        try {
            val request = Request.Builder()
                .url("https://api.spotify.com/v1/me/player")
                .header("Authorization", "Bearer $token")
                .build()

            val response = httpClient.newCall(request).execute()
            if (response.code == 200) {
                val bodyStr = response.body?.string() ?: return@withContext null
                val root = JSONObject(bodyStr)
                val isPlaying = root.optBoolean("is_playing", false)
                val progressMs = root.optLong("progress_ms", 0L)
                val item = root.optJSONObject("item") ?: return@withContext null

                val trackId = item.optString("id", "")
                val title = item.optString("name", "")
                val durationMs = item.optLong("duration_ms", 0L)

                val artistsArr = item.optJSONArray("artists")
                val artist = if (artistsArr != null && artistsArr.length() > 0) {
                    artistsArr.getJSONObject(0).optString("name", "")
                } else ""

                val albumObj = item.optJSONObject("album")
                val album = albumObj?.optString("name", "") ?: ""
                val images = albumObj?.optJSONArray("images")
                val artUrl = if (images != null && images.length() > 0) {
                    images.getJSONObject(0).optString("url", null)
                } else null

                return@withContext SpotifyPlaybackInfo(
                    trackId = trackId,
                    title = title,
                    artist = artist,
                    album = album,
                    durationMs = durationMs,
                    progressMs = progressMs,
                    isPlaying = isPlaying,
                    albumArtUrl = artUrl
                )
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
        return@withContext null
    }

    suspend fun controlPlayback(command: String): Boolean = withContext(Dispatchers.IO) {
        val token = getUserAccessToken() ?: return@withContext false
        try {
            val (url, method) = when (command.uppercase()) {
                "PLAY" -> Pair("https://api.spotify.com/v1/me/player/play", "PUT")
                "PAUSE" -> Pair("https://api.spotify.com/v1/me/player/pause", "PUT")
                "PLAY_PAUSE", "TOGGLE" -> {
                    val cur = fetchCurrentPlayback()
                    if (cur?.isPlaying == true) Pair("https://api.spotify.com/v1/me/player/pause", "PUT")
                    else Pair("https://api.spotify.com/v1/me/player/play", "PUT")
                }
                "NEXT", "SKIP_NEXT" -> Pair("https://api.spotify.com/v1/me/player/next", "POST")
                "PREVIOUS", "SKIP_PREV" -> Pair("https://api.spotify.com/v1/me/player/previous", "POST")
                else -> return@withContext false
            }

            val emptyBody = okhttp3.RequestBody.create(null, ByteArray(0))
            val reqBuilder = Request.Builder().url(url).header("Authorization", "Bearer $token")
            val req = if (method == "PUT") reqBuilder.put(emptyBody).build() else reqBuilder.post(emptyBody).build()

            val response = httpClient.newCall(req).execute()
            return@withContext response.isSuccessful || response.code in 200..204
        } catch (e: Exception) {
            e.printStackTrace()
        }
        return@withContext false
    }

    suspend fun searchTrackAlbumArtUrl(track: String, artist: String): String? = withContext(Dispatchers.IO) {
        try {
            val token = getUserAccessToken() ?: getClientCredentialsToken() ?: return@withContext null
            var cleanTrack = track.replace(Regex("""\(.*?\)|\[.*?\]|-.*"""), "").trim()
            if (cleanTrack.isEmpty()) cleanTrack = track.trim()
            var cleanArtist = artist.replace(Regex("""\(.*?\)|\[.*?\]|-.*"""), "").split(",")[0].trim()
            if (cleanArtist.isEmpty()) cleanArtist = artist.trim()

            val query = URLEncoder.encode("track:$cleanTrack artist:$cleanArtist", "UTF-8")
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
