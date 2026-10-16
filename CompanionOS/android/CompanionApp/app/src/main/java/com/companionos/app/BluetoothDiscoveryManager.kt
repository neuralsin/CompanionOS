package com.companionos.app

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothSocket
import android.content.Context
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import java.io.BufferedReader
import java.io.InputStreamReader
import java.io.OutputStream
import java.util.UUID

object BluetoothDiscoveryManager {

    private val SPP_UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB")
    private var bluetoothSocket: BluetoothSocket? = null
    private var outputStream: OutputStream? = null

    @Volatile
    var isBtConnected: Boolean = false
        private set

    @Volatile
    var connectedDeviceName: String? = null
        private set

    private val scope = CoroutineScope(Dispatchers.IO + Job())

    @SuppressLint("MissingPermission")
    fun startAutoDiscoveryAndConnect(context: Context) {
        val adapter = BluetoothAdapter.getDefaultAdapter() ?: return
        if (!adapter.isEnabled) return

        scope.launch {
            while (isActive) {
                if (!isBtConnected) {
                    try {
                        val pairedDevices: Set<BluetoothDevice>? = adapter.bondedDevices
                        var targetDevice: BluetoothDevice? = null

                        pairedDevices?.forEach { device ->
                            val name = device.name ?: ""
                            if (name.contains("Companion", ignoreCase = true) || name.contains("DeskBot", ignoreCase = true)) {
                                targetDevice = device
                            }
                        }

                        if (targetDevice != null) {
                            connectToDevice(targetDevice!!)
                        }
                    } catch (e: Exception) {
                        e.printStackTrace()
                    }
                }
                delay(8000) // Scan / reconnect interval
            }
        }
    }

    @SuppressLint("MissingPermission")
    private fun connectToDevice(device: BluetoothDevice) {
        try {
            bluetoothSocket?.close()
            val socket = device.createRfcommSocketToServiceRecord(SPP_UUID)
            socket.connect()

            bluetoothSocket = socket
            outputStream = socket.outputStream
            isBtConnected = true
            connectedDeviceName = device.name

            // Start bidirectional incoming telemetry stream reader
            startIncomingReader(socket)

            // Trigger instant cloud-to-bluetooth sync
            CompanionForegroundService.triggerInstantRelays()
        } catch (e: Exception) {
            isBtConnected = false
            bluetoothSocket = null
            outputStream = null
        }
    }

    private fun startIncomingReader(socket: BluetoothSocket) {
        scope.launch {
            try {
                val reader = BufferedReader(InputStreamReader(socket.inputStream))
                while (isActive && isBtConnected) {
                    val line = reader.readLine() ?: break
                    val trimmed = line.trim()
                    if (trimmed.isNotEmpty()) {
                        handleIncomingBtMessage(trimmed)
                    }
                }
            } catch (e: Exception) {
                // Connection dropped
            } finally {
                isBtConnected = false
                bluetoothSocket = null
                outputStream = null
            }
        }
    }

    private fun handleIncomingBtMessage(msg: String) {
        if (msg == "HELLO_COMPANION") {
            CompanionForegroundService.triggerInstantRelays()
        }
    }

    fun sendBtMessage(message: String): Boolean {
        return try {
            if (isBtConnected && outputStream != null) {
                val data = (message.trimEnd() + "\n").toByteArray(Charsets.UTF_8)
                outputStream?.write(data)
                outputStream?.flush()
                true
            } else {
                false
            }
        } catch (e: Exception) {
            isBtConnected = false
            bluetoothSocket = null
            outputStream = null
            false
        }
    }

    fun sendBtBytes(bytes: ByteArray): Boolean {
        return try {
            if (isBtConnected && outputStream != null) {
                outputStream?.write(bytes)
                outputStream?.flush()
                true
            } else {
                false
            }
        } catch (e: Exception) {
            isBtConnected = false
            bluetoothSocket = null
            outputStream = null
            false
        }
    }
}
