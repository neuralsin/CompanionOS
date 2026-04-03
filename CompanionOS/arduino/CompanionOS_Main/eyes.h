#ifndef EYES_H
#define EYES_H

#include "globals.h"
#include "theme_eyes.h"
#include "theme_transition.h"
#include <math.h>

// ═══════════════════════════════════════════════════════════
// EYE ANIMATION ENGINE V3 — Enhanced Cat Eyes
// + V4: Pet Personality, Exotic Mode Effects, Parallax
//
// Features:
//   - 5-tier gradient fills for smooth color transitions 
//   - Outer glow rings for depth illusion
//   - Emotion decorators: eyebrows, tears, sparkles, mouths
//   - LDR-based mood: dim light = sleepy, bright = energetic
//   - Time-based defaults: night = sleepy, morning = happy
//   - Cat-like almond geometry with tilt
//   - Vertical pill pupils with dual reflection highlights
//   - V4: Exotic mode overlays (aurora, particles, rain, etc)
// ═══════════════════════════════════════════════════════════

enum Emotion {
  EMO_NEUTRAL,
  EMO_HAPPY,
  EMO_SAD,
  EMO_EXCITED,
  EMO_LOVE,
  EMO_SLEEPY,
  EMO_ANGRY,
  EMO_SURPRISED,
  EMO_HORNY,
  EMO_COUNT
};

extern Emotion currentEmotion;
Emotion autoEmotion = EMO_NEUTRAL;  // Time/LDR suggested emotion

// ── Layout Constants ─────────────────────────────────────
#define EYE_Y        110
#define LEFT_EYE_X   95
#define RIGHT_EYE_X  225
#define EYE_W        90     // Full width of almond
#define EYE_H        42     // Baseline height (slightly taller for anime look)
#define PUPIL_W      12     // Slightly wider pupils for horny state
#define PUPIL_H      26

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
  
  // V4: Parallax offset — stars shift opposite to pupil direction for depth
  float parallaxX = exoticMode ? (-pupilOffsetX * 0.3) : 0;
  float parallaxY = exoticMode ? (-pupilOffsetY * 0.3) : 0;
  
  // Stars across the full screen background
  for (int i = 0; i < 40; i++) {
    uint32_t rX = xorshift(seed) % 320;
    uint32_t rY = xorshift(seed) % 210 + 16;  // Full range: 16 to 226 (below status bar)
    uint8_t  brightness = xorshift(seed) % 3;
    
    // Apply parallax
    int drawX = (int)rX + (int)parallaxX;
    int drawY = (int)rY + (int)parallaxY;
    // Clamp to screen bounds after parallax shift
    if (drawX < 0 || drawX >= SCREEN_W || drawY < 16 || drawY >= SCREEN_H - 10) continue;
    
    // Skip pixels that overlap the eye bounding boxes
    bool inLeftEye  = (drawX >= LEFT_EYE_X - EYE_W/2 - 8) && (drawX <= LEFT_EYE_X + EYE_W/2 + 8)
                   && (drawY >= EYE_Y - EYE_H - 8) && (drawY <= EYE_Y + EYE_H + 8);
    bool inRightEye = (drawX >= RIGHT_EYE_X - EYE_W/2 - 8) && (drawX <= RIGHT_EYE_X + EYE_W/2 + 8)
                   && (drawY >= EYE_Y - EYE_H - 8) && (drawY <= EYE_Y + EYE_H + 8);
    
    if (!inLeftEye && !inRightEye) {
      uint16_t col = brightness == 2 ? 0x4208 : brightness == 1 ? 0x2104 : 0x10A2;
      tft.drawPixel(drawX, drawY, col);
    }
  }
}

// Time state
int displayHour = 0;
int displayMinute = 0;
bool timeReceived = false;

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
TFT_eSprite eyeSpr = TFT_eSprite(&tft);
bool eyeSprAllocated = false;
int eyeSprAllocW = 0;
int eyeSprAllocH = 0;

void drawAlmondEye(int cx, int cy, int w, int h, float pX, float pY,
                   uint16_t c1, uint16_t c2, uint16_t c3, float tiltL, float tiltR,
                   bool isHorny = false) {
  float halfW = w / 2.0;

  // SUBMISSION 2: True Double-Buffering for Zero-Tear Eye Rendering
  int sprW = w + 20;
  int sprH = (h + 8) * 2;
  // Reallocate if current sprite is too small for the requested dimensions
  if (!eyeSprAllocated || sprW > eyeSprAllocW || sprH > eyeSprAllocH) {
    if (eyeSprAllocated) eyeSpr.deleteSprite();
    // Allocate with 10% headroom to avoid frequent reallocs from asymmetric sizing
    eyeSprAllocW = sprW + 12;
    eyeSprAllocH = sprH + 12;
    eyeSpr.createSprite(eyeSprAllocW, eyeSprAllocH);
    eyeSprAllocated = true;
  }
  eyeSpr.fillSprite(COLOR_BG);
  int scx = sprW / 2;
  int scy = sprH / 2;

  // Outer glow
  for (int ring = 3; ring >= 1; ring--) {
    uint16_t glowColor = blendColor(c3, COLOR_BG, 0.4 + ring * 0.2);
    for (int x = -(halfW + ring); x <= (halfW + ring); x++) {
      float nx = (float)x / (halfW + ring);
      float curve = 1.0 - (nx * nx);
      float tmod = (x < 0) ? (x * tiltL * 0.6) : (x * tiltR * 0.6);
      int yt = (int)(-(h + ring) * curve + tmod);
      int yb = (int)((h + ring) * curve - tmod);
      if (yb - yt > 0) {
        eyeSpr.drawFastVLine(scx + x, scy + yt, yb - yt, glowColor);
      }
    }
  }
  
  // Main eye body - 5-tier gradient
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
        if (t < 0.35) {
          color = blendColor(c1, c2, t / 0.35);
        } else if (t < 0.7) {
          color = blendColor(c2, c3, (t - 0.35) / 0.35);
        } else {
          color = blendColor(c3, c2, (t - 0.7) / 0.3);
        }
        eyeSpr.drawPixel(scx + x, scy + yt + py, color);
      }
    }
  }

  // ── PUPIL RENDER ─────────────────
  int px = scx + (int)pX;
  int py = scy + (int)pY;

  if (isHorny) {
    // Big glossy iris with heart shape
    eyeSpr.fillCircle(px, py - 2, PUPIL_W + 6, 0xFB9F);           // bright pink-violet
    eyeSpr.fillTriangle(px - 11, py + 6, px + 11, py + 6, px, py + 19, 0xFB9F);

    // Inner black heart pupil
    eyeSpr.fillCircle(px, py - 1, PUPIL_W - 1, TFT_BLACK);
    eyeSpr.fillTriangle(px - 7, py + 5, px + 7, py + 5, px, py + 13, TFT_BLACK);

    // Massive anime reflections (wet look)
    eyeSpr.fillCircle(px + 6, py - 9, 5, TFT_WHITE);
    eyeSpr.fillCircle(px + 4, py - 12, 3, TFT_WHITE);
    eyeSpr.fillCircle(px - 3, py - 6, 4, 0xCFFF);   // cyan highlight
  } 
  else {
    // Normal cat pupil
    eyeSpr.fillRoundRect(px - PUPIL_W/2 - 2, py - PUPIL_H/2 - 2, PUPIL_W + 4, PUPIL_H + 4, (PUPIL_W+4)/2, 0xFEA0);
    eyeSpr.drawRoundRect(px - PUPIL_W/2 - 3, py - PUPIL_H/2 - 3, PUPIL_W + 6, PUPIL_H + 6, (PUPIL_W+6)/2, 0x8260);
    eyeSpr.fillRoundRect(px - PUPIL_W/2, py - PUPIL_H/2, PUPIL_W, PUPIL_H, PUPIL_W/2, TFT_BLACK);
    eyeSpr.fillCircle(px + 4, py - 6, 3, TFT_WHITE);
    eyeSpr.fillCircle(px + 3, py - 9, 2, TFT_WHITE);
    eyeSpr.fillCircle(px - 2, py + 6, 2, 0x6B4D);
    
    // V4 EXOTIC: Limbal ring shimmer — iridescent outer ring on iris
    if (exoticMode) {
      // Hue shifts based on pupil X offset, mapped to RGB wheel
      float huePhase = (pX + 14.0) / 28.0 * 2.0 * PI;
      uint8_t hr = (uint8_t)(15 + 8 * sin(huePhase));
      uint8_t hg = (uint8_t)(20 + 15 * sin(huePhase + 2.09));
      uint8_t hb = (uint8_t)(15 + 8 * sin(huePhase + 4.18));
      uint16_t shimmerColor = (hr << 11) | (hg << 5) | hb;
      eyeSpr.drawRoundRect(px - PUPIL_W/2 - 4, py - PUPIL_H/2 - 4, PUPIL_W + 8, PUPIL_H + 8, (PUPIL_W+8)/2, shimmerColor);
    }
  }

  // V4 EXOTIC: Scanline shader — darken every 2nd line of the sprite
  if (exoticMode) {
    for (int sy = 0; sy < sprH; sy += 2) {
      for (int sx = 0; sx < sprW; sx += 3) {  // Skip pixels for perf
        uint16_t pixel = eyeSpr.readPixel(sx, sy);
        if (pixel != COLOR_BG) {
          uint16_t dimmed = blendColor(pixel, COLOR_BG, 0.15);
          eyeSpr.drawPixel(sx, sy, dimmed);
        }
      }
    }
  }

  // Push completed tear-free frame to screen and free RAM
  eyeSpr.pushSprite(cx - scx, cy - scy);
  // We no longer deleteSprite() here to prevent massive heap fragmentation
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

void drawTears(int cx, int cy) {
  if (exoticMode) {
    // V4 EXOTIC: Physics-based parabolic tears
    float t = (millis() % 2000) / 2000.0;  // 2-second cycle
    float vy0 = -3.0;  // Initial upward velocity
    float g = 12.0;    // Gravity
    float tearY = vy0 * t + 0.5 * g * t * t;  // Parabolic arc
    int ty = cy + 30 + (int)(tearY * 4);
    
    if (ty < SCREEN_H - 10) {
      tft.fillCircle(cx + 10, ty, 3, 0x5DDF);
      tft.fillCircle(cx + 10, ty - 4, 2, 0x5DDF);
    } else {
      // Splash at bottom
      tft.drawPixel(cx + 7, SCREEN_H - 12, 0x5DDF);
      tft.drawPixel(cx + 13, SCREEN_H - 12, 0x5DDF);
      tft.drawPixel(cx + 10, SCREEN_H - 10, 0x5DDF);
    }
  } else {
    // Legacy: Animated teardrop
    for (int i = 0; i < 3; i++) {
      int ty = cy + 35 + (i * 12);
      tft.fillCircle(cx + 10, ty, 3 - i, 0x5DDF);  // Light blue
    }
  }
}

void drawSparkles(int cx, int cy) {
  // Tiny diamond sparkles around the eye
  int spots[][2] = {{-30, -30}, {35, -25}, {-25, 30}, {30, 25}};
  for (int i = 0; i < 4; i++) {
    int sx = cx + spots[i][0];
    int sy = cy + spots[i][1];
    tft.drawPixel(sx, sy, TFT_WHITE);
    tft.drawPixel(sx-1, sy, TFT_WHITE);
    tft.drawPixel(sx+1, sy, TFT_WHITE);
    tft.drawPixel(sx, sy-1, TFT_WHITE);
    tft.drawPixel(sx, sy+1, TFT_WHITE);
  }
}

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

// ── HORNY DECORATORS ─────────────────────────────────────
void drawBlush(int lx, int rx, int y) {
  tft.fillCircle(lx - 28, y + 12, 14, 0xF8B2);   // soft pink blush
  tft.fillCircle(rx + 28, y + 12, 14, 0xF8B2);
}

void drawDrool(int cx, int cy) {
  tft.fillRoundRect(cx - 8, cy + 48, 7, 18, 4, 0xA7FF);     // glossy drool
  tft.fillCircle(cx - 5, cy + 64, 4, 0xA7FF);
}

// ── V4: PET PERSONALITY DECORATORS ──────────────────────
void drawPettingHearts() {
  // Procedural floating hearts during petting mode
  uint32_t seed = millis() / 200;
  for (int i = 0; i < 5; i++) {
    seed = seed * 1103515245 + 12345;
    int hx = 50 + (seed % 220);
    seed = seed * 1103515245 + 12345;
    int hy = 30 + (seed % 100);
    float drift = sin(millis() * 0.003 + i * 1.2) * 4;
    hy += (int)drift;
    
    // Tiny heart
    tft.fillCircle(hx - 2, hy, 2, 0xF81F);
    tft.fillCircle(hx + 2, hy, 2, 0xF81F);
    tft.fillTriangle(hx - 4, hy + 1, hx + 4, hy + 1, hx, hy + 5, 0xF81F);
  }
}

void drawPettingBlush() {
  // Soft blush under both eyes
  tft.fillCircle(LEFT_EYE_X - 15, EYE_Y + 25, 10, 0xF8B2);
  tft.fillCircle(RIGHT_EYE_X + 15, EYE_Y + 25, 10, 0xF8B2);
}

// ── V4 EXOTIC: Aurora Borealis Background ───────────────
void drawAuroraBackground() {
  if (!exoticMode) return;
  
  float t = millis() * 0.0003;  // Very slow drift
  
  // Emotion-keyed color palette
  uint16_t auroraC1, auroraC2;
  switch (currentEmotion) {
    case EMO_HAPPY:    auroraC1 = 0xFEA0; auroraC2 = 0x07E0; break;  // Gold → Green
    case EMO_SAD:      auroraC1 = 0x001F; auroraC2 = 0x4208; break;  // Blue → Gray
    case EMO_LOVE:     auroraC1 = 0xF81F; auroraC2 = 0xFB9F; break;  // Magenta → Pink
    case EMO_ANGRY:    auroraC1 = 0xF800; auroraC2 = 0xFBE0; break;  // Red → Orange
    case EMO_HORNY:    auroraC1 = 0xF81F; auroraC2 = 0xFFFF; break;  // Pink → White
    default:           auroraC1 = 0x001F; auroraC2 = 0x07FF; break;  // Blue → Cyan
  }
  
  // 4 sine waves creating aurora bands
  for (int x = 0; x < SCREEN_W; x += 4) {  // Skip for performance
    float wave1 = sin(x * 0.02 + t) * 20;
    float wave2 = sin(x * 0.03 + t * 1.3 + 1.0) * 15;
    float wave3 = sin(x * 0.015 + t * 0.7 + 2.0) * 25;
    
    int y1 = 80 + (int)(wave1 + wave2);
    int y2 = y1 + 8 + (int)(wave3 * 0.3);
    
    if (y1 >= 16 && y2 < SCREEN_H && y2 > y1) {
      float blend = (float)x / SCREEN_W;
      uint16_t color = blendColor(auroraC1, auroraC2, blend);
      // Very dim — alpha simulated by darkening
      color = blendColor(COLOR_BG, color, 0.15);
      tft.drawFastVLine(x, y1, y2 - y1, color);
    }
  }
}

// ── V4 EXOTIC: Breathing Glow Background ────────────────
void drawBreathingGlow() {
  if (!exoticMode) return;
  
  // 0.2Hz breathing cycle
  float breath = (sin(millis() * 0.00126) + 1.0) * 0.5;  // 0.0 → 1.0
  
  // Subtle glow circle at center
  int glowR = 40 + (int)(breath * 20);
  uint16_t glowColor = blendColor(COLOR_BG, 0x0013, breath * 0.3);
  tft.fillCircle(SCREEN_W/2, EYE_Y, glowR, glowColor);
}

// ── V4 EXOTIC: Particle System Idle ─────────────────────
void drawIdleParticles() {
  if (!exoticMode || isPetting) return;
  
  // 8 procedural dust motes using XOR-shift seeded by time
  uint32_t seed = 0x12345678;
  float timeSec = millis() * 0.001;
  
  for (int i = 0; i < 8; i++) {
    seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
    float baseX = (seed % 300) + 10;
    seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
    float baseY = (seed % 180) + 30;
    
    // Gentle sine drift
    float fx = baseX + sin(timeSec * 0.5 + i * 0.8) * 8;
    float fy = baseY + cos(timeSec * 0.3 + i * 1.1) * 6;
    
    int px = (int)fx;
    int py = (int)fy;
    
    if (px > 0 && px < SCREEN_W && py > 16 && py < SCREEN_H - 10) {
      tft.drawPixel(px, py, 0x2104);
    }
  }
}

// ── V4 EXOTIC: Rain/Snow Weather Overlay ────────────────
void drawWeatherOverlay() {
  if (!exoticMode) return;
  
  extern int weatherCode;
  bool isRain = (weatherCode >= 1063 && weatherCode < 1210);
  bool isSnow = (weatherCode >= 1210 && weatherCode <= 1225);
  
  if (!isRain && !isSnow) return;
  
  uint32_t seed = millis() / 50;  // Update every 50ms
  
  for (int i = 0; i < 12; i++) {
    seed = seed * 1103515245 + 12345 + i;
    int dropX = seed % SCREEN_W;
    seed = seed * 1103515245 + 12345;
    
    if (isRain) {
      int dropY = (millis() / 3 + i * 37) % (SCREEN_H - 20) + 16;
      tft.drawFastVLine(dropX, dropY, 4, 0x031F);  // Blue rain drop
    } else {
      int dropY = (millis() / 8 + i * 53) % (SCREEN_H - 20) + 16;
      float drift = sin(millis() * 0.002 + i) * 3;
      tft.drawPixel(dropX + (int)drift, dropY, TFT_WHITE);  // Snowflake
    }
  }
}

// ═══════════════════════════════════════════════════════════
// EMOTION COLORS & RENDERERS
// ═══════════════════════════════════════════════════════════

// Deep blue palette
#define DEEP_BLUE   0x0013
#define MID_BLUE    0x033F
#define BRIGHT_CYAN 0x07FF

// V4: Asymmetric eye sizing helper
void getAsymmetricSize(int &leftW, int &leftH, int &rightW, int &rightH, int baseW, int baseH) {
  if (exoticMode) {
    float scale = pupilOffsetX / 14.0;  // -1 to +1 range
    float leftScale = 1.0 + scale * 0.08;   // Looking left → left eye bigger
    float rightScale = 1.0 - scale * 0.08;  // Looking left → right eye smaller
    leftW = (int)(baseW * leftScale);
    leftH = (int)(baseH * leftScale);
    rightW = (int)(baseW * rightScale);
    rightH = (int)(baseH * rightScale);
  } else {
    leftW = rightW = baseW;
    leftH = rightH = baseH;
  }
}

void drawNeutralEyes() {
  int lw, lh, rw, rh;
  getAsymmetricSize(lw, lh, rw, rh, EYE_W, EYE_H);
  drawAlmondEye(LEFT_EYE_X, EYE_Y, lw, lh, pupilOffsetX, pupilOffsetY,
                DEEP_BLUE, MID_BLUE, BRIGHT_CYAN, 0.2, -0.2);
  drawAlmondEye(RIGHT_EYE_X, EYE_Y, rw, rh, pupilOffsetX, pupilOffsetY,
                DEEP_BLUE, MID_BLUE, BRIGHT_CYAN, 0.2, -0.2);
}

void drawHappyEyes() {
  int lw, lh, rw, rh;
  getAsymmetricSize(lw, lh, rw, rh, EYE_W, EYE_H - 12);
  drawAlmondEye(LEFT_EYE_X, EYE_Y, lw, lh, pupilOffsetX, pupilOffsetY,
                0x8260, 0xCCA0, TFT_YELLOW, 0.35, -0.1);
  drawAlmondEye(RIGHT_EYE_X, EYE_Y, rw, rh, pupilOffsetX, pupilOffsetY,
                0x8260, 0xCCA0, TFT_YELLOW, 0.1, -0.35);
  drawEyebrows(LEFT_EYE_X, RIGHT_EYE_X, EYE_Y, 0xCCA0, false);
  drawMouth(SCREEN_W/2, EYE_Y, true);
}

void drawSadEyes() {
  drawAlmondEye(LEFT_EYE_X, EYE_Y, EYE_W, EYE_H - 5, 0, 5,
                0x4208, 0x630C, 0xA514, -0.25, 0.25);
  drawAlmondEye(RIGHT_EYE_X, EYE_Y, EYE_W, EYE_H - 5, 0, 5,
                0x4208, 0x630C, 0xA514, -0.25, 0.25);
  drawTears(LEFT_EYE_X, EYE_Y);
  drawTears(RIGHT_EYE_X, EYE_Y);
  drawMouth(SCREEN_W/2, EYE_Y, false);
}

void drawExcitedEyes() {
  int lw, lh, rw, rh;
  getAsymmetricSize(lw, lh, rw, rh, EYE_W, EYE_H + 12);
  drawAlmondEye(LEFT_EYE_X, EYE_Y, lw, lh, pupilOffsetX*1.3, pupilOffsetY*1.3,
                0x900A, MID_BLUE, BRIGHT_CYAN, 0.1, -0.1);
  drawAlmondEye(RIGHT_EYE_X, EYE_Y, rw, rh, pupilOffsetX*1.3, pupilOffsetY*1.3,
                0x900A, MID_BLUE, BRIGHT_CYAN, 0.1, -0.1);
  drawSparkles(LEFT_EYE_X, EYE_Y);
  drawSparkles(RIGHT_EYE_X, EYE_Y);
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

void drawHornyEyes() {
  // Slightly squinted + upward tilt for seductive anime look
  drawAlmondEye(LEFT_EYE_X,  EYE_Y, EYE_W, EYE_H - 4, pupilOffsetX*1.2, pupilOffsetY*1.1,
                0xF81F, 0xFB9F, 0xFFFF, 0.25, -0.15, true);   // true = horny style

  drawAlmondEye(RIGHT_EYE_X, EYE_Y, EYE_W, EYE_H - 4, pupilOffsetX*1.2, pupilOffsetY*1.1,
                0xF81F, 0xFB9F, 0xFFFF, 0.15, -0.25, true);

  drawBlush(LEFT_EYE_X, RIGHT_EYE_X, EYE_Y);
  drawDrool(SCREEN_W/2, EYE_Y);

  // Small open mouth with tongue tip (simple)
  tft.fillRoundRect(SCREEN_W/2 - 10, EYE_Y + 58, 20, 8, 4, 0xF81F);
  tft.fillCircle(SCREEN_W/2 + 6, EYE_Y + 64, 4, 0xF81F);   // tongue
}

// V4: Petting eyes — squinted happy with blush and hearts
void drawPettingEyes() {
  // Very squinted (happy squint)
  drawAlmondEye(LEFT_EYE_X, EYE_Y, EYE_W, EYE_H - 20, pupilOffsetX*0.5, pupilOffsetY*0.5,
                0x8260, 0xCCA0, TFT_YELLOW, 0.4, -0.1);
  drawAlmondEye(RIGHT_EYE_X, EYE_Y, EYE_W, EYE_H - 20, pupilOffsetX*0.5, pupilOffsetY*0.5,
                0x8260, 0xCCA0, TFT_YELLOW, 0.1, -0.4);
  drawPettingBlush();
  drawPettingHearts();
  drawMouth(SCREEN_W/2, EYE_Y, true);
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
  // ═══════════════════════════════════════════════
  // THEME DISPATCH: Themes 2-10 use their own renderers
  // Themes 0-1 use the original V3/V4 code below
  // ═══════════════════════════════════════════════
  if (currentThemeId >= 2) {
    drawThemeBackground();
    switch (currentThemeId) {
      case 2:  drawPikachuEyes();      break;
      case 3:  drawChillEyes();        break;
      case 4:  drawGamingEyes();       break;
      case 5:  drawMinimalEyes();      break;
      case 6:  drawAngryThemeEyes();   break;
      case 7:  drawSleepEyes();        break;
      case 8:  drawMoodEyes();         break;
      case 9:  drawSystemEyes();       break;
      case 10: drawCompanionEyes();    break;
    }
    return;
  }

  // ═══════════════════════════════════════════════
  // ORIGINAL V3/V4 CODE — Theme 0 (Legacy) & Theme 1 (Exotic)
  // COMPLETELY UNTOUCHED from this point onward
  // ═══════════════════════════════════════════════

  // V4 EXOTIC: Background effects (drawn before eyes, behind them)
  if (exoticMode) {
    drawBreathingGlow();
    drawAuroraBackground();
    drawIdleParticles();
    drawWeatherOverlay();
  }
  
  // V4: Petting mode overrides normal emotion rendering
  if (isPetting) {
    drawPettingEyes();
    return;
  }
  
  switch (currentEmotion) {
    case EMO_HAPPY:     drawHappyEyes();     break;
    case EMO_SAD:       drawSadEyes();       break;
    case EMO_EXCITED:   drawExcitedEyes();   break;
    case EMO_LOVE:      drawLoveEyes();      break;
    case EMO_SLEEPY:    drawSleepyEyes();    break;
    case EMO_ANGRY:     drawAngryEyes();     break;
    case EMO_SURPRISED: drawSurprisedEyes(); break;
    case EMO_HORNY:     drawHornyEyes();     break;
    default:            drawNeutralEyes();   break;
  }
}

void setEmotion(Emotion newEmotion) {
  if (currentEmotion != newEmotion) {
    currentEmotion = newEmotion;
    lastInteractionTime = millis();  // V4: Reset interaction timer
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

// ── V4: Pet Personality System ──────────────────────────
void updatePetPersonality() {
  if (currentState != STATE_EYES) return;
  
  unsigned long now = millis();
  
  // Mood decay — every 60 seconds, reduce mood by 1 (down to 0)
  if (now - lastMoodDecay > 60000) {
    lastMoodDecay = now;
    if (petMoodLevel > 0) petMoodLevel--;
    
    // After 30 min of no interaction (mood < 70), become sleepy
    if (now - lastInteractionTime > 1800000 && petMoodLevel < 70) {
      if (currentEmotion != EMO_SLEEPY) {
        setEmotion(EMO_SLEEPY);
      }
    }
  }
  
  // Yawn animation — every ~60s of idle, do a brief eye squeeze
  if (!isYawning && !isPetting && now - lastInteractionTime > 15000) {
    if (now % 60000 < 50) {  // Trigger window
      isYawning = true;
      yawnStart = now;
    }
  }
  
  if (isYawning) {
    unsigned long elapsed = now - yawnStart;
    if (elapsed < 800) {
      // Yawn: draw thin-slit eyes for 800ms
      drawAlmondEye(LEFT_EYE_X, EYE_Y, EYE_W, EYE_H - 30, 0, 0,
                    0x410A, 0x610E, 0xA214, 0.05, -0.05);
      drawAlmondEye(RIGHT_EYE_X, EYE_Y, EYE_W, EYE_H - 30, 0, 0,
                    0x410A, 0x610E, 0xA214, 0.05, -0.05);
      // Open mouth yawn
      tft.fillCircle(SCREEN_W/2, EYE_Y + 55, 8, 0x4208);
      tft.fillCircle(SCREEN_W/2, EYE_Y + 55, 5, COLOR_BG);
    } else {
      isYawning = false;
      // Redraw normal eyes
      int eyeZoneY = 16;
      int eyeZoneH = SCREEN_H - 16;
      tft.fillRect(0, eyeZoneY, SCREEN_W, eyeZoneH, COLOR_BG);
      drawStarfield();
      drawEyes();
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
  } // <--- Missing brace restored
  
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
  bool moved = false;
  const float k = 0.08f;   // Spring stiffness
  const float c = 0.22f;   // Damping coefficient  
  const float moveThreshold = 1.5f;
  const float velDeadzone  = 0.8f;
  
  float dX = pupilTargetX - pupilOffsetX;
  float dY = pupilTargetY - pupilOffsetY;
  
  if (abs(dX) > moveThreshold || abs(dY) > moveThreshold ||
      abs(pupilVelX) > velDeadzone || abs(pupilVelY) > velDeadzone) {
    float prevX = pupilOffsetX, prevY = pupilOffsetY;
    float ax = (k * dX) - (c * pupilVelX);
    float ay = (k * dY) - (c * pupilVelY);
    pupilVelX += ax;
    pupilVelY += ay;
    pupilOffsetX += pupilVelX;
    pupilOffsetY += pupilVelY;
    if (abs(pupilOffsetX - prevX) > 1.0f || abs(pupilOffsetY - prevY) > 1.0f) {
      moved = true;
    }
  } else {
    pupilVelX = 0; pupilVelY = 0;
    pupilOffsetX = pupilTargetX;
    pupilOffsetY = pupilTargetY;
  }
  
  bool v5_active = (currentThemeId >= 2);

  // V5 Themes: Continuous rendering
  if (v5_active) {
    // Check custom blink interval if blinking is allowed in V5 (currently handled inside renderers or skipped)
    if (now - lastBlink > activeTheme.blinkIntervalMs && activeTheme.blinkIntervalMs > 0 && !isBlinking && random(10) > 4) {
      isBlinking = true;
      blinkPhase = 0;
    }
    
    drawEyes();  // Continuous frame updates

    // Optional generic V5 blink wipe
    if (isBlinking) {
      blinkPhase++;
      int maxFrames = activeTheme.blinkSpeed * 2;
      if (blinkPhase >= maxFrames) {
        isBlinking = false;
        lastBlink = now;
      } else {
        // Simple full-width eye-zone wipe
        int wipeH = 0;
        if (blinkPhase <= activeTheme.blinkSpeed) {
          wipeH = 100 * blinkPhase / activeTheme.blinkSpeed;
        } else {
          wipeH = 100 * (maxFrames - blinkPhase) / activeTheme.blinkSpeed;
        }
        tft.fillRect(0, THEME_EYE_Y - 50, SCREEN_W, wipeH, activeTheme.bg);
      }
    }
  } 
  // V3/V4 Original Logic
  else {
    // Blinking 
    if (now - lastBlink > 4500 && !isBlinking && random(10) > 4) {
      isBlinking = true;
      blinkPhase = 0;
    }
    
    if (isBlinking) {
      blinkPhase++;
      int coverY = EYE_Y - EYE_H - 6;
      int coverH = 0;
      int maxH = (EYE_H + 6) * 2;
      int eyeColW = EYE_W + 16;
      
      if (blinkPhase <= 2) coverH = maxH * blinkPhase / 2;
      else if (blinkPhase <= 4) coverH = maxH * (4 - blinkPhase) / 2;
      else {
        isBlinking = false;
        lastBlink = now;
        drawEyes();
        goto skip_v4_blink;
      }
      
      // V4 EXOTIC: Chromatic aberration on blink
      if (exoticMode && blinkPhase == 1) {
        // Red-tinted left offset
        tft.fillRect(LEFT_EYE_X - EYE_W/2 - 10, coverY, 4, coverH, 0xF800);
        // Blue-tinted right offset
        tft.fillRect(RIGHT_EYE_X + EYE_W/2 + 6, coverY, 4, coverH, 0x001F);
      }
      
      // Wipe per-eye column ONLY
      tft.fillRect(LEFT_EYE_X - EYE_W/2 - 8, coverY, eyeColW, coverH, COLOR_BG);
      tft.fillRect(RIGHT_EYE_X - EYE_W/2 - 8, coverY, eyeColW, coverH, COLOR_BG);
    } else if (moved) {
      drawEyes();
    }
  skip_v4_blink:;
  }
  
  // Independent 60FPS particle effect for Sleepy Z's
  if (currentEmotion == EMO_SLEEPY && !isBlinking) {
    drawZzz(RIGHT_EYE_X, EYE_Y); 
  }
  
  // V4: Pet personality update
  updatePetPersonality();
  
  // V4: Agent overlay (drawn on top of everything)
  extern void drawAgentOverlay();
  drawAgentOverlay();
}

void updateTimeFromUDP(String timeStr) {
  int colonIndex = timeStr.indexOf(':');
  if (colonIndex > 0) {
    displayHour = timeStr.substring(0, colonIndex).toInt();
    displayMinute = timeStr.substring(colonIndex + 1).toInt();
    timeReceived = true;
  }
}

#endif
