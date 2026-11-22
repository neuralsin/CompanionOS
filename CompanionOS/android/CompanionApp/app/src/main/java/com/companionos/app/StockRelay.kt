package com.companionos.app

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import okhttp3.Request
import org.json.JSONArray
import org.json.JSONObject
import java.util.Locale
import java.util.concurrent.TimeUnit

object StockRelay {
    private val client = OkHttpClient.Builder().callTimeout(10, TimeUnit.SECONDS).build()
    private val ids = mapOf("BTC" to "bitcoin", "ETH" to "ethereum", "SOL" to "solana")
    @Volatile var symbols = listOf("BTC", "ETH", "SOL")
    fun setSymbols(value: String) { symbols = value.uppercase(Locale.ROOT).split(',').map { it.trim() }.filter { it in ids }.distinct().take(3) }
    suspend fun fetchAndSendStocks() = withContext(Dispatchers.IO) {
        val selected = symbols.toList()
        if (selected.isEmpty()) return@withContext
        try {
            val url = "https://api.coingecko.com/api/v3/simple/price?ids=${selected.mapNotNull { ids[it] }.joinToString(",")}&vs_currencies=usd&include_24hr_change=true"
            client.newCall(Request.Builder().url(url).build()).execute().use { response ->
                if (!response.isSuccessful) return@withContext
                val root = JSONObject(response.body?.string() ?: return@withContext)
                val items = JSONArray()
                for (symbol in selected) {
                    val quote = root.optJSONObject(ids[symbol]!!) ?: continue
                    if (!quote.has("usd") || quote.isNull("usd")) continue
                    items.put(JSONObject().put("s", symbol)
                        .put("p", String.format(Locale.US, "$%.2f", quote.getDouble("usd")))
                        .put("u", quote.optDouble("usd_24h_change", 0.0) >= 0)
                        .put("d", if (quote.isNull("usd_24h_change")) "--" else String.format(Locale.US, "%+.2f%%", quote.getDouble("usd_24h_change"))))
                }
                if (items.length() > 0) {
                    val first = items.getJSONObject(0)
                    CompanionForegroundService.sendUdp("STOCKS:" + JSONObject().put("wl", items)
                        .put("symbol", first.getString("s")).put("price", first.getString("p"))
                        .put("delta", "").put("hist", JSONArray())
                        .put("pct", first.getString("d")).put("up", first.getBoolean("u")))
                }
            }
        } catch (_: Exception) { /* Keep last verified data; never synthesize market quotes. */ }
    }
}
