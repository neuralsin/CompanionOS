// ═══════════════════════════════════════════════════════════
// COMPANION OS v7.0 — MINIMALIST RETRO WATCH CHRONOGRAPH
// 
// Clean, elegant, uncluttered timepiece design.
// Zero text collisions • High-contrast vector hands • Smooth 1s tick
// ═══════════════════════════════════════════════════════════
#ifndef PAGE_RETRO_WATCH_H
#define PAGE_RETRO_WATCH_H

#include "globals.h"
#include "ui_components.h"
#include <math.h>

extern String currentTrack;
extern bool isPlaying;
extern void drawPageIndicator(int current, int total);

// Minimalist Luxury Palette
#define RW_BG         0x0000  // Pure OLED black
#define RW_BEZEL      0x2965  // Sleek titanium grey
#define RW_ACCENT     0x07FF  // Electric cyan
#define RW_GOLD       0xFEA0  // Warm gold
#define RW_MUTED      0x632C  // Subtle muted grey
#define RW_RED        0xF986  // Crimson second hand

static int rw_lastSec = -1;
static int rw_lastMin = -1;
static int rw_lastHour = -1;
static String rw_lastTrack = "";
static bool rw_drawn = false;
static String rw_lastStatus = "";

void resetRetroWatchDrawState() {
  rw_lastSec = -1;
  rw_lastMin = -1;
  rw_lastHour = -1;
  rw_lastTrack = "";
  rw_drawn = false;
  rw_lastStatus = "";
}

// ── Dial & Ticks ──
static void rw_drawDial(int cx, int cy, int r) {
  // Outer bezel ring
  tft.drawCircle(cx, cy, r, RW_BEZEL);
  tft.drawCircle(cx, cy, r - 1, 0x1082);

  // 12 Minimalist Hour Markers
  for (int i = 0; i < 12; i++) {
    float angle = i * 30.0f * DEG_TO_RAD - M_PI_2;
    float cosA = cos(angle);
    float sinA = sin(angle);

    int len = (i % 3 == 0) ? 6 : 3;
    uint16_t col = (i == 0) ? RW_ACCENT : (i % 3 == 0) ? RW_GOLD : RW_MUTED;

    int x1 = cx + cosA * (r - 2);
    int y1 = cy + sinA * (r - 2);
    int x2 = cx + cosA * (r - 2 - len);
    int y2 = cy + sinA * (r - 2 - len);
    tft.drawLine(x1, y1, x2, y2, col);
    if (i % 3 == 0) {
      // Double width for 12, 3, 6, 9
      tft.drawLine(x1 + (sinA != 0 ? 1 : 0), y1 + (cosA != 0 ? 1 : 0),
                   x2 + (sinA != 0 ? 1 : 0), y2 + (cosA != 0 ? 1 : 0), col);
    }
  }
}

// ── Vector Hands ──
static void rw_drawHands(int cx, int cy, int r, int h, int m, int s) {
  float hAngle = ((h % 12) + m / 60.0f) * 30.0f * DEG_TO_RAD - M_PI_2;
  float mAngle = (m + s / 60.0f) * 6.0f * DEG_TO_RAD - M_PI_2;
  float sAngle = s * 6.0f * DEG_TO_RAD - M_PI_2;

  // Hour hand (Bold sword hand)
  int hLen = r * 0.48f;
  int hx = cx + cos(hAngle) * hLen;
  int hy = cy + sin(hAngle) * hLen;
  tft.drawLine(cx, cy, hx, hy, TFT_WHITE);
  tft.drawLine(cx + 1, cy, hx + 1, hy, TFT_WHITE);
  tft.drawLine(cx, cy + 1, hx, hy + 1, TFT_WHITE);

  // Minute hand (Sleek cyan-tipped sword)
  int mLen = r * 0.72f;
  int mx = cx + cos(mAngle) * mLen;
  int my = cy + sin(mAngle) * mLen;
  tft.drawLine(cx, cy, mx, my, TFT_WHITE);
  tft.drawLine(cx + 1, cy, mx + 1, my, RW_ACCENT);

  // Second hand (Ultra-fine red needle with tail)
  int sLen = r * 0.82f;
  int sTail = r * 0.18f;
  int sx = cx + cos(sAngle) * sLen;
  int sy = cy + sin(sAngle) * sLen;
  int tx = cx - cos(sAngle) * sTail;
  int ty = cy - sin(sAngle) * sTail;
  tft.drawLine(tx, ty, sx, sy, RW_RED);

  // Center pivot dot (Clean metallic cap)
  tft.fillCircle(cx, cy, 2, RW_ACCENT);
  tft.drawPixel(cx, cy, TFT_BLACK);
}

static String rw_statusKey() {
  return String(uiPageGeneration) + "/" + currentTrack + "/" + String(isPlaying) + "/" + String(wifiConnected) + "/" + String(timeReceived);
}

static void rw_drawContent(bool updateMedia = true) {
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  const int mediaY = SCREEN_H - UI_FOOTER_H - tft.fontHeight(1) - 2;
  const int top = UI_HEADER_H + 15;
  const int cy = (top + mediaY - 4) / 2;
  const int cx = SCREEN_W / 2;
  const int r = min(cx - 4, (mediaY - 4 - top) / 2 - 1);

  tft.fillRect(0, UI_HEADER_H, SCREEN_W, 12, RW_BG);
  drawTruncatedText(4, UI_HEADER_H + 2, "WATCH", SCREEN_W / 2 - 8, RW_ACCENT, 1, RW_BG);
  char digital[12];
  if (timeReceived) {
    int hr12 = displayHour % 12;
    if (hr12 == 0) hr12 = 12;
    snprintf(digital, sizeof(digital), "%02d:%02d", hr12, displayMinute);
  } else {
    snprintf(digital, sizeof(digital), "--:--");
  }
  tft.setTextColor(RW_MUTED, RW_BG);
  tft.drawRightString(digital, SCREEN_W - 4, UI_HEADER_H + 2, 1);

  // Clear the complete hand sweep, then restore ticks it could have touched.
  tft.fillCircle(cx, cy, r - 1, RW_BG);
  rw_drawDial(cx, cy, r);
  if (timeReceived) {
    rw_drawHands(cx, cy, r, displayHour, displayMinute, displaySecond);
  } else {
    drawTruncatedText(cx - 36, cy - 4, "No time sync", 72, RW_MUTED, 1, RW_BG);
  }

  if (updateMedia) {
    tft.fillRect(0, mediaY, SCREEN_W, tft.fontHeight(1) + 2, RW_BG);
    String status;
    if (currentTrack.length()) status = String(isPlaying ? "Playing: " : "Paused: ") + currentTrack;
    else status = wifiConnected ? "No media - WiFi online" : "No media - WiFi offline";
    drawTruncatedText(4, mediaY, status.c_str(), SCREEN_W - 8, RW_ACCENT, 1, RW_BG);
  }
}

void drawRetroWatchPage() {
  tft.fillRect(0, UI_HEADER_H, SCREEN_W, SCREEN_H - UI_HEADER_H, RW_BG);
  rw_drawContent();
  drawPageIndicator(STATE_RETRO_WATCH, STATE_COUNT);
  rw_drawn = true;
  rw_lastSec = displaySecond;
  rw_lastMin = displayMinute;
  rw_lastHour = displayHour;
  rw_lastTrack = currentTrack;
  rw_lastStatus = rw_statusKey();
}

void redrawRetroWatchPartial() {
  if (currentState != STATE_RETRO_WATCH) return;
  String status = rw_statusKey();
  if (!rw_drawn) {
    drawRetroWatchPage();
  } else if (displaySecond != rw_lastSec || displayMinute != rw_lastMin ||
             displayHour != rw_lastHour || status != rw_lastStatus) {
    rw_drawContent(status != rw_lastStatus);
    rw_lastSec = displaySecond;
    rw_lastMin = displayMinute;
    rw_lastHour = displayHour;
    rw_lastTrack = currentTrack;
    rw_lastStatus = status;
  }
}

#endif
