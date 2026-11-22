#ifndef COMPANION_TEST_HOST_GLOBALS_H
#define COMPANION_TEST_HOST_GLOBALS_H

// Quoted includes resolve beside the firmware header first. Defining its actual
// guard is intentional: no Arduino/WiFi hardware headers enter this translation unit.
#define GLOBALS_H
#include "host_tft.h"

#ifndef SCREEN_W
#define SCREEN_W 160
#endif
#ifndef SCREEN_H
#define SCREEN_H 128
#endif
#define SCR_W SCREEN_W
#define SCR_H SCREEN_H
#define SCR_CX (SCREEN_W / 2)
#define SCR_CY (SCREEN_H / 2)
#define SCALE_X(v) ((int)((v) * ((float)SCREEN_W / 320.0f) + 0.5f))
#define SCALE_Y(v) ((int)((v) * ((float)SCREEN_H / 240.0f) + 0.5f))
#define SCALE_MIN(v) max(1, SCALE_X(v))
constexpr int UI_HEADER_H = 16, UI_FOOTER_H = 10;
constexpr uint16_t CLR_BG=0x0000, CLR_SURFACE=0x1082, CLR_SURFACE_2=0x18A3,
  CLR_BORDER=0x2104, CLR_PRIMARY=0x347F, CLR_SECONDARY=0xF800,
  CLR_DANGER=0xF800, CLR_SUCCESS=0x36E9, CLR_WARNING=0xFFE0,
  CLR_TEXT_HI=0xFFFF, CLR_TEXT_MED=0xC618, CLR_TEXT_LO=0x8410,
  TFT_WHITE=0xFFFF, TFT_BLACK=0, TFT_GREEN=0x07E0, TFT_CYAN=0x07FF, TFT_YELLOW=0xFFE0, TFT_RED=0xF800;
constexpr uint16_t COLOR_BG=CLR_BG;
constexpr int TL_DATUM=0, TC_DATUM=1, TR_DATUM=2, ML_DATUM=3, MC_DATUM=4,
  MR_DATUM=5, BL_DATUM=6, BC_DATUM=7, BR_DATUM=8;
HostTFT tft(SCREEN_W, SCREEN_H);
enum AppState { STATE_EYES, STATE_SPOTIFY, STATE_LYRICS, STATE_POMODORO, STATE_WEATHER,
  STATE_NOTIFICATIONS, STATE_NOTES, STATE_STOCKS, STATE_GAMING, STATE_SOCIAL,
  STATE_PRODUCTIVITY, STATE_NETWORK, STATE_SETTINGS, STATE_PC_DASH, STATE_RUVIEW,
  STATE_CLOCK_DASHBOARD, STATE_RETRO_WATCH, STATE_DR_HACK, STATE_CALENDAR, STATE_TIMETABLE, STATE_COUNT };
AppState currentState = STATE_EYES;
bool virtualSelectPressed=false, wifiConnected=false, musicPlaying=false, isPlaying=false,
  timeReceived=false, albumArtReady=false;
int displayHour=12, displayMinute=34, displaySecond=56;
String currentTrack, currentArtist, currentLyrics, currentLyricsLine2, currentWiFiSSID;
int playProgress=0, playDuration=0;
char gameTitle[24]="", sessionTime[12]="", gameStatus[16]="";
uint8_t achievePct=0, friendsOnline=0;
bool gameActive=false;
char recentGame[3][24]={};
uint16_t recentPlaytime[3]={};
char socialUser[16]="", socialApp[12]="", socialBody[80]="", socialTime[8]="";
uint16_t socialLikes=0, socialComments=0;
char taskCurrent[32]="", taskCurrentTime[20]="", taskNext1[32]="", taskNext1Time[16]="",
  taskNext2[32]="", taskNext2Time[16]="";
bool taskActive=false;
uint8_t taskProgressPct=0;
char stockSymbol[16]="", stockPrice[16]="", stockDelta[16]="", stockPctChg[16]="";
bool stockIsUp=true;
int16_t stockHistory[40]={};
uint8_t stockHistoryLen=0;
char wlSymbol[3][16]={}, wlPrice[3][16]={}, wlDelta[3][16]={};
bool wlIsUp[3]={true,true,true};
constexpr int WL_CONNECTED=3;
struct HostIP {
  String value="255.255.255.255";
  String toString() const { return value; }
};
struct HostWiFi {
  bool connected=false;
  int rssi=-65;
  String ssid, bssid="FF:FF:FF:FF:FF:FF", mac="FF:FF:FF:FF:FF:FF";
  HostIP local, gateway, subnet, dns;
  int status() const { return connected ? WL_CONNECTED : 0; }
  int RSSI() const { return rssi; }
  String SSID() const { return ssid; }
  String BSSIDstr() const { return bssid; }
  String macAddress() const { return mac; }
  int channel() const { return 13; }
  HostIP localIP() const { return local; }
  HostIP gatewayIP() const { return gateway; }
  HostIP subnetMask() const { return subnet; }
  HostIP dnsIP() const { return dns; }
} WiFi;
struct HostESP { int getFreeHeap() const { return 214748; } int getCpuFreqMHz() const { return 240; } } ESP;
int hostPageIndicatorCalls=0;
#ifdef COMPANION_HOST_REAL_UI
void drawPageIndicator(int,int);
#else
void drawPageIndicator(int, int) { ++hostPageIndicatorCalls; }
#endif
void handleNetwork() {}

constexpr double PI=3.14159265358979323846;
constexpr int THEME_COUNT=3;
#define HAS_TOUCH false
#define ALBUM_ART_W 64
#define ALBUM_ART_H 64
#define ALBUM_ART_PIXELS (ALBUM_ART_W * ALBUM_ART_H)
#define ALBUM_ART_BYTES (ALBUM_ART_PIXELS * sizeof(uint16_t))
uint16_t* albumArt=static_cast<uint16_t*>(calloc(ALBUM_ART_PIXELS, sizeof(uint16_t)));
uint16_t* albumArtIncoming=nullptr;
uint16_t* galleryImage=albumArt;
uint8_t albumArtRowsReceived[ALBUM_ART_H]={};
uint8_t albumArtRowsComplete=0;
uint32_t albumArtRevision=0, uiPageGeneration=0;
int activeTheme=0, notifCount=0, ldrValue=512;
bool receivingArt=false, customEyeActive=false, customEyeReady=false, agentOverlayActive=false;
#include "../arduino/CompanionOS_Main/art_memory.h"
uint16_t* customEyeImg=nullptr;
String currentNotes[4], prevLyricsLine, agentStatus, agentStatusText, standaloneSpotifyStatus, standaloneWeatherStatus;
String standaloneLyricsStatus;
bool standaloneLyricsSynced = true;
unsigned long agentStatusStart=0;
struct ThoughtBubble { bool active=false, fadingOut=false; uint8_t fadeAlpha=0; } activeBubble;

#endif
