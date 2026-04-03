#ifndef PAGE_GAMING_H
#define PAGE_GAMING_H
#include "globals.h"

extern uint16_t blendColor(uint16_t c1, uint16_t c2, float t);
extern void drawPageIndicator(int current, int total);

// ═══════════════════════════════════════════════════════════
// GAMING DASHBOARD — V6 Premium Steam-Inspired Page
//
// Layout: Left game card | Right session stats
// Cyber-neon aesthetic with cyan/green accents
// Data: Steam API + local process detection
// ═══════════════════════════════════════════════════════════

// ── Layout ───────────────────────────────────────────────
#define GAM_BG       0x0841   // Dark charcoal
#define GAM_CARD     0x1082   // Surface card
#define GAM_BORDER   0x2945   // Subtle border
#define GAM_CYAN     0x07DF   // Neon cyan
#define GAM_GREEN    0x2E8B   // Playing green
#define GAM_DIM      0x4A49   // Muted label
#define GAM_BAR_BG   0x2104   // Progress bar track
#define GAM_PURPLE   0x79FD   // Achievement purple

static bool gamingPageDrawn = false;
static char lastSessionTime[12] = "";

void resetGamingDrawState() {
  gamingPageDrawn = false;
  lastSessionTime[0] = 0;
}

// ── Draw Steam-style progress bar ────────────────────────
void drawAchievementBar(int x, int y, int w, int h, uint8_t pct) {
  // Track
  tft.fillRoundRect(x, y, w, h, h/2, GAM_BAR_BG);

  // Fill
  int fillW = (int)((long)pct * w / 100);
  if (fillW > 0) {
    // Gradient fill: left purple → right cyan
    for (int i = 0; i < fillW; i++) {
      float t = (float)i / max(1, fillW);
      // Simple 2-color lerp in RGB565 channels
      uint16_t r1 = (GAM_PURPLE >> 11) & 0x1F, g1 = (GAM_PURPLE >> 5) & 0x3F, b1 = GAM_PURPLE & 0x1F;
      uint16_t r2 = (GAM_CYAN >> 11) & 0x1F, g2 = (GAM_CYAN >> 5) & 0x3F, b2 = GAM_CYAN & 0x1F;
      uint16_t cr = r1 + (r2 - r1) * t;
      uint16_t cg = g1 + (g2 - g1) * t;
      uint16_t cb = b1 + (b2 - b1) * t;
      uint16_t col = (cr << 11) | (cg << 5) | cb;
      tft.drawFastVLine(x + i, y, h, col);
    }
    // Round the left end
    if (fillW > h) {
      tft.fillCircle(x + h/2, y + h/2, h/2 - 1, GAM_PURPLE);
    }
  }

  // Percentage text
  char pctStr[6];
  sprintf(pctStr, "%d%%", pct);
  tft.setTextColor(TFT_WHITE);
  tft.drawCentreString(pctStr, x + w/2, y + 1, 1);
}

// ── Draw "now playing" badge with pulsing dot ────────────
void drawPlayingBadge(int x, int y) {
  if (!gameActive) return;

  // Pulsing green dot
  float pulse = (sin(millis() * 0.004f) + 1.0f) * 0.5f;
  uint8_t brightness = 160 + (uint8_t)(95 * pulse);
  uint16_t dotColor = ((brightness >> 3) << 5) | 0x0400; // Green channel
  tft.fillCircle(x, y + 5, 4, dotColor);
  tft.fillCircle(x, y + 5, 2, GAM_GREEN);

  tft.setTextColor(GAM_GREEN);
  tft.drawString("NOW PLAYING", x + 10, y, 1);
}

// ── Full Page Draw ───────────────────────────────────────
void drawGamingPage() {
  tft.fillScreen(GAM_BG);

  // ── Header ──
  tft.setTextColor(GAM_DIM);
  tft.drawString("GAMING", 10, 3, 1);

  // Status dot
  tft.fillCircle(SCREEN_W - 15, 7, 3, gameActive ? GAM_GREEN : 0xE8C4);

  tft.drawFastHLine(0, 15, SCREEN_W, 0x2104);

  // ═══════════════════════════════════════════════════════
  // LEFT PANE: Game Card (0 to 145)
  // ═══════════════════════════════════════════════════════

  // Card background with subtle border
  tft.fillRoundRect(8, 22, 138, 195, 6, GAM_CARD);
  tft.drawRoundRect(8, 22, 138, 195, 6, GAM_BORDER);

  if (gameTitle[0]) {
    // Game icon placeholder (colored rectangle)
    uint16_t iconColor = GAM_CYAN;
    tft.fillRoundRect(18, 32, 118, 60, 4, 0x0421);

    // "Game cover" abstract pattern
    for (int i = 0; i < 8; i++) {
      int lx = 20 + (i * 14);
      int ly = 45 + (i % 3) * 8;
      tft.fillRect(lx, ly, 10, 3, blendColor(GAM_CYAN, GAM_PURPLE, (float)i/8));
    }

    // Game title
    tft.setTextColor(TFT_WHITE);
    String titleStr = String(gameTitle);
    if (titleStr.length() > 14) {
      tft.drawString(titleStr.substring(0, 14), 18, 100, 2);
      if (titleStr.length() > 14) {
        tft.drawString(titleStr.substring(14, 28), 18, 118, 2);
      }
    } else {
      tft.drawString(gameTitle, 18, 100, 2);
    }

    // Playing badge
    drawPlayingBadge(18, 142);

    // Status text
    tft.setTextColor(GAM_DIM);
    tft.drawString(gameStatus, 18, 165, 1);

  } else {
    // No game state
    tft.setTextColor(GAM_DIM);
    tft.drawCentreString("No game", 77, 90, 2);
    tft.drawCentreString("detected", 77, 108, 2);

    // Steam icon placeholder
    tft.drawRoundRect(57, 55, 40, 30, 4, GAM_DIM);
    tft.setTextColor(GAM_DIM);
    tft.drawCentreString("STEAM", 77, 62, 1);
  }

  // ═══════════════════════════════════════════════════════
  // RIGHT PANE: Stats (155 to 310)
  // ═══════════════════════════════════════════════════════

  int rx = 155;

  // Vertical separator
  tft.drawFastVLine(150, 16, SCREEN_H - 32, 0x2104);

  // ── Session Time Card ──
  tft.fillRoundRect(rx, 22, 155, 55, 4, GAM_CARD);
  tft.setTextColor(GAM_DIM);
  tft.drawString("SESSION", rx + 8, 27, 1);
  tft.drawFastHLine(rx + 8, 38, 80, 0x2104);

  // Huge session time
  tft.setTextColor(GAM_CYAN);
  tft.drawString(sessionTime, rx + 8, 44, 4);

  // ── Achievement Card ──
  tft.fillRoundRect(rx, 84, 155, 52, 4, GAM_CARD);
  tft.setTextColor(GAM_DIM);
  tft.drawString("ACHIEVEMENTS", rx + 8, 89, 1);

  drawAchievementBar(rx + 8, 105, 138, 14, achievePct);

  // ── Friends Card ──
  tft.fillRoundRect(rx, 143, 155, 45, 4, GAM_CARD);
  tft.setTextColor(GAM_DIM);
  tft.drawString("FRIENDS ONLINE", rx + 8, 148, 1);

  // Friend count
  char friendStr[8];
  sprintf(friendStr, "%d", friendsOnline);
  tft.setTextColor(GAM_GREEN);
  tft.drawString(friendStr, rx + 8, 163, 4);

  // Friend dots (visual indicator)
  for (int i = 0; i < min((int)friendsOnline, 8); i++) {
    tft.fillCircle(rx + 60 + i * 12, 172, 3, GAM_GREEN);
  }

  // ── Weekly Stats (small footer) ──
  tft.setTextColor(GAM_DIM);
  tft.drawString("This week", rx + 8, 195, 1);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("4.2h played", rx + 70, 195, 1);

  // Page dots
  drawPageIndicator(STATE_GAMING, STATE_COUNT);

  gamingPageDrawn = true;
}

// ── Partial Update ───────────────────────────────────────
void redrawGamingPartial() {
  if (currentState != STATE_GAMING) return;

  // Update session time (dirty rect)
  if (strcmp(sessionTime, lastSessionTime) != 0) {
    int rx = 155;
    tft.fillRect(rx + 8, 44, 140, 26, GAM_CARD);
    tft.setTextColor(GAM_CYAN);
    tft.drawString(sessionTime, rx + 8, 44, 4);
    strcpy(lastSessionTime, sessionTime);

    // Redraw achievement bar
    drawAchievementBar(rx + 8, 105, 138, 14, achievePct);

    // Redraw friends
    tft.fillRect(rx + 8, 163, 140, 20, GAM_CARD);
    char friendStr[8];
    sprintf(friendStr, "%d", friendsOnline);
    tft.setTextColor(GAM_GREEN);
    tft.drawString(friendStr, rx + 8, 163, 4);
    for (int i = 0; i < min((int)friendsOnline, 8); i++) {
      tft.fillCircle(rx + 60 + i * 12, 172, 3, GAM_GREEN);
    }
  }
}

#endif
