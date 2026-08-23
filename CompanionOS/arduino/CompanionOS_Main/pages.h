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
#include "page_ruview.h"
#include "page_clock_dashboard.h"
#include "theme2_eyes.h"
#include "theme2_spotify.h"
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
int pomoRemaining = 0;
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
  
  // 1:1 Spotify Replication Layout for 160x128
  int alb_x = 4, alb_y = 18, alb_s = 64; // Use full 64x64 to prevent array corruption
  int infoY = alb_y + alb_s + 4; // Y = 86
  String title = currentTrack.substring(0, 24);
  String artist = currentArtist.substring(0, 20);

  if (title != lastTrackTitle) {
    tft.fillRect(alb_x, alb_y, alb_s + 3, alb_s + 3, COLOR_BG);
    artDrawn = false;
    albumArtReady = false;
  }
  
  if (!artDrawn && albumArtReady) {
    tft.setSwapBytes(true);
    tft.pushImage(alb_x, alb_y, alb_s, alb_s, albumArt);
    tft.setSwapBytes(false);
    
    // Repaint shadow
    int blurRadius = 2;
    for (int i = 1; i <= blurRadius; i++) {
      uint16_t shadeColor = (i == 1) ? 0x1042 : 0x0821;
      tft.drawFastHLine(alb_x + i, alb_y + alb_s + i - 1, alb_s, shadeColor);
      tft.drawFastVLine(alb_x + alb_s + i - 1, alb_y + i, alb_s, shadeColor);
    }
    artDrawn = true;
  }
  
  if (forceSpotifyRedraw || title != lastTrackTitle) {
    tft.fillRect(alb_x, infoY, 150, 10, COLOR_BG); 
    tft.setTextColor(TFT_WHITE, COLOR_BG);
    tft.drawString(title, alb_x, infoY, 1);
    
    lastTrackTitle = title;
  }
  if (forceSpotifyRedraw || artist != lastArtist) {
    tft.fillRect(alb_x, infoY + 10, 150, 10, COLOR_BG); 
    tft.setTextColor(0x8410, COLOR_BG);
    tft.drawString(artist, alb_x, infoY + 10, 1);
    drawIconHeart(alb_x + tft.textWidth(artist, 1) + 4, infoY + 12, TFT_WHITE);
    lastArtist = artist;
  }

  // ── RIGHT COLUMN: Lyrics Card (Scaled for 160x128) ──
  int cardX = 72;
  int cardY = 18;
  int cardW = 84;
  int cardH = 64;
  
  if (!cardDrawn) {
    tft.fillRoundRect(cardX + 2, cardY + 2, cardW, cardH, 4, 0x0821);
    tft.fillRoundRect(cardX + 1, cardY + 1, cardW, cardH, 4, 0x1042);
    
    uint16_t startColor = 0x2125;
    uint16_t endColor = 0x0821;
    uint16_t sr = (startColor >> 11) & 0x1F, sg = (startColor >> 5) & 0x3F, sb = startColor & 0x1F;
    uint16_t er = (endColor >> 11) & 0x1F, eg = (endColor >> 5) & 0x3F, eb = endColor & 0x1F;
    
    for (int j = 0; j < cardH; j++) {
      float t = (float)j / cardH;
      uint16_t cr = sr + (er - sr) * t;
      uint16_t cg = sg + (eg - sg) * t;
      uint16_t cb = sb + (eb - sb) * t;
      uint16_t color = (cr << 11) | (cg << 5) | cb;
      
      int r = 4;
      if (j < r || j >= cardH - r) {
        int offset = r - (int)sqrt(r*r - pow(r - (j < r ? j : cardH - 1 - j), 2));
        tft.drawFastHLine(cardX + offset, cardY + j, cardW - 2*offset, color);
      } else {
        tft.drawFastHLine(cardX, cardY + j, cardW, color);
      }
    }
    
    tft.setTextColor(0x8410, startColor);
    tft.drawString("LYRICS", cardX + 4, cardY + 4, 1);
    cardDrawn = true;
  }
  
  if (currentLyrics != lastLyric1 || currentLyricsLine2 != lastLyric2 || prevLyricsLine != lastPrevLyric) {
    tft.fillRect(cardX + 2, cardY + 16, cardW - 4, cardH - 18, 0x0821);
    int yC = cardY + 16;
    
    if (prevLyricsLine.length() > 0) {
      tft.setTextColor(0x4208, 0x0821);
      tft.drawString(prevLyricsLine.substring(0, 14), cardX + 4, yC, 1); yC += 14;
    }
    
    tft.setTextColor(TFT_WHITE, 0x0821);
    tft.drawString(currentLyrics.substring(0, 14), cardX + 4, yC, 1); yC += 14;
    if (currentLyrics.length() > 14) {
      tft.drawString(currentLyrics.substring(14, 28), cardX + 4, yC, 1); yC += 14;
    }
    
    if (currentLyricsLine2.length() > 0 && yC < cardY + cardH - 10) {
      tft.setTextColor(0x4208, 0x0821);
      tft.drawString(currentLyricsLine2.substring(0, 14), cardX + 4, yC, 1);
    }
    
    lastLyric1 = currentLyrics;
    lastLyric2 = currentLyricsLine2;
    lastPrevLyric = prevLyricsLine;
  }

  // ── BOTTOM: Playback Controls ──
  int ctrlY = 106;
  if (!controlsDrawn) {
    tft.fillRect(0, ctrlY - 8, 160, 16, COLOR_BG); 
    drawIconPrev(45, ctrlY, TFT_WHITE);
    drawIconNext(115, ctrlY, TFT_WHITE);
    controlsDrawn = true;
  }

  if (isPlaying != lastPlaying || !controlsDrawn) {
    tft.fillCircle(80, ctrlY, 9, TFT_WHITE);
    if (isPlaying) drawIconPause(80, ctrlY, COLOR_BG);
    else drawIconPlay(82, ctrlY, COLOR_BG);
    lastPlaying = isPlaying;
  }

  // ── BOTTOM: Progress Bar ──
  int barX = 10, barY = 120, barW = 140;
  
  tft.fillRect(barX, barY - 2, barW + 10, 6, COLOR_BG);
  tft.fillRect(barX, barY, barW, 2, 0x4208);
  if (playDuration > 0) {
    int w = map(playProgress, 0, playDuration, 0, barW);
    w = constrain(w, 0, barW);
    tft.fillRect(barX, barY, w, 2, TFT_WHITE);
    tft.fillCircle(barX + w, barY + 1, 2, TFT_WHITE);
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

// ═══════════════════════════════════════════════════════════
// ALBUM ART PROCESSING
// ═══════════════════════════════════════════════════════════


void completeAlbumArt() {
  receivingArt = false;
  albumArtReady = true;
  artDrawn = false; // Force redraw in redrawSpotifyPartial
  
  if (currentState == STATE_SPOTIFY) {
    if (activeTheme == 1) {
      extern void t2_redrawSpotifyPartial();
      t2_redrawSpotifyPartial();
    } else {
      redrawSpotifyPartial(); // Redraw whole UI safely
    }
  } else if (currentState == STATE_GAMING) {
    extern void redrawGamingPartial();
    redrawGamingPartial();
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
  
  int cx = SCREEN_W / 2;
  int cy = SCREEN_H / 2 - SCALE_Y(10);
  
  float progress = 0;
  if (pomoTotal > 0 && pomoActive) {
    progress = 1.0 - ((float)pomoRemaining / pomoTotal);
  }
  
  uint16_t ringColor = pomoIsBreak ? TFT_GREEN : 0xDF26; // Orange
  
  if (lastRemaining != pomoRemaining || lastIsBreak != pomoIsBreak || lastActive != pomoActive) {
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
  }
}


// ═══════════════════════════════════════════════════════════
// WEATHER PAGE ☀️🌧️
// ═══════════════════════════════════════════════════════════

uint16_t getWeatherColor(int code) {
  // weatherapi.com condition codes
  if (code == 1000) return TFT_YELLOW;    // Sunny
  if (code == 1003 || code == 1006) return 0xC618;  // Partly cloudy / Cloudy
  if (code >= 1063 && code <= 1072) return 0x5DDF;  // Rain/Drizzle
  if (code >= 1180 && code <= 1201) return 0x031F;  // Heavy rain
  if (code >= 1210 && code <= 1225) return TFT_WHITE; // Snow
  if (code >= 1273 && code <= 1282) return TFT_YELLOW; // Thunder
  return 0x8410;
}

void redrawWeatherPartial() {
  if (currentState != STATE_WEATHER) return;
  
  tft.fillRect(0, 18, SCREEN_W, SCREEN_H - 35, COLOR_BG);
  
  if (SCREEN_W > 200) {
    // ── LARGE SCREEN (320×240) ──
    tft.setTextColor(0x8410, COLOR_BG);
    tft.drawString(weatherCity, SCALE_X(15), SCALE_Y(22), 2);
    
    char tempStr[8]; sprintf(tempStr, "%d", weatherTemp);
    tft.setTextColor(TFT_WHITE, COLOR_BG);
    tft.drawString(tempStr, SCALE_X(15), SCALE_Y(48), 7);
    
    int tw = tft.textWidth(tempStr, 7);
    tft.setTextColor(0x8410, COLOR_BG);
    tft.drawString("C", SCALE_X(15) + tw + SCALE_X(8), SCALE_Y(48), 4);
    tft.drawCircle(SCALE_X(15) + tw + SCALE_X(4), SCALE_Y(50), SCALE_MIN(3), 0x8410);
    
    uint16_t condColor = getWeatherColor(weatherCode);
    tft.setTextColor(condColor, COLOR_BG);
    tft.drawString(weatherCondition, SCALE_X(15), SCALE_Y(110), 2);
    
    int dx = 175;
    int dy0 = 30;
    int detailStep = 35;
    
    tft.setTextColor(0x6B4D, COLOR_BG);
    tft.drawString("Feels", dx, dy0, 1);
    tft.setTextColor(TFT_WHITE, COLOR_BG);
    char fb[8]; sprintf(fb, "%d C", weatherFeels);
    tft.drawString(fb, dx, dy0 + 10, 2);
    
    tft.setTextColor(0x6B4D, COLOR_BG);
    tft.drawString("Humid", dx, dy0 + detailStep, 1);
    tft.setTextColor(TFT_CYAN, COLOR_BG);
    char hb[8]; sprintf(hb, "%d%%", weatherHumidity);
    tft.drawString(hb, dx, dy0 + detailStep + 10, 2);
    
    tft.setTextColor(0x6B4D, COLOR_BG);
    tft.drawString("Wind", dx, dy0 + detailStep * 2, 1);
    tft.setTextColor(TFT_WHITE, COLOR_BG);
    char wb[12]; sprintf(wb, "%d km/h", weatherWind);
    tft.drawString(wb, dx, dy0 + detailStep * 2 + 10, 2);
    
    tft.setTextColor(0x6B4D, COLOR_BG);
    tft.drawString("Hi/Lo", dx, dy0 + detailStep * 3, 1);
    tft.setTextColor(TFT_WHITE, COLOR_BG);
    char hlb[12]; sprintf(hlb, "%d/%d", weatherHigh, weatherLow);
    tft.drawString(hlb, dx, dy0 + detailStep * 3 + 10, 2);
    
    // Sunrise/Sunset
    tft.setTextColor(TFT_YELLOW, COLOR_BG);
    tft.drawString("^", 15, 145, 2);
    tft.setTextColor(0x8410, COLOR_BG);
    tft.drawString(weatherSunrise, 30, 145, 1);
    
    tft.setTextColor(0xFBE0, COLOR_BG);
    tft.drawString("v", 15, 160, 2);
    tft.setTextColor(0x8410, COLOR_BG);
    tft.drawString(weatherSunset, 30, 160, 1);
    
  } else {
    // ── SMALL SCREEN (160×128) ──
    int y = 20; // Start below the 18px status bar
    
    // City name
    tft.setTextColor(0x8410, COLOR_BG);
    tft.drawString(weatherCity, 10, y, 1); // 8px high
    
    y += 14; 
    
    // Condition
    uint16_t condColor = getWeatherColor(weatherCode);
    tft.setTextColor(condColor, COLOR_BG);
    tft.drawString(weatherCondition, 10, y, 1);
    
    y += 20; 
    
    // Big temperature (left side)
    char tempStr[8]; sprintf(tempStr, "%d", weatherTemp);
    tft.setTextColor(TFT_WHITE, COLOR_BG);
    tft.drawString(tempStr, 10, y, 4); // Font 4 is ~26px high
    
    int tw = tft.textWidth(tempStr, 4);
    tft.setTextColor(0x8410, COLOR_BG);
    tft.drawString("C", 10 + tw + 4, y, 2); // Font 2 is 16px high
    tft.drawCircle(10 + tw + 2, y + 2, 2, 0x8410); // Degree symbol
    
    // Details (right side)
    int dx = 90;
    int dy0 = y - 4;
    int rowH = 15; // 8px font + 7px spacing = 15px
    
    tft.setTextColor(0x6B4D, COLOR_BG);
    tft.drawString("Feels", dx, dy0, 1);
    tft.setTextColor(TFT_WHITE, COLOR_BG);
    char fb[8]; sprintf(fb, "%d", weatherFeels);
    tft.drawRightString(fb, 150, dy0, 1);
    dy0 += rowH;
    
    tft.setTextColor(0x6B4D, COLOR_BG);
    tft.drawString("Humid", dx, dy0, 1);
    tft.setTextColor(TFT_CYAN, COLOR_BG);
    char hb[8]; sprintf(hb, "%d%%", weatherHumidity);
    tft.drawRightString(hb, 150, dy0, 1);
    dy0 += rowH;
    
    tft.setTextColor(0x6B4D, COLOR_BG);
    tft.drawString("Wind", dx, dy0, 1);
    tft.setTextColor(TFT_WHITE, COLOR_BG);
    char wb[12]; sprintf(wb, "%d", weatherWind);
    tft.drawRightString(wb, 150, dy0, 1);
    dy0 += rowH;
    
    tft.setTextColor(0x6B4D, COLOR_BG);
    tft.drawString("Hi/Lo", dx, dy0, 1);
    tft.setTextColor(TFT_WHITE, COLOR_BG);
    char hl[12]; sprintf(hl, "%d/%d", weatherHigh, weatherLow);
    tft.drawRightString(hl, 150, dy0, 1);
  }
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
  
  tft.fillRect(0, 18, SCREEN_W, SCREEN_H - 35, COLOR_BG);
  
  if (notifTotal == 0) {
    tft.setTextColor(0x4208);
    tft.drawCentreString("No notifications", SCREEN_W/2, SCREEN_H/2 - 10, 2);
    tft.drawCentreString("All caught up!", SCREEN_W/2, SCREEN_H/2 + 10, 1);
    return;
  }
  
  for (int i = 0; i < min(notifTotal, 3); i++) {
    int cardY = SCALE_Y(22) + (i * SCALE_Y(58));
    uint16_t borderColor = getNotifColor(notifApps[i]);
    
    // Card background
    tft.fillRoundRect(SCALE_X(10), cardY, SCREEN_W - SCALE_X(20), SCALE_Y(52), SCALE_MIN(4), 0x1082);
    // Left color accent bar
    tft.fillRect(SCALE_X(10), cardY, SCALE_X(4), SCALE_Y(52), borderColor);
    
    // App name (small, colored)
    tft.setTextColor(borderColor);
    tft.drawString(notifApps[i], SCALE_X(20), cardY + SCALE_Y(5), 1);
    
    // Time (right aligned, dim)
    tft.setTextColor(0x4208);
    tft.drawRightString(notifTimes[i], SCREEN_W - SCALE_X(15), cardY + SCALE_Y(5), 1);
    
    // Title (white, larger)
    tft.setTextColor(TFT_WHITE);
    int maxNotifChars = (SCREEN_W > 200) ? 28 : 14;
    tft.drawString(notifTitles[i].substring(0, maxNotifChars), SCALE_X(20), cardY + SCALE_Y(22), (SCREEN_W > 200) ? 2 : 1);
    
    // Dismiss X
    tft.setTextColor(0x4208);
    tft.drawString("x", SCREEN_W - SCALE_X(22), cardY + SCALE_Y(22), (SCREEN_W > 200) ? 2 : 1);
  }
  
  // Flash notification toggle at bottom
  tft.setTextColor(0x4208);
  tft.drawCentreString(flashNotifEnabled ? "Flash: ON" : "Flash: OFF", SCREEN_W/2, SCREEN_H - SCALE_Y(28), 1);
}

void updateFlashNotification() {
  if (flashNotifActive && millis() - flashNotifStart >= 5000) { // HIGH-07 FIX: >= prevents edge-case stuck overlay
    flashNotifActive = false;
    if (currentState == STATE_EYES) {
      // Clear overlay area and redraw eyes
      tft.fillRect(0, 0, SCREEN_W, 45, COLOR_BG);
      drawEyes();
    }
  }
}

// ═══════════════════════════════════════════════════════════
// NOTES PAGE
// ═══════════════════════════════════════════════════════════

void redrawNotesPartial() {
  if (currentState != STATE_NOTES) return;
  
  tft.fillRect(0, 18, SCREEN_W, SCREEN_H - 35, COLOR_BG);
  
  tft.setTextColor(TFT_WHITE);
  for (int i = 0; i < 4; i++) {
    if (currentNotes[i].length() > 0) {
      // Bullet point style
      tft.fillCircle(SCALE_X(15), SCALE_Y(42) + (i * SCALE_Y(35)), SCALE_MIN(2), TFT_CYAN);
      tft.drawString(currentNotes[i].substring(0, 35), SCALE_X(22), SCALE_Y(35) + (i * SCALE_Y(35)), (SCREEN_W > 200) ? 2 : 1);
    }
  }
  
  tft.setTextColor(0x4208);
  // HIGH-02 FIX: Show actual ESP IP instead of placeholder
  String notesUrl = "http://" + WiFi.localIP().toString() + ":5555";
  tft.drawCentreString(notesUrl.c_str(), SCREEN_W/2, SCREEN_H - SCALE_Y(28), 1);
}

// ═══════════════════════════════════════════════════════════
// SETTINGS / SYSTEM INFO PAGE
// ═══════════════════════════════════════════════════════════

int settingsScrollY = 0;

void redrawSettingsPartial() {
  if (currentState != STATE_SETTINGS) return;
  
  tft.fillRect(0, 18, SCREEN_W, SCREEN_H - 35, COLOR_BG);
  
  int y = 25 - settingsScrollY;
  
  // Network
  if (y > -20 && y < SCREEN_H) {
    tft.setTextColor(TFT_CYAN);
    tft.drawString("NETWORK", SCALE_X(10), y, 1);
  }
  y += 12; // 8px font + 4px spacing
  if (y > -20 && y < SCREEN_H) {
    int valX = (SCREEN_W > 200) ? 100 : 50;
    tft.setTextColor(0x8410);
    tft.drawString("IP:", SCALE_X(10), y, 1);
    tft.setTextColor(TFT_GREEN);
    tft.drawString(WiFi.localIP().toString(), valX, y, 1);
  }
  y += 10; // 8px font + 2px spacing
  if (y > -20 && y < SCREEN_H) {
    int valX = (SCREEN_W > 200) ? 100 : 50;
    tft.setTextColor(0x8410);
    tft.drawString("RSSI:", SCALE_X(10), y, 1);
    tft.setTextColor(TFT_WHITE);
    char rssi[8]; sprintf(rssi, "%d dBm", WiFi.RSSI());
    tft.drawString(rssi, valX, y, 1);
  }
  
  y += 16; // 15px section gap
  // System
  if (y > -20 && y < SCREEN_H) {
    tft.setTextColor(TFT_CYAN);
    tft.drawString("SYSTEM", SCALE_X(10), y, 1);
  }
  y += 12;
  if (y > -20 && y < SCREEN_H) {
    int valX = (SCREEN_W > 200) ? 100 : 50;
    tft.setTextColor(0x8410);
    tft.drawString("CPU:", SCALE_X(10), y, 1);
    tft.setTextColor(TFT_WHITE);
    char cpu[16]; sprintf(cpu, "%d MHz", ESP.getCpuFreqMHz());
    tft.drawString(cpu, valX, y, 1);
  }
  y += 10;
  if (y > -20 && y < SCREEN_H) {
    int valX = (SCREEN_W > 200) ? 100 : 50;
    tft.setTextColor(0x8410);
    tft.drawString("RAM:", SCALE_X(10), y, 1);
    tft.setTextColor(TFT_WHITE);
    char ram[16]; sprintf(ram, "%d bytes", ESP.getFreeHeap());
    tft.drawString(ram, valX, y, 1);
  }
  y += 10;
  if (y > -20 && y < SCREEN_H) {
    int valX = (SCREEN_W > 200) ? 100 : 50;
    tft.setTextColor(0x8410);
    tft.drawString("Uptime:", SCALE_X(10), y, 1);
    unsigned long secs = millis() / 1000;
    char up[16]; sprintf(up, "%02dh%02dm%02ds", (int)(secs/3600), (int)((secs%3600)/60), (int)(secs%60));
    tft.setTextColor(TFT_WHITE);
    tft.drawString(up, valX, y, 1);
  }
  
  y += 16;
  // Sensors
  if (y > -20 && y < SCREEN_H) {
    tft.setTextColor(TFT_CYAN);
    tft.drawString("SENSORS", SCALE_X(10), y, 1);
  }
  y += 12;
  if (y > -20 && y < SCREEN_H) {
    int valX = (SCREEN_W > 200) ? 100 : 50;
    tft.setTextColor(0x8410);
    tft.drawString("LDR:", SCALE_X(10), y, 1);
    tft.setTextColor(TFT_YELLOW);
    char ld[8]; sprintf(ld, "%d", ldrValue);
    tft.drawString(ld, valX, y, 1);
    // Visual bar
    int barLen = map(ldrValue, 0, 1024, 0, SCALE_X(100));
    tft.fillRect(SCALE_X(65), y + 1, barLen, 6, TFT_YELLOW);
    tft.fillRect(SCALE_X(65) + barLen, y + 1, SCALE_X(100) - barLen, 6, 0x2104);
  }
  y += 10;
  if (y > -20 && y < SCREEN_H) {
    #if HAS_TOUCH
    tft.setTextColor(0x8410);
    tft.drawString("Touch:", SCALE_X(10), y, 1);
    tft.setTextColor(TFT_GREEN);
    tft.drawString("XPT2046 OK", SCALE_X(50), y, 1);
    #else
    tft.setTextColor(0x8410);
    tft.drawString("Buttons:", SCALE_X(10), y, 1);
    tft.setTextColor(TFT_GREEN);
    tft.drawString("L/R/SEL", SCALE_X(55), y, 1);
    #endif
  }
  
  y += 10;
  if (y > -20 && y < SCREEN_H) {
    #ifdef ESP32
    int valX = (SCREEN_W > 200) ? 100 : 50;
    tft.setTextColor(0x8410);
    tft.drawString("nRF24:", SCALE_X(10), y, 1);
    
    RF24 t_jam1(NRF1_CE_PIN, NRF1_CSN_PIN);
    bool ok1 = t_jam1.begin();
    RF24 t_jam2(NRF2_CE_PIN, NRF2_CSN_PIN);
    bool ok2 = t_jam2.begin();
    
    char buf[16];
    sprintf(buf, "%s | %s", ok1 ? "OK" : "ERR", ok2 ? "OK" : "ERR");
    tft.setTextColor((ok1 && ok2) ? TFT_GREEN : ((ok1 || ok2) ? TFT_YELLOW : TFT_RED));
    tft.drawString(buf, valX, y, 1);
    #endif
  }
  
  y += 16;
  // Time
  if (y > -20 && y < SCREEN_H) {
    tft.setTextColor(TFT_CYAN);
    tft.drawString("TIME", SCALE_X(10), y, 1);
  }
  y += 12;
  if (y > -SCALE_Y(20) && y < SCREEN_H) {
    if (timeReceived) {
      char tb[8]; sprintf(tb, "%02d:%02d", displayHour, displayMinute);
      tft.setTextColor(TFT_YELLOW);
      tft.drawString(tb, SCALE_X(10), y, (SCREEN_W > 200) ? 4 : 2);
    } else {
      tft.setTextColor(0x4208);
      tft.drawString("--:--", SCALE_X(10), y, (SCREEN_W > 200) ? 4 : 2);
    }
  }

  // ── Theme Switcher (3-way) ──
  y += SCALE_Y(16);
  if (y > -SCALE_Y(20) && y < SCREEN_H) {
    tft.setTextColor(TFT_CYAN);
    tft.drawString("THEME", SCALE_X(10), y, 1);
    tft.setTextColor(0x8410); // Dim gray
    tft.drawString("[Click/SEL]", SCALE_X(46), y, 1);
  }
  y += SCALE_Y(12);
  if (y > -SCALE_Y(20) && y < SCREEN_H) {
    // 3-segment selector track
    int sliderX = SCALE_X(10), sliderW = SCALE_X(120), sliderH = SCALE_Y(24);
    int segW = sliderW / THEME_COUNT;  // 40px per segment
    int thumbW = segW - SCALE_X(4), thumbH = SCALE_Y(20);
    int thumbX = sliderX + SCALE_X(2) + (activeTheme * segW);
    
    // Track background (color changes per theme)
    uint16_t trackCol = (activeTheme == 0) ? 0x2104 : (activeTheme == 1) ? 0x4810 : 0x1848;
    tft.fillRoundRect(sliderX, y, sliderW, sliderH, sliderH/2, trackCol);
    // Thumb
    tft.fillRoundRect(thumbX, y + SCALE_Y(2), thumbW, thumbH, thumbH/2, TFT_WHITE);
    
    // Labels inside track segments
    const char* labels[] = {"OG", "T2", "RB"};
    for (int i = 0; i < THEME_COUNT; i++) {
      tft.setTextColor(activeTheme == i ? TFT_BLACK : 0x4208);
      tft.drawCentreString(labels[i], sliderX + (i * segW) + segW/2, y + SCALE_Y(5), 1);
    }
    
    // Current theme label
    const char* themeNames[] = {"Original", "Alternate", "RoboEyes"};
    tft.setTextColor(0x8410);
    tft.drawString(themeNames[activeTheme], sliderX + sliderW + SCALE_X(6), y + SCALE_Y(4), 1);
  }
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

void renderCurrentPage() {
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
    case STATE_RUVIEW: drawRuviewPageFull(); break;
    case STATE_CLOCK_DASHBOARD: drawClockDashboardPageFull(); break;
    #ifdef ESP32
    case STATE_DR_HACK: initDrHack(); break;
    #endif
    default: break;
  }
}

void changePage(int direction) {
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
  // HIGH-06 FIX: Reset all page draw states on page change to prevent stale partial redraws
  resetSpotifyDrawState();
  resetClockDashboardDrawState();
  extern void t2_resetSpotifyDrawState();
  t2_resetSpotifyDrawState();
  settingsScrollY = 0;
  #ifdef ESP32
  // Reset Dr. Hack state when leaving
  if (currentState != STATE_DR_HACK) {
    extern DrHackSubState dhCurrentState;
    dhCurrentState = DH_MENU;
  }
  #endif
  
  // Explicit full screen clear on page transition
  tft.fillScreen(COLOR_BG);
  
  renderCurrentPage();
}

#endif
