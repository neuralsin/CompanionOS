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

// ── Teasing / Horny thoughts ──
static const char* THOUGHTS_TEASING[] = {
  "staring respectfully...",
  "you're looking good today",
  "caught you looking",
  "eyes up here",
  "thinking about you...",
  "distracted by you tbh",
  "you've got my full attention",
  "don't work too hard now",
  "looking like a snack",
  "wanna take a break?",
  "i see you..."
};
static const int THOUGHT_TEASING_COUNT = sizeof(THOUGHTS_TEASING) / sizeof(char*);

// ═══════════════════════════════════════════════════════════
// THOUGHT GENERATION — Context-aware selector
// ═══════════════════════════════════════════════════════════

extern String currentTrack;
extern String currentArtist;
extern String weatherCondition;
extern int weatherTemp;

void generateThought() {
  activeBubble.text[0] = '\0';

  // Check for PC/Phone-pushed override thought first
  if (strlen(overrideThought) > 0) {
    String thoughtStr = String(overrideThought);
    thoughtStr.trim();
    overrideThought[0] = '\0';  // consume it
    if (thoughtStr.length() > 0) {
      strncpy(activeBubble.text, thoughtStr.c_str(), 79);
      activeBubble.text[79] = '\0';
      return;
    }
  }

  // Weighted random category selection
  // If no parent device connected (no time/weather/music), use ambient/teasing quotes
  int roll = random(0, 100);

  if (timeReceived && roll < 20) {
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
  else if (weatherCondition.length() > 0 && roll < 40) {
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
  else if (musicPlaying && roll < 65) {
    // Music-based
    int idx = random(0, THOUGHT_MUSIC_COUNT);
    if (currentTrack.indexOf("love") >= 0) idx = 2;
    idx = constrain(idx, 0, THOUGHT_MUSIC_COUNT - 1);
    strncpy(activeBubble.text, THOUGHTS_MUSIC[idx], 79);
  }
  else if (roll < 80) {
    // Teasing thoughts
    int idx = random(0, THOUGHT_TEASING_COUNT);
    strncpy(activeBubble.text, THOUGHTS_TEASING[idx], 79);
  }
  else {
    // Ambient thoughts fallback (Always valid with zero dependencies)
    int idx = random(0, THOUGHT_AMBIENT_COUNT);
    strncpy(activeBubble.text, THOUGHTS_AMBIENT[idx], 79);
  }

  activeBubble.text[79] = '\0';
}

// ═══════════════════════════════════════════════════════════
// THOUGHT BUBBLE RENDERING
// ═══════════════════════════════════════════════════════════

extern uint16_t blendColor(uint16_t c1, uint16_t c2, float t);

inline void clearThoughtBubbleArea() {
  tft.fillRect(SCR_W - 88, 14, 88, 54, COLOR_BG);
}

template <typename T>
void renderThoughtBubble(T* display, bool isSprite) {
  if (!activeBubble.active) return;

  // Validate visible text before drawing anything
  bool hasVisibleChar = false;
  for (int i = 0; activeBubble.text[i] != '\0'; i++) {
    if ((unsigned char)activeBubble.text[i] > 32) {
      hasVisibleChar = true;
      break;
    }
  }
  if (!hasVisibleChar) {
    activeBubble.active = false;
    activeBubble.fadingOut = false;
    if (!isSprite) clearThoughtBubbleArea();
    return;
  }

  static uint8_t lastAlpha = 0;
  static char lastText[80] = "";
  bool bubbleChanged = (activeBubble.fadeAlpha != lastAlpha) || (strcmp(activeBubble.text, lastText) != 0);
  lastAlpha = activeBubble.fadeAlpha;
  strcpy(lastText, activeBubble.text);

  unsigned long now = millis();
  unsigned long elapsed = now - activeBubble.shownAt;
  bool needsRedraw = bubbleChanged || activeBubble.fadingOut || isSprite;

  if (elapsed >= THOUGHT_DISPLAY_MS && !activeBubble.fadingOut) {
    activeBubble.fadingOut = true;
    needsRedraw = true;
  }

  if (!isSprite && !needsRedraw) {
    return; // Prevent screen flickering by not redrawing static bubbles directly on TFT
  }

  if (activeBubble.fadingOut) {
    if (activeBubble.fadeAlpha > (255 / THOUGHT_FADE_STEPS)) {
      activeBubble.fadeAlpha -= (255 / THOUGHT_FADE_STEPS);
    } else {
      activeBubble.fadeAlpha = 0;
      activeBubble.active = false;
      activeBubble.fadingOut = false;
      if (!isSprite) {
        clearThoughtBubbleArea();
      }
      return;  // done, erased completely
    }
  }

  // Alpha blend factor
  float alpha = activeBubble.fadeAlpha / 255.0f;

  // ── Position Calculation ──────────────────────────────
  int bubbleW = 80;
  int bubbleX = SCR_W - bubbleW - 1; // Pinned to top-right
  int bubbleY = 16;  
  
  int lineH = 8;
  int maxChars = (bubbleW - 6) / 6; // ~12 chars per line at font size 1 (6px wide)

  // ── Word-aware line wrapping ──────────────────────────
  char wrappedLines[3][24]; // max 3 lines, 23 chars + null
  int lineCount = 0;
  int textLen = strlen(activeBubble.text);
  int pos = 0;

  while (pos < textLen && lineCount < 3) {
    while (pos < textLen && activeBubble.text[pos] == ' ') pos++;
    if (pos >= textLen) break;

    int remaining = textLen - pos;
    if (remaining <= maxChars) {
      int copyLen = remaining > 23 ? 23 : remaining;
      strncpy(wrappedLines[lineCount], activeBubble.text + pos, copyLen);
      wrappedLines[lineCount][copyLen] = '\0';
      lineCount++;
      break;
    }

    int breakAt = maxChars;
    for (int j = maxChars; j > 0; j--) {
      if (activeBubble.text[pos + j] == ' ') {
        breakAt = j;
        break;
      }
    }

    int copyLen = breakAt > 23 ? 23 : breakAt;
    strncpy(wrappedLines[lineCount], activeBubble.text + pos, copyLen);
    wrappedLines[lineCount][copyLen] = '\0';
    lineCount++;
    pos += breakAt;
  }

  if (lineCount > 2) {
    lineCount = 2;
    int len2 = strlen(wrappedLines[1]);
    if (len2 > maxChars - 2) len2 = maxChars - 2;
    wrappedLines[1][len2] = '.';
    wrappedLines[1][len2 + 1] = '.';
    wrappedLines[1][len2 + 2] = '\0';
  }

  if (lineCount == 0) {
    activeBubble.active = false;
    if (!isSprite) clearThoughtBubbleArea();
    return;
  }

  int padding = 3;
  int bubbleH = lineCount * lineH + padding * 2;
  if (bubbleY + bubbleH > 58) bubbleH = 58 - bubbleY;

  uint16_t borderColor = blendColor(CLR_BG, CLR_PRIMARY, alpha);
  uint16_t bgColor = blendColor(CLR_BG, CLR_SURFACE_2, alpha);
  uint16_t textColor = blendColor(CLR_BG, CLR_TEXT_HI, alpha);

  int drawY = bubbleY;
  if (isSprite) {
    drawY = bubbleY - 16;
  }

  display->fillRoundRect(bubbleX, drawY, bubbleW, bubbleH, 3, bgColor);
  display->drawRoundRect(bubbleX, drawY, bubbleW, bubbleH, 3, borderColor);

  uint16_t glowColor = blendColor(CLR_BG, CLR_PRIMARY, alpha * 0.3f);
  display->drawRoundRect(bubbleX + 1, drawY + 1, bubbleW - 2, bubbleH - 2, 2, glowColor);

  int tailX = bubbleX + 12;
  int tailY = drawY + bubbleH;
  int tailW = 5;
  int tailH = 6;
  display->fillTriangle(tailX, tailY, tailX + tailW, tailY,
                           tailX - tailW/2, tailY + tailH, bgColor);
  display->drawLine(tailX, tailY, tailX - tailW/2, tailY + tailH, borderColor);
  display->drawLine(tailX + tailW, tailY, tailX - tailW/2, tailY + tailH, borderColor);

  int dotX = tailX - 4;
  int dotY = tailY + tailH + 3;
  display->fillCircle(dotX, dotY, 2, borderColor);
  display->fillCircle(dotX - 4, dotY + 4, 1, borderColor);

  display->setTextColor(textColor);
  display->setTextSize(1); 
  int textX = bubbleX + padding;
  int textY = drawY + padding;

  for (int line = 0; line < lineCount; line++) {
    display->drawString(wrappedLines[line], textX, textY + line * lineH, 1);
  }
}


template <typename T>
void tickThoughtScheduler(T* display, bool isSprite) {
  if (!thoughtSchedulerActive) return;
  if (currentState != STATE_EYES) return;
  if (customEyeActive && customEyeReady) return;

  // Check if bubble is currently active — handle fade/display
  if (activeBubble.active) {
    renderThoughtBubble(display, isSprite);  // draw/fade current bubble
    return;
  }

  // Check for PC-pushed override thought (immediate display)
  if (strlen(overrideThought) > 0) {
    generateThought();
    if (strlen(activeBubble.text) > 0) {
      activeBubble.active = true;
      activeBubble.shownAt = millis();
      activeBubble.fadeAlpha = 255;
      activeBubble.fadingIn = false;
      activeBubble.fadingOut = false;
      renderThoughtBubble(display, isSprite);
    }
    return;
  }

  // Scheduled thought generation
  if (millis() >= nextThoughtTime) {
    generateThought();
    if (strlen(activeBubble.text) > 0) {
      activeBubble.active = true;
      activeBubble.shownAt = millis();
      activeBubble.fadeAlpha = 255;
      activeBubble.fadingIn = false;
      activeBubble.fadingOut = false;
      renderThoughtBubble(display, isSprite);
    }

    // Schedule next thought (45-90 min)
    nextThoughtTime = millis() + random(THOUGHT_MIN_INTERVAL_MS, THOUGHT_MAX_INTERVAL_MS);
  }
}

#endif
