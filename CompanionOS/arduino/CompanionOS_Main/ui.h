#ifndef UI_H
#define UI_H

#include "globals.h"

// ═══════════════════════════════════════════════════════════
// UI COMPONENTS
// ═══════════════════════════════════════════════════════════

void drawButton(int x, int y, int w, int h, const char* label, uint16_t color) {
  tft.fillRoundRect(x, y, w, h, 5, color);
  tft.setTextColor(TFT_WHITE);
  tft.drawCentreString(label, x + (w/2), y + (h/2) - 8, 2);
}

void drawTopBar(const char* title) {
  tft.fillRect(0, 0, SCREEN_W, 25, COLOR_ACCENT);
  tft.setTextColor(TFT_WHITE);
  tft.drawCentreString(title, SCREEN_W/2, 5, 2);
}

void drawPageIndicator(int current, int total) {
  int spacing = 15;
  int startX = (SCREEN_W - ((total - 1) * spacing)) / 2;
  int y = 5; // In the top bar

  for (int i = 0; i < total; i++) {
    if (i == current) {
      tft.fillCircle(startX + (i * spacing), y + 7, 3, TFT_WHITE);
    } else {
      tft.drawCircle(startX + (i * spacing), y + 7, 3, TFT_LIGHTGREY);
    }
  }
}

#endif
