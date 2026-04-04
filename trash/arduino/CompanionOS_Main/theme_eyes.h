#ifndef THEME_EYES_H
#define THEME_EYES_H

#include "globals.h"
#include "themes.h"
#include <math.h>

// ═══════════════════════════════════════════════════════════
// THEME EYE RENDERERS V5 — 9 Completely Unique Eye Systems
//
// Each function is a standalone eye renderer with its own:
//   - Eye geometry (circles, diamonds, rings, lines, blobs, etc.)
//   - Pupil behavior
//   - Emotion-to-shape mapping
//   - Poke reactions
//   - Idle motion patterns
//
// These do NOT wrap or modify the original V3/V4 almond eyes.
// They share the existing eyeSpr sprite buffer for tear-free rendering.
// ═══════════════════════════════════════════════════════════

extern TFT_eSprite eyeSpr;
extern bool eyeSprAllocated;
extern int eyeSprAllocW, eyeSprAllocH;
extern float pupilOffsetX, pupilOffsetY;
extern Emotion currentEmotion;
extern uint16_t blendColor(uint16_t c1, uint16_t c2, float t);

// Shared layout constants
#define THEME_EYE_Y     110
#define THEME_LEFT_X    95
#define THEME_RIGHT_X   225

// ── Sprite helper: ensure sprite is allocated ────────────
void ensureEyeSprite(int w, int h) {
  if (!eyeSprAllocated || w > eyeSprAllocW || h > eyeSprAllocH) {
    if (eyeSprAllocated) eyeSpr.deleteSprite();
    eyeSprAllocW = w + 10;
    eyeSprAllocH = h + 10;
    eyeSpr.createSprite(eyeSprAllocW, eyeSprAllocH);
    eyeSprAllocated = true;
  }
}

// ── Theme-specific background dispatcher ─────────────────
void drawThemeBackground() {
  uint16_t bg = activeTheme.bg;

  switch (activeTheme.bgStyle) {
    case BG_STYLE_SOLID:
      // Solid fill — only first frame needed
      break;

    case BG_STYLE_GRID: {
      // Perspective wireframe grid — Gaming theme
      int cx = SCREEN_W / 2;
      int cy = SCREEN_H / 2 + 30;
      uint16_t gridColor = blendColor(activeTheme.bg, activeTheme.primary, 0.12f);
      // Horizontal converging lines
      for (int i = -5; i <= 5; i++) {
        int x1 = cx + i * 40;
        tft.drawLine(x1, SCREEN_H, cx, cy - 40, gridColor);
      }
      // Horizontal depth lines
      for (int j = 0; j < 6; j++) {
        int y = cy + j * 20;
        if (y < SCREEN_H) {
          float spread = (float)(j + 1) / 6.0f;
          int x1 = cx - (int)(160 * spread);
          int x2 = cx + (int)(160 * spread);
          tft.drawFastHLine(x1, y, x2 - x1, gridColor);
        }
      }
      break;
    }

    case BG_STYLE_GRADIENT: {
      // Vertical gradient — Companion theme
      uint16_t top = activeTheme.bg;
      uint16_t bot = blendColor(activeTheme.bg, activeTheme.primary, 0.15f);
      for (int y = 16; y < SCREEN_H; y += 4) {
        float t = (float)(y - 16) / (SCREEN_H - 16);
        uint16_t lineColor = blendColor(top, bot, t);
        tft.drawFastHLine(0, y, SCREEN_W, lineColor);
        tft.drawFastHLine(0, y + 1, SCREEN_W, lineColor);
        tft.drawFastHLine(0, y + 2, SCREEN_W, lineColor);
        tft.drawFastHLine(0, y + 3, SCREEN_W, lineColor);
      }
      break;
    }

    case BG_STYLE_NOISE: {
      // Animated noise grain — Angry theme
      uint32_t seed = millis() / 50;
      for (int i = 0; i < 30; i++) {
        seed = seed * 1103515245 + 12345 + i;
        int nx = seed % SCREEN_W;
        seed = seed * 1103515245 + 12345;
        int ny = 16 + (seed % (SCREEN_H - 20));
        uint16_t noiseColor = blendColor(activeTheme.bg, activeTheme.primary, 0.08f);
        tft.drawPixel(nx, ny, noiseColor);
      }
      break;
    }

    case BG_STYLE_FLUID: {
      // Slow shifting gradient bands — Mood theme
      float t = millis() * 0.0002f;
      for (int x = 0; x < SCREEN_W; x += 6) {
        float wave = sin(x * 0.02f + t) * 0.5f + 0.5f;
        uint16_t bandColor = blendColor(activeTheme.eyeC1, activeTheme.eyeC2, wave);
        bandColor = blendColor(activeTheme.bg, bandColor, 0.12f);
        int bandY = 60 + (int)(sin(x * 0.03f + t * 1.5f) * 20);
        int bandH = 8 + (int)(wave * 12);
        if (bandY > 16 && bandY + bandH < SCREEN_H) {
          tft.drawFastVLine(x, bandY, bandH, bandColor);
        }
      }
      break;
    }

    case BG_STYLE_VOID:
      // Pure black — Sleep theme (nothing to draw)
      break;

    default:
      break;
  }
}

// ═══════════════════════════════════════════════════════════
// THEME 2: PIKACHU EYES — Large Filled Circles
// JSON: large_circles, fast_and_irregular blinks, snappy darting
// Cheeks: red filled circles below eyes
// ═══════════════════════════════════════════════════════════

void drawPikachuEyes() {
  int eyeR = 30;  // Large circle radius
  int pupilR = 8;

  // Emotion modifiers
  int topSquish = 0;
  bool sparkle = false;
  bool cheeksOn = true;

  switch (currentEmotion) {
    case EMO_HAPPY:    topSquish = 10; break;
    case EMO_EXCITED:  topSquish = -5; sparkle = true; break;
    case EMO_SLEEPY:   topSquish = 18; break;
    case EMO_SAD:      topSquish = 5; break;
    case EMO_ANGRY:    topSquish = 12; cheeksOn = false; break;
    case EMO_SURPRISED: eyeR = 36; break;
    default: break;
  }

  // Clear eye zone
  tft.fillRect(THEME_LEFT_X - 45, THEME_EYE_Y - 45, 90, 90, activeTheme.bg);
  tft.fillRect(THEME_RIGHT_X - 45, THEME_EYE_Y - 45, 90, 90, activeTheme.bg);

  // Left eye — filled circle with gradient
  tft.fillCircle(THEME_LEFT_X, THEME_EYE_Y, eyeR, activeTheme.eyeC3);
  tft.fillCircle(THEME_LEFT_X, THEME_EYE_Y + 2, eyeR - 4, activeTheme.eyeC2);

  // Right eye
  tft.fillCircle(THEME_RIGHT_X, THEME_EYE_Y, eyeR, activeTheme.eyeC3);
  tft.fillCircle(THEME_RIGHT_X, THEME_EYE_Y + 2, eyeR - 4, activeTheme.eyeC2);

  // Squish top for happy/sleepy (draw bg rect over top portion)
  if (topSquish > 0) {
    tft.fillRect(THEME_LEFT_X - eyeR - 2, THEME_EYE_Y - eyeR - 2, eyeR * 2 + 4, topSquish, activeTheme.bg);
    tft.fillRect(THEME_RIGHT_X - eyeR - 2, THEME_EYE_Y - eyeR - 2, eyeR * 2 + 4, topSquish, activeTheme.bg);
  }

  // Pupils — small black circles with white catchlights
  int lpx = THEME_LEFT_X + (int)(pupilOffsetX * 0.6f);
  int lpy = THEME_EYE_Y + (int)(pupilOffsetY * 0.4f);
  int rpx = THEME_RIGHT_X + (int)(pupilOffsetX * 0.6f);
  int rpy = THEME_EYE_Y + (int)(pupilOffsetY * 0.4f);

  tft.fillCircle(lpx, lpy, pupilR, TFT_BLACK);
  tft.fillCircle(rpx, rpy, pupilR, TFT_BLACK);

  // White catchlight dots — darting
  tft.fillCircle(lpx + 3, lpy - 4, 3, TFT_WHITE);
  tft.fillCircle(lpx + 2, lpy - 6, 2, TFT_WHITE);
  tft.fillCircle(rpx + 3, rpy - 4, 3, TFT_WHITE);
  tft.fillCircle(rpx + 2, rpy - 6, 2, TFT_WHITE);

  // Red cheeks (Pikachu signature)
  if (cheeksOn) {
    tft.fillCircle(THEME_LEFT_X - 35, THEME_EYE_Y + 20, 10, activeTheme.cheekColor);
    tft.fillCircle(THEME_RIGHT_X + 35, THEME_EYE_Y + 20, 10, activeTheme.cheekColor);
  }

  // Sparkle effect for excited
  if (sparkle) {
    int spots[][2] = {{-40, -30}, {45, -25}, {-35, 35}, {40, 30}};
    for (int i = 0; i < 4; i++) {
      int sx = SCREEN_W / 2 + spots[i][0];
      int sy = THEME_EYE_Y + spots[i][1];
      tft.drawPixel(sx, sy, TFT_WHITE);
      tft.drawPixel(sx - 1, sy, TFT_WHITE);
      tft.drawPixel(sx + 1, sy, TFT_WHITE);
      tft.drawPixel(sx, sy - 1, TFT_WHITE);
      tft.drawPixel(sx, sy + 1, TFT_WHITE);
    }
  }
}

// ═══════════════════════════════════════════════════════════
// THEME 3: CHILL EYES — Soft Half-Circles (Bottom-Heavy)
// JSON: soft_half_circles_bottom_heavy, lazy fade blinks
// ═══════════════════════════════════════════════════════════

void drawChillEyes() {
  int eyeW = 50;
  int eyeH = 22;

  // Emotion modifiers
  float lidDroop = 0.0f;
  switch (currentEmotion) {
    case EMO_SLEEPY:   lidDroop = 0.6f; break;
    case EMO_HAPPY:    lidDroop = 0.2f; eyeH = 18; break;
    case EMO_SAD:      lidDroop = 0.4f; break;
    default: lidDroop = 0.1f; break;
  }

  int effectiveH = eyeH - (int)(lidDroop * eyeH);

  for (int side = 0; side < 2; side++) {
    int cx = (side == 0) ? THEME_LEFT_X : THEME_RIGHT_X;

    // Clear area
    tft.fillRect(cx - eyeW / 2 - 5, THEME_EYE_Y - eyeH - 5, eyeW + 10, eyeH * 2 + 10, activeTheme.bg);

    // Soft half-circle — bottom-heavy using filled ellipse approximation
    for (int x = -eyeW / 2; x <= eyeW / 2; x++) {
      float nx = (float)x / (eyeW / 2.0f);
      float curve = sqrt(max(0.0f, 1.0f - nx * nx));
      int top = THEME_EYE_Y - (int)(effectiveH * curve * 0.3f);  // Flat top
      int bot = THEME_EYE_Y + (int)(effectiveH * curve);          // Full bottom
      if (bot > top) {
        for (int y = top; y <= bot; y++) {
          float t = (float)(y - top) / (bot - top);
          uint16_t col = blendColor(activeTheme.eyeC1, activeTheme.eyeC3, t);
          tft.drawPixel(cx + x, y, col);
        }
      }
    }

    // Slow drifting glow pupil
    int px = cx + (int)(pupilOffsetX * 0.3f);
    int py = THEME_EYE_Y + 4 + (int)(pupilOffsetY * 0.2f);
    // Diffuse glow instead of sharp pupil
    uint16_t glowColor = blendColor(activeTheme.eyeC2, TFT_WHITE, 0.4f);
    tft.fillCircle(px, py, 6, glowColor);
    tft.fillCircle(px, py, 3, blendColor(glowColor, TFT_WHITE, 0.5f));
  }
}

// ═══════════════════════════════════════════════════════════
// THEME 4: GAMING EYES — Sharp Diamond Slits + Neon Glow
// JSON: sharp_diamond_slits, crosshair_target_lock, CRT scanlines
// ═══════════════════════════════════════════════════════════

void drawGamingEyes() {
  int diamW = 18;  // Half-width of diamond
  int diamH = 28;  // Half-height of diamond

  // Emotion modifiers
  bool crosshairMode = (currentEmotion == EMO_NEUTRAL || currentEmotion == EMO_ANGRY);

  for (int side = 0; side < 2; side++) {
    int cx = (side == 0) ? THEME_LEFT_X : THEME_RIGHT_X;
    int cy = THEME_EYE_Y;

    // Clear area
    tft.fillRect(cx - diamW - 8, cy - diamH - 8, diamW * 2 + 16, diamH * 2 + 16, activeTheme.bg);

    // Neon glow outline — outer diamond (2px thick)
    tft.drawLine(cx, cy - diamH - 3, cx + diamW + 3, cy, activeTheme.eyeC1);
    tft.drawLine(cx + diamW + 3, cy, cx, cy + diamH + 3, activeTheme.eyeC1);
    tft.drawLine(cx, cy + diamH + 3, cx - diamW - 3, cy, activeTheme.eyeC1);
    tft.drawLine(cx - diamW - 3, cy, cx, cy - diamH - 3, activeTheme.eyeC1);

    // Inner diamond filled — sharp 1px aliased
    tft.fillTriangle(cx, cy - diamH, cx + diamW, cy, cx, cy + diamH, activeTheme.eyeC2);
    tft.fillTriangle(cx, cy - diamH, cx - diamW, cy, cx, cy + diamH, activeTheme.eyeC2);

    // Center slit pupil — horizontal line
    int px = cx + (int)(pupilOffsetX * 0.8f);
    int py = cy + (int)(pupilOffsetY * 0.5f);
    tft.drawFastHLine(px - 6, py, 12, TFT_BLACK);
    tft.drawFastHLine(px - 4, py - 1, 8, TFT_BLACK);
    tft.drawFastHLine(px - 4, py + 1, 8, TFT_BLACK);

    // Crosshair overlay
    if (crosshairMode) {
      tft.drawFastHLine(cx - diamW + 4, cy, diamW * 2 - 8, activeTheme.primary);
      tft.drawFastVLine(cx, cy - diamH + 6, diamH * 2 - 12, activeTheme.primary);
      // Target dot center
      tft.fillCircle(px, py, 2, activeTheme.primary);
    }

    // CRT scanlines over eye — every 2nd line dimmed
    if (activeTheme.enableScanlines) {
      for (int sy = cy - diamH; sy <= cy + diamH; sy += 2) {
        tft.drawFastHLine(cx - diamW, sy, diamW * 2, blendColor(activeTheme.eyeC2, activeTheme.bg, 0.15f));
      }
    }
  }
}

// ═══════════════════════════════════════════════════════════
// THEME 5: MINIMAL EYES — Hollow Thin Rings + Dot Pupils
// JSON: hollow_perfect_rings, solid_black_dot_centered, completely still
// ═══════════════════════════════════════════════════════════

void drawMinimalEyes() {
  int ringR = 24;
  int ringThick = 2;
  int dotR = 4;

  // Emotion modifiers — subtle
  switch (currentEmotion) {
    case EMO_HAPPY:    break;  // Slight upward shift handled by pupil
    case EMO_SLEEPY:   ringR = 20; break;  // Smaller rings
    case EMO_ANGRY:    ringThick = 1; ringR = 28; break;  // Thin + wide
    case EMO_SURPRISED: ringR = 30; dotR = 2; break;  // Wide + tiny dot
    default: break;
  }

  for (int side = 0; side < 2; side++) {
    int cx = (side == 0) ? THEME_LEFT_X : THEME_RIGHT_X;
    int cy = THEME_EYE_Y;

    // Clear area
    tft.fillRect(cx - ringR - 5, cy - ringR - 5, ringR * 2 + 10, ringR * 2 + 10, activeTheme.bg);

    // Hollow ring — concentric circles
    for (int t = 0; t < ringThick; t++) {
      tft.drawCircle(cx, cy, ringR - t, activeTheme.eyeC1);
    }

    // Solid dot pupil — smooth tracking but subtle
    int px = cx + (int)(pupilOffsetX * 0.3f);
    int py = cy + (int)(pupilOffsetY * 0.2f);
    tft.fillCircle(px, py, dotR, activeTheme.eyeC1);
  }
}

// ═══════════════════════════════════════════════════════════
// THEME 6: ANGRY EYES — Sharp Inward-Pointing Triangles
// JSON: sharp_inward_pointing_triangles, tiny shaking dots,
//       constant shivering wobble, screen shake
// ═══════════════════════════════════════════════════════════

void drawAngryThemeEyes() {
  // Screen shake offset
  int shakeX = 0, shakeY = 0;
  if (activeTheme.enableScreenShake) {
    shakeX = (random(5) - 2);
    shakeY = (random(3) - 1);
  }

  int triW = 35;  // Half-width
  int triH = 30;  // Full height

  for (int side = 0; side < 2; side++) {
    int cx = (side == 0) ? THEME_LEFT_X + shakeX : THEME_RIGHT_X + shakeX;
    int cy = THEME_EYE_Y + shakeY;

    // Clear area
    tft.fillRect(cx - triW - 5, cy - triH / 2 - 10, triW * 2 + 10, triH + 20, activeTheme.bg);

    // Inward-pointing triangles — sharp V-shape frown
    // Left eye: peak points right-inward; Right eye: peak points left-inward
    int peakX, baseTopX, baseBotX;
    if (side == 0) {
      // Left eye: angry inward slant (top-left to bottom-right)
      peakX = cx + triW / 2;
      baseTopX = cx - triW;
      baseBotX = cx - triW / 2;
    } else {
      // Right eye: mirror
      peakX = cx - triW / 2;
      baseTopX = cx + triW;
      baseBotX = cx + triW / 2;
    }

    int topY = cy - triH / 2 - 5;
    int botY = cy + triH / 2;

    // Filled angry triangle
    tft.fillTriangle(baseTopX, topY, baseBotX, botY, peakX, cy, activeTheme.eyeC1);

    // Red glow border
    tft.drawTriangle(baseTopX, topY, baseBotX, botY, peakX, cy, activeTheme.primary);

    // Tiny shaking pupil dot
    int px = cx + (int)(pupilOffsetX * 0.4f) + (random(3) - 1);
    int py = cy + (int)(pupilOffsetY * 0.3f) + (random(3) - 1);
    tft.fillCircle(px, py, 3, TFT_BLACK);
    tft.fillCircle(px, py, 1, activeTheme.eyeC3);
  }

  // Red vignette at edges
  for (int v = 0; v < 15; v++) {
    uint16_t vigColor = blendColor(activeTheme.bg, activeTheme.primary, (15.0f - v) / 15.0f * 0.2f);
    tft.drawFastHLine(0, 16 + v, SCREEN_W, vigColor);
    tft.drawFastHLine(0, SCREEN_H - 10 - v, SCREEN_W, vigColor);
  }
}

// ═══════════════════════════════════════════════════════════
// THEME 7: SLEEP EYES — Flat Horizontal Lines (Closed)
// JSON: closed_flat_horizontal_lines, 10s deep sine breathing
// ═══════════════════════════════════════════════════════════

void drawSleepEyes() {
  // 10-second deep sine breathing cycle
  float breathPhase = sin(millis() * 0.000628f);  // ~10s period
  int breathOffset = (int)(breathPhase * 3.0f);    // ±3px vertical heave

  int lineW = 40;
  int lineY = THEME_EYE_Y + breathOffset;

  // Only clear a narrow band around the eyes
  tft.fillRect(THEME_LEFT_X - lineW - 5, lineY - 8, lineW * 2 + 10, 16, activeTheme.bg);
  tft.fillRect(THEME_RIGHT_X - lineW - 5, lineY - 8, lineW * 2 + 10, 16, activeTheme.bg);

  // Emotion modifiers — all map to variations of closed
  uint16_t lineColor = activeTheme.eyeC1;
  int lineThick = 2;

  switch (currentEmotion) {
    case EMO_HAPPY:
      // Closed smile curve — slight upward arc
      for (int x = -lineW; x <= lineW; x++) {
        float curve = -2.0f * (1.0f - (float)(x * x) / (float)(lineW * lineW));
        tft.drawPixel(THEME_LEFT_X + x, lineY + (int)curve, lineColor);
        tft.drawPixel(THEME_LEFT_X + x, lineY + (int)curve + 1, lineColor);
        tft.drawPixel(THEME_RIGHT_X + x, lineY + (int)curve, lineColor);
        tft.drawPixel(THEME_RIGHT_X + x, lineY + (int)curve + 1, lineColor);
      }
      return;

    case EMO_ANGRY:
      lineColor = blendColor(activeTheme.eyeC1, 0xF800, 0.3f);  // Reddish
      break;

    default: break;
  }

  // Default: flat horizontal lines
  for (int t = 0; t < lineThick; t++) {
    tft.drawFastHLine(THEME_LEFT_X - lineW, lineY + t, lineW * 2, lineColor);
    tft.drawFastHLine(THEME_RIGHT_X - lineW, lineY + t, lineW * 2, lineColor);
  }
}

// ═══════════════════════════════════════════════════════════
// THEME 8: MOOD EYES — Morphing Fluid Blobs
// JSON: morphing_fluid_blobs_like_lava_lamp, liquid merge blinks
// Uses overlapping filled circles to simulate metaball shapes
// ═══════════════════════════════════════════════════════════

void drawMoodEyes() {
  float t = millis() * 0.001f;  // Seconds

  // Dynamic color based on time-of-day palette shift
  float hueShift = sin(t * 0.1f) * 0.5f + 0.5f;
  uint16_t blobColor1 = blendColor(activeTheme.eyeC1, activeTheme.eyeC2, hueShift);
  uint16_t blobColor2 = blendColor(activeTheme.eyeC2, activeTheme.eyeC3, 1.0f - hueShift);

  for (int side = 0; side < 2; side++) {
    int cx = (side == 0) ? THEME_LEFT_X : THEME_RIGHT_X;
    int cy = THEME_EYE_Y;

    // Clear area
    tft.fillRect(cx - 40, cy - 35, 80, 70, activeTheme.bg);

    // Blob 1 — main body, morphing radius
    float morph1 = sin(t * 0.8f + side * 1.5f) * 4.0f;
    int r1 = 22 + (int)morph1;
    float dx1 = sin(t * 0.5f) * 4.0f;
    float dy1 = cos(t * 0.7f) * 3.0f;
    tft.fillCircle(cx + (int)dx1, cy + (int)dy1, r1, blobColor1);

    // Blob 2 — smaller, orbiting
    float morph2 = cos(t * 1.1f + side * 2.0f) * 3.0f;
    int r2 = 14 + (int)morph2;
    float dx2 = sin(t * 0.6f + 2.0f) * 10.0f;
    float dy2 = cos(t * 0.4f + 1.0f) * 8.0f;
    tft.fillCircle(cx + (int)dx2, cy + (int)dy2, r2, blobColor2);

    // Blob 3 — tiny accent blob
    int r3 = 8 + (int)(sin(t * 1.3f) * 3.0f);
    float dx3 = cos(t * 0.9f) * 6.0f;
    float dy3 = sin(t * 0.5f + 3.0f) * 5.0f;
    tft.fillCircle(cx + (int)dx3, cy + (int)dy3, r3, blendColor(blobColor1, blobColor2, 0.5f));

    // Inner pupil — darker colored blob floating inside
    int px = cx + (int)(pupilOffsetX * 0.3f + dx1 * 0.3f);
    int py = cy + (int)(pupilOffsetY * 0.2f + dy1 * 0.3f);
    uint16_t pupilColor = blendColor(blobColor1, activeTheme.bg, 0.6f);
    tft.fillCircle(px, py, 5, pupilColor);
  }
}

// ═══════════════════════════════════════════════════════════
// THEME 9: SYSTEM EYES — Text Telemetry (No Eyes)
// JSON: replaced entirely by raw data graphs / text
// Shows: RAM, FPS, WiFi RSSI, Heap, Frame time
// ═══════════════════════════════════════════════════════════

void drawSystemEyes() {
  static unsigned long lastSystemDraw = 0;
  static int frameCount = 0;
  static int displayFPS = 0;
  static unsigned long fpsTimer = 0;

  frameCount++;
  if (millis() - fpsTimer >= 1000) {
    displayFPS = frameCount;
    frameCount = 0;
    fpsTimer = millis();
  }

  // Only update once per second to save CPU
  if (millis() - lastSystemDraw < 500) return;
  lastSystemDraw = millis();

  // Clear diagnostic area
  tft.fillRect(0, 20, SCREEN_W, SCREEN_H - 40, activeTheme.bg);

  int y = 24;
  int lx = 10;

  // Title
  tft.setTextColor(activeTheme.accent, activeTheme.bg);
  tft.drawString("SYSTEM DIAGNOSTICS", lx, y, 2);
  y += 22;

  // RAM
  tft.setTextColor(activeTheme.primary, activeTheme.bg);
  tft.drawString("HEAP:", lx, y, 1);
  tft.setTextColor(activeTheme.text, activeTheme.bg);
  char ramBuf[24];
  sprintf(ramBuf, "%d bytes free", ESP.getFreeHeap());
  tft.drawString(ramBuf, 45, y, 1);
  // RAM bar
  int ramPercent = map(ESP.getFreeHeap(), 0, 80000, 0, 120);
  ramPercent = constrain(ramPercent, 0, 120);
  tft.fillRect(180, y, ramPercent, 8, TR_SUCCESS);
  tft.fillRect(180 + ramPercent, y, 120 - ramPercent, 8, 0x4208);
  y += 16;

  // FPS
  tft.setTextColor(activeTheme.primary, activeTheme.bg);
  tft.drawString("FPS:", lx, y, 1);
  tft.setTextColor(activeTheme.text, activeTheme.bg);
  char fpsBuf[8];
  sprintf(fpsBuf, "%d", displayFPS);
  tft.drawString(fpsBuf, 35, y, 1);
  y += 16;

  // WiFi RSSI
  tft.setTextColor(activeTheme.primary, activeTheme.bg);
  tft.drawString("RSSI:", lx, y, 1);
  tft.setTextColor(activeTheme.text, activeTheme.bg);
  char rssiBuf[16];
  sprintf(rssiBuf, "%d dBm", WiFi.RSSI());
  tft.drawString(rssiBuf, 45, y, 1);
  // RSSI bar
  int rssiBar = map(WiFi.RSSI(), -100, -30, 0, 120);
  rssiBar = constrain(rssiBar, 0, 120);
  tft.fillRect(180, y, rssiBar, 8, TR_CYAN);
  tft.fillRect(180 + rssiBar, y, 120 - rssiBar, 8, 0x4208);
  y += 16;

  // CPU
  tft.setTextColor(activeTheme.primary, activeTheme.bg);
  tft.drawString("CPU:", lx, y, 1);
  tft.setTextColor(activeTheme.text, activeTheme.bg);
  char cpuBuf[16];
  sprintf(cpuBuf, "%d MHz", ESP.getCpuFreqMHz());
  tft.drawString(cpuBuf, 35, y, 1);
  y += 16;

  // Theme ID
  tft.setTextColor(activeTheme.primary, activeTheme.bg);
  tft.drawString("THEME:", lx, y, 1);
  tft.setTextColor(activeTheme.accent, activeTheme.bg);
  char themeBuf[24];
  char tName[16];
  getThemeName(currentThemeId, tName, sizeof(tName));
  sprintf(themeBuf, "#%d %s", currentThemeId, tName);
  tft.drawString(themeBuf, 55, y, 1);
  y += 16;

  // Uptime
  unsigned long secs = millis() / 1000;
  tft.setTextColor(activeTheme.primary, activeTheme.bg);
  tft.drawString("UP:", lx, y, 1);
  tft.setTextColor(activeTheme.text, activeTheme.bg);
  char upBuf[16];
  sprintf(upBuf, "%02dh%02dm%02ds", (int)(secs / 3600), (int)((secs % 3600) / 60), (int)(secs % 60));
  tft.drawString(upBuf, 25, y, 1);
  y += 16;

  // LDR
  tft.setTextColor(activeTheme.primary, activeTheme.bg);
  tft.drawString("LDR:", lx, y, 1);
  tft.setTextColor(activeTheme.text, activeTheme.bg);
  char ldrBuf[8];
  sprintf(ldrBuf, "%d", ldrValue);
  tft.drawString(ldrBuf, 35, y, 1);
  int ldrBar = map(ldrValue, 0, 1024, 0, 120);
  tft.fillRect(180, y, ldrBar, 8, activeTheme.accent);
  tft.fillRect(180 + ldrBar, y, 120 - ldrBar, 8, 0x4208);
  y += 16;

  // Touch state
  tft.setTextColor(activeTheme.primary, activeTheme.bg);
  tft.drawString("TCH:", lx, y, 1);
  tft.setTextColor(digitalRead(TOUCH_LEFT) ? TR_SUCCESS : 0xF800, activeTheme.bg);
  tft.drawString(digitalRead(TOUCH_LEFT) ? "L:ON" : "L:--", 35, y, 1);
  tft.setTextColor(digitalRead(TOUCH_RIGHT) ? TR_SUCCESS : 0xF800, activeTheme.bg);
  tft.drawString(digitalRead(TOUCH_RIGHT) ? " R:ON" : " R:--", 70, y, 1);
}

// ═══════════════════════════════════════════════════════════
// THEME 10: COMPANION EYES — Warm Thick Ovals + Squash/Stretch
// JSON: soft_thick_ovals_with_dimples, saccadic darting,
//       squish_compress blinks, heart/tear/sweatdrop particles
// ═══════════════════════════════════════════════════════════

void drawCompanionEyes() {
  int ovalW = 38;
  int ovalH = 32;

  // Emotion-driven squash and stretch
  float squashX = 1.0f, squashY = 1.0f;
  bool showBlush = false;
  bool showTears = false;

  switch (currentEmotion) {
    case EMO_HAPPY:
      ovalH = 26;   // Squished happy
      showBlush = true;
      break;
    case EMO_EXCITED:
      squashY = 1.15f;  // Stretched tall
      break;
    case EMO_SAD:
      ovalH = 28;
      showTears = true;
      break;
    case EMO_SLEEPY:
      ovalH = 16;  // Very squished
      break;
    case EMO_SURPRISED:
      squashY = 1.3f;  // Very stretched
      squashX = 0.85f;
      break;
    case EMO_ANGRY:
      ovalH = 24;
      break;
    case EMO_LOVE:
      showBlush = true;
      break;
    default: break;
  }

  int finalW = (int)(ovalW * squashX);
  int finalH = (int)(ovalH * squashY);

  for (int side = 0; side < 2; side++) {
    int cx = (side == 0) ? THEME_LEFT_X : THEME_RIGHT_X;
    int cy = THEME_EYE_Y;

    // Clear eye zone
    tft.fillRect(cx - finalW - 8, cy - finalH - 8, finalW * 2 + 16, finalH * 2 + 16, activeTheme.bg);

    // Warm oval — thick body with gradient
    // Outer glow ring
    tft.fillEllipse(cx, cy, finalW + 3, finalH + 3, blendColor(activeTheme.bg, activeTheme.eyeC3, 0.3f));

    // Main oval body
    tft.fillEllipse(cx, cy, finalW, finalH, activeTheme.eyeC2);

    // Inner lighter gradient (top highlight)
    tft.fillEllipse(cx, cy - finalH / 4, finalW - 6, finalH / 2, activeTheme.eyeC3);

    // Pupil — distinct white catchlights that dart first
    int px = cx + (int)(pupilOffsetX * 0.5f);
    int py = cy + (int)(pupilOffsetY * 0.4f);

    // Iris circle
    tft.fillCircle(px, py, 9, blendColor(activeTheme.eyeC1, TFT_BLACK, 0.3f));
    // Pupil dot
    tft.fillCircle(px, py, 5, TFT_BLACK);

    // Catchlights (triple reflection for wet-eye look)
    tft.fillCircle(px + 3, py - 4, 3, TFT_WHITE);
    tft.fillCircle(px + 2, py - 6, 2, TFT_WHITE);
    tft.fillCircle(px - 2, py + 3, 1, blendColor(TFT_WHITE, activeTheme.eyeC3, 0.5f));

    // Dimple marks (tiny accent lines below outer corners)
    if (currentEmotion == EMO_HAPPY || showBlush) {
      int dimpleX = (side == 0) ? cx + finalW - 5 : cx - finalW + 5;
      tft.drawPixel(dimpleX, cy + finalH / 2, activeTheme.muted);
      tft.drawPixel(dimpleX, cy + finalH / 2 + 1, activeTheme.muted);
    }
  }

  // Blush cheeks
  if (showBlush && activeTheme.enableCheeks) {
    tft.fillCircle(THEME_LEFT_X - 30, THEME_EYE_Y + 25, 8, activeTheme.cheekColor);
    tft.fillCircle(THEME_RIGHT_X + 30, THEME_EYE_Y + 25, 8, activeTheme.cheekColor);
  }

  // Tear drops
  if (showTears) {
    float tearT = (millis() % 2000) / 2000.0f;
    int tearY = THEME_EYE_Y + 30 + (int)(tearT * 30);
    if (tearY < SCREEN_H - 10) {
      tft.fillCircle(THEME_LEFT_X + 10, tearY, 3, 0x5DDF);
      tft.fillCircle(THEME_LEFT_X + 10, tearY - 4, 2, 0x5DDF);
    }
  }
}

#endif
