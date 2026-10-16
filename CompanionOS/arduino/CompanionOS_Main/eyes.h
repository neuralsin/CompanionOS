#ifndef EYES_H
#define EYES_H

#include "globals.h"
#include "ui_components.h"
#include "thought_engine.h"
#include <math.h>

// ═══════════════════════════════════════════════════════════
// EYE ANIMATION ENGINE V3 — Enhanced Cat Eyes
//
// Features:
//   - 5-tier gradient fills for smooth color transitions 
//   - Outer glow rings for depth illusion
//   - Emotion decorators: eyebrows, tears, sparkles, mouths
//   - LDR-based mood: dim light = sleepy, bright = energetic
//   - Time-based defaults: night = sleepy, morning = happy
//   - Cat-like almond geometry with tilt
//   - Vertical pill pupils with dual reflection highlights
// ═══════════════════════════════════════════════════════════

// Emotion system now defined in globals.h
extern Emotion currentEmotion;
Emotion autoEmotion = EMO_NEUTRAL;  // Time/LDR suggested emotion

// ── Layout Constants (v7: SCALE-aware) ──────────────────
#define EYE_Y        (SCREEN_W > 200 ? SCALE_Y(110) : 70)
#define LEFT_EYE_X   (SCREEN_W > 200 ? SCALE_X(95) : 45)
#define RIGHT_EYE_X  (SCREEN_W > 200 ? SCALE_X(225) : 115)
#define EYE_W        (SCREEN_W > 200 ? 90 : 45)   // Full width of almond
#define EYE_H        (SCREEN_W > 200 ? 38 : 26)   // Baseline height
#define PUPIL_W      (SCREEN_W > 200 ? 10 : 6)
#define PUPIL_H      (SCREEN_W > 200 ? 28 : 20)

// ── Animation State ──────────────────────────────────────
extern unsigned long lastBlink;
extern bool isBlinking;
extern int blinkPhase;

float pupilOffsetX = 0;
float pupilOffsetY = 0;
float pupilTargetX = 0;
float pupilTargetY = 0;
float pupilVelX = 0;    // SUBMISSION 2: Spring-physics velocity
float pupilVelY = 0;    // SUBMISSION 2: Spring-physics velocity
unsigned long lastPupilMove = 0;

// SUBMISSION 2: Ambient starfield rendered into background zones
bool starfieldDrawn = false;
void drawStarfield() {
  // XOR-shift procedural starfield — zero RAM, no lookup tables
  // Uses 160MHz budget to compute all star positions mathematically
  uint32_t seed = 0xDEADBEEF;
  auto xorshift = [](uint32_t &s) -> uint32_t {
    s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s;
  };
  
  // Stars in the center zone between the two eyes
  for (int i = 0; i < 40; i++) {
    uint32_t rX = xorshift(seed) % SCREEN_W;
    uint32_t rY = xorshift(seed) % (SCREEN_H - 50) + 20;  // below status bar, above bottom
    uint8_t  brightness = xorshift(seed) % 3;
    
    // Skip pixels that overlap the eye bounding boxes
    bool inLeftEye  = (rX >= LEFT_EYE_X - EYE_W/2 - 8) && (rX <= LEFT_EYE_X + EYE_W/2 + 8)
                   && (rY >= EYE_Y - EYE_H - 8) && (rY <= EYE_Y + EYE_H + 8);
    bool inRightEye = (rX >= RIGHT_EYE_X - EYE_W/2 - 8) && (rX <= RIGHT_EYE_X + EYE_W/2 + 8)
                   && (rY >= EYE_Y - EYE_H - 8) && (rY <= EYE_Y + EYE_H + 8);
    
    if (!inLeftEye && !inRightEye && rY < (uint32_t)eyeRenderBottom()) {
      uint16_t col = brightness == 2 ? 0x4208 : brightness == 1 ? 0x2104 : 0x10A2;
      tft.drawPixel(rX, rY, col);
    }
  }
}

// Time state is now in globals.h
extern int displayHour;
extern int displayMinute;
extern bool timeReceived;

extern void drawStatusBar();

// blendColor() is defined in ui_components.h — using that version.
// Removed duplicate to avoid linker errors.

// ── Core Almond Eye Renderer ────────────────────────────
float currentBlinkMod = 1.0;

TFT_eSprite eyeSpr = TFT_eSprite(&tft);
bool eyeSprAllocated = false;
bool eyeSprAllocationAttempted = false;
TFT_eSprite zSpr(&tft);
bool zSprAttempted = false;

void releaseClassicEyeSprites() {
  eyeSpr.deleteSprite();
  eyeSprAllocated = false;
  eyeSprAllocationAttempted = false;
  zSpr.deleteSprite();
  zSprAttempted = false;
}

template <typename T>
void drawAlmondEyeOn(T* display, int scx, int scy, int w, int h, float pX, float pY,
                    uint16_t c1, uint16_t c2, uint16_t c3, float tiltL, float tiltR) {
  h = (int)((float)h * currentBlinkMod);
  if (h < 2) h = 2; // Prevent division by zero and inversion
  
  float halfW = w / 2.0;

  // Outer glow (2px around the eye shape)
  for (int ring = 2; ring >= 1; ring--) {
    uint16_t glowColor = blendColor(c3, COLOR_BG, 0.5 + ring * 0.2);
    for (int x = -(halfW + ring); x <= (halfW + ring); x++) {
      float nx = (float)x / (halfW + ring);
      float curve = 1.0 - (nx * nx);
      float tmod = (x < 0) ? (x * tiltL * 0.5) : (x * tiltR * 0.5);
      int yt = (int)(-(h + ring) * curve + tmod);
      int yb = (int)((h + ring) * curve - tmod);
      if (yb - yt > 0) {
        display->drawFastVLine(scx + x, scy + yt, yb - yt, glowColor);
      }
    }
  }
  
  // Main eye body with smooth 5-tier gradient
  for (int x = -halfW; x <= halfW; x++) {
    float nx = (float)x / halfW;
    float curve = 1.0 - (nx * nx);
    
    float tmod_top = (x < 0) ? (x * tiltL) : (x * tiltR);
    float tmod_bot = (x < 0) ? (x * tiltL) : (x * tiltR);

    int yt = (int)(-h * curve + tmod_top);
    int yb = (int)(h * curve - tmod_bot);
    int totalH = yb - yt;
    
    if (totalH > 0) {
      for (int py = 0; py < totalH; py++) {
        float t = (float)py / totalH;
        uint16_t color;
        if (t < 0.33) {
          color = blendColor(c1, c2, t / 0.33);
        } else if (t < 0.66) {
          color = blendColor(c2, c3, (t - 0.33) / 0.33);
        } else {
          color = blendColor(c3, c2, (t - 0.66) / 0.34);
        }
        display->drawPixel(scx + x, scy + yt + py, color);
      }
    }
  }

  if (h <= 2) return;

  // Vertical Cat Pupil
  int px = scx + (int)pX;
  int py = scy + (int)pY;
  
  float squish = (float)h / EYE_H;
  int p_h = (int)(PUPIL_H * squish);
  if (p_h < 2) p_h = 2;
  
  // Advanced Detailing: Rich Iris depth ring
  display->fillRoundRect(px - PUPIL_W/2 - 2, py - p_h/2 - 2, PUPIL_W + 4, p_h + 4, (PUPIL_W+4)/2, 0xFEA0);
  display->drawRoundRect(px - PUPIL_W/2 - 3, py - p_h/2 - 3, PUPIL_W + 6, p_h + 6, (PUPIL_W+6)/2, 0x8260);
  
  // Deep Black pill pupil
  display->fillRoundRect(px - PUPIL_W/2, py - p_h/2, PUPIL_W, p_h, PUPIL_W/2, TFT_BLACK);
  
  // Primary reflection (top-right curved glass highlight)
  if (squish > 0.3) {
    display->fillCircle(px + 4, py - (int)(6 * squish), 3, TFT_WHITE);
    display->fillCircle(px + 3, py - (int)(9 * squish), 2, TFT_WHITE);
    // Secondary bounce reflection (bottom-left)
    display->fillCircle(px - 2, py + (int)(6 * squish), 2, 0x6B4D);
  }

}

void drawAlmondEye(int cx, int cy, int w, int h, float pX, float pY,
                   uint16_t c1, uint16_t c2, uint16_t c3, float tiltL, float tiltR) {
  // A constant footprint covers every emotion and blink, erasing the previous
  // frame without reallocating or moving the sprite origin as the lids close.
  const int sprW = EYE_W + 16;
  const int sprH = (EYE_H + 18 + 8) * 2;
  if (!eyeSprAllocationAttempted) {
    eyeSprAllocationAttempted = true;
    eyeSpr.setColorDepth(16);
    eyeSprAllocated = eyeSpr.createSprite(sprW, sprH) != nullptr;
    if (!eyeSprAllocated) {
      eyeSpr.setColorDepth(8);
      eyeSprAllocated = eyeSpr.createSprite(sprW, sprH) != nullptr;
    }
  }
  if (eyeSprAllocated) {
    eyeSpr.fillSprite(COLOR_BG);
    drawAlmondEyeOn(&eyeSpr, sprW / 2, sprH / 2, w, h, pX, pY, c1, c2, c3, tiltL, tiltR);
    eyeSpr.pushSprite(cx - sprW / 2, cy - sprH / 2);
  } else {
    tft.fillRect(cx - sprW / 2, cy - sprH / 2, sprW, sprH, COLOR_BG);
    drawAlmondEyeOn(&tft, cx, cy, w, h, pX, pY, c1, c2, c3, tiltL, tiltR);
  }
}

// ═══════════════════════════════════════════════════════════
// EMOTION DECORATORS
// ═══════════════════════════════════════════════════════════

void drawEyebrows(int lx, int rx, int y, uint16_t color, bool angry) {
  if (angry) {
    // V-shaped angry brows
    tft.drawLine(lx - 25, y - 48, lx + 15, y - 40, color);
    tft.drawLine(lx - 25, y - 47, lx + 15, y - 39, color);
    tft.drawLine(rx - 15, y - 40, rx + 25, y - 48, color);
    tft.drawLine(rx - 15, y - 39, rx + 25, y - 47, color);
  } else {
    // Happy arched brows
    for (int i = -20; i <= 20; i++) {
      float arc = -5.0 * (1.0 - (float)(i*i) / 400.0);
      tft.drawPixel(lx + i, y - 45 + (int)arc, color);
      tft.drawPixel(lx + i, y - 44 + (int)arc, color);
      tft.drawPixel(rx + i, y - 45 + (int)arc, color);
      tft.drawPixel(rx + i, y - 44 + (int)arc, color);
    }
  }
}

// Tears and sparkles removed completely
void drawMouth(int cx, int cy, bool happy) {
  int my = cy + 55;
  if (happy) {
    // Small upward curve
    for (int i = -12; i <= 12; i++) {
      float curve = 3.0 * (1.0 - (float)(i*i) / 144.0);
      tft.drawPixel(cx + i, my + (int)curve, 0x8410);
    }
  } else {
    // Small downward curve
    for (int i = -10; i <= 10; i++) {
      float curve = -2.0 * (1.0 - (float)(i*i) / 100.0);
      tft.drawPixel(cx + i, my + (int)curve, 0x8410);
    }
  }
}

void drawHeartEyes(int cx, int cy, uint16_t color) {
  // Heart shape instead of almond
  tft.fillCircle(cx - 12, cy - 8, 12, color);
  tft.fillCircle(cx + 12, cy - 8, 12, color);
  tft.fillTriangle(cx - 24, cy - 2, cx + 24, cy - 2, cx, cy + 22, color);
}

void drawZzz(int cx, int cy) {
  // Independent 4.8KB RAM physics buffer for Zzz particles
  const int sprH = SCREEN_W > 200 ? 60 : 38;
  if (!zSprAttempted) {
    zSprAttempted = true;
    if (!zSpr.createSprite(40, sprH)) {
      zSpr.setColorDepth(8);
      zSpr.createSprite(40, sprH);
    }
  }
  if (!zSpr.created()) return;
  zSpr.fillSprite(COLOR_BG);
  
  // Oscillating anime floating phase
  float phase = (millis() % 3000) / 3000.0 * 2 * PI;
  
  int y1 = sprH - 12 - (int)(sin(phase) * 3);
  int y2 = sprH / 2 - 4 - (int)(sin(phase - 1.0) * 3);
  int y3 = 4 - (int)(sin(phase - 2.0) * 3);
  
  zSpr.setTextColor(0x8410);
  zSpr.drawString("z", 5, y1, 1);
  zSpr.drawString("Z", 15, y2, SCREEN_W > 200 ? 2 : 1);
  zSpr.drawString("Z", 25, y3, SCREEN_W > 200 ? 2 : 1);
  
  zSpr.pushSprite(min(cx + 35, SCREEN_W - 40), max(UI_HEADER_H, cy - 70));
  invalidateThoughtBubble();
}

// ═══════════════════════════════════════════════════════════
// EMOTION COLORS & RENDERERS
// ═══════════════════════════════════════════════════════════

// Deep blue palette
#define DEEP_BLUE   0x0013
#define MID_BLUE    0x033F
#define BRIGHT_CYAN 0x07FF

void drawNeutralEyes() {
  drawAlmondEye(LEFT_EYE_X, EYE_Y, EYE_W, EYE_H, pupilOffsetX, pupilOffsetY,
                DEEP_BLUE, MID_BLUE, BRIGHT_CYAN, 0.2, -0.2);
  drawAlmondEye(RIGHT_EYE_X, EYE_Y, EYE_W, EYE_H, pupilOffsetX, pupilOffsetY,
                DEEP_BLUE, MID_BLUE, BRIGHT_CYAN, 0.2, -0.2);
}

void drawHappyEyes() {
  // Squinted + golden + mouth
  drawAlmondEye(LEFT_EYE_X, EYE_Y, EYE_W, EYE_H - 12, pupilOffsetX, pupilOffsetY,
                0x8260, 0xCCA0, TFT_YELLOW, 0.35, -0.1);
  drawAlmondEye(RIGHT_EYE_X, EYE_Y, EYE_W, EYE_H - 12, pupilOffsetX, pupilOffsetY,
                0x8260, 0xCCA0, TFT_YELLOW, 0.1, -0.35);
  drawEyebrows(LEFT_EYE_X, RIGHT_EYE_X, EYE_Y, 0xCCA0, false);
  drawMouth(SCREEN_W/2, EYE_Y, true);
}

void drawSadEyes() {
  drawAlmondEye(LEFT_EYE_X, EYE_Y, EYE_W, EYE_H - 5, 0, 5,
                0x4208, 0x630C, 0xA514, -0.25, 0.25);
  drawAlmondEye(RIGHT_EYE_X, EYE_Y, EYE_W, EYE_H - 5, 0, 5,
                0x4208, 0x630C, 0xA514, -0.25, 0.25);
  drawMouth(SCREEN_W/2, EYE_Y, false);
}

void drawExcitedEyes() {
  // Big wide eyes alone
  drawAlmondEye(LEFT_EYE_X, EYE_Y, EYE_W, EYE_H + 12, pupilOffsetX*1.3, pupilOffsetY*1.3,
                0x900A, MID_BLUE, BRIGHT_CYAN, 0.1, -0.1);
  drawAlmondEye(RIGHT_EYE_X, EYE_Y, EYE_W, EYE_H + 12, pupilOffsetX*1.3, pupilOffsetY*1.3,
                0x900A, MID_BLUE, BRIGHT_CYAN, 0.1, -0.1);
  drawMouth(SCREEN_W/2, EYE_Y, true);
}

void drawLoveEyes() {
  // Heart-shaped eyes
  drawHeartEyes(LEFT_EYE_X, EYE_Y, TFT_MAGENTA);
  drawHeartEyes(RIGHT_EYE_X, EYE_Y, TFT_MAGENTA);
  // Tiny white reflections on hearts
  tft.fillCircle(LEFT_EYE_X + 5, EYE_Y - 10, 3, TFT_WHITE);
  tft.fillCircle(RIGHT_EYE_X + 5, EYE_Y - 10, 3, TFT_WHITE);
  drawMouth(SCREEN_W/2, EYE_Y, true);
}

void drawSleepyEyes() {
  // Very thin slits + Z's
  drawAlmondEye(LEFT_EYE_X, EYE_Y, EYE_W, EYE_H - 28, 0, 0,
                0x410A, 0x610E, 0xA214, 0.05, -0.05);
  drawAlmondEye(RIGHT_EYE_X, EYE_Y, EYE_W, EYE_H - 28, 0, 0,
                0x410A, 0x610E, 0xA214, 0.05, -0.05);
  // drawZzz is now frame-animated in updateEyes()
}

void drawAngryEyes() {
  // Narrow + red + V brows
  drawAlmondEye(LEFT_EYE_X, EYE_Y, EYE_W, EYE_H - 10, 5, -3,
                0x8000, 0xC000, TFT_RED, 0.45, 0.0);
  drawAlmondEye(RIGHT_EYE_X, EYE_Y, EYE_W, EYE_H - 10, -5, -3,
                0x8000, 0xC000, TFT_RED, 0.0, -0.45);
  drawEyebrows(LEFT_EYE_X, RIGHT_EYE_X, EYE_Y, TFT_RED, true);
  drawMouth(SCREEN_W/2, EYE_Y, false);
}

void drawSurprisedEyes() {
  // Round wide eyes
  drawAlmondEye(LEFT_EYE_X, EYE_Y, EYE_W - 20, EYE_H + 18, 0, 0,
                0x8260, 0xCCA0, TFT_YELLOW, 0.0, 0.0);
  drawAlmondEye(RIGHT_EYE_X, EYE_Y, EYE_W - 20, EYE_H + 18, 0, 0,
                0x8260, 0xCCA0, TFT_YELLOW, 0.0, 0.0);
  // Open mouth
  tft.drawCircle(SCREEN_W/2, EYE_Y + SCALE_Y(58), SCALE_MIN(6), 0x8410);
}

// ═══════════════════════════════════════════════════════════
// V7: SHOOTING STAR ANIMATION — diagonal streak across bg
// ═══════════════════════════════════════════════════════════

static unsigned long lastShootingStar = 0;
static int starX = -1, starY = -1, starLen = 0;
static bool starActive = false;

static void updateShootingStar() {
  unsigned long now = millis();
  
  if (!starActive) {
    // Trigger a new shooting star every 8-15 seconds
    if (now - lastShootingStar > (uint32_t)(8000 + random(7000))) {
      starX = random(SCALE_X(20), SCREEN_W - SCALE_X(20));
      starY = random(SCALE_Y(20), SCALE_Y(50));
      starLen = 0;
      starActive = true;
    }
    return;
  }
  
  // Animate the streak
  int maxLen = SCALE_X(40);
  if (starLen < maxLen) {
    int drawX = starX + starLen;
    int drawY = starY + starLen / 3;
    if (drawX < SCREEN_W && drawY < SCREEN_H) {
      // Bright head
      tft.drawPixel(drawX, drawY, TFT_WHITE);
      // Dim tail (fade previous pixels)
      if (starLen > 2) {
        tft.drawPixel(drawX - 2, drawY - 1, 0x4208);
      }
      if (starLen > 5) {
        tft.drawPixel(drawX - 5, drawY - 2, COLOR_BG);
      }
    }
    starLen += 2;
  } else {
    // Erase remaining tail
    for (int i = max(0, starLen - 6); i <= starLen; i++) {
      int ex = starX + i;
      int ey = starY + i / 3;
      if (ex < SCREEN_W && ey < SCREEN_H) {
        tft.drawPixel(ex, ey, COLOR_BG);
      }
    }
    starActive = false;
    lastShootingStar = now;
  }
}

// ═══════════════════════════════════════════════════════════
// V7: HEARTBEAT PULSE — subtle glow ring around eyes on LOVE
// ═══════════════════════════════════════════════════════════

static void drawHeartbeatPulse() {
  if (currentEmotion != EMO_LOVE) return;
  EyeRenderViewport viewport;
  
  float beat = (sin(millis() * 0.006f) + 1.0f) * 0.5f;
  uint16_t glowCol = blendColor(COLOR_BG, TFT_MAGENTA, beat * 0.25f);
  
  // Soft glow ring around each eye
  int r = EYE_W / 2 + SCALE_X(12);
  tft.drawCircle(LEFT_EYE_X, EYE_Y, r, glowCol);
  tft.drawCircle(RIGHT_EYE_X, EYE_Y, r, glowCol);
}



// ═══════════════════════════════════════════════════════════
// TIME & LDR BASED EMOTION LOGIC
// ═══════════════════════════════════════════════════════════

Emotion getAutoEmotion() {
  // Both platforms supply a normalized 0..1023 LDR value.
  if (ldrEnabled && ldrValue < 100) return EMO_SLEEPY;
  
  // Time-based defaults
  if (timeReceived) {
    if (displayHour >= 22 || displayHour < 6) return EMO_SLEEPY;
    if (displayHour >= 6 && displayHour < 9) return EMO_HAPPY;
    if (displayHour >= 9 && displayHour < 12) return EMO_NEUTRAL;
    if (displayHour >= 12 && displayHour < 14) return EMO_HAPPY;
    if (displayHour >= 14 && displayHour < 18) return EMO_NEUTRAL;
    if (displayHour >= 18 && displayHour < 22) return EMO_NEUTRAL;
  }
  
  return EMO_NEUTRAL;
}

// ═══════════════════════════════════════════════════════════
// MAIN DRAW & UPDATE
// ═══════════════════════════════════════════════════════════

static int pupilRenderedX = 0, pupilRenderedY = 0;

void drawEyes() {
  if (activeTheme == 0) {
    extern TFT_eSprite* t2CanvasPtr();
    extern TFT_eSprite* t3CanvasPtr();
    extern void t2CanvasSetNull();
    extern void t3CanvasSetNull();
    TFT_eSprite* t2 = t2CanvasPtr();
    TFT_eSprite* t3 = t3CanvasPtr();
    if (t2) { t2->deleteSprite(); delete t2; t2CanvasSetNull(); }
    if (t3) { t3->deleteSprite(); delete t3; t3CanvasSetNull(); }
    setThoughtSpriteCompositing(false);
  }
  EyeRenderViewport viewport;
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
  pupilRenderedX = (int)pupilOffsetX;
  pupilRenderedY = (int)pupilOffsetY;
  invalidateThoughtBubble();
}

void setEmotion(Emotion newEmotion) {
  lastManualEmotionTime = millis();
  if (currentEmotion != newEmotion) {
    currentEmotion = newEmotion;
    if (currentState == STATE_EYES) {
      // CRITICAL FIX: Only clear the safe eye zone, NOT the entire screen.
      int eyeZoneY = UI_HEADER_H;
      int eyeZoneH = eyeRenderBottom() - UI_HEADER_H;
      tft.fillRect(0, eyeZoneY, SCREEN_W, eyeZoneH, COLOR_BG);
      drawStarfield();
      drawEyes();
      drawStatusBar(); // Guarantee clock remains visible
    }
  }
}

void updateEyes() {
  if (currentState != STATE_EYES) return;
  if (customEyeActive && customEyeReady) return; // Pause animations for Memories image
  
  unsigned long now = millis();
  setThoughtSpriteCompositing(false);
  
  // Read LDR (platform-aware: ESP32=GPIO36/VP, ESP8266=A0)
  static unsigned long lastLDR = 0;
  if (now - lastLDR > 5000) {
    lastLDR = now;
    #ifdef ESP32
      ldrValue = analogRead(LDR_PIN) >> 2; // Match main's 12-bit to 10-bit normalization.
    #else
      ldrValue = analogRead(A0);
    #endif
    
    // Main owns ambient dispatch for all themes. This sensor tick must not
    // turn an automatic context change into a new manual-emotion lease.
    autoEmotion = getAutoEmotion();
  }
  
  static uint32_t lastAnimationFrame = 0;
  uint32_t frameElapsed = (uint32_t)now - lastAnimationFrame;
  if (frameElapsed < 33) return;
  lastAnimationFrame = now;
  float frameScale = min(2.0f, frameElapsed / 50.0f);

  // Pupil drift
  if (now - lastPupilMove > 3500) {
    if (random(100) > 35) {
      pupilTargetX = random(-14, 14);
      pupilTargetY = random(-10, 10);
    } else {
      pupilTargetX = 0;
      pupilTargetY = 0;
    }
    lastPupilMove = now;
  }
  
  // SUBMISSION 2: Spring-physics damped harmonic oscillator
  // F_spring = -k * displacement; F_damp = -c * velocity
  bool moved = false;
  const float k = 0.08f;   // Spring stiffness
  const float c = 0.22f;   // Damping coefficient  
  const float moveThreshold = 1.5f;  // Min pixel movement to trigger redraw (FIXES FLICKER)
  const float velDeadzone  = 0.8f;   // Stop integrating tiny velocities
  
  float dX = pupilTargetX - pupilOffsetX;
  float dY = pupilTargetY - pupilOffsetY;
  
  if (abs(dX) > moveThreshold || abs(dY) > moveThreshold ||
      abs(pupilVelX) > velDeadzone || abs(pupilVelY) > velDeadzone) {
    // Euler integration of spring + damper
    float ax = (k * dX) - (c * pupilVelX);
    float ay = (k * dY) - (c * pupilVelY);
    pupilVelX += ax * frameScale;
    pupilVelY += ay * frameScale;
    pupilOffsetX += pupilVelX * frameScale;
    pupilOffsetY += pupilVelY * frameScale;
  } else {
    // Snap to rest and kill velocity when close enough
    pupilVelX = 0; pupilVelY = 0;
    pupilOffsetX = pupilTargetX;
    pupilOffsetY = pupilTargetY;
  }
  // Compare against the last painted pixels, so subpixel steps accumulate.
  moved = (int)pupilOffsetX != pupilRenderedX || (int)pupilOffsetY != pupilRenderedY;
  
  // Blinking 
  static uint32_t blinkStarted = 0;
  if (now - lastBlink >= 4500 && !isBlinking) {
    lastBlink = now;
    if (random(10) > 4) {
      isBlinking = true;
      blinkStarted = now;
      blinkPhase = 0;
    }
  }
  
  if (isBlinking) {
    uint32_t elapsed = (uint32_t)now - blinkStarted;
    blinkPhase = elapsed / 50;
    if (elapsed < 100) {
      currentBlinkMod = 1.0f - elapsed / 100.0f;
    } else if (elapsed < 200) {
      currentBlinkMod = (elapsed - 100) / 100.0f;
    } else {
      currentBlinkMod = 1.0;
      isBlinking = false;
      lastBlink = now;
    }
    
    // Smoothly push the modified sprite to screen, zero flicker!
    drawEyes();
  } else if (moved) {
    // Redraw natively - no fillScreen = zero frame strobe.
    drawEyes();
  }
  
  // Independent 60FPS particle effect for Sleepy Z's
  if (currentEmotion == EMO_SLEEPY && !isBlinking) {
    drawZzz(RIGHT_EYE_X, EYE_Y); 
  }
  
  // V7: Shooting star animation (background, non-blocking)
  // updateShootingStar();
  
  // V7: Heartbeat glow ring on LOVE emotion
  drawHeartbeatPulse();
}

// updateTimeFromUDP() is defined in CompanionOS_Main.ino
// Removed duplicate to avoid linker errors.

#endif
