package com.companionos.app

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import okhttp3.Request
import org.json.JSONObject
import java.util.concurrent.TimeUnit

object GitHubRelay {

    private val httpClient = OkHttpClient.Builder()
        .connectTimeout(5, TimeUnit.SECONDS)
        .readTimeout(5, TimeUnit.SECONDS)
        .build()

    var githubUsername: String = "neuralsin"
    var githubToken: String = "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"

    suspend fun fetchAndSendGitHubStats() = withContext(Dispatchers.IO) {
        try {
            if (githubUsername.isEmpty()) return@withContext

            val url = "https://api.github.com/users/$githubUsername"
            val reqBuilder = Request.Builder().url(url).header("User-Agent", "CompanionOS-Android")
            if (githubToken.isNotEmpty()) {
                reqBuilder.header("Authorization", "token $githubToken")
            }

            val resp = httpClient.newCall(reqBuilder.build()).execute()
            if (resp.isSuccessful) {
                val jsonStr = resp.body?.string() ?: ""
                val obj = JSONObject(jsonStr)

                val login = obj.optString("login", githubUsername)
                val repos = obj.optInt("public_repos", 0)
                val followers = obj.optInt("followers", 0)
                val following = obj.optInt("following", 0)

                val payload = JSONObject().apply {
                    put("user", login)
                    put("repos", repos)
                    put("followers", followers)
                    put("following", following)
                }

                CompanionForegroundService.sendUdp("GITHUB:$payload")
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }
}
