#ifndef THEME_TRANSITION_H
#define THEME_TRANSITION_H

#include "globals.h"
#include "themes.h"

// ═══════════════════════════════════════════════════════════
// CYBERPUNK THEME TRANSITION — 5-Phase Bridge Animation
//
// Phases: Capture (350ms) → Shred (500ms) → Morph (650ms)
//       → Stitch (450ms) → Handoff (300ms) = ~2250ms total
// ═══════════════════════════════════════════════════════════

// ── Required forward declarations ────────────────────────
extern void saveThemeToEEPROM();
extern uint16_t blendColor(uint16_t c1, uint16_t c2, float t);
static uint32_t trSeed = 0xCAFEBABE;
static uint32_t trRand() {
  trSeed ^= trSeed << 13;
  trSeed ^= trSeed >> 17;
  trSeed ^= trSeed << 5;
  return trSeed;
}

// ── Status text lines ────────────────────────────────────
const char* TR_LINES[] = {
  "syncing theme...",
  "rewriting vibe...",
  "rebuilding frame...",
  "morphing identity...",
  "handing off..."
};

// ═══════════════════════════════════════════════════════════
// PHASE 1: CAPTURE (350ms)
// Source theme freezes, edge glow, corner brackets, jitter
// ═══════════════════════════════════════════════════════════

void phaseCapture(uint16_t fromColor, int durationMs) {
  unsigned long start = millis();

  // Dim the current screen to 70%
  // Draw darkening overlay strips
  for (int y = 0; y < SCREEN_H; y += 3) {
    tft.drawFastHLine(0, y, SCREEN_W, TR_BASE);
  }

  while (millis() - start < (unsigned long)durationMs) {
    float t = (float)(millis() - start) / durationMs;  // 0→1

    // Corner brackets that lock on
    int bracketSize = 20 + (int)(t * 15);
    uint16_t bracketColor = blendColor(TR_NEUTRAL, TR_CYAN, t);

    // Top-left
    tft.drawFastHLine(4, 4, bracketSize, bracketColor);
    tft.drawFastVLine(4, 4, bracketSize, bracketColor);
    // Top-right
    tft.drawFastHLine(SCREEN_W - 4 - bracketSize, 4, bracketSize, bracketColor);
    tft.drawFastVLine(SCREEN_W - 5, 4, bracketSize, bracketColor);
    // Bottom-left
    tft.drawFastHLine(4, SCREEN_H - 5, bracketSize, bracketColor);
    tft.drawFastVLine(4, SCREEN_H - 4 - bracketSize, bracketSize, bracketColor);
    // Bottom-right
    tft.drawFastHLine(SCREEN_W - 4 - bracketSize, SCREEN_H - 5, bracketSize, bracketColor);
    tft.drawFastVLine(SCREEN_W - 5, SCREEN_H - 4 - bracketSize, bracketSize, bracketColor);

    // Edge glow intensifies — draw bright pixels along edges
    if (t > 0.3f) {
      for (int i = 0; i < 8; i++) {
        int x = trRand() % SCREEN_W;
        tft.drawPixel(x, 0, blendColor(TR_BASE, fromColor, t * 0.5f));
        tft.drawPixel(x, SCREEN_H - 1, blendColor(TR_BASE, fromColor, t * 0.5f));
      }
    }

    // Micro jitter — shift a few horizontal lines
    if (t > 0.5f) {
      int jitterY = trRand() % SCREEN_H;
      int jitterX = (trRand() % 5) - 2;  // -2 to +2
      if (jitterX != 0) {
        // Simulate by drawing a small colored rect
        tft.fillRect(jitterX < 0 ? 0 : SCREEN_W - 4, jitterY, 4, 2, TR_PINK);
      }
    }

    delay(25);
  }
}

// ═══════════════════════════════════════════════════════════
// PHASE 2: SHRED (500ms)
// Screen fractures into neon shards drifting outward
// ═══════════════════════════════════════════════════════════

void phaseShred(uint16_t fromColor, int durationMs) {
  unsigned long start = millis();
  tft.fillScreen(TR_BASE);

  // Generate shard positions (procedural, no arrays)
  while (millis() - start < (unsigned long)durationMs) {
    float t = (float)(millis() - start) / durationMs;

    // Perspective grid in background
    int cx = SCREEN_W / 2;
    int cy = SCREEN_H / 2;
    for (int i = 0; i < 6; i++) {
      int offset = 20 + i * 25;
      uint16_t gridColor = blendColor(TR_BASE, TR_PURPLE, 0.15f);
      tft.drawRect(cx - offset, cy - offset * SCREEN_H / SCREEN_W,
                    offset * 2, offset * 2 * SCREEN_H / SCREEN_W, gridColor);
    }

    // Neon shards drifting outward from center
    trSeed = 0xDEAD0000 + (uint32_t)(t * 100);  // Deterministic per frame
    for (int s = 0; s < 12; s++) {
      float angle = (trRand() % 360) * PI / 180.0f;
      float dist = t * (40 + trRand() % 80);
      int sx = cx + (int)(cos(angle) * dist);
      int sy = cy + (int)(sin(angle) * dist);
      int sw = 4 + trRand() % 8;
      int sh = 2 + trRand() % 4;

      uint16_t shardColor;
      int colorPick = trRand() % 4;
      if (colorPick == 0) shardColor = TR_CYAN;
      else if (colorPick == 1) shardColor = TR_PINK;
      else if (colorPick == 2) shardColor = fromColor;
      else shardColor = TR_PURPLE;

      if (sx > 0 && sx < SCREEN_W - sw && sy > 0 && sy < SCREEN_H - sh) {
        tft.fillRect(sx, sy, sw, sh, shardColor);
      }
    }

    // Horizontal tear lines
    if (t > 0.2f) {
      int tearY = trRand() % SCREEN_H;
      tft.drawFastHLine(0, tearY, SCREEN_W, TR_CYAN);
      // Chromatic split
      if (tearY > 2) tft.drawFastHLine(0, tearY - 2, SCREEN_W / 3, 0xF800);  // Red offset
      if (tearY < SCREEN_H - 2) tft.drawFastHLine(SCREEN_W * 2 / 3, tearY + 2, SCREEN_W / 3, 0x001F);  // Blue offset
    }

    // Scanline warp
    for (int sl = 0; sl < 3; sl++) {
      int sly = trRand() % SCREEN_H;
      tft.drawFastHLine(0, sly, SCREEN_W, blendColor(TR_BASE, TR_NEUTRAL, 0.08f));
    }

    delay(30);
  }
}

// ═══════════════════════════════════════════════════════════
// PHASE 3: MORPH (650ms)
// Data ring expands, destination colors bleed in, text types
// ═══════════════════════════════════════════════════════════

void phaseMorph(uint16_t toColor, const char* themeName, int durationMs) {
  unsigned long start = millis();
  int cx = SCREEN_W / 2;
  int cy = SCREEN_H / 2;
  tft.fillScreen(TR_BASE);

  while (millis() - start < (unsigned long)durationMs) {
    float t = (float)(millis() - start) / durationMs;

    // Data ring — expanding concentric arcs
    int ringR = 20 + (int)(t * 60);
    uint16_t ringColor = blendColor(TR_PURPLE, toColor, t);
    tft.drawCircle(cx, cy, ringR, ringColor);
    tft.drawCircle(cx, cy, ringR - 2, blendColor(ringColor, TR_BASE, 0.5f));

    // Segmented ring markers
    for (int seg = 0; seg < 8; seg++) {
      float a = seg * PI / 4.0f + t * 2.0f;
      int px = cx + (int)(cos(a) * ringR);
      int py = cy + (int)(sin(a) * ringR);
      if (px > 2 && px < SCREEN_W - 2 && py > 2 && py < SCREEN_H - 2) {
        tft.fillRect(px - 1, py - 1, 3, 3, TR_CYAN);
      }
    }

    // Destination color bleed — colored rects seeding from center
    if (t > 0.3f) {
      for (int b = 0; b < 5; b++) {
        int bx = cx - 40 + (int)(trRand() % 80);
        int by = cy - 30 + (int)(trRand() % 60);
        int bw = 3 + trRand() % 10;
        int bh = 2 + trRand() % 6;
        uint16_t bleedColor = blendColor(TR_BASE, toColor, t * 0.4f);
        tft.fillRect(bx, by, bw, bh, bleedColor);
      }
    }

    // Typing text — theme name
    if (t > 0.4f) {
      int charsToShow = (int)((t - 0.4f) / 0.6f * strlen(themeName));
      charsToShow = min(charsToShow, (int)strlen(themeName));
      char buf[17];
      strncpy(buf, themeName, charsToShow);
      buf[charsToShow] = '\0';

      tft.setTextColor(TR_CYAN, TR_BASE);
      tft.fillRect(cx - 50, cy + 50, 100, 16, TR_BASE);
      tft.drawCentreString(buf, cx, cy + 50, 2);
    }

    // Status line at bottom
    int lineIdx = min((int)(t * 3), 2);
    tft.setTextColor(TR_NEUTRAL, TR_BASE);
    tft.fillRect(0, SCREEN_H - 16, SCREEN_W, 16, TR_BASE);
    tft.drawCentreString(TR_LINES[lineIdx], cx, SCREEN_H - 14, 1);

    // Pulse ring — bright flash at t=0.5
    if (t > 0.48f && t < 0.55f) {
      int flashR = ringR + 10;
      tft.drawCircle(cx, cy, flashR, TFT_WHITE);
    }

    // Floating particles — upward drift
    for (int p = 0; p < 8; p++) {
      int px = trRand() % SCREEN_W;
      int py = SCREEN_H - (int)(t * SCREEN_H * 0.6f) - (trRand() % 40);
      if (py > 0 && py < SCREEN_H) {
        uint16_t pColor = (trRand() % 2) ? TR_PURPLE : TR_CYAN;
        tft.drawPixel(px, py, pColor);
        if (trRand() % 3 == 0) tft.drawPixel(px, py - 1, pColor);  // Trail
      }
    }

    delay(30);
  }
}

// ═══════════════════════════════════════════════════════════
// PHASE 4: STITCH (450ms)
// Shards snap back into place, spring settle, sparks
// ═══════════════════════════════════════════════════════════

void phaseStitch(uint16_t toColor, int durationMs) {
  unsigned long start = millis();
  int cx = SCREEN_W / 2;
  int cy = SCREEN_H / 2;

  while (millis() - start < (unsigned long)durationMs) {
    float t = (float)(millis() - start) / durationMs;
    float easeT = t * t * (3.0f - 2.0f * t);  // smoothstep

    // Background fills with destination color from center outward
    int fillR = (int)(easeT * 200);
    tft.fillCircle(cx, cy, fillR, blendColor(TR_BASE, toColor, easeT * 0.3f));

    // Glow traces — lines from corners converging to center
    float invT = 1.0f - easeT;
    uint16_t traceColor = blendColor(toColor, TR_GLOW, invT);

    int endX = cx + (int)((0 - cx) * invT);
    int endY = cy + (int)((0 - cy) * invT);
    tft.drawLine(endX, endY, cx, cy, traceColor);

    endX = cx + (int)((SCREEN_W - cx) * invT);
    endY = cy + (int)((SCREEN_H - cy) * invT);
    tft.drawLine(endX, endY, cx, cy, traceColor);

    // Spark particles at convergence point
    if (easeT > 0.3f && easeT < 0.8f) {
      for (int sp = 0; sp < 6; sp++) {
        float sparkAngle = (trRand() % 360) * PI / 180.0f;
        float sparkDist = (1.0f - easeT) * 30 + trRand() % 10;
        int spx = cx + (int)(cos(sparkAngle) * sparkDist);
        int spy = cy + (int)(sin(sparkAngle) * sparkDist);
        if (spx > 0 && spx < SCREEN_W && spy > 0 && spy < SCREEN_H) {
          tft.drawPixel(spx, spy, TR_CYAN);
          tft.drawPixel(spx + 1, spy, TR_GLOW);
        }
      }
    }

    // HUD frame tightens
    int frameInset = 4 + (int)(invT * 30);
    tft.drawRect(frameInset, frameInset, SCREEN_W - frameInset * 2, SCREEN_H - frameInset * 2,
                 blendColor(TR_NEUTRAL, toColor, easeT));

    // Status text
    tft.setTextColor(blendColor(TR_NEUTRAL, toColor, easeT), TR_BASE);
    tft.drawCentreString(TR_LINES[3], cx, SCREEN_H - 14, 1);

    delay(30);
  }
}

// ═══════════════════════════════════════════════════════════
// PHASE 5: HANDOFF (300ms)
// Final neon flash, fade into destination, eyes settle
// ═══════════════════════════════════════════════════════════

void phaseHandoff(uint16_t toColor, uint16_t toBg, int durationMs) {
  unsigned long start = millis();
  int cx = SCREEN_W / 2;

  // Brief neon flash
  tft.fillScreen(blendColor(TR_BASE, toColor, 0.3f));
  delay(60);

  while (millis() - start < (unsigned long)durationMs) {
    float t = (float)(millis() - start) / durationMs;
    float easeT = t * t;  // easeIn

    // Fade from transition base to destination background
    uint16_t bgFade = blendColor(blendColor(TR_BASE, toColor, 0.3f), toBg, easeT);

    // Draw horizontal bands fading to destination
    for (int y = 0; y < SCREEN_H; y += 4) {
      tft.drawFastHLine(0, y, SCREEN_W, bgFade);
      tft.drawFastHLine(0, y + 1, SCREEN_W, bgFade);
      tft.drawFastHLine(0, y + 2, SCREEN_W, bgFade);
      tft.drawFastHLine(0, y + 3, SCREEN_W, bgFade);
    }

    // Final shimmer — tiny residual glitch pixels
    if (t < 0.6f) {
      for (int g = 0; g < 4; g++) {
        int gx = trRand() % SCREEN_W;
        int gy = trRand() % SCREEN_H;
        tft.drawPixel(gx, gy, TR_CYAN);
      }
    }

    // Status text fades out
    if (t < 0.5f) {
      tft.setTextColor(blendColor(TR_NEUTRAL, toBg, t * 2.0f));
      tft.drawCentreString(TR_LINES[4], cx, SCREEN_H - 14, 1);
    }

    delay(25);
  }

  // Final clean fill to destination background
  tft.fillScreen(toBg);
}

// ═══════════════════════════════════════════════════════════
// MASTER TRANSITION FUNCTION
// ═══════════════════════════════════════════════════════════

void playThemeTransition(uint8_t fromId, uint8_t toId) {
  // Read colors from PROGMEM
  uint16_t fromPrimary = getThemePrimaryColor(fromId);
  uint16_t toPrimary   = getThemePrimaryColor(toId);

  // Read destination bg
  ThemeDef toDef;
  memcpy_P(&toDef, &THEMES[toId], sizeof(ThemeDef));
  uint16_t toBg = toDef.bg;

  // Get destination theme name
  char toName[16];
  getThemeName(toId, toName, sizeof(toName));

  // Run 5 phases
  phaseCapture(fromPrimary, 350);
  phaseShred(fromPrimary, 500);
  phaseMorph(toPrimary, toName, 650);
  phaseStitch(toPrimary, 450);
  phaseHandoff(toPrimary, toBg, 300);

  // Now load the new theme into RAM
  currentThemeId = toId;
  loadTheme(toId);
  saveThemeToEEPROM();
}

#endif
