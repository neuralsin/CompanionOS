// ═══════════════════════════════════════════════════════════
// COMPANION OS v7.0 — GLOBAL VARIABLES & TYPES
// ═══════════════════════════════════════════════════════════
#ifndef GLOBALS_H
#define GLOBALS_H

#include <TFT_eSPI.h>
#if HAS_TOUCH
  #include <XPT2046_Touchscreen.h>
#endif
#include <WiFiUdp.h>
#include <EEPROM.h>
#include "config.h"

// ═══════════════════════════════════════════════════════════
// RESOLUTION-AWARE LAYOUT HELPERS
// 🔴 BUG-04 FIX: Use float intermediate to prevent integer
// truncation destroying small values (SCALE_X(1)=0 etc.)
// Added SCALE_MIN for guaranteed minimum 1px output.
// ═══════════════════════════════════════════════════════════

#define SCR_W       SCREEN_W
#define SCR_H       SCREEN_H
#define SCR_CX      (SCREEN_W / 2)
#define SCR_CY      (SCREEN_H / 2)
#define SCALE_X(v)  ((int)((v) * ((float)SCREEN_W / 320.0f) + 0.5f))
#define SCALE_Y(v)  ((int)((v) * ((float)SCREEN_H / 240.0f) + 0.5f))
#define SCALE_MIN(v) max(1, SCALE_X(v))   // enforce minimum 1px for borders/dots

// ═══════════════════════════════════════════════════════════
// GLOBAL OBJECTS
// ═══════════════════════════════════════════════════════════

TFT_eSPI tft = TFT_eSPI();
#if HAS_TOUCH
  XPT2046_Touchscreen ts(TOUCH_CS);
#endif
WiFiUDP udp;

// ═══════════════════════════════════════════════════════════
// V7 PAGE ARCHITECTURE — 13 pages (+ Dr. Hack on ESP32)
// ═══════════════════════════════════════════════════════════

// [LEGACY - v6.0] AppState had 12 entries (STATE_EYES..STATE_SETTINGS + STATE_COUNT=12)
enum AppState {
  STATE_EYES,          // 0
  STATE_SPOTIFY,       // 1
  STATE_POMODORO,      // 2
  STATE_WEATHER,       // 3
  STATE_NOTIFICATIONS, // 4
  STATE_NOTES,         // 5
  STATE_STOCKS,        // 6
  STATE_GAMING,        // 7
  STATE_SOCIAL,        // 8
  STATE_PRODUCTIVITY,  // 9
  STATE_NETWORK,       // 10
  STATE_SETTINGS,      // 11
  STATE_DR_HACK,       // 12 — ESP32 only (#ifdef ESP32 guard in page_dr_hack.h)
  STATE_COUNT          // = 13 pages
};

extern AppState currentState;

// Pet Emotion State
enum Emotion {
  EMO_NEUTRAL,
  EMO_HAPPY,
  EMO_SAD,
  EMO_EXCITED,
  EMO_LOVE,
  EMO_SLEEPY,
  EMO_ANGRY,
  EMO_SURPRISED,
  EMO_COUNT
};

extern Emotion currentEmotion;

// ═══════════════════════════════════════════════════════════
// DR. HACK SUB-STATE MACHINE (ESP32 only)
// ═══════════════════════════════════════════════════════════
#ifdef ESP32
enum DrHackSubState {
  DH_MENU,           // Main tool selector grid
  DH_WIFI_SCANNER,   // Scan nearby APs, RSSI, channel, encryption
  DH_PORT_SCANNER,   // TCP port scan target IP
  DH_BEACON_SPAM,    // Broadcast fake AP names
  DH_DEAUTH,         // Deauth frames (ESP32 only, legal on your own network)
  DH_PACKET_MONITOR, // Raw 802.11 packet counter by type
  DH_BT_SCANNER,     // BLE device scanner
  DH_INFO,           // System info: chip ID, MAC, free heap, CPU freq
  DH_ABOUT           // About / version screen (8th grid cell)
};

extern DrHackSubState dhCurrentState;
extern int dhCursorIndex;
#endif

// ═══════════════════════════════════════════════════════════
// THOUGHT BUBBLE ENGINE
// ═══════════════════════════════════════════════════════════

#define THOUGHT_MIN_INTERVAL_MS  (45UL * 60UL * 1000UL)  // min 45 min between thoughts
#define THOUGHT_MAX_INTERVAL_MS  (90UL * 60UL * 1000UL)  // max 90 min (random in range)
#define THOUGHT_DISPLAY_MS       (2UL * 60UL * 1000UL)   // visible for 2 minutes
#define THOUGHT_FADE_STEPS       20                       // fade in/out over 20 frames

struct ThoughtBubble {
  char text[80];           // thought text (max 80 chars)
  bool active;             // currently displayed
  unsigned long shownAt;   // millis() when shown
  uint8_t fadeAlpha;       // 0–255 opacity (simulated via color blending)
  bool fadingIn;
  bool fadingOut;
};

ThoughtBubble activeBubble = {"", false, 0, 0, false, false};
char overrideThought[80] = "";  // PC-pushed thought (THOUGHT: command)

// ═══════════════════════════════════════════════════════════
// SENSOR & SYSTEM STATE
// ═══════════════════════════════════════════════════════════

// LDR sensor reading
int ldrValue = 512;  // Default mid-brightness
bool ldrEnabled = true;

// System Time
int displayHour = 0;
int displayMinute = 0;
int displaySecond = 0;
bool timeReceived = false;
unsigned long lastTimeUpdateMillis = 0;

// ═══════════════════════════════════════════════════════════
// V6 DATA: Stocks / Gaming / Social / Productivity
// ═══════════════════════════════════════════════════════════

// Stocks
char stockSymbol[16]  = "NVDA";
char stockPrice[16]   = "---";
char stockDelta[16]   = "";
char stockPctChg[16]  = "";
bool stockIsUp        = true;
int16_t stockHistory[40];       // 40-point sparkline stored as int16 cents
uint8_t stockHistoryLen = 0;
char wlSymbol[3][16]  = {"AAPL","MSFT","TSLA"};
char wlPrice[3][16]   = {"---","---","---"};
char wlDelta[3][16]   = {"","",""};
bool wlIsUp[3]        = {true,true,true};

// Gaming
char gameTitle[24]    = "";
char sessionTime[12]  = "00:00";
uint8_t achievePct    = 0;
uint8_t friendsOnline = 0;
bool gameActive       = false;
char gameStatus[16]   = "Offline";

// 🟡 GAP-01 FIX: Add recent games list for gaming dashboard
char recentGame[3][24] = {"", "", ""};
uint16_t recentPlaytime[3] = {0, 0, 0};  // minutes

// Social
char socialUser[16]   = "";
char socialApp[12]    = "";
char socialBody[80]   = "";
char socialTime[8]    = "";
uint16_t socialLikes  = 0;
uint16_t socialComments = 0;

// Productivity
char taskCurrent[32]      = "";
char taskCurrentTime[20]  = "";
char taskNext1[32]        = "";
char taskNext1Time[16]    = "";
char taskNext2[32]        = "";
char taskNext2Time[16]    = "";
bool taskActive           = false;
uint8_t taskProgressPct   = 0;

// ═══════════════════════════════════════════════════════════
// CONNECTION STATE
// ═══════════════════════════════════════════════════════════

bool wifiConnected = false;
#ifdef ESP32
  bool btConnected = false;     // 🔵 MIN-01: ideally in network.h but kept here for extern access
#endif
bool musicPlaying = false;
int notifCount = 0;

// Transport capability flag
// 🟠 CRIT-02 FIX: BT cannot carry binary image streams
// Only WiFi supports binary chunk protocol (0xFE prefix)
#define transportCanStream (wifiConnected)

// ═══════════════════════════════════════════════════════════
// SHARED BUFFERS — Album Art / Cover / Gallery
// 🟠 CRIT-05 FIX: Macro-defined size for both platforms
// ═══════════════════════════════════════════════════════════

#define ALBUM_ART_W  96
#define ALBUM_ART_H  96
uint16_t albumArt[ALBUM_ART_W * ALBUM_ART_H];   // 18432 bytes (ESP8266 near limit, ESP32 fine)
bool albumArtReady = false;
bool receivingArt = false;

// Shared Spotify Lyrics
String currentLyrics = "";
String currentLyricsLine2 = "";
String prevLyricsLine = "";

// ═══════════════════════════════════════════════════════════
// DESKTOP PET PERSONALITY
// ═══════════════════════════════════════════════════════════

unsigned long lastInteractionTime = 0;
int petMoodLevel = 100;          // 100 = happy, 0 = bored/sleepy
unsigned long lastMoodDecay = 0;
bool isYawning = false;
unsigned long yawnStart = 0;

// ═══════════════════════════════════════════════════════════
// ANTIGRAVITY AGENT STATUS
// ═══════════════════════════════════════════════════════════

String agentStatusText = "";
String agentStatus = "";          // "thinking", "done", "error", ""
unsigned long agentStatusStart = 0;
bool agentOverlayActive = false;

// ═══════════════════════════════════════════════════════════
// GALLERY / SLIDESHOW
// ═══════════════════════════════════════════════════════════

uint16_t* galleryImage = albumArt;
bool galleryReady = false;
int galleryIndex = 0;
unsigned long lastGalleryAdvance = 0;

// Weather code (shared for rain/snow overlay)
extern int weatherCode;

// 🟡 GAP-04: exoticMode is dead code. Kept as compile-safe
// no-op define. Grep shows no functional reads beyond render
// conditionals that always evaluate false.
#define exoticMode false

// Theme System
// 0 = Original (Theme 1), 1 = Theme 2 (alternate eyes + spotify skin)
int activeTheme = 0;

#endif
