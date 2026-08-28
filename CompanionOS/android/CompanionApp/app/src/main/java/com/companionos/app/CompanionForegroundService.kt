package com.companionos.app

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.graphics.Bitmap
import android.net.wifi.WifiManager
import android.os.Build
import android.os.IBinder
import android.os.PowerManager
import androidx.core.app.NotificationCompat
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import org.json.JSONObject
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.net.NetworkInterface

class CompanionForegroundService : Service() {

    private val serviceJob = Job()
    private val serviceScope = CoroutineScope(Dispatchers.IO + serviceJob)
    private var wakeLock: PowerManager.WakeLock? = null
    private var multicastLock: WifiManager.MulticastLock? = null

    companion object {
        const val CHANNEL_ID = "CompanionOS_Service_Channel"
        const val NOTIFICATION_ID = 1001
        const val ACTION_STOP_SERVICE = "com.companionos.app.ACTION_STOP_SERVICE"
        const val ESP_PORT = 8888
        const val LOCAL_PORT = 8889

        var discoveredEspIp: String? = null
        var manualBotIp: String? = null
        private var udpSocket: DatagramSocket? = null

        fun sendUdp(message: String) {
            CoroutineScope(Dispatchers.IO).launch {
                val data = message.toByteArray(Charsets.UTF_8)
                if (udpSocket == null || udpSocket!!.isClosed) {
                    udpSocket = DatagramSocket().apply { broadcast = true }
                }

                // 1. Direct Unicast if known
                val directIp = manualBotIp ?: discoveredEspIp
                if (directIp != null) {
                    try {
                        val packet = DatagramPacket(
                            data,
                            data.size,
                            InetAddress.getByName(directIp),
                            ESP_PORT
                        )
                        udpSocket?.send(packet)
                    } catch (e: Exception) {
                        e.printStackTrace()
                    }
                }

                // 2. Subnet Directed Broadcasts (guaranteed to pass through mobile Wi-Fi & Hotspots)
                try {
                    val interfaces = NetworkInterface.getNetworkInterfaces()
                    while (interfaces.hasMoreElements()) {
                        val iface = interfaces.nextElement()
                        if (iface.isLoopback || !iface.isUp) continue
                        for (addr in iface.interfaceAddresses) {
                            val bcast = addr.broadcast
                            if (bcast != null) {
                                try {
                                    val packet = DatagramPacket(data, data.size, bcast, ESP_PORT)
                                    udpSocket?.send(packet)
                                } catch (e: Exception) {}
                            }
                        }
                    }
                } catch (e: Exception) {}

                // 3. Global Broadcast Fallback
                try {
                    val bcast = DatagramPacket(
                        data,
                        data.size,
                        InetAddress.getByName("255.255.255.255"),
                        ESP_PORT
                    )
                    udpSocket?.send(bcast)
                } catch (e: Exception) {}

                // 4. Dual-Transport: also send over Bluetooth if connected
                if (BluetoothDiscoveryManager.isBtConnected) {
                    BluetoothDiscoveryManager.sendBtMessage(message)
                }
            }
        }

        fun sendUdpBytes(bytes: ByteArray) {
            CoroutineScope(Dispatchers.IO).launch {
                if (udpSocket == null || udpSocket!!.isClosed) {
                    udpSocket = DatagramSocket().apply { broadcast = true }
                }

                val directIp = manualBotIp ?: discoveredEspIp
                if (directIp != null) {
                    try {
                        val packet = DatagramPacket(
                            bytes,
                            bytes.size,
                            InetAddress.getByName(directIp),
                            ESP_PORT
                        )
                        udpSocket?.send(packet)
                    } catch (e: Exception) {
                        e.printStackTrace()
                    }
                }

                try {
                    val interfaces = NetworkInterface.getNetworkInterfaces()
                    while (interfaces.hasMoreElements()) {
                        val iface = interfaces.nextElement()
                        if (iface.isLoopback || !iface.isUp) continue
                        for (addr in iface.interfaceAddresses) {
                            val bcast = addr.broadcast
                            if (bcast != null) {
                                try {
                                    val p = DatagramPacket(bytes, bytes.size, bcast, ESP_PORT)
                                    udpSocket?.send(p)
                                } catch (e: Exception) {}
                            }
                        }
                    }
                } catch (e: Exception) {}

                try {
                    val bcast = DatagramPacket(
                        bytes,
                        bytes.size,
                        InetAddress.getByName("255.255.255.255"),
                        ESP_PORT
                    )
                    udpSocket?.send(bcast)
                } catch (e: Exception) {}

                if (BluetoothDiscoveryManager.isBtConnected) {
                    BluetoothDiscoveryManager.sendBtBytes(bytes)
                }
            }
        }

        fun streamCustomPhoto(bitmap: Bitmap) {
            CoroutineScope(Dispatchers.IO).launch {
                MediaArtProcessor.streamCustomEyeImage(bitmap)
            }
        }

        fun triggerInstantRelays() {
            CoroutineScope(Dispatchers.IO).launch {
                TimeRelay.syncDeviceTime()
                WeatherRelay.fetchAndSendWeather()
                StockRelay.fetchAndSendStocks()
                TaskRelay.sendProductivityStatus()
                GitHubRelay.fetchAndSendGitHubStats()
                SteamRelay.fetchAndSendGamingStats()
                SocialRelay.sendSocialFeed()
                ThoughtRelay.injectRandomThought()
            }
        }

        fun saveAndSetWifi(ssid: String, pass: String) {
            val json = JSONObject().apply {
                put("ssid", ssid)
                put("pass", pass)
            }
            sendUdp("WIFI:SET:$json")
        }

        fun saveAndSetMultiWifi(jsonPayload: String) {
            sendUdp("WIFI:MULTI_SET:$jsonPayload")
        }
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == ACTION_STOP_SERVICE) {
            try {
                multicastLock?.let { if (it.isHeld) it.release() }
                wakeLock?.let { if (it.isHeld) it.release() }
                udpSocket?.close()
                stopForeground(true)
                stopSelf()
                android.os.Process.killProcess(android.os.Process.myPid())
            } catch (e: Exception) {
                e.printStackTrace()
            }
            return START_NOT_STICKY
        }
        return START_STICKY
    }

    override fun onCreate() {
        super.onCreate()
        createNotificationChannel()

        val powerManager = getSystemService(Context.POWER_SERVICE) as PowerManager
        wakeLock = powerManager.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "CompanionOS::BackgroundLock").apply {
            acquire(24 * 60 * 60 * 1000L)
        }

        // Acquire MulticastLock directly in background service for 24/7 unthrottled UDP packets
        try {
            val wifiManager = applicationContext.getSystemService(Context.WIFI_SERVICE) as WifiManager
            multicastLock = wifiManager.createMulticastLock("CompanionOS::MulticastLock").apply {
                setReferenceCounted(true)
                acquire()
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }

        startForeground(NOTIFICATION_ID, buildNotification("CompanionOS Master Controller Active"))
        startUdpListener()
        startActiveDiscoveryProbe()
        BluetoothDiscoveryManager.startAutoDiscoveryAndConnect(this)
        startBackgroundRelays()
    }

    private fun buildNotification(statusText: String): Notification {
        val notificationIntent = Intent(this, MainActivity::class.java)
        val pendingIntent = PendingIntent.getActivity(
            this,
            0,
            notificationIntent,
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT
        )

        val stopIntent = Intent(this, CompanionForegroundService::class.java).apply {
            action = ACTION_STOP_SERVICE
        }
        val stopPendingIntent = PendingIntent.getService(
            this,
            1,
            stopIntent,
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT
        )

        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle("CompanionOS Controller")
            .setContentText(statusText)
            .setSmallIcon(android.R.drawable.stat_notify_sync)
            .setContentIntent(pendingIntent)
            .addAction(android.R.drawable.ic_menu_close_clear_cancel, "STOP / EXIT", stopPendingIntent)
            .setOngoing(true)
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .build()
    }

    private fun updateNotification(statusText: String) {
        val manager = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        manager.notify(NOTIFICATION_ID, buildNotification(statusText))
    }

    private fun startUdpListener() {
        serviceScope.launch {
            try {
                val socket = DatagramSocket(LOCAL_PORT).apply {
                    broadcast = true
                    soTimeout = 2000
                }
                val buffer = ByteArray(2048)

                while (isActive) {
                    try {
                        val packet = DatagramPacket(buffer, buffer.size)
                        socket.receive(packet)
                        val senderIp = packet.address.hostAddress

                        if (senderIp != null && senderIp != "127.0.0.1") {
                            if (discoveredEspIp != senderIp) {
                                discoveredEspIp = senderIp
                                updateNotification("Connected: $senderIp (WiFi)")
                                triggerInstantRelays()
                            }
                        }
                    } catch (e: Exception) {
                        // socket timeout
                    }
                }
            } catch (e: Exception) {
                e.printStackTrace()
            }
        }
    }

    private fun startActiveDiscoveryProbe() {
        serviceScope.launch {
            while (isActive) {
                if (discoveredEspIp == null) {
                    performSubnetProbe()
                    delay(2000)
                } else {
                    delay(15000)
                }
            }
        }
    }

    private fun performSubnetProbe() {
        try {
            val probeData = "COMPANION_PROBE".toByteArray(Charsets.UTF_8)
            val socket = DatagramSocket().apply { broadcast = true }

            // 1. Global Broadcast
            try {
                val p = DatagramPacket(probeData, probeData.size, InetAddress.getByName("255.255.255.255"), ESP_PORT)
                socket.send(p)
            } catch (e: Exception) {}

            // 2. Discover local IP subnets and broadcast to each interface
            val interfaces = NetworkInterface.getNetworkInterfaces()
            while (interfaces.hasMoreElements()) {
                val iface = interfaces.nextElement()
                if (iface.isLoopback || !iface.isUp) continue

                for (addr in iface.interfaceAddresses) {
                    val bcast = addr.broadcast
                    if (bcast != null) {
                        try {
                            val p = DatagramPacket(probeData, probeData.size, bcast, ESP_PORT)
                            socket.send(p)
                        } catch (e: Exception) {}
                    }

                    // Fast sweep on /24 subnet
                    val hostAddr = addr.address?.hostAddress
                    if (hostAddr != null && hostAddr.contains(".") && !hostAddr.startsWith("127.")) {
                        val prefix = hostAddr.substringBeforeLast(".")
                        serviceScope.launch {
                            for (i in 1..254) {
                                if (discoveredEspIp != null) break
                                try {
                                    val target = InetAddress.getByName("$prefix.$i")
                                    val p = DatagramPacket(probeData, probeData.size, target, ESP_PORT)
                                    socket.send(p)
                                } catch (e: Exception) {}
                            }
                        }
                    }
                }
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    private fun startBackgroundRelays() {
        // 1. Time Sync Loop (Every 60s)
        serviceScope.launch {
            while (isActive) {
                TimeRelay.syncDeviceTime()
                delay(60000)
            }
        }

        // 2. Weather Relay Loop (Every 15 minutes)
        serviceScope.launch {
            while (isActive) {
                WeatherRelay.fetchAndSendWeather()
                delay(15 * 60000)
            }
        }

        // 3. Stock & Crypto Relay Loop (Every 5 minutes)
        serviceScope.launch {
            while (isActive) {
                StockRelay.fetchAndSendStocks()
                delay(5 * 60000)
            }
        }

        // 4. Productivity & Task Relay Loop (Every 10 minutes)
        serviceScope.launch {
            while (isActive) {
                TaskRelay.sendProductivityStatus()
                delay(10 * 60000)
            }
        }

        // 5. GitHub & Developer Stats Loop (Every 10 minutes)
        serviceScope.launch {
            while (isActive) {
                GitHubRelay.fetchAndSendGitHubStats()
                delay(10 * 60000)
            }
        }

        // 6. Steam & Gaming Status Loop (Every 5 minutes)
        serviceScope.launch {
            while (isActive) {
                SteamRelay.fetchAndSendGamingStats()
                delay(5 * 60000)
            }
        }

        // 7. Social Community Feed Loop (Every 15 minutes)
        serviceScope.launch {
            while (isActive) {
                SocialRelay.sendSocialFeed()
                delay(15 * 60000)
            }
        }

        // 8. AI Thought Bubble Loop (Every 2 minutes)
        serviceScope.launch {
            while (isActive) {
                delay(2 * 60000)
                ThoughtRelay.injectRandomThought()
            }
        }

        // 9. Telemetry Heartbeat (Every 15s)
        serviceScope.launch {
            while (isActive) {
                delay(15000)
                if (discoveredEspIp != null) {
                    sendUdp("PING")
                }
            }
        }
    }

    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                CHANNEL_ID,
                "CompanionOS Background Service",
                NotificationManager.IMPORTANCE_LOW
            ).apply {
                description = "Keeps CompanionOS bot synchronization and relays active 24/7"
            }
            val manager = getSystemService(NotificationManager::class.java)
            manager.createNotificationChannel(channel)
        }
    }

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onDestroy() {
        super.onDestroy()
        serviceJob.cancel()
        multicastLock?.let { if (it.isHeld) it.release() }
        wakeLock?.let { if (it.isHeld) it.release() }
    }
}
