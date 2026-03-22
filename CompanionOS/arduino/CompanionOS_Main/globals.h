#ifndef GLOBALS_H
#define GLOBALS_H

#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <WiFiUdp.h>
#include "config.h"

// ═══════════════════════════════════════════════════════════
// GLOBAL OBJECTS
// ═══════════════════════════════════════════════════════════

TFT_eSPI tft = TFT_eSPI();
XPT2046_Touchscreen ts(TOUCH_CS);
WiFiUDP udp;

// V3 Page Architecture
enum AppState {
  STATE_EYES,
  STATE_SPOTIFY,
  STATE_POMODORO,
  STATE_WEATHER,
  STATE_NOTIFICATIONS,
  STATE_NOTES,
  STATE_SETTINGS,
  STATE_COUNT  // = 7 pages
};

extern AppState currentState;

// LDR sensor reading (0-1024, shared with MIC on A0 or separate wire)
int ldrValue = 512;  // Default mid-brightness
bool ldrEnabled = true;

// Status bar state
bool wifiConnected = false;
bool musicPlaying = false;
int notifCount = 0;

// Shared Spotify Memory
uint16_t albumArt[9216];
bool albumArtReady = false;
bool receivingArt = false;

// Shared Spotify Lyrics
String currentLyrics = "";
String currentLyricsLine2 = "";
String prevLyricsLine = "";

#endif
