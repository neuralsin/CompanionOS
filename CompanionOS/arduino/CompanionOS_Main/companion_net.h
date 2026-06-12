// ═══════════════════════════════════════════════════════════
// COMPANION OS v7.0 — NETWORK FUNCTIONS
// Dual-platform: ESP32 (WiFi + BT) / ESP8266 (WiFi only)
// ═══════════════════════════════════════════════════════════
#ifndef COMPANION_NET_H
#define COMPANION_NET_H

// [LEGACY - v6.0] Original included ESP8266WiFi.h unconditionally.
// Now uses platform-conditional includes.

#ifdef ESP32
#include <WiFi.h>
#include <BluetoothSerial.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#endif

#include "globals.h"
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>

#include "eyes.h"

// ═══════════════════════════════════════════════════════════
// STATE & FORWARD DECLARATIONS
// ═══════════════════════════════════════════════════════════

extern char udpBuffer[2048];
String pcIPStr = DEFAULT_PC_IP;

// ESP32 System Web Server for Direct Memory Uploads
static WebServer sysWebServer(8080);
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
int playDuration = 100;
bool isPlaying = false;
extern bool artDrawn;

String currentNotes[4] = { "", "", "", "" };

// ESP32 Bluetooth Serial
#ifdef ESP32
BluetoothSerial btSerial;
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

void handleVirtualButton(String btn) {
  btn.trim();
  btn.toUpperCase();

  if (btn == "HOME") {
    virtualHomePressed = true;
    if (currentState != STATE_EYES) {
      changePage(-(int)currentState);
    }
    lastInteractionTime = millis();
    return;
  }

  if (btn == "LEFT_LONG") {
    if (currentState == STATE_SPOTIFY) sendCommand("PREV");
    lastInteractionTime = millis();
    return;
  }

  if (btn == "LEFT") {
    virtualLeftPressed = true;
#ifdef ESP32
    if (currentState == STATE_DR_HACK) {
      extern void dhNavigate(int delta);
      dhNavigate(-1);
      lastInteractionTime = millis();
      return;
    }
#endif

    changePage(-1);
    lastInteractionTime = millis();
    return;
  }

  if (btn == "RIGHT_LONG") {
    if (currentState == STATE_SPOTIFY) sendCommand("NEXT");
    lastInteractionTime = millis();
    return;
  }

  if (btn == "RIGHT") {
    virtualRightPressed = true;
#ifdef ESP32
    if (currentState == STATE_DR_HACK) {
      extern void dhNavigate(int delta);
      dhNavigate(1);
      lastInteractionTime = millis();
      return;
    }
#endif

    changePage(1);
    lastInteractionTime = millis();
    return;
  }

  if (btn == "SELECT_LONG") {
    if (currentState == STATE_DR_HACK) {
      extern void dhSelect();
      dhSelect();
    }
    lastInteractionTime = millis();
    return;
  }

  if (btn == "SELECT") {
    virtualSelectPressed = true;
#ifdef ESP32
    if (currentState == STATE_DR_HACK) {
      extern void dhSelect();
      dhSelect();
      lastInteractionTime = millis();
      return;
    }
#endif

    if (currentState == STATE_EYES) {
      if (activeTheme == 2) t3_nextExpression();
      else if (activeTheme == 1) t2_nextExpression();
      else setEmotion((Emotion)((currentEmotion + 1) % EMO_COUNT));
    } else if (currentState == STATE_SPOTIFY) {
      sendCommand("PLAY_PAUSE");
    } else if (currentState == STATE_POMODORO) {
      sendCommand(pomoActive ? "POMO:PAUSE" : "POMO:START");
    } else if (currentState == STATE_NOTIFICATIONS) {
      flashNotifEnabled = !flashNotifEnabled;
      redrawNotificationsPartial();
    } else if (currentState == STATE_SETTINGS) {
      activeTheme = (activeTheme + 1) % THEME_COUNT;
      EEPROM.begin(EEPROM_SIZE);
      EEPROM.write(EEPROM_ACTIVE_THEME_ADDR, activeTheme);
      EEPROM.commit();
      EEPROM.end();
      renderCurrentPage();
    }
    lastInteractionTime = millis();
    return;
  }

  if (btn == "UP") {
    virtualUpPressed = true;
    if (currentState == STATE_SETTINGS) {
      extern int settingsScrollY;
      settingsScrollY -= 20;
      if (settingsScrollY < 0) settingsScrollY = 0;
      renderCurrentPage();
    }
    lastInteractionTime = millis();
    return;
  }

  if (btn == "DOWN") {
    virtualDownPressed = true;
    if (currentState == STATE_SETTINGS) {
      extern int settingsScrollY;
      settingsScrollY += 20;
      renderCurrentPage();
    }
    lastInteractionTime = millis();
    return;
  }
}

void setupWiFi() {
  Serial.print(F("WiFi setup..."));
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(TFT_WHITE);
  tft.drawCentreString("Connecting WiFi...", SCR_CX, SCR_CY, 2);

  // Match legacy: simple WiFi.begin — no disconnect, no setSleep, no setTxPower
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int timeout = 0;
  // Legacy used 60 (30 seconds). Keep the same generous timeout.
  while (WiFi.status() != WL_CONNECTED && timeout < 60) {
    delay(500);
    Serial.print(".");
    timeout++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println(F(" OK"));
    Serial.print(F("IP: "));
    Serial.println(WiFi.localIP());

    // Start System Web Server
    sysWebServer.on("/", HTTP_GET, [](){
      sysWebServer.send(200, "text/html", MEMORIES_HTML);
    });
    sysWebServer.on("/memories", HTTP_GET, [](){
      sysWebServer.send(200, "text/html", MEMORIES_HTML);
    });
    sysWebServer.on("/upload", HTTP_POST, [](){
      sysWebServer.send(200, "text/plain", "OK");
    }, [](){
      HTTPUpload& upload = sysWebServer.upload();
      if (upload.status == UPLOAD_FILE_START) {
        if (!customEyeImg) {
          customEyeImg = (uint16_t*)malloc(160 * 128 * 2);
        }
        customEyeActive = false;
        customEyeReady = false;
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (customEyeImg && (upload.totalSize + upload.currentSize <= 160 * 128 * 2)) {
          memcpy((uint8_t*)customEyeImg + upload.totalSize, upload.buf, upload.currentSize);
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        customEyeActive = true;
        customEyeReady = true;
        extern void renderCurrentPage();
        renderCurrentPage();
      }
    });
    sysWebServer.begin();
    Serial.println(F("SysWebServer started on port 8080"));


    tft.fillScreen(COLOR_BG);
    tft.setTextColor(TFT_GREEN);
    tft.drawCentreString("WiFi Connected!", SCR_CX, SCR_CY - SCALE_Y(20), 2);
    tft.drawCentreString(WiFi.localIP().toString(), SCR_CX, SCR_CY + SCALE_Y(10), 2);

    // NTP Time Sync — match legacy: non-blocking update(), NOT forceUpdate()
    timeClient.begin();
    if (timeClient.update()) {
      displayHour = timeClient.getHours();
      displayMinute = timeClient.getMinutes();
      displaySecond = timeClient.getSeconds();
      timeReceived = true;
      lastTimeUpdateMillis = millis();
      lastNTPSync = millis();
      Serial.printf("NTP synced: %02d:%02d:%02d IST\n", displayHour, displayMinute, displaySecond);
    } else {
      Serial.println(F("NTP initial sync failed, will retry"));
    }
  } else {
    Serial.println(F(" FAILED"));
    tft.setTextColor(TFT_RED);
    tft.drawCentreString("WiFi Failed!", SCR_CX, SCR_CY - SCALE_Y(20), 2);
  }

  udp.begin(UDP_PORT_RX);
  delay(1500);

  // Auto-discovery handshake — match legacy: use string "255.255.255.255"
  if (wifiConnected) {
    udp.beginPacket("255.255.255.255", UDP_PORT_TX);
    const char* hello = "HELLO_COMPANION";
    udp.write((const uint8_t*)hello, strlen(hello));
    udp.endPacket();
  }
}

// ═══════════════════════════════════════════════════════════
// ESP32 BLUETOOTH TRANSPORT
// ═══════════════════════════════════════════════════════════

#ifdef ESP32
void setupBluetooth() {
  if (!btInitialized) {
    btSerial.begin("CompanionOS");
    btInitialized = true;
    Serial.println(F("BT Serial started as 'CompanionOS'"));
  }
}

void handleBluetoothData() {
  if (!btInitialized) return;

  if (btSerial.hasClient()) {
    btConnected = true;
  }

  if (btSerial.available()) {
    String msg = btSerial.readStringUntil('\n');
    msg.trim();
    if (msg.length() > 0) {
      handleCommand(msg);
    }
  }
}
#endif

// ═══════════════════════════════════════════════════════════
// SEND COMMAND — Dual transport (WiFi preferred, BT fallback)
// ═══════════════════════════════════════════════════════════

void sendCommand(String cmd) {
  // Match legacy: use pcIPStr.c_str() directly
  udp.beginPacket(pcIPStr.c_str(), UDP_PORT_TX);
  udp.write((const uint8_t*)cmd.c_str(), cmd.length());
  udp.endPacket();
#ifdef ESP32
  if (btConnected && btInitialized) {
    btSerial.println(cmd);
  }
#endif
  Serial.print(F("Sent: "));
  Serial.println(cmd);
}

// ═══════════════════════════════════════════════════════════
// HANDLE NETWORK — Main loop
// ═══════════════════════════════════════════════════════════

// Network state trackers
// ═══════════════════════════════════════════════════════════

bool pcFound = false;
unsigned long lastDiscoveryShout = 0;

void handleNetwork() {
  if (wifiConnected) {
    sysWebServer.handleClient();
  }

  // Periodic NTP re-sync (every 5 minutes)
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

  // Auto-discovery — match legacy: use string broadcast address
  if (!pcFound && millis() - lastDiscoveryShout > 3000) {
    udp.beginPacket("255.255.255.255", UDP_PORT_TX);
    const char* hello = "HELLO_COMPANION";
    udp.write((const uint8_t*)hello, strlen(hello));
    udp.endPacket();
    lastDiscoveryShout = millis();
  }

  // UDP packet processing
  int packetSize = udp.parsePacket();
  if (packetSize) {
    pcFound = true;
    IPAddress remoteIP = udp.remoteIP();
    pcIPStr = remoteIP.toString();

    int readLen = min(packetSize, 2047);
    int len = udp.read(udpBuffer, readLen);

    // V4 OPTIMIZATION: BARE-METAL BINARY PACKET SNIFFING
    if (len > 3 && (unsigned char)udpBuffer[0] == 0xFD) {
      // CUSTOM EYE IMAGE CHUNKS
      if (customEyeImg == nullptr) {
        customEyeImg = (uint16_t*)malloc(160 * 128 * 2);
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
      }
      return;
    }

    if (len > 3 && (unsigned char)udpBuffer[0] == 0xFE) {
      int chunkIdx = ((unsigned char)udpBuffer[1] << 8) | ((unsigned char)udpBuffer[2]);
      int pixelsInChunk = (len - 3) / 2;
      int imgWidth = (currentState == STATE_GAMING) ? 100 : ALBUM_ART_W;
      int imgHeight = (currentState == STATE_GAMING) ? 60 : ALBUM_ART_H;
      int imagePixels = imgWidth * imgHeight;
      int maxPixels = ALBUM_ART_W * ALBUM_ART_H;
      int pixelsPerChunk = imgWidth * 2;
      int startPixel = chunkIdx * pixelsPerChunk;

      if (startPixel >= 0 && startPixel < imagePixels && startPixel < maxPixels) {
        uint16_t* dest = albumArt + startPixel;
        int safePixels = min(pixelsInChunk, min(imagePixels - startPixel, maxPixels - startPixel));
        if (safePixels <= 0) return;

        for (int i = 0; i < safePixels; i++) {
          int offset = 3 + (i * 2);
          dest[i] = ((uint16_t)(unsigned char)udpBuffer[offset] << 8) | (unsigned char)udpBuffer[offset + 1];
        }

        // Progressive rendering
        int imgX = SCALE_X(10);
        int imgY = SCALE_Y(25);
        if (currentState == STATE_GAMING) {
          imgX = SCALE_X(110);
          imgY = SCALE_Y(32);
        } else if (currentState == STATE_SPOTIFY && activeTheme == 1) {
          imgX = SCALE_X(140);
          imgY = SCALE_Y(20);
        }

        int yStart = startPixel / imgWidth;
        int maxRows = safePixels / imgWidth;

        bool enableProgressive = true;
        if (currentState == STATE_SPOTIFY && activeTheme != 0) {
          enableProgressive = false;
        }
        if ((currentState == STATE_SPOTIFY || currentState == STATE_GAMING) && enableProgressive) {
          if (maxRows > 0) {
            tft.pushImage(imgX, imgY + yStart, imgWidth, maxRows, dest);
          }
        }
      }
      return;
    }

    // Standard string packet
    udpBuffer[len] = 0;
    String msg = String(udpBuffer);
    handleCommand(msg);
  }

  // Update WiFi status
  wifiConnected = (WiFi.status() == WL_CONNECTED);

// ESP32: also check BT data
#ifdef ESP32
  handleBluetoothData();
#endif
}

// ═══════════════════════════════════════════════════════════
// HANDLE COMMAND — Central command parser
// ═══════════════════════════════════════════════════════════

// [LEGACY - v6.0] updateTimeFromUDP forward declaration
extern void updateTimeFromUDP(String t);

void handleCommand(String msg) {
  if (msg.startsWith("EMOTION:")) {
    String emo = msg.substring(8);
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
    if (customEyeImg == nullptr) {
      customEyeImg = (uint16_t*)malloc(160 * 128 * 2);
    }
  } else if (msg.startsWith("CUSTEYE:DONE")) {
    customEyeReady = true;
    if (customEyeActive && currentState == STATE_EYES) {
      renderCurrentPage();
    }
  } else if (msg.startsWith("CUSTEYE:TOGGLE:")) {
    customEyeActive = (msg.substring(15).toInt() == 1);
    if (currentState == STATE_EYES) {
      renderCurrentPage();
    }
  } else if (msg.startsWith("DRHACK:")) {
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
      
      if (agentStatus == "thinking") {
        if (activeTheme == 2) t3_setEmotion(EMO_EXCITED);
        else if (activeTheme == 1) t2_setEmotion(EMO_EXCITED);
        else setEmotion(EMO_EXCITED);
      } else if (agentStatus == "done") {
        if (activeTheme == 2) t3_setEmotion(EMO_HAPPY);
        else if (activeTheme == 1) t2_setEmotion(EMO_HAPPY);
        else setEmotion(EMO_HAPPY);
      } else if (agentStatus == "error") {
        if (activeTheme == 2) t3_setEmotion(EMO_SAD);
        else if (activeTheme == 1) t2_setEmotion(EMO_SAD);
        else setEmotion(EMO_SAD);
      }
    }
  } else if (msg.startsWith("TRACK:")) {
    String json = msg.substring(6);
    DynamicJsonDocument doc(1024);
    if (!deserializeJson(doc, json)) {
      currentTrack = doc["track"].as<String>();
      currentArtist = doc["artist"].as<String>();
      playDuration = doc["duration"].as<int>();
      musicPlaying = true;
      extern void t2_redrawSpotifyPartial();
      if (activeTheme == 1) t2_redrawSpotifyPartial();
      else redrawSpotifyPartial();
    }
  } else if (msg.startsWith("ART_START:")) {
    receivingArt = true;
    albumArtReady = false;
    artDrawn = false;
  } else if (msg.startsWith("ART_COMPLETE:")) {
    completeAlbumArt();
  } else if (msg.startsWith("STATE:")) {
    String json = msg.substring(6);
    DynamicJsonDocument doc(512);
    if (!deserializeJson(doc, json)) {
      playProgress = doc["progress"].as<int>();
      isPlaying = doc["playing"].as<bool>();
      musicPlaying = isPlaying;
      extern void t2_redrawSpotifyPartial();
      if (activeTheme == 1) t2_redrawSpotifyPartial();
      else redrawSpotifyPartial();
    }
  } else if (msg.startsWith("LYRICS:")) {
    String json = msg.substring(7);
    DynamicJsonDocument doc(1024);
    if (!deserializeJson(doc, json)) {
      JsonArray array = doc.as<JsonArray>();
      if (array.size() >= 3) {
        prevLyricsLine = array[0].as<String>();
        currentLyrics = array[1].as<String>();
        currentLyricsLine2 = array[2].as<String>();
      } else if (array.size() > 0) {
        currentLyrics = array[0].as<String>();
        currentLyricsLine2 = (array.size() > 1) ? array[1].as<String>() : "";
        prevLyricsLine = "";
      } else {
        currentLyrics = "Instrumental";
        currentLyricsLine2 = "";
        prevLyricsLine = "";
      }
      extern void t2_redrawSpotifyPartial();
      if (activeTheme == 1) t2_redrawSpotifyPartial();
      else redrawSpotifyPartial();
    }
  } else if (msg.startsWith("WEATHER:")) {
    String json = msg.substring(8);
    DynamicJsonDocument doc(1024);
    if (!deserializeJson(doc, json)) {
      weatherTemp = doc["temp"].as<int>();
      weatherFeels = doc["feels"].as<int>();
      weatherHumidity = doc["humidity"].as<int>();
      weatherCondition = doc["condition"].as<String>();
      weatherCode = doc["code"].as<int>();
      weatherWind = doc["wind"].as<int>();
      weatherSunrise = doc["sunrise"].as<String>();
      weatherSunset = doc["sunset"].as<String>();
      weatherHigh = doc["high"].as<int>();
      weatherLow = doc["low"].as<int>();
      weatherCity = doc["city"].as<String>();
      redrawWeatherPartial();
    }
  } else if (msg.startsWith("POMO:")) {
    String json = msg.substring(5);
    DynamicJsonDocument doc(512);
    if (!deserializeJson(doc, json)) {
      pomoRemaining = doc["remaining"].as<int>();
      pomoTotal = doc["total"].as<int>();
      pomoIsBreak = doc["is_break"].as<bool>();
      pomoSessions = doc["sessions"].as<int>();
      pomoActive = doc["active"].as<bool>();
      redrawPomodoroPartial();
    }
  } else if (msg.startsWith("NOTIF:")) {
    String json = msg.substring(6);
    DynamicJsonDocument doc(1024);
    if (!deserializeJson(doc, json)) {
      JsonArray array = doc.as<JsonArray>();
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

      if (notifTotal > 0 && currentState == STATE_EYES) {
        showFlashNotification(notifTitles[0]);
      }
    }
  } else if (msg.startsWith("NOTES:")) {
    String json = msg.substring(6);
    DynamicJsonDocument doc(1024);
    if (!deserializeJson(doc, json)) {
      JsonArray array = doc.as<JsonArray>();
      for (int i = 0; i < 4; i++) {
        if (i < (int)array.size()) currentNotes[i] = array[i].as<String>();
        else currentNotes[i] = "";
      }
      redrawNotesPartial();
    }
  } else if (msg.startsWith("TIME:")) {
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
    String json = msg.substring(7);
    DynamicJsonDocument doc(1024);
    if (!deserializeJson(doc, json)) {
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
        JsonArray wl = doc["wl"];
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
    String json = msg.substring(7);
    DynamicJsonDocument doc(512);
    if (!deserializeJson(doc, json)) {
      if (doc.containsKey("title")) SAFESTR(gameTitle, doc["title"], 24);
      if (doc.containsKey("session")) SAFESTR(sessionTime, doc["session"], 12);
      if (doc.containsKey("achieve")) achievePct = doc["achieve"].as<uint8_t>();
      if (doc.containsKey("friends")) friendsOnline = doc["friends"].as<uint8_t>();
      if (doc.containsKey("active")) gameActive = doc["active"].as<bool>();
      if (doc.containsKey("status")) SAFESTR(gameStatus, doc["status"], 16);

      // 🟡 GAP-01 FIX: Parse recent games list
      if (doc.containsKey("recent")) {
        JsonArray recent = doc["recent"];
        for (int i = 0; i < min((int)recent.size(), 3); i++) {
          SAFESTR(recentGame[i], recent[i]["name"], 24);
          recentPlaytime[i] = recent[i]["time"].as<uint16_t>();
        }
      }

      redrawGamingPartial();
    }
  } else if (msg.startsWith("SOCIAL:")) {
    String json = msg.substring(7);
    DynamicJsonDocument doc(512);
    if (!deserializeJson(doc, json)) {
      if (doc.containsKey("user")) SAFESTR(socialUser, doc["user"], 16);
      if (doc.containsKey("app")) SAFESTR(socialApp, doc["app"], 12);
      if (doc.containsKey("body")) SAFESTR(socialBody, doc["body"], 80);
      if (doc.containsKey("time")) SAFESTR(socialTime, doc["time"], 8);
      if (doc.containsKey("likes")) socialLikes = doc["likes"].as<uint16_t>();
      if (doc.containsKey("comments")) socialComments = doc["comments"].as<uint16_t>();
      redrawSocialPartial();
    }
  } else if (msg.startsWith("TASKS:")) {
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
      if (doc.containsKey("progress")) taskProgressPct = doc["progress"].as<uint8_t>();
      redrawProductivityPartial();
    }
  }
  // ═══════════════════════════════════════════════════════
  // V7: New Command Handlers
  // ═══════════════════════════════════════════════════════

  // THOUGHT: — PC-pushed thought bubble override
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
    // Commands from PC bridge:
    // HACK:SCAN_RESULT:{json} — WiFi scan results from PC
    // HACK:PORT_RESULT:{json} — Port scan results from PC
    // Currently a placeholder — Dr. Hack mode uses on-device scanning
    Serial.print(F("HACK CMD: "));
    Serial.println(msg.substring(5));
  }

  // BTN: — Web remote virtual button press
  else if (msg.startsWith("BTN:")) {
    String btn = msg.substring(4);
    handleVirtualButton(btn);
  }

  // PAGE: — Direct page navigation from web remote
  else if (msg.startsWith("PAGE:")) {
    int page = msg.substring(5).toInt();
    if (page >= 0 && page < STATE_COUNT) {
      extern void changePage(int delta);
      int delta = page - (int)currentState;
      if (delta != 0) changePage(delta);
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
      EEPROM.begin(EEPROM_SIZE);
      EEPROM.write(EEPROM_ACTIVE_THEME_ADDR, activeTheme);
      EEPROM.commit();
      EEPROM.end();
      renderCurrentPage();
    }
  }
}

#endif
