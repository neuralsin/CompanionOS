#ifndef PAGES_H
#define PAGES_H

#include "globals.h"
#include "ui.h"
#include "eyes.h"
#include "page_stocks.h"
#include "page_gaming.h"
#include "page_social.h"
#include "page_productivity.h"
#include "page_network.h"
#include "page_pc_dash.h"
#include "page_ruview.h"
#include "page_clock_dashboard.h"
#include "theme2_eyes.h"
#include "theme2_spotify.h"
#include "page_retro_watch.h"
#ifdef ESP32
#include "page_dr_hack.h"
#endif

// Data from network.h
extern String currentTrack;
extern String currentArtist;
extern String currentLyrics;
extern String currentLyricsLine2;
extern int playProgress;
extern int playDuration;
extern bool isPlaying;

extern String currentNotes[4];

// Weather data
String weatherCondition = "";
int weatherTemp = 0;
int weatherFeels = 0;
int weatherHumidity = 0;
int weatherWind = 0;
int weatherHigh = 0;
int weatherLow = 0;
String weatherCity = "";
String weatherSunrise = "";
String weatherSunset = "";
int weatherCode = 0;

// Pomodoro data
int pomoRemaining = 1500;
int pomoTotal = 1500;
bool pomoIsBreak = false;
int pomoSessions = 0;
bool pomoActive = false;

// Notification data
String notifApps[3] = {"", "", ""};
String notifTitles[3] = {"", "", ""};
String notifTimes[3] = {"", "", ""};
int notifTotal = 0;

// Flash notification
bool flashNotifActive = false;
unsigned long flashNotifStart = 0;
String flashNotifText = "";
bool flashNotifEnabled = true;  // Toggle from notification page

// ═══════════════════════════════════════════════════════════
// VECTOR ICON HELPERS
// ═══════════════════════════════════════════════════════════

void drawIconPlay(int cx, int cy, uint16_t color) {
  tft.fillTriangle(cx - 4, cy - 6, cx - 4, cy + 6, cx + 6, cy, color);
}
void drawIconPause(int cx, int cy, uint16_t color) {
  tft.fillRect(cx - 5, cy - 5, 3, 11, color);
  tft.fillRect(cx + 2, cy - 5, 3, 11, color);
}
void drawIconPrev(int cx, int cy, uint16_t color) {
  tft.fillRect(cx - 6, cy - 5, 2, 11, color);
  tft.fillTriangle(cx + 6, cy - 5, cx + 6, cy + 5, cx - 2, cy, color);
}
void drawIconNext(int cx, int cy, uint16_t color) {
  tft.fillTriangle(cx - 6, cy - 5, cx - 6, cy + 5, cx + 2, cy, color);
  tft.fillRect(cx + 5, cy - 5, 2, 11, color);
}
void drawIconShuffle(int cx, int cy, uint16_t color) {
  tft.drawLine(cx - 6, cy + 3, cx - 2, cy + 3, color);
  tft.drawLine(cx - 2, cy + 3, cx + 2, cy - 3, color);
  tft.drawLine(cx + 2, cy - 3, cx + 6, cy - 3, color);
  tft.drawLine(cx - 6, cy - 3, cx - 2, cy - 3, color);
  tft.drawLine(cx - 2, cy - 3, cx - 1, cy - 2, color);
  tft.drawLine(cx + 1, cy + 2, cx + 2, cy + 3, color);
  tft.drawLine(cx + 2, cy + 3, cx + 6, cy + 3, color);
  // Arrow heads
  tft.drawLine(cx + 4, cy - 5, cx + 6, cy - 3, color);
  tft.drawLine(cx + 4, cy - 1, cx + 6, cy - 3, color);
  tft.drawLine(cx + 4, cy + 1, cx + 6, cy + 3, color);
  tft.drawLine(cx + 4, cy + 5, cx + 6, cy + 3, color);
}
void drawIconRepeat(int cx, int cy, uint16_t color) {
  tft.drawLine(cx - 5, cy - 4, cx - 5, cy + 2, color);
  tft.drawLine(cx - 5, cy - 4, cx + 5, cy - 4, color);
  tft.drawLine(cx + 5, cy - 2, cx + 5, cy + 4, color);
  tft.drawLine(cx - 5, cy + 4, cx + 5, cy + 4, color);
  // Arrow heads
  tft.drawLine(cx + 3, cy - 6, cx + 5, cy - 4, color);
  tft.drawLine(cx + 3, cy - 2, cx + 5, cy - 4, color);
  tft.drawLine(cx - 3, cy + 2, cx - 5, cy + 4, color);
  tft.drawLine(cx - 3, cy + 6, cx - 5, cy + 4, color);
}
void drawIconHeart(int cx, int cy, uint16_t color) {
  tft.fillCircle(cx - 3, cy - 1, 3, color);
  tft.fillCircle(cx + 3, cy - 1, 3, color);
  tft.fillTriangle(cx - 6, cy, cx + 6, cy, cx, cy + 6, color);
}

// ═══════════════════════════════════════════════════════════
// SPOTIFY PAGE (Fixed layout)
// ═══════════════════════════════════════════════════════════

// 1:1 Spotify Replication Layout Constants
#define ALBUM_X SCALE_X(10)
#define ALBUM_Y SCALE_Y(25)
#define ALBUM_SIZE SCALE_MIN(96) // Large square album

// ── State Trackers for Spotify ──
String lastTrackTitle = "";
String lastArtist = "";
String lastLyric1 = "";
String lastLyric2 = "";
String lastPrevLyric = "";
bool artDrawn = false;
static bool controlsDrawn = false;
static bool cardDrawn = false;
static bool lastPlaying = false;
bool forceSpotifyRedraw = false;

void redrawSpotifyPartial() {
  if (currentState != STATE_SPOTIFY) return;
  const int ax = 4, ay = 18, side = 64;
  const int cx = 74, cw = SCREEN_W - cx - 4;
  String title = currentTrack.length() ? currentTrack : "Nothing playing";
  String artist = currentArtist.length() ? currentArtist : "Spotify";
  bool changed = title != lastTrackTitle || artist != lastArtist;
  static bool placeholder = false;
  static String lastServiceStatus;
  static uint32_t lastPage = 0, artVersion = 0;
  static int lastProgressWidth = -1;
  if (lastPage != uiPageGeneration || forceSpotifyRedraw || changed) {
    placeholder = false;
    artDrawn = false;
    lastProgressWidth = -1;
    lastPage = uiPageGeneration;
  }
  if (albumArtReady && albumArt && (!artDrawn || artVersion != albumArtRevision)) {
    tft.setSwapBytes(true);
    tft.pushImage(ax, ay, side, side, albumArt);
    tft.setSwapBytes(false);
    tft.drawRect(ax - 1, ay - 1, side + 2, side + 2, CLR_BORDER);
    artDrawn = true;
    artVersion = albumArtRevision;
    placeholder = false;
  } else if (!albumArtReady && !placeholder) {
    tft.fillRoundRect(ax, ay, side, side, 4, CLR_SURFACE);
    tft.drawCircle(ax + 32, ay + 32, 20, CLR_BORDER);
    tft.drawCircle(ax + 32, ay + 32, 7, CLR_TEXT_LO);
    tft.fillCircle(ax + 32, ay + 32, 2, CLR_PRIMARY);
    placeholder = true;
  }
  bool lyricsChanged = currentLyrics != lastLyric1 || currentLyricsLine2 != lastLyric2 || forceSpotifyRedraw || changed || standaloneSpotifyStatus != lastServiceStatus || isPlaying != lastPlaying;
  if (!cardDrawn || lyricsChanged) {
    tft.fillRoundRect(cx, ay, cw, side, 4, CLR_SURFACE);
    drawTruncatedText(cx + 4, ay + 4, currentLyrics.length() ? "LYRICS" : "SPOTIFY", cw - 8, CLR_PRIMARY, 1, CLR_SURFACE);
    String text = currentLyrics.length() ? currentLyrics : (currentTrack.length() ? (isPlaying ? "Now playing" : "Paused") : (wifiConnected ? "Ready for music" : "Wi-Fi offline"));
    if (!currentLyrics.length() && standaloneSpotifyStatus.length()) text = standaloneSpotifyStatus;
    int used = drawWrappedText(cx + 4, ay + 18, text.c_str(), cw - 8, 30, CLR_TEXT_HI, 1, CLR_SURFACE);
    if (currentLyricsLine2.length())
      drawWrappedText(cx + 4, ay + 18 + used * 10, currentLyricsLine2.c_str(), cw - 8, 42 - used * 10, CLR_TEXT_LO, 1, CLR_SURFACE);
    cardDrawn = true;
    lastLyric1 = currentLyrics;
    lastLyric2 = currentLyricsLine2;
    lastServiceStatus = standaloneSpotifyStatus;
  }
  if (changed || forceSpotifyRedraw) {
    tft.fillRect(4, 85, SCREEN_W - 8, 20, COLOR_BG);
    drawTruncatedText(4, 85, title.c_str(), SCREEN_W - 8, CLR_TEXT_HI, 1);
    drawTruncatedText(4, 95, artist.c_str(), SCREEN_W - 8, CLR_TEXT_MED, 1);
    lastTrackTitle = title;
    lastArtist = artist;
  }
  if (!controlsDrawn || forceSpotifyRedraw || isPlaying != lastPlaying) {
    int middle = SCREEN_W / 2;
    tft.fillRect(0, 105, SCREEN_W, 16, COLOR_BG);
    drawIconPrev(middle - 32, 112, CLR_TEXT_MED);
    drawIconNext(middle + 32, 112, CLR_TEXT_MED);
    tft.fillCircle(middle, 112, 7, CLR_TEXT_HI);
    if (isPlaying) drawIconPause(middle, 112, COLOR_BG);
    else drawIconPlay(middle + 1, 112, COLOR_BG);
    lastPlaying = isPlaying;
    controlsDrawn = true;
  }
  int bw = SCREEN_W - 16;
  int w = playDuration > 0 ? (int)((int64_t)bw * constrain(playProgress, 0, playDuration) / playDuration) : 0;
  if (lastProgressWidth != w) {
    tft.fillRect(8, SCREEN_H - 4, bw, 2, CLR_BORDER);
    if (w > 0) tft.fillRect(8, SCREEN_H - 4, w, 2, CLR_SUCCESS);
    lastProgressWidth = w;
  }
  forceSpotifyRedraw = false;
}

// Reset static trackers when entering the Spotify page fresh
void resetSpotifyDrawState() {
  artDrawn = false;
  controlsDrawn = false;
  cardDrawn = false;
  lastPlaying = !isPlaying; // Force a mismatch to ensure redraw
  forceSpotifyRedraw = true;
}

void tickSpotifyVisuals() {
  if (currentState != STATE_SPOTIFY) return;
  static unsigned long lastTick = 0, scrollStart = 0;
  static String lastTitle;
  static uint32_t lastPage = 0;
  if (millis() - lastTick < 50) return;
  lastTick = millis();
  if (lastTitle != currentTrack || lastPage != uiPageGeneration) {
    lastTitle = currentTrack;
    lastPage = uiPageGeneration;
    scrollStart = millis();
  }
  if (activeTheme == 1) {
    t2_redrawSpotifyPartial();
    if (currentTrack.length())
      drawMarqueeText(74, 18, currentTrack.c_str(), SCREEN_W - 78, TFT_WHITE, 1, &scrollStart, T2S_BG);
  } else {
    redrawSpotifyPartial();
    if (currentTrack.length())
      drawMarqueeText(4, 85, currentTrack.c_str(), SCREEN_W - 8, CLR_TEXT_HI, 1, &scrollStart);
  }
  // A quiet playback indicator, not a fabricated audio spectrum.
  if (currentLyrics.length() == 0) {
    int x = SCREEN_W - 17;
    int y = activeTheme == 1 ? 108 : 22;
    uint16_t bg = activeTheme == 1 ? T2S_BG : CLR_SURFACE;
    tft.fillRect(x, y, 10, 8, bg);
    for (int i = 0; i < 3; i++) {
      int h = isPlaying ? 2 + (int)(2.5f * (1.0f + sinf(millis() * 0.004f + i * 1.3f))) : 2;
      tft.fillRect(x + i * 3, y + 7 - h, 2, h, isPlaying ? CLR_SUCCESS : CLR_TEXT_LO);
    }
  }
}

// ═══════════════════════════════════════════════════════════
// ALBUM ART PROCESSING
// ═══════════════════════════════════════════════════════════


void completeAlbumArt() {
  if (albumArtRowsComplete != ALBUM_ART_H) return;
  receivingArt = false;
  if (albumArt && albumArtIncoming) memcpy(albumArt, albumArtIncoming, ALBUM_ART_BYTES);
  albumArtRevision++;
  albumArtReady = true;
  artDrawn = false;
  t2s_artDrawn = false;
  if (currentState == STATE_SPOTIFY) {
    if (activeTheme == 1) t2_redrawSpotifyPartial();
    else redrawSpotifyPartial();
  }
}

// CRIT-02 FIX: Fast arc drawing using line segments instead of O(n²) pixel loop.
// Original used sqrt()+atan2() per pixel (~12,100 iterations) freezing ESP for 1-2s.
void drawSmoothRing(int cx, int cy, int r, int thickness, float percent, uint16_t fgColor, uint16_t bgColor) {
  // Draw background ring (full circle)
  for (int angle = 0; angle < 360; angle += 2) {
    float rad = (angle - 90) * PI / 180.0;  // Start at 12 o'clock
    for (int t = 0; t < thickness; t++) {
      int px = cx + cos(rad) * (r - t);
      int py = cy + sin(rad) * (r - t);
      tft.drawPixel(px, py, bgColor);
    }
  }
  // Draw foreground arc (progress portion)
  int endAngle = (int)(percent * 360.0);
  for (int angle = 0; angle < endAngle; angle += 2) {
    float rad = (angle - 90) * PI / 180.0;
    int x1 = cx + cos(rad) * (r - thickness);
    int y1 = cy + sin(rad) * (r - thickness);
    int x2 = cx + cos(rad) * r;
    int y2 = cy + sin(rad) * r;
    tft.drawLine(x1, y1, x2, y2, fgColor);
  }
}

// ═══════════════════════════════════════════════════════════
// POMODORO TIMER PAGE 🍅
// ═══════════════════════════════════════════════════════════

void redrawPomodoroPartial() {
  if (currentState != STATE_POMODORO) return;
  
  static int lastRemaining = -1;
  static bool lastIsBreak = false;
  static int lastSessions = -1;
  static bool lastActive = false;
  static uint32_t lastPage = 0;
  if (lastPage != uiPageGeneration) {
    lastRemaining = -1;
    lastSessions = -1;
    lastPage = uiPageGeneration;
  }
  
  int cx = SCREEN_W / 2;
  int cy = SCREEN_H / 2 - SCALE_Y(10);
  
  float progress = 0;
  if (pomoTotal > 0 && pomoActive) {
    progress = 1.0 - ((float)pomoRemaining / pomoTotal);
  }
  
  uint16_t ringColor = pomoIsBreak ? TFT_GREEN : 0xDF26; // Orange
  
  if (lastRemaining != pomoRemaining || lastIsBreak != pomoIsBreak || lastActive != pomoActive || lastSessions != pomoSessions) {
    // Only clear the text area inside the ring instead of a massive 160x160 rectangle
    tft.fillRect(cx - SCALE_X(50), cy - SCALE_Y(30), SCALE_X(100), SCALE_Y(60), COLOR_BG);
    drawSmoothRing(cx, cy, SCALE_MIN(55), SCALE_MIN(6), progress, ringColor, 0x18E3);
    
    // Center time display
    int minutes = pomoRemaining / 60;
    int seconds = pomoRemaining % 60;
    char timeStr[8];
    sprintf(timeStr, "%02d:%02d", minutes, seconds);
    
    tft.setTextColor(TFT_WHITE, COLOR_BG);
    tft.drawCentreString(timeStr, cx, cy - SCALE_Y(12), (SCREEN_W > 200) ? 4 : 2);
    
    // Label
    tft.setTextColor(pomoIsBreak ? TFT_GREEN : TFT_CYAN, COLOR_BG);
    tft.drawCentreString(pomoIsBreak ? "BREAK" : "FOCUS", cx, cy + SCALE_Y(15), (SCREEN_W > 200) ? 2 : 1);
    
    // Session count & Status (only clear their specific lines)
    tft.fillRect(cx - SCALE_X(60), cy + SCALE_Y(55), SCALE_X(120), SCALE_Y(30), COLOR_BG);
    tft.setTextColor(0x8410, COLOR_BG);
    char sessStr[16];
    sprintf(sessStr, "%d sessions", pomoSessions);
    tft.drawCentreString(sessStr, cx, cy + SCALE_Y(55), 1);
    
    if (!pomoActive) {
      tft.setTextColor(0x6B4D, COLOR_BG);
      tft.drawCentreString("TAP TO START", cx, cy + SCALE_Y(70), 1);
    } else {
      tft.setTextColor(0x4208, COLOR_BG);
      tft.drawCentreString("TAP TO PAUSE", cx, cy + SCALE_Y(70), 1);
    }
    
    lastRemaining = pomoRemaining;
    lastIsBreak = pomoIsBreak;
    lastActive = pomoActive;
    lastSessions = pomoSessions;
  }
}


// ═══════════════════════════════════════════════════════════
// WEATHER PAGE ☀️🌧️
// ═══════════════════════════════════════════════════════════

extern String standaloneWeatherStatus;

// Host WeatherAPI codes and the standalone collector's OpenWeather-style codes.
static int weatherIconKind(int code) {
  if (code == 1000 || code == 800) return 0;
  if (code == 1003 || code == 801) return 1;
  if (code == 1006 || code == 1009 || (code >= 802 && code <= 804)) return 2;
  if ((code >= 200 && code < 300) || code == 1087 || (code >= 1273 && code <= 1282)) return 4;
  if ((code >= 600 && code < 700) || code == 1066 || code == 1069 || code == 1114 || code == 1117 ||
      (code >= 1204 && code <= 1237) || (code >= 1249 && code <= 1264)) return 5;
  if ((code >= 300 && code < 600) || code == 1063 || code == 1072 ||
      (code >= 1150 && code <= 1201) || (code >= 1240 && code <= 1246)) return 3;
  if ((code >= 700 && code < 800) || code == 1030 || code == 1135 || code == 1147) return 6;
  return 7;
}

uint16_t getWeatherColor(int code) {
  const int kind = weatherIconKind(code);
  if (kind == 0 || kind == 1 || kind == 4) return 0xFE60;
  if (kind == 3) return 0x5DDF;
  if (kind == 5) return TFT_WHITE;
  return CLR_TEXT_MED;
}

static void drawWeatherIcon(int x, int y, int size, int kind) {
  auto px = [&](int v) { return x + v * size / 64; };
  auto py = [&](int v) { return y + v * size / 64; };
  auto radius = [&](int v) { return max(1, v * size / 64); };
  const uint16_t sun = 0xFE60, cloud = 0xBDF7, rain = 0x5DDF;
  if (kind == 7) {
    tft.drawCircle(px(32), py(30), radius(20), CLR_TEXT_LO);
    tft.drawLine(px(18), py(44), px(46), py(16), CLR_TEXT_MED);
    return;
  }
  if (kind == 0 || kind == 1) {
    const int cx = kind == 0 ? 32 : 21, cy = kind == 0 ? 30 : 22;
    for (int i = 0; i < 8; i++) {
      float a = i * PI / 4;
      tft.drawLine(px(cx + (int)(16 * cos(a))), py(cy + (int)(16 * sin(a))),
                   px(cx + (int)(21 * cos(a))), py(cy + (int)(21 * sin(a))), sun);
    }
    tft.fillCircle(px(cx), py(cy), radius(11), sun);
    if (kind == 0) return;
  }
  tft.fillCircle(px(20), py(34), radius(12), cloud);
  tft.fillCircle(px(33), py(27), radius(15), 0xDEFB);
  tft.fillCircle(px(47), py(35), radius(11), cloud);
  tft.fillRoundRect(px(9), py(34), radius(49), radius(15), radius(6), cloud);
  if (kind == 3) {
    for (int i = 0; i < 3; i++)
      tft.drawLine(px(20 + i * 12), py(52), px(16 + i * 12), py(60), rain);
  } else if (kind == 4) {
    tft.fillTriangle(px(34), py(44), px(25), py(56), px(34), py(53), sun);
    tft.fillTriangle(px(33), py(51), px(41), py(51), px(29), py(63), sun);
  } else if (kind == 5) {
    for (int i = 0; i < 2; i++) {
      const int cx = i ? 44 : 20;
      tft.drawLine(px(cx - 4), py(56), px(cx + 4), py(56), TFT_WHITE);
      tft.drawLine(px(cx - 2), py(52), px(cx + 2), py(60), TFT_WHITE);
      tft.drawLine(px(cx - 2), py(60), px(cx + 2), py(52), TFT_WHITE);
    }
  } else if (kind == 6) {
    for (int row = 0; row < 3; row++)
      tft.drawFastHLine(px(12 + row * 3), py(51 + row * 5), radius(40 - row * 3), CLR_TEXT_LO);
  }
}

void redrawWeatherPartial() {
  if (currentState != STATE_WEATHER) return;
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  tft.fillRect(0, UI_HEADER_H, SCREEN_W, SCREEN_H - UI_HEADER_H - UI_FOOTER_H, COLOR_BG);
  const bool compact = SCREEN_W <= 200;
  const int pad = compact ? 6 : 12;
  String condition = uiText(weatherCondition.c_str());
  condition.trim();
  if (!condition.length()) {
    const int iconSize = compact ? 44 : 88;
    drawWeatherIcon(compact ? 8 : 32, compact ? 40 : 65, iconSize, 7);
    const int textX = compact ? 64 : 144;
    drawTruncatedText(textX, compact ? 40 : 80, "No forecast", SCREEN_W - textX - pad,
                      CLR_TEXT_HI, compact ? 1 : 2);
    String status = !wifiConnected ? "Wi-Fi offline" :
      (standaloneWeatherStatus.length() ? standaloneWeatherStatus : "No forecast received");
    drawWrappedText(textX, compact ? 56 : 106, status.c_str(), SCREEN_W - textX - pad,
                    compact ? 50 : 80, CLR_TEXT_MED);
    drawPageIndicator(STATE_WEATHER, STATE_COUNT);
    return;
  }

  const char* status = !wifiConnected ? "Offline" : standaloneWeatherStatus.length() ? "Saved" : "";
  const int statusW = tft.textWidth(status, 1);
  drawTruncatedText(pad, compact ? 20 : 24, weatherCity.length() ? weatherCity.c_str() : "Location unavailable",
                    SCREEN_W - 2 * pad - (statusW ? statusW + 8 : 0), CLR_TEXT_MED, compact ? 1 : 2);
  if (statusW) {
    tft.setTextColor(0xFE60, COLOR_BG);
    tft.drawRightString(status, SCREEN_W - pad, compact ? 20 : 28, 1);
  }

  drawWeatherIcon(compact ? 6 : 18, compact ? 34 : 50, compact ? 46 : 86, weatherIconKind(weatherCode));
  const int tempX = compact ? 60 : 120, tempY = compact ? 33 : 52;
  String temp = String(weatherTemp) + " C";
  tft.setTextSize(compact ? 1 : 2);
  drawTruncatedText(tempX, tempY, temp.c_str(), SCREEN_W - tempX - pad, CLR_TEXT_HI, 4);
  tft.setTextSize(1);
  drawWrappedText(tempX, compact ? 62 : 110, condition.c_str(), SCREEN_W - tempX - pad,
                  compact ? 20 : 36, getWeatherColor(weatherCode), compact ? 1 : 2);

  if (compact) {
    tft.drawFastHLine(pad, 82, SCREEN_W - 2 * pad, CLR_BORDER);
    const int rightX = SCREEN_W / 2 + 6, leftW = SCREEN_W / 2 - pad - 4;
    String feels = String("Feels ") + String(weatherFeels) + " C";
    String humid = String("RH ") + String(weatherHumidity) + "%";
    String wind = String(weatherWind) + " km/h";
    String range = String("H") + String(weatherHigh) + " L" + String(weatherLow);
    drawTruncatedText(pad, 86, feels.c_str(), leftW, CLR_TEXT_HI, 1);
    drawTruncatedText(rightX, 86, humid.c_str(), SCREEN_W - rightX - pad, 0x5DDF, 1);
    drawTruncatedText(pad, 98, wind.c_str(), leftW, CLR_TEXT_MED, 1);
    drawTruncatedText(rightX, 98, range.c_str(), SCREEN_W - rightX - pad, CLR_TEXT_MED, 1);
    tft.drawFastHLine(pad, 108, SCREEN_W - 2 * pad, CLR_BORDER);
    for (int i = 0; i < 2; i++) {
      const int x = i ? rightX : pad;
      tft.drawFastHLine(x, 116, 9, 0xFE60);
      tft.drawLine(x + 4, 110, x + 4, 114, 0xFE60);
      tft.drawLine(x + 1, i ? 111 : 113, x + 4, i ? 114 : 110, 0xFE60);
      tft.drawLine(x + 7, i ? 111 : 113, x + 4, i ? 114 : 110, 0xFE60);
      String value = i ? weatherSunset : weatherSunrise;
      drawTruncatedText(x + 14, 110, value.length() ? value.c_str() : "--",
                        i ? SCREEN_W - x - pad - 14 : leftW - 14, CLR_TEXT_LO, 1);
    }
  } else {
    tft.drawFastHLine(pad, 148, SCREEN_W - 2 * pad, CLR_BORDER);
    const int colW = (SCREEN_W - 2 * pad) / 3;
    const char* labels[] = {"FEELS LIKE", "HUMIDITY", "WIND"};
    const String values[] = {String(weatherFeels) + " C", String(weatherHumidity) + "%", String(weatherWind) + " km/h"};
    const char* lower[] = {"HIGH / LOW", "SUNRISE", "SUNSET"};
    const String detail[] = {String(weatherHigh) + " / " + String(weatherLow) + " C",
      weatherSunrise.length() ? weatherSunrise : "--", weatherSunset.length() ? weatherSunset : "--"};
    for (int i = 0; i < 3; i++) {
      const int x = pad + i * colW;
      drawTruncatedText(x, 156, labels[i], colW - 6, CLR_TEXT_LO, 1);
      drawTruncatedText(x, 169, values[i].c_str(), colW - 6, i == 1 ? 0x5DDF : CLR_TEXT_HI, 2);
      drawTruncatedText(x, 199, lower[i], colW - 6, CLR_TEXT_LO, 1);
      drawTruncatedText(x, 212, detail[i].c_str(), colW - 6, i ? 0xFE60 : CLR_TEXT_MED, 1);
    }
    tft.drawFastHLine(pad, 191, SCREEN_W - 2 * pad, CLR_BORDER);
  }
  drawPageIndicator(STATE_WEATHER, STATE_COUNT);
}

// ═══════════════════════════════════════════════════════════
// NOTIFICATIONS PAGE 🔔
// ═══════════════════════════════════════════════════════════

uint16_t getNotifColor(String app) {
  if (app.indexOf("Mail") >= 0 || app.indexOf("mail") >= 0) return 0xFBE0; // Orange
  if (app.indexOf("Cal") >= 0) return 0x031F;   // Blue
  if (app.indexOf("Msg") >= 0 || app.indexOf("msg") >= 0) return TFT_GREEN;
  if (app.indexOf("Spot") >= 0) return TFT_GREEN;
  return TFT_CYAN;
}

void redrawNotificationsPartial() {
  if (currentState != STATE_NOTIFICATIONS) return;
  tft.fillRect(0, UI_HEADER_H, SCREEN_W, SCREEN_H - UI_HEADER_H, COLOR_BG);
  int pad = 5, top = UI_HEADER_H + 4;
  int bottom = SCREEN_H - UI_FOOTER_H - 14;
  if (notifTotal <= 0) {
    drawWrappedText(pad, SCREEN_H / 2 - 12, "No notifications\nAll caught up", SCREEN_W - 2 * pad, 30, CLR_TEXT_MED);
  } else {
    int rowH = (bottom - top) / 3;
    for (int i = 0; i < min(notifTotal, 3); i++) {
      int y = top + i * rowH;
      uint16_t accent = getNotifColor(notifApps[i]);
      tft.fillRoundRect(pad, y, SCREEN_W - pad * 2, rowH - 3, 3, CLR_SURFACE);
      tft.fillRect(pad, y + 3, 2, rowH - 9, accent);
      String stamp = fitUiText(notifTimes[i].c_str(), 42, 1);
      int stampW = tft.textWidth(stamp, 1);
      drawTruncatedText(pad + 6, y + 3, notifApps[i].c_str(), SCREEN_W - 2 * pad - stampW - 15, accent, 1, CLR_SURFACE);
      tft.setTextColor(CLR_TEXT_LO, CLR_SURFACE);
      tft.drawRightString(stamp, SCREEN_W - pad - 4, y + 3, 1);
      drawWrappedText(pad + 6, y + 14, notifTitles[i].c_str(), SCREEN_W - 2 * pad - 12, rowH - 14, TFT_WHITE, 1, CLR_SURFACE);
    }
  }
  drawTruncatedText(pad, SCREEN_H - UI_FOOTER_H - 10, flashNotifEnabled ? "Alerts on" : "Alerts off", SCREEN_W - 2 * pad, CLR_TEXT_LO, 1);
  drawPageIndicator(STATE_NOTIFICATIONS, STATE_COUNT);
}

void updateFlashNotification() {
  if (flashNotifActive && millis() - flashNotifStart >= 3000) {
    flashNotifActive = false;
    if (currentState == STATE_EYES) {
      renderCurrentPage();
    }
  }
}

// ═══════════════════════════════════════════════════════════
// NOTES PAGE
// ═══════════════════════════════════════════════════════════

void redrawNotesPartial() {
  if (currentState != STATE_NOTES) return;
  tft.fillRect(0, UI_HEADER_H, SCREEN_W, SCREEN_H - UI_HEADER_H, COLOR_BG);
  int top = UI_HEADER_H + 5;
  int rowH = (SCREEN_H - UI_FOOTER_H - top) / 4;
  bool any = false;
  for (int i = 0; i < 4; i++) {
    if (!currentNotes[i].length()) continue;
    any = true;
    int y = top + i * rowH;
    tft.fillCircle(7, y + 4, 2, CLR_PRIMARY);
    drawWrappedText(15, y, currentNotes[i].c_str(), SCREEN_W - 21, rowH - 2, CLR_TEXT_HI);
  }
  if (!any) drawWrappedText(8, SCREEN_H / 2 - 5, "No notes yet", SCREEN_W - 16, 20, CLR_TEXT_MED);
  drawPageIndicator(STATE_NOTES, STATE_COUNT);
}

// ═══════════════════════════════════════════════════════════
// SETTINGS / SYSTEM INFO PAGE
// ═══════════════════════════════════════════════════════════

int settingsScrollY = 0;

void redrawSettingsPartial() {
  if (currentState != STATE_SETTINGS) return;
  const int contentH = SCREEN_H - UI_HEADER_H - UI_FOOTER_H;
  settingsScrollY = constrain(settingsScrollY, 0, max(0, 228 - contentH));
  tft.setViewport(0, UI_HEADER_H, SCREEN_W, contentH, false);
  tft.fillRect(0, UI_HEADER_H, SCREEN_W, contentH, COLOR_BG);
  int y = UI_HEADER_H + 5 - settingsScrollY;
  drawTruncatedText(6, y, "APPEARANCE", SCREEN_W - 12, CLR_PRIMARY, 1);
  y += 13;
  const char* names[] = {"Classic", "Soft", "Robo"};
  int segment = (SCREEN_W - 12) / 3;
  for (int i = 0; i < THEME_COUNT; i++) {
    uint16_t bg = activeTheme == i ? CLR_PRIMARY : CLR_SURFACE;
    uint16_t fg = activeTheme == i ? TFT_BLACK : CLR_TEXT_MED;
    tft.fillRoundRect(6 + i * segment, y, segment - 3, 16, 3, bg);
    drawTruncatedText(9 + i * segment, y + 4, names[i], segment - 9, fg, 1, bg);
  }
  y += 24;
  auto row = [&](const char* label, const String& value, uint16_t color) {
    drawTruncatedText(6, y, label, 45, CLR_TEXT_LO, 1);
    drawTruncatedText(54, y, value.c_str(), SCREEN_W - 60, color, 1);
    y += 13;
  };
  row("Wi-Fi", wifiConnected ? WiFi.SSID() : "Offline", CLR_TEXT_HI);
  row("IP", wifiConnected ? WiFi.localIP().toString() : "--", CLR_TEXT_MED);
  row("Signal", wifiConnected ? String(WiFi.RSSI()) + " dBm" : "--", CLR_TEXT_MED);
  y += 5;
  row("CPU", String(ESP.getCpuFreqMHz()) + " MHz", CLR_TEXT_HI);
  row("Memory", String(ESP.getFreeHeap() / 1024) + " KB free", CLR_TEXT_HI);
  row("Uptime", String(millis() / 60000) + " min", CLR_TEXT_MED);
  row("Light", String(ldrValue), CLR_TEXT_MED);
  row("Input", HAS_TOUCH ? "Touch" : "L / R / Select", CLR_TEXT_MED);
  #ifdef ESP32
  // Probe once, outside the repeated scroll paint path.
  static int nrf1 = -1, nrf2 = -1;
  if (nrf1 < 0) {
    digitalWrite(TFT_CS, HIGH);
    pinMode(CC1101_CSN_PIN, OUTPUT);
    digitalWrite(CC1101_CSN_PIN, HIGH);
    pinMode(NRF1_CSN_PIN, OUTPUT); digitalWrite(NRF1_CSN_PIN, HIGH);
    pinMode(NRF2_CSN_PIN, OUTPUT); digitalWrite(NRF2_CSN_PIN, HIGH);
    RF24 radio1(NRF1_CE_PIN, NRF1_CSN_PIN), radio2(NRF2_CE_PIN, NRF2_CSN_PIN);
    nrf1 = radio1.begin(); nrf2 = radio2.begin();
    if (nrf1) radio1.powerDown();
    if (nrf2) radio2.powerDown();
  }
  row("Radio 1", nrf1 ? "Detected" : "Not detected", nrf1 ? CLR_SUCCESS : CLR_TEXT_LO);
  row("Radio 2", nrf2 ? "Detected" : "Not detected", nrf2 ? CLR_SUCCESS : CLR_TEXT_LO);
  #endif
  tft.resetViewport();
  drawPageIndicator(STATE_SETTINGS, STATE_COUNT);
}

// ═══════════════════════════════════════════════════════════
// PAGE DRAWING & ORCHESTRATION
// ═══════════════════════════════════════════════════════════

void drawEyesPage() {
  if (customEyeActive && customEyeReady && customEyeImg != nullptr) {
    tft.setSwapBytes(true); // Ensure correct endianness for image
    tft.pushImage(0, 0, 160, 128, customEyeImg);
    tft.setSwapBytes(false);
    return;
  }
  
  if (activeTheme == 2) { tft.fillScreen(COLOR_BG); t3_drawEyesPage(); drawStatusBar(); return; }
  if (activeTheme == 1) { tft.fillScreen(COLOR_BG); t2_drawEyesPage(); drawStatusBar(); return; }
  tft.fillRect(0, 16, SCREEN_W, SCREEN_H - 16, COLOR_BG);
  drawStarfield();
  drawEyes();
  drawStatusBar();
}

void drawSpotifyPage() {
  if (activeTheme == 1) { t2_drawSpotifyPage(); drawStatusBar(); return; }
  tft.fillScreen(COLOR_BG);
  resetSpotifyDrawState();
  redrawSpotifyPartial();
  drawStatusBar();
}

void drawPomodoroPage() {
  tft.fillScreen(COLOR_BG);
  redrawPomodoroPartial();
  drawPageHeader("Pomodoro");
  drawStatusBar();
}

void drawWeatherPage() {
  tft.fillScreen(COLOR_BG);
  redrawWeatherPartial();
  drawPageHeader("Weather");
  drawStatusBar();
}

void drawNotificationsPage() {
  tft.fillScreen(COLOR_BG);
  char nb[32]; sprintf(nb, "Notifications (%d)", notifTotal);
  redrawNotificationsPartial();
  drawPageHeader(nb);
  drawStatusBar();
}

void drawNotesPage() {
  tft.fillScreen(COLOR_BG);
  redrawNotesPartial();
  drawPageHeader("Quick Notes");
  drawStatusBar();
}

void drawSettingsPage() {
  tft.fillScreen(COLOR_BG);
  drawPageHeader("System Info");
  drawStatusBar();
  settingsScrollY = 0;
  redrawSettingsPartial();
}

// ═══════════════════════════════════════════════════════════
// V6 NEW PAGE WRAPPERS
// ═══════════════════════════════════════════════════════════

void drawStocksPageFull() {
  resetStockDrawState();
  drawStocksPage();
  drawStatusBar();
}

void drawGamingPageFull() {
  resetGamingDrawState();
  drawGamingPage();
  drawStatusBar();
}

void drawSocialPageFull() {
  resetSocialDrawState();
  drawSocialPage();
  drawStatusBar();
}

void drawProductivityPageFull() {
  resetProductivityDrawState();
  drawProductivityPage();
  drawStatusBar();
}

void drawNetworkPageFull() {
  resetNetworkDrawState();
  drawNetworkPage();
  drawStatusBar();
}

void drawClockDashboardPageFull() {
  resetClockDashboardDrawState();
  drawClockDashboardPage();
  drawStatusBar();
}

void drawRetroWatchPageFull() {
  resetRetroWatchDrawState();
  drawRetroWatchPage();
  drawStatusBar();
}

void renderCurrentPage() {
  uiPageGeneration++;
  tft.resetViewport();
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  tft.setTextPadding(0);
  tft.setTextWrap(false, false);
  #ifdef ESP32
  if (currentState != STATE_EYES) {
    releaseClassicEyeSprites();
    if (T2EyeDrawer::t2Canvas) {
      T2EyeDrawer::t2Canvas->deleteSprite();
      delete T2EyeDrawer::t2Canvas;
      t2CanvasSetNull();
    }
    if (t3_canvas) {
      t3_canvas->deleteSprite();
      delete t3_canvas;
      t3CanvasSetNull();
    }
    setThoughtSpriteCompositing(true);
  }
  #endif
  switch(currentState) {
    case STATE_EYES: drawEyesPage(); break;
    case STATE_SPOTIFY: drawSpotifyPage(); break;
    case STATE_POMODORO: drawPomodoroPage(); break;
    case STATE_WEATHER: drawWeatherPage(); break;
    case STATE_NOTIFICATIONS: drawNotificationsPage(); break;
    case STATE_NOTES: drawNotesPage(); break;
    case STATE_STOCKS: drawStocksPageFull(); break;
    case STATE_GAMING: drawGamingPageFull(); break;
    case STATE_SOCIAL: drawSocialPageFull(); break;
    case STATE_PRODUCTIVITY: drawProductivityPageFull(); break;
    case STATE_NETWORK: drawNetworkPageFull(); break;
    case STATE_SETTINGS: drawSettingsPage(); break;
    case STATE_PC_DASH: drawPcDashPageFull(); break;
    case STATE_RUVIEW:
      #ifdef ESP32
      {
        extern void resumeCSICollector();
        resumeCSICollector();
      }
      #endif
      drawRuviewPageFull();
      break;
    case STATE_CLOCK_DASHBOARD: drawClockDashboardPageFull(); break;
    case STATE_RETRO_WATCH: drawRetroWatchPageFull(); break;
    #ifdef ESP32
    case STATE_DR_HACK: initDrHack(); break;
    #endif
    default: break;
  }
}

void changePage(int direction) {
  #ifdef ESP32
  AppState prevState = currentState;
  #endif
  int next = (int)currentState + direction;
  // Compute effective max page count
  int maxPage = STATE_COUNT;
  #ifndef ESP32
  // ESP8266: skip Dr. Hack (STATE_DR_HACK) — it's ESP32-only
  maxPage = STATE_DR_HACK; // stop before DR_HACK
  #endif
  if (next >= maxPage) next = 0;
  if (next < 0) next = maxPage - 1;
  currentState = (AppState)next;

  if (currentState != STATE_EYES) {
    customEyeActive = false;
  }
  // Reset all page draw states on page change to prevent stale partial redraws
  resetSpotifyDrawState();
  resetClockDashboardDrawState();
  resetRetroWatchDrawState();
  resetPcDashDrawState();
  extern void t2_resetSpotifyDrawState();
  t2_resetSpotifyDrawState();
  settingsScrollY = 0;
  #ifdef ESP32
  // Reset Dr. Hack state when leaving
  if (currentState != STATE_DR_HACK) {
    extern DrHackSubState dhCurrentState;
    dhCurrentState = DH_MENU;
  }
  // Manage RuView CSI hardware state: pause when leaving, resume when entering
  if (prevState == STATE_RUVIEW && currentState != STATE_RUVIEW) {
    extern void pauseCSICollector();
    pauseCSICollector();
  } else if (currentState == STATE_RUVIEW) {
    extern void resumeCSICollector();
    resumeCSICollector();
  }
  #endif
  
  extern ThoughtBubble activeBubble;
  activeBubble.active = false;
  activeBubble.fadingOut = false;
  activeBubble.fadeAlpha = 0;
  // Explicit full screen clear on page transition
  tft.fillScreen(COLOR_BG);
  
  renderCurrentPage();
}

void setPage(int targetPage) {
  int maxPage = STATE_COUNT;
  #ifndef ESP32
  maxPage = STATE_DR_HACK;
  #endif
  if (targetPage < 0 || targetPage >= maxPage) return;
  if (targetPage == (int)currentState) return;
  if (targetPage != (int)STATE_EYES) {
    customEyeActive = false;
  }
  #ifdef ESP32
  AppState prevState = currentState;
  #endif
  currentState = (AppState)targetPage;
  resetSpotifyDrawState();
  resetClockDashboardDrawState();
  resetRetroWatchDrawState();
  resetPcDashDrawState();
  extern void t2_resetSpotifyDrawState();
  t2_resetSpotifyDrawState();
  settingsScrollY = 0;
  #ifdef ESP32
  if (currentState != STATE_DR_HACK) {
    extern DrHackSubState dhCurrentState;
    dhCurrentState = DH_MENU;
  }
  // Manage RuView CSI hardware state: pause when leaving, resume when entering
  if (prevState == STATE_RUVIEW && currentState != STATE_RUVIEW) {
    extern void pauseCSICollector();
    pauseCSICollector();
  } else if (currentState == STATE_RUVIEW) {
    extern void resumeCSICollector();
    resumeCSICollector();
  }
  #endif
  extern ThoughtBubble activeBubble;
  activeBubble.active = false;
  activeBubble.fadingOut = false;
  activeBubble.fadeAlpha = 0;
  tft.fillScreen(COLOR_BG);
  renderCurrentPage();
}

#endif
