#ifndef NETWORK_H
#define NETWORK_H

#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>

#include "globals.h"
#include "eyes.h"

// ═══════════════════════════════════════════════════════════
// NETWORK FUNCTIONS V3
// ═══════════════════════════════════════════════════════════

extern char udpBuffer[512];
String pcIPStr = DEFAULT_PC_IP;

// Global data states
String currentTrack = "";
String currentArtist = "";
extern String currentLyrics;
extern String currentLyricsLine2;
extern String prevLyricsLine;
int playProgress = 0;
int playDuration = 100;
bool isPlaying = false;
extern bool artDrawn; // Added this line

String currentNotes[4] = {"", "", "", ""};

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

// Safe string extraction to prevent nullptr crash on ESP
#define SAFESTR(dest, src, sz) do { const char* _s = (src).as<const char*>(); if (_s) { strncpy(dest, _s, sz-1); dest[sz-1] = '\0'; } } while(0)

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

void setupWiFi() {
  Serial.print(F("WiFi setup..."));
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(TFT_WHITE);
  tft.drawCentreString("Connecting WiFi...", SCREEN_W/2, SCREEN_H/2, 2);
  
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  int timeout = 0;
  // Increase timeout to 60 to allow 30 seconds for slower routers to handshake
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
    
    tft.fillScreen(COLOR_BG);
    tft.setTextColor(TFT_GREEN);
    tft.drawCentreString("WiFi Connected!", SCREEN_W/2, SCREEN_H/2 - 20, 2);
    tft.drawCentreString(WiFi.localIP().toString(), SCREEN_W/2, SCREEN_H/2 + 10, 2);
  } else {
    Serial.println(F(" FAILED"));
    tft.setTextColor(TFT_RED);
    tft.drawCentreString("WiFi Failed!", SCREEN_W/2, SCREEN_H/2 - 20, 2);
  }
  
  udp.begin(UDP_PORT_RX);
  delay(1500);

  // Auto-discovery handshake
  udp.beginPacket("255.255.255.255", UDP_PORT_TX);
  const char* hello = "HELLO_COMPANION";
  udp.write(hello, strlen(hello));
  udp.endPacket();
}

void sendCommand(String cmd) {
  udp.beginPacket(pcIPStr.c_str(), UDP_PORT_TX);
  udp.write(cmd.c_str());
  udp.endPacket();
  Serial.print(F("Sent: "));
  Serial.println(cmd);
}

bool pcFound = false;
unsigned long lastDiscoveryShout = 0;

void handleNetwork() {
  // Auto-discovery
  if (!pcFound && millis() - lastDiscoveryShout > 3000) {
    udp.beginPacket("255.255.255.255", UDP_PORT_TX);
    const char* hello = "HELLO_COMPANION";
    udp.write(hello, strlen(hello));
    udp.endPacket();
    lastDiscoveryShout = millis();
  }

  int packetSize = udp.parsePacket();
  if (packetSize) {
    pcFound = true;
    IPAddress remoteIP = udp.remoteIP();
    pcIPStr = remoteIP.toString();
    
    int readLen = min(packetSize, 511);
    int len = udp.read(udpBuffer, readLen);
    
    // ── V4 OPTIMIZATION: BARE-METAL BINARY PACKET SNIFFING ──
    // If the first byte is 0xFE, this is a raw structured image packet, bypassing String handling entirely
    if (len > 3 && (unsigned char)udpBuffer[0] == 0xFE) {
      int chunkIdx = ((unsigned char)udpBuffer[1] << 8) | ((unsigned char)udpBuffer[2]);
      int pixelsInChunk = (len - 3) / 2;
      int imgWidth = (currentState == STATE_GAMING) ? 100 : 96;
      int pixels_per_chunk = imgWidth * 2;
      int startPixel = chunkIdx * pixels_per_chunk;
      
      if (startPixel < 9216) {
        uint16_t* dest = albumArt + startPixel;
        
        // Unpack RGB565 with Native Endianness Flipping (L, H)
        for (int i = 0; i < pixelsInChunk; i++) {
          int offset = 3 + (i * 2);
          dest[i] = (unsigned char)udpBuffer[offset] | ((unsigned char)udpBuffer[offset+1] << 8); 
        }
        
        // Instant Progressive Hardware Rendering straight onto the physical display
        int imgX = (currentState == STATE_GAMING) ? 27 : 10;
        int imgY = (currentState == STATE_GAMING) ? 32 : 25;
        
        int yStart = startPixel / imgWidth;
        int maxRows = pixelsInChunk / imgWidth;
        
        if (currentState == STATE_SPOTIFY || currentState == STATE_GAMING) {
          if (maxRows > 0) {
             tft.pushImage(imgX, imgY + yStart, imgWidth, maxRows, dest);
          }
        }
      }
      return; // Bypass the expensive String payload parsing
    }
    
    // Standard string packet fallback logic
    udpBuffer[len] = 0;
    String msg = String(udpBuffer);
    
    handleCommand(msg);
  }
  
  // Update WiFi status for status bar
  wifiConnected = (WiFi.status() == WL_CONNECTED);
}

void handleCommand(String msg) {
    if (msg.startsWith("EMOTION:")) {
      String emo = msg.substring(8);
      if (emo == "HAPPY") setEmotion(EMO_HAPPY);
      else if (emo == "SAD") setEmotion(EMO_SAD);
      else if (emo == "EXCITED") setEmotion(EMO_EXCITED);
      else if (emo == "LOVE") setEmotion(EMO_LOVE);
      else if (emo == "SLEEPY") setEmotion(EMO_SLEEPY);
      else if (emo == "ANGRY") setEmotion(EMO_ANGRY);
      else if (emo == "SURPRISED") setEmotion(EMO_SURPRISED);
      else if (emo == "NEUTRAL") setEmotion(EMO_NEUTRAL);
    }
    // ── V4: Antigravity Agent Status ──────────────────
    else if (msg.startsWith("AGENT:")) {
      String json = msg.substring(6);
      DynamicJsonDocument doc(512);
      if (!deserializeJson(doc, json)) {
        agentStatus = doc["status"].as<String>();
        agentStatusText = doc["text"].as<String>();
        agentStatusStart = millis();
        agentOverlayActive = true;
        lastInteractionTime = millis();
        
        if (agentStatus == "thinking") setEmotion(EMO_EXCITED);
        else if (agentStatus == "done") setEmotion(EMO_HAPPY);
        else if (agentStatus == "error") setEmotion(EMO_SAD);
      }
    }
    else if (msg.startsWith("TRACK:")) {
      String json = msg.substring(6);
      DynamicJsonDocument doc(1024);
      if (!deserializeJson(doc, json)) {
        currentTrack = doc["track"].as<String>();
        currentArtist = doc["artist"].as<String>();
        playDuration = doc["duration"].as<int>();
        musicPlaying = true;
        redrawSpotifyPartial();
      }
    }
    else if (msg.startsWith("ART_START:")) {
      receivingArt = true;
      albumArtReady = false;
      artDrawn = false;
    }
    else if (msg.startsWith("ART_COMPLETE:")) {
      completeAlbumArt();
    }
    else if (msg.startsWith("STATE:")) {
      String json = msg.substring(6);
      DynamicJsonDocument doc(512);
      if (!deserializeJson(doc, json)) {
        playProgress = doc["progress"].as<int>();
        isPlaying = doc["playing"].as<bool>();
        musicPlaying = isPlaying;
        redrawSpotifyPartial();
      }
    }
    else if (msg.startsWith("LYRICS:")) {
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
        redrawSpotifyPartial();
      }
    }
    else if (msg.startsWith("WEATHER:")) {
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
    }
    else if (msg.startsWith("POMO:")) {
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
    }
    else if (msg.startsWith("NOTIF:")) {
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
    }
    else if (msg.startsWith("NOTES:")) {
      String json = msg.substring(6);
      DynamicJsonDocument doc(1024);
      if (!deserializeJson(doc, json)) {
        JsonArray array = doc.as<JsonArray>();
        for(int i = 0; i < 4; i++) {
          if (i < (int)array.size()) currentNotes[i] = array[i].as<String>();
          else currentNotes[i] = "";
        }
        redrawNotesPartial();
      }
    }
    else if (msg.startsWith("TIME:")) {
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
    // V6: New Page Data Handlers
    // ═══════════════════════════════════════════════════════
    else if (msg.startsWith("STOCKS:")) {
      String json = msg.substring(7);
      DynamicJsonDocument doc(1024);
      if (!deserializeJson(doc, json)) {
        if (doc.containsKey("symbol")) SAFESTR(stockSymbol, doc["symbol"], 16);
        if (doc.containsKey("price"))  SAFESTR(stockPrice, doc["price"], 16);
        if (doc.containsKey("delta"))  SAFESTR(stockDelta, doc["delta"], 16);
        if (doc.containsKey("pct"))    SAFESTR(stockPctChg, doc["pct"], 16);
        if (doc.containsKey("up"))     stockIsUp = doc["up"].as<bool>();
        
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
    }
    else if (msg.startsWith("GAMING:")) {
      String json = msg.substring(7);
      DynamicJsonDocument doc(512);
      if (!deserializeJson(doc, json)) {
        if (doc.containsKey("title"))   SAFESTR(gameTitle, doc["title"], 24);
        if (doc.containsKey("session")) SAFESTR(sessionTime, doc["session"], 12);
        if (doc.containsKey("achieve")) achievePct = doc["achieve"].as<uint8_t>();
        if (doc.containsKey("friends")) friendsOnline = doc["friends"].as<uint8_t>();
        if (doc.containsKey("active"))  gameActive = doc["active"].as<bool>();
        if (doc.containsKey("status"))  SAFESTR(gameStatus, doc["status"], 16);
        redrawGamingPartial();
      }
    }
    else if (msg.startsWith("SOCIAL:")) {
      String json = msg.substring(7);
      DynamicJsonDocument doc(512);
      if (!deserializeJson(doc, json)) {
        if (doc.containsKey("user"))     SAFESTR(socialUser, doc["user"], 16);
        if (doc.containsKey("app"))      SAFESTR(socialApp, doc["app"], 12);
        if (doc.containsKey("body"))     SAFESTR(socialBody, doc["body"], 80);
        if (doc.containsKey("time"))     SAFESTR(socialTime, doc["time"], 8);
        if (doc.containsKey("likes"))    socialLikes = doc["likes"].as<uint16_t>();
        if (doc.containsKey("comments")) socialComments = doc["comments"].as<uint16_t>();
        redrawSocialPartial();
      }
    }
    else if (msg.startsWith("TASKS:")) {
      String json = msg.substring(6);
      DynamicJsonDocument doc(1024);
      if (!deserializeJson(doc, json)) {
        if (doc.containsKey("current"))      SAFESTR(taskCurrent, doc["current"], 32);
        if (doc.containsKey("current_time")) SAFESTR(taskCurrentTime, doc["current_time"], 20);
        if (doc.containsKey("next1"))        SAFESTR(taskNext1, doc["next1"], 32);
        if (doc.containsKey("next1_time"))   SAFESTR(taskNext1Time, doc["next1_time"], 16);
        if (doc.containsKey("next2"))        SAFESTR(taskNext2, doc["next2"], 32);
        if (doc.containsKey("next2_time"))   SAFESTR(taskNext2Time, doc["next2_time"], 16);
        if (doc.containsKey("active"))       taskActive = doc["active"].as<bool>();
        if (doc.containsKey("progress"))     taskProgressPct = doc["progress"].as<uint8_t>();
        redrawProductivityPartial();
      }
    }
}

#endif
