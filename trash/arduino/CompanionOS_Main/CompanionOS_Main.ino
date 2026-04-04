/*
 * ═══════════════════════════════════════════════════════════
 *   COMPANION OS v6.0 — Multi-Page Dashboard Engine
 *   
 *   11 Pages: Eyes, Spotify, Pomodoro, Weather, 
 *            Notifications, Notes, Stocks, Gaming,
 *            Social, Productivity, Settings
 *   11 Themes: Legacy, Exotic, Pikachu, Chill, Gaming,
 *              Minimal, Angry, Sleep, Mood, System, Companion
 *   V6: Full dashboard pages with premium UI
 * ═══════════════════════════════════════════════════════════
 */

#include <LittleFS.h>
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
char udpBuffer[512];

void runBootSequence() {
  tft.fillScreen(COLOR_BG);
  
  // Central Logo
  tft.setTextColor(COLOR_EYE);
  tft.drawCentreString("CompanionOS", SCREEN_W/2, 35, 4);
  tft.setTextColor(TFT_LIGHTGREY);
  tft.drawCentreString("v6.0 by neuralsin", SCREEN_W/2, 65, 2);

  // V5: Show active theme name
  char bootThemeName[16];
  getThemeName(currentThemeId, bootThemeName, sizeof(bootThemeName));
  char themeLabel[32];
  sprintf(themeLabel, "Theme: %s", bootThemeName);
  tft.setTextColor(getThemePrimaryColor(currentThemeId));
  tft.drawCentreString(themeLabel, SCREEN_W/2, 88, 1);

  delay(600);

  // Boot diagnostics
  struct BootTask {
    String name;
    String value;
  };

  BootTask tasks[7] = {
    {"CPU Core:", String(ESP.getCpuFreqMHz()) + " MHz [OK]"},
    {"RAM:", String(ESP.getFreeHeap() / 1024) + " KB Free"},
    {"FLASH:", String(ESP.getFlashChipRealSize() / 1024) + " KB [OK]"},
    {"LITTLEFS:", "Bypassed [OK]"},
    {"NETWORK:", WiFi.localIP().toString()},
    {"LDR Sensor:", String(analogRead(A0))},
    {"Display:", "320x240 [OK]"}
  };

  tft.setTextFont(2);
  for (int i = 0; i < 7; i++) {
    int barY = 110 + (i * 20);
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
  Serial.println(F("║   COMPANION OS v6.0 - Dashboard     ║"));
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
  Serial.println(F("Sensors... OK"));
  
  Serial.print(F("LittleFS... "));
  if (!LittleFS.begin()) {
    Serial.println(F("FAIL! Check Data Upload"));
  } else {
    Serial.println(F("OK"));
  }
  
  // V5: Load theme from EEPROM (migrates V4 exotic flag on first boot)
  loadThemeFromEEPROM();
  char tName[16];
  getThemeName(currentThemeId, tName, sizeof(tName));
  Serial.print(F("Theme: "));
  Serial.print(tName);
  Serial.print(F(" (ID: "));
  Serial.print(currentThemeId);
  Serial.println(F(")"));
  
  // V4: Initialize pet personality
  lastInteractionTime = millis();
  lastMoodDecay = millis();
  petMoodLevel = 100;
  
  setupWiFi();
  runBootSequence();
  
  renderCurrentPage();
  Serial.println(F("\n✓ Ready!\n"));
}

void loop() {
  handleNetwork();
  handleTouch();
  
  // Local native time ticking
  if (timeReceived && millis() - lastTimeUpdateMillis >= 1000) {
     lastTimeUpdateMillis += 1000;
     displaySecond++;
     if (displaySecond >= 60) {
        displaySecond = 0;
        displayMinute++;
        
        // Refresh the global clock visually
        if (currentState < STATE_STOCKS) { // Legacy pages render the status bar
           extern void drawStatusBar();
           drawStatusBar();
        }
        
        if (displayMinute >= 60) {
           displayMinute = 0;
           displayHour++;
           if (displayHour >= 24) displayHour = 0;
        }
     }
  }

  // Non-blocking UI frame timer (~20 FPS)
  static unsigned long lastFrameTime = 0;
  if (millis() - lastFrameTime >= 50) {
    lastFrameTime = millis();
    
    if (currentState == STATE_EYES) {
      updateEyes();
    }
    // V6: Productivity clock tick (update every frame for blinking colon)
    else if (currentState == STATE_PRODUCTIVITY) {
      redrawProductivityPartial();
    }
    // V7: Settings smooth scroll engine
    else if (currentState == STATE_SETTINGS) {
      extern int settingsScrollY;
      extern float smoothScrollY;
      if (abs(smoothScrollY - settingsScrollY) > 0.5f) {
        redrawSettingsPartial();
      }
    }
    
    // Flash notification timeout
    updateFlashNotification();
  }
}
