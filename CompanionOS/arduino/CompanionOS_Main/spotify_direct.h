#ifndef SPOTIFY_DIRECT_H
#define SPOTIFY_DIRECT_H

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <TJpg_Decoder.h>

#include "globals.h"
#include "config.h"

// ═══════════════════════════════════════════════════════════
// DIRECT SPOTIFY ENGINE (Autonomous ESP8266 Subsystem)
// 1:1 Logic with companion_controller.py
// - Autonomous OAuth Token Refresh with Spotify Accounts API
// - Real-time Player state polling (/v1/me/player)
// - Dynamic synced lyrics fetching & parsing from LRCLIB.net
// - Direct 64x64 Album Art download + TJpg_Decoder to 96x96
// - Direct Touch & Gesture Playback Controls
// ═══════════════════════════════════════════════════════════

// External globals from other headers
extern String currentTrack;
extern String currentArtist;
extern String currentLyrics;
extern String currentLyricsLine2;
extern String prevLyricsLine;
extern int playProgress;
extern int playDuration;
extern bool isPlaying;
extern bool musicPlaying;
extern bool albumArtReady;
extern bool artDrawn;
extern uint8_t activeTheme;
extern AppState currentState;

// Theme 2 controls
extern uint8_t t2_volume;
extern bool    t2_shuffle;
extern uint8_t t2_repeat;
extern bool    t2s_artDrawn;

extern void redrawSpotifyPartial();
extern void t2_redrawSpotifyPartial();

// OAuth Token storage
static String spotifyAccessToken = "";
static unsigned long spotifyTokenExpiresAt = 0;
static bool spotifyTokenRefreshing = false;

// Track & Art State Tracking
static char lastSpotifyTrackId[40] = "";
static String spotifyArtUrl = "";
static bool newArtPending = false;
static unsigned long lastSpotifyPoll = 0;
static unsigned long lastLocalProgressTick = 0;

// Synced lyrics data structure (1:1 with Python parsed lines)
struct DirectLyricLine {
  uint32_t timeMs;
  char words[48];
};

#define DIRECT_MAX_LYRICS 75
static DirectLyricLine directLyrics[DIRECT_MAX_LYRICS];
static int directLyricCount = 0;
static String lastLyricKey = "";

// ── Base64 Encoding Helper ──
static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static String spotifyBase64(const String& input) {
  String output = "";
  int len = input.length();
  for (int i = 0; i < len; i += 3) {
    uint32_t val = (uint8_t)input[i] << 16;
    if (i + 1 < len) val |= (uint8_t)input[i + 1] << 8;
    if (i + 2 < len) val |= (uint8_t)input[i + 2];
    output += b64_table[(val >> 18) & 0x3F];
    output += b64_table[(val >> 12) & 0x3F];
    output += (i + 1 < len) ? b64_table[(val >> 6) & 0x3F] : '=';
    output += (i + 2 < len) ? b64_table[val & 0x3F] : '=';
  }
  return output;
}

// ── URL Encoding Helper for LRCLIB Query ──
static String spotifyUrlEncode(const String& str) {
  String encoded = "";
  char buf[4];
  for (unsigned int i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else if (c == ' ') {
      encoded += '+';
    } else {
      sprintf(buf, "%%%02X", (uint8_t)c);
      encoded += buf;
    }
  }
  return encoded;
}

// ── TJpg_Decoder Callback for 64x64 Album Art Decode ──
static bool spotifyTjpgCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  for (int j = 0; j < h; j++) {
    for (int i = 0; i < w; i++) {
      int px = x + i;
      int py = y + j;
      if (px < 64 && py < 64) {
        albumArt[py * 64 + px] = bitmap[j * w + i];
      }
    }
  }
  return true;
}

// ═══════════════════════════════════════════════════════════
// 1. REFRESH SPOTIFY ACCESS TOKEN
// ═══════════════════════════════════════════════════════════
bool refreshSpotifyToken() {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (strlen(SPOTIFY_CLIENT_ID) == 0 || strlen(SPOTIFY_REFRESH_TOKEN) == 0) return false;

  Serial.println(F("[Spotify Direct] Refreshing OAuth token..."));
  spotifyTokenRefreshing = true;

  WiFiClientSecure client;
  client.setInsecure();
  client.setBufferSizes(1024, 512);
  client.setTimeout(4000);

  if (!client.connect("accounts.spotify.com", 443)) {
    Serial.println(F("[Spotify Direct] Failed to connect to accounts.spotify.com"));
    spotifyTokenRefreshing = false;
    return false;
  }

  String body = "grant_type=refresh_token&refresh_token=" + String(SPOTIFY_REFRESH_TOKEN);
  String authHeader = "Basic " + spotifyBase64(String(SPOTIFY_CLIENT_ID) + ":" + String(SPOTIFY_CLIENT_SECRET));

  client.print(F("POST /api/token HTTP/1.0\r\n"));
  client.print(F("Host: accounts.spotify.com\r\n"));
  client.print(F("User-Agent: ESP8266-CompanionOS\r\n"));
  client.print("Authorization: " + authHeader + "\r\n");
  client.print(F("Content-Type: application/x-www-form-urlencoded\r\n"));
  client.print("Content-Length: " + String(body.length()) + "\r\n");
  client.print(F("Connection: close\r\n\r\n"));
  client.print(body);

  // Skip HTTP headers
  bool headersEnd = false;
  unsigned long timeout = millis() + 4000;
  while (client.connected() && millis() < timeout) {
    String line = client.readStringUntil('\n');
    if (line == "\r" || line.length() == 0) {
      headersEnd = true;
      break;
    }
    yield();
  }

  if (!headersEnd) {
    client.stop();
    spotifyTokenRefreshing = false;
    return false;
  }

  // Parse JSON response
  DynamicJsonDocument doc(1024);
  DeserializationError err = deserializeJson(doc, client);
  client.stop();

  if (!err && doc.containsKey("access_token")) {
    spotifyAccessToken = doc["access_token"].as<String>();
    unsigned long expiresIn = doc["expires_in"] | 3600;
    // Set expiry 90 seconds early to prevent edge timeouts
    spotifyTokenExpiresAt = millis() + (expiresIn > 90 ? (expiresIn - 90) : 3000) * 1000UL;
    Serial.println(F("[Spotify Direct] Access token successfully acquired!"));
    spotifyTokenRefreshing = false;
    return true;
  } else {
    Serial.print(F("[Spotify Direct] Token parse error: "));
    Serial.println(err.c_str());
    spotifyTokenRefreshing = false;
    return false;
  }
}

// ═══════════════════════════════════════════════════════════
// 2. 1:1 LYRICS SYNC MATCHER (Matches Python controller logic)
// ═══════════════════════════════════════════════════════════
void updateSyncedLyrics() {
  if (directLyricCount == 0) {
    if (currentLyrics != "♪ Instrumental ♪" && currentLyrics.length() > 0) {
      // keep current or clear
    }
    return;
  }

  // Find matching index where timeMs <= playProgress + 500 (exact Python matching logic)
  int currentIdx = -1;
  uint32_t targetTime = (playProgress > 0) ? (playProgress + 500) : 500;

  for (int i = 0; i < directLyricCount; i++) {
    if (directLyrics[i].timeMs <= targetTime) {
      currentIdx = i;
    } else {
      break;
    }
  }

  String newPrev = "";
  String newCur = "";
  String newNext = "";

  if (currentIdx >= 0) {
    newPrev = (currentIdx > 0) ? String(directLyrics[currentIdx - 1].words) : "";
    newCur = String(directLyrics[currentIdx].words);
    newNext = (currentIdx + 1 < directLyricCount) ? String(directLyrics[currentIdx + 1].words) : "";
  } else {
    // Intro before first line triggers
    newPrev = "";
    newCur = String(directLyrics[0].words);
    newNext = (directLyricCount > 1) ? String(directLyrics[1].words) : "";
  }

  String combo = newPrev + "|" + newCur + "|" + newNext;
  if (combo != lastLyricKey) {
    lastLyricKey = combo;
    prevLyricsLine = newPrev;
    currentLyrics = newCur;
    currentLyricsLine2 = newNext;

    if (currentState == STATE_SPOTIFY) {
      if (activeTheme == 1) t2_redrawSpotifyPartial();
      else redrawSpotifyPartial();
    }
  }
}

// ═══════════════════════════════════════════════════════════
// 3. FETCH LYRICS FROM LRCLIB (Exact 1:1 API with Python)
// ═══════════════════════════════════════════════════════════
void fetchSpotifyLyrics(const String& trackName, const String& artistName) {
  if (WiFi.status() != WL_CONNECTED || trackName.length() == 0) return;

  Serial.println(F("[Spotify Direct] Fetching lyrics from LRCLIB..."));
  directLyricCount = 0;
  lastLyricKey = "";

  WiFiClientSecure client;
  client.setInsecure();
  client.setBufferSizes(1024, 512);
  client.setTimeout(4000);

  if (!client.connect("lrclib.net", 443)) {
    Serial.println(F("[Spotify Direct] Failed to connect to lrclib.net"));
    currentLyrics = "♪ Lyrics unavailable ♪";
    currentLyricsLine2 = "";
    prevLyricsLine = "";
    return;
  }

  String path = "/api/get?track_name=" + spotifyUrlEncode(trackName) + "&artist_name=" + spotifyUrlEncode(artistName);

  client.print("GET " + path + " HTTP/1.0\r\n");
  client.print(F("Host: lrclib.net\r\n"));
  client.print(F("User-Agent: CompanionOS/1.0 (ESP8266)\r\n"));
  client.print(F("Connection: close\r\n\r\n"));

  // Skip HTTP headers
  bool headersEnd = false;
  unsigned long timeout = millis() + 4000;
  while (client.connected() && millis() < timeout) {
    String line = client.readStringUntil('\n');
    if (line == "\r" || line.length() == 0) {
      headersEnd = true;
      break;
    }
    yield();
  }

  if (!headersEnd) {
    client.stop();
    return;
  }

  // Filter JSON directly from stream to save heap
  StaticJsonDocument<256> filter;
  filter["syncedLyrics"] = true;
  filter["plainLyrics"] = true;

  DynamicJsonDocument doc(2048);
  DeserializationError err = deserializeJson(doc, client, DeserializationOption::Filter(filter));
  client.stop();

  if (!err) {
    if (doc.containsKey("syncedLyrics") && !doc["syncedLyrics"].isNull()) {
      const char* raw = doc["syncedLyrics"];
      if (raw && strlen(raw) > 0) {
        // Stream parse lines: [mm:ss.xx] words
        const char* p = raw;
        while (*p && directLyricCount < DIRECT_MAX_LYRICS) {
          if (*p == '[') {
            p++;
            // Parse minutes
            int m = 0;
            while (*p >= '0' && *p <= '9') { m = m * 10 + (*p - '0'); p++; }
            if (*p == ':') p++;
            // Parse seconds
            int s = 0;
            while (*p >= '0' && *p <= '9') { s = s * 10 + (*p - '0'); p++; }
            // Parse fraction
            int frac = 0;
            if (*p == '.') {
              p++;
              int fracDigits = 0;
              while (*p >= '0' && *p <= '9') {
                if (fracDigits < 3) frac = frac * 10 + (*p - '0');
                fracDigits++;
                p++;
              }
              if (fracDigits == 1) frac *= 100;
              else if (fracDigits == 2) frac *= 10;
            }
            while (*p && *p != ']') p++;
            if (*p == ']') p++;
            while (*p == ' ' || *p == '\t') p++;

            // Extract words until newline
            int wIdx = 0;
            while (*p && *p != '\r' && *p != '\n' && wIdx < 47) {
              directLyrics[directLyricCount].words[wIdx++] = *p++;
            }
            directLyrics[directLyricCount].words[wIdx] = '\0';
            directLyrics[directLyricCount].timeMs = (uint32_t)m * 60000UL + (uint32_t)s * 1000UL + (uint32_t)frac;

            if (wIdx > 0) {
              directLyricCount++;
            }
          }
          // Move to next line
          while (*p && *p != '\n') p++;
          if (*p == '\n') p++;
          yield();
        }
        Serial.printf("[Spotify Direct] Parsed %d synced lyric lines\n", directLyricCount);
      }
    } else if (doc.containsKey("plainLyrics") && !doc["plainLyrics"].isNull()) {
      directLyricCount = 0;
      prevLyricsLine = "";
      currentLyrics = "♪ Unsynced lyrics available ♪";
      currentLyricsLine2 = "";
      return;
    }
  }

  if (directLyricCount == 0) {
    prevLyricsLine = "";
    currentLyrics = "♪ Instrumental ♪";
    currentLyricsLine2 = "";
  } else {
    updateSyncedLyrics();
  }
}

// ═══════════════════════════════════════════════════════════
// 4. FETCH ALBUM ART & STREAM DECODE INTO albumArt[9216]
// ═══════════════════════════════════════════════════════════
void fetchSpotifyAlbumArt(const String& artUrl) {
  if (WiFi.status() != WL_CONNECTED || artUrl.length() == 0) return;

  Serial.println(F("[Spotify Direct] Downloading album art JPEG..."));

  // Extract host and path from URL (e.g., https://i.scdn.co/image/...)
  String host = "i.scdn.co";
  String path = artUrl;
  if (path.startsWith("https://")) {
    path = path.substring(8);
    int slashIdx = path.indexOf('/');
    if (slashIdx > 0) {
      host = path.substring(0, slashIdx);
      path = path.substring(slashIdx);
    }
  } else if (path.startsWith("http://")) {
    path = path.substring(7);
    int slashIdx = path.indexOf('/');
    if (slashIdx > 0) {
      host = path.substring(0, slashIdx);
      path = path.substring(slashIdx);
    }
  }

  WiFiClientSecure client;
  client.setInsecure();
  client.setBufferSizes(1024, 512);
  client.setTimeout(4000);

  if (!client.connect(host.c_str(), 443)) {
    Serial.println(F("[Spotify Direct] Failed to connect to image host"));
    return;
  }

  client.print("GET " + path + " HTTP/1.0\r\n");
  client.print("Host: " + host + "\r\n");
  client.print(F("User-Agent: ESP8266-CompanionOS\r\n"));
  client.print(F("Connection: close\r\n\r\n"));

  // Skip HTTP headers
  int contentLength = 0;
  bool headersEnd = false;
  unsigned long timeout = millis() + 4000;
  while (client.connected() && millis() < timeout) {
    String line = client.readStringUntil('\n');
    if (line.startsWith("Content-Length: ") || line.startsWith("content-length: ")) {
      contentLength = line.substring(16).toInt();
    }
    if (line == "\r" || line.length() == 0) {
      headersEnd = true;
      break;
    }
    yield();
  }

  if (!headersEnd) {
    client.stop();
    return;
  }

  // Download JPEG payload into static buffer (64x64 is ~1.5 - 3.5 KB)
  static uint8_t jpgBuffer[5120];
  int totalBytes = 0;
  timeout = millis() + 4000;

  while (client.connected() && millis() < timeout && totalBytes < (int)sizeof(jpgBuffer)) {
    while (client.available() && totalBytes < (int)sizeof(jpgBuffer)) {
      jpgBuffer[totalBytes++] = client.read();
    }
    yield();
  }
  client.stop();

  if (totalBytes < 100) {
    Serial.println(F("[Spotify Direct] JPEG download failed or incomplete"));
    return;
  }

  Serial.printf("[Spotify Direct] Decoding %d byte JPEG to 96x96...\n", totalBytes);

  // Setup TJpg_Decoder
  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(spotifyTjpgCallback);

  // First decode directly into the first 64*64 words of albumArt
  JRESULT jres = TJpgDec.drawJpg(0, 0, jpgBuffer, totalBytes);
  if (jres == JDR_OK) {
    // Upscale in-place backwards from 64x64 to full 96x96
    // Since inY * 64 + inX <= y * 96 + x, data is never overwritten before reading!
    for (int y = 95; y >= 0; y--) {
      int inY = (y * 64) / 96;
      for (int x = 95; x >= 0; x--) {
        int inX = (x * 64) / 96;
        albumArt[y * 96 + x] = albumArt[inY * 64 + inX];
      }
    }

    albumArtReady = true;
    artDrawn = false;
    t2s_artDrawn = false;
    Serial.println(F("[Spotify Direct] Album art ready and upscaled to 96x96!"));

    if (currentState == STATE_SPOTIFY) {
      if (activeTheme == 1) t2_redrawSpotifyPartial();
      else redrawSpotifyPartial();
    }
  } else {
    Serial.printf("[Spotify Direct] TJpgDec error code: %d\n", jres);
  }
}

// ═══════════════════════════════════════════════════════════
// 5. POLL SPOTIFY PLAYER STATE (/v1/me/player)
// ═══════════════════════════════════════════════════════════
void pollSpotifyPlayer() {
  if (WiFi.status() != WL_CONNECTED || spotifyAccessToken.length() == 0) return;

  WiFiClientSecure client;
  client.setInsecure();
  client.setBufferSizes(1024, 512);
  client.setTimeout(3500);

  if (!client.connect("api.spotify.com", 443)) return;

  client.print(F("GET /v1/me/player HTTP/1.0\r\n"));
  client.print(F("Host: api.spotify.com\r\n"));
  client.print(F("User-Agent: ESP8266-CompanionOS\r\n"));
  client.print("Authorization: Bearer " + spotifyAccessToken + "\r\n");
  client.print(F("Connection: close\r\n\r\n"));

  // Check HTTP response code
  String statusLine = client.readStringUntil('\n');
  if (statusLine.indexOf("204") > 0) {
    // 204 No Content: Player inactive
    isPlaying = false;
    musicPlaying = false;
    client.stop();
    return;
  }
  if (statusLine.indexOf("401") > 0) {
    // Token expired, force refresh next time
    spotifyAccessToken = "";
    client.stop();
    return;
  }

  // Skip remaining headers
  bool headersEnd = false;
  unsigned long timeout = millis() + 3000;
  while (client.connected() && millis() < timeout) {
    String line = client.readStringUntil('\n');
    if (line == "\r" || line.length() == 0) {
      headersEnd = true;
      break;
    }
    yield();
  }

  if (!headersEnd) {
    client.stop();
    return;
  }

  // Filter JSON to only essential fields
  StaticJsonDocument<384> filter;
  filter["is_playing"] = true;
  filter["progress_ms"] = true;
  filter["shuffle_state"] = true;
  filter["repeat_state"] = true;
  filter["device"]["volume_percent"] = true;
  filter["item"]["name"] = true;
  filter["item"]["id"] = true;
  filter["item"]["duration_ms"] = true;
  filter["item"]["artists"][0]["name"] = true;
  filter["item"]["album"]["images"][2]["url"] = true; // 64x64 thumbnail
  filter["item"]["album"]["images"][1]["url"] = true; // 300x300 fallback

  DynamicJsonDocument doc(1536);
  DeserializationError err = deserializeJson(doc, client, DeserializationOption::Filter(filter));
  client.stop();

  if (err) return;

  bool playing = doc["is_playing"] | false;
  long progress = doc["progress_ms"] | 0;
  const char* tName = doc["item"]["name"];
  const char* tId = doc["item"]["id"];
  long dur = doc["item"]["duration_ms"] | 0;
  const char* aName = doc["item"]["artists"][0]["name"];

  // Pick 64x64 or 300x300 art URL
  const char* imgUrl = doc["item"]["album"]["images"][2]["url"];
  if (!imgUrl) imgUrl = doc["item"]["album"]["images"][1]["url"];

  t2_shuffle = doc["shuffle_state"] | false;
  const char* rep = doc["repeat_state"] | "off";
  if (strcmp(rep, "track") == 0) t2_repeat = 2;
  else if (strcmp(rep, "context") == 0) t2_repeat = 1;
  else t2_repeat = 0;

  if (doc["device"].containsKey("volume_percent")) {
    t2_volume = doc["device"]["volume_percent"].as<uint8_t>();
  }

  if (tId && strlen(tId) > 0) {
    if (strcmp(tId, lastSpotifyTrackId) != 0) {
      // New track detected!
      strncpy(lastSpotifyTrackId, tId, sizeof(lastSpotifyTrackId) - 1);
      currentTrack = tName ? tName : "";
      currentArtist = aName ? aName : "";
      playDuration = (int)dur;
      playProgress = (int)progress;
      isPlaying = playing;
      musicPlaying = isPlaying;

      Serial.printf("[Spotify Direct] Track: %s by %s\n", currentTrack.c_str(), currentArtist.c_str());

      // Fetch new lyrics
      fetchSpotifyLyrics(currentTrack, currentArtist);

      // Fetch new album art
      if (imgUrl) {
        spotifyArtUrl = String(imgUrl);
        fetchSpotifyAlbumArt(spotifyArtUrl);
      }

      if (currentState == STATE_SPOTIFY) {
        if (activeTheme == 1) t2_redrawSpotifyPartial();
        else redrawSpotifyPartial();
      }
    } else {
      // Same track: update progress & status
      isPlaying = playing;
      musicPlaying = isPlaying;
      playProgress = (int)progress;
      updateSyncedLyrics();

      if (currentState == STATE_SPOTIFY) {
        if (activeTheme == 1) t2_redrawSpotifyPartial();
        else redrawSpotifyPartial();
      }
    }
  }
}

// ═══════════════════════════════════════════════════════════
// 6. DIRECT TOUCH & GESTURE PLAYBACK CONTROLS
// ═══════════════════════════════════════════════════════════
void spotifyDirectControl(String cmd) {
  if (WiFi.status() != WL_CONNECTED || spotifyAccessToken.length() == 0) return;

  WiFiClientSecure client;
  client.setInsecure();
  client.setBufferSizes(512, 512);
  client.setTimeout(2500);

  if (!client.connect("api.spotify.com", 443)) return;

  String method = "POST";
  String path = "";

  if (cmd == "PLAY_PAUSE") {
    method = "PUT";
    if (isPlaying) {
      path = "/v1/me/player/pause";
      isPlaying = false;
      musicPlaying = false;
    } else {
      path = "/v1/me/player/play";
      isPlaying = true;
      musicPlaying = true;
    }
  } else if (cmd == "NEXT") {
    method = "POST";
    path = "/v1/me/player/next";
  } else if (cmd == "PREV") {
    method = "POST";
    path = "/v1/me/player/previous";
  } else if (cmd.startsWith("SEEK:")) {
    int pos = cmd.substring(5).toInt();
    method = "PUT";
    path = "/v1/me/player/seek?position_ms=" + String(pos);
    playProgress = pos;
  } else if (cmd == "SHUFFLE:TOGGLE") {
    t2_shuffle = !t2_shuffle;
    method = "PUT";
    path = "/v1/me/player/shuffle?state=" + String(t2_shuffle ? "true" : "false");
  } else if (cmd == "REPEAT:TOGGLE") {
    t2_repeat = (t2_repeat + 1) % 3;
    const char* repStates[] = {"off", "context", "track"};
    method = "PUT";
    path = "/v1/me/player/repeat?state=" + String(repStates[t2_repeat]);
  } else if (cmd.startsWith("VOLUME:")) {
    int v = cmd.substring(7).toInt();
    t2_volume = v;
    method = "PUT";
    path = "/v1/me/player/volume?volume_percent=" + String(v);
  } else if (cmd == "LIKE:TOGGLE" && strlen(lastSpotifyTrackId) > 0) {
    // Direct library toggle
    method = "PUT";
    path = "/v1/me/tracks?ids=" + String(lastSpotifyTrackId);
  }

  if (path.length() > 0) {
    client.print(method + " " + path + " HTTP/1.1\r\n");
    client.print(F("Host: api.spotify.com\r\n"));
    client.print(F("User-Agent: ESP8266-CompanionOS\r\n"));
    client.print("Authorization: Bearer " + spotifyAccessToken + "\r\n");
    client.print(F("Content-Length: 0\r\n"));
    client.print(F("Connection: close\r\n\r\n"));

    // Redraw immediately for instant visual feedback
    if (currentState == STATE_SPOTIFY) {
      if (activeTheme == 1) t2_redrawSpotifyPartial();
      else redrawSpotifyPartial();
    }
    // Force immediate sync on next loop
    lastSpotifyPoll = 0;
  }
  client.stop();
}

// ═══════════════════════════════════════════════════════════
// 7. SETUP & MAIN LOOP HOOKS
// ═══════════════════════════════════════════════════════════
void setupSpotifyDirect() {
  lastSpotifyTrackId[0] = 0;
  directLyricCount = 0;
}

void handleSpotifyDirect() {
  if (WiFi.status() != WL_CONNECTED) return;

  unsigned long now = millis();

  // 1. Token validation / refresh
  if ((spotifyAccessToken.length() == 0 || now >= spotifyTokenExpiresAt) && !spotifyTokenRefreshing) {
    refreshSpotifyToken();
  }

  // 2. Local progress increment (1 second resolution between network polls)
  if (isPlaying && now - lastLocalProgressTick >= 1000) {
    lastLocalProgressTick = now;
    playProgress += 1000;
    if (playDuration > 0 && playProgress > playDuration) playProgress = playDuration;

    updateSyncedLyrics();

    if (currentState == STATE_SPOTIFY) {
      if (activeTheme == 1) t2_redrawSpotifyPartial();
      else redrawSpotifyPartial();
    }
  }

  // 3. Periodic API polling
  unsigned long pollInterval = (currentState == STATE_SPOTIFY) ? 2500 : (isPlaying ? 6000 : 12000);
  if (now - lastSpotifyPoll >= pollInterval) {
    lastSpotifyPoll = now;
    pollSpotifyPlayer();
  }
}

#endif
