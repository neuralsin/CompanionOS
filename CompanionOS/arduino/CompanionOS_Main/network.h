#ifndef NETWORK_H
#define NETWORK_H

#include "globals.h"
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h> // REQUIRED NOW for safe parsing
#include "eyes.h"

// ═══════════════════════════════════════════════════════════
// NETWORK FUNCTIONS
// ═══════════════════════════════════════════════════════════

extern char udpBuffer[512];
String pcIPStr = DEFAULT_PC_IP; // Fallback

// Global data states
String currentTrack = "";
String currentArtist = "";
String currentLyrics = "";
int playProgress = 0;
int playDuration = 100;
bool isPlaying = false;

String ghUser = "";
String ghRepos = "";
String ghFollowers = "";

String currentNotes[4] = {"", "", "", ""};

extern void redrawSpotifyPartial();
extern void redrawGithubPartial();
extern void redrawNotesPartial();
extern void prepareAlbumArt();
extern void processArtChunk(int chunkIdx, String hexData);
extern void completeAlbumArt();

void handleCommand(String msg);

void setupWiFi() {
  Serial.print(F("WiFi setup..."));
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(TFT_WHITE);
  tft.drawCentreString("Connecting WiFi...", SCREEN_W/2, SCREEN_H/2, 2);
  
  WiFiManager wifiManager;
  wifiManager.setDebugOutput(false);
  
  if (!wifiManager.autoConnect("CompanionOS-Setup")) {
    Serial.println("Failed to connect and hit timeout. Rebooting...");
    delay(3000);
    ESP.restart();
    delay(5000);
  }

  Serial.println(F(" OK"));
  Serial.print(F("IP: "));
  Serial.println(WiFi.localIP());
  
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(TFT_GREEN);
  tft.drawCentreString("WiFi Connected!", SCREEN_W/2, SCREEN_H/2 - 20, 2);
  tft.drawCentreString(WiFi.localIP().toString(), SCREEN_W/2, SCREEN_H/2 + 10, 2);
  
  udp.begin(UDP_PORT_RX);
  delay(1500);

  // Network Auto-Discovery Handshake (Fixes Chicken & Egg bug)
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

void handleNetwork() {
  int packetSize = udp.parsePacket();
  if (packetSize) {
    IPAddress remoteIP = udp.remoteIP();
    pcIPStr = remoteIP.toString();
    
    // We allocated 512, ensure we don't overflow
    int readLen = min(packetSize, 511);
    int len = udp.read(udpBuffer, readLen);
    udpBuffer[len] = 0;
    String msg = String(udpBuffer);
    
    handleCommand(msg);
  }
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
    }
    else if (msg.startsWith("TRACK:")) {
      String json = msg.substring(6);
      DynamicJsonDocument doc(1024);
      if (!deserializeJson(doc, json)) {
        currentTrack = doc["track"].as<String>();
        currentArtist = doc["artist"].as<String>();
        playDuration = doc["duration"].as<int>();
        redrawSpotifyPartial();
      }
    }
    else if (msg.startsWith("STATE:")) {
      String json = msg.substring(6);
      DynamicJsonDocument doc(512);
      if (!deserializeJson(doc, json)) {
        playProgress = doc["progress"].as<int>();
        isPlaying = doc["playing"].as<bool>();
        redrawSpotifyPartial();
      }
    }
    else if (msg.startsWith("LYRICS:")) {
      String json = msg.substring(7);
      DynamicJsonDocument doc(1024);
      if (!deserializeJson(doc, json)) {
        JsonArray array = doc.as<JsonArray>();
        if (array.size() > 0) {
          currentLyrics = array[0].as<String>();
        } else {
          currentLyrics = "♪ Instrumental ♪";
        }
        redrawSpotifyPartial();
      }
    }
    else if (msg.startsWith("GITHUB:")) {
      String json = msg.substring(7);
      DynamicJsonDocument doc(1024);
      if (!deserializeJson(doc, json)) {
        ghUser = doc["username"].as<String>();
        ghRepos = doc["repos"].as<String>();
        ghFollowers = doc["followers"].as<String>();
        redrawGithubPartial();
      }
    }
    else if (msg.startsWith("NOTES:")) {
      String json = msg.substring(6);
      DynamicJsonDocument doc(1024);
      if (!deserializeJson(doc, json)) {
        JsonArray array = doc.as<JsonArray>();
        for(int i=0; i<4; i++) {
          if (i < array.size()) currentNotes[i] = array[i].as<String>();
          else currentNotes[i] = "";
        }
        redrawNotesPartial();
      }
    }
    else if (msg.startsWith("ART_START:")) {
      prepareAlbumArt();
    }
    else if (msg.startsWith("ART_CHUNK:")) {
      int firstColon = msg.indexOf(':', 10);
      int chunkIdx = msg.substring(10, firstColon).toInt();
      String hexData = msg.substring(firstColon + 1);
      processArtChunk(chunkIdx, hexData);
    }
    else if (msg.startsWith("ART_COMPLETE")) {
      completeAlbumArt();
    }
    else if (msg.startsWith("TIME:")) {
      String json = msg.substring(5);
      DynamicJsonDocument doc(256);
      if (!deserializeJson(doc, json)) {
        displayHour = doc["h"].as<int>();
        displayMinute = doc["m"].as<int>();
        bootMillis = millis();
        timeReceived = true;
        if (currentState == STATE_EYES) drawTimeDisplay();
      }
    }
}

#endif
