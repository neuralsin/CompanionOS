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

#include "network.h"
#include "globals.h"
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

void runBootSequence() {
  tft.fillScreen(COLOR_BG);
  
  // Central Logo
  tft.setTextColor(COLOR_EYE);
  tft.drawCentreString("CompanionOS", SCREEN_W/2, 40, 4);
  tft.setTextColor(TFT_LIGHTGREY);
  tft.drawCentreString("by neuralsin", SCREEN_W/2, 65, 2);

  delay(600);

  // Real-time hardware diagnostic payloads
  struct BootTask {
    String name;
    String value;
  };

  BootTask tasks[6] = {
    {"CPU Core:", String(ESP.getCpuFreqMHz()) + " MHz [OK]"},
    {"RAM Integrity:", String(ESP.getFreeHeap() / 1024) + " KB Free"},
    {"STORAGE Array:", String(ESP.getFlashChipRealSize() / 1024) + " KB [OK]"},
    {"NETWORK Sync:", WiFi.localIP().toString()},
    {"THERMAL Mgmt:", "ACTIVE"},
    {"UI Matrix:", "320x240 [LNDSCP]"}
  };

  tft.setTextFont(2);
  for (int i = 0; i < 6; i++) {
    int barY = 100 + (i * 20);
    
    // Task Name
    tft.setTextColor(TFT_WHITE);
    tft.drawString(tasks[i].name, 10, barY);
    
    // Hollow Progress Bar
    tft.drawRect(120, barY + 2, 80, 10, TFT_DARKGREY);
    
    // Animate Bar Fill
    for (int p = 0; p <= 76; p += 19) {
      tft.fillRect(122, barY + 4, p, 6, COLOR_EYE);
      delay(40); // Fast sci-fi stutter step
    }
    
    // Final Output Value
    tft.setTextColor(TFT_GREEN);
    tft.drawString(tasks[i].value, 210, barY);
    delay(150);
  }

  // Final glow sequence
  delay(1000);
  tft.fillScreen(COLOR_BG);
}

void setup() {
  Serial.begin(115200);
  Serial.println(F("\n\n╔════════════════════════════════════════╗"));
  Serial.println(F("║   COMPANION OS - Production v2.0       ║"));
  Serial.println(F("╚════════════════════════════════════════╝\n"));
  
  Serial.print(F("Display... "));
  tft.init();
  tft.setRotation(1);  // LANDSCAPE
  tft.fillScreen(COLOR_BG);
  Serial.println(F("OK"));
  
  Serial.print(F("Touch... "));
  ts.begin();
  ts.setRotation(1);   // LANDSCAPE
  Serial.println(F("OK"));
  
  pinMode(TOUCH_LEFT, INPUT);
  pinMode(TOUCH_RIGHT, INPUT);
  pinMode(MIC_PIN, INPUT);
  Serial.println(F("Sensors... OK"));
  
  // Connect WiFi automatically using captive portal without exposing hard-coded info!
  setupWiFi();
  
  // Fire sci-fi boot sequence post-networking
  runBootSequence(); 
  
  renderCurrentPage();
  Serial.println(F("\n✓ Ready!\n"));
}

void loop() {
  // Unthrottled IO for precise touch and zero-latency UDP
  handleNetwork();
  handleTouch();
  
  // Non-blocking UI Frame Timer (~20 FPS)
  static unsigned long lastFrameTime = 0;
  if (millis() - lastFrameTime >= 50) {
    lastFrameTime = millis();
    if (currentState == STATE_EYES) {
      updateEyes();
    } else if (currentState == STATE_VISUALIZER) {
      updateVisualizer();
    }
  }
}
