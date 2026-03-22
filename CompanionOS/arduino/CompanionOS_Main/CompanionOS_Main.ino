/*
 * ═══════════════════════════════════════════════════════════
 *   COMPANION OS v3.0 — Full Production
 *   
 *   7 Pages: Eyes, Spotify, Pomodoro, Weather, 
 *            Notifications, Quick Notes, System Info
 *   LDR sensor support on A0
 *   Time-based + light-based auto emotions
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
char udpBuffer[512];

void runBootSequence() {
  tft.fillScreen(COLOR_BG);
  
  // Central Logo
  tft.setTextColor(COLOR_EYE);
  tft.drawCentreString("CompanionOS", SCREEN_W/2, 40, 4);
  tft.setTextColor(TFT_LIGHTGREY);
  tft.drawCentreString("v3.0 by neuralsin", SCREEN_W/2, 70, 2);

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
  Serial.println(F("║   COMPANION OS v3.0 - Production       ║"));
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
  
  setupWiFi();
  runBootSequence();
  
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
    }
    
    // Flash notification timeout
    updateFlashNotification();
  }
}
