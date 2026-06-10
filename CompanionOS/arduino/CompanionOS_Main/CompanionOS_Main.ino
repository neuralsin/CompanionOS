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
#endif

#include "ui_components.h"
#include "companion_net.h"
#include "eyes.h"
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
unsigned long lastBlink = 0;
bool isBlinking = false;
int blinkPhase = 0;
char udpBuffer[2048];

uint16_t* customEyeImg = nullptr;
bool customEyeActive = false;
bool customEyeReady = false;

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
  #endif

  #ifdef ESP32
    analogReadResolution(10);
  #endif
  Serial.println(F("Sensors... OK"));

  Serial.println(F("BOOTING... STEP 4 (EEPROM Init)"));
  // EEPROM
  EEPROM.begin(EEPROM_SIZE);
  activeTheme = EEPROM.read(EEPROM_ACTIVE_THEME_ADDR);
  if (activeTheme >= THEME_COUNT) activeTheme = 0;  // Sanitize on first boot (0xFF → 0)
  EEPROM.end();

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

  lastInteractionTime = millis();

  renderCurrentPage();
  Serial.println(F("\n✓ Ready!\n"));
}

// ═══════════════════════════════════════════════════════════
// MAIN LOOP
// ═══════════════════════════════════════════════════════════

void loop() {
  // Lazy BT init — Commented out to prevent brownout during WiFi.
  // Bluetooth is now initialized dynamically when needed (e.g. Dr. Hack).
  /*
  #ifdef ESP32
  {
    static bool btLazyDone = false;
    if (!btLazyDone && millis() > 10000) {
      btLazyDone = true;
      setupBluetooth();
      Serial.println(F("BT lazy-init complete (10s post-boot)"));
    }
  }
  #endif
  */

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
      if (activeTheme == 2) {
        t3_updateEyes();
      } else if (activeTheme == 1) {
        t2_updateEyes();
      } else {
        updateEyes();
        tickThoughtScheduler(&tft, false);
      }
      
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

  // WiFi auto-reconnect (check every 30s)
  static unsigned long lastWifiCheck = 0;
  if (!wifiConnected && millis() - lastWifiCheck > 30000) {
    lastWifiCheck = millis();
    Serial.println(F("WiFi disconnected, attempting reconnect..."));
    WiFi.disconnect();
    delay(100);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 20) {
      delay(500);
      timeout++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      Serial.print(F("WiFi reconnected! IP: "));
      Serial.println(WiFi.localIP());
    } else {
      Serial.println(F("WiFi reconnect failed, will retry in 30s"));
    }
  }
}
