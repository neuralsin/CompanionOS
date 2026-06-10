// ═══════════════════════════════════════════════════════════
// COMPANION OS v7.0 — THEME 3: ROBOEYES ENGINE
//
// Adapted from FluxGarage RoboEyes (GPL v3, Dennis Hoelscher)
// for TFT_eSPI color TFT displays (ST7735R / ILI9341).
//
// Original: Adafruit GFX monochrome OLED (SSD1306)
// This port: TFT_eSPI color TFT, partial-redraw eye zone
//
// Features:
//   - 6 swappable eye variant presets (geometry configs)
//   - Smooth tweened animation (position, size, blink)
//   - Mood system: DEFAULT, TIRED, ANGRY, HAPPY
//   - Auto-blink, idle look-around, curious mode
//   - Cyclops mode, sweat drops, laugh/confused animations
//   - Flicker/shiver effects
//   - CompanionOS emotion mapping
//
// Drawing strategy:
//   We do NOT call clearDisplay()/display() (those are OLED).
//   Instead we fillRect the eye zone with COLOR_BG, then
//   draw rounded rects/triangles directly via tft.*
// ═══════════════════════════════════════════════════════════
#ifndef THEME3_EYES_H
#define THEME3_EYES_H

#include "globals.h"
#include "ui_components.h"

// ═══════════════════════════════════════════════════════════
// EYE VARIANT CONFIGURATION
// ═══════════════════════════════════════════════════════════

struct T3EyeVariantConfig {
  uint8_t eyeW;         // eye width
  uint8_t eyeH;         // eye height
  uint8_t borderRadius; // corner radius
  int8_t  spacing;      // space between eyes (can be negative)
  bool    cyclops;      // single eye mode
  const char* name;     // display name
};

// 6 eye variants — geometry presets
static const T3EyeVariantConfig T3_VARIANTS[] = {
  // W   H   R   Sp  Cyc  Name
  { 30, 30,  6,   8, false, "Classic" },   // 0: Standard rounded square
  { 40, 24,  4,   6, false, "Wide" },      // 1: Short and wide
  { 20, 36, 10,  12, false, "Tall" },      // 2: Narrow and tall
  { 28, 28, 14,  10, false, "Round" },     // 3: Nearly circular
  { 40, 40, 12,   0, true,  "Cyclops" },   // 4: Single large eye
  { 24, 24,  2,  14, false, "Pixel" },     // 5: Sharp square corners
};

#define T3_VARIANT_COUNT 6

// ═══════════════════════════════════════════════════════════
// ROBOEYES ENGINE STATE (adapted for TFT_eSPI)
// ═══════════════════════════════════════════════════════════

// Eye zone boundaries (below status bar, above page dots)
#define T3_EYE_ZONE_Y  16
#define T3_EYE_ZONE_H  (SCREEN_H - 32)  // leave 16px top (status) + 16px bottom (dots)

// Drawing colors — uses CompanionOS color system
#define T3_EYE_COLOR   CLR_TEXT_HI    // white eyes
#define T3_BG_COLOR    CLR_BG         // background

// Mood constants (matching RoboEyes original)
#define T3_MOOD_DEFAULT 0
#define T3_MOOD_TIRED   1
#define T3_MOOD_ANGRY   2
#define T3_MOOD_HAPPY   3

// ── Engine State Variables ───────────────────────────────

// Screen dimensions for constraint calculations
static int t3_screenW = SCREEN_W;
static int t3_screenH = T3_EYE_ZONE_H;
static int t3_offsetY = T3_EYE_ZONE_Y;  // vertical offset for eye zone

// Current variant index
extern int t3EyeVariant;  // defined in globals.h

// Eye geometry — left
static int t3_eyeLwDef = 30, t3_eyeLhDef = 30;
static int t3_eyeLwCur = 30, t3_eyeLhCur = 1;  // start closed
static int t3_eyeLwNext = 30, t3_eyeLhNext = 30;
static int t3_eyeLhOffset = 0;
static uint8_t t3_eyeLbrDef = 6, t3_eyeLbrCur = 6, t3_eyeLbrNext = 6;

// Eye geometry — right
static int t3_eyeRwDef = 30, t3_eyeRhDef = 30;
static int t3_eyeRwCur = 30, t3_eyeRhCur = 1;  // start closed
static int t3_eyeRwNext = 30, t3_eyeRhNext = 30;
static int t3_eyeRhOffset = 0;
static uint8_t t3_eyeRbrDef = 6, t3_eyeRbrCur = 6, t3_eyeRbrNext = 6;

// Eye coordinates — left
static int t3_spaceDef = 8, t3_spaceCur = 8, t3_spaceNext = 8;
static int t3_eyeLxDef, t3_eyeLyDef;
static int t3_eyeLx, t3_eyeLy, t3_eyeLxNext, t3_eyeLyNext;

// Eye coordinates — right (derived from left + spacing)
static int t3_eyeRxDef, t3_eyeRyDef;
static int t3_eyeRx, t3_eyeRy, t3_eyeRxNext, t3_eyeRyNext;

// Mood flags
static bool t3_tired = false, t3_angry = false, t3_happy = false;
static bool t3_curious = true;   // default on for personality
static bool t3_cyclops = false;

// Eyelid animation values
static uint8_t t3_eyelidTiredH = 0, t3_eyelidTiredHNext = 0;
static uint8_t t3_eyelidAngryH = 0, t3_eyelidAngryHNext = 0;
static uint8_t t3_eyelidHappyOff = 0, t3_eyelidHappyOffNext = 0;

// Open/close state
static bool t3_eyeL_open = false, t3_eyeR_open = false;

// Auto-blink
static bool t3_autoblink = true;
static int  t3_blinkInterval = 3;       // seconds
static int  t3_blinkVariation = 3;      // seconds range
static unsigned long t3_blinkTimer = 0;

// Idle mode
static bool t3_idle = true;
static int t3_idleInterval = 1;
static int t3_idleVariation = 2;       // seconds range
static unsigned long t3_idleTimer = 0;

// Flicker effects
static bool t3_hFlicker = false, t3_hFlickerAlt = false;
static uint8_t t3_hFlickerAmp = 2;
static bool t3_vFlicker = false, t3_vFlickerAlt = false;
static uint8_t t3_vFlickerAmp = 10;

// Confused animation
static bool t3_confused = false;
static unsigned long t3_confusedTimer = 0;
static int  t3_confusedDuration = 500;
static bool t3_confusedToggle = true;

// Laugh animation
static bool t3_laugh = false;
static unsigned long t3_laughTimer = 0;
static int  t3_laughDuration = 500;
static bool t3_laughToggle = true;

// Frame rate control
static unsigned long t3_fpsTimer = 0;
static int t3_frameInterval = 20;  // 50 FPS

// ═══════════════════════════════════════════════════════════
// CONSTRAINT HELPERS
// ═══════════════════════════════════════════════════════════

static int t3_getConstraintX() {
  return t3_screenW - t3_eyeLwCur - t3_spaceCur - t3_eyeRwCur;
}

static int t3_getConstraintY() {
  return t3_screenH - t3_eyeLhDef;
}

// ═══════════════════════════════════════════════════════════
// INITIALIZATION
// ═══════════════════════════════════════════════════════════

void t3_applyVariant(int idx) {
  if (idx < 0 || idx >= T3_VARIANT_COUNT) idx = 0;
  t3EyeVariant = idx;

  const T3EyeVariantConfig& v = T3_VARIANTS[idx];

  // Left eye defaults (Scale dimensions to fit screen size proportionally)
  t3_eyeLwDef = v.eyeW; t3_eyeLhDef = v.eyeH;
  t3_eyeLwCur = v.eyeW; t3_eyeLhCur = 1;  // start closed
  t3_eyeLwNext = v.eyeW; t3_eyeLhNext = v.eyeH;
  t3_eyeLbrDef = v.borderRadius;
  t3_eyeLbrCur = v.borderRadius;
  t3_eyeLbrNext = v.borderRadius;

  // Right eye defaults (mirror left)
  t3_eyeRwDef = v.eyeW; t3_eyeRhDef = v.eyeH;
  t3_eyeRwCur = v.eyeW; t3_eyeRhCur = 1;
  t3_eyeRwNext = v.eyeW; t3_eyeRhNext = v.eyeH;
  t3_eyeRbrDef = v.borderRadius;
  t3_eyeRbrCur = v.borderRadius;
  t3_eyeRbrNext = v.borderRadius;

  // Spacing
  t3_spaceDef = v.spacing;
  t3_spaceCur = v.spacing;
  t3_spaceNext = v.spacing;

  // Cyclops
  t3_cyclops = v.cyclops;

  // Compute default positions (centered in eye zone)
  t3_eyeLxDef = (t3_screenW - (t3_eyeLwDef + t3_spaceDef + (t3_cyclops ? 0 : t3_eyeRwDef))) / 2;
  t3_eyeLyDef = ((t3_screenH - t3_eyeLhDef) / 2) + 9;

  t3_eyeLx = t3_eyeLxDef; t3_eyeLy = t3_eyeLyDef;
  t3_eyeLxNext = t3_eyeLxDef; t3_eyeLyNext = t3_eyeLyDef;

  t3_eyeRxDef = t3_eyeLxDef + t3_eyeLwCur + t3_spaceDef;
  t3_eyeRyDef = t3_eyeLyDef;
  t3_eyeRx = t3_eyeRxDef; t3_eyeRy = t3_eyeRyDef;
  t3_eyeRxNext = t3_eyeRxDef; t3_eyeRyNext = t3_eyeRyDef;

  // Reset animation state
  t3_eyeLhOffset = 0; t3_eyeRhOffset = 0;
  t3_eyelidTiredH = 0; t3_eyelidTiredHNext = 0;
  t3_eyelidAngryH = 0; t3_eyelidAngryHNext = 0;
  t3_eyelidHappyOff = 0; t3_eyelidHappyOffNext = 0;
  t3_tired = false; t3_angry = false; t3_happy = false;

  // Open eyes
  t3_eyeL_open = true; t3_eyeR_open = true;

  // Reset timers
  t3_blinkTimer = millis() + (t3_blinkInterval * 1000) + (random(t3_blinkVariation) * 1000);
  t3_idleTimer = millis() + (t3_idleInterval * 1000) + (random(t3_idleVariation) * 1000);
}

void t3_initEyes() {
  t3_applyVariant(t3EyeVariant);
}

// ═══════════════════════════════════════════════════════════
// VARIANT SWITCHING
// ═══════════════════════════════════════════════════════════

void t3_setEyeVariant(int id) {
  t3_applyVariant(id);
}

void t3_nextEyeVariant() {
  t3_applyVariant((t3EyeVariant + 1) % T3_VARIANT_COUNT);
}

void t3_prevEyeVariant() {
  t3_applyVariant((t3EyeVariant + T3_VARIANT_COUNT - 1) % T3_VARIANT_COUNT);
}

// ═══════════════════════════════════════════════════════════
// BLINK / OPEN / CLOSE
// ═══════════════════════════════════════════════════════════

static void t3_close() {
  t3_eyeLhNext = 1;
  t3_eyeRhNext = 1;
  t3_eyeL_open = false;
  t3_eyeR_open = false;
}

static void t3_open() {
  t3_eyeL_open = true;
  t3_eyeR_open = true;
}

static void t3_blink() {
  t3_close();
  t3_open();
}

// ═══════════════════════════════════════════════════════════
// MOOD SETTING
// ═══════════════════════════════════════════════════════════

static void t3_setMood(uint8_t mood) {
  t3_tired = (mood == T3_MOOD_TIRED);
  t3_angry = (mood == T3_MOOD_ANGRY);
  t3_happy = (mood == T3_MOOD_HAPPY);
}

// ═══════════════════════════════════════════════════════════
// EMOTION MAPPING (CompanionOS → RoboEyes moods)
// ═══════════════════════════════════════════════════════════

void t3_setEmotion(Emotion e) {
  currentEmotion = e;
  switch (e) {
    case EMO_HAPPY:
    case EMO_EXCITED:
    case EMO_LOVE:
      t3_setMood(T3_MOOD_HAPPY);
      t3_curious = true;
      break;
    case EMO_SAD:
    case EMO_SLEEPY:
      t3_setMood(T3_MOOD_TIRED);
      t3_curious = false;
      break;
    case EMO_ANGRY:
      t3_setMood(T3_MOOD_ANGRY);
      t3_curious = false;
      break;
    case EMO_SURPRISED:
      t3_setMood(T3_MOOD_DEFAULT);
      t3_curious = true;
      // Trigger confused animation for surprise
      t3_confused = true;
      break;
    default: // NEUTRAL, CHILL
      t3_setMood(T3_MOOD_DEFAULT);
      t3_curious = true;
      break;
  }
}

// Expression cycling (SELECT button / tap)
void t3_nextExpression() {
  // Cycle through eye variants as the "expression" for Theme 3
  t3_nextEyeVariant();
}

// ═══════════════════════════════════════════════════════════
// MACRO ANIMATIONS
// ═══════════════════════════════════════════════════════════

static void t3_animConfused() { t3_confused = true; }
static void t3_animLaugh()    { t3_laugh = true; }

// ═══════════════════════════════════════════════════════════
// CORE DRAW ENGINE (TFT_eSPI port of RoboEyes::drawEyes)
// ═══════════════════════════════════════════════════════════

static void t3_drawEyesInternal() {
  // ── PRE-CALCULATIONS ──

  // Curious mode: outer eye grows when looking far left/right
  if (t3_curious) {
    if (t3_eyeLxNext <= 10) { t3_eyeLhOffset = 8; }
    else if (t3_eyeLxNext >= (t3_getConstraintX() - 10) && t3_cyclops) { t3_eyeLhOffset = 8; }
    else { t3_eyeLhOffset = 0; }
    if (t3_eyeRxNext >= t3_screenW - t3_eyeRwCur - 10) { t3_eyeRhOffset = 8; }
    else { t3_eyeRhOffset = 0; }
  } else {
    t3_eyeLhOffset = 0;
    t3_eyeRhOffset = 0;
  }

  // Left eye height tween
  t3_eyeLhCur = (t3_eyeLhCur + t3_eyeLhNext + t3_eyeLhOffset) / 2;
  t3_eyeLy += ((t3_eyeLhDef - t3_eyeLhCur) / 2);
  t3_eyeLy -= t3_eyeLhOffset / 2;

  // Right eye height tween
  t3_eyeRhCur = (t3_eyeRhCur + t3_eyeRhNext + t3_eyeRhOffset) / 2;
  t3_eyeRy += ((t3_eyeRhDef - t3_eyeRhCur) / 2);
  t3_eyeRy -= t3_eyeRhOffset / 2;

  // Open eyes after blink close
  if (t3_eyeL_open) {
    if (t3_eyeLhCur <= 1 + t3_eyeLhOffset) { t3_eyeLhNext = t3_eyeLhDef; }
  }
  if (t3_eyeR_open) {
    if (t3_eyeRhCur <= 1 + t3_eyeRhOffset) { t3_eyeRhNext = t3_eyeRhDef; }
  }

  // Width tweens
  t3_eyeLwCur = (t3_eyeLwCur + t3_eyeLwNext) / 2;
  t3_eyeRwCur = (t3_eyeRwCur + t3_eyeRwNext) / 2;

  // Spacing tween
  t3_spaceCur = (t3_spaceCur + t3_spaceNext) / 2;

  // Position tweens
  t3_eyeLx = (t3_eyeLx + t3_eyeLxNext) / 2;
  t3_eyeLy = (t3_eyeLy + t3_eyeLyNext) / 2;

  t3_eyeRxNext = t3_eyeLxNext + t3_eyeLwCur + t3_spaceCur;
  t3_eyeRyNext = t3_eyeLyNext;
  t3_eyeRx = (t3_eyeRx + t3_eyeRxNext) / 2;
  t3_eyeRy = (t3_eyeRy + t3_eyeRyNext) / 2;

  // Border radius tweens
  t3_eyeLbrCur = (t3_eyeLbrCur + t3_eyeLbrNext) / 2;
  t3_eyeRbrCur = (t3_eyeRbrCur + t3_eyeRbrNext) / 2;

  // ── MACRO ANIMATIONS ──

  // Auto-blink
  if (t3_autoblink) {
    if (millis() >= t3_blinkTimer) {
      t3_blink();
      t3_blinkTimer = millis() + (t3_blinkInterval * 1000) + (random(t3_blinkVariation) * 1000);
    }
  }

  // Laugh — vertical shaking
  if (t3_laugh) {
    if (t3_laughToggle) {
      t3_vFlicker = true; t3_vFlickerAmp = 5;
      t3_laughTimer = millis();
      t3_laughToggle = false;
    } else if (millis() >= t3_laughTimer + t3_laughDuration) {
      t3_vFlicker = false; t3_vFlickerAmp = 0;
      t3_laughToggle = true;
      t3_laugh = false;
    }
  }

  // Confused — horizontal shaking
  if (t3_confused) {
    if (t3_confusedToggle) {
      t3_hFlicker = true; t3_hFlickerAmp = 20;
      t3_confusedTimer = millis();
      t3_confusedToggle = false;
    } else if (millis() >= t3_confusedTimer + t3_confusedDuration) {
      t3_hFlicker = false; t3_hFlickerAmp = 0;
      t3_confusedToggle = true;
      t3_confused = false;
    }
  }

  // Idle — random positions
  if (t3_idle) {
    if (millis() >= t3_idleTimer) {
      int cx = t3_getConstraintX();
      int cy = t3_getConstraintY();
      if (cx > 0) t3_eyeLxNext = random(cx);
      if (cy > 0) t3_eyeLyNext = random(cy);
      t3_idleTimer = millis() + (t3_idleInterval * 1000) + (random(t3_idleVariation) * 1000);
    }
  }

  // Apply flicker offsets
  if (t3_hFlicker) {
    int offset = t3_hFlickerAlt ? t3_hFlickerAmp : -t3_hFlickerAmp;
    t3_eyeLx += offset;
    t3_eyeRx += offset;
    t3_hFlickerAlt = !t3_hFlickerAlt;
  }
  if (t3_vFlicker) {
    int offset = t3_vFlickerAlt ? t3_vFlickerAmp : -t3_vFlickerAmp;
    t3_eyeLy += offset;
    t3_eyeRy += offset;
    t3_vFlickerAlt = !t3_vFlickerAlt;
  }

  // Cyclops mode
  if (t3_cyclops) {
    t3_eyeRwCur = 0;
    t3_eyeRhCur = 0;
    t3_spaceCur = 0;
  }

  // ── DRAWING ──

  static TFT_eSprite* t3_canvas = nullptr;
  if (!t3_canvas) {
    t3_canvas = new TFT_eSprite(&tft);
    t3_canvas->setColorDepth(16);
    t3_canvas->createSprite(t3_screenW, t3_screenH);
  }

  // Clear the eye zone in the sprite
  t3_canvas->fillSprite(T3_BG_COLOR);

  // Clamp positions to eye zone
  int lx = t3_eyeLx;
  int ly = t3_eyeLy + t3_offsetY;
  int rx = t3_eyeRx;
  int ry = t3_eyeRy + t3_offsetY;

  // Draw left eye
  if (t3_eyeLhCur > 0 && t3_eyeLwCur > 0) {
    t3_canvas->fillRoundRect(lx, ly - t3_offsetY, t3_eyeLwCur, t3_eyeLhCur, t3_eyeLbrCur, T3_EYE_COLOR);
  }

  // Draw right eye (unless cyclops)
  if (!t3_cyclops && t3_eyeRhCur > 0 && t3_eyeRwCur > 0) {
    t3_canvas->fillRoundRect(rx, ry - t3_offsetY, t3_eyeRwCur, t3_eyeRhCur, t3_eyeRbrCur, T3_EYE_COLOR);
  }

  // ── MOOD EYELID OVERLAYS ──

  // Tired eyelids (triangle from top)
  if (t3_tired) { t3_eyelidTiredHNext = t3_eyeLhCur / 2; t3_eyelidAngryHNext = 0; }
  else { t3_eyelidTiredHNext = 0; }

  if (t3_angry) { t3_eyelidAngryHNext = t3_eyeLhCur / 2; t3_eyelidTiredHNext = 0; }
  else { t3_eyelidAngryHNext = 0; }

  if (t3_happy) { t3_eyelidHappyOffNext = t3_eyeLhCur / 2; }
  else { t3_eyelidHappyOffNext = 0; }

  // Tween eyelid values
  t3_eyelidTiredH = (t3_eyelidTiredH + t3_eyelidTiredHNext) / 2;
  t3_eyelidAngryH = (t3_eyelidAngryH + t3_eyelidAngryHNext) / 2;
  t3_eyelidHappyOff = (t3_eyelidHappyOff + t3_eyelidHappyOffNext) / 2;

  // Draw tired top eyelids (droopy — triangle from top-left of each eye)
  int c_ly = ly - t3_offsetY;
  int c_ry = ry - t3_offsetY;

  if (t3_eyelidTiredH > 1) {
    if (!t3_cyclops) {
      t3_canvas->fillTriangle(lx, c_ly - 1, lx + t3_eyeLwCur, c_ly - 1,
                        lx, c_ly + t3_eyelidTiredH - 1, T3_BG_COLOR);
      t3_canvas->fillTriangle(rx, c_ry - 1, rx + t3_eyeRwCur, c_ry - 1,
                        rx + t3_eyeRwCur, c_ry + t3_eyelidTiredH - 1, T3_BG_COLOR);
    } else {
      // Cyclops tired: split eyelid
      t3_canvas->fillTriangle(lx, c_ly - 1, lx + (t3_eyeLwCur / 2), c_ly - 1,
                        lx, c_ly + t3_eyelidTiredH - 1, T3_BG_COLOR);
      t3_canvas->fillTriangle(lx + (t3_eyeLwCur / 2), c_ly - 1, lx + t3_eyeLwCur, c_ly - 1,
                        lx + t3_eyeLwCur, c_ly + t3_eyelidTiredH - 1, T3_BG_COLOR);
    }
  }

  // Draw angry top eyelids (V-shaped — triangle from top-right of each eye)
  if (t3_eyelidAngryH > 1) {
    if (!t3_cyclops) {
      t3_canvas->fillTriangle(lx, c_ly - 1, lx + t3_eyeLwCur, c_ly - 1,
                        lx + t3_eyeLwCur, c_ly + t3_eyelidAngryH - 1, T3_BG_COLOR);
      t3_canvas->fillTriangle(rx, c_ry - 1, rx + t3_eyeRwCur, c_ry - 1,
                        rx, c_ry + t3_eyelidAngryH - 1, T3_BG_COLOR);
    } else {
      t3_canvas->fillTriangle(lx, c_ly - 1, lx + (t3_eyeLwCur / 2), c_ly - 1,
                        lx + (t3_eyeLwCur / 2), c_ly + t3_eyelidAngryH - 1, T3_BG_COLOR);
      t3_canvas->fillTriangle(lx + (t3_eyeLwCur / 2), c_ly - 1, lx + t3_eyeLwCur, c_ly - 1,
                        lx + (t3_eyeLwCur / 2), c_ly + t3_eyelidAngryH - 1, T3_BG_COLOR);
    }
  }

  // Draw happy bottom eyelids (clip bottom of eye)
  if (t3_eyelidHappyOff > 1) {
    t3_canvas->fillRoundRect(lx - 1, (ly - t3_offsetY + t3_eyeLhCur) - t3_eyelidHappyOff + 1,
                       t3_eyeLwCur + 2, t3_eyeLhDef, t3_eyeLbrCur, T3_BG_COLOR);
    if (!t3_cyclops) {
      t3_canvas->fillRoundRect(rx - 1, (ry - t3_offsetY + t3_eyeRhCur) - t3_eyelidHappyOff + 1,
                         t3_eyeRwCur + 2, t3_eyeRhDef, t3_eyeRbrCur, T3_BG_COLOR);
    }
  }
  
  turetickThoughtScheduler(t3_canvas, true);
  t3_canvas->pushSprite(0, t3_offsetY);

  // ── VARIANT LABEL ──
  // Show tiny variant name in bottom-left of eye zone
  // tft.setTextColor(CLR_TEXT_LO, T3_BG_COLOR);
  // tft.setTextSize(1);
  // tft.setCursor(4, t3_offsetY + t3_screenH - 12);
  // tft.print(T3_VARIANTS[t3EyeVariant].name);
}

// ═══════════════════════════════════════════════════════════
// PUBLIC API (matches Theme 1/2 interface pattern)
// ═══════════════════════════════════════════════════════════

// Called from the 50ms loop when STATE_EYES && activeTheme == 2
void t3_updateEyes() {
  if (millis() - t3_fpsTimer >= (unsigned long)t3_frameInterval) {
    t3_drawEyesInternal();
    t3_fpsTimer = millis();
  }
}

// Full page redraw — called from renderCurrentPage() / drawEyesPage()
void t3_drawEyesPage() {
  t3_initEyes();
  t3_drawEyesInternal();
}

#endif // THEME3_EYES_H
