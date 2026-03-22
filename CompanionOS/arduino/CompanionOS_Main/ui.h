#ifndef UI_H
#define UI_H

#include "globals.h"

extern int displayHour;
extern int displayMinute;
extern bool timeReceived;

// ═══════════════════════════════════════════════════════════
// UI COMPONENTS - V3 (No orange bar, thin status line)
// ═══════════════════════════════════════════════════════════

void drawButton(int x, int y, int w, int h, const char* label, uint16_t color) {
  tft.fillRoundRect(x, y, w, h, 5, color);
  tft.setTextColor(TFT_WHITE);
  tft.drawCentreString(label, x + (w/2), y + (h/2) - 8, 2);
}

// Minimal status bar: tiny icons at top-right, no colored background
void drawStatusBar() {
  // Clear status bar area (top 15px)
  tft.fillRect(0, 0, SCREEN_W, 15, COLOR_BG);
  
  // Persistent Clock on left
  if (timeReceived) {
    char timeStr[6];
    sprintf(timeStr, "%02d:%02d", displayHour, displayMinute);
    tft.setTextColor(TFT_WHITE, COLOR_BG);
    tft.drawString(timeStr, 8, 2, 1);
  }
  
  int x = SCREEN_W - 10;
  int y = 5;
  
  // Notification dot (leftmost)
  if (notifCount > 0) {
    tft.fillCircle(x - 55, y, 3, TFT_MAGENTA);
    tft.setTextColor(TFT_WHITE, COLOR_BG);
    char nb[4];
    sprintf(nb, "%d", min(notifCount, 9));
    tft.drawString(nb, x - 49, y - 4, 1);
  }
  
  // Music icon (middle)
  if (musicPlaying) {
    tft.drawFastVLine(x - 32, y - 3, 6, TFT_GREEN);
    tft.drawFastVLine(x - 29, y - 4, 7, TFT_GREEN);
    tft.fillCircle(x - 32, y + 3, 1, TFT_GREEN);
    tft.fillCircle(x - 29, y + 3, 1, TFT_GREEN);
  }
  
  // WiFi icon (premium 3-arc style)
  if (wifiConnected) {
    tft.fillCircle(x - 5, y + 8, 2, TFT_CYAN);
    // Arc 1
    tft.drawFastHLine(x - 7, y + 4, 5, TFT_CYAN);
    tft.drawPixel(x - 8, y + 5, TFT_CYAN); tft.drawPixel(x + 2, y + 5, TFT_CYAN);
    // Arc 2
    tft.drawFastHLine(x - 10, y, 11, TFT_CYAN);
    tft.drawPixel(x - 11, y + 1, TFT_CYAN); tft.drawPixel(x + 1, y + 1, TFT_CYAN);
    // Arc 3
    tft.drawFastHLine(x - 13, y - 4, 17, TFT_CYAN);
    tft.drawPixel(x - 14, y - 3, TFT_CYAN); tft.drawPixel(x + 4, y - 3, TFT_CYAN);
  } else {
    // Red dot + X
    tft.fillCircle(x - 5, y + 8, 2, TFT_RED);
    tft.drawLine(x - 8, y, x - 2, y + 6, TFT_RED);
    tft.drawLine(x - 2, y, x - 8, y + 6, TFT_RED);
  }
}

// Page dots - bottom center, thin
void drawPageIndicator(int current, int total) {
  int spacing = 12;
  int startX = (SCREEN_W - ((total - 1) * spacing)) / 2;
  int y = SCREEN_H - 8;

  for (int i = 0; i < total; i++) {
    if (i == current) {
      tft.fillCircle(startX + (i * spacing), y, 3, TFT_WHITE);
    } else {
      tft.fillCircle(startX + (i * spacing), y, 2, 0x4208);
    }
  }
}

// Thin header for content pages (no orange, just text)
void drawPageHeader(const char* title) {
  tft.fillRect(0, 0, SCREEN_W - 80, 16, COLOR_BG);
  tft.setTextColor(0x8410);  // Dim grey
  tft.drawString(title, 8, 2, 2);
}

#endif
