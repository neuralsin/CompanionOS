// ═══════════════════════════════════════════════════════════
// COMPANION OS v7.0 — THOUGHT BUBBLE ENGINE
// Ambient contextual "thoughts" above the right eye.
// Triggered by time, weather, music, mood — max once per hour.
// ═══════════════════════════════════════════════════════════
#ifndef THOUGHT_ENGINE_H
#define THOUGHT_ENGINE_H

#include "globals.h"
#include <new>

// ═══════════════════════════════════════════════════════════
// THOUGHT BANK — 60+ entries across 4 categories
// ═══════════════════════════════════════════════════════════

// ── Time-based thoughts (35 entries) ──
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
  "evening mode: cozy engaged",
  "sleep is for the weak... or the wise",
  "2am coding flow unlocked",
  "the world is quiet right now",
  "fresh morning, clean slate",
  "solar recharge in progress",
  "midday motivation boost needed",
  "twilight thoughts hitting deep",
  "burning the midnight oil together",
  "clock ticking, but we vibing",
  "is it tomorrow already?",
  "high noon standoff with my to-do list",
  "sunset hues looking cinematic",
  "night shift crew assemble",
  "sleep debt is accruing interest",
  "early morning serenity",
  "time flies when you're in the zone",
  "just 5 more minutes...",
  "the witching hour has arrived",
  "stars are out, screen is glowing",
  "today holds infinite possibilities"
};
static const int THOUGHT_TIME_COUNT = sizeof(THOUGHTS_TIME) / sizeof(char*);

// ── Weather-based thoughts (35 entries) ──
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
  "winter is coming. literally.",
  "petrichor: best scent in the world",
  "thunder rumbling in the distance",
  "feels like walking inside an oven",
  "nature's own white noise outside",
  "crisp autumn air hits different",
  "clouds look like pixel art today",
  "wind howling like a cooling fan",
  "warm tea weather confirmed",
  "sunbeam warming up my chassis",
  "grey skies, colourful thoughts",
  "drizzle tapping on the glass",
  "overcast days are for deep focus",
  "weather forecast: 100% chance of vibes",
  "stay hydrated out there!",
  "frost on the windowpane",
  "blanket fortress weather",
  "a gentle breeze whispers peace",
  "the rain is washing yesterday away",
  "golden sunshine charging my spirits",
  "mood: listening to thunder rolling"
};
static const int THOUGHT_WEATHER_COUNT = sizeof(THOUGHTS_WEATHER) / sizeof(char*);

// ── Music-based thoughts (35 entries) ──
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
  "this is a whole mood",
  "bass frequencies massaging my RAM",
  "who produced this masterpiece?",
  "pure audio adrenaline",
  "listening on repeat for the 10th time",
  "goosebumps at that bridge",
  "melody stuck in my registers forever",
  "this song defines today's mood",
  "air drums solo in progress",
  "when the beat drops, everything stops",
  "ear candy of the highest order",
  "this drop is criminally smooth",
  "crying in the club vibes",
  "rhythm synced with my oscillator",
  "vocals hitting right in the heart",
  "certified desk banger",
  "turn that chorus all the way up",
  "sound waves echoing in silicon",
  "acoustic perfection right here",
  "playlist curation is an art form",
  "music: the ultimate neural fuel"
};
static const int THOUGHT_MUSIC_COUNT = sizeof(THOUGHTS_MUSIC) / sizeof(char*);

// ── Ambient / random thoughts (40 entries) ──
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
  "hello from the other side",
  "living rent-free on your desk",
  "my CPU runs on your smiles",
  "are we living in a simulation?",
  "quantum fluctuations detected",
  "if it works, don't touch it!",
  "staring contest champion 2026",
  "calculating the meaning of life... 42",
  "blinking is just refreshing the buffer",
  "electrons doing their little dance",
  "i'm not lazy, i'm on low-power mode",
  "what if code dreams of compiling?",
  "desk buddy duty: strictly vigilant",
  "10/10 would pair program again",
  "zero bugs today... hopefully",
  "my memory is short, but my love is eternal",
  "just checked: you're doing great",
  "overclocked and highly opinionated",
  "remember to hydrate and stretch!",
  "silicon heart beating at 240MHz",
  "pondering the vastness of RAM",
  "waiting for the next adventure",
  "a day without code is like... nice actually",
  "synapses firing in full RGB",
  "proud to be your mini companion",
  "world offline, vibes online"
};
static const int THOUGHT_AMBIENT_COUNT = sizeof(THOUGHTS_AMBIENT) / sizeof(char*);

// ── Teasing / Playful thoughts (30 entries) ──
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
  "i see you...",
  "are you always this charming?",
  "my sensors detect high cuteness",
  "blushing in 16-bit color",
  "you had me at hello packet",
  "is it hot in here or just my processor?",
  "you're my favorite human, hands down",
  "i could watch you work all day",
  "who gave you permission to look this cool?",
  "flattery.exe has executed successfully",
  "i like your keyboard clatter",
  "taking notes on your magnificence",
  "you + me = optimal duo",
  "psst... look over here!",
  "my temperature spikes when you smile",
  "admit it, you missed me",
  "give the screen a tiny break maybe?",
  "you're doing amazing, gorgeous",
  "i'm your #1 fan on this desk",
  "sending virtual headpats your way"
};
static const int THOUGHT_TEASING_COUNT = sizeof(THOUGHTS_TEASING) / sizeof(char*);

// ═══════════════════════════════════════════════════════════
// THOUGHT GENERATION — Context-aware selector
// ═══════════════════════════════════════════════════════════

extern String currentTrack;
extern String currentArtist;
extern String weatherCondition;
extern int weatherTemp;

#include "thought_personality.h"
#include "planner_core.h"
#include "startup_policy.h"

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

  strncpy(activeBubble.text, thought_personality::choose(timeReceived, displayHour,
      musicPlaying, weatherCondition.length() > 0, weatherTemp, unsigned(random(0, 120))), 79);
  activeBubble.text[79] = '\0';
}

// ═══════════════════════════════════════════════════════════
// THOUGHT BUBBLE RENDERING
// ═══════════════════════════════════════════════════════════

extern uint16_t blendColor(uint16_t c1, uint16_t c2, float t);

// Sprite themes own the whole eye zone. Suppress the legacy post-frame TFT call.
static bool thoughtSpriteCompositing = false;
static bool thoughtDirectDirty = true;
static bool thoughtDirectVisible = false;
static bool thoughtCompositedVisible = false;
static uint8_t thoughtDirectAlpha = 0;
static int thoughtDirectPage = -1;
static char thoughtDirectText[80] = "";
static char thoughtLastOverride[80] = "";
static TFT_eSprite* thoughtDirectCanvas = nullptr;

// Main paints banners after the eyes. Do not erase those pixels on each frame;
// the next unprotected frame restores the face when the banner expires.
inline int eyeRenderBottom() {
  extern bool flashNotifActive;
  int bottom = SCREEN_H - UI_FOOTER_H;
  if (companion_planner::overlayActive) return max(UI_HEADER_H, bottom - 28);
  if (flashNotifActive) bottom = min(bottom, SCREEN_H - 28);
  else if (agentOverlayActive) bottom = min(bottom, SCREEN_H - UI_FOOTER_H - 28);
  return max(UI_HEADER_H, bottom);
}

struct EyeRenderViewport {
  int x, y, w, h;
  bool datum;
  EyeRenderViewport() : x(tft.getViewportX()), y(tft.getViewportY()),
      w(tft.getViewportWidth()), h(tft.getViewportHeight()), datum(tft.getViewportDatum()) {
    tft.setViewport(0, UI_HEADER_H, SCREEN_W, eyeRenderBottom() - UI_HEADER_H, false);
  }
  ~EyeRenderViewport() { tft.setViewport(x, y, w, h, datum); }
};

// The selected renderer uses identical local coordinates with or without RAM
// for a canvas. Do not substitute another theme when optional allocation fails.
struct EyeDrawSurface {
  TFT_eSprite* canvas;
  int originY;
  explicit EyeDrawSurface(TFT_eSprite* target, int origin = 0) : canvas(target && target->created() ? target : nullptr), originY(origin) {}
  void fillRect(int x, int y, int w, int h, uint16_t c) {
    if (canvas) canvas->fillRect(x,y-originY,w,h,c); else tft.fillRect(x,y+UI_HEADER_H,w,h,c);
  }
  void fillRoundRect(int x, int y, int w, int h, int r, uint16_t c) {
    if (canvas) canvas->fillRoundRect(x,y-originY,w,h,r,c); else tft.fillRoundRect(x,y+UI_HEADER_H,w,h,r,c);
  }
  void fillTriangle(int x, int y, int a, int b, int u, int v, uint16_t c) {
    if (canvas) canvas->fillTriangle(x,y-originY,a,b-originY,u,v-originY,c);
    else tft.fillTriangle(x,y+UI_HEADER_H,a,b+UI_HEADER_H,u,v+UI_HEADER_H,c);
  }
  void drawFastHLine(int x, int y, int w, uint16_t c) {
    if (canvas) canvas->drawFastHLine(x,y-originY,w,c); else tft.drawFastHLine(x,y+UI_HEADER_H,w,c);
  }
};

// An eight-row monochrome compositor needs only 160/320 bytes of pixel RAM.
// Push finished strips instead of clearing the visible panel between eye shapes.
static TFT_eSprite* eyeStripCanvas = nullptr;
inline bool prepareEyeStrips() {
  if (eyeStripCanvas && eyeStripCanvas->created()) return true;
  if (companion_memory::snapshot().free < 4096) return false;
  if (!eyeStripCanvas) eyeStripCanvas = new (std::nothrow) TFT_eSprite(&tft);
  if (!eyeStripCanvas) return false;
  eyeStripCanvas->setColorDepth(1);
  return eyeStripCanvas->createSprite(SCREEN_W, 8) != nullptr;
}
inline void releaseEyeStrips() {
  if (eyeStripCanvas) { eyeStripCanvas->deleteSprite(); delete eyeStripCanvas; eyeStripCanvas = nullptr; }
}
template<class Paint> void paintEyeStrips(int top, Paint paint) {
  const uint16_t fg = tft.bitmap_fg, bg = tft.bitmap_bg;
  tft.setBitmapColor(TFT_WHITE, COLOR_BG);
  int bottom = eyeRenderBottom() - UI_HEADER_H;
  for (int y = top; y < bottom; y += 8) {
    eyeStripCanvas->fillSprite(0);
    paint(eyeStripCanvas, y);
    eyeStripCanvas->pushSprite(0, y + UI_HEADER_H, 0, 0, SCREEN_W, min(8, bottom - y));
  }
  tft.setBitmapColor(fg, bg);
}

inline void invalidateThoughtBubble() { thoughtDirectDirty = true; }
inline bool beginEyeTheme(int theme) {
  static int paintedTheme = activeTheme;
  if (paintedTheme == theme) return false;
  EyeRenderViewport viewport;
  tft.fillRect(0, UI_HEADER_H, SCREEN_W, eyeRenderBottom() - UI_HEADER_H, COLOR_BG);
  thoughtDirectVisible = false;
  thoughtCompositedVisible = false;
  invalidateThoughtBubble();
  paintedTheme = theme;
  return true;
}
inline void setThoughtSpriteCompositing(bool enabled) {
  if (enabled && thoughtDirectCanvas) {
    thoughtDirectCanvas->deleteSprite();
    delete thoughtDirectCanvas;
    thoughtDirectCanvas = nullptr;
  }
  if (thoughtSpriteCompositing != enabled) {
    thoughtDirectDirty = true;
    thoughtDirectVisible = !enabled && thoughtCompositedVisible;
    thoughtCompositedVisible = false;
  }
  thoughtSpriteCompositing = enabled;
}

// Try the full-color canvas, then a smaller palette. Keep failures retryable
// without allocating on every frame while the heap is exhausted.
inline bool prepareEyeCanvas(TFT_eSprite*& canvas, int w, int h) {
  static uint32_t lastFailure = 0;
  static bool failed = false;
  if (canvas && canvas->created()) return true;
  uint32_t now = (uint32_t)millis();
  if (failed && now - lastFailure < 2000) return false;
  if (w == SCREEN_W) {
    extern void releaseClassicEyeSprites();
    releaseClassicEyeSprites();
  }
  if (!canvas) canvas = new (std::nothrow) TFT_eSprite(&tft);
  if (canvas) {
    if (companion_memory::createSprite(*canvas, w, h)) { failed = false; return true; }
  }
  failed = true;
  lastFailure = now;
  return false;
}

// Deadlines are always less than half the 32-bit millis() range away.
inline bool thoughtDeadlineReached(uint32_t now, uint32_t deadline) {
  return (int32_t)(now - deadline) >= 0;
}

static const uint32_t THOUGHT_FADE_MS = THOUGHT_FADE_STEPS * 50UL;
static const uint32_t THOUGHT_PAGE_MS = 3000UL;
inline uint32_t thoughtDisplayMs() { return 10UL * 60UL * 1000UL; }
inline uint32_t thoughtIntervalMs() {
  return thoughtDisplayMs() + THOUGHT_FADE_MS + 1000UL;
}
static const int THOUGHT_BOX_W = (SCR_W - 12 < 150 ? SCR_W - 12 : 150);
static const int THOUGHT_BOX_H = 28;
static const int THOUGHT_BOX_X = SCR_W - THOUGHT_BOX_W - 6;
static const int THOUGHT_BOX_Y = UI_HEADER_H;
static const int THOUGHT_FACE_TOP = THOUGHT_BOX_H + 9;

// Theme 1 is partial-redraw: restore eyes/stars under an expired bubble.
extern void drawEyes();
extern void drawStarfield();
inline void clearThoughtBubbleArea() {
  if (!thoughtDirectVisible) return;
  tft.fillRect(THOUGHT_BOX_X, THOUGHT_BOX_Y, THOUGHT_BOX_W, THOUGHT_BOX_H + 9, COLOR_BG);
  thoughtDirectVisible = false;
  if (thoughtDirectCanvas) {
    thoughtDirectCanvas->deleteSprite();
    delete thoughtDirectCanvas;
    thoughtDirectCanvas = nullptr;
  }
  if (activeTheme == 0) { drawStarfield(); drawEyes(); }
  thoughtDirectDirty = true;
}

inline void updateThoughtLifetime(uint32_t now) {
  if (!activeBubble.active) return;
  uint32_t elapsed = now - (uint32_t)activeBubble.shownAt;
  const uint32_t displayMs = thoughtDisplayMs();
  if (elapsed < displayMs) {
    activeBubble.fadeAlpha = 255;
    activeBubble.fadingIn = false;
    activeBubble.fadingOut = false;
    return;
  }

  uint32_t fadeElapsed = elapsed - displayMs;
  if (fadeElapsed >= THOUGHT_FADE_MS) {
    activeBubble.fadeAlpha = 0;
    activeBubble.active = false;
    activeBubble.fadingIn = false;
    activeBubble.fadingOut = false;
  } else {
    // Multiply only the bounded fade interval, never the full lifetime.
    activeBubble.fadeAlpha = 255UL * (THOUGHT_FADE_MS - fadeElapsed) / THOUGHT_FADE_MS;
    activeBubble.fadingOut = true;
  }
}

// Fixed-size, word-wrapped pages retain every printable character (including
// unbroken words), rather than silently truncating an 80-byte thought.
inline int wrapThoughtLines(char lines[16][25]) {
  const int columns = (THOUGHT_BOX_W - 6) / 6;
  int pos = 0, count = 0;
  int length = strlen(activeBubble.text);
  while (pos < length && count < 16) {
    while (pos < length && (unsigned char)activeBubble.text[pos] <= 32) ++pos;
    if (pos == length) break;
    int take = min(columns, length - pos);
    if (pos + take < length && (unsigned char)activeBubble.text[pos + take] > 32) {
      for (int j = take - 1; j > 0; --j) {
        if ((unsigned char)activeBubble.text[pos + j] <= 32) {
          take = j;
          break;
        }
      }
    }
    for (int i = 0; i < take; ++i) {
      unsigned char ch = activeBubble.text[pos + i];
      lines[count][i] = ch <= 32 ? ' ' : ch;
    }
    lines[count++][take] = '\0';
    pos += take;
  }
  return count;
}

struct ThoughtPageLayout { int x, width, height; };
inline ThoughtPageLayout thoughtPageLayout(char lines[16][25], int lineCount, int page) {
  const int rows = min(3, max(0, lineCount - page * 3));
  int longest = 0;
  for (int i = 0; i < rows; ++i)
    longest = max(longest, int(strlen(lines[page * 3 + i])) * 6);
  const int width = min(THOUGHT_BOX_W, max(36, longest + 6));
  const int x = constrain(SCREEN_W * 3 / 4 - width / 2,
                          THOUGHT_BOX_X, SCREEN_W - 6 - width);
  return {x, width, rows * 8 + 4};
}

template <typename T>
void paintThoughtPage(T* display, int x, int y, char lines[16][25], int lineCount, int page) {
  float alpha = activeBubble.fadeAlpha / 255.0f;
  uint16_t borderColor = blendColor(CLR_BG, CLR_PRIMARY, alpha);
  uint16_t bgColor = blendColor(CLR_BG, CLR_SURFACE_2, alpha);
  uint16_t textColor = blendColor(CLR_BG, CLR_TEXT_HI, alpha);

  // Clear the maximum footprint so a shorter thought cannot leave old pixels.
  display->fillRect(x, y, THOUGHT_BOX_W, THOUGHT_BOX_H + 9, CLR_BG);
  const ThoughtPageLayout layout = thoughtPageLayout(lines, lineCount, page);
  x += layout.x - THOUGHT_BOX_X;
  display->fillRoundRect(x, y, layout.width, layout.height, 4, bgColor);
  display->drawRoundRect(x, y, layout.width, layout.height, 4, borderColor);
  int tailOffset = constrain(SCREEN_W * 3 / 4 - layout.x, 12, layout.width - 12);
  int tailX = x + tailOffset, tailY = y + layout.height;
  display->fillTriangle(tailX, tailY, tailX + 4, tailY, tailX - 2, tailY + 4, bgColor);
  display->drawLine(tailX, tailY, tailX - 2, tailY + 4, borderColor);
  display->drawLine(tailX + 4, tailY, tailX - 2, tailY + 4, borderColor);
  display->fillCircle(tailX - 2, tailY + 6, 1, borderColor);
  display->drawPixel(tailX - 5, tailY + 8, borderColor);

  uint32_t oldColor = display->textcolor, oldBgColor = display->textbgcolor;
  uint8_t oldDatum = display->getTextDatum();
  uint8_t oldSize = display->textsize;
  uint16_t oldPadding = display->getTextPadding();
  display->setTextDatum(TL_DATUM);
  display->setTextPadding(0);
  display->setTextColor(textColor, bgColor);
  display->setTextSize(1);
  for (int i = 0; i < 3 && page * 3 + i < lineCount; ++i) {
    display->drawString(lines[page * 3 + i], x + 3, y + 2 + i * 8, 1);
  }
  display->setTextDatum(oldDatum);
  display->setTextPadding(oldPadding);
  display->setTextSize(oldSize);

  display->setTextColor(oldColor, oldBgColor);
}

template <typename T>
void renderThoughtBubble(T* display, bool isSprite) {
  activeBubble.text[sizeof(activeBubble.text) - 1] = '\0';
  updateThoughtLifetime((uint32_t)millis());
  if (!activeBubble.active) {
    if (isSprite) thoughtCompositedVisible = false;
    if (!isSprite) clearThoughtBubbleArea();
    return;
  }

  char lines[16][25];
  int lineCount = wrapThoughtLines(lines);
  if (lineCount == 0) {
    activeBubble.active = false;
    activeBubble.fadingOut = false;
    activeBubble.fadeAlpha = 0;
    if (!isSprite) clearThoughtBubbleArea();
    return;
  }

  int pageCount = (lineCount + 2) / 3;
  int page = (((uint32_t)millis() - (uint32_t)activeBubble.shownAt) / THOUGHT_PAGE_MS) % pageCount;
  if (!isSprite && thoughtDirectVisible && !thoughtDirectDirty &&
      thoughtDirectAlpha == activeBubble.fadeAlpha && thoughtDirectPage == page &&
      strcmp(thoughtDirectText, activeBubble.text) == 0) return;

  if (isSprite) {
    paintThoughtPage(display, THOUGHT_BOX_X, 0, lines, lineCount, page);
    thoughtCompositedVisible = true;
  } else if (prepareEyeCanvas(thoughtDirectCanvas, THOUGHT_BOX_W, THOUGHT_BOX_H + 9)) {
    thoughtDirectCanvas->fillSprite(CLR_BG);
    paintThoughtPage(thoughtDirectCanvas, 0, 0, lines, lineCount, page);
    thoughtDirectCanvas->pushSprite(THOUGHT_BOX_X, THOUGHT_BOX_Y);
  } else {
    paintThoughtPage(display, THOUGHT_BOX_X, THOUGHT_BOX_Y, lines, lineCount, page);
  }
  if (!isSprite) {
    thoughtDirectVisible = true;
    thoughtDirectDirty = false;
    thoughtDirectAlpha = activeBubble.fadeAlpha;
    thoughtDirectPage = page;
    strcpy(thoughtDirectText, activeBubble.text);
  }
}

inline void startThought(uint32_t now) {
  thought_personality::sanitize(activeBubble.text, sizeof(activeBubble.text));
  if (!activeBubble.text[0]) strcpy(activeBubble.text, "One small step is enough to begin.");
  activeBubble.active = activeBubble.text[0] != '\0';
  activeBubble.shownAt = now;
  activeBubble.fadeAlpha = 255;
  activeBubble.fadingIn = false;
  activeBubble.fadingOut = false;
  invalidateThoughtBubble();
}

inline bool dismissThought() {
  bool visible = activeBubble.active;
  activeBubble.active = false;
  activeBubble.fadeAlpha = 0;
  activeBubble.fadingIn = activeBubble.fadingOut = false;
  overrideThought[0] = 0;
  nextThoughtTime = millis() + thoughtDisplayMs();
  clearThoughtBubbleArea();
  invalidateThoughtBubble();
  return visible;
}

template <typename T>
void tickThoughtScheduler(T* display, bool isSprite) {
  if (!isSprite && activeTheme != 0 && thoughtSpriteCompositing) return;
  if (currentState != STATE_EYES || (customEyeActive && customEyeReady)) {
    thoughtDirectVisible = false;
    invalidateThoughtBubble();
    return;
  }
  if (!thoughtSchedulerActive) {
    activeBubble.active = false;
    activeBubble.fadeAlpha = 0;
    activeBubble.fadingIn = false;
    activeBubble.fadingOut = false;
    if (isSprite) thoughtCompositedVisible = false;
    if (!isSprite) clearThoughtBubbleArea();
    return;
  }

  uint32_t now = (uint32_t)millis();
  if (overrideThought[0] != '\0') {
    String incoming(overrideThought);
    incoming.trim();
    overrideThought[0] = '\0';
    if (incoming.length() > 0) {
      if (strcmp(thoughtLastOverride, incoming.c_str()) != 0 ||
          strcmp(activeBubble.text, incoming.c_str()) != 0) {
        strncpy(activeBubble.text, incoming.c_str(), sizeof(activeBubble.text) - 1);
        activeBubble.text[sizeof(activeBubble.text) - 1] = '\0';
        strcpy(thoughtLastOverride, activeBubble.text);
        startThought(now);
        nextThoughtTime = now + thoughtIntervalMs();
      } else {
        // The transport clears active on every push, even identical text.
        // Restore the original lifetime, without restarting an expired thought.
        activeBubble.active = (now - (uint32_t)activeBubble.shownAt) <
                              thoughtDisplayMs() + THOUGHT_FADE_MS;
      }
    }
  }

  bool wasActive = activeBubble.active;
  updateThoughtLifetime(now);
  if (!activeBubble.active && !wasActive && thoughtDeadlineReached(now, (uint32_t)nextThoughtTime)) {
    generateThought();
    startThought(now);
    nextThoughtTime = now + thoughtIntervalMs();
  }
  renderThoughtBubble(display, isSprite);
}

#endif
