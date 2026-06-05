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
#define ALBUM_X 10
#define ALBUM_Y 25
#define ALBUM_SIZE 96 // Large square album

// ── State Trackers for Spotify ──
String lastTrackTitle = "";
String lastArtist = "";
String lastLyric1 = "";
String lastLyric2 = "";
String lastPrevLyric = "";
bool artDrawn = false;
static bool controlsDrawn = false;
static bool cardDrawn = false;

void redrawSpotifyPartial() {
  if (currentState != STATE_SPOTIFY) return;
  
  // ── LEFT COLUMN: Album Art + Track Info ──
  int infoY = ALBUM_Y + ALBUM_SIZE + 10;
  String title = currentTrack.substring(0, 12);
  String artist = currentArtist.substring(0, 14);
  
  if (!artDrawn && albumArtReady) {
    tft.pushImage(ALBUM_X, ALBUM_Y, ALBUM_SIZE, ALBUM_SIZE, albumArt);
    
    // Repaint the shadow wrap since we just pasted the square
    int blurRadius = 4;
    for (int i = 1; i <= blurRadius; i++) {
      uint16_t shadeColor = (i == 1) ? 0x1042 : (i == 2) ? 0x0821 : 0x0000;
      tft.drawFastHLine(ALBUM_X + i, ALBUM_Y + ALBUM_SIZE + i - 1, ALBUM_SIZE, shadeColor);
      tft.drawFastVLine(ALBUM_X + ALBUM_SIZE + i - 1, ALBUM_Y + i, ALBUM_SIZE, shadeColor);
    }
    artDrawn = true;
  }
  
  if (title != lastTrackTitle) {
    tft.fillRect(ALBUM_X, infoY, 110, 20, COLOR_BG);
    tft.setTextColor(TFT_WHITE, COLOR_BG);
    tft.drawString(title, ALBUM_X, infoY, 2); // Bold white
    lastTrackTitle = title;
  }
  if (artist != lastArtist) {
    tft.fillRect(ALBUM_X, infoY + 18, 110, 16, COLOR_BG);
    tft.setTextColor(0x8410, COLOR_BG); // Dim gray
    tft.drawString(artist, ALBUM_X, infoY + 18, 1);
    drawIconHeart(ALBUM_X + tft.textWidth(artist, 1) + 8, infoY + 22, TFT_WHITE);
    lastArtist = artist;
  }

  // ── CENTER COLUMN: Playback Controls ──
  // Raised main row slightly to fit the new bottom row
  int ctrlY = SCALE_Y(118);
  int ctrlX = SCALE_X(160);
  
  
  if (!controlsDrawn) {
    tft.fillRect(SCALE_X(110), ctrlY - 20, SCALE_X(100), 60, COLOR_BG); 
    
    // Main Row: Prev, (Play/Pause is drawn below), Next  — Wide spacing
    drawIconPrev(SCALE_X(130), ctrlY, TFT_WHITE);
    drawIconNext(SCALE_X(190), ctrlY, TFT_WHITE);
    
    // Bottom Row: Shuffle, Repeat
    drawIconShuffle(SCALE_X(140), ctrlY + 24, 0x6B4D);
    drawIconRepeat(SCALE_X(180), ctrlY + 24, 0x6B4D);
    
    controlsDrawn = true;
  }

  // Play/Pause Circle
  static bool lastPlaying = false;
  if (isPlaying != lastPlaying) { // HIGH-04 FIX: removed || true that caused per-frame redraws
    tft.fillCircle(ctrlX, ctrlY, 14, TFT_WHITE);
    if (isPlaying) drawIconPause(ctrlX, ctrlY, COLOR_BG);
    else drawIconPlay(ctrlX + 2, ctrlY, COLOR_BG);
    lastPlaying = isPlaying;
  }

  // ── CENTER COLUMN: Progress Bar + Waveform Pulse ──
  // SUBMISSION 2: Animated waveform pulse using sin() on song position
  // This requires 160MHz FPU to evaluate 8 waveform bars per frame without jitter.
  int barX = SCALE_X(120);
  int barY = SCALE_Y(150);
  int barW = SCALE_X(80);
  
  // Draw scrubber track
  tft.fillRect(barX, barY - 2, barW + 10, 6, COLOR_BG);
  tft.fillRect(barX, barY, barW, 2, 0x4208);
  if (playDuration > 0) {
    int w = map(playProgress, 0, playDuration, 0, barW);
    w = constrain(w, 0, barW);
    tft.fillRect(barX, barY, w, 2, TFT_WHITE);
    tft.fillCircle(barX + w, barY + 1, 3, TFT_WHITE);
  }
  
  // SUBMISSION 2: Waveform pulse bars under the scrubber
  // Phase offset is derived from playProgress so it moves with the song
  float phase = (float)(playProgress % 3000) / 3000.0 * 2.0 * PI;
  tft.fillRect(barX, barY + 6, barW, 12, COLOR_BG); // Clear waveform zone
  int numBars = 8;
  int barSpacing = barW / numBars;
  for (int i = 0; i < numBars; i++) {
    float wave = sin(phase + (float)i * 0.8f);
    int barH = (int)(abs(wave) * 8.0f) + 2; // 2-10px tall
    int bx = barX + i * barSpacing;
    int by = barY + 6 + (10 - barH); // Align to bottom of zone
    
    // Bars left of scrubber head: bright green; right: dim
    int scrubberX = (playDuration > 0) ? map(playProgress, 0, playDuration, barX, barX + barW) : barX;
    uint16_t bColor = (bx < scrubberX) ? 0x07E0 : 0x2104; // Green vs dark
    tft.fillRect(bx, by, barSpacing - 2, barH, bColor);
  }
  
  // Time labels
  char t1[16]; char t2[16];
  sprintf(t1, "%d:%02d", (playProgress/1000)/60, (playProgress/1000)%60);
  sprintf(t2, "%d:%02d", (playDuration/1000)/60, (playDuration/1000)%60);
  tft.setTextColor(0x8410, COLOR_BG);
  tft.drawString(t1, barX - 5, barY + 20, 1);
  tft.fillRect(barX + barW - 30, barY + 20, 40, 12, COLOR_BG);
  tft.drawRightString(t2, barX + barW + 5, barY + 20, 1);

  // ── RIGHT COLUMN: Lyrics Card (only on large screens) ──
  // On 160×128 screens there's no room for a 105px card to the right
  if (SCREEN_W > 200) {
  int cardX = SCALE_X(210);
  int cardY = SCALE_Y(25);
  int cardW = SCALE_X(105);
  int cardH = SCALE_Y(190);
  
  if (!cardDrawn) {
    // Advanced 160MHz Premium Detailing: Drop Shadow
    // Background is black, so to cast a shadow we drop a subtle deep grey aura 
    tft.fillRoundRect(cardX + 4, cardY + 4, cardW, cardH, 8, 0x0821);
    tft.fillRoundRect(cardX + 2, cardY + 2, cardW, cardH, 8, 0x1042);
    
    // Advanced 160MHz Premium Detailing: Metallic Glass Gradient
    uint16_t startColor = 0x2125; // Brighter violet top highlight
    uint16_t endColor = 0x0821;   // Darker base
    uint16_t sr = (startColor >> 11) & 0x1F, sg = (startColor >> 5) & 0x3F, sb = startColor & 0x1F;
    uint16_t er = (endColor >> 11) & 0x1F, eg = (endColor >> 5) & 0x3F, eb = endColor & 0x1F;
    
    for (int j = 0; j < cardH; j++) {
      float t = (float)j / cardH;
      uint16_t cr = sr + (er - sr) * t;
      uint16_t cg = sg + (eg - sg) * t;
      uint16_t cb = sb + (eb - sb) * t;
      uint16_t color = (cr << 11) | (cg << 5) | cb;
      
      int r = 8;
      if (j < r || j >= cardH - r) {
        int offset = r - sqrt(r*r - pow(r - (j < r ? j : cardH - 1 - j), 2));
        tft.drawFastHLine(cardX + offset, cardY + j, cardW - 2*offset, color);
      } else {
        tft.drawFastHLine(cardX, cardY + j, cardW, color);
      }
    }
    
    tft.setTextColor(0x8410, startColor); // Transparent proxy
    tft.drawString("UP NEXT / LYRICS", cardX + 8, cardY + 10, 1);
    
    // Share/Options placeholder icons
    tft.fillCircle(cardX + cardW - 18, cardY + 12, 1, 0x6B4D);
    tft.fillCircle(cardX + cardW - 14, cardY + 12, 1, 0x6B4D);
    tft.fillCircle(cardX + cardW - 10, cardY + 12, 1, 0x6B4D);
    
    cardDrawn = true;
  }
  
  // Check if we need to redraw any lyric lines
  if (currentLyrics != lastLyric1 || currentLyricsLine2 != lastLyric2 || prevLyricsLine != lastPrevLyric) {
    // Clear lyrics content block gracefully with a dark solid to match gradient bottom
    tft.fillRect(cardX + 4, cardY + 30, cardW - 8, cardH - 35, 0x0821);
    
    int yC = cardY + 35;
    
    // Previous lyric (dim)
    if (prevLyricsLine.length() > 0) {
      tft.setTextColor(0x4208, 0x0821);
      String p1 = prevLyricsLine.substring(0, 12);
      String p2 = prevLyricsLine.length() > 12 ? prevLyricsLine.substring(12, 24) : "";
      tft.drawString(p1, cardX + 8, yC, 2); yC += 16;
      if (p2.length() > 0) { tft.drawString(p2, cardX + 8, yC, 2); yC += 16; }
      yC += 8; // Margin
    } else {
      yC += 24; // Empty line padding
    }
    
    // Current lyric (bright, bold substitute)
    tft.setTextColor(TFT_WHITE, 0x0821); // Bright white
    String c1 = currentLyrics.substring(0, 12);
    String c2 = currentLyrics.length() > 12 ? currentLyrics.substring(12, 24) : "";
    tft.drawString(c1, cardX + 8, yC, 2); yC += 16;
    if (c2.length() > 0) { tft.drawString(c2, cardX + 8, yC, 2); yC += 16; }
    yC += 8; // Margin
    
    // Next lyric (dimming)
    if (currentLyricsLine2.length() > 0) {
      tft.setTextColor(0x4208, 0x0821); // Dim gray again
      String n1 = currentLyricsLine2.substring(0, 12);
      String n2 = currentLyricsLine2.length() > 12 ? currentLyricsLine2.substring(12, 24) : "";
      tft.drawString(n1, cardX + 8, yC, 2); yC += 16;
      if (n2.length() > 0) tft.drawString(n2, cardX + 8, yC, 2);
    }
    
    // Assign tracker states to prevent re-rendering when unnecessary
    lastLyric1 = currentLyrics;
    lastLyric2 = currentLyricsLine2;
    lastPrevLyric = prevLyricsLine;
  }
  } // end if (SCREEN_W > 200) — lyrics card
}

// Reset static trackers when entering the Spotify page fresh
void resetSpotifyDrawState() {
  lastTrackTitle = "";
  lastArtist = "";
  lastLyric1 = "";
  lastLyric2 = "";
  lastPrevLyric = "";
  artDrawn = false;
  controlsDrawn = false;
  cardDrawn = false;
}

// ═══════════════════════════════════════════════════════════
// ALBUM ART PROCESSING
// ═══════════════════════════════════════════════════════════


void completeAlbumArt() {
  receivingArt = false;
  albumArtReady = true;
  
  if (currentState == STATE_SPOTIFY) {
    if (activeTheme == 1) {
      extern void t2_redrawSpotifyPartial();
      t2_redrawSpotifyPartial();
    } else {
      // Advanced 160MHz Drop Shadow around Album Art 
      // (We draw this strictly when album art is done, so it wraps perfectly)
      int blurRadius = 4;
      for (int i = 1; i <= blurRadius; i++) {
        uint16_t shadeColor = (i == 1) ? 0x1042 : (i == 2) ? 0x0821 : 0x0000;
        // Bottom shadow
        tft.drawFastHLine(ALBUM_X + i, ALBUM_Y + ALBUM_SIZE + i - 1, ALBUM_SIZE, shadeColor);
        // Right shadow
        tft.drawFastVLine(ALBUM_X + ALBUM_SIZE + i - 1, ALBUM_Y + i, ALBUM_SIZE, shadeColor);
      }
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
  int cy = SCREEN_H / 2 - 10;
  
  float progress = 0;
  if (pomoTotal > 0 && pomoActive) {
    progress = 1.0 - ((float)pomoRemaining / pomoTotal);
  }
  
  uint16_t ringColor = pomoIsBreak ? TFT_GREEN : 0xDF26; // Orange
  
  if (lastRemaining != pomoRemaining || lastIsBreak != pomoIsBreak || lastActive != pomoActive) {
    // Only clear the text area inside the ring instead of a massive 160x160 rectangle
    tft.fillRect(cx - 50, cy - 30, 100, 60, COLOR_BG);
    drawSmoothRing(cx, cy, 55, 6, progress, ringColor, 0x18E3);
    
    // Center time display
    int minutes = pomoRemaining / 60;
    int seconds = pomoRemaining % 60;
    char timeStr[8];
    sprintf(timeStr, "%02d:%02d", minutes, seconds);
    
    tft.setTextColor(TFT_WHITE, COLOR_BG);
    tft.drawCentreString(timeStr, cx, cy - 12, 4);
    
    // Label
    tft.setTextColor(pomoIsBreak ? TFT_GREEN : TFT_CYAN, COLOR_BG);
    tft.drawCentreString(pomoIsBreak ? "BREAK" : "FOCUS", cx, cy + 15, 2);
    
    // Session count & Status (only clear their specific lines)
    tft.fillRect(cx - 60, cy + 55, 120, 30, COLOR_BG);
    tft.setTextColor(0x8410, COLOR_BG);
    char sessStr[16];
    sprintf(sessStr, "%d sessions", pomoSessions);
    tft.drawCentreString(sessStr, cx, cy + 55, 1);
    
    if (!pomoActive) {
      tft.setTextColor(0x6B4D, COLOR_BG);
      tft.drawCentreString("TAP TO START", cx, cy + 70, 1);
    } else {
      tft.setTextColor(0x4208, COLOR_BG);
      tft.drawCentreString("TAP TO PAUSE", cx, cy + 70, 1);
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
  
  // City name
  tft.setTextColor(0x8410, COLOR_BG);
  tft.drawString(weatherCity, SCALE_X(15), SCALE_Y(22), (SCREEN_W > 200) ? 2 : 1);
  
  // Big temperature — Font 4 on small screens (Font 7 overflows 160px)
  char tempStr[8];
  sprintf(tempStr, "%d", weatherTemp);
  tft.setTextColor(TFT_WHITE, COLOR_BG);
  int tempFont = (SCREEN_W > 200) ? 7 : 4;
  tft.drawString(tempStr, SCALE_X(15), SCALE_Y(48), tempFont);
  
  // Degree symbol + C
  int tw = tft.textWidth(tempStr, tempFont);
  tft.setTextColor(0x8410, COLOR_BG);
  tft.drawString("C", SCALE_X(15) + tw + SCALE_X(8), SCALE_Y(48), (SCREEN_W > 200) ? 4 : 2);
  tft.drawCircle(SCALE_X(15) + tw + SCALE_X(4), SCALE_Y(50), SCALE_MIN(3), 0x8410);
  
  // Condition text
  uint16_t condColor = getWeatherColor(weatherCode);
  tft.setTextColor(condColor, COLOR_BG);
  tft.drawString(weatherCondition, SCALE_X(15), SCALE_Y(110), (SCREEN_W > 200) ? 2 : 1);
  
  // Details column — right side on large, compact on small
  int dx = (SCREEN_W > 200) ? 175 : SCALE_X(80);
  int detailFont = (SCREEN_W > 200) ? 2 : 1;
  int detailStep = (SCREEN_W > 200) ? 35 : SCALE_Y(18);
  int dy0 = (SCREEN_W > 200) ? 30 : SCALE_Y(30);
  
  tft.setTextColor(0x6B4D, COLOR_BG);
  tft.drawString("Feels", dx, dy0, 1);
  tft.setTextColor(TFT_WHITE, COLOR_BG);
  char fb[8]; sprintf(fb, "%d C", weatherFeels);
  tft.drawString(fb, dx, dy0 + 10, detailFont);
  
  tft.setTextColor(0x6B4D, COLOR_BG);
  tft.drawString("Humid", dx, dy0 + detailStep, 1);
  tft.setTextColor(TFT_CYAN, COLOR_BG);
  char hb[8]; sprintf(hb, "%d%%", weatherHumidity);
  tft.drawString(hb, dx, dy0 + detailStep + 10, detailFont);
  
  tft.setTextColor(0x6B4D, COLOR_BG);
  tft.drawString("Wind", dx, dy0 + detailStep * 2, 1);
  tft.setTextColor(TFT_WHITE, COLOR_BG);
  char wb[12]; sprintf(wb, "%d km/h", weatherWind);
  tft.drawString(wb, dx, dy0 + detailStep * 2 + 10, detailFont);
  
  tft.setTextColor(0x6B4D, COLOR_BG);
  tft.drawString("Hi/Lo", dx, dy0 + detailStep * 3, 1);
  tft.setTextColor(TFT_WHITE, COLOR_BG);
  char hlb[12]; sprintf(hlb, "%d/%d", weatherHigh, weatherLow);
  tft.drawString(hlb, dx, dy0 + detailStep * 3 + 10, detailFont);
  
  // Sunrise/Sunset — only if screen is tall enough
  if (SCREEN_H > 160) {
    tft.setTextColor(TFT_YELLOW, COLOR_BG);
    tft.drawString("^", 15, 145, 2);
    tft.setTextColor(0x8410, COLOR_BG);
    tft.drawString(weatherSunrise, 30, 145, 1);
    
    tft.setTextColor(0xFBE0, COLOR_BG);
    tft.drawString("v", 15, 160, 2);
    tft.setTextColor(0x8410, COLOR_BG);
    tft.drawString(weatherSunset, 30, 160, 1);
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
    int cardY = 22 + (i * 58);
    uint16_t borderColor = getNotifColor(notifApps[i]);
    
    // Card background
    tft.fillRoundRect(10, cardY, SCREEN_W - 20, 52, 4, 0x1082);
    // Left color accent bar
    tft.fillRect(10, cardY, 4, 52, borderColor);
    
    // App name (small, colored)
    tft.setTextColor(borderColor);
    tft.drawString(notifApps[i], 20, cardY + 5, 1);
    
    // Time (right aligned, dim)
    tft.setTextColor(0x4208);
    tft.drawRightString(notifTimes[i], SCREEN_W - 15, cardY + 5, 1);
    
    // Title (white, larger)
    tft.setTextColor(TFT_WHITE);
    int maxNotifChars = (SCREEN_W > 200) ? 28 : 14;
    tft.drawString(notifTitles[i].substring(0, maxNotifChars), 20, cardY + 22, (SCREEN_W > 200) ? 2 : 1);
    
    // Dismiss X
    tft.setTextColor(0x4208);
    tft.drawString("x", SCREEN_W - 22, cardY + 22, 2);
  }
  
  // Flash notification toggle at bottom
  tft.setTextColor(0x4208);
  tft.drawCentreString(flashNotifEnabled ? "Flash: ON" : "Flash: OFF", SCREEN_W/2, SCREEN_H - 28, 1);
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
      tft.fillCircle(15, 42 + (i * 35), 2, TFT_CYAN);
      tft.drawString(currentNotes[i].substring(0, 35), 22, 35 + (i * 35), 2);
    }
  }
  
  tft.setTextColor(0x4208);
  // HIGH-02 FIX: Show actual ESP IP instead of placeholder
  String notesUrl = "http://" + WiFi.localIP().toString() + ":5555";
  tft.drawCentreString(notesUrl.c_str(), SCREEN_W/2, SCREEN_H - 28, 1);
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
    tft.drawString("NETWORK", 10, y, 2);
  }
  y += 20;
  if (y > -20 && y < SCREEN_H) {
    tft.setTextColor(0x8410);
    tft.drawString("IP:", 10, y, 1);
    tft.setTextColor(TFT_GREEN);
    tft.drawString(WiFi.localIP().toString(), 30, y, 1);
  }
  y += 14;
  if (y > -20 && y < SCREEN_H) {
    tft.setTextColor(0x8410);
    tft.drawString("RSSI:", 10, y, 1);
    tft.setTextColor(TFT_WHITE);
    char rssi[8]; sprintf(rssi, "%d dBm", WiFi.RSSI());
    tft.drawString(rssi, 45, y, 1);
  }
  
  y += 20;
  // System
  if (y > -20 && y < SCREEN_H) {
    tft.setTextColor(TFT_CYAN);
    tft.drawString("SYSTEM", 10, y, 2);
  }
  y += 20;
  if (y > -20 && y < SCREEN_H) {
    tft.setTextColor(0x8410);
    tft.drawString("CPU:", 10, y, 1);
    tft.setTextColor(TFT_WHITE);
    char cpu[16]; sprintf(cpu, "%d MHz", ESP.getCpuFreqMHz());
    tft.drawString(cpu, 35, y, 1);
  }
  y += 14;
  if (y > -20 && y < SCREEN_H) {
    tft.setTextColor(0x8410);
    tft.drawString("RAM:", 10, y, 1);
    tft.setTextColor(TFT_WHITE);
    char ram[16]; sprintf(ram, "%d bytes", ESP.getFreeHeap());
    tft.drawString(ram, 35, y, 1);
  }
  y += 14;
  if (y > -20 && y < SCREEN_H) {
    tft.setTextColor(0x8410);
    tft.drawString("Uptime:", 10, y, 1);
    unsigned long secs = millis() / 1000;
    char up[16]; sprintf(up, "%02dh%02dm%02ds", (int)(secs/3600), (int)((secs%3600)/60), (int)(secs%60));
    tft.setTextColor(TFT_WHITE);
    tft.drawString(up, 50, y, 1);
  }
  
  y += 20;
  // Sensors
  if (y > -20 && y < SCREEN_H) {
    tft.setTextColor(TFT_CYAN);
    tft.drawString("SENSORS", 10, y, 2);
  }
  y += 20;
  if (y > -20 && y < SCREEN_H) {
    tft.setTextColor(0x8410);
    tft.drawString("LDR:", 10, y, 1);
    tft.setTextColor(TFT_YELLOW);
    char ld[8]; sprintf(ld, "%d", ldrValue);
    tft.drawString(ld, 35, y, 1);
    // Visual bar
    int barLen = map(ldrValue, 0, 1024, 0, 100);
    tft.fillRect(65, y + 1, barLen, 6, TFT_YELLOW);
    tft.fillRect(65 + barLen, y + 1, 100 - barLen, 6, 0x2104);
  }
  y += 14;
  if (y > -20 && y < SCREEN_H) {
    #if HAS_TOUCH
    tft.setTextColor(0x8410);
    tft.drawString("Touch:", 10, y, 1);
    tft.setTextColor(TFT_GREEN);
    tft.drawString("XPT2046 OK", 50, y, 1);
    #else
    tft.setTextColor(0x8410);
    tft.drawString("Buttons:", 10, y, 1);
    tft.setTextColor(TFT_GREEN);
    tft.drawString("L/R/SEL", 55, y, 1);
    #endif
  }
  
  y += 20;
  // Time
  if (y > -20 && y < SCREEN_H) {
    tft.setTextColor(TFT_CYAN);
    tft.drawString("TIME", 10, y, 2);
  }
  y += 20;
  if (y > -20 && y < SCREEN_H) {
    if (timeReceived) {
      char tb[8]; sprintf(tb, "%02d:%02d", displayHour, displayMinute);
      tft.setTextColor(TFT_YELLOW);
      tft.drawString(tb, 10, y, 4);
    } else {
      tft.setTextColor(0x4208);
      tft.drawString("--:--", 10, y, 4);
    }
  }

  // ── Theme Switcher (3-way) ──
  y += 40;
  if (y > -20 && y < SCREEN_H) {
    tft.setTextColor(TFT_CYAN);
    tft.drawString("THEME", 10, y, 2);
  }
  y += 22;
  if (y > -20 && y < SCREEN_H) {
    // 3-segment selector track
    int sliderX = 10, sliderW = 120, sliderH = 24;
    int segW = sliderW / THEME_COUNT;  // 40px per segment
    int thumbW = segW - 4, thumbH = 20;
    int thumbX = sliderX + 2 + (activeTheme * segW);
    
    // Track background (color changes per theme)
    uint16_t trackCol = (activeTheme == 0) ? 0x2104 : (activeTheme == 1) ? 0x4810 : 0x1848;
    tft.fillRoundRect(sliderX, y, sliderW, sliderH, sliderH/2, trackCol);
    // Thumb
    tft.fillRoundRect(thumbX, y + 2, thumbW, thumbH, thumbH/2, TFT_WHITE);
    
    // Labels inside track segments
    const char* labels[] = {"OG", "T2", "RB"};
    for (int i = 0; i < THEME_COUNT; i++) {
      tft.setTextColor(activeTheme == i ? TFT_BLACK : 0x4208);
      tft.drawCentreString(labels[i], sliderX + (i * segW) + segW/2, y + 5, 2);
    }
    
    // Current theme label
    const char* themeNames[] = {"Original", "Alternate", "RoboEyes"};
    tft.setTextColor(0x8410);
    tft.drawString(themeNames[activeTheme], sliderX + sliderW + 6, y + 4, 1);
  }
}

// ═══════════════════════════════════════════════════════════
// PAGE DRAWING & ORCHESTRATION
// ═══════════════════════════════════════════════════════════

void drawEyesPage() {
  if (activeTheme == 2) { t3_drawEyesPage(); drawStatusBar(); return; }
  if (activeTheme == 1) { t2_drawEyesPage(); drawStatusBar(); return; }
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
  settingsScrollY = 0;
  #ifdef ESP32
  // Reset Dr. Hack state when leaving
  if (currentState != STATE_DR_HACK) {
    extern DrHackSubState dhCurrentState;
    dhCurrentState = DH_MENU;
  }
  #endif
  renderCurrentPage();
}

#endif
