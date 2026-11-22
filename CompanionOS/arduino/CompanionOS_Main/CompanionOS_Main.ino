/*
 * ═══════════════════════════════════════════════════════════
 *   COMPANION OS v7.0 — Dual Platform (ESP32 + ESP8266)
 *   
 *   13 Pages: Eyes, Spotify, Pomodoro, Weather,
 *             Notifications, Quick Notes, Stocks,
 *             Gaming, Social, Productivity, Network,
 *             System Info, Dr.Hack (ESP32 only)
 *   V4: Pet personality, agent overlay, flash notifs
 *   V6: Stocks, Gaming, Social, Productivity dashboards
 *   V7: Physical buttons, thought bubbles, BT transport,
 *       Dr. Hack hacking suite, resolution scaling
 * ═══════════════════════════════════════════════════════════
 */

#include "globals.h"
#include "boot_health.h"
#include "display_power.h"
#include "art_memory.h"
static bool workspaceRestart = false;

#ifdef ESP32
#include <esp_bt.h>
#include <esp32-hal-bt.h>
#include <esp_system.h>
RTC_NOINIT_ATTR uint32_t requestedBootPage;

// Let Arduino release controller RAM before setup when Bluetooth is disabled.
bool btInUse() { return ENABLE_BLE_TOOLS; }

bool switchMemoryWorkspace(int targetPage) {
#if !ENABLE_BLE_TOOLS
  (void)targetPage;
  return false;
#else
  if ((targetPage == STATE_DR_HACK) == companion_memory::radioWorkspace()) return false;
  requestedBootPage = companion_memory::encodeBootPage(unsigned(targetPage));
  Serial.println(F("[memory] Restarting into requested workspace"));
  Serial.flush();
  ESP.restart();
  return true;
#endif
}
#endif

#ifdef ESP32
  #include "buttons.h"
  #include "rotary_knob.h"
#endif

#include "ui_components.h"
#include "companion_net.h"
#include "eyes.h"
#ifdef ESP32
  #include "csi_collector.h"
#endif
#include "thought_engine.h"
#include "theme3_eyes.h"
#include "pages.h"
#include "ui.h"
#include "boot_screen.h"

#if HAS_TOUCH
  #include "touch.h"
#endif
#ifdef ESP32
  #include "soc/soc.h"
  #include "soc/rtc_cntl_reg.h"
#endif

// Define uninitialized globals
AppState currentState = STATE_EYES;
Emotion currentEmotion = EMO_NEUTRAL;
unsigned long lastManualEmotionTime = 0;
unsigned long lastBlink = 0;
bool isBlinking = false;
int blinkPhase = 0;
char udpBuffer[2048];

uint16_t* customEyeImg = nullptr;
bool customEyeActive = false;
bool customEyeReady = false;
uint8_t customEyeRowsReceived[128] = {0};
uint16_t* albumArt = nullptr;
uint16_t* albumArtIncoming = nullptr;
uint16_t* galleryImage = nullptr;

// Thought engine scheduler state
unsigned long nextThoughtTime = 0;
bool thoughtSchedulerActive = false;

// ═══════════════════════════════════════════════════════════
// V7 BOOT SEQUENCE — Platform-aware ASCII logo + diagnostics
// ═══════════════════════════════════════════════════════════

void runBootSequence() {
  const uint32_t started = millis();
  drawBootFrame(0, true);
  display_power::show();
  while (uint32_t(millis() - started) < 900) {
    drawBootFrame(uint32_t(millis() - started), false);
    display_power::tick();
    delay(16);
  }
  drawBootFrame(900, false);
}

// ═══════════════════════════════════════════════════════════
// V7 THOUGHT BUBBLE SCHEDULER
// ═══════════════════════════════════════════════════════════

void initThoughtScheduler() {
  // Offline personality is visible without waiting for a host or API.
  nextThoughtTime = millis() + random(15000, 45000);
  thoughtSchedulerActive = true;
}


// V6/V7: UDP TIME PARSER
// ═══════════════════════════════════════════════════════════

void updateTimeFromUDP(String t) {
  if (t.length() < 4 || t.length() > 8) return;
  const char* part = t.c_str();
  char* end = nullptr;
  long hour = strtol(part, &end, 10);
  if (end == part || *end != ':') return;
  part = end + 1;
  long minute = strtol(part, &end, 10);
  if (end == part) return;
  long second = 0;
  if (*end == ':') {
    part = end + 1;
    second = strtol(part, &end, 10);
    if (end == part) return;
  }
  if (*end || hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) return;
  displayHour = (int)hour;
  displayMinute = (int)minute;
  displaySecond = (int)second;
  timeReceived = true;
  lastTimeUpdateMillis = millis();
}

// ═══════════════════════════════════════════════════════════
// SETUP
// ═══════════════════════════════════════════════════════════

void setup() {
  display_power::off();
  Serial.begin(115200);
  boot_health::begin();
  #ifdef ESP32
  currentState = (AppState)companion_memory::decodeBootPage(
      requestedBootPage, esp_reset_reason() == ESP_RST_SW, STATE_COUNT);
  workspaceRestart = requestedBootPage == companion_memory::encodeBootPage(unsigned(currentState)) &&
                     esp_reset_reason() == ESP_RST_SW;
  requestedBootPage = 0; // One-shot: a crash/power cycle must not loop into a tool.
  companion_memory::radioWorkspace() = ENABLE_BLE_TOOLS && currentState == STATE_DR_HACK;
  #if ENABLE_BLE_TOOLS
  if (!companion_memory::radioWorkspace()) {
    auto before = companion_memory::snapshot();
    esp_err_t rc = esp_bt_mem_release(ESP_BT_MODE_BTDM);
    auto after = companion_memory::snapshot();
    Serial.printf("[memory:LYRICS-UI-v3] Bluetooth release=%d reclaimed=%u free=%u largest=%u\n",
                  int(rc), unsigned(after.free >= before.free ? after.free - before.free : 0),
                  unsigned(after.free), unsigned(after.largest));
  }
  #else
  Serial.println(F("[boot] Bluetooth disabled; controller RAM released by core"));
  #endif
  #endif
  Serial.println(F("BOOTING... STEP 1"));

  Serial.println(F("\n\n╔════════════════════════════════════════╗"));
  Serial.println(F("║   COMPANION OS v7.0 - " PLATFORM_NAME "            ║"));
  Serial.println(F("╚════════════════════════════════════════╝\n"));

  Serial.println(F("BOOTING... STEP 2 (Display Init)"));
  boot_health::stage(2);
  // Display init
  Serial.print(F("Display... "));
  
  tft.init();
  #ifdef ESP32
    tft.setRotation(1);
  #else
    tft.setRotation(1);
  #endif
  tft.fillScreen(CLR_BG);
  display_power::attach();
  display_power::write(0);
  Serial.println(F("OK"));
  if (boot_health::recovery) {
    #ifdef ESP32
    initButtons();
    #endif
    renderCurrentPage();
    display_power::show();
    return; // Recovery must not repeat a failing NVS/WiFi/API startup.
  }

  Serial.println(F("BOOTING... STEP 3 (Input Init)"));
  boot_health::stage(3);
  // Input init
  #if HAS_TOUCH
    Serial.print(F("Touch... "));
    ts.begin();
    ts.setRotation(1);
    Serial.println(F("OK"));
  #endif

  #ifdef ESP32
    Serial.print(F("Buttons... "));
    initButtons();
    Serial.println(F("OK (GPIO13/14/27)"));
    Serial.print(F("Rotary Knob... "));
    initRotaryKnob();
    Serial.println(F("OK (GPIO36)"));
  #endif

  #ifdef ESP32
    analogReadResolution(12);
  #endif
  Serial.println(F("Sensors... OK"));

  Serial.println(F("BOOTING... STEP 4 (EEPROM Init)"));
  boot_health::stage(4);
  // EEPROM
  EEPROM.begin(EEPROM_SIZE);
  activeTheme = EEPROM.read(EEPROM_ACTIVE_THEME_ADDR);
  if (activeTheme >= THEME_COUNT) activeTheme = 0;  // Sanitize on first boot (0xFF → 0)
  EEPROM.end();
  companion_planner::init();

  // Allocate the driver before eye canvases can fragment its startup heap.
  // Web services and API jobs still wait for a stable first frame and link.
  boot_health::checkpoint(50);
  setupWiFi();

  Serial.println(F("BOOTING... STEP 6 (Boot Sequence)"));
  // Boot sequence animation
  boot_health::stage(6);
  if (!workspaceRestart && !boot_health::recovery) runBootSequence();

  Serial.println(F("BOOTING... STEP 7 (Thought Engine)"));
  // Initialize thought engine
  initThoughtScheduler();
  boot_health::stage(7);

  // Offline mode keeps the face, clock and thoughts local. Connectivity retries
  // run quietly in the main loop; Bluetooth is never an automatic fallback.
  // CSI is initialized only when its tile is opened.

  lastInteractionTime = millis();

  boot_health::checkpoint(70);
  renderCurrentPage();
  boot_health::checkpoint(71);
  if (workspaceRestart) display_power::show();
  companion_startup::gate.firstFrame(millis());
  boot_health::stage(8);
  Serial.println(F("\n✓ Ready!\n"));
}

// ═══════════════════════════════════════════════════════════
// MAIN LOOP
// ═══════════════════════════════════════════════════════════

void loop() {
  boot_health::healthy();
  display_power::tick();
  if (boot_health::recovery) {
    #ifdef ESP32
    static uint32_t pressedAt = 0;
    if (digitalRead(BTN_SELECT) == BTN_ACTIVE_LEVEL) {
      if (!pressedAt) pressedAt = millis();
      if (uint32_t(millis() - pressedAt) > 2000) boot_health::retry();
    } else pressedAt = 0;
    #endif
    delay(10); return;
  }
  tickWiFiConnectivity();
  boot_health::stage(9);
  handleNetwork();
  boot_health::stage(10);
  companion_planner::tick();

  // V8: Tick CSI collector — only when RuView page is active to save bandwidth and CPU
  #ifdef ESP32
    if (currentState == STATE_RUVIEW) {
      tickCSICollector();
    }
  #endif

  // Input handling — platform specific
  #if HAS_TOUCH
    handleTouch();
  #endif
  #ifdef ESP32
    handleButtons();
    tickRotaryKnob();
  #endif

  // Non-blocking UI frame timer (~20 FPS)
  static unsigned long lastFrameTime = 0;
  if (millis() - lastFrameTime >= 33) {
    lastFrameTime = millis();

    if (currentState == STATE_EYES) {
      if (activeTheme == 2) {
        t3_updateEyes();
      } else if (activeTheme == 1) {
        t2_updateEyes();
      } else {
        updateEyes();
      }
      if (activeTheme == 0) tickThoughtScheduler(&tft, false);
      
      // V4: Agent overlay on eyes page
      drawAgentOverlay();
      // V7: Flash notification overlay
      drawFlashNotification();
      companion_planner::drawAlert();
      // V7: Dr. Hack entry tile on eyes page (ESP32)
      #ifdef ESP32
        drawDrHackTile();
      #endif
    }

    // Flash notification timeout
    updateFlashNotification();
    if (currentState == STATE_RUVIEW) redrawRuviewPartial();
  }

  // Status bar refresh every 1 second
  static unsigned long lastStatusBarUpdate = 0;
  if (millis() - lastStatusBarUpdate >= 1000) {
    lastStatusBarUpdate = millis();
    if (currentState != STATE_DR_HACK && currentState != STATE_RUVIEW && 
        currentState != STATE_PC_DASH) {
      drawStatusBar();
    }
  }

  // PC Dashboard page refresh / offline timeout check
  if (currentState == STATE_PC_DASH) {
    extern void tickPcDash();
    tickPcDash();
  }

  // Network stats page refresh every 2 seconds
  static unsigned long lastNetRefresh = 0;
  if (currentState == STATE_NETWORK && millis() - lastNetRefresh >= 2000) {
    lastNetRefresh = millis();
    redrawNetworkPartial();
  }

  // Clock / Screensaver Dashboard page refresh
  if (currentState == STATE_CLOCK_DASHBOARD) {
    extern void redrawClockDashboardPartial();
    redrawClockDashboardPartial();
  }

  // Keep transport time advancing on every page, with no one-second jumps.
  unsigned long progressNow = millis();
  if (isPlaying && playDuration > 0) {
    playProgress = min(playDuration, playProgress + (int)(progressNow - spotifyProgressUpdatedAt));
  }
  spotifyProgressUpdatedAt = progressNow;
  tickLyricsNavigation();
  tickRemoteLyrics();
  redrawLyricsPage();
  tickSpotifyVisuals();

  if (!pomoActive || isHostPomodoroActive()) {
    pomoClockBaseline = millis();
  } else if (millis() - pomoClockBaseline >= 1000) {
    int seconds = (millis() - pomoClockBaseline) / 1000;
    pomoClockBaseline += seconds * 1000UL;
    pomoRemaining = max(0, pomoRemaining - seconds);
    if (pomoRemaining == 0) {
      pomoActive = false;
      if (!pomoIsBreak) pomoSessions++;
      showFlashNotification(pomoIsBreak ? "Break complete" : "Focus complete");
    }
    redrawPomodoroPartial();
  }

  // Retro Watch Chronograph tile page refresh
  if (currentState == STATE_RETRO_WATCH) {
    extern void redrawRetroWatchPartial();
    redrawRetroWatchPartial();
  }

  // V4: Pet mood decay (every 60 seconds of no interaction)
  if (millis() - lastMoodDecay > 60000) {
    lastMoodDecay = millis();
    petMoodLevel = max(0, petMoodLevel - 2);
  }

  tickIdleExpression();

  // Native internal clock tick (1s resolution between NTP syncs)
  if (timeReceived && millis() - lastTimeUpdateMillis >= 1000) {
    unsigned long elapsedSeconds = (millis() - lastTimeUpdateMillis) / 1000;
    lastTimeUpdateMillis += elapsedSeconds * 1000;
    displaySecond += elapsedSeconds;
    if (displaySecond >= 60) {
      displayMinute += displaySecond / 60;
      displaySecond %= 60;
      if (displayMinute >= 60) {
        displayHour = (displayHour + displayMinute / 60) % 24;
        displayMinute %= 60;
      }
    }
  }

  // LDR read (every 2s)
  static unsigned long lastLDR = 0;
  if (ldrEnabled && millis() - lastLDR > 2000) {
    lastLDR = millis();
    #ifdef ESP32
      ldrValue = analogRead(LDR_PIN) >> 2;
    #else
      ldrValue = analogRead(A0);
    #endif
  }

  // Apply ambient time/light context to all themes, preserving manual choices.
  static unsigned long lastAmbientCheck = 0;
  if (currentState == STATE_EYES && millis() - lastAmbientCheck >= 5000 &&
      !manualEmotionOverride && !isYawning) {
    lastAmbientCheck = millis();
    Emotion mood = getAutoEmotion();
    if (mood != currentEmotion) {
      applyAmbientEmotion(mood);
    }
  }

}
