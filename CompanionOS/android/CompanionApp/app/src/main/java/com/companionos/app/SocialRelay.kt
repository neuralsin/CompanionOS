package com.companionos.app

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

object SocialRelay {

    suspend fun sendSocialFeed() = withContext(Dispatchers.IO) {
        try {
            val timeStr = SimpleDateFormat("HH:mm", Locale.getDefault()).format(Date())
            val payload = JSONObject().apply {
                put("user", "@neuralsin")
                put("app", "YouTube")
                put("body", "New CompanionOS update is live! Check the repo.")
                put("time", timeStr)
                put("likes", 128)
                put("comments", 24)
            }
            CompanionForegroundService.sendUdp("SOCIAL:$payload")
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }
}
