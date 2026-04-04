#ifndef PAGE_GAMING_H
#define PAGE_GAMING_H
#include "globals.h"

extern uint16_t blendColor(uint16_t c1, uint16_t c2, float t);
extern void drawPageIndicator(int current, int total);

// ═══════════════════════════════════════════════════════════
// GAMING DASHBOARD — V6 Premium Steam-Inspired Page
// ═══════════════════════════════════════════════════════════

#define GAM_BG       0x0841
#define GAM_CARD     0x1082
#define GAM_BORDER   0x18E3
#define GAM_CYAN     0x07DF
#define GAM_GREEN    0x2E8B
#define GAM_DIM      0x4A49
#define GAM_BAR_BG   0x2104
#define GAM_PURPLE   0x79FD

static bool gamingPageDrawn = false;
static char lastSessionTime[12] = "";

void resetGamingDrawState() {
  gamingPageDrawn = false;
  lastSessionTime[0] = 0;
}

void drawAchievementBar(int x, int y, int w, int h, uint8_t pct) {
  tft.fillRoundRect(x, y, w, h, h/2, GAM_BAR_BG);
  int fillW = (int)((long)pct * w / 100);
  if (fillW > 0) {
    for (int i = 0; i < fillW; i++) {
      float t = (float)i / max(1, fillW);
      uint16_t r1 = (GAM_PURPLE >> 11) & 0x1F, g1 = (GAM_PURPLE >> 5) & 0x3F, b1 = GAM_PURPLE & 0x1F;
      uint16_t r2 = (GAM_CYAN >> 11) & 0x1F, g2 = (GAM_CYAN >> 5) & 0x3F, b2 = GAM_CYAN & 0x1F;
      uint16_t cr = r1 + (r2 - r1) * t;
      uint16_t cg = g1 + (g2 - g1) * t;
      uint16_t cb = b1 + (b2 - b1) * t;
      uint16_t col = (cr << 11) | (cg << 5) | cb;
      tft.drawFastVLine(x + i, y, h, col);
    }
    if (fillW > h) {
      tft.fillCircle(x + h/2, y + h/2, h/2 - 1, GAM_PURPLE);
    }
  }
  char pctStr[6];
  sprintf(pctStr, "%d%%", pct);
  tft.setTextColor(TFT_WHITE);
  tft.drawCentreString(pctStr, x + w/2, y + 1, 1);
}

void drawPlayingBadge(int x, int y) {
  if (!gameActive) return;
  float pulse = (sin(millis() * 0.004f) + 1.0f) * 0.5f;
  uint8_t brightness = 160 + (uint8_t)(95 * pulse);
  uint16_t dotColor = ((brightness >> 3) << 5) | 0x0400;
  tft.fillCircle(x, y + 5, 4, dotColor);
  tft.fillCircle(x, y + 5, 2, GAM_GREEN);
  tft.setTextColor(GAM_GREEN);
  tft.drawString("NOW PLAYING", x + 10, y, 1);
}

void drawGamingPage() {
  tft.fillScreen(GAM_BG);

  tft.setTextColor(GAM_DIM);
  tft.drawString("GAMING", 10, 3, 1);
  tft.fillCircle(SCREEN_W - 15, 7, 3, gameActive ? GAM_GREEN : 0xE8C4);
  tft.drawFastHLine(0, 15, SCREEN_W, 0x2104);

  int px1 = 8, w1 = 95;
  tft.fillRoundRect(px1, 22, w1, 55, 4, GAM_CARD);
  tft.setTextColor(GAM_DIM);
  tft.drawString("SESSION", px1+5, 27, 1);
  tft.setTextColor(GAM_CYAN);
  tft.drawString(sessionTime, px1+5, 44, 2);

  tft.fillRoundRect(px1, 84, w1, 50, 4, GAM_CARD);
  tft.setTextColor(GAM_DIM);
  tft.drawString("ACHIEVE", px1+5, 89, 1);
  drawAchievementBar(px1+5, 105, w1-10, 10, achievePct);

  tft.fillRoundRect(px1, 140, w1, 45, 4, GAM_CARD);
  tft.setTextColor(GAM_DIM);
  tft.drawString("FRIENDS", px1+5, 145, 1);
  char friendStr[8]; sprintf(friendStr, "%d", friendsOnline);
  tft.setTextColor(GAM_GREEN);
  tft.drawString(friendStr, px1+5, 160, 2);

  int px2 = 110, w2 = 100;
  tft.fillRoundRect(px2, 22, w2, 163, 6, GAM_CARD);
  tft.drawRoundRect(px2, 22, w2, 163, 6, GAM_BORDER);

  if (gameTitle[0]) {
    if (albumArt[0] != 0 || albumArt[1] != 0) {
      tft.pushImage(px2, 32, 100, 60, (uint16_t*)albumArt);
    } else {
      tft.fillRoundRect(px2+5, 32, w2-10, 60, 4, 0x0421);
      for (int i=0; i<6; i++) {
        tft.fillRect(px2+10+(i*12), 45+(i%3)*8, 8, 3, blendColor(GAM_CYAN, GAM_PURPLE, (float)i/6));
      }
    }
    tft.setTextColor(TFT_WHITE);
    String titleStr = String(gameTitle);
    tft.drawString(titleStr.substring(0, 10), px2+5, 100, 1);
    tft.drawString(titleStr.substring(10, 20), px2+5, 115, 1);

    drawPlayingBadge(px2+5, 142);
    tft.setTextColor(GAM_DIM);
    tft.drawString(gameStatus, px2+5, 165, 1);
  } else {
    tft.setTextColor(GAM_DIM);
    tft.drawCentreString("No Game", px2+w2/2, 90, 1);
  }

  int px3 = 215, w3 = 95;
  tft.drawRoundRect(px3+25, 22, 45, 30, 4, GAM_DIM);
  tft.setTextColor(GAM_DIM);
  tft.drawCentreString("STEAM", px3+47, 30, 1);

  tft.fillRoundRect(px3, 60, w3, 125, 4, GAM_CARD);
  tft.drawString("WEEKLY", px3+5, 65, 1);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("4.2h played", px3+5, 80, 1);

  drawPageIndicator(STATE_GAMING, STATE_COUNT);
  gamingPageDrawn = true;
}

void redrawGamingPartial() {
  if (currentState != STATE_GAMING) return;
  
  static char lastGameTitle[24] = "";
  int px2 = 110, w2 = 100;
  
  if (strcmp(gameTitle, lastGameTitle) != 0 || albumArtReady) {
    if (albumArtReady) albumArtReady = false; // consume flag
    strcpy(lastGameTitle, gameTitle);
    
    tft.fillRect(px2+5, 100, 90, 30, GAM_CARD); // Clear old title
    tft.fillRect(px2+5, 165, 90, 15, GAM_CARD); // Clear status
    
    if (gameTitle[0]) {
      if (albumArt[0] != 0 || albumArt[1] != 0) {
        tft.pushImage(px2+1, 32, 100, 60, (uint16_t*)albumArt);
      } else {
        tft.fillRoundRect(px2+5, 32, w2-10, 60, 4, 0x0421);
        for (int i=0; i<6; i++) {
          tft.fillRect(px2+10+(i*12), 45+(i%3)*8, 8, 3, blendColor(GAM_CYAN, GAM_PURPLE, (float)i/6));
        }
      }
      tft.setTextColor(TFT_WHITE);
      String titleStr = String(gameTitle);
      tft.drawString(titleStr.substring(0, 10), px2+5, 100, 1);
      tft.drawString(titleStr.substring(10, 20), px2+5, 115, 1);
      
      tft.setTextColor(GAM_DIM);
      tft.drawString(gameStatus, px2+5, 165, 1);
    } else {
      tft.fillRect(px2, 32, 100, 60, GAM_CARD); // clear image
      tft.setTextColor(GAM_DIM);
      tft.drawCentreString("No Game", px2+w2/2, 90, 1);
    }
  }

  if (strcmp(sessionTime, lastSessionTime) != 0) {
    int px1 = 8;
    tft.fillRect(px1+5, 44, 80, 20, GAM_CARD);
    tft.setTextColor(GAM_CYAN);
    tft.drawString(sessionTime, px1+5, 44, 2);
    strcpy(lastSessionTime, sessionTime);

    drawAchievementBar(px1+5, 105, 85, 10, achievePct);

    tft.fillRect(px1+5, 160, 80, 20, GAM_CARD);
    char friendStr[8]; sprintf(friendStr, "%d", friendsOnline);
    tft.setTextColor(GAM_GREEN);
    tft.drawString(friendStr, px1+5, 160, 2);
  }
}

#endif
