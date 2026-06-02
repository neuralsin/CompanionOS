#ifndef PAGE_GAMING_H
#define PAGE_GAMING_H
#include "globals.h"

extern void drawPageIndicator(int current, int total);

// ═══════════════════════════════════════════════════════════
// GAMING DASHBOARD — V7 Compact Steam-Inspired Page
// Resolution-aware via SCALE_X/SCALE_Y macros.
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

// Achievement progress bar with purple→cyan gradient
void drawAchievementBar(int x, int y, int w, int h, uint8_t pct) {
  tft.fillRoundRect(x, y, w, h, h/2, GAM_BAR_BG);
  int fillW = (int)((long)pct * w / 100);
  if (fillW > 0) {
    for (int i = 0; i < fillW; i++) {
      float t = (float)i / max(1, fillW);
      uint16_t col = blendColor(GAM_PURPLE, GAM_CYAN, t);
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

// Pulsing "NOW PLAYING" badge
void drawPlayingBadge(int x, int y) {
  if (!gameActive) return;
  float pulse = (sin(millis() * 0.004f) + 1.0f) * 0.5f;
  uint8_t brightness = 160 + (uint8_t)(95 * pulse);
  uint16_t dotColor = ((brightness >> 3) << 5) | 0x0400;
  tft.fillCircle(x, y + SCALE_Y(5), SCALE_MIN(4), dotColor);
  tft.fillCircle(x, y + SCALE_Y(5), SCALE_MIN(2), GAM_GREEN);
  tft.setTextColor(GAM_GREEN);
  tft.drawString("PLAYING", x + SCALE_X(8), y, 1);
}

// ═══════════════════════════════════════════════════════════
// FULL PAGE DRAW — Redesigned for 128×160 (landscape)
// Layout: Header | Session + Friends row | Game card | Recent
// ═══════════════════════════════════════════════════════════

void drawGamingPage() {
  tft.fillScreen(GAM_BG);

  // ── Header bar ──
  tft.setTextColor(GAM_DIM);
  tft.drawString("GAMING", SCALE_X(10), SCALE_Y(3), 1);
  tft.fillCircle(SCREEN_W - SCALE_X(15), SCALE_Y(7), SCALE_MIN(3), gameActive ? GAM_GREEN : 0xE8C4);
  tft.drawFastHLine(0, SCALE_Y(14), SCREEN_W, 0x2104);

  // ── Left column: Session + Friends + Achievement ──
  int lx = SCALE_X(4);
  int lw = SCALE_X(72);
  int topY = SCALE_Y(18);

  // Session card
  tft.fillRoundRect(lx, topY, lw, SCALE_Y(28), 3, GAM_CARD);
  tft.setTextColor(GAM_DIM);
  tft.drawString("SESSION", lx + SCALE_X(4), topY + SCALE_Y(2), 1);
  tft.setTextColor(GAM_CYAN);
  tft.drawString(sessionTime, lx + SCALE_X(4), topY + SCALE_Y(14), 1);

  // Friends card
  int friendY = topY + SCALE_Y(32);
  tft.fillRoundRect(lx, friendY, lw, SCALE_Y(24), 3, GAM_CARD);
  tft.setTextColor(GAM_DIM);
  tft.drawString("FRIENDS", lx + SCALE_X(4), friendY + SCALE_Y(2), 1);
  char friendStr[8]; sprintf(friendStr, "%d", friendsOnline);
  tft.setTextColor(GAM_GREEN);
  tft.drawString(friendStr, lx + SCALE_X(4), friendY + SCALE_Y(13), 1);

  // Achievement bar
  int achY = friendY + SCALE_Y(28);
  tft.setTextColor(GAM_DIM);
  tft.drawString("ACHIEV", lx + SCALE_X(4), achY, 1);
  drawAchievementBar(lx, achY + SCALE_Y(10), lw, SCALE_Y(8), achievePct);

  // ── Right column: Current game card ──
  int rx = lx + lw + SCALE_X(4);
  int rw = SCREEN_W - rx - SCALE_X(4);

  tft.fillRoundRect(rx, topY, rw, SCALE_Y(70), 4, GAM_CARD);
  tft.drawRoundRect(rx, topY, rw, SCALE_Y(70), 4, GAM_BORDER);

  if (gameTitle[0]) {
    // Game title (2 lines)
    tft.setTextColor(TFT_WHITE);
    String titleStr = String(gameTitle);
    int titleX = rx + SCALE_X(4);
    int titleY = topY + SCALE_Y(4);
    
    if (titleStr.length() <= 10) {
      tft.drawString(titleStr.c_str(), titleX, titleY, 1);
    } else {
      tft.drawString(titleStr.substring(0, 10).c_str(), titleX, titleY, 1);
      tft.drawString(titleStr.substring(10, 20).c_str(), titleX, titleY + SCALE_Y(12), 1);
    }

    // Now playing badge
    drawPlayingBadge(rx + SCALE_X(4), topY + SCALE_Y(35));

    // Status line
    tft.setTextColor(GAM_DIM);
    tft.drawString(gameStatus, rx + SCALE_X(4), topY + SCALE_Y(55), 1);
  } else {
    tft.setTextColor(GAM_DIM);
    tft.drawCentreString("No Game", rx + rw/2, topY + SCALE_Y(30), 1);
  }

  // ── Bottom section: Recent games list ──
  int recentY = SCALE_Y(100);
  tft.setTextColor(GAM_DIM);
  tft.drawString("RECENT", lx + SCALE_X(2), recentY, 1);
  recentY += SCALE_Y(10);

  for (int i = 0; i < 3; i++) {
    int rowY = recentY + i * SCALE_Y(10);
    if (recentGame[i][0]) {
      // Game name (truncated)
      tft.setTextColor(GAM_CYAN);
      String rName = String(recentGame[i]);
      if (rName.length() > 12) rName = rName.substring(0, 10) + "..";
      tft.drawString(rName.c_str(), lx + SCALE_X(2), rowY, 1);

      // Playtime
      char ptBuf[12];
      if (recentPlaytime[i] >= 60) {
        sprintf(ptBuf, "%dh", recentPlaytime[i] / 60);
      } else {
        sprintf(ptBuf, "%dm", recentPlaytime[i]);
      }
      tft.setTextColor(GAM_DIM);
      tft.drawString(ptBuf, SCREEN_W - SCALE_X(24), rowY, 1);
    } else {
      tft.setTextColor(0x2104);
      tft.drawString("---", lx + SCALE_X(2), rowY, 1);
    }
  }

  drawPageIndicator(STATE_GAMING, STATE_COUNT);
  gamingPageDrawn = true;
}

// ═══════════════════════════════════════════════════════════
// PARTIAL REDRAW — Only update changed fields
// ═══════════════════════════════════════════════════════════

void redrawGamingPartial() {
  if (currentState != STATE_GAMING) return;
  
  static char lastGameTitle[24] = "";
  int rx = SCALE_X(4) + SCALE_X(72) + SCALE_X(4);
  int rw = SCREEN_W - rx - SCALE_X(4);
  int topY = SCALE_Y(18);
  
  // Title changed or album art ready
  if (strcmp(gameTitle, lastGameTitle) != 0 || albumArtReady) {
    if (albumArtReady) albumArtReady = false;
    strcpy(lastGameTitle, gameTitle);
    
    // Clear and redraw game card
    tft.fillRoundRect(rx + 1, topY + 1, rw - 2, SCALE_Y(70) - 2, 3, GAM_CARD);
    
    if (gameTitle[0]) {
      tft.setTextColor(TFT_WHITE);
      String titleStr = String(gameTitle);
      int titleX = rx + SCALE_X(4);
      int titleY = topY + SCALE_Y(4);
      
      if (titleStr.length() <= 10) {
        tft.drawString(titleStr.c_str(), titleX, titleY, 1);
      } else {
        tft.drawString(titleStr.substring(0, 10).c_str(), titleX, titleY, 1);
        tft.drawString(titleStr.substring(10, 20).c_str(), titleX, titleY + SCALE_Y(12), 1);
      }
      
      drawPlayingBadge(rx + SCALE_X(4), topY + SCALE_Y(35));
      tft.setTextColor(GAM_DIM);
      tft.drawString(gameStatus, rx + SCALE_X(4), topY + SCALE_Y(55), 1);
    } else {
      tft.setTextColor(GAM_DIM);
      tft.drawCentreString("No Game", rx + rw/2, topY + SCALE_Y(30), 1);
    }
  }

  // Session time changed
  if (strcmp(sessionTime, lastSessionTime) != 0) {
    int lx = SCALE_X(4);
    int lw = SCALE_X(72);
    
    // Update session
    tft.fillRect(lx + SCALE_X(4), topY + SCALE_Y(14), lw - SCALE_X(8), SCALE_Y(12), GAM_CARD);
    tft.setTextColor(GAM_CYAN);
    tft.drawString(sessionTime, lx + SCALE_X(4), topY + SCALE_Y(14), 1);
    strcpy(lastSessionTime, sessionTime);

    // Update friends
    int friendY = topY + SCALE_Y(32);
    tft.fillRect(lx + SCALE_X(4), friendY + SCALE_Y(13), lw - SCALE_X(8), SCALE_Y(10), GAM_CARD);
    char friendStr[8]; sprintf(friendStr, "%d", friendsOnline);
    tft.setTextColor(GAM_GREEN);
    tft.drawString(friendStr, lx + SCALE_X(4), friendY + SCALE_Y(13), 1);

    // Update achievement
    int achY = friendY + SCALE_Y(28);
    drawAchievementBar(lx, achY + SCALE_Y(10), lw, SCALE_Y(8), achievePct);
  }
}

#endif
