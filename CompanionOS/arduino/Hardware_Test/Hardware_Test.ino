#include <TFT_eSPI.h>

// ═════ PLATFORM DETECTION ═════
#ifdef ESP32
  // ESP32: Physical buttons, no touch
  #define BTN_LEFT   13
  #define BTN_SELECT 14
  #define BTN_RIGHT  27
  #define LDR_PIN    39
#else
  // ESP8266: XPT2046 touch, capacitive sensors
  #include <XPT2046_Touchscreen.h>
  #define TOUCH_CS 15      // D8
  #define TOUCH_LEFT 0     // D3 
  #define TOUCH_RIGHT 2    // D4 
  #define STRAND_MIC A0    // A0
#endif

// Driver Initialization
TFT_eSPI tft = TFT_eSPI(); 
#ifndef ESP32
  XPT2046_Touchscreen ts(TOUCH_CS);
#endif

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println(F("\n\n"));
  Serial.println(F("======================================="));
  Serial.println(F(" COMPANION OS - FULL HARDWARE AUDIT "));
  Serial.println(F("======================================="));

  #ifdef ESP32
    Serial.println(F("Platform: ESP32 + ST7735R 1.8\" TFT"));
    Serial.println(F("Input: Physical buttons (GPIO13/14/27)"));
    
    // Setup button pins
    pinMode(BTN_LEFT, INPUT_PULLUP);
    pinMode(BTN_SELECT, INPUT_PULLUP);
    pinMode(BTN_RIGHT, INPUT_PULLUP);
  #else
    Serial.println(F("Platform: ESP8266 + ILI9341 2.8\" TFT"));
    Serial.println(F("Input: XPT2046 touch + cap sensors"));
    
    // Setup Input Pins
    pinMode(TOUCH_LEFT, INPUT);
    pinMode(TOUCH_RIGHT, INPUT);
  #endif
  
  // Initialize Screen
  Serial.print(F("Initializing TFT Screen... "));
  tft.init();
  #ifdef ESP32
    tft.setRotation(1);  // Landscape 160×128
  #else
    tft.setRotation(0);  // Portrait 240×320
  #endif
  tft.fillScreen(TFT_BLACK);
  
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(10, 20);
  tft.println("HARDWARE DIAGNOSTIC");
  tft.setCursor(10, 40);
  #ifdef ESP32
    tft.println("ESP32 + ST7735R");
  #else
    tft.println("ESP8266 + ILI9341");
  #endif
  tft.setCursor(10, 60);
  tft.println("Running...");
  Serial.println(F("Done."));
  
  #ifndef ESP32
    // Initialize Touch (ESP8266 only)
    Serial.print(F("Initializing XPT2046 Touch... "));
    ts.begin();
    ts.setRotation(0);
    Serial.println(F("Done.\n"));
  #else
    Serial.println();
  #endif
  
  Serial.println(F("======================================="));
  Serial.println(F("WATCH THE LIVE SENSOR READINGS BELOW:"));
  Serial.println(F("=======================================\n"));
}

void loop() {
  #ifdef ESP32
    // ── ESP32: Read physical buttons + LDR ──
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint >= 200) {
      int leftVal  = digitalRead(BTN_LEFT);
      int selVal   = digitalRead(BTN_SELECT);
      int rightVal = digitalRead(BTN_RIGHT);
      int ldrVal   = analogRead(LDR_PIN);
      
      Serial.print(F("LEFT(13): "));
      Serial.print(leftVal == LOW ? "[PRESSED]" : "_off_   ");
      Serial.print(F("  |  SEL(14): "));
      Serial.print(selVal == LOW ? "[PRESSED]" : "_off_   ");
      Serial.print(F("  |  RIGHT(27): "));
      Serial.print(rightVal == LOW ? "[PRESSED]" : "_off_   ");
      Serial.print(F("  |  LDR(39): "));
      Serial.println(ldrVal);
      
      // Visual feedback on screen
      tft.fillRect(10, 80, 140, 40, TFT_BLACK);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.setCursor(10, 80);
      tft.print("L:"); tft.print(leftVal == LOW ? "ON " : "off");
      tft.print(" S:"); tft.print(selVal == LOW ? "ON " : "off");
      tft.print(" R:"); tft.print(rightVal == LOW ? "ON " : "off");
      tft.setCursor(10, 100);
      tft.print("LDR: "); tft.print(ldrVal);
      
      lastPrint = millis();
    }
  #else
    // ── ESP8266: Read touch + cap sensors ──
    if (ts.touched()) {
      TS_Point p = ts.getPoint();
      Serial.print(F("\n\n>>> SCREEN TOUCHED! (X:"));
      Serial.print(p.x);
      Serial.print(F(" Y:"));
      Serial.print(p.y);
      Serial.println(F(") <<<"));
      
      long mapX = map(p.x, 250, 3800, 240, 0);
      long mapY = map(p.y, 250, 3800, 320, 0);
      tft.fillCircle(mapX, mapY, 3, TFT_CYAN);
    }

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
  #endif
}
