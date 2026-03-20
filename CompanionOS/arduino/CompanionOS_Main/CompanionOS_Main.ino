/*
 * ═══════════════════════════════════════════════════════════
 *   COMPANION OS - Full Production Version
 *   
 *   Working system with:
 *   - Modular Architecture 
 *   - WiFiManager AutoConnect AP (No hardcoded credentials)
 *   - UDP connection dynamically tracks the Python PC
 * ═══════════════════════════════════════════════════════════
 */

#include "globals.h"
#include "network.h"
#include "eyes.h"
#include "pages.h"
#include "touch.h"
#include "ui.h"

// Define uninitialized globals
AppState currentState = STATE_EYES;
Emotion currentEmotion = EMO_HAPPY;
unsigned long lastBlink = 0;
bool isBlinking = false;
int blinkPhase = 0;
char udpBuffer[512];

void setup() {
  Serial.begin(115200);
  Serial.println(F("\n\n╔════════════════════════════════════════╗"));
  Serial.println(F("║   COMPANION OS - Production v2.0       ║"));
  Serial.println(F("╚════════════════════════════════════════╝\n"));
  
  Serial.print(F("Display... "));
  tft.init();
  tft.setRotation(0);  
  tft.fillScreen(COLOR_BG);
  Serial.println(F("OK"));
  
  Serial.print(F("Touch... "));
  ts.begin();
  ts.setRotation(0);
  Serial.println(F("OK"));
  
  pinMode(TOUCH_LEFT, INPUT);
  pinMode(TOUCH_RIGHT, INPUT);
  pinMode(MIC_PIN, INPUT);
  Serial.println(F("Sensors... OK"));
  
  // Connect WiFi automatically using captive portal without exposing hard-coded info!
  setupWiFi();
  
  renderCurrentPage();
  Serial.println(F("\n✓ Ready!\n"));
}

void loop() {
  handleNetwork();
  handleTouch();
  if (currentState == STATE_EYES) {
    updateEyes();
  }
  delay(50); 
}
