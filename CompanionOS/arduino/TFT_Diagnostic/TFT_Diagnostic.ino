#include <TFT_eSPI.h>

void setup() {
  Serial.begin(115200);
  delay(2000); // Wait for Serial Monitor to open
  
  Serial.println(F("\n\n══════════════════════════════════════════"));
  Serial.println(F("    TFT_eSPI COMPILER PIN DIAGNOSTIC   "));
  Serial.println(F("══════════════════════════════════════════"));
  
  // Driver detection
  #ifdef ST7735_DRIVER
    Serial.println(F("Driver: ST7735 [ESP32 1.8\" TFT]"));
  #elif defined(ILI9341_DRIVER)
    Serial.println(F("Driver: ILI9341 [ESP8266 2.8\" TFT]"));
  #else
    Serial.println(F("Driver: UNKNOWN OR WRONG DRIVER"));
  #endif

  // Platform
  #ifdef ESP32
    Serial.println(F("Platform: ESP32"));
  #elif defined(ESP8266)
    Serial.println(F("Platform: ESP8266"));
  #else
    Serial.println(F("Platform: UNKNOWN"));
  #endif

  Serial.println(F("\n--- COMPILED PINS ---"));
  
  #ifdef TFT_MOSI
    Serial.print(F("MOSI: ")); Serial.println(TFT_MOSI);
  #endif
  #ifdef TFT_MISO
    Serial.print(F("MISO: ")); Serial.println(TFT_MISO);
  #endif
  #ifdef TFT_SCLK
    Serial.print(F("SCLK: ")); Serial.println(TFT_SCLK);
  #endif
  #ifdef TFT_CS
    Serial.print(F("TFT_CS: ")); Serial.println(TFT_CS);
  #endif
  #ifdef TFT_DC
    Serial.print(F("TFT_DC: ")); Serial.println(TFT_DC);
  #endif
  #ifdef TFT_RST
    Serial.print(F("TFT_RST: ")); Serial.println(TFT_RST);
  #endif
  #ifdef TOUCH_CS
    Serial.print(F("TOUCH_CS: ")); Serial.println(TOUCH_CS);
  #else
    Serial.println(F("TOUCH_CS: Not defined (no touch)"));
  #endif
  
  // Screen dimensions
  Serial.println(F("\n--- SCREEN CONFIG ---"));
  #ifdef TFT_WIDTH
    Serial.print(F("TFT_WIDTH: ")); Serial.println(TFT_WIDTH);
  #endif
  #ifdef TFT_HEIGHT
    Serial.print(F("TFT_HEIGHT: ")); Serial.println(TFT_HEIGHT);
  #endif
  
  Serial.println(F("══════════════════════════════════════════"));
}

void loop() {
  // Do nothing
}
