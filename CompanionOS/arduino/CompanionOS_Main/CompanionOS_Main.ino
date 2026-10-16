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
  tft.fillScreen(CLR_BG);

  // Phase 1: ASCII Logo "C/OS" with gradient glow
  int logoY = SCALE_Y(10);
  tft.setTextColor(CLR_PRIMARY);
  tft.drawCentreString("C/OS", SCR_CX, logoY, 4);
  
  tft.setTextColor(CLR_TEXT_MED);
  tft.drawCentreString("CompanionOS v7.0", SCR_CX, logoY + SCALE_Y(30), 2);
  tft.setTextColor(CLR_TEXT_LO);
  tft.drawCentreString("by neuralsin", SCR_CX, logoY + SCALE_Y(48), 1);

  // Glow ring around "C/OS"
  uint16_t glowColor = darkenColor(CLR_PRIMARY, 60);
  tft.drawRoundRect(SCR_CX - SCALE_X(30), logoY - SCALE_Y(4), SCALE_X(60), SCALE_Y(28), 4, glowColor);
  
  delay(600);

  // Phase 2: System diagnostics
  int diagY = SCALE_Y(72);
  int lineH = SCALE_Y(14);
  int barX = SCALE_X(70);
  int barW = SCALE_X(40);
  int barH = SCALE_Y(6);
  int valX = barX + barW + SCALE_X(6);

  struct BootTask {
    const char* name;
    String value;
  };

  BootTask tasks[] = {
    {"CPU:", String(ESP.getCpuFreqMHz()) + "MHz"},
    {"RAM:", String(ESP.getFreeHeap() / 1024) + "KB"},
    {"NET:", wifiConnected ? WiFi.localIP().toString() : "offline"},
    {"DSP:", String(SCREEN_W) + "x" + String(SCREEN_H)},
    {"PLT:", PLATFORM_NAME},
    #ifdef ESP32
    {"BTN:", "L/R/SEL"},
    #else
    {"TCH:", "XPT2046"},
    #endif
  };

  int taskCount = sizeof(tasks) / sizeof(tasks[0]);

  for (int i = 0; i < taskCount; i++) {
    int y = diagY + i * lineH;

    // Label
    tft.setTextColor(CLR_TEXT_LO);
    tft.drawString(tasks[i].name, SCALE_X(4), y, 1);

    // Progress bar animation
    tft.drawRect(barX, y + 1, barW, barH, CLR_BORDER);
    for (int p = 0; p < barW - 2; p += SCALE_X(4)) {
      tft.fillRect(barX + 1, y + 2, p, barH - 2, CLR_PRIMARY);
      delay(10);
    }
    tft.fillRect(barX + 1, y + 2, barW - 2, barH - 2, CLR_SUCCESS); // done = green

    // Value
    tft.setTextColor(CLR_SUCCESS);
    tft.drawString(tasks[i].value, valX, y, 1);

    delay(50);
  }

  // Phase 3: Separator + ready message
  int readyY = diagY + taskCount * lineH + SCALE_Y(8);
  drawSeparator(SCALE_X(10), readyY, SCR_W - SCALE_X(20), CLR_PRIMARY);

  tft.setTextColor(CLR_PRIMARY);
  tft.drawCentreString("SYSTEMS ONLINE", SCR_CX, readyY + SCALE_Y(6), 1);

  // Scanline wipe transition
  delay(600);
  for (int y = 0; y < SCR_H; y += 2) {
    tft.drawFastHLine(0, y, SCR_W, CLR_BG);
    tft.drawFastHLine(0, y + 1, SCR_W, CLR_BG);
    delay(4);
  }
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
  albumArt = (uint16_t*)malloc(ALBUM_ART_BYTES);
  albumArtIncoming = (uint16_t*)malloc(ALBUM_ART_BYTES);
  if (albumArt) memset(albumArt, 0, ALBUM_ART_BYTES);
  if (albumArtIncoming) memset(albumArtIncoming, 0, ALBUM_ART_BYTES);
  galleryImage = albumArt;
  #ifdef ESP32
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Disable brownout detector to prevent reboot loop on WiFi connect
  #endif
  
  Serial.begin(115200);
  delay(1000); // Wait for serial to settle
  Serial.println(F("BOOTING... STEP 1"));

  Serial.println(F("\n\n╔════════════════════════════════════════╗"));
  Serial.println(F("║   COMPANION OS v7.0 - " PLATFORM_NAME "            ║"));
  Serial.println(F("╚════════════════════════════════════════╝\n"));

  Serial.println(F("BOOTING... STEP 2 (Display Init)"));
  // Display init
  Serial.print(F("Display... "));
  #ifdef ESP32
    #ifdef TFT_BL
      // BROWNOUT FIX: Start backlight DIM during boot to reduce current draw.
      // Will ramp up to full brightness after WiFi/BT are stable.
      #if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
        ledcAttach(TFT_BL, 5000, 8);
        ledcWrite(TFT_BL, 64); // 25% brightness during boot (low power)
      #else
        ledcSetup(0, 5000, 8);
        ledcAttachPin(TFT_BL, 0);
        ledcWrite(0, 64); // 25% brightness during boot (low power)
      #endif
    #endif
  #endif
  
  tft.init();
  #ifdef ESP32
    tft.setRotation(1);
    SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI);
  #else
    tft.setRotation(1);
  #endif
  tft.fillScreen(CLR_BG);
  Serial.println(F("OK"));

  Serial.println(F("BOOTING... STEP 3 (Input Init)"));
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
  // EEPROM
  EEPROM.begin(EEPROM_SIZE);
  activeTheme = EEPROM.read(EEPROM_ACTIVE_THEME_ADDR);
  if (activeTheme >= THEME_COUNT) activeTheme = 0;  // Sanitize on first boot (0xFF → 0)
  EEPROM.end();

  #ifdef ESP32
    setupStandaloneServices();
  #endif

  Serial.println(F("BOOTING... STEP 5 (Network Init)"));
  
  // Network — match legacy: just call setupWiFi(), no BT during boot
  setupWiFi();

  // Backlight to full after WiFi
  #ifdef ESP32
    #ifdef TFT_BL
      #if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
        ledcWrite(TFT_BL, 220);
      #else
        ledcWrite(0, 220);
      #endif
    #endif
  #endif

  Serial.println(F("BOOTING... STEP 6 (Boot Sequence)"));
  // Boot sequence animation
  runBootSequence();

  Serial.println(F("BOOTING... STEP 7 (Thought Engine)"));
  // Initialize thought engine
  initThoughtScheduler();

  // Offline mode keeps the face, clock and thoughts local. Connectivity retries
  // run quietly in the main loop; Bluetooth is never an automatic fallback.
  #ifdef ESP32
    if (wifiConnected) {
      setupCSICollector();
    }
  #endif

  lastInteractionTime = millis();

  renderCurrentPage();
  Serial.println(F("\n✓ Ready!\n"));
}

// ═══════════════════════════════════════════════════════════
// MAIN LOOP
// ═══════════════════════════════════════════════════════════

void loop() {
  tickWiFiConnectivity();
  handleNetwork();

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

  // V4: Auto-yawn after 5 mins of no interaction
  if (currentState == STATE_EYES && !isYawning) {
    if (millis() - lastInteractionTime > 300000 && petMoodLevel < 30) {
      isYawning = true;
      yawnStart = millis();
      setEmotion(EMO_SLEEPY);
    }
  }
  if (isYawning && millis() - yawnStart > 3000) {
    isYawning = false;
    setEmotion(EMO_NEUTRAL);
  }

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
  if (millis() - lastLDR > 2000) {
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
      millis() - lastManualEmotionTime > 60000) {
    lastAmbientCheck = millis();
    Emotion mood = getAutoEmotion();
    if (mood != currentEmotion) {
      if (activeTheme == 2) t3_setEmotion(mood);
      else if (activeTheme == 1) t2_setEmotion(mood);
      else setEmotion(mood);
      // This was an automatic change, so it must not acquire a manual lease.
      lastManualEmotionTime = millis() - 60001UL;
    }
  }

}
