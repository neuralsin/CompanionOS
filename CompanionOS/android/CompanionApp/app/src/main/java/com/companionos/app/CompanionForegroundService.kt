package com.companionos.app

import android.app.*
import android.content.Context
import android.content.Intent
import android.net.wifi.WifiManager
import android.os.IBinder
import android.os.SystemClock
import androidx.core.app.NotificationCompat
import kotlinx.coroutines.*
import java.net.*

class CompanionForegroundService : Service() {
    private val job = SupervisorJob()
    private val scope = CoroutineScope(Dispatchers.IO + job)
    private var listener: DatagramSocket? = null
    private var multicast: WifiManager.MulticastLock? = null

    companion object {
        const val ESP_PORT = 8888
        const val LOCAL_PORT = 8889
        const val ACTION_STOP_SERVICE = "com.companionos.app.ACTION_STOP_SERVICE"
        @Volatile var discoveredEspIp: String? = null
        @Volatile var manualBotIp: String? = null
        @Volatile var lastBotSeen = 0L
        @Volatile var currentBotPage = -1
        @Volatile var authAckAt = 0L
        @Volatile var phoneRelays = false
        private var activeScope: CoroutineScope? = null
        private var sender: DatagramSocket? = null
        private val socketLock = Any()

        fun isBotOnline(): Boolean = lastBotSeen != 0L && SystemClock.elapsedRealtime() - lastBotSeen < 20000
        fun sendUdp(message: String) { activeScope?.launch { sendUdpDirect(message) } }
        fun sendUdpDirect(message: String) {
            // Native Spotify and offline thoughts own the bot unless phone media is explicitly enabled.
            if (!phoneRelays && listOf("SPOTIFY:", "STATE:", "TRACK:", "ARTIST:", "PROGRESS:", "LYRICS:").any { message.startsWith(it) }) return
            sendUdpBytesDirect(message.toByteArray(Charsets.UTF_8))
        }
        fun sendUdpBytesDirect(bytes: ByteArray) {
            if (bytes.size > 2048) return
            synchronized(socketLock) {
                try {
                    val ip = manualBotIp ?: discoveredEspIp ?: return
                    if (!BotProtocol.isPrivateIpv4(ip)) return
                    if (sender == null || sender!!.isClosed) sender = DatagramSocket()
                    sender!!.send(DatagramPacket(bytes, bytes.size, InetAddress.getByName(ip), ESP_PORT))
                } catch (_: Exception) { /* Connection status expires via the heartbeat, never fabricate success. */ }
            }
        }
        fun triggerInstantRelays() { activeScope?.launch { TimeRelay.syncDeviceTime() } }
    }

    override fun onCreate() {
        super.onCreate()
        activeScope = scope
        val prefs = SecurePrefs.open(this, "CompanionPrefs")
        manualBotIp = prefs.getString("manual_bot_ip", "")?.takeIf { it.isNotEmpty() }
        phoneRelays = prefs.getBoolean("phone_relays", false)
        SpotifyWebApiClient.init(this)
        GitHubRelay.githubUsername = prefs.getString("github_user", "") ?: ""
        GitHubRelay.githubToken = prefs.getString("github_token", "") ?: ""
        WeatherRelay.defaultCity = prefs.getString("weather_city", "Delhi").orEmpty().ifBlank { "Delhi" }
        StockRelay.setSymbols(prefs.getString("stocks_list", "BTC,ETH,SOL").orEmpty())
        val manager = getSystemService(NotificationManager::class.java)
        manager.createNotificationChannel(NotificationChannel("bot", "Deskbot connection", NotificationManager.IMPORTANCE_LOW))
        val open = PendingIntent.getActivity(this, 0, Intent(this, MainActivity::class.java), PendingIntent.FLAG_IMMUTABLE)
        val stop = PendingIntent.getService(this, 1, Intent(this, javaClass).setAction(ACTION_STOP_SERVICE), PendingIntent.FLAG_IMMUTABLE)
        startForeground(1001, NotificationCompat.Builder(this, "bot").setSmallIcon(R.mipmap.ic_launcher)
            .setContentTitle("CompanionOS").setContentText("WiFi controller active").setContentIntent(open)
            .addAction(0, "Disconnect", stop).setOngoing(true).build())
        val wifi = applicationContext.getSystemService(Context.WIFI_SERVICE) as WifiManager
        multicast = wifi.createMulticastLock("companion.discovery").apply { setReferenceCounted(false); acquire() }
        scope.launch { receiveLoop() }
        scope.launch {
            while (isActive) {
                if (!isBotOnline()) { discoveredEspIp = null; currentBotPage = -1 }
                probe()
                delay(5000)
            }
        }
        scope.launch {
            while (isActive) {
                if (isBotOnline()) TimeRelay.syncDeviceTime()
                delay(60000)
            }
        }
        scope.launch {
            while (isActive) {
                if (phoneRelays && isBotOnline()) when (currentBotPage) {
                    4 -> WeatherRelay.fetchAndSendWeather()
                    7 -> StockRelay.fetchAndSendStocks()
                }
                delay(60000)
            }
        }
    }

    private fun probe() {
        val target = manualBotIp ?: discoveredEspIp
        if (target != null) { sendUdpDirect("COMPANION_PROBE:ANDROID"); return }
        try {
            DatagramSocket().use { socket ->
                socket.broadcast = true
                val data = "COMPANION_PROBE:ANDROID".toByteArray()
                val addresses = mutableSetOf("255.255.255.255")
                NetworkInterface.getNetworkInterfaces()?.toList()?.filter { it.isUp && !it.isLoopback }?.forEach { iface ->
                    iface.interfaceAddresses.mapNotNullTo(addresses) { it.broadcast?.hostAddress }
                }
                for (address in addresses) try { socket.send(DatagramPacket(data, data.size, InetAddress.getByName(address), ESP_PORT)) }
                catch (_: Exception) {}
            }
        } catch (_: Exception) {}
    }

    private suspend fun receiveLoop() {
        try {
            DatagramSocket(LOCAL_PORT).use { socket ->
                listener = socket
                socket.soTimeout = 1500
                val bytes = ByteArray(2048)
                while (scope.isActive) {
                    try {
                        val packet = DatagramPacket(bytes, bytes.size)
                        socket.receive(packet)
                        val ip = packet.address.hostAddress ?: continue
                        if (!BotProtocol.isPrivateIpv4(ip) || (manualBotIp != null && manualBotIp != ip)) continue
                        val msg = String(packet.data, 0, packet.length, Charsets.UTF_8).trim()
                        if (msg == "HELLO_COMPANION") {
                            if (discoveredEspIp != null && discoveredEspIp != ip && isBotOnline()) continue
                            discoveredEspIp = ip; lastBotSeen = SystemClock.elapsedRealtime()
                        }
                        if (ip != (manualBotIp ?: discoveredEspIp)) continue
                        if (msg.startsWith("VIEW:")) {
                            val page = msg.substringAfter(':').toIntOrNull()
                            if (page != null && page in 0 until BotProtocol.PAGE_COUNT) {
                                currentBotPage = page; lastBotSeen = SystemClock.elapsedRealtime()
                            }
                        } else if (msg == "SPOTIFY_AUTH_OK") authAckAt = SystemClock.elapsedRealtime()
                        else if (msg.startsWith("CUSTEYE_REQ:")) {
                            val rows = msg.substringAfter(':').split(',').mapNotNull { it.toIntOrNull()?.takeIf { row -> row in 0..127 } }
                            scope.launch { MediaArtProcessor.retransmitCustomEyeRows(rows) }
                        } else if (phoneRelays && msg.startsWith("ART_REQ:")) {
                            val rows = msg.substringAfter(':').split(',').mapNotNull { it.toIntOrNull()?.takeIf { row -> row in 0..127 } }
                            scope.launch { MediaArtProcessor.retransmitAlbumArtChunks(rows) }
                        } else if (phoneRelays && msg in listOf("PLAY_PAUSE", "NEXT", "PREV")) {
                            CompanionNotificationListenerService.instance?.handleMediaAction(msg)
                        }
                    } catch (_: SocketTimeoutException) {} catch (_: SocketException) { break }
                }
            }
        } catch (_: Exception) { stopSelf() }
    }
    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == ACTION_STOP_SERVICE) { stopSelf(); return START_NOT_STICKY }
        return START_STICKY
    }
    override fun onBind(intent: Intent?): IBinder? = null
    override fun onDestroy() {
        activeScope = null; job.cancel(); listener?.close()
        synchronized(socketLock) { sender?.close(); sender = null }
        multicast?.let { if (it.isHeld) it.release() }
        lastBotSeen = 0; discoveredEspIp = null
        super.onDestroy()
    }
}
