// ═══════════════════════════════════════════════════════════
// COMPANION OS v7.0 — GLOBAL VARIABLES & TYPES
// ═══════════════════════════════════════════════════════════
#ifndef GLOBALS_H
#define GLOBALS_H

#include "config.h"
#include <TFT_eSPI.h>
#if HAS_TOUCH
  #include <XPT2046_Touchscreen.h>
#endif

// ESP32 Core 3.x: Network library must be loaded BEFORE any WiFi headers.
// WiFiGeneric.h uses Network types (network_event_handle_t, NetworkInterface,
// arduino_event_id_t) via macros without including them.
#ifdef ESP32
#include <NetworkInterface.h>
#include <NetworkEvents.h>
#include <NetworkManager.h>
#include <NetworkClient.h>
#include <NetworkServer.h>
#include <NetworkUdp.h>
#endif

// FS namespace fix: WebServer.h uses bare 'FS' but <FS.h> puts it in fs::
#include <FS.h>
using fs::FS;
using fs::File;

#include <WiFiUdp.h>
#include <EEPROM.h>

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
  STATE_RUVIEW,        // 12 — RuView CSI Presence Detection
  STATE_DR_HACK,       // 13 — ESP32 only (#ifdef ESP32 guard in page_dr_hack.h)
  STATE_COUNT          // = 14 pages
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
// 16 tools across 2 pages of 8 tools each
// ═══════════════════════════════════════════════════════════
#ifdef ESP32

#define DH_TOOLS_PER_PAGE 8
#define DH_MENU_PAGES     6
#define DH_TOTAL_TOOLS    48

enum DrHackSubState {
  DH_MENU,
  // Page 1: WiFi Tools
  DH_WIFI_SCANNER,     DH_CHANNEL_SCAN,     DH_WIFI_RADAR,       DH_WIFI_DIRECTION,
  DH_BEACON_SPAM,      DH_DEAUTH,           DH_EVIL_PORTAL,      DH_PROBE_SNIFFER,
  // Page 2: WiFi+ & BLE
  DH_KARMA,            DH_WIFI_CONFIG,       DH_BT_SCANNER,       DH_BLE_INSPECTOR,
  DH_BLE_SPAM,         DH_BT_DISRUPTOR,      DH_IPHONE_REMOTE,    DH_BT_JAMMER,
  // Page 3: Radio & IR
  DH_JAMMER_24,        DH_RADIO_SCANNER,     DH_IR_CAPTURE,        DH_IR_REPLAY,
  DH_IR_TX_TEST,       DH_IR_ANALYZER,       DH_IR_SNIFFER,        DH_IR_PROTOCOL,
  // Page 4: IR+ & CC1101
  DH_IR_NIGHT,         DH_IR_PROXIMITY,      DH_IR_REMOTES,        DH_IR_SAVED,
  DH_CC_DIAG,          DH_CC_SPECTRUM,        DH_CC_WATERFALL,      DH_CC_FREQ_MON,
  // Page 5: CC1101+ & RF
  DH_CC_FINDER,        DH_CC_BRUTE,          DH_CC_CODE_CHECK,     DH_CC_RF_ANALYZE,
  DH_CC_RAW_VIEW,      DH_CC_RF_LIVE,        DH_CC_LAB_REPLAY,     DH_CC_TEST_BEACON,
  // Page 6: System
  DH_PORT_SCANNER,     DH_PACKET_MONITOR,    DH_INFO,              DH_WEB_DASHBOARD,
  DH_HW_DIAG,          DH_INPUT_MONITOR,     DH_ABOUT,             DH_RFCLOWN_JAM
};

extern DrHackSubState dhCurrentState;
extern int dhCursorIndex;
extern int dhMenuPage;
#endif

// ═══════════════════════════════════════════════════════════
// THOUGHT BUBBLE ENGINE
// ═══════════════════════════════════════════════════════════

#define THOUGHT_MIN_INTERVAL_MS  (5UL * 60UL * 1000UL)   // min 5 min between thoughts
#define THOUGHT_MAX_INTERVAL_MS  (20UL * 60UL * 1000UL)  // max 20 min (random in range)
#define THOUGHT_DISPLAY_MS       (5UL * 60UL * 1000UL)   // visible for 5 minutes
#define THOUGHT_FADE_STEPS       20                       // fade in/out over 20 frames

extern bool thoughtSchedulerActive;
extern unsigned long nextThoughtTime;

extern uint16_t* customEyeImg;
extern bool customEyeActive;
extern bool customEyeReady;
extern uint16_t spotifyArtCache[4096]; // 64x64 cache for Spotify persistence

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

#define ALBUM_ART_W  64
#define ALBUM_ART_H  64
uint16_t albumArt[ALBUM_ART_W * ALBUM_ART_H];   // 18432 bytes (ESP8266 near limit, ESP32 fine)
bool albumArtReady = false;
bool receivingArt = false;
uint8_t albumArtRowsReceived[ALBUM_ART_H] = {0};
uint8_t albumArtRowsComplete = 0;

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
// VIRTUAL BUTTON FLAGS (for remote controller)
// ═══════════════════════════════════════════════════════════
// Virtual Buttons from Web Remote
bool virtualSelectPressed = false;
bool virtualLeftPressed = false;
bool virtualRightPressed = false;
bool virtualHomePressed = false;
bool virtualUpPressed = false;
bool virtualDownPressed = false;

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
// 0 = Original (Theme 1), 1 = Theme 2 (alternate eyes + spotify skin), 2 = Theme 3 (RoboEyes)
#define THEME_COUNT 3
int activeTheme = 0;
int t3EyeVariant = 0;  // Theme 3 eye variant index (0–5)

#endif
