#ifndef PAGE_CLOCK_DASHBOARD_H
#define PAGE_CLOCK_DASHBOARD_H

#include "globals.h"
#include "ui_components.h"
#include <math.h>

// ═══════════════════════════════════════════════════════════
// CLOCK & SPOTIFY / LYRICS DASHBOARD (Minimal & Sleek)
// ═══════════════════════════════════════════════════════════

#define CLK_BG        0x0841  // Deep dark blue-black
#define CLK_CARD_BG   0x1082  // Dark surface
#define CLK_BORDER    0x2104  // Subtle border
#define CLK_CYAN      0x07FF  // Vibrant cyan
#define CLK_PURPLE    0x981F  // Deep purple
#define CLK_TEXT_DIM  0x8410  // Dim text

static int lastClockSec = -1;
static int lastClockMin = -1;
static int lastClockHour = -1;
static int lastClockProgSec = -1;
static String lastClockTrack = "";
static String lastClockLyric = "";
static bool clockDashDrawn = false;

void resetClockDashboardDrawState() {
  lastClockSec = -1;
  lastClockMin = -1;
  lastClockHour = -1;
  lastClockProgSec = -1;
  lastClockTrack = "";
  lastClockLyric = "";
  clockDashDrawn = false;
}

static void drawEQVisualizer(int x, int y, bool active) {
  for (int i = 0; i < 4; i++) {
    int barH = active ? random(2, 9) : 3;
    int bx = x + i * 4;
    tft.fillRect(bx, y + 8 - barH, 2, barH, CLK_PURPLE);
    tft.fillRect(bx, y, 2, 8 - barH, CLK_CARD_BG);
  }
}

// ── Full Page Renderer ──

void drawClockDashboardPage() {
  tft.fillScreen(CLK_BG);

  int pad = SCALE_X(3);
  int col1W = SCALE_X(70);
  int col2W = SCR_W - col1W - (pad * 3);

  // ═══════════════════════════════════════════════════════════
  // LEFT COLUMN: Clean Minimalist Digital Clock Card
  // ═══════════════════════════════════════════════════════════
  int c1X = pad;
  int c1Y = pad;
  int c1H = SCR_H - (pad * 2);

  tft.fillRoundRect(c1X, c1Y, col1W, c1H, 4, CLK_CARD_BG);
  tft.drawRoundRect(c1X, c1Y, col1W, c1H, 4, CLK_BORDER);

  // Digital Time Display
  char timeBuf[12];
  int hr12 = displayHour % 12;
  if (hr12 == 0) hr12 = 12;
  sprintf(timeBuf, "%02d:%02d", hr12, displayMinute);
  
  tft.setTextColor(CLK_CYAN, CLK_CARD_BG);
  tft.drawCentreString(timeBuf, c1X + col1W / 2, c1Y + SCALE_Y(14), 4);

  // AM/PM & Seconds indicator
  char secBuf[16];
  sprintf(secBuf, "%s • :%02d", (displayHour >= 12 ? "PM" : "AM"), displaySecond);
  tft.setTextColor(CLK_PURPLE, CLK_CARD_BG);
  tft.drawCentreString(secBuf, c1X + col1W / 2, c1Y + SCALE_Y(44), 1);

  // Divider
  tft.drawFastHLine(c1X + SCALE_X(6), c1Y + SCALE_Y(60), col1W - SCALE_X(12), CLK_BORDER);

  // Wi-Fi & Device Status
  extern String currentWiFiSSID;
  tft.fillCircle(c1X + SCALE_X(10), c1Y + SCALE_Y(76), 2, wifiConnected ? CLK_CYAN : CLK_TEXT_DIM);
  tft.setTextColor(TFT_WHITE, CLK_CARD_BG);
  tft.drawString(wifiConnected ? "CONNECTED" : "OFFLINE", c1X + SCALE_X(16), c1Y + SCALE_Y(72), 1);

  String rawSSID = wifiConnected ? ((currentWiFiSSID.length() > 0) ? currentWiFiSSID : "Online") : "BT Ready";
  drawTruncatedText(c1X + SCALE_X(8), c1Y + SCALE_Y(86), rawSSID.c_str(), col1W - SCALE_X(16), CLK_TEXT_DIM, 1);

  // ═══════════════════════════════════════════════════════════
  // RIGHT COLUMN: Media & Live Lyrics Card
  // ═══════════════════════════════════════════════════════════
  int c2X = c1X + col1W + pad;
  int c2Y = pad;
  int c2H = SCR_H - (pad * 2);

  tft.fillRoundRect(c2X, c2Y, col2W, c2H, 4, CLK_CARD_BG);
  tft.drawRoundRect(c2X, c2Y, col2W, c2H, 4, CLK_BORDER);

  // Header: NOW PLAYING + Visualizer
  tft.setTextColor(CLK_PURPLE, CLK_CARD_BG);
  tft.drawString("NOW PLAYING", c2X + SCALE_X(6), c2Y + SCALE_Y(5), 1);
  drawEQVisualizer(c2X + col2W - SCALE_X(20), c2Y + SCALE_Y(5), musicPlaying);

  // Track Title & Artist
  int infoX = c2X + SCALE_X(6);
  int infoY = c2Y + SCALE_Y(18);
  int infoW = col2W - SCALE_X(12);

  String spotTrack = (currentTrack.length() > 0) ? currentTrack : "No Active Media";
  String spotArtist = (currentArtist.length() > 0) ? currentArtist : "CompanionOS";

  tft.setTextColor(TFT_WHITE, CLK_CARD_BG);
  drawTruncatedText(infoX, infoY, spotTrack.c_str(), infoW, TFT_WHITE, 1);
  tft.setTextColor(CLK_TEXT_DIM, CLK_CARD_BG);
  drawTruncatedText(infoX, infoY + SCALE_Y(11), spotArtist.c_str(), infoW, CLK_TEXT_DIM, 1);

  // Live Progress Bar & Duration
  int prgY = infoY + SCALE_Y(26);
  int prgX = c2X + SCALE_X(6);
  int prgW = col2W - SCALE_X(12);

  int curSec = playProgress / 1000;
  int totSec = playDuration / 1000;
  char timeProgressStr[16];
  sprintf(timeProgressStr, "%d:%02d / %d:%02d", curSec / 60, curSec % 60, totSec / 60, totSec % 60);

  tft.setTextColor(CLK_TEXT_DIM, CLK_CARD_BG);
  tft.drawString(timeProgressStr, prgX, prgY - SCALE_Y(9), 1);

  tft.drawFastHLine(prgX, prgY, prgW, CLK_BORDER);
  int fillW = (totSec > 0) ? (prgW * curSec) / max(1, totSec) : 0;
  fillW = constrain(fillW, 0, prgW);
  if (fillW > 0) {
    tft.drawFastHLine(prgX, prgY, fillW, CLK_CYAN);
  }

  // ═══════════════════════════════════════════════════════════
  // BOTTOM SECTION: Live Synced Lyrics
  // ═══════════════════════════════════════════════════════════
  int lyrY = prgY + SCALE_Y(8);
  tft.drawFastHLine(c2X + SCALE_X(4), lyrY, col2W - SCALE_X(8), CLK_BORDER);

  tft.setTextColor(CLK_CYAN, CLK_CARD_BG);
  tft.drawString("LYRICS", c2X + SCALE_X(6), lyrY + SCALE_Y(3), 1);

  int lLineY = lyrY + SCALE_Y(14);
  int maxLyrW = col2W - SCALE_X(12);

  String activeLyr = (currentLyrics.length() > 0) ? currentLyrics : "♪ Listening with CompanionOS";
  tft.setTextColor(TFT_WHITE, CLK_CARD_BG);
  drawTruncatedText(c2X + SCALE_X(6), lLineY, activeLyr.c_str(), maxLyrW, TFT_WHITE, 1);

  if (currentLyricsLine2.length() > 0 && lLineY + SCALE_Y(11) < c2Y + c2H - SCALE_Y(4)) {
    tft.setTextColor(CLK_TEXT_DIM, CLK_CARD_BG);
    drawTruncatedText(c2X + SCALE_X(6), lLineY + SCALE_Y(11), currentLyricsLine2.c_str(), maxLyrW, CLK_TEXT_DIM, 1);
  }

  drawPageIndicator(STATE_CLOCK_DASHBOARD, STATE_COUNT);
  clockDashDrawn = true;
  lastClockSec = displaySecond;
  lastClockMin = displayMinute;
  lastClockHour = displayHour;
  lastClockProgSec = curSec;
  lastClockTrack = currentTrack;
  lastClockLyric = currentLyrics;
}

// ── Flicker-Free Partial Refresh ──

void redrawClockDashboardPartial() {
  if (currentState != STATE_CLOCK_DASHBOARD) return;

  if (!clockDashDrawn || currentTrack != lastClockTrack || currentLyrics != lastClockLyric) {
    drawClockDashboardPage();
    return;
  }

  if (displaySecond != lastClockSec || displayMinute != lastClockMin) {
    int pad = SCALE_X(3);
    int col1W = SCALE_X(70);
    int c1X = pad;
    int c1Y = pad;

    // Repaint digital time text
    char timeBuf[12];
    int hr12 = displayHour % 12;
    if (hr12 == 0) hr12 = 12;
    sprintf(timeBuf, "%02d:%02d", hr12, displayMinute);
    tft.setTextColor(CLK_CYAN, CLK_CARD_BG);
    tft.drawCentreString(timeBuf, c1X + col1W / 2, c1Y + SCALE_Y(14), 4);

    // Repaint seconds
    char secBuf[16];
    sprintf(secBuf, "%s • :%02d", (displayHour >= 12 ? "PM" : "AM"), displaySecond);
    tft.setTextColor(CLK_PURPLE, CLK_CARD_BG);
    tft.drawCentreString(secBuf, c1X + col1W / 2, c1Y + SCALE_Y(44), 1);

    // Update Spotify progress bar if playing
    int curSec = playProgress / 1000;
    if (curSec != lastClockProgSec) {
      int col2W = SCR_W - col1W - (pad * 3);
      int c2X = c1X + col1W + pad;
      int c2Y = pad;
      int infoY = c2Y + SCALE_Y(18);
      int prgY = infoY + SCALE_Y(26);
      int prgX = c2X + SCALE_X(6);
      int prgW = col2W - SCALE_X(12);

      int totSec = playDuration / 1000;
      char timeProgressStr[16];
      sprintf(timeProgressStr, "%d:%02d / %d:%02d", curSec / 60, curSec % 60, totSec / 60, totSec % 60);

      tft.fillRect(prgX, prgY - SCALE_Y(9), prgW, 8, CLK_CARD_BG);
      tft.setTextColor(CLK_TEXT_DIM, CLK_CARD_BG);
      tft.drawString(timeProgressStr, prgX, prgY - SCALE_Y(9), 1);

      tft.drawFastHLine(prgX, prgY, prgW, CLK_BORDER);
      int fillW = (totSec > 0) ? (prgW * curSec) / max(1, totSec) : 0;
      fillW = constrain(fillW, 0, prgW);
      if (fillW > 0) {
        tft.drawFastHLine(prgX, prgY, fillW, CLK_CYAN);
      }
      lastClockProgSec = curSec;
    }

    lastClockSec = displaySecond;
    lastClockMin = displayMinute;
    lastClockHour = displayHour;
  }
}

#endif // PAGE_CLOCK_DASHBOARD_H
