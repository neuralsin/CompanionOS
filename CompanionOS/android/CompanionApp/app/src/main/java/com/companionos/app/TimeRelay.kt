package com.companionos.app

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.util.Calendar

object TimeRelay {

    suspend fun syncDeviceTime() = withContext(Dispatchers.IO) {
        try {
            val cal = Calendar.getInstance()
            val hour = cal.get(Calendar.HOUR_OF_DAY)
            val minute = cal.get(Calendar.MINUTE)
            val second = cal.get(Calendar.SECOND)
            val dayOfWeek = cal.get(Calendar.DAY_OF_WEEK)
            val dayOfMonth = cal.get(Calendar.DAY_OF_MONTH)
            val month = cal.get(Calendar.MONTH) + 1
            val year = cal.get(Calendar.YEAR)

            val days = arrayOf("SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT")
            val dayName = days[dayOfWeek - 1]

            val payload = JSONObject().apply {
                put("hour", hour)
                put("min", minute)
                put("sec", second)
                put("day", dayName)
                put("date", dayOfMonth)
                put("month", month)
                put("year", year)
            }

            CompanionForegroundService.sendUdp("TIME:$payload")
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }
}
