// ═══════════════════════════════════════════════════════════
// COMPANION OS v7.0 — NETWORK FUNCTIONS
// Dual-platform: ESP32 (WiFi + BT) / ESP8266 (WiFi only)
// ═══════════════════════════════════════════════════════════
#ifndef COMPANION_NET_H
#define COMPANION_NET_H
#include "lyrics_state.h"

// [LEGACY - v6.0] Original included ESP8266WiFi.h unconditionally.
// Now uses platform-conditional includes.

#ifdef ESP32
#include <WiFi.h>
#if HAS_BLUETOOTH
#include <BluetoothSerial.h>
#endif
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#endif

#include "globals.h"
#include <utility>
#include <WiFiUdp.h>
#include <NTPClient.h>
#ifdef ESP32
  #include <WebServer.h>
#else
  #include <ESP8266WebServer.h>
#endif
#include <ArduinoJson.h>

#include "eyes.h"

// ═══════════════════════════════════════════════════════════
// STATE & FORWARD DECLARATIONS
// ═══════════════════════════════════════════════════════════

extern char udpBuffer[2048];
String pcIPStr = DEFAULT_PC_IP;

// System Web Server for Direct Memory Uploads
#ifdef ESP32
  static WebServer sysWebServer(8080);
#else
  static ESP8266WebServer sysWebServer(8080);
#endif
static bool memoryUploadValid = false;
static const char MEMORIES_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><title>CompanionOS Memories</title>
<meta name="viewport" content="width=device-width,initial-scale=1">
<style>
body{background:#111;color:#fff;font-family:sans-serif;text-align:center;padding:20px;margin:0;}
.card{background:#222;padding:20px;border-radius:10px;display:inline-block;max-width:400px;width:100%;}
button{background:#05f;color:#fff;border:none;padding:12px 20px;border-radius:6px;font-size:16px;cursor:pointer;margin-top:15px;width:100%;}
input{margin-top:15px;color:#aaa;}
#status{margin-top:15px;font-weight:bold;color:#0f0;}
</style></head><body>
<div class="card"><h2>Upload Memory</h2>
<p style="color:#aaa;font-size:14px;line-height:1.4;">Select a photo to directly display it on CompanionOS. Your device will automatically resize and convert it before sending.</p>
<input type="file" id="file" accept="image/*"><br>
<button onclick="upload()">Send to Screen</button>
<div id="status"></div>
<canvas id="cvs" style="display:none;"></canvas>
</div>
<script>
function upload() {
  const file = document.getElementById('file').files[0];
  if (!file) { document.getElementById('status').innerText = 'Please select a file.'; return; }
  document.getElementById('status').innerText = 'Processing...';
  const img = new Image();
  img.onload = () => {
    const cvs = document.getElementById('cvs');
    cvs.width = 160; cvs.height = 128;
    const ctx = cvs.getContext('2d');
    const scale = Math.max(160/img.width, 128/img.height);
    const nw = img.width * scale;
    const nh = img.height * scale;
    const nx = (160 - nw) / 2;
    const ny = (128 - nh) / 2;
    ctx.drawImage(img, nx, ny, nw, nh);
    const imgData = ctx.getImageData(0,0,160,128).data;
    const buf = new Uint8Array(160*128*2);
    let j=0;
    for(let i=0; i<imgData.length; i+=4) {
      const rgb565 = ((imgData[i]>>3)<<11) | ((imgData[i+1]>>2)<<5) | (imgData[i+2]>>3);
      buf[j++] = rgb565 & 0xFF;
      buf[j++] = (rgb565 >> 8) & 0xFF;
    }
    const blob = new Blob([buf], { type: 'application/octet-stream' });
    const fd = new FormData();
    fd.append('file', blob, 'memory.bin');
    document.getElementById('status').innerText = 'Uploading...';
    fetch('/upload', { method: 'POST', body: fd }).then(res => {
      document.getElementById('status').innerText = 'Success!';
    }).catch(err => {
      document.getElementById('status').innerText = 'Failed!';
    });
  };
  img.src = URL.createObjectURL(file);
}
</script></body></html>
)rawliteral";

// NTP Time Sync — IST = UTC + 5:30 = 19800 seconds
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_SERVER, NTP_OFFSET, 300000);  // sync every 5 min
unsigned long lastNTPSync = 0;

// Track data
String currentTrack = "";
String currentArtist = "";
extern String currentLyrics;
extern String currentLyricsLine2;
extern String prevLyricsLine;
int playProgress = 0;
int playDuration = 0;
unsigned long spotifyProgressUpdatedAt = 0;
bool isPlaying = false;
extern bool artDrawn;
extern bool prepareCustomEyeImage();

String currentNotes[4] = { "", "", "", "" };

// ESP32 Bluetooth Serial
#ifdef ESP32
#if HAS_BLUETOOTH
BluetoothSerial btSerial;
#endif
bool btInitialized = false;
#endif

// Forward declarations for page redraw functions
extern void redrawSpotifyPartial();
extern void redrawNotesPartial();
extern void redrawWeatherPartial();
extern void redrawPomodoroPartial();
extern void redrawNotificationsPartial();
extern void redrawSettingsPartial();
extern void redrawStocksPartial();
extern void redrawGamingPartial();
extern void redrawSocialPartial();
extern void redrawProductivityPartial();
extern void redrawClockDashboardPartial();
extern void redrawRetroWatchPartial();
extern void drawStatusBar();
extern void processArtChunk(int chunkIdx, String hexData);
extern void completeAlbumArt();
extern void showFlashNotification(String text);
extern void renderCurrentPage();
extern void changePage(int direction);
extern void setEmotion(Emotion e);
extern void t2_nextExpression();
extern void t2_setEmotion(Emotion e);
extern void t3_nextExpression();
extern void t3_setEmotion(Emotion e);
extern void nextPcDashSubPage();
extern void resetSpotifyDrawState();
extern void t2_resetSpotifyDrawState();
extern bool t2_initialized;
extern bool flashNotifEnabled;

// Safe string extraction to prevent nullptr crash
#define SAFESTR(dest, src, sz) \
  do { \
    const char* _s = (src).as<const char*>(); \
    if (_s) { \
      strncpy(dest, _s, sz - 1); \
      dest[sz - 1] = '\0'; \
    } \
  } while (0)

// Weather data (defined in pages.h, declared here)
extern String weatherCondition;
extern int weatherTemp;
extern int weatherFeels;
extern int weatherHumidity;
extern int weatherWind;
extern int weatherHigh;
extern int weatherLow;
extern String weatherCity;
extern String weatherSunrise;
extern String weatherSunset;
extern int weatherCode;

// Pomodoro data
extern int pomoRemaining;
extern int pomoTotal;
extern bool pomoIsBreak;
extern int pomoSessions;
extern bool pomoActive;

// Notification data
extern String notifApps[3];
extern String notifTitles[3];
extern String notifTimes[3];
extern int notifTotal;

void handleCommand(String msg);
void sendCommand(String cmd);
bool isHostActive();
bool isHostPlaybackActive();
bool isHostWeatherActive();
bool isHostPomodoroActive();
void tickWiFiConnectivity();
bool queueStandaloneCommand(const String& cmd);
unsigned long pomoClockBaseline = 0;

void handleVirtualButton(String btn) {
  extern bool dismissEyesOverlays();
  extern bool plannerButton(int step);
  btn.trim();
  btn.toUpperCase();
  if (btn == "DISMISS") { dismissEyesOverlays(); return; }
  if ((btn == "UP" || btn == "DOWN" || btn == "SELECT") && plannerButton(btn == "UP" ? -1 : 1)) return;
  if (btn == "SELECT_LONG" && currentState == STATE_EYES && dismissEyesOverlays()) return;

  if (btn == "HOME") {
    virtualHomePressed = true;
    if (currentState != STATE_EYES) {
      changePage(-(int)currentState);
    }
    lastInteractionTime = millis();
    return;
  }

  if (btn == "LEFT_LONG") {
    if (currentState == STATE_SPOTIFY || currentState == STATE_LYRICS) sendCommand("PREV");
    else if (currentState != STATE_EYES) changePage(-(int)currentState);
    lastInteractionTime = millis();
    return;
  }

  if (btn == "LEFT") {
    virtualLeftPressed = true;
#ifdef ESP32
    if (currentState == STATE_DR_HACK) {
      extern void dhNavigate(int delta);
      extern DrHackSubState dhCurrentState;
      if (dhCurrentState == DH_MENU || dhUsesPageInput(dhCurrentState)) {
        virtualLeftPressed = false;
        dhNavigate(-1);
      }
      lastInteractionTime = millis();
      return;
    }
#endif

    changePage(-1);
    lastInteractionTime = millis();
    return;
  }

  if (btn == "RIGHT_LONG") {
    if (currentState == STATE_SPOTIFY || currentState == STATE_LYRICS) sendCommand("NEXT");
    else if (currentState != STATE_SETTINGS) changePage(STATE_SETTINGS - (int)currentState);
    lastInteractionTime = millis();
    return;
  }

  if (btn == "RIGHT") {
    virtualRightPressed = true;
#ifdef ESP32
    if (currentState == STATE_DR_HACK) {
      extern void dhNavigate(int delta);
      extern DrHackSubState dhCurrentState;
      if (dhCurrentState == DH_MENU || dhUsesPageInput(dhCurrentState)) {
        virtualRightPressed = false;
        dhNavigate(1);
      }
      lastInteractionTime = millis();
      return;
    }
#endif

    changePage(1);
    lastInteractionTime = millis();
    return;
  }

  if (btn == "UP") {
    virtualUpPressed = true;
    if (currentState == STATE_SETTINGS) {
      extern int settingsScrollY;
      settingsScrollY = max(0, settingsScrollY - 20);
      redrawSettingsPartial();
    } else {
      changePage(-1);
    }
    lastInteractionTime = millis();
    return;
  }

  if (btn == "DOWN") {
    virtualDownPressed = true;
    if (currentState == STATE_SETTINGS) {
      extern int settingsScrollY;
      settingsScrollY = min(120, settingsScrollY + 20);
      redrawSettingsPartial();
    } else {
      changePage(1);
    }
    lastInteractionTime = millis();
    return;
  }

  if (btn == "SELECT_LONG") {
#ifdef ESP32
    if (currentState == STATE_DR_HACK) {
      extern void dhSelect();
      extern DrHackSubState dhCurrentState;
      if (dhCurrentState == DH_MENU) {
        changePage(-(int)currentState);
      } else if (dhCurrentState == DH_BLE_INSPECTOR) {
        dhCurrentState = DH_MENU;
        renderCurrentPage();
      } else if (dhUsesPageInput(dhCurrentState)) {
        virtualSelectPressed = false;
        dhSelect();
      } else {
        extern unsigned long virtualSelectHoldUntil;
        virtualSelectPressed = true;
        virtualSelectHoldUntil = millis() + 850;
      }
    } else
#endif
    if (currentState == STATE_EYES) {
      Emotion next = (Emotion)((currentEmotion + 1) % EMO_COUNT);
      if (activeTheme == 2) t3_setEmotion(next);
      else if (activeTheme == 1) t2_setEmotion(next);
      else setEmotion(next);
    } else if (currentState == STATE_SPOTIFY || currentState == STATE_LYRICS) sendCommand("SHUFFLE:TOGGLE");
    else if (currentState == STATE_POMODORO) sendCommand("POMO:RESET");
    else changePage(-(int)currentState);
    lastInteractionTime = millis();
    return;
  }

  if (btn == "SELECT") {
    virtualSelectPressed = true;
#ifdef ESP32
    if (currentState == STATE_DR_HACK) {
      extern void dhSelect();
      extern DrHackSubState dhCurrentState;
      if (dhCurrentState == DH_MENU) dhSelect();
      else if (dhUsesPageInput(dhCurrentState)) { virtualSelectPressed = false; dhSelect(); }
      else virtualSelectPressed = true;
      lastInteractionTime = millis();
      return;
    }
#endif

    if (currentState == STATE_EYES) {
      if (customEyeActive) { customEyeActive = false; renderCurrentPage(); }
      else if (activeTheme == 2) t3_nextExpression();
      else if (activeTheme == 1) t2_nextExpression();
      else setEmotion((Emotion)((currentEmotion + 1) % EMO_COUNT));
    } else if (currentState == STATE_SPOTIFY || currentState == STATE_LYRICS) {
      sendCommand("PLAY_PAUSE");
    } else if (currentState == STATE_POMODORO) {
      sendCommand(pomoActive ? "POMO:PAUSE" : "POMO:START");
    } else if (currentState == STATE_NOTIFICATIONS) {
      flashNotifEnabled = !flashNotifEnabled;
      redrawNotificationsPartial();
    } else if (currentState == STATE_PC_DASH) {
      nextPcDashSubPage();
    } else if (currentState == STATE_SETTINGS) {
      activeTheme = (activeTheme + 1) % THEME_COUNT;
      EEPROM.begin(EEPROM_SIZE);
      EEPROM.write(EEPROM_ACTIVE_THEME_ADDR, activeTheme);
      EEPROM.commit();
      EEPROM.end();
      resetSpotifyDrawState(); t2_resetSpotifyDrawState(); t2_initialized = false;
      renderCurrentPage();
    }
    lastInteractionTime = millis();
    return;
  }

}

#include "wifi_connectivity.h"

// ═══════════════════════════════════════════════════════════
// ESP32 BLUETOOTH TRANSPORT
// ═══════════════════════════════════════════════════════════

#ifdef ESP32
#if HAS_BLUETOOTH
void setupBluetooth() {
  if (!btInitialized) {
    // 1. Completely disconnect and power down WiFi radio to prevent coexistence panic
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    wifiConnected = false;
    delay(100);
    yield();

    // 2. Prevent crash if memory is too fragmented
    if (ESP.getFreeHeap() < 40000) {
      Serial.printf("Low RAM (%u bytes), skipping BT to prevent crash\n", ESP.getFreeHeap());
      return;
    }

    // 3. Start Bluetooth Classic SPP
    btSerial.begin("CompanionOS");
    btInitialized = true;
    Serial.println(F("BT Serial started safely in Offline Mode as 'CompanionOS'"));
  }
}

void startBluetoothExplicit() {
  setupBluetooth();
}

static char btLineBuf[256];
static int btLineIdx = 0;

void handleBluetoothData() {
  if (!btInitialized) return;

  while (btSerial.available() > 0) {
    int c = btSerial.read();
    if (c < 0) break;
    
    if (c == '\n' || c == '\r') {
      if (btLineIdx > 0) {
        btLineBuf[btLineIdx] = '\0';
        String msg = String(btLineBuf);
        msg.trim();
        btLineIdx = 0;
        if (msg.length() > 0) {
          extern unsigned long lastHostPacketMillis;
          lastHostPacketMillis = millis();
          handleCommand(msg);
        }
      }
    } else {
      if (btLineIdx < (int)sizeof(btLineBuf) - 1) {
        btLineBuf[btLineIdx++] = (char)c;
      } else {
        btLineIdx = 0; // Prevent buffer overrun
      }
    }
  }
}
#else
void setupBluetooth() {}
void startBluetoothExplicit() {}
void handleBluetoothData() {}
#endif // HAS_BLUETOOTH
#endif // ESP32

// ═══════════════════════════════════════════════════════════
// SEND COMMAND — Dual transport (WiFi preferred, BT fallback)
// ═══════════════════════════════════════════════════════════

void sendCommand(String cmd) {
  if (!isHostPomodoroActive() && cmd.startsWith("POMO:")) {
    if (cmd == "POMO:RESET") {
      pomoActive = false;
      pomoIsBreak = false;
      pomoRemaining = pomoTotal = 1500;
    } else if (cmd == "POMO:START") {
      if (pomoRemaining <= 0) pomoRemaining = max(1, pomoTotal);
      pomoActive = true;
    } else if (cmd == "POMO:PAUSE") {
      pomoActive = false;
    } else return;
    pomoClockBaseline = millis();
    redrawPomodoroPartial();
    return;
  }
  #ifdef ESP32
  if (queueStandaloneCommand(cmd)) return;
  #endif
  // Match legacy: use pcIPStr.c_str() directly
  udp.beginPacket(pcIPStr.c_str(), UDP_PORT_TX);
  udp.write((const uint8_t*)cmd.c_str(), cmd.length());
  udp.endPacket();
#ifdef ESP32
  #if HAS_BLUETOOTH
  if (btConnected && btInitialized) {
    btSerial.println(cmd);
  }
#endif
#endif
  Serial.print(F("Sent: "));
  Serial.println(cmd);
}

// ═══════════════════════════════════════════════════════════
// STANDALONE HOST PRESENCE & DIRECT APIS (SPOTIFY & WEATHER)
// ═══════════════════════════════════════════════════════════

unsigned long lastHostPacketMillis = 0;
unsigned long lastHostPlaybackMillis = 0;
bool isHostAndroid = false;

bool isHostActive() {
  return (lastHostPacketMillis > 0 && (millis() - lastHostPacketMillis < HOST_TIMEOUT_MS));
}

bool isHostPlaybackActive() {
  return lastHostPlaybackMillis > 0 && millis() - lastHostPlaybackMillis < HOST_TIMEOUT_MS;
}

unsigned long lastHostWeatherMillis = 0;
unsigned long lastHostPomodoroMillis = 0;

bool isHostWeatherActive() {
  return lastHostWeatherMillis > 0 && millis() - lastHostWeatherMillis < WEATHER_POLL_INTERVAL_MS;
}

bool isHostPomodoroActive() {
  return isHostActive() && lastHostPomodoroMillis > 0 && (millis() - lastHostPomodoroMillis < HOST_TIMEOUT_MS);
}


// Standalone Spotify state
String spotifyAccessToken = DEFAULT_SPOTIFY_ACCESS_TOKEN;
String spotifyRefreshToken = DEFAULT_SPOTIFY_REFRESH_TOKEN;
String spotifyClientId = SPOTIFY_CLIENT_ID;
String spotifyClientSecret = SPOTIFY_CLIENT_SECRET;
unsigned long lastSpotifyPollMillis = 0;
unsigned long lastWeatherPollMillis = 0;

static inline String encodeBase64(const String& input) {
  static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  String out = "";
  int len = input.length();
  for (int i = 0; i < len; i += 3) {
    uint32_t octet_a = (uint8_t)input[i];
    uint32_t octet_b = (i + 1 < len) ? (uint8_t)input[i + 1] : 0;
    uint32_t octet_c = (i + 2 < len) ? (uint8_t)input[i + 2] : 0;
    uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;
    out += b64_table[(triple >> 18) & 0x3F];
    out += b64_table[(triple >> 12) & 0x3F];
    out += (i + 1 < len) ? b64_table[(triple >> 6) & 0x3F] : '=';
    out += (i + 2 < len) ? b64_table[triple & 0x3F] : '=';
  }
  return out;
}

#ifdef ESP32
#include "standalone_services.h"
#else
String standaloneSpotifyStatus;
String standaloneWeatherStatus;
String standaloneLyricsStatus;
bool standaloneLyricsSynced = true;
bool refreshSpotifyToken() {
  if (spotifyRefreshToken.length() == 0 || spotifyClientId.length() == 0 || spotifyClientSecret.length() == 0) {
    return false;
  }
#if defined(ESP32) || defined(ESP8266)
  WiFiClientSecure client;
  client.setInsecure();
  #ifdef ESP8266
  client.setBufferSizes(1024, 512);
  #endif

  HTTPClient https;
  if (!https.begin(client, "https://accounts.spotify.com/api/token")) {
    return false;
  }

  https.setTimeout(8000);
  https.addHeader("Content-Type", "application/x-www-form-urlencoded");
  String authBasic = encodeBase64(spotifyClientId + ":" + spotifyClientSecret);
  https.addHeader("Authorization", "Basic " + authBasic);

  String postBody = "grant_type=refresh_token&refresh_token=" + spotifyRefreshToken;
  int httpCode = https.POST(postBody);

  if (httpCode == 200) {
    String resp = https.getString();
    DynamicJsonDocument doc(1024);
    if (!deserializeJson(doc, resp)) {
      if (doc.containsKey("access_token")) {
        spotifyAccessToken = doc["access_token"].as<String>();
        Serial.println(F("Spotify standalone token refreshed!"));
        https.end();
        return true;
      }
    }
  } else {
    Serial.printf("Spotify refresh failed HTTP %d\n", httpCode);
  }
  https.end();
#endif
  return false;
}

void pollSpotifyStandalone() {
  if (!wifiConnected) return;
  if (isHostActive() && !isHostAndroid) return;

  unsigned long now = millis();
  unsigned long interval = (currentState == STATE_SPOTIFY || isPlaying) ? 
                           SPOTIFY_POLL_INTERVAL_ACTIVE_MS : SPOTIFY_POLL_INTERVAL_IDLE_MS;

  if (now - lastSpotifyPollMillis < interval) return;
  lastSpotifyPollMillis = now;

  if (spotifyAccessToken.length() == 0) {
    if (!refreshSpotifyToken()) return;
  }

#if defined(ESP32) || defined(ESP8266)
  WiFiClientSecure client;
  client.setInsecure();
  #ifdef ESP8266
  client.setBufferSizes(1024, 512);
  #endif

  HTTPClient https;
  if (!https.begin(client, "https://api.spotify.com/v1/me/player")) {
    return;
  }

  https.setTimeout(8000);
  https.addHeader("Authorization", "Bearer " + spotifyAccessToken);
  int httpCode = https.GET();

  if (httpCode == 200) {
    String payload = https.getString();
    DynamicJsonDocument doc(2048);
    if (!deserializeJson(doc, payload)) {
      bool playing = doc["is_playing"] | false;
      int prg = doc["progress_ms"] | 0;
      JsonObject item = doc["item"];
      if (!item.isNull()) {
        String trk = item["name"] | "";
        String art = "";
        if (item.containsKey("artists") && item["artists"].size() > 0) {
          art = item["artists"][0]["name"] | "";
        }
        int dur = item["duration_ms"] | 0;

        if (trk.length() > 0 && (trk != currentTrack || art != currentArtist)) {
          currentTrack = trk;
          currentArtist = art;
          currentLyrics = "";
          currentLyricsLine2 = "";
          prevLyricsLine = "";
        }
        playProgress = prg;
        playDuration = dur;
      }
      isPlaying = playing;
      musicPlaying = playing;

      extern void t2_redrawSpotifyPartial();
      if (activeTheme == 1 && currentState == STATE_SPOTIFY) t2_redrawSpotifyPartial();
      else if (currentState == STATE_SPOTIFY) redrawSpotifyPartial();
      else if (currentState == STATE_CLOCK_DASHBOARD) redrawClockDashboardPartial();
      else if (currentState == STATE_RETRO_WATCH) redrawRetroWatchPartial();
    }
  } else if (httpCode == 204) {
    if (isPlaying) {
      isPlaying = false;
      musicPlaying = false;
      extern void t2_redrawSpotifyPartial();
      if (activeTheme == 1 && currentState == STATE_SPOTIFY) t2_redrawSpotifyPartial();
      else if (currentState == STATE_SPOTIFY) redrawSpotifyPartial();
    }
  } else if (httpCode == 401) {
    Serial.println(F("Spotify token expired (401), refreshing..."));
    refreshSpotifyToken();
  }
  https.end();
#endif
}

void fetchWeatherStandalone() {
  if (!wifiConnected || isHostActive()) return;

  unsigned long now = millis();
  if (lastWeatherPollMillis > 0 && now - lastWeatherPollMillis < WEATHER_POLL_INTERVAL_MS) return;
  lastWeatherPollMillis = now;

#if defined(ESP32) || defined(ESP8266)
  WiFiClientSecure client;
  client.setInsecure();
  #ifdef ESP8266
  client.setBufferSizes(2048, 512);
  #endif

  HTTPClient https;
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(WEATHER_LATITUDE, 4) +
               "&longitude=" + String(WEATHER_LONGITUDE, 4) +
               "&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,wind_speed_10m" +
               "&daily=temperature_2m_max,temperature_2m_min,sunrise,sunset&timezone=auto";

  if (!https.begin(client, url)) {
    return;
  }

  https.setTimeout(8000);
  int httpCode = https.GET();

  if (httpCode == 200) {
    String payload = https.getString();
    DynamicJsonDocument doc(2048);
    if (!deserializeJson(doc, payload)) {
      JsonObject current = doc["current"];
      JsonObject daily = doc["daily"];

      if (!current.isNull()) {
        weatherTemp = (int)(current["temperature_2m"].as<float>() + 0.5f);
        weatherFeels = (int)(current["apparent_temperature"].as<float>() + 0.5f);
        weatherHumidity = current["relative_humidity_2m"] | 50;
        weatherWind = (int)(current["wind_speed_10m"].as<float>() + 0.5f);
        int wmo = current["weather_code"] | 0;

        // Map WMO code to condition & code (identical to WeatherRelay.kt)
        switch (wmo) {
          case 0:  weatherCondition = "Clear"; weatherCode = 800; break;
          case 1:
          case 2:
          case 3:  weatherCondition = "Partly Cloudy"; weatherCode = 802; break;
          case 45:
          case 48: weatherCondition = "Foggy"; weatherCode = 741; break;
          case 51: case 53: case 55:
          case 56: case 57: weatherCondition = "Drizzle"; weatherCode = 300; break;
          case 61: case 63: case 65:
          case 66: case 67: weatherCondition = "Rain"; weatherCode = 500; break;
          case 71: case 73: case 75:
          case 77: weatherCondition = "Snow"; weatherCode = 600; break;
          case 80: case 81: case 82: weatherCondition = "Showers"; weatherCode = 521; break;
          case 85: case 86: weatherCondition = "Snow Showers"; weatherCode = 621; break;
          case 95: case 96: case 99: weatherCondition = "Thunderstorm"; weatherCode = 200; break;
          default: weatherCondition = "Clouds"; weatherCode = 803; break;
        }
      }

      if (!daily.isNull()) {
        if (daily.containsKey("temperature_2m_max") && daily["temperature_2m_max"].size() > 0) {
          weatherHigh = (int)(daily["temperature_2m_max"][0].as<float>() + 0.5f);
        } else {
          weatherHigh = weatherTemp + 4;
        }
        if (daily.containsKey("temperature_2m_min") && daily["temperature_2m_min"].size() > 0) {
          weatherLow = (int)(daily["temperature_2m_min"][0].as<float>() + 0.5f);
        } else {
          weatherLow = weatherTemp - 4;
        }
        if (daily.containsKey("sunrise") && daily["sunrise"].size() > 0) {
          String sr = daily["sunrise"][0].as<String>();
          int tIdx = sr.indexOf('T');
          weatherSunrise = (tIdx > 0) ? sr.substring(tIdx + 1, tIdx + 6) : sr;
        }
        if (daily.containsKey("sunset") && daily["sunset"].size() > 0) {
          String ss = daily["sunset"][0].as<String>();
          int tIdx = ss.indexOf('T');
          weatherSunset = (tIdx > 0) ? ss.substring(tIdx + 1, tIdx + 6) : ss;
        }
      }

      weatherCity = WEATHER_CITY_NAME;
      Serial.printf("Standalone Weather: %s %d C (%s)\n", weatherCity.c_str(), weatherTemp, weatherCondition.c_str());
      redrawWeatherPartial();
    }
  } else {
    Serial.printf("Standalone Weather fetch failed HTTP %d\n", httpCode);
  }
  https.end();
#endif
}

// ═══════════════════════════════════════════════════════════
// HANDLE NETWORK — Main loop
// ═══════════════════════════════════════════════════════════

#endif  // ESP32 background worker / ESP8266 legacy fallback

// Network state trackers
// ═══════════════════════════════════════════════════════════

bool pcFound = false;
unsigned long lastDiscoveryShout = 0;

void handleNetwork() {
  if (!wifiServicesStarted) {
    #ifdef ESP32
    tickStandaloneServices();
    #endif
    return;
  }
  if (dhNetPaused) {
    #ifdef ESP32
    tickStandaloneServices();
    #endif
    return; // Pause background services during intense RF/WiFi Dr. Hack operations
  }

  if (wifiConnected) {
    sysWebServer.handleClient();
  }

  // Periodic NTP re-sync (every 5 minutes)
  #ifdef ESP32
  static unsigned long lastClockRead = 0;
  if (wifiConnected && millis() - lastClockRead >= 1000) {
    lastClockRead = millis();
    struct tm clock;
    if (getLocalTime(&clock, 0)) {
      displayHour = clock.tm_hour;
      displayMinute = clock.tm_min;
      displaySecond = clock.tm_sec;
      timeReceived = true;
      lastTimeUpdateMillis = millis();
    }
  }
  #else
  if (wifiConnected && millis() - lastNTPSync >= 300000) {
    if (timeClient.update()) {
      displayHour = timeClient.getHours();
      displayMinute = timeClient.getMinutes();
      displaySecond = timeClient.getSeconds();
      timeReceived = true;
      lastTimeUpdateMillis = millis();
      Serial.printf("NTP resync: %02d:%02d:%02d IST\n", displayHour, displayMinute, displaySecond);
    }
    lastNTPSync = millis();
  }
  #endif

  // Auto-discovery — match legacy: use string broadcast address
  if (wifiConnected && !pcFound && millis() - lastDiscoveryShout > 3000) {
    udp.beginPacket("255.255.255.255", UDP_PORT_TX);
    const char* hello = "HELLO_COMPANION";
    udp.write((const uint8_t*)hello, strlen(hello));
    udp.endPacket();
    lastDiscoveryShout = millis();
  }

  // Sustain virtual select long press for tools
  extern unsigned long virtualSelectHoldUntil;
  extern bool virtualSelectPressed;
  if (virtualSelectHoldUntil != 0) {
    if (millis() < virtualSelectHoldUntil) {
      virtualSelectPressed = true;
    } else {
      virtualSelectHoldUntil = 0;
    }
  }

  // PC Dashboard dedicated UDP port 4210 listener
  bool wantsPcDash = wifiConnected &&
      companion_memory::nearby(int(currentState), int(STATE_PC_DASH), int(STATE_COUNT));
  if (pcDashListening && !wantsPcDash) { udpPcDash.stop(); pcDashListening = false; }
  if (!pcDashListening && wantsPcDash) pcDashListening = udpPcDash.begin(4210) != 0;
  // Renewable demand lease: optional PC feeds stop if this device goes away.
  static int sentView = -1;
  static uint32_t lastViewSent = 0;
  if (wifiConnected && (sentView != int(currentState) || uint32_t(millis() - lastViewSent) >= 5000)) {
    udp.beginPacket(pcIPStr.c_str(), UDP_PORT_TX);
    udp.print(String("VIEW:") + String(int(currentState)));
    udp.endPacket();
    sentView = int(currentState); lastViewSent = millis();
  }
  int pcdPacketSize = pcDashListening ? udpPcDash.parsePacket() : 0;
  if (pcdPacketSize) {
    int pcdReadLen = min(pcdPacketSize, 2047);
    char pcdBuf[2048];
    int pcdLen = udpPcDash.read(pcdBuf, pcdReadLen);
    if (pcdLen > 0) {
      pcdBuf[pcdLen] = '\0';
      extern void processPcDashPacket(const char* packet);
      if (pcdLen == pcdPacketSize) processPcDashPacket(pcdBuf);
    }
  }

  // UDP packet processing
  int packetSize = udp.parsePacket();
  if (packetSize) {
    pcFound = true;
    lastHostPacketMillis = millis();
    IPAddress remoteIP = udp.remoteIP();
    pcIPStr = remoteIP.toString();

    int readLen = min(packetSize, 2047);
    int len = udp.read(udpBuffer, readLen);
    if (len <= 0) return;

    // V4 OPTIMIZATION: BARE-METAL BINARY PACKET SNIFFING
    if (len > 3 && (unsigned char)udpBuffer[0] == 0xFD) {
      // CUSTOM EYE IMAGE CHUNKS (160x128 RGB565)
      if (customEyeImg == nullptr) {
        if (!prepareCustomEyeImage()) return;
      }
      if (customEyeImg == nullptr) return; // Out of memory
      
      int chunkIdx = ((unsigned char)udpBuffer[1] << 8) | ((unsigned char)udpBuffer[2]);
      int pixelsInChunk = (len - 3) / 2;
      int imgWidth = 160;
      int imagePixels = 160 * 128;
      int startPixel = chunkIdx * imgWidth;
      
      if (startPixel >= 0 && startPixel < imagePixels) {
        uint16_t* dest = customEyeImg + startPixel;
        int safePixels = min(pixelsInChunk, imagePixels - startPixel);
        for (int i = 0; i < safePixels; i++) {
          int offset = 3 + (i * 2);
          dest[i] = ((uint16_t)(unsigned char)udpBuffer[offset] << 8) | (unsigned char)udpBuffer[offset + 1];
        }
        if (chunkIdx >= 0 && chunkIdx < 128) {
          customEyeRowsReceived[chunkIdx] = 1;
        }
      }
      return;
    }

    if (len > 3 && (unsigned char)udpBuffer[0] == 0xFE) {
      int chunkIdx = ((unsigned char)udpBuffer[1] << 8) | ((unsigned char)udpBuffer[2]);
      int pixelsInChunk = (len - 3) / 2;

      // Auto-initialize on first chunk if ART_START: was missed over UDP
      if (chunkIdx == 0 && !receivingArt) {
        if (!prepareIncomingArt()) return;
        receivingArt = true;
        albumArtReady = false;
        artDrawn = false;
        albumArtRowsComplete = 0;
        memset(albumArtRowsReceived, 0, sizeof(albumArtRowsReceived));
        albumArtStartedAt = millis();
      }
      if (!receivingArt || (len - 3) % (ALBUM_ART_W * 2) != 0) return;

      int imgWidth = ALBUM_ART_W;
      int imgHeight = ALBUM_ART_H;
      int imagePixels = imgWidth * imgHeight;
      int maxPixels = ALBUM_ART_W * ALBUM_ART_H;
      int pixelsPerChunk = imgWidth;
      int startPixel = chunkIdx * pixelsPerChunk;

      if (albumArtIncoming && startPixel >= 0 && startPixel < imagePixels && startPixel < maxPixels) {
        uint16_t* dest = albumArtIncoming + startPixel;
        int safePixels = min(pixelsInChunk, min(imagePixels - startPixel, maxPixels - startPixel));
        if (safePixels <= 0) return;

        for (int i = 0; i < safePixels; i++) {
          int offset = 3 + (i * 2);
          dest[i] = ((uint16_t)(unsigned char)udpBuffer[offset] << 8) | (unsigned char)udpBuffer[offset + 1];
        }

        int firstRow = startPixel / imgWidth;
        int rowCount = safePixels / imgWidth;
        for (int row = firstRow; row < firstRow + rowCount && row < ALBUM_ART_H; row++) {
          if (!albumArtRowsReceived[row]) {
            albumArtRowsReceived[row] = 1;
            if (albumArtRowsComplete < ALBUM_ART_H) albumArtRowsComplete++;
          }
        }

        // Auto-complete if all rows arrived or on last chunk
        if (albumArtRowsComplete >= ALBUM_ART_H) {
          completeAlbumArt();
        }
      }
      return;
    }

    if (len > 3 && (unsigned char)udpBuffer[0] == 0xFC) {
      // This legacy game-cover channel has no active sender or display owner.
      // Keep it isolated from the active Spotify image transfer.
      return;
    }

    // Standard string packet
    udpBuffer[len] = 0;
    String msg = String(udpBuffer);
    handleCommand(msg);
  }

  // Update WiFi status
  if (receivingArt && millis() - albumArtStartedAt > 8000) {
    discardIncomingArt();
  }
  wifiConnected = (WiFi.status() == WL_CONNECTED);

// ESP32: also check BT data
#ifdef ESP32
  handleBluetoothData();
#endif

  // Standalone Services (When no host device is active or host is Android)
  #ifdef ESP32
  tickStandaloneServices();
  #else
  if (wifiConnected) {
    if (!isHostActive() || isHostAndroid) pollSpotifyStandalone();
    if (!isHostActive()) fetchWeatherStandalone();
  }
  #endif
}

// ═══════════════════════════════════════════════════════════
// HANDLE COMMAND — Central command parser
// ═══════════════════════════════════════════════════════════

// [LEGACY - v6.0] updateTimeFromUDP forward declaration
extern void updateTimeFromUDP(String t);

void invalidatePlaybackArt() {
  companion_lyrics::timing() = companion_lyrics::Timing();
  albumArtReady = false;
  receivingArt = false;
  albumArtRowsComplete = 0;
  albumArtRevision++;
  memset(albumArtRowsReceived, 0, sizeof(albumArtRowsReceived));
  extern void resetSpotifyDrawState();
  extern void t2_resetSpotifyDrawState();
  resetSpotifyDrawState();
  t2_resetSpotifyDrawState();
}

void handleCommand(String msg) {
  // Keep durable Notes/Notifications state even while their UI is hidden.
  if (currentState == STATE_LYRICS &&
      (msg.startsWith("THOUGHT:") || msg.startsWith("AGENT:") || msg.startsWith("WEATHER:") ||
       msg.startsWith("STOCKS:") || msg.startsWith("GAMING:") || msg.startsWith("SOCIAL:") ||
       msg.startsWith("TASKS:") || msg.startsWith("RUVIEW:") || msg.startsWith("PCDASH:"))) return;
  if (msg.startsWith("SPOTIFY:") || msg.startsWith("PROGRESS:") ||
      msg.startsWith("TRACK:") || msg.startsWith("STATE:")) {
    spotifyProgressUpdatedAt = millis();
    lastHostPlaybackMillis = millis();
  }
  if (msg.startsWith("HOST:")) {
    if (msg.indexOf("ANDROID") >= 0) isHostAndroid = true;
    else if (msg.indexOf("PC") >= 0) isHostAndroid = false;
    lastHostPacketMillis = millis();
    return;
  } else if (msg.startsWith("EMOTION:")) {
    String emo = msg.substring(8);
    if (emo == "AUTO") {
      extern void resumeAutoEmotion();
      resumeAutoEmotion();
      return;
    }
    Emotion e = EMO_NEUTRAL;
    if (emo == "HAPPY") e = EMO_HAPPY;
    else if (emo == "SAD") e = EMO_SAD;
    else if (emo == "EXCITED") e = EMO_EXCITED;
    else if (emo == "LOVE") e = EMO_LOVE;
    else if (emo == "SLEEPY") e = EMO_SLEEPY;
    else if (emo == "ANGRY") e = EMO_ANGRY;
    else if (emo == "SURPRISED") e = EMO_SURPRISED;
    if (activeTheme == 2) t3_setEmotion(e);
    else if (activeTheme == 1) t2_setEmotion(e);
    else setEmotion(e);
  } else if (msg.startsWith("CUSTEYE:START")) {
    customEyeReady = false;
    customEyeActive = false;
    memset(customEyeRowsReceived, 0, sizeof(customEyeRowsReceived));
    if (customEyeImg == nullptr) {
      if (!prepareCustomEyeImage()) {
        sendCommand("CUSTEYE_ERROR:MEMORY_OR_PAGE");
        return;
      }
    }
    if (customEyeImg) memset(customEyeImg, 0, 160 * 128 * 2);
  } else if (msg.startsWith("CUSTEYE:VERIFY") || msg.startsWith("CUSTEYE:DONE")) {
    if (!customEyeImg) return;
    // Exact byte-by-byte check — ZERO interpolation!
    String missingRows = "";
    int missingCount = 0;
    for (int r = 0; r < 128; r++) {
      if (!customEyeRowsReceived[r]) {
        if (missingCount > 0) missingRows += ",";
        missingRows += String(r);
        missingCount++;
        if (missingCount >= 25) break; // Request in batches of up to 25 rows per packet
      }
    }
    if (missingCount > 0) {
      sendCommand("CUSTEYE_REQ:" + missingRows);
    } else {
      customEyeReady = true;
      customEyeActive = true;
      if (currentState == STATE_EYES) {
        renderCurrentPage();
      }
    }
  } else if (msg.startsWith("CUSTEYE:TOGGLE:")) {
    customEyeActive = customEyeReady && customEyeImg && (msg.substring(15).toInt() == 1);
    if (currentState == STATE_EYES) {
      renderCurrentPage();
    }
  } else if (msg == "CUSTEYE:OFF" || msg == "CUSTEYE:DISMISS" || msg == "CUSTEYE:CLEAR" || msg == "PHOTODROP:OFF" || msg == "PHOTODROP:DISMISS") {
    customEyeActive = false;
    if (currentState == STATE_EYES) {
      renderCurrentPage();
    }
#ifdef ESP32
  } else if (msg.startsWith("DRHACK:")) {
    // Never execute a remote tool command across a workspace restart. The user
    // must explicitly select it again after the radio workspace is ready.
    if (switchMemoryWorkspace(STATE_DR_HACK)) return;
    String action = msg.substring(7);
    currentState = STATE_DR_HACK;
    
    extern DrHackSubState dhCurrentState;
    if (action == "DEAUTH") {
      extern void dhRunDeauth();
      dhCurrentState = DH_DEAUTH;
      dhRunDeauth();
      dhCurrentState = DH_MENU;
      renderCurrentPage();
    } 
    else if (action == "SPAM") {
      extern void dhRunBeaconSpam();
      dhCurrentState = DH_BEACON_SPAM;
      dhRunBeaconSpam();
      dhCurrentState = DH_MENU;
      renderCurrentPage();
    }
    else if (action == "MONITOR") {
      extern void dhRunPacketMonitor();
      dhCurrentState = DH_PACKET_MONITOR;
      dhRunPacketMonitor();
      dhCurrentState = DH_MENU;
      renderCurrentPage();
    }
#endif
  } else if (msg.startsWith("EYE_THEME:")) {
    int theme = msg.substring(10).toInt();
    if (theme >= 0 && theme < THEME_COUNT) {
      activeTheme = theme;
      wakeForManualAppearance();
      if (currentState == STATE_EYES) {
        renderCurrentPage();
      }
    }
  }
  // V4: Antigravity Agent Status
  else if (msg.startsWith("AGENT:")) {
    String json = msg.substring(6);
    DynamicJsonDocument doc(512);
    if (!deserializeJson(doc, json)) {
      agentStatus = doc["status"].as<String>();
      agentStatusText = doc["text"].as<String>();
      if (agentStatusText.length() > 0) {
        agentStatusStart = millis();
        agentOverlayActive = true;
        lastInteractionTime = millis();
      }
      
      if (!manualEmotionOverride) {
        if (agentStatus == "thinking") applyAmbientEmotion(EMO_EXCITED);
        else if (agentStatus == "done") applyAmbientEmotion(EMO_HAPPY);
        else if (agentStatus == "error") applyAmbientEmotion(EMO_SAD);
      }
    }
  }
  // ── SPOTIFY / MEDIA DATA PACKETS ──
  else if (msg.startsWith("SPOTIFY:")) {
    String json = msg.substring(8);
    DynamicJsonDocument doc(1024);
    if (!deserializeJson(doc, json)) {
      if (doc.containsKey("track")) {
        String newTrack = doc["track"].as<String>();
        if (newTrack != currentTrack) {
          invalidatePlaybackArt();
          currentTrack = newTrack;
          currentLyrics = "";
          currentLyricsLine2 = "";
          prevLyricsLine = "";
        }
      }
      if (doc.containsKey("artist")) currentArtist = doc["artist"].as<String>();
      if (doc.containsKey("duration")) {
        int dur = doc["duration"].as<int>();
        playDuration = constrain(dur, 0, 86400000);
      }
      if (doc.containsKey("progress")) {
        int prg = doc["progress"].as<int>();
        playProgress = max(0, prg);
      }
      if (doc.containsKey("playing")) {
        isPlaying = doc["playing"].as<bool>();
        musicPlaying = isPlaying;
      }
      extern void t2_redrawSpotifyPartial();
      if (activeTheme == 1 && currentState == STATE_SPOTIFY) t2_redrawSpotifyPartial();
      else if (currentState == STATE_SPOTIFY) redrawSpotifyPartial();
      else if (currentState == STATE_CLOCK_DASHBOARD) redrawClockDashboardPartial();
      else if (currentState == STATE_RETRO_WATCH) redrawRetroWatchPartial();
    }
  } else if (msg.startsWith("PROGRESS:")) {
    String json = msg.substring(9);
    DynamicJsonDocument doc(512);
    if (!deserializeJson(doc, json)) {
      if (doc.containsKey("progress")) {
        int prg = doc["progress"].as<int>();
        playProgress = max(0, prg);
      }
      if (doc.containsKey("duration")) {
        int dur = doc["duration"].as<int>();
        playDuration = constrain(dur, 0, 86400000);
      }
      extern void t2_redrawSpotifyPartial();
      if (activeTheme == 1 && currentState == STATE_SPOTIFY) t2_redrawSpotifyPartial();
      else if (currentState == STATE_SPOTIFY) redrawSpotifyPartial();
      else if (currentState == STATE_CLOCK_DASHBOARD) redrawClockDashboardPartial();
      else if (currentState == STATE_RETRO_WATCH) redrawRetroWatchPartial();
    }
  } else if (msg.startsWith("TRACK:")) {
    String json = msg.substring(6);
    DynamicJsonDocument doc(1024);
    if (!deserializeJson(doc, json)) {
      String newTrack = doc["track"].as<String>();
      String newArtist = doc["artist"].as<String>();
      if (newTrack != currentTrack || newArtist != currentArtist) {
        invalidatePlaybackArt();
        currentTrack = newTrack;
        currentArtist = newArtist;
        currentLyrics = "";
        currentLyricsLine2 = "";
        prevLyricsLine = "";
      }
      if (doc.containsKey("duration")) {
        int dur = doc["duration"].as<int>();
        playDuration = constrain(dur, 0, 86400000);
      }
      musicPlaying = true;
      isPlaying = true;
      extern void t2_redrawSpotifyPartial();
      if (activeTheme == 1 && currentState == STATE_SPOTIFY) t2_redrawSpotifyPartial();
      else if (currentState == STATE_SPOTIFY) redrawSpotifyPartial();
      else if (currentState == STATE_CLOCK_DASHBOARD) redrawClockDashboardPartial();
      else if (currentState == STATE_RETRO_WATCH) redrawRetroWatchPartial();
    }
  } else if (msg.startsWith("ART_START:")) {
    if (!prepareIncomingArt()) return;
    receivingArt = true;
    albumArtReady = false;
    artDrawn = false;
    albumArtRowsComplete = 0;
    memset(albumArtRowsReceived, 0, sizeof(albumArtRowsReceived));
    if (albumArtIncoming) memset(albumArtIncoming, 0, ALBUM_ART_BYTES);
    albumArtStartedAt = millis();
  } else if (msg.startsWith("ART_COMPLETE:")) {
    if (!receivingArt || !albumArtIncoming) return;
    int missingRows = ALBUM_ART_H - albumArtRowsComplete;
    if (missingRows > 0) {
      // Never publish an incomplete frame; request the missing four-row chunks.
      String missingChunks = "";
      int count = 0;
      for (int c = 0; c < ALBUM_ART_H; c += 4) {
        bool chunkMissing = false;
        for (int r = c; r < c + 4; r++) {
          if (!albumArtRowsReceived[r]) { chunkMissing = true; break; }
        }
        if (chunkMissing) {
          if (count > 0) missingChunks += ",";
          missingChunks += String(c);
          count++;
        }
      }
      if (count > 0) {
        sendCommand("ART_REQ:" + missingChunks);
      }
    } else if (albumArtRowsComplete == ALBUM_ART_H) {
      completeAlbumArt();
    } else {
      receivingArt = false;
      albumArtReady = false;
    }
  } else if (msg.startsWith("SPOTIFY_RESET:")) {
    discardIncomingArt();
    currentLyrics = "";
    currentLyricsLine2 = "";
    prevLyricsLine = "";
    if (albumArt) memset(albumArt, 0, ALBUM_ART_BYTES);
    albumArtRowsComplete = 0;
    memset(albumArtRowsReceived, 0, sizeof(albumArtRowsReceived));
    artDrawn = false;
    albumArtReady = false;
    extern void t2_redrawSpotifyPartial();
    if (activeTheme == 1 && currentState == STATE_SPOTIFY) t2_redrawSpotifyPartial();
    else if (currentState == STATE_SPOTIFY) redrawSpotifyPartial();
  } else if (msg.startsWith("SPOTIFY_AUTH:")) {
    String payload = msg.substring(13);
    if (payload.length() > 1800) return;
    DynamicJsonDocument doc(2048);
    if (!deserializeJson(doc, payload.c_str())) {
      if (!doc["access"].is<const char*>() || !doc["refresh"].is<const char*>() ||
          !doc["client_id"].is<const char*>() || !doc["client_secret"].is<const char*>()) return;
      auto valid = [](const char* text, size_t capacity) {
        if (strlen(text) >= capacity) return false;
        for (; *text; ++text) if (uint8_t(*text) <= 32 || uint8_t(*text) >= 127) return false;
        return true;
      };
      const char* access = doc["access"];
      const char* refresh = doc["refresh"];
      const char* clientId = doc["client_id"];
      const char* secret = doc["client_secret"];
      if (doc["access"].as<JsonString>().size() != strlen(access) ||
          doc["refresh"].as<JsonString>().size() != strlen(refresh) ||
          doc["client_id"].as<JsonString>().size() != strlen(clientId) ||
          doc["client_secret"].as<JsonString>().size() != strlen(secret)) return;
      if (!*refresh || !*clientId || !valid(access, 2048) || !valid(refresh, 1024) ||
          !valid(clientId, 128) || !valid(secret, 256)) return;
      String nextAccess(access), nextRefresh(refresh), nextClientId(clientId), nextSecret(secret);
      if (nextAccess.length() != strlen(access) || nextRefresh.length() != strlen(refresh) ||
          nextClientId.length() != strlen(clientId) || nextSecret.length() != strlen(secret)) return;
      bool changed = spotifyAccessToken != nextAccess || spotifyRefreshToken != nextRefresh ||
                     spotifyClientId != nextClientId || spotifyClientSecret != nextSecret;
      spotifyAccessToken = std::move(nextAccess); spotifyRefreshToken = std::move(nextRefresh);
      spotifyClientId = std::move(nextClientId); spotifyClientSecret = std::move(nextSecret);
      #ifdef ESP32
      if (changed) notifyStandaloneCredentialsChanged();
      #endif
      if (changed) showFlashNotification("Spotify Synced");
      sendCommand("SPOTIFY_AUTH_OK");
    }
  } else if (msg.startsWith("STATE:")) {
    String json = msg.substring(6);
    DynamicJsonDocument doc(512);
    if (!deserializeJson(doc, json)) {
      if (doc.containsKey("playing")) {
        isPlaying = doc["playing"].as<bool>();
        musicPlaying = isPlaying;
      }
      if (doc.containsKey("progress")) {
        int prg = doc["progress"].as<int>();
        playProgress = max(0, prg);
      }
      if (doc.containsKey("track")) {
        String newTrack = doc["track"].as<String>();
        if (newTrack != currentTrack) {
          invalidatePlaybackArt();
          currentTrack = newTrack;
          currentLyrics = "";
          currentLyricsLine2 = "";
          prevLyricsLine = "";
        }
      }
      if (doc.containsKey("artist")) currentArtist = doc["artist"].as<String>();
      extern void t2_redrawSpotifyPartial();
      if (activeTheme == 1 && currentState == STATE_SPOTIFY) t2_redrawSpotifyPartial();
      else if (currentState == STATE_SPOTIFY) redrawSpotifyPartial();
      else if (currentState == STATE_CLOCK_DASHBOARD) redrawClockDashboardPartial();
      else if (currentState == STATE_RETRO_WATCH) redrawRetroWatchPartial();
    }
  } else if (msg.startsWith("LYRICS:")) {
    String json = msg.substring(7);
    DynamicJsonDocument doc(1024);
    if (!deserializeJson(doc, json)) {
      companion_lyrics::timing() = companion_lyrics::Timing();
      if (doc.is<JsonArray>()) {
        standaloneLyricsSynced = true;
        standaloneLyricsStatus = "";
        JsonArray arr = doc.as<JsonArray>();
        if (arr.size() >= 1) prevLyricsLine = arr[0].as<String>();
        if (arr.size() >= 2) currentLyrics = arr[1].as<String>();
        if (arr.size() >= 3) currentLyricsLine2 = arr[2].as<String>();
      } else {
        standaloneLyricsSynced = doc["synced"] | true;
        standaloneLyricsStatus = doc["status"] | "";
        if (doc.containsKey("line1")) currentLyrics = doc["line1"].as<String>();
        if (doc.containsKey("line2")) currentLyricsLine2 = doc["line2"].as<String>();
        if (doc.containsKey("prev"))  prevLyricsLine = doc["prev"].as<String>();
        companion_lyrics::timing().start = doc["line_at"] | int64_t(-1);
        companion_lyrics::timing().next = doc["next_at"] | int64_t(-1);
      }
      extern void t2_redrawSpotifyPartial();
      if (activeTheme == 1 && currentState == STATE_SPOTIFY) t2_redrawSpotifyPartial();
      else if (currentState == STATE_SPOTIFY) redrawSpotifyPartial();
      else if (currentState == STATE_CLOCK_DASHBOARD) redrawClockDashboardPartial();
      else if (currentState == STATE_RETRO_WATCH) redrawRetroWatchPartial();
    }
  } else if (msg.startsWith("WEATHER:")) {
    String json = msg.substring(8);
    DynamicJsonDocument doc(1024);
    if (!deserializeJson(doc, json)) {
      if ((!doc["temp"].is<int>() && !doc["temp"].is<float>()) ||
          doc["temp"].as<float>() < -100 || doc["temp"].as<float>() > 100 ||
          (!doc["condition"].is<const char*>() && !doc["cond"].is<const char*>())) return;
      lastHostWeatherMillis = millis();
      if (doc.containsKey("temp")) weatherTemp = doc["temp"].as<int>();
      if (doc.containsKey("condition")) weatherCondition = doc["condition"].as<String>();
      else if (doc.containsKey("cond")) weatherCondition = doc["cond"].as<String>();
      if (doc.containsKey("humidity")) weatherHumidity = doc["humidity"].as<int>();
      if (doc.containsKey("city")) weatherCity = doc["city"].as<String>();
      if (doc.containsKey("wind")) weatherWind = doc["wind"].as<int>();
      if (doc.containsKey("code")) weatherCode = doc["code"].as<int>();
      if (doc.containsKey("high")) weatherHigh = doc["high"].as<int>();
      if (doc.containsKey("low")) weatherLow = doc["low"].as<int>();
      if (doc.containsKey("feels")) weatherFeels = doc["feels"].as<int>();
      redrawWeatherPartial();
    }
  } else if (msg.startsWith("POMO:")) {
    String json = msg.substring(5);
    DynamicJsonDocument doc(512);
    if (!deserializeJson(doc, json) && doc.is<JsonObject>()) {
      bool recognized = false;
      for (const char* key : {"remaining", "total", "sessions"}) {
        if (!doc.containsKey(key)) continue;
        recognized = true;
        if (!doc[key].is<int>()) return;
        int value = doc[key].as<int>();
        if (value < 0 || value > (!strcmp(key, "sessions") ? 9999 : 86400)) return;
      }
      for (const char* key : {"active", "is_break"}) {
        if (!doc.containsKey(key)) continue;
        recognized = true;
        if (!doc[key].is<bool>()) return;
      }
      int remaining = doc["remaining"] | pomoRemaining;
      int total = doc["total"] | pomoTotal;
      bool active = doc["active"] | pomoActive;
      if (!recognized || remaining > total || (active && !total)) return;
      lastHostPomodoroMillis = millis();
      pomoClockBaseline = millis();
      if (doc.containsKey("remaining")) pomoRemaining = doc["remaining"].as<int>();
      if (doc.containsKey("total")) pomoTotal = doc["total"].as<int>();
      if (doc.containsKey("active")) pomoActive = doc["active"].as<bool>();
      if (doc.containsKey("is_break")) pomoIsBreak = doc["is_break"].as<bool>();
      if (doc.containsKey("sessions")) pomoSessions = doc["sessions"].as<int>();
      redrawPomodoroPartial();
    }
  } else if (msg.startsWith("NOTIF:")) {
    lastHostPacketMillis = millis();
    String json = msg.substring(6);
    DynamicJsonDocument doc(1024);
    if (!deserializeJson(doc, json) && doc.is<JsonArray>()) {
      JsonArray array = doc.as<JsonArray>();
      String oldTitle = notifTitles[0], oldTime = notifTimes[0], oldApp = notifApps[0];
      notifTotal = array.size();
      notifCount = notifTotal;
      for (int i = 0; i < 3; i++) {
        if (i < (int)array.size()) {
          notifApps[i] = array[i]["app"].as<String>();
          notifTitles[i] = array[i]["title"].as<String>();
          notifTimes[i] = array[i]["time"].as<String>();
        } else {
          notifApps[i] = "";
          notifTitles[i] = "";
          notifTimes[i] = "";
        }
      }
      redrawNotificationsPartial();

      if (notifTotal > 0 && currentState == STATE_EYES &&
          (oldTitle != notifTitles[0] || oldTime != notifTimes[0] || oldApp != notifApps[0])) {
        showFlashNotification(notifTitles[0]);
      }
    }
  } else if (msg.startsWith("NOTES:")) {
    String json = msg.substring(6);
    DynamicJsonDocument doc(1024);
    if (!deserializeJson(doc, json) && doc.is<JsonArray>()) {
      JsonArray array = doc.as<JsonArray>();
      for (int i = 0; i < 4; i++) {
        if (i < (int)array.size()) currentNotes[i] = array[i].as<String>();
        else currentNotes[i] = "";
      }
      redrawNotesPartial();
    }
  } else if (msg.startsWith("TIME:")) {
    lastHostPacketMillis = millis();
    String json = msg.substring(5);
    DynamicJsonDocument doc(256);
    deserializeJson(doc, json);
    if (doc.containsKey("time")) {
      String t = doc["time"];
      updateTimeFromUDP(t);
      drawStatusBar();
    }
  }
  // ═══════════════════════════════════════════════════════
  // V6: Page Data Handlers
  // ═══════════════════════════════════════════════════════
  else if (msg.startsWith("STOCKS:")) {
    if (!companion_memory::nearby(int(currentState), int(STATE_STOCKS), int(STATE_COUNT))) return;
    String json = msg.substring(7);
    DynamicJsonDocument doc(1024);
    if (!deserializeJson(doc, json)) {
      if ((doc.containsKey("wl") && !doc["wl"].is<JsonArray>()) ||
          (doc.containsKey("hist") && !doc["hist"].is<JsonArray>())) return;
      if (doc.containsKey("symbol")) SAFESTR(stockSymbol, doc["symbol"], 16);
      if (doc.containsKey("price")) SAFESTR(stockPrice, doc["price"], 16);
      if (doc.containsKey("delta")) SAFESTR(stockDelta, doc["delta"], 16);
      if (doc.containsKey("pct")) SAFESTR(stockPctChg, doc["pct"], 16);
      if (doc.containsKey("up")) stockIsUp = doc["up"].as<bool>();

      if (doc.containsKey("hist")) {
        JsonArray hist = doc["hist"];
        stockHistoryLen = min((int)hist.size(), 40);
        for (int i = 0; i < stockHistoryLen; i++) {
          stockHistory[i] = hist[i].as<int16_t>();
        }
      }

      if (doc.containsKey("wl")) {
        if (!doc["wl"].is<JsonArray>()) return;
        JsonArray wl = doc["wl"];
        for (int i = 0; i < 3; ++i) {
          wlSymbol[i][0] = wlPrice[i][0] = wlDelta[i][0] = 0;
          wlIsUp[i] = false;
        }
        for (int i = 0; i < min((int)wl.size(), 3); i++) {
          SAFESTR(wlSymbol[i], wl[i]["s"], 16);
          SAFESTR(wlPrice[i], wl[i]["p"], 16);
          SAFESTR(wlDelta[i], wl[i]["d"], 16);
          wlIsUp[i] = wl[i]["u"].as<bool>();
        }
      }

      redrawStocksPartial();
    }
  } else if (msg.startsWith("GAMING:")) {
    if (!companion_memory::nearby(int(currentState), int(STATE_GAMING), int(STATE_COUNT))) return;
    lastHostPacketMillis = millis();
    String json = msg.substring(7);
    DynamicJsonDocument doc(512);
    if (!deserializeJson(doc, json)) {
      if (doc.containsKey("recent") && !doc["recent"].is<JsonArray>()) return;
      if (doc.containsKey("title")) SAFESTR(gameTitle, doc["title"], 24);
      if (doc.containsKey("session")) SAFESTR(sessionTime, doc["session"], 12);
      if (doc.containsKey("achieve")) achievePct = constrain(doc["achieve"].as<int>(), 0, 100);
      if (doc.containsKey("friends")) friendsOnline = constrain(doc["friends"].as<int>(), 0, 255);
      if (doc.containsKey("active")) gameActive = doc["active"].as<bool>();
      if (doc.containsKey("status")) SAFESTR(gameStatus, doc["status"], 16);

      // 🟡 GAP-01 FIX: Parse recent games list
      if (doc.containsKey("recent")) {
        if (!doc["recent"].is<JsonArray>()) return;
        JsonArray recent = doc["recent"];
        for (int i = 0; i < 3; ++i) { recentGame[i][0] = 0; recentPlaytime[i] = 0; }
        for (int i = 0; i < min((int)recent.size(), 3); i++) {
          SAFESTR(recentGame[i], recent[i]["name"], 24);
          recentPlaytime[i] = constrain(recent[i]["time"].as<int>(), 0, 65535);
        }
      }

      redrawGamingPartial();
    }
  } else if (msg.startsWith("SOCIAL:")) {
    if (!companion_memory::nearby(int(currentState), int(STATE_SOCIAL), int(STATE_COUNT))) return;
    String json = msg.substring(7);
    DynamicJsonDocument doc(512);
    if (!deserializeJson(doc, json)) {
      if (doc.containsKey("user")) SAFESTR(socialUser, doc["user"], 16);
      if (doc.containsKey("app")) SAFESTR(socialApp, doc["app"], 12);
      if (doc.containsKey("body")) SAFESTR(socialBody, doc["body"], 80);
      if (doc.containsKey("time")) SAFESTR(socialTime, doc["time"], 8);
      if (doc.containsKey("likes")) socialLikes = constrain(doc["likes"].as<int>(), 0, 65535);
      if (doc.containsKey("comments")) socialComments = constrain(doc["comments"].as<int>(), 0, 65535);
      redrawSocialPartial();
    }
  } else if (msg.startsWith("TASKS:")) {
    if (!companion_memory::nearby(int(currentState), int(STATE_PRODUCTIVITY), int(STATE_COUNT))) return;
    String json = msg.substring(6);
    DynamicJsonDocument doc(1024);
    if (!deserializeJson(doc, json)) {
      if (doc.containsKey("current")) SAFESTR(taskCurrent, doc["current"], 32);
      if (doc.containsKey("current_time")) SAFESTR(taskCurrentTime, doc["current_time"], 20);
      if (doc.containsKey("next1")) SAFESTR(taskNext1, doc["next1"], 32);
      if (doc.containsKey("next1_time")) SAFESTR(taskNext1Time, doc["next1_time"], 16);
      if (doc.containsKey("next2")) SAFESTR(taskNext2, doc["next2"], 32);
      if (doc.containsKey("next2_time")) SAFESTR(taskNext2Time, doc["next2_time"], 16);
      if (doc.containsKey("active")) taskActive = doc["active"].as<bool>();
      if (doc.containsKey("progress")) taskProgressPct = constrain(doc["progress"].as<int>(), 0, 100);
      redrawProductivityPartial();
    }
  }
  // ═══════════════════════════════════════════════════════
  // V7: New Command Handlers
  // ═══════════════════════════════════════════════════════

  // THOUGHT: — PC/Phone-pushed thought bubble override
  else if (msg.startsWith("THOUGHT:")) {
    String thought = msg.substring(8);
    thought.trim();
    if (thought.length() > 0 && thought.length() < 80) {
      strncpy(overrideThought, thought.c_str(), 79);
      overrideThought[79] = '\0';
    }
  }

  // HACK: — PC-side hack command relay (results pushed back)
  else if (msg.startsWith("HACK:")) {
    Serial.print(F("HACK CMD: "));
    Serial.println(msg.substring(5));
    if (currentState == STATE_DR_HACK) {
      showFlashNotification("Dr.Hack event received");
    }
  }

  // COMPANION_PROBE / PING — Auto-discovery instant response
  else if (msg.startsWith("COMPANION_PROBE") || msg.startsWith("PING") || msg == "HELLO_BOT") {
    if (msg.indexOf("ANDROID") >= 0) isHostAndroid = true;
    else if (msg.indexOf("PC") >= 0) isHostAndroid = false;
    pcFound = true;
    lastHostPacketMillis = millis();
    udp.beginPacket(udp.remoteIP(), UDP_PORT_TX);
    const char* hello = "HELLO_COMPANION";
    udp.write((const uint8_t*)hello, strlen(hello));
    udp.endPacket();
  }

  // BTN: — Web remote virtual button press
  else if (msg.startsWith("BTN:")) {
    String btn = msg.substring(4);
    handleVirtualButton(btn);
  }
  
  // CMD: — Custom commands from Web Remote
  else if (msg.startsWith("CMD:")) {
    String cmd = msg.substring(4);
    if (cmd == "PLAY_PAUSE" || cmd == "NEXT" || cmd == "PREV" ||
        cmd == "POMO:START" || cmd == "POMO:PAUSE" || cmd == "POMO:RESET") {
      sendCommand(cmd);
    } else if (cmd == "NRF_CHECK") {
      extern int settingsScrollY;
      extern void setPage(int targetPage);
      setPage(STATE_SETTINGS);
      settingsScrollY = 0;
      redrawSettingsPartial();
    }
  }

  // PAGE: — Direct page navigation from web remote / Android App
  else if (msg.startsWith("PAGE:")) {
    int page = msg.substring(5).toInt();
    extern void setPage(int targetPage);
    setPage(page);
  }

  // WIFI:MULTI_SET: — Save prioritized multi-network array
  else if (msg.startsWith("WIFI:MULTI_SET:")) {
    int braceIdx = msg.indexOf('{');
    if (braceIdx >= 0) {
      String json = msg.substring(braceIdx);
      DynamicJsonDocument doc(1024);
      if (!deserializeJson(doc, json)) {
        JsonArray arr = doc["networks"].as<JsonArray>();
        std::vector<std::pair<String, String>> nets;
        for (JsonObject obj : arr) {
          String s = obj["ssid"].as<String>();
          String p = obj["pass"].as<String>();
          if (s.length() > 0) {
            nets.push_back({s, p});
          }
        }
        if (nets.size() > 0) {
          saveMultiWiFiCredentials(nets);
          showFlashNotification("Saved Networks!");
          delay(800);
          #ifdef ESP32
          ESP.restart();
          #else
          ESP.reset();
          #endif
        }
      }
    }
  }

  // WIFI:SET: / WIFI:CONFIG: — Dynamic remote single Wi-Fi configuration
  else if (msg.startsWith("WIFI:SET:") || msg.startsWith("WIFI:CONFIG:")) {
    int braceIdx = msg.indexOf('{');
    if (braceIdx >= 0) {
      String json = msg.substring(braceIdx);
      DynamicJsonDocument doc(512);
      if (!deserializeJson(doc, json)) {
        String newSSID = doc["ssid"].as<String>();
        String newPASS = doc["pass"].as<String>();
        if (newSSID.length() > 0) {
          saveWiFiCredentials(newSSID, newPASS);
          showFlashNotification("WiFi Updating...");
          WiFi.disconnect();
          WiFi.begin(currentWiFiSSID.c_str(), currentWiFiPASS.c_str());
        }
      }
    }
  }

  // ═══════════════════════════════════════════════════════
  // Theme 2 Extended Data
  // ═══════════════════════════════════════════════════════
  else if (msg.startsWith("T2SPOT:")) {
    String json = msg.substring(7);
    DynamicJsonDocument doc(512);
    if (!deserializeJson(doc, json)) {
      extern uint8_t t2_volume;
      extern bool t2_shuffle;
      extern uint8_t t2_repeat;
      extern char t2_device[24];
      if (doc.containsKey("vol")) t2_volume = doc["vol"].as<uint8_t>();
      if (doc.containsKey("shuf")) t2_shuffle = doc["shuf"].as<bool>();
      if (doc.containsKey("rep")) t2_repeat = doc["rep"].as<uint8_t>();
      if (doc.containsKey("dev")) SAFESTR(t2_device, doc["dev"], 24);
      extern void t2_redrawSpotifyPartial();
      extern bool t2s_overlayDrawn;
      t2s_overlayDrawn = false;
      if (activeTheme == 1 && currentState == STATE_SPOTIFY) t2_redrawSpotifyPartial();
    }
  } else if (msg.startsWith("THEME:")) {
    int t = msg.substring(6).toInt();
    if (t >= 0 && t < THEME_COUNT) {
      activeTheme = t;
      wakeForManualAppearance();
      EEPROM.begin(EEPROM_SIZE);
      EEPROM.write(EEPROM_ACTIVE_THEME_ADDR, activeTheme);
      EEPROM.commit();
      EEPROM.end();
      showFlashNotification("Theme Changed");
      renderCurrentPage();
    }
  } else if (msg.startsWith("RUVIEW:")) {
    // ── RuView CSI Presence Data ──
    lastHostPacketMillis = millis();
    if (currentState != STATE_RUVIEW) return; // Skip CPU-intensive parsing when not viewing RuView
    String payload = msg.substring(7);
    StaticJsonDocument<1536> doc;
    if (deserializeJson(doc, payload) == DeserializationError::Ok && doc.is<JsonObject>()) {
      if (!doc["status"].is<const char*>() || (!doc["pps"].is<float>() && !doc["pps"].is<int>())) return;
      float pps = doc["pps"].as<float>();
      if (!isfinite(pps) || pps < 0 || pps > 2000) return;
      extern bool    rvOccupied;
      extern bool    rvMotion;
      extern float   rvConfidence;
      extern float   rvVariance;
      extern int8_t  rvRSSI;
      extern bool    rvCalibrating;
      extern float   rvPPS;
      extern int     rvSubcarriers;
      extern char    rvStatus[];
      extern char    rvZoneLabel[];

      rvOccupied    = doc["occupied"] | false;
      rvMotion      = doc["motion"] | false;
      float confidence = doc["confidence"] | 0.0f, variance = doc["variance"] | 0.0f;
      rvConfidence  = isfinite(confidence) ? constrain(confidence, 0.0f, 100.0f) : 0;
      rvVariance    = isfinite(variance) ? constrain(variance, 0.0f, 1.0e9f) : 0;
      rvRSSI        = constrain(doc["rssi"] | 0, -127, 0);
      rvCalibrating = doc["calibrating"] | false;
      rvPPS         = pps;
      rvSubcarriers = constrain(doc["subcarriers"] | 0, 0, 1024);
      if (!pps) { rvOccupied = rvMotion = false; rvConfidence = 0; }

      const char* st = doc["status"] | "Unknown";
      strncpy(rvStatus, st, 23);
      rvStatus[23] = '\0';

      if (doc.containsKey("label")) {
        const char* lbl = doc["label"] | "Room";
        strncpy(rvZoneLabel, lbl, 15);
        rvZoneLabel[15] = '\0';
      }
      extern void noteRuviewPacket();
      noteRuviewPacket();

      // Only redraw if on RuView page
      if (currentState == STATE_RUVIEW) {
        extern void redrawRuviewPartial();
        redrawRuviewPartial();
      }
    }
  } else if (msg.startsWith("PCDASH:") || (msg.indexOf(',') >= 0 && isdigit(msg.charAt(0)))) {
    // ── PC Dashboard Telemetry (InnoCircuitsLab port) ──
    extern void processPcDashPacket(const char* packet);
    processPcDashPacket(msg.c_str());
  }
}

#endif
