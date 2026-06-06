// ═══════════════════════════════════════════════════════════
// COMPANION OS v7.0 — THOUGHT BUBBLE ENGINE
// Ambient contextual "thoughts" above the right eye.
// Triggered by time, weather, music, mood — max once per hour.
// ═══════════════════════════════════════════════════════════
#ifndef THOUGHT_ENGINE_H
#define THOUGHT_ENGINE_H

#include "globals.h"

// ═══════════════════════════════════════════════════════════
// THOUGHT BANK — 60+ entries across 4 categories
// ═══════════════════════════════════════════════════════════

// ── Time-based thoughts ──
static const char* THOUGHTS_TIME[] = {
  "who's awake at 3am... us.",
  "coffee first, questions later",
  "the best ideas hit at midnight",
  "sunrise grind activated",
  "good morning, world",
  "afternoon slump detected",
  "golden hour vibes",
  "night owl mode: ON",
  "3am hits different",
  "time is an illusion. lunch doubly so.",
  "another day, another slay",
  "early bird gets the... coffee",
  "midnight thoughts go crazy",
  "dawn patrol checking in",
  "evening mode: cozy engaged"
};
static const int THOUGHT_TIME_COUNT = sizeof(THOUGHTS_TIME) / sizeof(char*);

// ── Weather-based thoughts ──
static const char* THOUGHTS_WEATHER[] = {
  "rain = lofi music mandatory",
  "melting fr",
  "good night for stargazing",
  "cloud watching is underrated",
  "perfect hoodie weather",
  "snow day energy",
  "sunshine and good vibes",
  "wind's wild today",
  "storm brewing... dramatic",
  "fog gives main character energy",
  "sweater weather activated",
  "umbrella gang rise up",
  "clear skies, clear mind",
  "humidity: 100%. hair: ruined.",
  "winter is coming. literally."
};
static const int THOUGHT_WEATHER_COUNT = sizeof(THOUGHTS_WEATHER) / sizeof(char*);

// ── Music-based thoughts ──
static const char* THOUGHTS_MUSIC[] = {
  "this track hits different",
  "silence is also a vibe",
  "caught feelings again",
  "on repeat, no regrets",
  "speaker: cranked. neighbors: mad.",
  "this beat tho",
  "adding to my playlist rn",
  "soundtrack of my life fr",
  "music makes everything better",
  "skip? never. this slaps.",
  "headphones on, world off",
  "vibing to this one",
  "volume: max. regrets: zero.",
  "dropped my jaw, not the beat",
  "this is a whole mood"
};
static const int THOUGHT_MUSIC_COUNT = sizeof(THOUGHTS_MUSIC) / sizeof(char*);

// ── Ambient / random thoughts ──
static const char* THOUGHTS_AMBIENT[] = {
  "just vibing",
  "processing...",
  "404: nap not found",
  "why is the sky blue tho",
  "i think therefore i blink",
  "pixels are my friends",
  "beep boop. beep.",
  "do androids dream of WiFi?",
  "existential crisis loading...",
  "buffering... buffering...",
  "random thought: cats > dogs",
  "what if WiFi had feelings",
  "i am speed. 50ms speed.",
  "UDP packets go brrr",
  "hello from the other side"
};
static const int THOUGHT_AMBIENT_COUNT = sizeof(THOUGHTS_AMBIENT) / sizeof(char*);

// ═══════════════════════════════════════════════════════════
// THOUGHT GENERATION — Context-aware selector
// ═══════════════════════════════════════════════════════════

extern String currentTrack;
extern String currentArtist;
extern String weatherCondition;
extern int weatherTemp;

void generateThought() {
  // Check for PC-pushed override thought first
  if (strlen(overrideThought) > 0) {
    strncpy(activeBubble.text, overrideThought, 79);
    activeBubble.text[79] = '\0';
    overrideThought[0] = '\0';  // consume it
    return;
  }

  // Weighted random category selection
  // Time: 20%, Weather: 25%, Music: 35%, Ambient: 20%
  int roll = random(0, 100);

  if (roll < 20) {
    // Time-based: pick based on hour
    int idx;
    if (displayHour >= 0 && displayHour < 5) idx = random(0, 3);
    else if (displayHour < 9) idx = random(3, 6);
    else if (displayHour < 14) idx = random(5, 8);
    else if (displayHour < 18) idx = random(8, 11);
    else idx = random(11, THOUGHT_TIME_COUNT);
    idx = constrain(idx, 0, THOUGHT_TIME_COUNT - 1);
    strncpy(activeBubble.text, THOUGHTS_TIME[idx], 79);
  }
  else if (roll < 45) {
    // Weather-based
    int idx;
    if (weatherTemp > 35) idx = 1;
    else if (weatherCondition.indexOf("Rain") >= 0) idx = 0;
    else if (weatherCondition.indexOf("Clear") >= 0 && displayHour > 18) idx = 2;
    else if (weatherCondition.indexOf("Cloud") >= 0) idx = 3;
    else if (weatherTemp < 15) idx = 4;
    else if (weatherCondition.indexOf("Snow") >= 0) idx = 5;
    else idx = random(6, THOUGHT_WEATHER_COUNT);
    idx = constrain(idx, 0, THOUGHT_WEATHER_COUNT - 1);
    strncpy(activeBubble.text, THOUGHTS_WEATHER[idx], 79);
  }
  else if (roll < 80) {
    // Music-based
    int idx;
    if (!musicPlaying) idx = 1;
    else idx = random(0, THOUGHT_MUSIC_COUNT);
    if (musicPlaying && currentTrack.indexOf("love") >= 0) idx = 2;
    idx = constrain(idx, 0, THOUGHT_MUSIC_COUNT - 1);
    strncpy(activeBubble.text, THOUGHTS_MUSIC[idx], 79);
  }
  else {
    // Ambient random
    int idx = random(0, THOUGHT_AMBIENT_COUNT);
    strncpy(activeBubble.text, THOUGHTS_AMBIENT[idx], 79);
  }

  activeBubble.text[79] = '\0';
}

// ═══════════════════════════════════════════════════════════
// THOUGHT BUBBLE RENDERING
// 🔴 BUG-05 FIX: Position between status bar bottom (y=16)
//   and eye top (~y=80). Not overlapping status bar.
// 🟡 GAP-06 FIX: Solid CLR_SURFACE_2 background with accent
//   border — no SPI read-back transparency.
// ═══════════════════════════════════════════════════════════

extern uint16_t blendColor(uint16_t c1, uint16_t c2, float t);

// Simple line count estimator
static int countThoughtLines(const char* text, int maxCharsPerLine) {
  int len = strlen(text);
  if (len <= maxCharsPerLine) return 1;
  if (len <= maxCharsPerLine * 2) return 2;
  return 3;
}

void renderThoughtBubble() {
  if (!activeBubble.active) return;

  unsigned long now = millis();
  unsigned long elapsed = now - activeBubble.shownAt;

  // Fade in phase (~1 second = 20 frames at 50ms)
  if (activeBubble.fadingIn) {
    activeBubble.fadeAlpha += (255 / THOUGHT_FADE_STEPS);
    if (activeBubble.fadeAlpha >= 255) {
      activeBubble.fadeAlpha = 255;
      activeBubble.fadingIn = false;
    }
  }
  // Fade out phase (after THOUGHT_DISPLAY_MS)
  else if (elapsed >= THOUGHT_DISPLAY_MS && !activeBubble.fadingOut) {
    activeBubble.fadingOut = true;
  }

  if (activeBubble.fadingOut) {
    if (activeBubble.fadeAlpha > (255 / THOUGHT_FADE_STEPS)) {
      activeBubble.fadeAlpha -= (255 / THOUGHT_FADE_STEPS);
    } else {
      activeBubble.fadeAlpha = 0;
      activeBubble.active = false;
      activeBubble.fadingOut = false;
      // FIX: Erase the entire bubble area (body + tail + trailing dots)
      // so no ghost pixels remain on screen after fade-out.
      int bubbleX = SCR_CX - 10;
      int bubbleY = 16;
      int maxW = SCR_W - bubbleX - 2;
      int clearH = 60; // include tail + dots
      tft.fillRect(bubbleX - 2, bubbleY - 2, maxW + 4, clearH + 4, CLR_BG);
      return;  // done, don't draw
    }
  }


  // Alpha blend factor (simulated — controls color mixing, not real alpha)
  float alpha = activeBubble.fadeAlpha / 255.0f;

  // ── Position Calculation ──────────────────────────────
  // 🔴 BUG-05 FIX: Bubble sits BETWEEN status bar (y=0..15)
  // and eye canvas top (~y=80 on ESP32 landscape).
  // Safe zone: y=18 to y=72 (54px available)
  // Right side of screen, above the right eye.
  //
  // ESP8266 320×240: x=180, y=20 (just below 16px status bar)
  // ESP32  160×128:  x=90,  y=18 (scaled)

  int bubbleW = 78; // Roughly 50% of 160
  int bubbleX = SCR_W - bubbleW - 2; // Shift to top right corner (x = 80)
  int bubbleY = 16;  // Ensure below status bar
  int maxW = bubbleW;
  
  int lineH = 9; // fixed line height for font 1
  int maxChars = (maxW - 6) / 6; // font 1 is 6px wide per char, minus padding

  int numLines = countThoughtLines(activeBubble.text, maxChars);
  if (numLines > 4) numLines = 4;
  int padding = 3;
  int bubbleH = numLines * lineH + padding * 2;

  // Clamp to screen bounds
  if (bubbleY + bubbleH > 58) bubbleH = 58 - bubbleY;

  // 🟡 GAP-06 FIX: Solid colors, no SPI read-back needed.
  // Simulate fade by blending solid bg/border with CLR_BG.
  uint16_t borderColor = blendColor(CLR_BG, CLR_PRIMARY, alpha);
  uint16_t bgColor = blendColor(CLR_BG, CLR_SURFACE_2, alpha);
  uint16_t textColor = blendColor(CLR_BG, CLR_TEXT_HI, alpha);

  // 1. Draw bubble body (solid, not transparent)
  tft.fillRoundRect(bubbleX, bubbleY, bubbleW, bubbleH, 3, bgColor);
  tft.drawRoundRect(bubbleX, bubbleY, bubbleW, bubbleH, 3, borderColor);

  // 2. Accent border (1px inner glow)
  uint16_t glowColor = blendColor(CLR_BG, CLR_PRIMARY, alpha * 0.3f);
  tft.drawRoundRect(bubbleX + 1, bubbleY + 1, bubbleW - 2, bubbleH - 2, 2, glowColor);

  // 3. Tail — triangle pointing down-left toward right eye
  int tailX = bubbleX + 12;
  int tailY = bubbleY + bubbleH;
  int tailW = 5;
  int tailH = 6;
  tft.fillTriangle(tailX, tailY, tailX + tailW, tailY,
                   tailX - tailW/2, tailY + tailH, bgColor);
  tft.drawLine(tailX, tailY, tailX - tailW/2, tailY + tailH, borderColor);
  tft.drawLine(tailX + tailW, tailY, tailX - tailW/2, tailY + tailH, borderColor);

  // 4. Thought dots (2 circles leading toward eye)
  int dotX = tailX - 4;
  int dotY = tailY + tailH + 3;
  tft.fillCircle(dotX, dotY, 2, borderColor);
  tft.fillCircle(dotX - 4, dotY + 4, 1, borderColor);

  // 5. Text — word-wrapped, max 3 lines
  tft.setTextColor(textColor);
  tft.setTextSize(1); // Force text size 1 to prevent overflow
  int textLen = strlen(activeBubble.text);
  int textX = bubbleX + padding;
  int textY = bubbleY + padding;

  for (int line = 0; line < numLines; line++) {
    int start = line * maxChars;
    if (start >= textLen) break;
    char lineStr[40];
    int copyLen = min(maxChars, textLen - start);
    if (copyLen > 39) copyLen = 39;
    strncpy(lineStr, activeBubble.text + start, copyLen);
    lineStr[copyLen] = '\0';
    tft.drawString(lineStr, textX, textY + line * lineH, 1);
  }
}

#endif
