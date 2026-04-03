#ifndef THEMES_H
#define THEMES_H

#include <Arduino.h>
#include "config.h"

// ═══════════════════════════════════════════════════════════
// THEME SYSTEM V5 — All Data in PROGMEM (4MB Flash)
//
// 11 themes × ~100 bytes each = ~1.1KB in Flash
// Only 1 runtime copy (~48 bytes) lives in RAM
// ═══════════════════════════════════════════════════════════

// Transition \u0026 UI palette (RGB565)
#define TR_BASE    0x0000   // #05060A \u2192 near black
#define TR_PURPLE  0x79FD   // #7C3AED purple accent
#define TR_CYAN    0x269E   // #22D3EE cyan accent
#define TR_PINK    0xF96A   // #FF2DAA pink accent
#define TR_GLOW    0xB37F   // #B56CFF glow
#define TR_NEUTRAL 0xAD55   // #A1A1AA neutral grey
#define TR_WARN    0xFBE0   // #F97316 warning orange
#define TR_SUCCESS 0x3696   // #34D399 success green

// Eye style identifiers \u2014 each gets its own renderer in theme_eyes.h
#define EYE_STYLE_LEGACY    0   // V3 almond cats (original)
#define EYE_STYLE_EXOTIC    1   // V4 with shimmer/scanlines
#define EYE_STYLE_CIRCLE    2   // Pikachu: large filled circles
#define EYE_STYLE_HALF      3   // Chill: soft half-circles
#define EYE_STYLE_DIAMOND   4   // Gaming: sharp diamond slits
#define EYE_STYLE_RING      5   // Minimal: hollow thin rings
#define EYE_STYLE_TRIANGLE  6   // Angry: sharp inward-pointing triangles
#define EYE_STYLE_LINE      7   // Sleep: flat horizontal lines
#define EYE_STYLE_BLOB      8   // Mood: morphing overlapping circles
#define EYE_STYLE_TEXT      9   // System: no eyes, raw telemetry text
#define EYE_STYLE_OVAL     10   // Companion: warm thick ovals

// Background style identifiers
#define BG_STYLE_STARS      0   // Procedural XOR-shift starfield
#define BG_STYLE_AURORA     1   // Stars + aurora sine bands
#define BG_STYLE_SOLID      2   // Flat solid color fill
#define BG_STYLE_GRID       3   // Perspective wireframe grid
#define BG_STYLE_GRADIENT   4   // Vertical gradient
#define BG_STYLE_NOISE      5   // Animated static noise
#define BG_STYLE_VOID       6   // Pure black, nothing drawn
#define BG_STYLE_FLUID      7   // Slow shifting gradient bands

// ── Theme Definition (stored in Flash) ───────────────────
struct ThemeDef {
  // Identity
  char name[16];

  // Colors (RGB565 format)
  uint16_t bg;
  uint16_t surface;
  uint16_t primary;
  uint16_t secondary;
  uint16_t accent;
  uint16_t text;
  uint16_t muted;

  // Eye colors (gradient or fill)
  uint16_t eyeC1;     // Dark/inner
  uint16_t eyeC2;     // Mid
  uint16_t eyeC3;     // Bright/outer
  uint16_t cheekColor; // Blush color (0 = no blush)

  // Physics
  float springK;
  float damping;
  int16_t pupilRangeX;
  int16_t pupilRangeY;
  uint16_t blinkIntervalMs;
  uint8_t blinkSpeed;          // Frames per blink half-cycle

  // Style
  uint8_t eyeStyle;
  uint8_t bgStyle;
  uint8_t defaultEmotion;

  // Feature flags
  bool enableStarfield;
  bool enableAurora;
  bool enableParticles;
  bool enableScanlines;
  bool enableWeatherOverlay;
  bool enableCheeks;
  bool enableScreenShake;
};

// ── Runtime Copy (lives in RAM, ~48 bytes) ───────────────
struct ThemeRuntime {
  uint16_t bg, primary, secondary, accent, text, muted;
  uint16_t eyeC1, eyeC2, eyeC3, cheekColor;
  float springK, damping;
  int16_t pupilRangeX, pupilRangeY;
  uint16_t blinkIntervalMs;
  uint8_t blinkSpeed;
  uint8_t eyeStyle, bgStyle, defaultEmotion;
  bool enableStarfield, enableAurora, enableParticles;
  bool enableScanlines, enableWeatherOverlay;
  bool enableCheeks, enableScreenShake;
};

// Global runtime theme (single RAM instance)
ThemeRuntime activeTheme;

// ═══════════════════════════════════════════════════════════
// PROGMEM THEME TABLE — 11 themes in Flash
// ═══════════════════════════════════════════════════════════

// Helper: Convert 24-bit hex to RGB565
// #RRGGBB → ((R>>3)<<11) | ((G>>2)<<5) | (B>>3)
// Pre-computed below for each theme's JSON palette

const ThemeDef THEMES[THEME_COUNT] PROGMEM = {

  // ── Theme 0: Legacy (V3 Original) ─────────────────────
  {
    "Legacy",
    0x0000, 0x0000,         // bg, surface: black
    0x07FF, 0x033F,         // primary: cyan, secondary: mid-blue
    0xFFFF, 0xFFFF, 0x8410, // accent: white, text: white, muted: grey
    0x0013, 0x033F, 0x07FF, // eyeC1-3: deep blue → mid blue → cyan
    0x0000,                 // cheekColor: none
    0.08f, 0.22f,           // springK, damping
    14, 10,                 // pupilRangeX, Y
    4500, 2,                // blinkInterval, blinkSpeed
    EYE_STYLE_LEGACY, BG_STYLE_STARS, 0,  // style, bg, defaultEmo
    true, false, false, false, false, false, false  // flags
  },

  // ── Theme 1: Exotic (V4 Aurora/Effects) ───────────────
  {
    "Exotic",
    0x0000, 0x0000,
    0x07FF, 0x033F,
    0xFFFF, 0xFFFF, 0x8410,
    0x0013, 0x033F, 0x07FF,
    0x0000,
    0.08f, 0.22f,
    14, 10,
    4500, 2,
    EYE_STYLE_EXOTIC, BG_STYLE_AURORA, 0,
    true, true, true, true, true, false, false
  },

  // ── Theme 2: Pikachu (High Energy & Electric) ─────────
  // JSON: bg=#F6CA35, primary=#E22E21, eye=large circles
  // Physics: high spring, low damping, fast blinks
  {
    "Pikachu",
    0xF6A6, 0xFEF1,         // bg: #F6CA35→0xF6A6, surface: #FCE578
    0xE144, 0x0000,         // primary: #E22E21→red, secondary: black
    0xFFFF, 0x0000, 0x5A40, // accent: white, text: black, muted: #5C4A08
    0xE144, 0xFEA0, 0xFFFF, // eyeC1: red, eyeC2: yellow, eyeC3: white
    0xF800,                 // cheekColor: bright red
    0.18f, 0.12f,           // springK HIGH, damping LOW (bouncy)
    18, 12,                 // pupilRange: wide darting
    2500, 1,                // blinkInterval: fast, blinkSpeed: snap
    EYE_STYLE_CIRCLE, BG_STYLE_SOLID, 1,  // circles, solid yellow bg, happy
    false, false, false, false, false, true, false
  },

  // ── Theme 3: Chill / Night Mode (Soft & Sleepy) ───────
  // JSON: bg=#0F111A, primary=#8A9BEE, eye=soft half-circles
  // Physics: very low spring, heavy damping, slow blinks
  {
    "Chill",
    0x0883, 0x18E5,         // bg: #0F111A→dark, surface: #1A1D2B
    0x8CDD, 0x5B13,         // primary: #8A9BEE→soft indigo, sec: #5C6A9C
    0xB5FF, 0xE0FF, 0x7AAA, // accent, text, muted
    0x8CDD, 0x5B13, 0xB5FF, // eyeC1-3: indigo gradient
    0x0000,
    0.03f, 0.40f,           // springK VERY LOW, damping HIGH (viscous)
    8, 6,                   // pupilRange: small slow drift
    8000, 4,                // blinkInterval: rare, blinkSpeed: slow fade
    EYE_STYLE_HALF, BG_STYLE_AURORA, 3,  // half-circles, aurora bg, calm
    true, true, true, false, true, false, false
  },

  // ── Theme 4: Gaming Mode (Sharp & Neon Cyberpunk) ─────
  // JSON: bg=#050505, primary=#00FF41, eye=sharp diamond slits
  // Physics: instant snap, zero bounce, heavy jitter
  {
    "Gaming",
    0x0000, 0x1082,         // bg: pure black, surface: #111111
    0x07E8, 0xF800,         // primary: #00FF41 neon green, sec: #FF003C red
    0x07FF, 0xFFFF, 0x4208, // accent: cyan, text: white, muted: #444
    0x07E8, 0x07FF, 0xFFFF, // eyeC1: green, eyeC2: cyan, eyeC3: white
    0x0000,
    0.50f, 0.90f,           // springK MAX, damping CRITICAL (instant snap)
    16, 10,
    3000, 1,                // blink: robotic interval, instant
    EYE_STYLE_DIAMOND, BG_STYLE_GRID, 8,  // diamonds, grid bg, focused
    false, false, false, true, true, false, false  // scanlines ON
  },

  // ── Theme 5: Minimal / Clean Mode (Elegant & Silent) ──
  // JSON: bg=#FFFFFF, primary=#000000, eye=hollow thin rings
  // Physics: smooth easing, critical damp, no bounce
  {
    "Minimal",
    0xFFFF, 0xF79E,         // bg: white, surface: #F5F5F7
    0x0000, 0x8C71,         // primary: black, secondary: #8E8E93
    0x051F, 0x0000, 0x8C71, // accent: #007AFF, text: black, muted: grey
    0x0000, 0x4208, 0x8410, // eyeC1: black, eyeC2: dark grey, eyeC3: grey
    0x0000,
    0.10f, 0.30f,           // springK medium, damping CRITICAL (no bounce)
    10, 7,
    5000, 3,                // blink: moderate rhythmic, smooth
    EYE_STYLE_RING, BG_STYLE_SOLID, 0,  // rings, solid white, neutral
    false, false, false, false, false, false, false  // NO effects
  },

  // ── Theme 6: Angry / Overstimulated (Aggressive) ──────
  // JSON: bg=#330000, primary=#FF0000, eye=sharp inward triangles
  // Physics: extreme spring, chaotic bounce, heavy shake
  {
    "Angry",
    0x3000, 0x4800,         // bg: #330000 dark red, surface: #4A0000
    0xF800, 0xFAC0,         // primary: pure red, secondary: #FF5500 orange
    0xFFFF, 0xF9A6, 0x8800, // accent: white, text: #FF3333, muted: #880000
    0xF800, 0xFA00, 0xFFFF, // eyeC1: red, eyeC2: orange, eyeC3: white
    0x0000,
    0.25f, 0.10f,           // springK EXTREME, damping LOW (chaotic)
    20, 14,                 // pupilRange: erratic wide
    2000, 1,                // blink: fast twitchy
    EYE_STYLE_TRIANGLE, BG_STYLE_NOISE, 7,  // triangles, noise bg, angry
    false, false, true, false, false, false, true  // particles + SHAKE
  },

  // ── Theme 7: Sleep Mode (Void & Dormant) ──────────────
  // JSON: bg=#000000, primary=#333333, eye=closed flat lines
  // Physics: glacially slow, overdamped
  {
    "Sleep",
    0x0000, 0x0841,         // bg: pitch black, surface: #050505
    0x31A6, 0x2104,         // primary: #333333, secondary: #222222
    0x4228, 0x528A, 0x31A6, // accent, text: #555555, muted
    0x31A6, 0x2104, 0x4228, // eyeC1-3: dim greys
    0x0000,
    0.01f, 0.50f,           // springK MINIMAL, damping MAX
    3, 2,                   // pupilRange: almost none
    0, 0,                   // blink: none (already closed)
    EYE_STYLE_LINE, BG_STYLE_VOID, 5,  // lines, void bg, sleepy
    false, false, false, false, false, false, false  // ALL OFF
  },

  // ── Theme 8: Mood Reactive (Fluid & Aesthetic) ────────
  // JSON: bg=#D2A2FF, primary=#4A00E0, eye=morphing blobs
  // Physics: fluid sloshing, high damping, organic
  {
    "Mood",
    0xD51F, 0xEDBF,         // bg: #D2A2FF lavender, surface: #EAC6FF
    0x4816, 0x8D7C,         // primary: #4A00E0 purple, sec: #8E2DE2
    0xF81F, 0xFBBF, 0xE71C, // accent: #FF007F, text: #FAFAFA, muted
    0x4816, 0x8D7C, 0xF81F, // eyeC1: purple, eyeC2: violet, eyeC3: pink
    0x0000,
    0.05f, 0.35f,           // springK LOW fluid, damping HIGH viscous
    12, 8,
    6000, 4,                // blink: slow irregular, slow merge
    EYE_STYLE_BLOB, BG_STYLE_FLUID, 3,  // blobs, fluid bg, calm
    false, false, true, false, true, false, false  // particles + weather
  },

  // ── Theme 9: System / Debug (Diagnostic) ──────────────
  // JSON: bg=#0000AA, primary=#FFFFFF, eye=text telemetry
  // Physics: bypass (step updates only)
  {
    "System",
    0x0015, 0x0011,         // bg: #0000AA blue, surface: #000088
    0xFFFF, 0xAD55,         // primary: white, secondary: #AAAAAA
    0xFFE0, 0xFFFF, 0xAD55, // accent: yellow, text: white, muted: grey
    0xFFFF, 0xAD55, 0xFFE0, // eyeC1: white, eyeC2: grey, eyeC3: yellow
    0x0000,
    0.50f, 0.90f,           // instant snap (bypass physics feel)
    10, 6,
    0, 0,                   // blink: none (no eyes)
    EYE_STYLE_TEXT, BG_STYLE_SOLID, 0,  // text readout, solid blue, neutral
    false, false, false, false, false, false, false
  },

  // ── Theme 10: Companion (Rich & Alive) ────────────────
  // JSON: bg=#FFE8D6, primary=#FF7B54, eye=warm thick ovals
  // Physics: organic mix of soft breathing + snappy attention
  {
    "Companion",
    0xFF36, 0xFFF3,         // bg: #FFE8D6 peach, surface: #FFF3E6
    0xFBCA, 0xFD0F,         // primary: #FF7B54 coral, sec: #FFA07A salmon
    0xFEB6, 0x4A06, 0x8B8B, // accent: #FFD56F gold, text: #4A3F35, muted
    0xFBCA, 0xFD0F, 0xFEB6, // eyeC1: coral, eyeC2: salmon, eyeC3: gold
    0xFB2C,                 // cheekColor: warm pink blush
    0.12f, 0.18f,           // springK organic med, damping light bounce
    14, 10,
    4000, 2,                // blink: contextual, squish compress
    EYE_STYLE_OVAL, BG_STYLE_GRADIENT, 1,  // ovals, gradient bg, happy
    false, false, true, false, true, true, false  // particles + weather + cheeks
  }
};

// ── Theme Name Table (for settings UI) ──────────────────
const char THEME_NAMES[THEME_COUNT][16] PROGMEM = {
  "Legacy", "Exotic", "Pikachu", "Chill",
  "Gaming", "Minimal", "Angry", "Sleep",
  "Mood", "System", "Companion"
};

// ═══════════════════════════════════════════════════════════
// THEME LOADER — Copy from PROGMEM → RAM
// ═══════════════════════════════════════════════════════════

void loadTheme(uint8_t id) {
  if (id >= THEME_COUNT) id = 0;

  ThemeDef def;
  memcpy_P(&def, &THEMES[id], sizeof(ThemeDef));

  activeTheme.bg              = def.bg;
  activeTheme.primary         = def.primary;
  activeTheme.secondary       = def.secondary;
  activeTheme.accent          = def.accent;
  activeTheme.text            = def.text;
  activeTheme.muted           = def.muted;
  activeTheme.eyeC1           = def.eyeC1;
  activeTheme.eyeC2           = def.eyeC2;
  activeTheme.eyeC3           = def.eyeC3;
  activeTheme.cheekColor      = def.cheekColor;
  activeTheme.springK         = def.springK;
  activeTheme.damping         = def.damping;
  activeTheme.pupilRangeX     = def.pupilRangeX;
  activeTheme.pupilRangeY     = def.pupilRangeY;
  activeTheme.blinkIntervalMs = def.blinkIntervalMs;
  activeTheme.blinkSpeed      = def.blinkSpeed;
  activeTheme.eyeStyle        = def.eyeStyle;
  activeTheme.bgStyle         = def.bgStyle;
  activeTheme.defaultEmotion  = def.defaultEmotion;
  activeTheme.enableStarfield      = def.enableStarfield;
  activeTheme.enableAurora         = def.enableAurora;
  activeTheme.enableParticles      = def.enableParticles;
  activeTheme.enableScanlines      = def.enableScanlines;
  activeTheme.enableWeatherOverlay = def.enableWeatherOverlay;
  activeTheme.enableCheeks         = def.enableCheeks;
  activeTheme.enableScreenShake    = def.enableScreenShake;
}

// Get theme name from PROGMEM
void getThemeName(uint8_t id, char* buf, uint8_t bufLen) {
  if (id >= THEME_COUNT) id = 0;
  strncpy_P(buf, THEME_NAMES[id], bufLen - 1);
  buf[bufLen - 1] = '\0';
}

// Get theme primary color from PROGMEM (for settings UI dots)
uint16_t getThemePrimaryColor(uint8_t id) {
  if (id >= THEME_COUNT) id = 0;
  ThemeDef def;
  memcpy_P(&def, &THEMES[id], sizeof(ThemeDef));
  return def.primary;
}

#endif
