package com.companionos.app

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.HttpUrl.Companion.toHttpUrlOrNull
import okhttp3.OkHttpClient
import okhttp3.Request
import org.json.JSONArray
import org.json.JSONObject
import java.net.URLEncoder
import java.util.concurrent.TimeUnit
import java.util.regex.Pattern

object LrcLibLyricsService {

    private val httpClient = OkHttpClient.Builder()
        .connectTimeout(5, TimeUnit.SECONDS)
        .readTimeout(8, TimeUnit.SECONDS)
        .build()

    data class LyricLine(val timeMs: Long, val text: String)

    private fun cleanTitle(title: String): String {
        return title
            .replace(Regex("""\s*-\s*Remaster(ed)?(\s*\d{4})?""", RegexOption.IGNORE_CASE), "")
            .replace(Regex("""\s*\(feat\..*?\)""", RegexOption.IGNORE_CASE), "")
            .replace(Regex("""\s*\[feat\..*?\]""", RegexOption.IGNORE_CASE), "")
            .replace(Regex("""\s*\(with\s+.*?\)""", RegexOption.IGNORE_CASE), "")
            .replace(Regex("""\s*\(Live.*?\)""", RegexOption.IGNORE_CASE), "")
            .replace(Regex("""\s*-\s*Single Version""", RegexOption.IGNORE_CASE), "")
            .replace(Regex("""\s*-\s*Radio Edit""", RegexOption.IGNORE_CASE), "")
            .trim()
    }

    private fun parseLrc(syncedLyrics: String): List<LyricLine> {
        val lines = mutableListOf<LyricLine>()
        val pattern = Pattern.compile("""\[(\d{2}):(\d{2})\.(\d{2,3})\](.*)""")
        
        for (rawLine in syncedLyrics.lines()) {
            val matcher = pattern.matcher(rawLine.trim())
            if (matcher.matches()) {
                val min = matcher.group(1)?.toLongOrNull() ?: 0L
                val sec = matcher.group(2)?.toLongOrNull() ?: 0L
                val fracStr = matcher.group(3) ?: "00"
                val frac = if (fracStr.length == 2) fracStr.toLong() * 10 else fracStr.toLong()
                val totalMs = (min * 60 * 1000) + (sec * 1000) + frac
                val text = matcher.group(4)?.trim() ?: ""
                lines.add(LyricLine(totalMs, text))
            }
        }
        return lines.sortedBy { it.timeMs }
    }

    suspend fun fetchLyrics(track: String, artist: String): List<LyricLine> = withContext(Dispatchers.IO) {
        val cleanTrk = cleanTitle(track)
        val cleanArt = artist.split(",")[0].trim()

        try {
            // 1. Direct GET request
            val url = "https://lrclib.net/api/get".toHttpUrlOrNull()?.newBuilder()
                ?.addQueryParameter("track_name", cleanTrk)
                ?.addQueryParameter("artist_name", cleanArt)
                ?.build()

            if (url != null) {
                val request = Request.Builder()
                    .url(url)
                    .header("User-Agent", "CompanionOS Android v7.0")
                    .build()

                val response = httpClient.newCall(request).execute()
                if (response.isSuccessful) {
                    val body = response.body?.string()
                    if (!body.isNullOrEmpty()) {
                        val json = JSONObject(body)
                        val synced = json.optString("syncedLyrics", "")
                        if (synced.isNotEmpty()) {
                            return@withContext parseLrc(synced)
                        }
                    }
                }
            }

            // 2. Search Fallback
            val searchUrl = "https://lrclib.net/api/search".toHttpUrlOrNull()?.newBuilder()
                ?.addQueryParameter("q", "$cleanTrk $cleanArt")
                ?.build()

            if (searchUrl != null) {
                val searchReq = Request.Builder()
                    .url(searchUrl)
                    .header("User-Agent", "CompanionOS Android v7.0")
                    .build()

                val searchResp = httpClient.newCall(searchReq).execute()
                if (searchResp.isSuccessful) {
                    val searchBody = searchResp.body?.string()
                    if (!searchBody.isNullOrEmpty()) {
                        val arr = JSONArray(searchBody)
                        if (arr.length() > 0) {
                            val first = arr.getJSONObject(0)
                            val synced = first.optString("syncedLyrics", "")
                            if (synced.isNotEmpty()) {
                                return@withContext parseLrc(synced)
                            }
                        }
                    }
                }
            }
        } catch (e: Exception) {
            // Silent error handling for network resilience
        }
        emptyList()
    }

    fun getLyricsAtPosition(lyrics: List<LyricLine>, posMs: Long): Triple<String, String, String> {
        if (lyrics.isEmpty()) return Triple("", "", "")

        var currentIdx = -1
        for (i in lyrics.indices) {
            if (lyrics[i].timeMs <= posMs) {
                currentIdx = i
            } else {
                break
            }
        }

        if (currentIdx == -1) {
            val line1 = lyrics.firstOrNull()?.text ?: ""
            val line2 = if (lyrics.size > 1) lyrics[1].text else ""
            return Triple("", line1, line2)
        }

        val prev = if (currentIdx > 0) lyrics[currentIdx - 1].text else ""
        val cur = lyrics[currentIdx].text
        val next = if (currentIdx + 1 < lyrics.size) lyrics[currentIdx + 1].text else ""

        return Triple(prev, cur, next)
    }
}
