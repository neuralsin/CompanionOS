#ifndef EYES_H
#define EYES_H

#include "globals.h"
#include <math.h>

// ═══════════════════════════════════════════════════════════
// EYE ANIMATION ENGINE - Production v3.0
//
// Features:
//   - 8 distinct emotional expressions
//   - Smooth multi-phase blinking with eyelid animation
//   - Autonomous pupil drift (idle "looking around")
//   - Real-time clock display on the Eyes page
//   - Per-frame partial redraw (no full-screen flicker)
// ═══════════════════════════════════════════════════════════

enum Emotion {
  EMO_HAPPY,
  EMO_SAD,
  EMO_EXCITED,
  EMO_LOVE,
  EMO_SLEEPY,
  EMO_ANGRY,
  EMO_SURPRISED,
  EMO_NEUTRAL,
  EMO_COUNT
};

extern Emotion currentEmotion;

// ── Layout Constants ─────────────────────────────────────
#define EYE_Y        155
#define LEFT_EYE_X   70
#define RIGHT_EYE_X  170
#define EYE_RX       38   // Horizontal radius
#define EYE_RY       46   // Vertical radius
#define PUPIL_R      12
#define IRIS_R       20
#define HIGHLIGHT_R  4

// ── Animation State ──────────────────────────────────────
extern unsigned long lastBlink;
extern bool isBlinking;
extern int blinkPhase;

// Pupil drift
float pupilOffsetX = 0;
float pupilOffsetY = 0;
float pupilTargetX = 0;
float pupilTargetY = 0;
unsigned long lastPupilMove = 0;

// Time state (synced from Python via UDP "TIME:" payload, or from NTP)
int displayHour = 0;
int displayMinute = 0;
bool timeReceived = false;
unsigned long lastTimeDraw = 0;
unsigned long bootMillis = 0;

// ── Forward Declarations ─────────────────────────────────
void drawEyes();
void setEmotion(Emotion newEmotion);
void updateEyes();
void drawTimeDisplay();

// ── Helper: Draw a single rounded-rect eye shape ─────────
void drawEyeShape(int cx, int cy, int rx, int ry, uint16_t color) {
  // Rounded rectangle eye (like the reference video)
  int cornerR = min(rx, ry) / 3;
  tft.fillRoundRect(cx - rx, cy - ry, rx * 2, ry * 2, cornerR, color);
}

// ── Helper: Draw iris + pupil + highlight inside an eye ──
void drawPupil(int cx, int cy, float offX, float offY, uint16_t irisColor) {
  int px = cx + (int)offX;
  int py = cy + (int)offY;
  tft.fillCircle(px, py, IRIS_R, irisColor);
  tft.fillCircle(px, py, PUPIL_R, COLOR_PUPIL);
  tft.fillCircle(px - 4, py - 5, HIGHLIGHT_R, COLOR_HIGHLIGHT);
  tft.fillCircle(px + 6, py + 3, 2, 0xC618); // subtle secondary highlight
}

// ── Helper: Draw a heart shape ───────────────────────────
void drawHeart(int cx, int cy, int size, uint16_t color) {
  int half = size / 2;
  tft.fillCircle(cx - half, cy - half/2, half, color);
  tft.fillCircle(cx + half, cy - half/2, half, color);
  tft.fillTriangle(cx - size - 2, cy, cx + size + 2, cy, cx, cy + size + half/2, color);
}

// ── Helper: Draw a small star ────────────────────────────
void drawStar(int cx, int cy, int r, uint16_t color) {
  for (int i = 0; i < 5; i++) {
    float angle = i * 72.0 * PI / 180.0;
    int x1 = cx + (int)(r * cos(angle));
    int y1 = cy + (int)(r * sin(angle));
    tft.drawLine(cx, cy, x1, y1, color);
  }
  tft.fillCircle(cx, cy, 2, color);
}

// ═══════════════════════════════════════════════════════════
// EMOTION RENDERERS
// ═══════════════════════════════════════════════════════════

void drawHappyEyes() {
  // Rounded rect eyes, squinted bottom (happy arc)
  drawEyeShape(LEFT_EYE_X, EYE_Y, EYE_RX, EYE_RY - 10, COLOR_EYE);
  drawPupil(LEFT_EYE_X, EYE_Y - 5, pupilOffsetX, pupilOffsetY, 0x0410);
  // Happy curve underneath
  tft.fillRect(LEFT_EYE_X - EYE_RX, EYE_Y + EYE_RY - 22, EYE_RX * 2, 14, COLOR_BG);

  drawEyeShape(RIGHT_EYE_X, EYE_Y, EYE_RX, EYE_RY - 10, COLOR_EYE);
  drawPupil(RIGHT_EYE_X, EYE_Y - 5, pupilOffsetX, pupilOffsetY, 0x0410);
  tft.fillRect(RIGHT_EYE_X - EYE_RX, EYE_Y + EYE_RY - 22, EYE_RX * 2, 14, COLOR_BG);

  // Small mouth
  tft.drawArc(SCREEN_W/2, EYE_Y + 60, 15, 12, 200, 340, COLOR_EYE, COLOR_BG);
}

void drawSadEyes() {
  // Droopy at the outer corners
  drawEyeShape(LEFT_EYE_X, EYE_Y + 5, EYE_RX, EYE_RY - 5, COLOR_EYE);
  drawPupil(LEFT_EYE_X, EYE_Y + 8, 0, 4, 0x0410);
  // Droopy eyebrow
  tft.drawLine(LEFT_EYE_X - EYE_RX, EYE_Y - EYE_RY + 5, LEFT_EYE_X + EYE_RX, EYE_Y - EYE_RY - 8, 0x4208);
  
  drawEyeShape(RIGHT_EYE_X, EYE_Y + 5, EYE_RX, EYE_RY - 5, COLOR_EYE);
  drawPupil(RIGHT_EYE_X, EYE_Y + 8, 0, 4, 0x0410);
  tft.drawLine(RIGHT_EYE_X - EYE_RX, EYE_Y - EYE_RY - 8, RIGHT_EYE_X + EYE_RX, EYE_Y - EYE_RY + 5, 0x4208);

  // Tear drops
  for (int i = 0; i < 3; i++) {
    int ty = EYE_Y + EYE_RY + 5 + i * 12;
    tft.fillCircle(LEFT_EYE_X + 5, ty, 3 - i, COLOR_EYE);
    tft.fillCircle(RIGHT_EYE_X - 5, ty, 3 - i, COLOR_EYE);
  }
}

void drawExcitedEyes() {
  // Extra large, wide open
  int rx = EYE_RX + 8;
  int ry = EYE_RY + 8;
  drawEyeShape(LEFT_EYE_X, EYE_Y, rx, ry, COLOR_EYE);
  drawPupil(LEFT_EYE_X, EYE_Y, pupilOffsetX * 1.5, pupilOffsetY * 1.5, 0x0410);

  drawEyeShape(RIGHT_EYE_X, EYE_Y, rx, ry, COLOR_EYE);
  drawPupil(RIGHT_EYE_X, EYE_Y, pupilOffsetX * 1.5, pupilOffsetY * 1.5, 0x0410);

  // Sparkles around
  for (int i = 0; i < 8; i++) {
    float angle = (millis() / 200 + i * 45) * PI / 180.0;
    int sx = SCREEN_W/2 + (int)(90 * cos(angle));
    int sy = EYE_Y + (int)(70 * sin(angle));
    if (sy > 40 && sy < SCREEN_H - 80) drawStar(sx, sy, 5, COLOR_EYE);
  }
}

void drawLoveEyes() {
  // Hearts as eyes
  drawHeart(LEFT_EYE_X, EYE_Y, 22, TFT_PINK);
  drawHeart(RIGHT_EYE_X, EYE_Y, 22, TFT_PINK);

  // Floating hearts
  for (int i = 0; i < 4; i++) {
    int hx = 30 + random(0, SCREEN_W - 60);
    int hy = 40 + random(0, 60);
    drawHeart(hx, hy, 6 + random(0, 4), 0xF8B2);
  }
}

void drawSleepyEyes() {
  // Very thin horizontal slits
  int slitH = 8;
  int cornerR = 4;

  tft.fillRoundRect(LEFT_EYE_X - EYE_RX, EYE_Y - slitH/2, EYE_RX * 2, slitH, cornerR, COLOR_EYE);
  tft.fillRoundRect(RIGHT_EYE_X - EYE_RX, EYE_Y - slitH/2, EYE_RX * 2, slitH, cornerR, COLOR_EYE);

  // Floating Z's
  tft.setTextColor(COLOR_EYE);
  tft.drawString("z", RIGHT_EYE_X + 30, EYE_Y - 50, 2);
  tft.drawString("Z", RIGHT_EYE_X + 40, EYE_Y - 75, 4);
  tft.drawString("Z", RIGHT_EYE_X + 20, EYE_Y - 100, 2);
}

void drawAngryEyes() {
  // Narrow, angled inward
  int rx = EYE_RX;
  int ry = EYE_RY - 15;
  drawEyeShape(LEFT_EYE_X, EYE_Y, rx, ry, TFT_RED);
  drawPupil(LEFT_EYE_X, EYE_Y, 3, 0, TFT_MAROON);

  drawEyeShape(RIGHT_EYE_X, EYE_Y, rx, ry, TFT_RED);
  drawPupil(RIGHT_EYE_X, EYE_Y, -3, 0, TFT_MAROON);

  // Angry thick eyebrows (V shape inward)
  for (int t = 0; t < 4; t++) {
    tft.drawLine(LEFT_EYE_X - EYE_RX - 5, EYE_Y - ry - 10 + t, LEFT_EYE_X + EYE_RX + 5, EYE_Y - ry - 20 + t, TFT_RED);
    tft.drawLine(RIGHT_EYE_X - EYE_RX - 5, EYE_Y - ry - 20 + t, RIGHT_EYE_X + EYE_RX + 5, EYE_Y - ry - 10 + t, TFT_RED);
  }
}

void drawSurprisedEyes() {
  // Perfectly round, wide open
  int r = EYE_RX + 5;
  tft.fillCircle(LEFT_EYE_X, EYE_Y, r, COLOR_EYE);
  tft.fillCircle(LEFT_EYE_X, EYE_Y, IRIS_R + 2, 0x0410);
  tft.fillCircle(LEFT_EYE_X, EYE_Y, 6, COLOR_PUPIL);
  tft.fillCircle(LEFT_EYE_X - 3, EYE_Y - 4, 3, COLOR_HIGHLIGHT);

  tft.fillCircle(RIGHT_EYE_X, EYE_Y, r, COLOR_EYE);
  tft.fillCircle(RIGHT_EYE_X, EYE_Y, IRIS_R + 2, 0x0410);
  tft.fillCircle(RIGHT_EYE_X, EYE_Y, 6, COLOR_PUPIL);
  tft.fillCircle(RIGHT_EYE_X - 3, EYE_Y - 4, 3, COLOR_HIGHLIGHT);

  // Open mouth "O"
  tft.drawCircle(SCREEN_W/2, EYE_Y + 65, 10, COLOR_EYE);
}

void drawNeutralEyes() {
  drawEyeShape(LEFT_EYE_X, EYE_Y, EYE_RX, EYE_RY, COLOR_EYE);
  drawPupil(LEFT_EYE_X, EYE_Y, pupilOffsetX, pupilOffsetY, 0x0410);

  drawEyeShape(RIGHT_EYE_X, EYE_Y, EYE_RX, EYE_RY, COLOR_EYE);
  drawPupil(RIGHT_EYE_X, EYE_Y, pupilOffsetX, pupilOffsetY, 0x0410);
}

// ═══════════════════════════════════════════════════════════
// MAIN DRAW CALL
// ═══════════════════════════════════════════════════════════

void drawEyes() {
  // Only clear the eye canvas area, not the clock or top bar
  tft.fillRect(0, 35, SCREEN_W, SCREEN_H - 70, COLOR_BG);

  switch (currentEmotion) {
    case EMO_HAPPY:     drawHappyEyes(); break;
    case EMO_SAD:       drawSadEyes(); break;
    case EMO_EXCITED:   drawExcitedEyes(); break;
    case EMO_LOVE:      drawLoveEyes(); break;
    case EMO_SLEEPY:    drawSleepyEyes(); break;
    case EMO_ANGRY:     drawAngryEyes(); break;
    case EMO_SURPRISED: drawSurprisedEyes(); break;
    default:            drawNeutralEyes(); break;
  }
}

void setEmotion(Emotion newEmotion) {
  if (newEmotion != currentEmotion) {
    currentEmotion = newEmotion;
    drawEyes();
  }
}

// ═══════════════════════════════════════════════════════════
// TIME DISPLAY (Bottom of Eyes Page)
// ═══════════════════════════════════════════════════════════

void drawTimeDisplay() {
  // Clear time area at the bottom
  tft.fillRect(0, SCREEN_H - 35, SCREEN_W, 35, COLOR_BG);

  // Build time string from local millis clock
  unsigned long elapsed = (millis() - bootMillis) / 1000;
  int h = displayHour + (elapsed / 3600);
  int m = displayMinute + ((elapsed % 3600) / 60);
  h = h % 24;
  m = m % 60;

  char timeBuf[6];
  sprintf(timeBuf, "%02d:%02d", h, m);

  tft.setTextColor(TFT_WHITE, COLOR_BG);
  tft.drawCentreString(timeBuf, SCREEN_W/2, SCREEN_H - 30, 7); // Font 7: Large 7-seg
}

// ═══════════════════════════════════════════════════════════
// ANIMATION LOOP (called every frame from loop())
// ═══════════════════════════════════════════════════════════

void updateEyes() {
  unsigned long now = millis();

  // ── Pupil Drift (idle look-around) ──
  if (now - lastPupilMove > 2500) {
    lastPupilMove = now;
    pupilTargetX = random(-10, 11);
    pupilTargetY = random(-6, 7);
  }
  // Smooth lerp toward target
  pupilOffsetX += (pupilTargetX - pupilOffsetX) * 0.15;
  pupilOffsetY += (pupilTargetY - pupilOffsetY) * 0.15;

  // ── Blinking ──
  if (now - lastBlink > 3000 + random(0, 2000)) {
    if (!isBlinking) {
      isBlinking = true;
      blinkPhase = 0;
      lastBlink = now;
    }
  }

  if (isBlinking) {
    blinkPhase++;
    if (blinkPhase <= 3) {
      // Close: progressively cover eyes from top and bottom
      int closeAmount = blinkPhase * (EYE_RY / 3);
      tft.fillRect(LEFT_EYE_X - EYE_RX - 2, EYE_Y - EYE_RY - 2, EYE_RX * 2 + 4, closeAmount, COLOR_BG);
      tft.fillRect(LEFT_EYE_X - EYE_RX - 2, EYE_Y + EYE_RY + 2 - closeAmount, EYE_RX * 2 + 4, closeAmount, COLOR_BG);
      tft.fillRect(RIGHT_EYE_X - EYE_RX - 2, EYE_Y - EYE_RY - 2, EYE_RX * 2 + 4, closeAmount, COLOR_BG);
      tft.fillRect(RIGHT_EYE_X - EYE_RX - 2, EYE_Y + EYE_RY + 2 - closeAmount, EYE_RX * 2 + 4, closeAmount, COLOR_BG);
      // Eyelid lines
      tft.fillRect(LEFT_EYE_X - EYE_RX, EYE_Y - 1, EYE_RX * 2, 3, COLOR_EYE);
      tft.fillRect(RIGHT_EYE_X - EYE_RX, EYE_Y - 1, EYE_RX * 2, 3, COLOR_EYE);
    } else if (blinkPhase == 5) {
      drawEyes();
      isBlinking = false;
    }
  }

  // ── Time Display (refresh every ~15s to avoid flicker) ──
  if (now - lastTimeDraw > 15000 || lastTimeDraw == 0) {
    lastTimeDraw = now;
    drawTimeDisplay();
  }
}

#endif
