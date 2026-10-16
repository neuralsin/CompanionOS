#ifndef EYES_H
#define EYES_H

#include "globals.h"
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

// ── Layout Constants ─────────────────────────────────────
#define EYE_Y        110
#define LEFT_EYE_X   95
#define RIGHT_EYE_X  225
#define EYE_W        90     // Full width of almond
#define EYE_H        38     // Baseline height
#define PUPIL_W      10
#define PUPIL_H      28

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
    uint32_t rX = xorshift(seed) % 320;
    uint32_t rY = xorshift(seed) % 150 + 30;
    uint8_t  brightness = xorshift(seed) % 3;
    
    // Skip pixels that overlap the eye bounding boxes
    bool inLeftEye  = (rX >= LEFT_EYE_X - EYE_W/2 - 8) && (rX <= LEFT_EYE_X + EYE_W/2 + 8)
                   && (rY >= EYE_Y - EYE_H - 8) && (rY <= EYE_Y + EYE_H + 8);
    bool inRightEye = (rX >= RIGHT_EYE_X - EYE_W/2 - 8) && (rX <= RIGHT_EYE_X + EYE_W/2 + 8)
                   && (rY >= EYE_Y - EYE_H - 8) && (rY <= EYE_Y + EYE_H + 8);
    
    if (!inLeftEye && !inRightEye) {
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

// ── Color Interpolation ──────────────────────────────────
uint16_t blendColor(uint16_t c1, uint16_t c2, float t) {
  if (t <= 0) return c1;
  if (t >= 1) return c2;
  uint8_t r1 = (c1 >> 11) & 0x1F, g1 = (c1 >> 5) & 0x3F, b1 = c1 & 0x1F;
  uint8_t r2 = (c2 >> 11) & 0x1F, g2 = (c2 >> 5) & 0x3F, b2 = c2 & 0x1F;
  uint8_t r = r1 + (r2 - r1) * t;
  uint8_t g = g1 + (g2 - g1) * t;
  uint8_t b = b1 + (b2 - b1) * t;
  return (r << 11) | (g << 5) | b;
}

// ── Core Almond Eye Renderer ────────────────────────────
float currentBlinkMod = 1.0;

TFT_eSprite eyeSpr = TFT_eSprite(&tft);
bool eyeSprAllocated = false;

void drawAlmondEye(int cx, int cy, int w, int h, float pX, float pY,
                   uint16_t c1, uint16_t c2, uint16_t c3, float tiltL, float tiltR) {
  h = (int)((float)h * currentBlinkMod);
  if (h < 2) h = 2; // Prevent division by zero and inversion
  
  float halfW = w / 2.0;

  // SUBMISSION 2: True Double-Buffering for Zero-Tear Eye Rendering
  int sprW = w + 16;
  int sprH = (h + 4) * 2;
  if (!eyeSprAllocated) {
    eyeSpr.createSprite(sprW, sprH);
    eyeSprAllocated = true;
  }
  eyeSpr.fillSprite(COLOR_BG);
  int scx = sprW / 2;
  int scy = sprH / 2;

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
        eyeSpr.drawFastVLine(scx + x, scy + yt, yb - yt, glowColor);
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
        eyeSpr.drawPixel(scx + x, scy + yt + py, color);
      }
    }
  }

  // Vertical Cat Pupil
  int px = scx + (int)pX;
  int py = scy + (int)pY;
  
  float squish = (float)h / EYE_H;
  int p_h = (int)(PUPIL_H * squish);
  if (p_h < 2) p_h = 2;
  
  // Advanced Detailing: Rich Iris depth ring
  eyeSpr.fillRoundRect(px - PUPIL_W/2 - 2, py - p_h/2 - 2, PUPIL_W + 4, p_h + 4, (PUPIL_W+4)/2, 0xFEA0);
  eyeSpr.drawRoundRect(px - PUPIL_W/2 - 3, py - p_h/2 - 3, PUPIL_W + 6, p_h + 6, (PUPIL_W+6)/2, 0x8260);
  
  // Deep Black pill pupil
  eyeSpr.fillRoundRect(px - PUPIL_W/2, py - p_h/2, PUPIL_W, p_h, PUPIL_W/2, TFT_BLACK);
  
  // Primary reflection (top-right curved glass highlight)
  if (squish > 0.3) {
    eyeSpr.fillCircle(px + 4, py - (int)(6 * squish), 3, TFT_WHITE);
    eyeSpr.fillCircle(px + 3, py - (int)(9 * squish), 2, TFT_WHITE);
    // Secondary bounce reflection (bottom-left)
    eyeSpr.fillCircle(px - 2, py + (int)(6 * squish), 2, 0x6B4D);
  }

  // Push completed tear-free frame to screen
  eyeSpr.pushSprite(cx - scx, cy - scy);
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
  static TFT_eSprite zSpr(&tft);
  static bool zSprAlloc = false;
  if (!zSprAlloc) {
    zSpr.createSprite(40, 60);
    zSprAlloc = true;
  }
  zSpr.fillSprite(COLOR_BG);
  
  // Oscillating anime floating phase
  float phase = (millis() % 3000) / 3000.0 * 2 * PI;
  
  int y1 = 45 - (int)(sin(phase) * 6);
  int y2 = 30 - (int)(sin(phase - 1.0) * 8);
  int y3 = 10 - (int)(sin(phase - 2.0) * 12);
  
  zSpr.setTextColor(0x8410);
  zSpr.drawString("z", 5, y1, 1);
  zSpr.drawString("Z", 15, y2, 2);
  zSpr.drawString("Z", 25, y3, 2); 
  
  zSpr.pushSprite(cx + 35, cy - 70);
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
  tft.drawCircle(SCREEN_W/2, EYE_Y + 58, 6, 0x8410);
}



// ═══════════════════════════════════════════════════════════
// TIME & LDR BASED EMOTION LOGIC
// ═══════════════════════════════════════════════════════════

Emotion getAutoEmotion() {
  // LDR-based: very dark = sleepy
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

void drawEyes() {
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
  if (currentEmotion != newEmotion) {
    currentEmotion = newEmotion;
    if (currentState == STATE_EYES) {
      // CRITICAL FIX: Only clear the safe eye zone, NOT the entire screen.
      int eyeZoneY = 16;  // Below status bar
      int eyeZoneH = SCREEN_H - 16;
      tft.fillRect(0, eyeZoneY, SCREEN_W, eyeZoneH, COLOR_BG);
      drawStarfield();
      drawEyes();
      drawStatusBar(); // Guarantee clock remains visible
    }
  }
}

void updateEyes() {
  if (currentState != STATE_EYES) return;
  
  unsigned long now = millis();
  
  // Read LDR (on A0)
  static unsigned long lastLDR = 0;
  if (now - lastLDR > 5000) {
    lastLDR = now;
    ldrValue = analogRead(A0);
    
    // Auto-emotion based on LDR + time (only if no manual emotion override recently)
    Emotion suggested = getAutoEmotion();
    if (suggested != autoEmotion) {
      autoEmotion = suggested;
      // Only apply if emotion hasn't been manually overridden in last 30 seconds
      static unsigned long lastManualEmotion = 0;
      if (now - lastManualEmotion > 30000) {
        setEmotion(autoEmotion);
      }
    }
  }
  
  // Pupil drift (High-speed 160MHz mode)
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
    float prevX = pupilOffsetX, prevY = pupilOffsetY;
    // Euler integration of spring + damper
    float ax = (k * dX) - (c * pupilVelX);
    float ay = (k * dY) - (c * pupilVelY);
    pupilVelX += ax;
    pupilVelY += ay;
    pupilOffsetX += pupilVelX;
    pupilOffsetY += pupilVelY;
    // Only mark moved if we physically moved more than 1 pixel
    if (abs(pupilOffsetX - prevX) > 1.0f || abs(pupilOffsetY - prevY) > 1.0f) {
      moved = true;
    }
  } else {
    // Snap to rest and kill velocity when close enough
    pupilVelX = 0; pupilVelY = 0;
    pupilOffsetX = pupilTargetX;
    pupilOffsetY = pupilTargetY;
  }
  
  // Blinking 
  if (now - lastBlink > 4500 && !isBlinking && random(10) > 4) {
    isBlinking = true;
    blinkPhase = 0;
  }
  
  if (isBlinking) {
    blinkPhase++;
    
    // Physics-based rapid blink speed for 160MHz tickrate
    if (blinkPhase <= 2) {
      currentBlinkMod = 1.0 - ((float)blinkPhase / 2.0);
    } else if (blinkPhase <= 4) {
      currentBlinkMod = (float)(blinkPhase - 2) / 2.0;
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
}

void updateTimeFromUDP(String timeStr) {
  int colonIndex = timeStr.indexOf(':');
  if (colonIndex > 0) {
    displayHour = timeStr.substring(0, colonIndex).toInt();
    displayMinute = timeStr.substring(colonIndex + 1).toInt();
    timeReceived = true;
    // UI drawing is handled by ui.h globally now
  }
}

#endif
