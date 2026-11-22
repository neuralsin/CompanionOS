#ifndef PAGE_CLOCK_DASHBOARD_H
#define PAGE_CLOCK_DASHBOARD_H

#include "globals.h"
#include "ui_components.h"

extern String currentTrack;
extern String currentArtist;
extern int playProgress;
extern int playDuration;
extern bool isPlaying;
extern String currentWiFiSSID;
extern void drawPageIndicator(int current, int total);

#define CLK_BG        0x0841
#define CLK_CARD_BG   0x1082
#define CLK_BORDER    0x2104
#define CLK_CYAN      0x07FF
#define CLK_PURPLE    0x981F
#define CLK_TEXT_DIM  0x8410

static bool clockDashDrawn = false;
static String clkLastContent = "";
static int clkLastSecond = -1, clkLastMinute = -1, clkLastHour = -1;
static int clkLastProgress = -1, clkLastDuration = -1;

void resetClockDashboardDrawState() {
  clockDashDrawn = false;
  clkLastContent = "";
}

static String clk_contentKey() {
  return String(uiPageGeneration) + "\n" + currentTrack + "\n" + currentArtist + "\n" +
         currentLyrics + "\n" + currentLyricsLine2 + "\n" + currentWiFiSSID + "/" +
         String(timeReceived) + "/" + String(isPlaying) + "/" + String(wifiConnected);
}

static void clk_drawTime() {
  const bool compact = SCREEN_W <= 200;
  tft.fillRect(4, UI_HEADER_H + 14, compact ? 84 : 96, compact ? 16 : 26, CLK_BG);
  const int secondsX = compact ? 94 : 8;
  const int secondsY = compact ? UI_HEADER_H + 20 : UI_HEADER_H + 48;
  const int secondsW = compact ? SCREEN_W - 98 : 88;
  tft.fillRect(secondsX, secondsY, secondsW, tft.fontHeight(1), CLK_BG);
  char value[16];
  int hr12 = displayHour % 12;
  if (hr12 == 0) hr12 = 12;
  if (timeReceived) snprintf(value, sizeof(value), "%02d:%02d", hr12, displayMinute);
  else snprintf(value, sizeof(value), "--:--");
  tft.setTextColor(CLK_CYAN, CLK_BG);
  tft.drawCentreString(value, compact ? 44 : 52, UI_HEADER_H + 14, compact ? 2 : 4);
  if (timeReceived) snprintf(value, sizeof(value), "%s :%02d", displayHour >= 12 ? "PM" : "AM", displaySecond);
  else snprintf(value, sizeof(value), "NO SYNC");
  drawTruncatedText(secondsX, secondsY, value, secondsW, CLK_TEXT_DIM, 1, CLK_BG);
  clkLastSecond = displaySecond;
  clkLastMinute = displayMinute;
  clkLastHour = displayHour;
}

static void clk_drawProgress() {
  const bool compact = SCREEN_W <= 200;
  const int x = compact ? 4 : 108;
  const int w = SCREEN_W - x - 4;
  const int lineH = tft.fontHeight(1) + 2;
  const int y = (compact ? UI_HEADER_H + 32 : UI_HEADER_H + 16) + 3 * lineH;
  tft.fillRect(x, y, w, lineH + 2, CLK_BG);
  char value[32];
  const int total = max(0, playDuration / 1000);
  const int elapsed = total > 0 ? constrain(playProgress / 1000, 0, total) : 0;
  if (currentTrack.length() && total > 0) {
    snprintf(value, sizeof(value), "%d:%02d / %d:%02d", elapsed / 60, elapsed % 60, total / 60, total % 60);
  } else {
    snprintf(value, sizeof(value), "--:-- / --:--");
  }
  drawTruncatedText(x, y, value, w, CLK_TEXT_DIM, 1, CLK_BG);
  tft.fillRect(x, y + lineH, w, 2, CLK_BORDER);
  if (currentTrack.length() && total > 0) {
    tft.fillRect(x, y + lineH, (int)((long)w * elapsed / total), 2, CLK_CYAN);
  }
  clkLastProgress = playProgress / 1000;
  clkLastDuration = playDuration;
}

static void clk_drawContent() {
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  const bool compact = SCREEN_W <= 200;
  const int lineH = tft.fontHeight(1) + 2;
  const int bottom = SCREEN_H - UI_FOOTER_H - 2;
  const int clockW = compact ? SCREEN_W - 8 : 96;
  const int mediaX = compact ? 4 : 108;
  const int mediaW = SCREEN_W - mediaX - 4;
  const int mediaY = compact ? UI_HEADER_H + 32 : UI_HEADER_H + 16;

  tft.fillRect(0, UI_HEADER_H, SCREEN_W, SCREEN_H - UI_HEADER_H - UI_FOOTER_H, CLK_BG);
  drawTruncatedText(4, UI_HEADER_H + 2, "CLOCK", SCREEN_W / 2 - 8, CLK_PURPLE, 1, CLK_BG);
  const char* connection = wifiConnected ? "WIFI ON" : "WIFI OFF";
  drawTruncatedText(SCREEN_W - 52, UI_HEADER_H + 2, connection, 48, CLK_TEXT_DIM, 1, CLK_BG);

  clk_drawTime();
  if (!compact) {
    tft.drawFastVLine(104, UI_HEADER_H + 2, bottom - UI_HEADER_H - 2, CLK_BORDER);
    drawWrappedText(8, UI_HEADER_H + 68,
                    wifiConnected ? currentWiFiSSID.c_str() : "WiFi disconnected",
                    clockW - 8, bottom - UI_HEADER_H - 68, CLK_TEXT_DIM, 1, CLK_BG);
  }

  int y = mediaY;
  const bool hasTrack = currentTrack.length() > 0;
  drawTruncatedText(mediaX, y, hasTrack ? (isPlaying ? "PLAYING" : "PAUSED") : "NO MEDIA",
                    mediaW, CLK_PURPLE, 1, CLK_BG);
  // A play/pause symbol reports playback state, not fabricated audio levels.
  if (hasTrack) {
    int x = mediaX + mediaW - 10;
    if (isPlaying) tft.fillTriangle(x, y, x, y + 6, x + 6, y + 3, CLK_CYAN);
    else {
      tft.fillRect(x, y, 2, 7, CLK_TEXT_DIM);
      tft.fillRect(x + 4, y, 2, 7, CLK_TEXT_DIM);
    }
  }
  y += lineH;
  drawTruncatedText(mediaX, y, hasTrack ? currentTrack.c_str() : "No active media",
                    mediaW, TFT_WHITE, 1, CLK_BG);
  y += lineH;
  drawTruncatedText(mediaX, y, currentArtist.length() ? currentArtist.c_str() : "Artist unavailable",
                    mediaW, CLK_TEXT_DIM, 1, CLK_BG);
  y += lineH;

  clk_drawProgress();
  y += lineH + 6;
  if (currentLyrics.length() == 0) {
    drawWrappedText(mediaX, y, "No lyrics available", mediaW, bottom - y, CLK_TEXT_DIM, 1, CLK_BG);
  } else if (currentLyricsLine2.length() > 0 && bottom - y >= lineH + tft.fontHeight(1)) {
    drawTruncatedText(mediaX, y, currentLyrics.c_str(), mediaW, TFT_WHITE, 1, CLK_BG);
    drawWrappedText(mediaX, y + lineH, currentLyricsLine2.c_str(), mediaW,
                    bottom - y - lineH, CLK_TEXT_DIM, 1, CLK_BG);
  } else {
    drawWrappedText(mediaX, y, currentLyrics.c_str(), mediaW, bottom - y, TFT_WHITE, 1, CLK_BG);
  }
}

void drawClockDashboardPage() {
  tft.fillRect(0, UI_HEADER_H, SCREEN_W, SCREEN_H - UI_HEADER_H, CLK_BG);
  clk_drawContent();
  drawPageIndicator(STATE_CLOCK_DASHBOARD, STATE_COUNT);
  clockDashDrawn = true;
  clkLastContent = clk_contentKey();
}

void redrawClockDashboardPartial() {
  if (currentState != STATE_CLOCK_DASHBOARD) return;
  String key = clk_contentKey();
  if (!clockDashDrawn) {
    drawClockDashboardPage();
  } else if (key != clkLastContent) {
    drawClockDashboardPage();
  } else {
    tft.setTextSize(1);
    tft.setTextDatum(TL_DATUM);
    if (displaySecond != clkLastSecond || displayMinute != clkLastMinute || displayHour != clkLastHour) {
      clk_drawTime();
    }
    if (playProgress / 1000 != clkLastProgress || playDuration != clkLastDuration) {
      clk_drawProgress();
    }
  }
}

#endif
