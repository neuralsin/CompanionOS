package com.companionos.app

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.text.SimpleDateFormat
import java.util.Calendar
import java.util.Date
import java.util.Locale

object TaskRelay {

    // Default productivity tasks or sync
    var currentTask = "Code CompanionOS v7.0"
    var taskNext1 = "Hardware Verification"
    var taskNext2 = "Deploy APK"

    suspend fun sendProductivityStatus() = withContext(Dispatchers.IO) {
        try {
            val now = Date()
            val timeFormat = SimpleDateFormat("HH:mm", Locale.getDefault())
            val curTimeStr = timeFormat.format(now)

            val cal = Calendar.getInstance()
            cal.add(Calendar.HOUR_OF_DAY, 1)
            val next1TimeStr = timeFormat.format(cal.time)

            cal.add(Calendar.HOUR_OF_DAY, 2)
            val next2TimeStr = timeFormat.format(cal.time)

            val payload = JSONObject().apply {
                put("current", currentTask)
                put("current_time", curTimeStr)
                put("next1", taskNext1)
                put("next1_time", next1TimeStr)
                put("next2", taskNext2)
                put("next2_time", next2TimeStr)
                put("active", true)
                put("progress", 75)
            }

            CompanionForegroundService.sendUdp("TASKS:$payload")
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }
}
