#ifndef UI_H
#define UI_H

#include "globals.h"

extern int displayHour;
extern int displayMinute;
extern bool timeReceived;

// ═══════════════════════════════════════════════════════════
// UI COMPONENTS - V3 (No orange bar, thin status line)
// + V4 ADDITIONS: Loading screen, agent overlay
// ═══════════════════════════════════════════════════════════

void drawButton(int x, int y, int w, int h, const char* label, uint16_t color) {
  tft.fillRoundRect(x, y, w, h, 5, color);
  tft.setTextColor(TFT_WHITE);
  tft.drawCentreString(label, x + (w/2), y + (h/2) - 8, 2);
}

// ── V4: Loading Screen for Mode Switching ─────────────────
void showLoadingScreen(const char* message) {
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(TFT_CYAN);
  tft.drawCentreString(message, SCREEN_W/2, SCREEN_H/2 - 20, 2);
  tft.drawRect(SCREEN_W/2 - 60, SCREEN_H/2 + 10, 120, 8, 0x4208);
  for (int i = 0; i < 116; i += 4) {
    tft.fillRect(SCREEN_W/2 - 58 + i, SCREEN_H/2 + 12, 4, 4, TFT_CYAN);
    delay(15);
  }
  delay(200);
}

// Minimal status bar: tiny icons at top-right, no colored background
void drawStatusBar() {
  tft.fillRect(0, 0, SCREEN_W, 15, COLOR_BG);
  
  // Persistent Clock on left
  if (timeReceived) {
    char timeStr[6];
    sprintf(timeStr, "%02d:%02d", displayHour, displayMinute);
    tft.setTextColor(TFT_WHITE, COLOR_BG);
    tft.drawString(timeStr, 8, 2, 1);
  }

  // V4: Song name on eyes page (right of clock)
  if (currentState == STATE_EYES && musicPlaying) {
    extern String currentTrack;
    String songSnippet = currentTrack.substring(0, 18);
    tft.setTextColor(0x6B4D, COLOR_BG);
    tft.drawString(songSnippet, 50, 2, 1);
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
    tft.drawFastHLine(x - 7, y + 4, 5, TFT_CYAN);
    tft.drawPixel(x - 8, y + 5, TFT_CYAN); tft.drawPixel(x + 2, y + 5, TFT_CYAN);
    tft.drawFastHLine(x - 10, y, 11, TFT_CYAN);
    tft.drawPixel(x - 11, y + 1, TFT_CYAN); tft.drawPixel(x + 1, y + 1, TFT_CYAN);
    tft.drawFastHLine(x - 13, y - 4, 17, TFT_CYAN);
    tft.drawPixel(x - 14, y - 3, TFT_CYAN); tft.drawPixel(x + 4, y - 3, TFT_CYAN);
  } else {
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

// ── V4: Agent Status Overlay ──────────────────────────────
void drawAgentOverlay() {
  if (!agentOverlayActive) return;
  
  extern uint16_t blendColor(uint16_t c1, uint16_t c2, float t);
  
  uint16_t bgColor = 0x1082;
  uint16_t accentColor = TFT_CYAN;
  if (agentStatus == "error") {
    accentColor = TFT_RED;
  } else if (agentStatus == "done") {
    accentColor = TFT_GREEN;
  }
  
  tft.fillRoundRect(8, SCREEN_H - 55, SCREEN_W - 16, 42, 6, bgColor);
  tft.fillRect(8, SCREEN_H - 55, 4, 42, accentColor);
  
  if (agentStatus == "thinking") {
    float pulse = (sin(millis() * 0.005) + 1.0) * 0.5;
    uint16_t dotColor = blendColor(0x4208, accentColor, pulse);
    tft.fillCircle(20, SCREEN_H - 34, 3, dotColor);
  } else {
    tft.fillCircle(20, SCREEN_H - 34, 3, accentColor);
  }
  
  tft.setTextColor(TFT_WHITE);
  String displayText = agentStatusText.substring(0, 32);
  tft.drawString(displayText, 28, SCREEN_H - 42, 1);
  if (agentStatusText.length() > 32) {
    tft.drawString(agentStatusText.substring(32, 64), 28, SCREEN_H - 30, 1);
  }
  
  if (agentStatus == "done" && millis() - agentStatusStart > 5000) {
    agentOverlayActive = false;
  }
  if (agentStatus == "error" && millis() - agentStatusStart > 8000) {
    agentOverlayActive = false;
  }
}

#endif
