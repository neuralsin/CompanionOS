#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

// ═════ PIN DEFINITIONS ═════
#define TOUCH_CS 15      // D8
#define TOUCH_LEFT 0     // D3 
#define TOUCH_RIGHT 2    // D4 
#define STRAND_MIC A0    // A0

// Driver Initialization
TFT_eSPI tft = TFT_eSPI(); 
XPT2046_Touchscreen ts(TOUCH_CS);

void setup() {
  Serial.begin(115200);
  delay(1000); // Give serial monitor time to open
  
  Serial.println(F("\n\n"));
  Serial.println(F("======================================="));
  Serial.println(F(" COMPANION OS - FULL HARDWARE AUDIT "));
  Serial.println(F("======================================="));

  // 1. Setup Input Pins
  pinMode(TOUCH_LEFT, INPUT);
  pinMode(TOUCH_RIGHT, INPUT);
  
  // 2. Initialize Screen
  Serial.print(F("Initializing TFT Screen... "));
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  
  // Draw basic test text on screen
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(10, 50);
  tft.println("HARDWARE DIAGNOSTIC");
  tft.setCursor(10, 80);
  tft.println("Running...");
  Serial.println(F("Done."));
  
  // 3. Initialize Touch
  Serial.print(F("Initializing XPT2046 Touch... "));
  ts.begin();
  ts.setRotation(0);
  Serial.println(F("Done.\n"));
  
  Serial.println(F("======================================="));
  Serial.println(F("WATCH THE LIVE SENSOR READINGS BELOW:"));
  Serial.println(F("═══════════════════════════════════════\n"));
}

void loop() {
  // Check the physical TFT touchscreen mapping at maximum unthrottled loop speed
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    Serial.print(F("\n\n>>> SCREEN TOUCHED! (X:"));
    Serial.print(p.x);
    Serial.print(F(" Y:"));
    Serial.print(p.y);
    Serial.println(F(") <<<"));
    
    // Draw dot on screen wherever they push (Inverted mapping applied!)
    long mapX = map(p.x, 250, 3800, 240, 0);
    long mapY = map(p.y, 250, 3800, 320, 0);
    tft.fillCircle(mapX, mapY, 3, TFT_CYAN);
  }

  // Print raw sensor info only 5 times a second to prevent terminal flood
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint >= 200) {
    int leftVal  = digitalRead(TOUCH_LEFT);
    int rightVal = digitalRead(TOUCH_RIGHT);
    int micVal   = analogRead(STRAND_MIC);
    
    Serial.print(F("LEFT Sensor (D3): ")); 
    Serial.print(leftVal == HIGH ? "[TOUCHED!]" : "_off_   ");
    Serial.print(F("  |  RIGHT Sensor (D4): ")); 
    Serial.print(rightVal == HIGH ? "[TOUCHED!]" : "_off_   ");
    Serial.print(F("  |  MIC Audio (A0): ")); 
    Serial.println(micVal);
    
    lastPrint = millis();
  }
}
