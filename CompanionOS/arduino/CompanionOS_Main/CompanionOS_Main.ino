/*
 * ═══════════════════════════════════════════════════════════
 *   COMPANION OS v6.0 — Full Production
 *   
 *   11 Pages: Eyes, Spotify, Pomodoro, Weather, 
 *             Notifications, Quick Notes, Stocks,
 *             Gaming, Social, Productivity, System Info
 *   V4: Pet personality, agent overlay, flash notifs
 *   V6: Stocks, Gaming, Social, Productivity dashboards
 * ═══════════════════════════════════════════════════════════
 */

#include "network.h"
#include "globals.h"
#include "eyes.h"
#include "pages.h"
#include "touch.h"
#include "ui.h"

// Define uninitialized globals
AppState currentState = STATE_EYES;
Emotion currentEmotion = EMO_NEUTRAL;
unsigned long lastBlink = 0;
bool isBlinking = false;
int blinkPhase = 0;
char udpBuffer[2048];

void runBootSequence() {
  tft.fillScreen(COLOR_BG);
  
  // Central Logo
  tft.setTextColor(COLOR_EYE);
  tft.drawCentreString("CompanionOS", SCREEN_W/2, 40, 4);
  tft.setTextColor(TFT_LIGHTGREY);
  tft.drawCentreString("v6.0 by neuralsin", SCREEN_W/2, 70, 2);

  delay(600);

  // Boot diagnostics
  struct BootTask {
    String name;
    String value;
  };

  BootTask tasks[6] = {
    {"CPU Core:", String(ESP.getCpuFreqMHz()) + " MHz [OK]"},
    {"RAM:", String(ESP.getFreeHeap() / 1024) + " KB Free"},
    {"FLASH:", String(ESP.getFlashChipRealSize() / 1024) + " KB [OK]"},
    {"NETWORK:", WiFi.localIP().toString()},
    {"LDR Sensor:", String(analogRead(A0))},
    {"Display:", "320x240 [OK]"}
  };

  tft.setTextFont(2);
  for (int i = 0; i < 6; i++) {
    int barY = 100 + (i * 20);
    tft.setTextColor(TFT_WHITE);
    tft.drawString(tasks[i].name, 10, barY);
    tft.drawRect(120, barY + 2, 80, 10, TFT_DARKGREY);
    for (int p = 0; p <= 76; p += 19) {
      tft.fillRect(122, barY + 4, p, 6, COLOR_EYE);
      delay(30);
    }
    tft.setTextColor(TFT_GREEN);
    tft.drawString(tasks[i].value, 210, barY);
    delay(100);
  }

  delay(800);
  tft.fillScreen(COLOR_BG);
}

void setup() {
  Serial.begin(115200);
  Serial.println(F("\n\n╔════════════════════════════════════════╗"));
  Serial.println(F("║   COMPANION OS v6.0 - Production       ║"));
  Serial.println(F("╚════════════════════════════════════════╝\n"));
  
  Serial.print(F("Display... "));
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(COLOR_BG);
  Serial.println(F("OK"));
  
  Serial.print(F("Touch... "));
  ts.begin();
  ts.setRotation(1);
  Serial.println(F("OK"));
  
  pinMode(TOUCH_LEFT, INPUT);
  pinMode(TOUCH_RIGHT, INPUT);
  // A0 is used for LDR (analog input, no pinMode needed)
  Serial.println(F("Sensors... OK"));
  
  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.end();
  
  setupWiFi();
  runBootSequence();
  
  lastInteractionTime = millis();
  
  renderCurrentPage();
  Serial.println(F("\n✓ Ready!\n"));
}

void loop() {
  handleNetwork();
  handleTouch();
  
  // Non-blocking UI frame timer (~20 FPS)
  static unsigned long lastFrameTime = 0;
  if (millis() - lastFrameTime >= 50) {
    lastFrameTime = millis();
    
    if (currentState == STATE_EYES) {
      updateEyes();
      
      // V4: Agent overlay on eyes page
      drawAgentOverlay();
    }
    
    // Flash notification timeout
    updateFlashNotification();
  }
  
  // Status bar refresh every 1 second (keeps clock updating on ALL pages)
  static unsigned long lastStatusBarUpdate = 0;
  if (millis() - lastStatusBarUpdate >= 1000) {
    lastStatusBarUpdate = millis();
    drawStatusBar();
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
  
  // Native internal clock tick (1s resolution between UDP syncs)
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
    ldrValue = analogRead(A0);
  }
}
