#ifndef COMPANION_TEST_EYES_THOUGHT_HOST_H
#define COMPANION_TEST_EYES_THOUGHT_HOST_H

#include "host_arduino.h"
#include <cassert>
#include <vector>
#define COMPANION_HOST_MEMORY
#include "../arduino/CompanionOS_Main/memory_policy.h"
namespace companion_memory { Snapshot testSnapshot = {256 * 1024, 200 * 1024}; }
using std::abs;

// Bypass hardware dependencies, not the four implementation headers under test.
#define GLOBALS_H
#define UI_COMPONENTS_H
#ifndef SCREEN_W
#define SCREEN_W 160
#endif
#ifndef SCREEN_H
#define SCREEN_H 128
#endif
#define SCR_W SCREEN_W
#define SCR_H SCREEN_H
#define SCALE_X(v) ((int)((v) * (float)SCREEN_W / 320.0f + 0.5f))
#define SCALE_Y(v) ((int)((v) * (float)SCREEN_H / 240.0f + 0.5f))
#define SCALE_MIN(v) max(1, SCALE_X(v))
#define THOUGHT_DISPLAY_MS (10UL * 60UL * 1000UL)
#define THOUGHT_MIN_INTERVAL_MS (5UL * 60UL * 1000UL)
#define THOUGHT_MAX_INTERVAL_MS (20UL * 60UL * 1000UL)
#define THOUGHT_FADE_STEPS 20
constexpr int UI_HEADER_H = 16, UI_FOOTER_H = 10;
constexpr double PI = 3.14159265358979323846;
constexpr int TL_DATUM = 0, A0 = 0, LDR_PIN = 39;
enum {
  COLOR_BG = 0, CLR_BG = 0, CLR_PRIMARY = 0x347F, CLR_SURFACE_2 = 0x18A3,
  CLR_TEXT_HI = 65535, TFT_WHITE = 65535, TFT_BLACK = 0,
  TFT_YELLOW = 65504, TFT_MAGENTA = 63519, TFT_RED = 63488
};
static int hostAdcValue = 500;
inline int analogRead(int) { return hostAdcValue; }

#ifdef COMPANION_EYES_RASTER
#include "host_tft.h"
using TFT_eSPI = HostTFT;
TFT_eSPI tft(SCREEN_W, SCREEN_H);
#else
struct EyeHostRect {
  int x, y, w, h;
};

// An instrumented command sink, not a panel rasterizer. Viewport getters match
// TFT_eSPI: false datum exposes screen dimensions, not the absolute clip.
struct TFT_eSPI {
  int ops = 0, pushes = 0;
  int vx = 0, vy = 0, vw = SCREEN_W, vh = SCREEN_H;
  bool vd = false;
  EyeHostRect clipBounds{0, 0, SCREEN_W, SCREEN_H};
  std::vector<EyeHostRect> writes;
  std::vector<EyeHostRect> triangles;
  std::vector<std::string> strings;
  uint8_t textsize = 1, datum = 0;
  uint16_t padding = 0;
  uint32_t textcolor = 0, textbgcolor = 0;
  uint32_t bitmap_fg = 65535, bitmap_bg = 0;
  void setBitmapColor(uint16_t fg, uint16_t bg) { bitmap_fg=fg; bitmap_bg=bg; }

  void setViewport(int x, int y, int w, int h, bool d = true) {
    vd = d;
    vx = d ? x : 0; vy = d ? y : 0;
    vw = d ? w : SCREEN_W; vh = d ? h : SCREEN_H;
    clipBounds = {max(0, x), max(0, y),
                  max(0, min(SCREEN_W, x + w) - max(0, x)),
                  max(0, min(SCREEN_H, y + h) - max(0, y))};
  }
  int getViewportX() const { return vx; }
  int getViewportY() const { return vy; }
  int getViewportWidth() const { return vw; }
  int getViewportHeight() const { return vh; }
  bool getViewportDatum() const { return vd; }
  void record(int x, int y, int w, int h) {
    ++ops;
    x += vx; y += vy;
    int left = max(x, clipBounds.x), top = max(y, clipBounds.y);
    int right = min(x + w, clipBounds.x + clipBounds.w);
    int bottom = min(y + h, clipBounds.y + clipBounds.h);
    if (right > left && bottom > top) writes.push_back({left, top, right - left, bottom - top});
  }
  void fillRect(int x, int y, int w, int h, uint16_t) { record(x, y, w, h); }
  void fillRoundRect(int x, int y, int w, int h, int, uint16_t) { record(x, y, w, h); }
  void drawRoundRect(int x, int y, int w, int h, int, uint16_t) { record(x, y, w, h); }
  void fillTriangle(int a, int b, int c, int d, int e, int f, uint16_t) {
    triangles.push_back({min(a,min(c,e)), min(b,min(d,f)), max(a,max(c,e))-min(a,min(c,e))+1,
                         max(b,max(d,f))-min(b,min(d,f))+1});
    record(min(a, min(c, e)), min(b, min(d, f)),
           max(a, max(c, e)) - min(a, min(c, e)) + 1,
           max(b, max(d, f)) - min(b, min(d, f)) + 1);
  }
  void drawLine(int x, int y, int a, int b, uint16_t) {
    record(min(x, a), min(y, b), abs(a - x) + 1, abs(b - y) + 1);
  }
  void fillCircle(int x, int y, int r, uint16_t) { record(x - r, y - r, r * 2 + 1, r * 2 + 1); }
  void drawCircle(int x, int y, int r, uint16_t c) { fillCircle(x, y, r, c); }
  void drawPixel(int x, int y, uint16_t) { record(x, y, 1, 1); }
  void drawFastVLine(int x, int y, int h, uint16_t) { record(x, y, 1, h); }
  void drawFastHLine(int x, int y, int w, uint16_t) { record(x, y, w, 1); }
  void setTextColor(uint32_t c) { textcolor = textbgcolor = c; }
  void setTextColor(uint32_t c, uint32_t b) { textcolor = c; textbgcolor = b; }
  void setTextSize(uint8_t s) { textsize = s; }
  void setTextDatum(uint8_t d) { datum = d; }
  uint8_t getTextDatum() const { return datum; }
  void setTextPadding(uint16_t p) { padding = p; }
  uint16_t getTextPadding() const { return padding; }
  void drawString(const char* s, int x, int y, int) {
    strings.emplace_back(s);
    record(x, y, std::strlen(s) * 6 * textsize, 8 * textsize);
  }
};
TFT_eSPI tft;
bool fail16 = false, fail8 = false;
int allocations = 0;
struct TFT_eSprite : TFT_eSPI {
  TFT_eSPI* parent;
  bool valid = false;
  int depth = 16, w = 0, h = 0;
  int lastPushX = 0, lastPushY = 0, lastPushW = 0, lastPushH = 0;
  explicit TFT_eSprite(TFT_eSPI* target) : parent(target) {}
  void setColorDepth(int d) { assert(d == 16 || d == 8 || d == 1); depth = d; }
  void* createSprite(int x, int y) {
    ++allocations;
    valid = !(depth == 16 ? fail16 : fail8);
    w = x; h = y;
    return valid ? this : nullptr;
  }
  void deleteSprite() { valid = false; }
  bool created() const { return valid; }
  void fillSprite(uint16_t) { assert(valid); ++ops; }
  void pushSprite(int x, int y) { pushSprite(x, y, 0, 0, w, h); }
  bool pushSprite(int x, int y, int sx, int sy, int width, int height) {
    assert(valid && sx >= 0 && sy >= 0 && width >= 0 && height >= 0);
    assert(sx + width <= w && sy + height <= h);
    ++pushes;
    lastPushX = x; lastPushY = y; lastPushW = width; lastPushH = height;
    parent->record(x, y, width, height);
    return true;
  }
};
#endif

enum Emotion { EMO_NEUTRAL, EMO_HAPPY, EMO_SAD, EMO_EXCITED, EMO_LOVE,
               EMO_SLEEPY, EMO_ANGRY, EMO_SURPRISED, EMO_COUNT };
Emotion currentEmotion = EMO_NEUTRAL;
enum { STATE_EYES, STATE_OTHER };
int currentState = STATE_EYES, activeTheme = 0, t3EyeVariant = 0;
bool customEyeActive = false, customEyeReady = false, thoughtSchedulerActive = true;
bool agentOverlayActive = false, flashNotifActive = false;
bool timeReceived = false, musicPlaying = false, ldrEnabled = false, isBlinking = false;
bool manualEmotionOverride = false, isYawning = false;
unsigned long lastInteractionTime = 0, yawnStart = 0;
int petMoodLevel = 100;
int displayHour = 12, displayMinute = 0, weatherTemp = 20, ldrValue = 500, blinkPhase = 0;
unsigned long nextThoughtTime = 900000, lastBlink = 0, lastManualEmotionTime = 0;
String currentTrack, currentArtist, weatherCondition;
struct ThoughtBubble {
  char text[80];
  bool active;
  unsigned long shownAt;
  uint8_t fadeAlpha;
  bool fadingIn, fadingOut;
};
ThoughtBubble activeBubble = {"", false, 0, 0, false, false};
char overrideThought[80] = "";
// Color interpolation and status-bar painting are dependencies, not subjects
// of this suite; their behavior is covered separately by the UI host suite.
uint16_t blendColor(uint16_t a, uint16_t b, float t) {
  t=constrain(t,0.0f,1.0f);
  int r=(a>>11)&31,g=(a>>5)&63,blue=a&31;
  r+=int((int((b>>11)&31)-r)*t);g+=int((int((b>>5)&63)-g)*t);
  blue+=int((int(b&31)-blue)*t);
  return uint16_t((r<<11)|(g<<5)|blue);
}
void drawStatusBar() {}

#endif
