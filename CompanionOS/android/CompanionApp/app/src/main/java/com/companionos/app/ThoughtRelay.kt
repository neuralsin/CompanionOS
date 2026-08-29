package com.companionos.app

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONObject
import kotlin.random.Random

object ThoughtRelay {

    private val GENERAL_THOUGHTS = listOf(
        "Make it work, make it right, make it fast.",
        "Talk is cheap. Show me the code.",
        "Code is like humor. When you have to explain it, it’s bad.",
        "Simplicity is the soul of efficiency.",
        "First, solve the problem. Then, write the code.",
        "Stay hungry, stay foolish.",
        "Dream big, build fast, learn always.",
        "Consistency beats intensity every single time.",
        "Focus on progress, not perfection.",
        "Your future is created by what you do today.",
        "Neural sync optimal. CompanionOS ready.",
        "Cybertronic core operating at 100%.",
        "Optimizing display buffers and memory...",
        "Companion bot is happy to hang out!",
        "Coffee: turning caffeine into code since 1995.",
        "Git commit -m 'Fixed everything. Hopefully.'",
        "It compiles on my machine!",
        "Refactoring the universe, one line at a time."
    )

    private val MUSIC_THOUGHTS = listOf(
        "This song hits different rn.",
        "Vibing on repeat all day.",
        "Pure audio bliss.",
        "Soundtrack to a productive flow.",
        "Turn the volume up!"
    )

    private val WEATHER_THOUGHTS = listOf(
        "Rainy vibes + lofi = perfect combo.",
        "Sun's out! Time to conquer the day.",
        "Cold outside? Blanket burrito mode.",
        "Weather check complete. Stay cozy!"
    )

    suspend fun injectRandomThought() = withContext(Dispatchers.IO) {
        try {
            val pool = GENERAL_THOUGHTS + WEATHER_THOUGHTS
            val thought = pool[Random.nextInt(pool.size)]
            pushThought(thought)
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    suspend fun pushMusicThought(track: String, artist: String) = withContext(Dispatchers.IO) {
        try {
            val templates = listOf(
                "Vibing to $track",
                "$artist never misses!",
                "On repeat: $track",
                "Great music taste detected."
            )
            val thought = templates[Random.nextInt(templates.size)]
            pushThought(thought)
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    suspend fun pushThought(text: String) = withContext(Dispatchers.IO) {
        try {
            val clean = if (text.length > 76) text.substring(0, 73) + "..." else text
            CompanionForegroundService.sendUdp("THOUGHT:$clean")
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
