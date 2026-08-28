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

void resetRetroWatchDrawState() {
  rw_lastSec = -1;
  rw_lastMin = -1;
  rw_lastHour = -1;
  rw_lastTrack = "";
  rw_drawn = false;
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

// ── Corner Telemetry (Minimalist) ──
static void rw_drawMinimalInfo(int cx, int cy, int r) {
  // Top: Minimal Digital Time
  char digBuf[10];
  int hr12 = displayHour % 12;
  if (hr12 == 0) hr12 = 12;
  sprintf(digBuf, "%d:%02d", hr12, displayMinute);
  
  tft.setTextColor(RW_MUTED, RW_BG);
  tft.drawCentreString(digBuf, cx, cy - r - SCALE_Y(1), 1);

  // Bottom: Song / Wi-Fi status
  if (musicPlaying && currentTrack.length() > 0) {
    String s = "♪ " + currentTrack;
    drawTruncatedText(SCALE_X(6), SCR_H - SCALE_Y(9), s.c_str(), SCR_W - SCALE_X(12), RW_ACCENT, 1);
  } else {
    tft.setTextColor(wifiConnected ? RW_MUTED : 0x4208, RW_BG);
    tft.drawCentreString(wifiConnected ? "ONLINE" : "OFFLINE", cx, cy + r - SCALE_Y(7), 1);
  }
}

// ── Full Page ──
void drawRetroWatchPage() {
  tft.fillScreen(RW_BG);

  int cx = SCR_CX;
  int cy = SCR_CY;
  int r = min(SCR_CX, SCR_CY) - SCALE_MIN(6);

  rw_drawDial(cx, cy, r);
  rw_drawMinimalInfo(cx, cy, r);
  rw_drawHands(cx, cy, r, displayHour, displayMinute, displaySecond);

  drawPageIndicator(STATE_RETRO_WATCH, STATE_COUNT);

  rw_drawn = true;
  rw_lastSec = displaySecond;
  rw_lastMin = displayMinute;
  rw_lastHour = displayHour;
  rw_lastTrack = currentTrack;
}

// ── 1s Partial Update (Zero Flicker) ──
void redrawRetroWatchPartial() {
  if (currentState != STATE_RETRO_WATCH) return;

  if (!rw_drawn || currentTrack != rw_lastTrack) {
    drawRetroWatchPage();
    return;
  }

  if (displaySecond != rw_lastSec || displayMinute != rw_lastMin) {
    int cx = SCR_CX;
    int cy = SCR_CY;
    int r = min(SCR_CX, SCR_CY) - SCALE_MIN(6);

    // Clear inner dial only (leaves bezel intact)
    tft.fillCircle(cx, cy, r - 7, RW_BG);

    // Minimal digital time update
    char digBuf[10];
    int hr12 = displayHour % 12;
    if (hr12 == 0) hr12 = 12;
    sprintf(digBuf, "%d:%02d", hr12, displayMinute);
    tft.setTextColor(RW_MUTED, RW_BG);
    tft.drawCentreString(digBuf, cx, cy - r - SCALE_Y(1), 1);

    // Redraw hands
    rw_drawHands(cx, cy, r, displayHour, displayMinute, displaySecond);

    rw_lastSec = displaySecond;
    rw_lastMin = displayMinute;
    rw_lastHour = displayHour;
    rw_lastTrack = currentTrack;
  }
}

#endif // PAGE_RETRO_WATCH_H
