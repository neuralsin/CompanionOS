package com.companionos.app

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONObject
import kotlin.random.Random

object ThoughtRelay {

    private val THOUGHTS = listOf(
        "Checking system telemetry...",
        "Neural sync optimal.",
        "Listening for audio triggers...",
        "Companion bot is happy!",
        "Optimizing display buffers...",
        "Wi-Fi signal locked.",
        "Ready to assist you today.",
        "Cybertronic core online."
    )

    suspend fun injectRandomThought() = withContext(Dispatchers.IO) {
        try {
            val thought = THOUGHTS[Random.nextInt(THOUGHTS.size)]
            CompanionForegroundService.sendUdp("THOUGHT:$thought")
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    suspend fun sendAgentStatus(status: String, message: String) = withContext(Dispatchers.IO) {
        try {
            val payload = JSONObject().apply {
                put("status", status)
                put("text", message)
            }
            CompanionForegroundService.sendUdp("AGENT:$payload")
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }
}
