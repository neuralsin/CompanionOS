package com.companionos.app

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import okhttp3.Request
import org.json.JSONObject
import java.util.concurrent.TimeUnit

object WeatherRelay {

    private val httpClient = OkHttpClient.Builder()
        .connectTimeout(6, TimeUnit.SECONDS)
        .readTimeout(10, TimeUnit.SECONDS)
        .build()

    var defaultCity = "Delhi"
    var cityName = "Delhi"
    var latitude = 28.6139
    var longitude = 77.2090

    private fun mapWmoCode(code: Int): Pair<String, Int> {
        return when (code) {
            0 -> Pair("Clear", 800)
            1, 2, 3 -> Pair("Partly Cloudy", 802)
            45, 48 -> Pair("Foggy", 741)
            51, 53, 55, 56, 57 -> Pair("Drizzle", 300)
            61, 63, 65, 66, 67 -> Pair("Rain", 500)
            71, 73, 75, 77 -> Pair("Snow", 600)
            80, 81, 82 -> Pair("Showers", 521)
            85, 86 -> Pair("Snow Showers", 621)
            95, 96, 99 -> Pair("Thunderstorm", 200)
            else -> Pair("Clouds", 803)
        }
    }

    suspend fun fetchAndSendWeather() = withContext(Dispatchers.IO) {
        try {
            val url = "https://api.open-meteo.com/v1/forecast?latitude=$latitude&longitude=$longitude&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,wind_speed_10m&daily=temperature_2m_max,temperature_2m_min,sunrise,sunset&timezone=auto"
            val request = Request.Builder().url(url).build()
            val response = httpClient.newCall(request).execute()

            if (response.isSuccessful) {
                val body = response.body?.string()
                if (!body.isNullOrEmpty()) {
                    val root = JSONObject(body)
                    val current = root.getJSONObject("current")
                    val daily = root.getJSONObject("daily")

                    val temp = current.optDouble("temperature_2m", 25.0).toInt()
                    val feels = current.optDouble("apparent_temperature", 26.0).toInt()
                    val humidity = current.optInt("relative_humidity_2m", 50)
                    val wind = current.optDouble("wind_speed_10m", 10.0).toInt()
                    val wmoCode = current.optInt("weather_code", 0)

                    val maxArr = daily.optJSONArray("temperature_2m_max")
                    val minArr = daily.optJSONArray("temperature_2m_min")
                    val high = if (maxArr != null && maxArr.length() > 0) maxArr.getDouble(0).toInt() else temp + 4
                    val low = if (minArr != null && minArr.length() > 0) minArr.getDouble(0).toInt() else temp - 4

                    val (cond, code) = mapWmoCode(wmoCode)

                    val payload = JSONObject().apply {
                        put("city", cityName)
                        put("temp", temp)
                        put("feels", feels)
                        put("humidity", humidity)
                        put("wind", wind)
                        put("high", high)
                        put("low", low)
                        put("cond", cond)
                        put("code", code)
                    }

                    CompanionForegroundService.sendUdp("WEATHER:$payload")
                }
            }
        } catch (e: Exception) {
            // Log resiliently
        }
    }
}
