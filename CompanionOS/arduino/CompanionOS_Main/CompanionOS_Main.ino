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

#include "network.h"
#include "globals.h"
#include "eyes.h"
#include "thought_engine.h"
#include "pages.h"
#include "ui.h"

#if HAS_TOUCH
  #include "touch.h"
#endif
#ifdef ESP32
  #include "buttons.h"
#endif

// Define uninitialized globals
AppState currentState = STATE_EYES;
Emotion currentEmotion = EMO_NEUTRAL;
unsigned long lastBlink = 0;
bool isBlinking = false;
int blinkPhase = 0;
char udpBuffer[2048];

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
  // First thought between 5-15 min (not immediately)
  nextThoughtTime = millis() + random(300000, 900000);
  thoughtSchedulerActive = true;
}

void tickThoughtScheduler() {
  if (!thoughtSchedulerActive) return;
  if (currentState != STATE_EYES) return;  // Only on eyes page

  // Check if bubble is currently active — handle fade/display
  if (activeBubble.active) {
    renderThoughtBubble();  // draw/fade current bubble
    return;
  }

  // Check for PC-pushed override thought (immediate display)
  if (strlen(overrideThought) > 0) {
    generateThought();  // will consume overrideThought
    activeBubble.active = true;
    activeBubble.shownAt = millis();
    activeBubble.fadeAlpha = 0;
    activeBubble.fadingIn = true;
    activeBubble.fadingOut = false;
    return;
  }

  // Scheduled thought generation
  if (millis() >= nextThoughtTime) {
    generateThought();
    activeBubble.active = true;
    activeBubble.shownAt = millis();
    activeBubble.fadeAlpha = 0;
    activeBubble.fadingIn = true;
    activeBubble.fadingOut = false;

    // Schedule next thought (45-90 min)
    nextThoughtTime = millis() + random(THOUGHT_MIN_INTERVAL_MS, THOUGHT_MAX_INTERVAL_MS);
  }
}

// ═══════════════════════════════════════════════════════════
// V6/V7: UDP TIME PARSER
// ═══════════════════════════════════════════════════════════

void updateTimeFromUDP(String t) {
  int colonIdx = t.indexOf(':');
  if (colonIdx > 0) {
    displayHour = t.substring(0, colonIdx).toInt();
    displayMinute = t.substring(colonIdx + 1).toInt();
    displaySecond = 0;
    timeReceived = true;
    lastTimeUpdateMillis = millis();
  }
}

// ═══════════════════════════════════════════════════════════
// SETUP
// ═══════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  Serial.println(F("\n\n╔════════════════════════════════════════╗"));
  Serial.println(F("║   COMPANION OS v7.0 - " PLATFORM_NAME "            ║"));
  Serial.println(F("╚════════════════════════════════════════╝\n"));

  // Display init
  Serial.print(F("Display... "));
  tft.init();
  #ifdef ESP32
    // 🔴 BUG-03 FIX: Landscape (rotation=1) = 160w×128h
    // Matches original ESP8266 aspect ratio. SCALE_X/Y normalize cleanly.
    tft.setRotation(1);
  #else
    tft.setRotation(1);   // ILI9341 320×240 landscape
  #endif
  tft.fillScreen(CLR_BG);
  Serial.println(F("OK"));

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
    Serial.println(F("OK (GPIO34/35/36)"));
  #endif

  // [LEGACY - v6.0] TOUCH_LEFT/TOUCH_RIGHT sensor init REMOVED
  // Pins GPIO0/GPIO2 freed (see config_esp8266.h)

  // LDR analog sensor
  #ifdef ESP32
    // LDR on GPIO39 (ADC1, input-only)
    analogReadResolution(10);  // 10-bit for parity with ESP8266
  #endif
  Serial.println(F("Sensors... OK"));

  // EEPROM
  EEPROM.begin(EEPROM_SIZE);
  activeTheme = EEPROM.read(EEPROM_ACTIVE_THEME_ADDR);
  if (activeTheme > 1) activeTheme = 0;  // Sanitize on first boot (0xFF → 0)
  EEPROM.end();

  // Network (WiFi + BT on ESP32)
  setupWiFi();
  #ifdef ESP32
    setupBluetooth();
  #endif

  // Boot sequence animation
  runBootSequence();

  // Initialize thought engine
  initThoughtScheduler();

  lastInteractionTime = millis();

  renderCurrentPage();
  Serial.println(F("\n✓ Ready!\n"));
}

// ═══════════════════════════════════════════════════════════
// MAIN LOOP
// ═══════════════════════════════════════════════════════════

void loop() {
  handleNetwork();

  // Input handling — platform specific
  #if HAS_TOUCH
    handleTouch();
  #endif
  #ifdef ESP32
    handleButtons();
  #endif

  // Non-blocking UI frame timer (~20 FPS)
  static unsigned long lastFrameTime = 0;
  if (millis() - lastFrameTime >= 50) {
    lastFrameTime = millis();

    if (currentState == STATE_EYES) {
      if (activeTheme == 1) {
        t2_updateEyes();
      } else {
        updateEyes();
      }
      // V7: Thought bubble rendering
      tickThoughtScheduler();
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
  }

  // Status bar refresh every 1 second
  static unsigned long lastStatusBarUpdate = 0;
  if (millis() - lastStatusBarUpdate >= 1000) {
    lastStatusBarUpdate = millis();
    drawStatusBar();
  }

  // Network stats page refresh every 2 seconds
  static unsigned long lastNetRefresh = 0;
  if (currentState == STATE_NETWORK && millis() - lastNetRefresh >= 2000) {
    lastNetRefresh = millis();
    redrawNetworkPartial();
  }

  // V4: Pet mood decay (every 60 seconds of no interaction)
  if (millis() - lastMoodDecay > 60000) {
    lastMoodDecay = millis();
    if (petMoodLevel > 0) petMoodLevel -= 2;
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
    lastTimeUpdateMillis = millis();
    displaySecond++;
    if (displaySecond >= 60) {
      displaySecond = 0;
      displayMinute++;
      if (displayMinute >= 60) {
        displayMinute = 0;
        displayHour = (displayHour + 1) % 24;
      }
    }
  }

  // LDR read (every 2s)
  static unsigned long lastLDR = 0;
  if (millis() - lastLDR > 2000) {
    lastLDR = millis();
    #ifdef ESP32
      ldrValue = analogRead(LDR_PIN);
    #else
      ldrValue = analogRead(A0);
    #endif
  }
}
