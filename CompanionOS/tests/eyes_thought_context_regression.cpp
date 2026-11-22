#include "eyes_thought_host.h"
#include <iostream>

#ifdef NDEBUG
#error "Regression assertions must not be disabled"
#endif

// Select both dependency-free banks without changing the shared host fake.
static int thoughtRoll = 50;
long eyeTestRandom(long a, long b) {
  return a == 0 && b == 100 ? thoughtRoll : random(a, b);
}
long eyeTestRandom(long b) { return eyeTestRandom(0, b); }
#define random eyeTestRandom
#include "../arduino/CompanionOS_Main/eyes.h"
#include "../arduino/CompanionOS_Main/thought_engine.h"
#include "../arduino/CompanionOS_Main/theme2_eyes.h"
#include "../arduino/CompanionOS_Main/theme3_eyes.h"
#include "parent_personality_under_test.h"
#undef random

static int failures = 0, checks = 0;
static std::string context;
void check(bool condition, const char* message) {
  ++checks;
  if (!condition) {
    ++failures;
    std::cout << "FAIL " << context << ": " << message << '\n';
  }
}

void fresh(int theme, const char* label) {
  hostMillis += 100000;
  context = "theme" + std::to_string(theme) + "/" + label;
  currentState = STATE_EYES;
  activeTheme = theme;
  currentEmotion = autoEmotion = EMO_NEUTRAL;
  lastManualEmotionTime = hostMillis - 60001;
  manualEmotionOverride = false;
  isYawning = false;
  lastBlink = hostMillis;
  customEyeActive = customEyeReady = false;
  activeBubble.active = false;
  activeBubble.text[0] = overrideThought[0] = '\0';
  thoughtLastOverride[0] = '\0';
  thoughtSchedulerActive = false;
  ldrEnabled = false;
  timeReceived = true;
  displayHour = 9;
  musicPlaying = false;
  currentTrack = currentArtist = weatherCondition = "";
  thoughtRoll = 50;
  if (theme == 1) t2_initEyes();
  else if (theme == 2) t3_initEyes();
  else drawEyes();
}

void frame() {
  if (activeTheme == 2) t3_updateEyes();
  else if (activeTheme == 1) t2_updateEyes();
  else {
    updateEyes();
    tickThoughtScheduler(&tft, false);
  }
}

template <std::size_t N>
bool inBank(const char* text, const char* const (&bank)[N]) {
  for (const char* entry : bank) if (std::strcmp(text, entry) == 0) return true;
  return false;
}

void checkVisual(Emotion expected) {
  check(currentEmotion == expected, "currentEmotion follows the time/light mood");
  if (activeTheme == 1) {
    int height = expected == EMO_SLEEPY ? T2P_Sleepy.Height :
                 expected == EMO_HAPPY ? T2P_Happy.Height : T2P_Normal.Height;
    check(t2_rightEye.Transition.Destin.Height == height, "theme2 visual preset matches ambient mood");
  } else if (activeTheme == 2) {
    check(t3_tired == (expected == EMO_SLEEPY) && t3_happy == (expected == EMO_HAPPY),
          "theme3 visual mood flags match ambient mood");
    if (expected == EMO_SLEEPY) check(t3_eyeLhNext == t3_scale(4), "theme3 sleepy lids are applied");
  }
}

void offlineThoughts(int theme) {
  fresh(theme, "offline-thoughts");
  thought_personality::sequence = 0;
  thought_personality::context[0] = 0;
  timeReceived = false; // No NTP, phone time, PC, music, or weather.
  initThoughtScheduler(); // Actual function extracted from the current .ino.
  uint32_t deadline = (uint32_t)nextThoughtTime;
  uint32_t wait = deadline - hostMillis;
  check(thoughtSchedulerActive && wait >= 15000 && wait < 45000,
        "first local thought is scheduled 15-45 seconds after init");
  check(getAutoEmotion() == EMO_NEUTRAL, "missing wall-clock time has a neutral fallback");
  // Main submits frames 33ms apart; back-to-back 1ms calls would correctly be
  // throttled by theme3 and would not model the firmware's frame cadence.
  hostMillis = deadline - 33;
  frame();
  check(!activeBubble.active, "first thought does not appear early");
  hostMillis = deadline;
  frame();
  check(activeBubble.active && activeBubble.text[0], "offline thought appears at the deadline");
  check(inBank(activeBubble.text, thought_personality::focus), "offline selection uses the local focus bank");
  uint32_t repeat = (uint32_t)nextThoughtTime - hostMillis;
  check(repeat == thoughtDisplayMs() + THOUGHT_FADE_MS + 1000UL,
        "offline thought schedules a later local thought");
  hostMillis += thoughtDisplayMs() + THOUGHT_FADE_MS;
  frame();
  check(!activeBubble.active, "offline thought expires normally");
  thoughtRoll = 90;
  hostMillis = (uint32_t)nextThoughtTime;
  frame();
  check(activeBubble.active && inBank(activeBubble.text, thought_personality::focus),
        "next offline thought uses the local focus bank without external data");
}

void hourlyMoods(int theme) {
  fresh(theme, "hourly-moods");
  for (int hour = 0; hour < 24; ++hour) {
    hostMillis += 5000;
    displayHour = hour;
    tickAmbientParent(); // Actual ambient block extracted from loop().
    Emotion expected = hour < 6 || hour >= 22 ? EMO_SLEEPY :
                       hour < 9 || (hour >= 12 && hour < 14) ? EMO_HAPPY : EMO_NEUTRAL;
    check(getAutoEmotion() == expected, "hourly selector matches the documented daily ranges");
    checkVisual(expected);
    check(millis() - lastManualEmotionTime > 60000, "automatic dispatch does not acquire a manual lease");
  }
  hostMillis += 5000;
  displayHour = 12; ldrEnabled = true; ldrValue = 99;
  tickAmbientParent(); checkVisual(EMO_SLEEPY);
  hostMillis += 5000;
  ldrValue = 100;
  tickAmbientParent(); checkVisual(EMO_HAPPY);
}

void manualLease(int theme) {
  fresh(theme, "manual-lease");
  displayHour = 23;
  if (theme == 2) t3_setEmotion(EMO_HAPPY);
  else if (theme == 1) t2_setEmotion(EMO_HAPPY);
  else setEmotion(EMO_HAPPY);
  unsigned long manualStarted = lastManualEmotionTime;
  hostMillis += 33;
  if (theme == 2) t3_drawEyesPage();
  else if (theme == 1) {
    t2_drawEyesPage();
    activeTheme = 0; drawEyes();
    activeTheme = 1; t2_drawEyesPage();
  } else drawEyes();
  checkVisual(EMO_HAPPY);
  check(lastManualEmotionTime == manualStarted, "reentry preserves the active manual-choice timestamp");
  hostMillis = (uint32_t)manualStarted + 59999;
  tickAmbientParent(); checkVisual(EMO_HAPPY);
  ++hostMillis; tickAmbientParent(); checkVisual(EMO_HAPPY); // Parent uses >60000.
  hostMillis += 3600000; tickAmbientParent(); checkVisual(EMO_HAPPY);
  resumeAutoEmotion(); checkVisual(EMO_SLEEPY);
  displayHour = 7;
  hostMillis += 5000; tickAmbientParent(); checkVisual(EMO_HAPPY);
}

void pageReentry(int theme) {
  fresh(theme, "mood-after-reentry");
  displayHour = 23;
  tickAmbientParent(); checkVisual(EMO_SLEEPY);
  unsigned long ambientLease = lastManualEmotionTime;
  if (theme == 2) {
    // This public page renderer runs t3_initEyes() on every eyes-page reentry.
    t3_drawEyesPage();
  } else if (theme == 1) {
    // Switching to theme3 frees theme2. Switching back initializes it again.
    t2_drawEyesPage();
    activeTheme = 2; t3_drawEyesPage();
    activeTheme = 1; t2_drawEyesPage();
  } else {
    drawEyes();
  }
  checkVisual(EMO_SLEEPY);
  check(lastManualEmotionTime == ambientLease, "reentry does not renew an automatic mood's manual lease");
  hostMillis += 5000;
  tickAmbientParent();
  checkVisual(EMO_SLEEPY);
}

void classicFrameOrdering() {
  fresh(0, "automatic-lease-in-real-frame-order");
  displayHour = 7;
  unsigned long manualBeforeFrame = lastManualEmotionTime;
  frame();
  check(currentEmotion == EMO_NEUTRAL, "Classic animation leaves ambient dispatch to main");
  check(lastManualEmotionTime == manualBeforeFrame, "Classic animation does not modify the manual lease");
  tickAmbientParent(); // Main runs the frame before its ambient block.
  checkVisual(EMO_HAPPY);
  check(millis() - lastManualEmotionTime > 60000,
        "Classic's legacy automatic path must not start a manual lease");
  displayHour = 23;
  hostMillis += 5001;
  frame();
  tickAmbientParent();
  checkVisual(EMO_SLEEPY);
}

int main() {
  int before = failures;
  for (int theme = 0; theme < 3; ++theme) offlineThoughts(theme);
  std::cout << (failures == before ? "PASS " : "FAIL ")
            << "offline scheduling for all three themes\n";
  before = failures;
  for (int theme = 0; theme < 3; ++theme) hourlyMoods(theme);
  for (int theme = 0; theme < 3; ++theme) manualLease(theme);
  std::cout << (failures == before ? "PASS " : "FAIL ")
            << "24-hour mood dispatch, optional light override, persistent manual priority, 5-second cadence\n";
  for (int theme = 0; theme < 3; ++theme) pageReentry(theme);
  classicFrameOrdering();
  std::cout << checks << " context checks, " << failures << " failures ("
            << SCREEN_W << "x" << SCREEN_H << ")\n";
  return failures ? 1 : 0;
}
