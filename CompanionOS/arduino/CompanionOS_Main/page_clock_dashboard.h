#ifndef PAGE_CLOCK_DASHBOARD_H
#define PAGE_CLOCK_DASHBOARD_H

#include "globals.h"
#include "ui_components.h"
#include <math.h>

// ═══════════════════════════════════════════════════════════
// CLOCK & EXPANDED DYNAMIC SPOTIFY / LYRICS DASHBOARD
// ═══════════════════════════════════════════════════════════

#define CLK_BG        0x0841  // Deep dark blue-black
#define CLK_CARD_BG   0x1082  // Dark surface
#define CLK_BORDER    0x2104  // Subtle border
#define CLK_CYAN      0x07FF  // Vibrant cyan
#define CLK_PURPLE    0x981F  // Deep glowing purple
#define CLK_MAGENTA   0xF81F  // Bright magenta
#define CLK_GREEN     0x07E0  // Battery green
#define CLK_BLUE      0x03FF  // Bluetooth blue
#define CLK_TEXT_DIM  0x8410  // Dim text

static int lastClockSec = -1;
static int lastClockMin = -1;
static int lastClockHour = -1;
static String lastClockTrack = "";
static bool clockDashDrawn = false;
static uint8_t eqBarHeights[5] = {3, 7, 5, 8, 4};

void resetClockDashboardDrawState() {
  lastClockSec = -1;
  lastClockMin = -1;
  lastClockHour = -1;
  lastClockTrack = "";
  clockDashDrawn = false;
}

// ── Vector Drawing & Dynamic Sizing Text Helpers ──

static void drawAnalogClock(int cx, int cy, int r, int h, int m, int s) {
  // Outer gradient ring
  tft.drawCircle(cx, cy, r + 2, CLK_PURPLE);
  tft.drawCircle(cx, cy, r + 1, CLK_CYAN);
  tft.drawCircle(cx, cy, r, CLK_BORDER);

  // Hour numbers (12, 3, 6, 9)
  tft.setTextColor(TFT_WHITE);
  int numR = r - SCALE_MIN(7);
  tft.drawCentreString("12", cx, cy - numR - 3, 1);
  tft.drawCentreString("6",  cx, cy + numR - 4, 1);
  tft.drawString("3", cx + numR - 3, cy - 3, 1);
  tft.drawString("9", cx - numR - 3, cy - 3, 1);

  // Minute/Second ticks
  for (int i = 0; i < 12; i++) {
    if (i == 0 || i == 3 || i == 6 || i == 9) continue;
    float angle = i * 30.0f * DEG_TO_RAD - M_PI_2;
    int x1 = cx + cos(angle) * (r - 2);
    int y1 = cy + sin(angle) * (r - 2);
    int x2 = cx + cos(angle) * (r - 4);
    int y2 = cy + sin(angle) * (r - 4);
    tft.drawLine(x1, y1, x2, y2, CLK_TEXT_DIM);
  }

  // Calculate hand angles
  float hAngle = ((h % 12) + m / 60.0f) * 30.0f * DEG_TO_RAD - M_PI_2;
  float mAngle = (m + s / 60.0f) * 6.0f * DEG_TO_RAD - M_PI_2;
  float sAngle = s * 6.0f * DEG_TO_RAD - M_PI_2;

  // Hour hand (thick white)
  int hLen = r * 0.52f;
  int hx = cx + cos(hAngle) * hLen;
  int hy = cy + sin(hAngle) * hLen;
  tft.drawLine(cx, cy, hx, hy, TFT_WHITE);
  tft.drawLine(cx + 1, cy, hx + 1, hy, TFT_WHITE);

  // Minute hand (white)
  int mLen = r * 0.75f;
  int mx = cx + cos(mAngle) * mLen;
  int my = cy + sin(mAngle) * mLen;
  tft.drawLine(cx, cy, mx, my, TFT_WHITE);

  // Second hand (vibrant cyan)
  int sLen = r * 0.85f;
  int sx = cx + cos(sAngle) * sLen;
  int sy = cy + sin(sAngle) * sLen;
  tft.drawLine(cx, cy, sx, sy, CLK_CYAN);

  // Center pivot dot
  tft.fillCircle(cx, cy, 3, CLK_PURPLE);
  tft.fillCircle(cx, cy, 1, TFT_WHITE);
}

static void drawEQVisualizer(int x, int y, bool active) {
  for (int i = 0; i < 5; i++) {
    if (active) {
      eqBarHeights[i] = random(2, SCALE_Y(10));
    } else {
      eqBarHeights[i] = 3;
    }
    int barH = eqBarHeights[i];
    int bx = x + i * SCALE_X(3);
    tft.fillRect(bx, y + SCALE_Y(10) - barH, SCALE_MIN(2), barH, CLK_PURPLE);
  }
}

static void drawControlsBar(int cx, int cy, bool playing) {
  // Shuffle
  tft.drawPixel(cx - SCALE_X(28), cy - 2, CLK_CYAN);
  tft.drawLine(cx - SCALE_X(32), cy + 2, cx - SCALE_X(26), cy - 2, CLK_CYAN);
  tft.drawLine(cx - SCALE_X(32), cy - 2, cx - SCALE_X(26), cy + 2, CLK_CYAN);

  // Prev |<
  tft.fillRect(cx - SCALE_X(18), cy - 4, SCALE_MIN(2), 8, TFT_WHITE);
  tft.fillTriangle(cx - SCALE_X(10), cy - 4, cx - SCALE_X(10), cy + 4, cx - SCALE_X(16), cy, TFT_WHITE);

  // Play / Pause Circle Button
  int pR = SCALE_MIN(9);
  tft.fillCircle(cx, cy, pR, CLK_PURPLE);
  tft.drawCircle(cx, cy, pR, CLK_CYAN);
  if (playing) {
    tft.fillRect(cx - 3, cy - 4, 2, 8, TFT_WHITE);
    tft.fillRect(cx + 1, cy - 4, 2, 8, TFT_WHITE);
  } else {
    tft.fillTriangle(cx - 2, cy - 4, cx - 2, cy + 4, cx + 4, cy, TFT_WHITE);
  }

  // Next >|
  tft.fillTriangle(cx + SCALE_X(10), cy - 4, cx + SCALE_X(10), cy + 4, cx + SCALE_X(16), cy, TFT_WHITE);
  tft.fillRect(cx + SCALE_X(16), cy - 4, SCALE_MIN(2), 8, TFT_WHITE);

  // Repeat
  tft.drawCircle(cx + SCALE_X(28), cy, 3, CLK_CYAN);
}

// Dynamic Sizing & Wrapping Text Helper for Spotify & Lyrics
static int drawDynamicWrappedText(int x, int y, const char* text, int maxW, uint16_t color, bool bold) {
  int font = (SCREEN_W > 200 && bold) ? 2 : 1;
  int w = tft.textWidth(text, font);

  if (w <= maxW) {
    tft.setTextColor(color, CLK_CARD_BG);
    tft.drawString(text, x, y, font);
    return tft.fontHeight(font);
  }

  // If larger than maxW, attempt font 1 or 2-line wrap
  if (font > 1 && tft.textWidth(text, 1) <= maxW) {
    tft.setTextColor(color, CLK_CARD_BG);
    tft.drawString(text, x, y, 1);
    return tft.fontHeight(1);
  }

  // Multi-line wrap at font 1
  tft.setTextColor(color, CLK_CARD_BG);
  String str = String(text);
  int splitIdx = str.lastIndexOf(' ', str.length() / 2 + 4);
  if (splitIdx <= 0) splitIdx = str.length() / 2;

  String line1 = str.substring(0, splitIdx);
  String line2 = str.substring(splitIdx);
  line1.trim(); line2.trim();

  drawTruncatedText(x, y, line1.c_str(), maxW, color, 1);
  drawTruncatedText(x, y + 8, line2.c_str(), maxW, color, 1);
  return 16;
}

// ── Full Page Renderer ──

void drawClockDashboardPage() {
  tft.fillScreen(CLK_BG);

  int pad = SCALE_X(4);
  int col1W = SCALE_X(70);
  int col2W = SCR_W - col1W - (pad * 3);

  // ═══════════════════════════════════════════════════════════
  // LEFT COLUMN TOP: Analog & Digital Clock Widget
  // ═══════════════════════════════════════════════════════════
  int c1X = pad;
  int c1Y = pad;
  int c1H = SCALE_Y(86);

  tft.fillRoundRect(c1X, c1Y, col1W, c1H, 4, CLK_CARD_BG);
  tft.drawRoundRect(c1X, c1Y, col1W, c1H, 4, CLK_BORDER);

  // Analog Clock Center
  int clkCX = c1X + col1W / 2;
  int clkCY = c1Y + SCALE_Y(28);
  int clkR = SCALE_MIN(20);
  drawAnalogClock(clkCX, clkCY, clkR, displayHour, displayMinute, displaySecond);

  // Digital Time
  char timeBuf[12];
  int hr12 = displayHour % 12;
  if (hr12 == 0) hr12 = 12;
  sprintf(timeBuf, "%02d:%02d", hr12, displayMinute);
  int digY = c1Y + SCALE_Y(52);
  tft.setTextColor(CLK_CYAN);
  tft.drawString(timeBuf, c1X + SCALE_X(4), digY, (SCREEN_W > 200) ? 4 : 2);
  tft.setTextColor(CLK_PURPLE);
  tft.drawString(displayHour >= 12 ? "PM" : "AM", c1X + col1W - SCALE_X(16), digY + SCALE_Y(2), 1);

  // Dynamic Date String
  tft.setTextColor(TFT_WHITE);
  tft.drawCentreString("08 AUG 2026 | FRI", clkCX, c1Y + c1H - SCALE_Y(9), 1);

  // ═══════════════════════════════════════════════════════════
  // LEFT COLUMN BOTTOM: Wi-Fi Status Card (Dynamic Wrapped Text)
  // ═══════════════════════════════════════════════════════════
  int wY = c1Y + c1H + pad;
  int wH = SCR_H - wY - pad;

  tft.fillRoundRect(c1X, wY, col1W, wH, 4, CLK_CARD_BG);
  tft.drawRoundRect(c1X, wY, col1W, wH, 4, CLK_BORDER);

  // Wi-Fi Icon & Title
  tft.fillCircle(c1X + SCALE_X(6), wY + SCALE_Y(7), 3, CLK_CYAN);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("Wi-Fi", c1X + SCALE_X(12), wY + SCALE_Y(3), 1);

  // SSID with dynamic text wrapping / bounds protection
  String rawSSID = wifiConnected ? ((WiFi.SSID().length() > 0) ? WiFi.SSID() : "Companion_5G") : "Offline";
  drawDynamicWrappedText(c1X + SCALE_X(4), wY + SCALE_Y(14), rawSSID.c_str(), col1W - SCALE_X(8),
                         wifiConnected ? CLK_CYAN : CLK_TEXT_DIM, false);

  // ═══════════════════════════════════════════════════════════
  // RIGHT COLUMN: Expanded Direct Spotify & Live Lyrics Card
  // ═══════════════════════════════════════════════════════════
  int c2X = c1X + col1W + pad;
  int c2Y = pad;
  int c2H = SCR_H - (pad * 2);

  tft.fillRoundRect(c2X, c2Y, col2W, c2H, 4, CLK_CARD_BG);
  tft.drawRoundRect(c2X, c2Y, col2W, c2H, 4, CLK_BORDER);

  // Header: NOW PLAYING + Visualizer
  tft.setTextColor(CLK_PURPLE);
  tft.drawString("NOW PLAYING", c2X + SCALE_X(4), c2Y + SCALE_Y(3), 1);
  drawEQVisualizer(c2X + col2W - SCALE_X(18), c2Y + SCALE_Y(3), isPlaying);

  // Direct Spotify Album Art Thumbnail
  int artX = c2X + SCALE_X(4);
  int artY = c2Y + SCALE_Y(14);
  int artS = SCALE_MIN(28);

  if (albumArtReady) {
    tft.setSwapBytes(true);
    tft.pushImage(artX, artY, 64, 64, albumArt); // Direct memory unpack
    tft.setSwapBytes(false);
  } else {
    // Spotify Vinyl Placeholder
    tft.fillRoundRect(artX, artY, artS, artS, 3, 0x18C3);
    tft.drawCircle(artX + artS/2, artY + artS/2, artS/3, CLK_CYAN);
    tft.fillCircle(artX + artS/2, artY + artS/2, 2, TFT_WHITE);
  }

  // Direct Spotify Track Title & Artist (Dynamic Sizing & Bounded Wrapping)
  int infoX = artX + artS + SCALE_X(4);
  int infoW = col2W - artS - SCALE_X(12);

  String spotTrack = (currentTrack.length() > 0) ? currentTrack : "Spotify Idle";
  String spotArtist = (currentArtist.length() > 0) ? currentArtist : "Connect PC";

  drawDynamicWrappedText(infoX, artY, spotTrack.c_str(), infoW, TFT_WHITE, true);
  drawDynamicWrappedText(infoX, artY + SCALE_Y(9), spotArtist.c_str(), infoW, CLK_TEXT_DIM, false);

  // Quality Tag Pill: Spotify
  int pillY = artY + SCALE_Y(18);
  tft.fillRoundRect(infoX, pillY, SCALE_X(36), SCALE_Y(8), 2, 0x1848);
  tft.setTextColor(CLK_CYAN);
  tft.drawString("Spotify", infoX + SCALE_X(2), pillY + 1, 1);

  // Live Progress Bar & Duration
  int prgY = c2Y + SCALE_Y(46);
  int prgX = c2X + SCALE_X(18);
  int prgW = col2W - SCALE_X(36);

  char curTimeStr[8], totTimeStr[8];
  sprintf(curTimeStr, "%02d:%02d", playProgress / 60, playProgress % 60);
  sprintf(totTimeStr, "%02d:%02d", playDuration / 60, playDuration % 60);

  tft.setTextColor(CLK_TEXT_DIM);
  tft.drawString(curTimeStr, c2X + SCALE_X(2), prgY - 2, 1);
  tft.drawString(totTimeStr, c2X + col2W - SCALE_X(16), prgY - 2, 1);

  tft.drawFastHLine(prgX, prgY + 2, prgW, CLK_BORDER);
  int fillW = (prgW * playProgress) / max(1, playDuration);
  tft.drawFastHLine(prgX, prgY + 2, fillW, CLK_CYAN);
  tft.fillCircle(prgX + fillW, prgY + 2, 2, CLK_PURPLE);

  // Playback Controls
  drawControlsBar(c2X + col2W / 2, c2Y + SCALE_Y(58), isPlaying);

  // ═══════════════════════════════════════════════════════════
  // EXPANDED BOTTOM SECTION: Dynamic Lyrics Sizing & Wrapping
  // ═══════════════════════════════════════════════════════════
  int lyrY = c2Y + SCALE_Y(66);
  tft.drawFastHLine(c2X + SCALE_X(2), lyrY, col2W - SCALE_X(4), CLK_BORDER);

  tft.setTextColor(CLK_CYAN);
  tft.drawString("LYRICS", c2X + SCALE_X(4), lyrY + SCALE_Y(3), 1);

  int lLineY = lyrY + SCALE_Y(14);
  int maxLyrW = col2W - SCALE_X(8);

  if (prevLyricsLine.length() > 0) {
    int h1 = drawDynamicWrappedText(c2X + SCALE_X(4), lLineY, prevLyricsLine.c_str(), maxLyrW, CLK_TEXT_DIM, false);
    lLineY += max(SCALE_Y(9), h1);
  }

  String activeLyr = (currentLyrics.length() > 0) ? currentLyrics : "Vibing with CompanionOS";
  int h2 = drawDynamicWrappedText(c2X + SCALE_X(4), lLineY, activeLyr.c_str(), maxLyrW, TFT_WHITE, true);
  lLineY += max(SCALE_Y(9), h2);

  if (currentLyricsLine2.length() > 0 && lLineY < c2Y + c2H - SCALE_Y(8)) {
    drawDynamicWrappedText(c2X + SCALE_X(4), lLineY, currentLyricsLine2.c_str(), maxLyrW, CLK_TEXT_DIM, false);
  }

  drawPageIndicator(STATE_CLOCK_DASHBOARD, STATE_COUNT);
  clockDashDrawn = true;
  lastClockSec = displaySecond;
  lastClockMin = displayMinute;
  lastClockHour = displayHour;
  lastClockTrack = currentTrack;
}

void redrawClockDashboardPartial() {
  if (currentState != STATE_CLOCK_DASHBOARD) return;

  if (displaySecond != lastClockSec || currentTrack != lastClockTrack) {
    drawClockDashboardPage(); // Full clean refresh on clock tick / track change
  }
}

#endif // PAGE_CLOCK_DASHBOARD_H
