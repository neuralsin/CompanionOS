package com.companionos.app

import android.Manifest
import android.annotation.SuppressLint
import android.content.Intent
import android.graphics.BitmapFactory
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.provider.Settings
import android.util.Base64
import android.webkit.*
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import kotlinx.coroutines.*
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.RequestBody.Companion.toRequestBody
import org.json.JSONObject
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean

class MainActivity : AppCompatActivity() {
    private lateinit var webView: WebView
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main)
    private val polling = AtomicBoolean(false)
    private val http = OkHttpClient.Builder().connectTimeout(2, TimeUnit.SECONDS)
        .readTimeout(3, TimeUnit.SECONDS).callTimeout(4, TimeUnit.SECONDS).followRedirects(false).build()
    private val prefs by lazy { SecurePrefs.open(this, "CompanionPrefs") }
    private val notifications = registerForActivityResult(ActivityResultContracts.RequestPermission()) {}
    private val pickPhoto = registerForActivityResult(ActivityResultContracts.GetContent()) { uri ->
        if (uri != null) scope.launch(Dispatchers.IO) {
            try {
                val options = BitmapFactory.Options().apply { inJustDecodeBounds = true }
                contentResolver.openInputStream(uri).use { BitmapFactory.decodeStream(it, null, options) }
                require(options.outWidth > 0 && options.outHeight > 0)
                options.inSampleSize = 1
                while (maxOf(options.outWidth, options.outHeight) / options.inSampleSize > 512) options.inSampleSize *= 2
                options.inJustDecodeBounds = false
                val bitmap = contentResolver.openInputStream(uri).use { BitmapFactory.decodeStream(it, null, options) }
                    ?: error("Cannot decode image")
                MediaArtProcessor.streamCustomEyeImage(bitmap)
                bitmap.recycle()
                message("Photo sent; waiting for bot verification")
            } catch (_: Exception) { message("Photo upload failed") }
        }
    }

    @SuppressLint("SetJavaScriptEnabled")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        SpotifyWebApiClient.init(this)
        CompanionForegroundService.manualBotIp = prefs.getString("manual_bot_ip", "")?.takeIf { it.isNotEmpty() }
        CompanionForegroundService.phoneRelays = prefs.getBoolean("phone_relays", false)
        webView = findViewById(R.id.companion_webview)
        ViewCompat.setOnApplyWindowInsetsListener(webView) { view, insets ->
            val bars = insets.getInsets(WindowInsetsCompat.Type.systemBars() or WindowInsetsCompat.Type.ime())
            view.setPadding(bars.left, bars.top, bars.right, bars.bottom)
            insets
        }
        webView.settings.apply {
            javaScriptEnabled = true
            domStorageEnabled = false
            allowFileAccess = false
            allowContentAccess = false
            mixedContentMode = WebSettings.MIXED_CONTENT_NEVER_ALLOW
        }
        webView.webViewClient = object : WebViewClient() {
            override fun shouldOverrideUrlLoading(view: WebView?, request: WebResourceRequest?): Boolean = true
            @Deprecated("Legacy WebView callback")
            override fun shouldOverrideUrlLoading(view: WebView?, url: String?): Boolean = true
        }
        webView.webChromeClient = object : WebChromeClient() {
            override fun onJsConfirm(view: WebView?, url: String?, message: String?, result: JsResult): Boolean {
                androidx.appcompat.app.AlertDialog.Builder(this@MainActivity).setTitle("CompanionOS")
                    .setMessage(message).setPositiveButton("Continue") { _, _ -> result.confirm() }
                    .setNegativeButton("Cancel") { _, _ -> result.cancel() }
                    .setOnCancelListener { result.cancel() }.show()
                return true
            }
        }
        webView.addJavascriptInterface(Bridge(), "AndroidBridge")
        webView.loadUrl("file:///android_asset/www/index.html")
        ContextCompat.startForegroundService(this, Intent(this, CompanionForegroundService::class.java))
        if (Build.VERSION.SDK_INT >= 33) notifications.launch(Manifest.permission.POST_NOTIFICATIONS)
        handleOAuth(intent)
    }

    override fun onNewIntent(intent: Intent) { super.onNewIntent(intent); setIntent(intent); handleOAuth(intent) }
    private fun handleOAuth(intent: Intent?) {
        val uri = intent?.data ?: return
        if (uri.scheme != "companionos" || uri.host != "callback") return
        val code = uri.getQueryParameter("code") ?: return
        intent.data = null
        scope.launch {
            val ok = SpotifyWebApiClient.exchangeCodeForToken(code, uri.getQueryParameter("state"))
            message(if (ok) "Spotify authorized. Tap Sync to bot to provision it." else "Authorization failed or expired. Try connecting again.")
            emit("onSettings", settings())
        }
    }
    private fun emit(callback: String, data: JSONObject) {
        runOnUiThread { if (!isDestroyed) webView.evaluateJavascript("window.$callback && window.$callback($data)", null) }
    }
    private fun message(text: String) { emit("onResult", JSONObject().put("message", text)) }
    private fun settings(): JSONObject = JSONObject().apply {
        for (key in listOf("spotify_id", "github_user", "weather_city", "stocks_list", "manual_bot_ip")) put(key, prefs.getString(key, ""))
        put("spotify_saved", SpotifyWebApiClient.isUserAuthenticated())
        put("phone_relays", CompanionForegroundService.phoneRelays)
        put("networks", prefs.getString("multi_wifi_json", "{\"networks\":[]}"))
    }

    inner class Bridge {
        @JavascriptInterface fun loadPlannerDraft(): String = prefs.getString("planner_draft", "") ?: ""
        @JavascriptInterface fun savePlannerDraft(json: String) {
            try {
                require(json.toByteArray(Charsets.UTF_8).size <= 6000)
                val value = JSONObject(json)
                require(value.getJSONArray("events").length() <= 12 && value.getJSONArray("classes").length() <= 16)
                prefs.edit().putString("planner_draft", json).apply()
            } catch (_: Exception) { message("Planner is too large or invalid") }
        }
        @JavascriptInterface fun loadPlanner() { plannerRequest(null) }
        @JavascriptInterface fun syncPlanner(json: String) {
            savePlannerDraft(json)
            if (json.toByteArray(Charsets.UTF_8).size > 6000) return
            plannerRequest(json)
        }
        @JavascriptInterface fun retryBoot() {
            scope.launch(Dispatchers.IO) {
                try {
                    val ip = CompanionForegroundService.manualBotIp ?: CompanionForegroundService.discoveredEspIp
                    require(ip != null && BotProtocol.isPrivateIpv4(ip))
                    http.newCall(Request.Builder().url("http://$ip:8080/api/recovery")
                        .post("".toRequestBody()).build()).execute().use { require(it.isSuccessful) }
                    message("Bot restarting")
                } catch (_: Exception) { message("Restart not confirmed; hold Select on the bot to retry") }
            }
        }
        @JavascriptInterface fun loadSettings(): String = settings().toString()
        @JavascriptInterface fun setManualBotIp(value: String) {
            val ip = value.trim()
            if (ip.isNotEmpty() && !BotProtocol.isPrivateIpv4(ip)) { message("Enter a private LAN IPv4 address"); return }
            prefs.edit().putString("manual_bot_ip", ip).apply()
            CompanionForegroundService.manualBotIp = ip.ifEmpty { null }
            CompanionForegroundService.discoveredEspIp = null
            CompanionForegroundService.lastBotSeen = 0
            message("Target saved; checking connection")
        }
        @JavascriptInterface fun sendUdp(command: String) {
            if (!BotProtocol.isAllowedCommand(command)) { message("Unsupported command"); return }
            if (!CompanionForegroundService.isBotOnline()) { message("Bot offline; command not sent"); return }
            CompanionForegroundService.sendUdp(command)
        }
        @JavascriptInterface fun startSpotifyAuth() { runOnUiThread {
            try { startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(SpotifyWebApiClient.getAuthorizationUrl()))) }
            catch (e: Exception) { message(e.message ?: "Browser unavailable") }
        } }
        @JavascriptInterface fun saveSettings(json: String) {
            try {
                val data = JSONObject(json)
                val id = data.optString("spotify_id").trim()
                require(id.isEmpty() || id.matches(Regex("[A-Za-z0-9]{16,128}")))
                SpotifyWebApiClient.configure(this@MainActivity, id, data.optString("spotify_secret").trim(),
                    data.optString("spotify_refresh").trim(), data.optString("spotify_access").trim())
                val edit = prefs.edit()
                for (key in listOf("github_user", "weather_city", "stocks_list")) if (data.has(key)) edit.putString(key, data.optString(key).trim().take(256))
                val token = data.optString("github_token").trim()
                if (token.isNotEmpty()) edit.putString("github_token", token)
                CompanionForegroundService.phoneRelays = data.optBoolean("phone_relays", false)
                edit.putBoolean("phone_relays", CompanionForegroundService.phoneRelays).apply()
                if (data.has("github_user")) GitHubRelay.githubUsername = data.optString("github_user")
                GitHubRelay.githubToken = prefs.getString("github_token", "") ?: ""
                WeatherRelay.defaultCity = data.optString("weather_city").ifBlank { "Delhi" }
                StockRelay.setSymbols(data.optString("stocks_list"))
                message("Saved on phone")
                emit("onSettings", settings())
            } catch (_: Exception) { message("Invalid settings; check the fields") }
        }
        @JavascriptInterface fun syncSpotify() {
            if (!CompanionForegroundService.isBotOnline()) { message("Connect to the bot before syncing credentials"); return }
            if (!SpotifyWebApiClient.isUserAuthenticated()) { message("Authorize Spotify or enter a refresh token first"); return }
            scope.launch(Dispatchers.IO) {
                SpotifyWebApiClient.getUserAccessToken()
                val payload = "SPOTIFY_AUTH:" + SpotifyWebApiClient.provisioningPayload()
                if (payload.toByteArray(Charsets.UTF_8).size > 1900) { message("Credentials exceed the bot packet limit"); return@launch }
                val before = CompanionForegroundService.authAckAt
                CompanionForegroundService.sendUdpDirect(payload)
                repeat(30) {
                    delay(100)
                    if (CompanionForegroundService.authAckAt > before) {
                        message("Bot acknowledged Spotify credentials"); return@launch
                    }
                }
                message("No bot acknowledgement. Credentials remain saved on this phone.")
            }
        }
        @JavascriptInterface fun saveWifi(json: String) {
            try {
                require(CompanionForegroundService.isBotOnline())
                val list = JSONObject(json).getJSONArray("networks")
                require(list.length() in 1..5)
                for (i in 0 until list.length()) {
                    val network = list.getJSONObject(i)
                    require(network.getString("ssid").toByteArray(Charsets.UTF_8).size in 1..32)
                    require(network.getString("pass").toByteArray(Charsets.UTF_8).size <= 63)
                }
                require(json.toByteArray(Charsets.UTF_8).size <= 900)
                prefs.edit().putString("multi_wifi_json", json).apply()
                CompanionForegroundService.sendUdp("WIFI:MULTI_SET:$json")
                message("Networks sent. The bot may reconnect.")
            } catch (_: Exception) { message("Connect to the bot and check network names and passwords") }
        }
        @JavascriptInterface fun pollBot(capture: Boolean) {
            if (!polling.compareAndSet(false, true)) return
            scope.launch(Dispatchers.IO) {
                val state = JSONObject()
                try {
                    val ip = CompanionForegroundService.manualBotIp ?: CompanionForegroundService.discoveredEspIp
                    require(ip != null && BotProtocol.isPrivateIpv4(ip))
                    state.put("ip", ip)
                    http.newCall(Request.Builder().url("http://$ip:8080/api/status").build()).execute().use { response ->
                        if (response.code == 404) {
                            state.put("legacy", true).put("online", CompanionForegroundService.isBotOnline())
                                .put("page", CompanionForegroundService.currentBotPage)
                        } else {
                            require(response.isSuccessful)
                            val status = JSONObject(response.body?.string() ?: "{}")
                            require(status.optInt("protocol") == 1)
                            status.keys().forEach { key -> state.put(key, status.get(key)) }
                            CompanionForegroundService.lastBotSeen = android.os.SystemClock.elapsedRealtime()
                            CompanionForegroundService.currentBotPage = status.getInt("page")
                            state.put("online", true)
                            if (capture) http.newCall(Request.Builder().url("http://$ip:8080/api/screen.bmp").build()).execute().use { screen ->
                                if (screen.isSuccessful) {
                                    val bytes = screen.body?.bytes() ?: error("Empty screen")
                                    require(bytes.size in 54..(54 + 480 * 320 * 3))
                                    state.put("screen", "data:image/bmp;base64," + Base64.encodeToString(bytes, Base64.NO_WRAP))
                                } else state.put("screen_error", if (screen.code == 501) "TFT readback unavailable. Check MISO wiring." else "Capture paused; retrying")
                            }
                            Unit
                        }
                    }
                } catch (_: Exception) {
                    state.put("online", CompanionForegroundService.isBotOnline())
                        .put("page", CompanionForegroundService.currentBotPage)
                        .put("screen_error", "Screen endpoint unavailable; updated firmware required")
                } finally { polling.set(false) }
                emit("onBotState", state)
            }
        }
        @JavascriptInterface fun pickAndUploadImage() { runOnUiThread { pickPhoto.launch("image/*") } }
        @JavascriptInterface fun openNotificationSettings() { runOnUiThread { startActivity(Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS)) } }
        @JavascriptInterface fun openBatterySettings() { runOnUiThread { startActivity(Intent(Settings.ACTION_IGNORE_BATTERY_OPTIMIZATION_SETTINGS)) } }
        @JavascriptInterface fun exitApp() { runOnUiThread {
            stopService(Intent(this@MainActivity, CompanionForegroundService::class.java)); finishAffinity()
        } }
    }
    private val plannerBusy = java.util.concurrent.atomic.AtomicBoolean(false)
    private fun plannerRequest(json: String?) {
        if (!plannerBusy.compareAndSet(false, true)) { message("Planner sync already in progress"); return }
        scope.launch(Dispatchers.IO) {
            try {
                val ip = CompanionForegroundService.manualBotIp ?: CompanionForegroundService.discoveredEspIp
                require(ip != null && BotProtocol.isPrivateIpv4(ip))
                val url = "http://$ip:8080/api/planner"
                if (json != null) {
                    http.newCall(Request.Builder().url(url).post(json.toRequestBody("application/json".toMediaType())).build()).execute().use { response ->
                        if (!response.isSuccessful) {
                            val error = when (response.code) {
                                409 -> "Bot schedule changed. Reload from bot before syncing."
                                404 -> "Planner requires the updated deskbot firmware."
                                503 -> "Bot busy with Spotify. Draft saved; retry shortly."
                                else -> "Bot rejected schedule (${response.code}). Draft kept on phone."
                            }
                            emit("onPlannerError", JSONObject().put("message", error)); return@launch
                        }
                        val saved = JSONObject(response.body?.string() ?: "{}")
                        require(saved.optInt("version") == 1)
                        prefs.edit().putString("planner_draft", saved.toString()).apply()
                        emit("onPlanner", saved)
                        message("Planner saved on bot")
                        return@launch
                    }
                }
                http.newCall(Request.Builder().url(url).build()).execute().use { response ->
                    require(response.isSuccessful)
                    val value = JSONObject(response.body?.string() ?: "{}")
                    require(value.optInt("version") == 1)
                    prefs.edit().putString("planner_draft", value.toString()).apply()
                    emit("onPlanner", value)
                    message(if (json == null) "Loaded from bot" else "Planner saved on bot")
                }
            } catch (_: Exception) {
                emit("onPlannerError", JSONObject().put("message", "Planner not confirmed. Draft kept on phone; check bot connection and firmware."))
            } finally { plannerBusy.set(false) }
        }
    }
    override fun onDestroy() {
        scope.cancel(); webView.removeJavascriptInterface("AndroidBridge"); webView.destroy(); super.onDestroy()
    }
}
