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
import java.security.SecureRandom
import java.security.MessageDigest

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

    var clientId = ""
    var clientSecret = ""
    const val REDIRECT_URI = "companionos://callback"
    const val SCOPES = "user-read-playback-state user-modify-playback-state user-read-currently-playing"

    const val DEFAULT_REFRESH_TOKEN = ""
    const val DEFAULT_ACCESS_TOKEN = ""

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
        prefs = SecurePrefs.open(context, "spotify_auth_prefs")
        val config = SecurePrefs.open(context, "CompanionPrefs")
        clientId = config.getString("spotify_id", "") ?: ""
        clientSecret = config.getString("spotify_secret", "") ?: ""
        userAccessToken = prefs?.getString("access_token", DEFAULT_ACCESS_TOKEN) ?: DEFAULT_ACCESS_TOKEN
        userRefreshToken = prefs?.getString("refresh_token", DEFAULT_REFRESH_TOKEN) ?: DEFAULT_REFRESH_TOKEN
        userTokenExpiry = prefs?.getLong("token_expiry", 0L) ?: 0L
    }

    fun isUserAuthenticated(): Boolean {
        return !userRefreshToken.isNullOrEmpty()
    }

    fun getRefreshToken(): String? = userRefreshToken
    fun getCurrentAccessToken(): String? = userAccessToken

    @android.annotation.SuppressLint("ApplySharedPref") // Persist verifier before leaving the process for the browser.
    fun getAuthorizationUrl(): String {
        require(clientId.isNotBlank()) { "Save a Spotify client ID first" }
        fun randomKey(): String = Base64.encodeToString(ByteArray(48).also { SecureRandom().nextBytes(it) }, Base64.URL_SAFE or Base64.NO_WRAP or Base64.NO_PADDING)
        val verifier = randomKey()
        val state = randomKey()
        prefs?.edit()?.putString("oauth_verifier", verifier)?.putString("oauth_state", state)
            ?.putLong("oauth_started", System.currentTimeMillis())?.commit()
        val challenge = Base64.encodeToString(MessageDigest.getInstance("SHA-256").digest(verifier.toByteArray(Charsets.US_ASCII)), Base64.URL_SAFE or Base64.NO_WRAP or Base64.NO_PADDING)
        val encodedScopes = URLEncoder.encode(SCOPES, "UTF-8")
        val encodedRedirect = URLEncoder.encode(REDIRECT_URI, "UTF-8")
        return "https://accounts.spotify.com/authorize?client_id=$clientId&response_type=code&redirect_uri=$encodedRedirect&scope=$encodedScopes&state=$state&code_challenge_method=S256&code_challenge=$challenge"
    }

    suspend fun exchangeCodeForToken(code: String, state: String?): Boolean = withContext(Dispatchers.IO) {
        val expected = prefs?.getString("oauth_state", null)
        val verifier = prefs?.getString("oauth_verifier", null)
        val started = prefs?.getLong("oauth_started", 0) ?: 0
        if (state.isNullOrEmpty() || expected == null || verifier == null ||
            !MessageDigest.isEqual(state.toByteArray(), expected.toByteArray()) ||
            System.currentTimeMillis() - started !in 0..600000) return@withContext false
        prefs?.edit()?.remove("oauth_state")?.remove("oauth_verifier")?.apply()
        try {
            val body = FormBody.Builder()
                .add("grant_type", "authorization_code")
                .add("code", code)
                .add("redirect_uri", REDIRECT_URI)
                .add("client_id", clientId)
                .add("code_verifier", verifier)
                .build()

            val request = Request.Builder()
                .url("https://accounts.spotify.com/api/token")
                .post(body)
                .build()

            httpClient.newCall(request).execute().use { response ->
            if (response.isSuccessful) {
                val jsonStr = response.body?.string() ?: ""
                val obj = JSONObject(jsonStr)
                val access = obj.optString("access_token")
                val refresh = obj.optString("refresh_token", userRefreshToken ?: "")
                val expiresIn = obj.optLong("expires_in", 3600L)

                if (access.isNotEmpty()) {
                    // Public-client PKCE refresh tokens must not use a client secret.
                    clientSecret = ""
                    prefs?.edit()?.putBoolean("pkce", true)?.apply()
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
                .add("client_id", clientId)
                .build()

            val request = Request.Builder()
                .url("https://accounts.spotify.com/api/token")
                .apply { if (clientSecret.isNotEmpty() && prefs?.getBoolean("pkce", false) != true) header("Authorization", authHeader) }
                .post(body)
                .build()

            httpClient.newCall(request).execute().use { response ->
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

    fun configure(context: Context, id: String, secret: String, refresh: String, access: String) {
        val changedClient = id != clientId
        val edit = SecurePrefs.open(context, "CompanionPrefs").edit().putString("spotify_id", id)
        if (secret.isNotBlank() || changedClient) edit.putString("spotify_secret", secret)
        edit.apply()
        clientId = id
        if (secret.isNotBlank() || changedClient) clientSecret = secret
        if (changedClient || refresh.isNotBlank()) {
            userRefreshToken = refresh
            userAccessToken = access
            userTokenExpiry = 0
            prefs?.edit()?.putString("refresh_token", refresh)?.putString("access_token", access)
                ?.putLong("token_expiry", 0)?.putBoolean("pkce", clientSecret.isEmpty())?.apply()
        } else if (access.isNotBlank()) {
            userAccessToken = access
            userTokenExpiry = 0
            prefs?.edit()?.putString("access_token", access)?.putLong("token_expiry", 0)?.apply()
        }
    }

    fun provisioningPayload(): JSONObject = JSONObject().apply {
        put("access", userAccessToken ?: "")
        put("refresh", userRefreshToken ?: "")
        put("client_id", clientId)
        put("client_secret", if (prefs?.getBoolean("pkce", false) == true) "" else clientSecret)
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

            httpClient.newCall(request).execute().use { response ->
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

            httpClient.newCall(request).execute().use { response ->
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
                    images.getJSONObject(0).optString("url", "").takeIf { it.isNotEmpty() }
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

            httpClient.newCall(req).execute().use { response ->
            return@withContext response.isSuccessful || response.code in 200..204
            }
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

            httpClient.newCall(request).execute().use { response ->
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
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
        return@withContext null
    }
}
