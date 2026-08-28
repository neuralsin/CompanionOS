package com.companionos.app

import android.annotation.SuppressLint
import android.content.Context
import android.content.Intent
import android.graphics.BitmapFactory
import android.net.Uri
import android.net.wifi.WifiManager
import android.os.Build
import android.os.Bundle
import android.provider.Settings
import android.webkit.JavascriptInterface
import android.webkit.WebChromeClient
import android.webkit.WebSettings
import android.webkit.WebView
import android.webkit.WebViewClient
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat

class MainActivity : AppCompatActivity() {

    private lateinit var webView: WebView
    private var multicastLock: WifiManager.MulticastLock? = null

    private val pickImageLauncher = registerForActivityResult(ActivityResultContracts.GetContent()) { uri: Uri? ->
        if (uri != null) {
            try {
                val inputStream = contentResolver.openInputStream(uri)
                val bitmap = BitmapFactory.decodeStream(inputStream)
                inputStream?.close()
                if (bitmap != null) {
                    CompanionForegroundService.streamCustomPhoto(bitmap)
                    Toast.makeText(this, "Streaming custom photo to bot...", Toast.LENGTH_SHORT).show()
                }
            } catch (e: Exception) {
                Toast.makeText(this, "Failed to load photo: ${e.message}", Toast.LENGTH_SHORT).show()
            }
        }
    }

    @SuppressLint("SetJavaScriptEnabled")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        // Acquire Multicast lock for UDP discovery broadcasts
        val wifi = applicationContext.getSystemService(Context.WIFI_SERVICE) as WifiManager
        multicastLock = wifi.createMulticastLock("CompanionMulticastLock").apply {
            setReferenceCounted(true)
            acquire()
        }

        // Start 24/7 Companion Background Service
        startCompanionService()

        // Check if Notification Listener is enabled (needed for Spotify & Chat Relays)
        checkNotificationListenerPermission()

        // Setup WebView
        webView = findViewById(R.id.companion_webview)
        setupWebView()

        // Load offline bundled UI
        webView.loadUrl("file:///android_asset/www/index.html")
    }

    private fun checkNotificationListenerPermission() {
        try {
            val enabledListeners = Settings.Secure.getString(contentResolver, "enabled_notification_listeners")
            val pkg = packageName
            if (enabledListeners == null || !enabledListeners.contains(pkg)) {
                val intent = Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS)
                intent.flags = Intent.FLAG_ACTIVITY_NEW_TASK
                startActivity(intent)
                Toast.makeText(this, "Please enable CompanionOS Notification Access for Spotify & Notif Relays", Toast.LENGTH_LONG).show()
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    @SuppressLint("SetJavaScriptEnabled")
    private fun setupWebView() {
        val settings: WebSettings = webView.settings
        settings.javaScriptEnabled = true
        settings.domStorageEnabled = true
        settings.allowFileAccess = true
        settings.allowContentAccess = true
        settings.databaseEnabled = true
        settings.cacheMode = WebSettings.LOAD_DEFAULT
        settings.useWideViewPort = true
        settings.loadWithOverviewMode = true

        webView.webChromeClient = WebChromeClient()
        webView.webViewClient = object : WebViewClient() {
            override fun onPageFinished(view: WebView?, url: String?) {
                super.onPageFinished(view, url)
                val ip = CompanionForegroundService.discoveredEspIp ?: "192.168.1.42"
                webView.evaluateJavascript("if (window.onNativeReady) window.onNativeReady('$ip');", null)
            }
        }

        // Register Native JavaScript Bridge
        webView.addJavascriptInterface(CompanionNativeBridge(), "AndroidBridge")
    }

    private fun startCompanionService() {
        val serviceIntent = Intent(this, CompanionForegroundService::class.java)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            ContextCompat.startForegroundService(this, serviceIntent)
        } else {
            startService(serviceIntent)
        }
    }

    inner class CompanionNativeBridge {
        @JavascriptInterface
        fun sendUdp(message: String) {
            CompanionForegroundService.sendUdp(message)
        }

        @JavascriptInterface
        fun setWifiCredentials(ssid: String, pass: String) {
            CompanionForegroundService.saveAndSetWifi(ssid, pass)
            runOnUiThread {
                Toast.makeText(this@MainActivity, "Transmitting Wi-Fi to ESP...", Toast.LENGTH_SHORT).show()
            }
        }

        @JavascriptInterface
        fun pickAndUploadImage() {
            runOnUiThread {
                pickImageLauncher.launch("image/*")
            }
        }

        @JavascriptInterface
        fun setManualBotIp(ip: String) {
            val cleanIp = ip.trim()
            CompanionForegroundService.manualBotIp = if (cleanIp.isNotEmpty()) cleanIp else null
            getSharedPreferences("CompanionPrefs", Context.MODE_PRIVATE)
                .edit().putString("manual_bot_ip", cleanIp).apply()
            runOnUiThread {
                Toast.makeText(this@MainActivity, "Target Bot IP set to: $cleanIp", Toast.LENGTH_SHORT).show()
            }
        }

        @JavascriptInterface
        fun getManualBotIp(): String {
            val prefs = getSharedPreferences("CompanionPrefs", Context.MODE_PRIVATE)
            return prefs.getString("manual_bot_ip", "") ?: ""
        }

        @JavascriptInterface
        fun saveMultiWifi(jsonPayload: String) {
            getSharedPreferences("CompanionPrefs", Context.MODE_PRIVATE)
                .edit().putString("multi_wifi_json", jsonPayload).apply()
            CompanionForegroundService.saveAndSetMultiWifi(jsonPayload)
            runOnUiThread {
                Toast.makeText(this@MainActivity, "Syncing prioritized networks to Bot...", Toast.LENGTH_SHORT).show()
            }
        }

        @JavascriptInterface
        fun loadMultiWifi(): String {
            val prefs = getSharedPreferences("CompanionPrefs", Context.MODE_PRIVATE)
            return prefs.getString("multi_wifi_json", "") ?: ""
        }

        @JavascriptInterface
        fun saveApiCredentials(spotifyId: String, spotifySecret: String, githubUser: String, githubToken: String, weatherCity: String, weatherKey: String, stocks: String) {
            getSharedPreferences("CompanionPrefs", Context.MODE_PRIVATE).edit()
                .putString("spotify_id", spotifyId.trim())
                .putString("spotify_secret", spotifySecret.trim())
                .putString("github_user", githubUser.trim())
                .putString("github_token", githubToken.trim())
                .putString("weather_city", weatherCity.trim())
                .putString("weather_key", weatherKey.trim())
                .putString("stocks_list", stocks.trim())
                .apply()

            GitHubRelay.githubUsername = githubUser.trim()
            GitHubRelay.githubToken = githubToken.trim()
            WeatherRelay.defaultCity = if (weatherCity.trim().isNotEmpty()) weatherCity.trim() else "Delhi"

            CompanionForegroundService.triggerInstantRelays()

            runOnUiThread {
                Toast.makeText(this@MainActivity, "APIs Saved & Relays Triggered!", Toast.LENGTH_SHORT).show()
            }
        }

        @JavascriptInterface
        fun loadApiCredentials(): String {
            val prefs = getSharedPreferences("CompanionPrefs", Context.MODE_PRIVATE)
            val obj = org.json.JSONObject().apply {
                put("spotify_id", prefs.getString("spotify_id", "450a2e6ab1ed4ce4ac9738f654814240"))
                put("spotify_secret", prefs.getString("spotify_secret", "78200536e7804c8ca427102a6ba2138f"))
                put("github_user", prefs.getString("github_user", "neuralsin"))
                put("github_token", prefs.getString("github_token", "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"))
                put("weather_city", prefs.getString("weather_city", "Delhi"))
                put("weather_key", prefs.getString("weather_key", ""))
                put("stocks_list", prefs.getString("stocks_list", "BTC,ETH,NVDA,AAPL,TSLA"))
            }
            return obj.toString()
        }

        @JavascriptInterface
        fun exitApp() {
            runOnUiThread {
                Toast.makeText(this@MainActivity, "Shutting down CompanionOS...", Toast.LENGTH_SHORT).show()
                val stopIntent = Intent(this@MainActivity, CompanionForegroundService::class.java).apply {
                    action = CompanionForegroundService.ACTION_STOP_SERVICE
                }
                startService(stopIntent)
                finishAffinity()
                android.os.Process.killProcess(android.os.Process.myPid())
            }
        }

        @JavascriptInterface
        fun getDiscoveredIp(): String {
            return CompanionForegroundService.manualBotIp 
                ?: CompanionForegroundService.discoveredEspIp 
                ?: "Searching..."
        }

        @JavascriptInterface
        fun getBluetoothStatus(): String {
            return if (BluetoothDiscoveryManager.isBtConnected) {
                "Connected (${BluetoothDiscoveryManager.connectedDeviceName})"
            } else {
                "Searching..."
            }
        }

        @JavascriptInterface
        fun showToast(msg: String) {
            runOnUiThread {
                Toast.makeText(this@MainActivity, msg, Toast.LENGTH_SHORT).show()
            }
        }

        @JavascriptInterface
        fun openNotificationSettings() {
            startActivity(Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS))
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        multicastLock?.let {
            if (it.isHeld) it.release()
        }
    }
}
