#ifndef GLOBALS_H
#define GLOBALS_H

#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
#include "config.h"
#include "themes.h"

// ═══════════════════════════════════════════════════════════
// GLOBAL OBJECTS
// ═══════════════════════════════════════════════════════════

TFT_eSPI tft = TFT_eSPI();
XPT2046_Touchscreen ts(TOUCH_CS);
WiFiUDP udp;

// V6 Page Architecture — 11 pages
enum AppState {
  STATE_EYES,          // 0
  STATE_SPOTIFY,       // 1
  STATE_POMODORO,      // 2
  STATE_WEATHER,       // 3
  STATE_NOTIFICATIONS, // 4
  STATE_NOTES,         // 5
  STATE_STOCKS,        // 6  ← V6 NEW
  STATE_GAMING,        // 7  ← V6 NEW
  STATE_SOCIAL,        // 8  ← V6 NEW
  STATE_PRODUCTIVITY,  // 9  ← V6 NEW
  STATE_SETTINGS,      // 10
  STATE_COUNT          // = 11 pages
};

extern AppState currentState;

// LDR sensor reading (0-1024, shared with MIC on A0 or separate wire)
int ldrValue = 512;  // Default mid-brightness
bool ldrEnabled = true;

// ═══════════════════════════════════════════════════════════
// V6 DATA: Stocks / Gaming / Social / Productivity
// ═══════════════════════════════════════════════════════════

// ── Stocks ──
char stockSymbol[8]   = "NVDA";
char stockPrice[12]   = "---";
char stockDelta[12]   = "";
char stockPctChg[10]  = "";
bool stockIsUp        = true;
int16_t stockHistory[40];       // 40-point sparkline stored as int16 cents
uint8_t stockHistoryLen = 0;
char wlSymbol[3][8]   = {"AAPL","MSFT","TSLA"};
char wlPrice[3][12]   = {"---","---","---"};
char wlDelta[3][10]   = {"","",""};
bool wlIsUp[3]        = {true,true,true};

// ── Gaming ──
char gameTitle[24]    = "";
char sessionTime[12]  = "00:00";
uint8_t achievePct    = 0;
uint8_t friendsOnline = 0;
bool gameActive       = false;
char gameStatus[16]   = "Offline";

// ── Social ──
char socialUser[16]   = "";
char socialApp[12]    = "";
char socialBody[80]   = "";
char socialTime[8]    = "";
uint16_t socialLikes  = 0;
uint16_t socialComments = 0;

// ── Productivity ──
char taskCurrent[32]      = "";
char taskCurrentTime[20]  = "";
char taskNext1[32]        = "";
char taskNext1Time[16]    = "";
char taskNext2[32]        = "";
char taskNext2Time[16]    = "";
bool taskActive           = false;
uint8_t taskProgressPct   = 0;

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

// ═══════════════════════════════════════════════════════════
// V5 THEME SYSTEM — Replaces V4 exoticMode boolean
// (Additive only — no legacy code modified above this line)
// ═══════════════════════════════════════════════════════════

// Active theme index (0-10), persisted in EEPROM
uint8_t currentThemeId = 0;

// Backward-compat macro: ALL existing V4 code using 'exoticMode'
// continues to work — evaluates true only when Exotic theme is active
#define exoticMode (currentThemeId == 1)

// Desktop Pet Personality State
unsigned long lastInteractionTime = 0;
bool isPetting = false;
int petMoodLevel = 100;          // 100 = happy, 0 = bored/sleepy
unsigned long lastMoodDecay = 0;
bool isYawning = false;
unsigned long yawnStart = 0;

// Antigravity Agent Status
String agentStatusText = "";
String agentStatus = "";          // "thinking", "done", "error", ""
unsigned long agentStatusStart = 0;
bool agentOverlayActive = false;

// Gallery / Slideshow — shares albumArt buffer (same 96x96 RGB565 format, never simultaneous)
uint16_t* galleryImage = albumArt;  // Alias, saves 18KB RAM
bool galleryReady = false;
int galleryIndex = 0;
unsigned long lastGalleryAdvance = 0;

// Weather code (shared for rain/snow overlay in exotic mode)
extern int weatherCode;

#endif

