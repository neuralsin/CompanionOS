#include <TFT_eSPI.h> // Hardware-specific library

TFT_eSPI tft = TFT_eSPI();       // Invoke custom library

void setup(void) {
  Serial.begin(115200);
  delay(1000);
  
  #ifdef ESP32
    Serial.println(F("\n\n--- ST7735R 1.8\" TFT DISPLAY TEST (ESP32) ---"));
  #else
    Serial.println(F("\n\n--- ILI9341 2.8\" TFT DISPLAY TEST (ESP8266) ---"));
  #endif
  
  // Power up the display driver
  Serial.print(F("Initializing SPI Driver... "));
  tft.init();
  
  #ifdef ESP32
    tft.setRotation(1);  // ST7735R landscape: 160×128
  #else
    tft.setRotation(0);  // ILI9341 portrait: 240×320
  #endif
  
  Serial.println(F("Done."));
  Serial.print(F("Screen size: "));
  Serial.print(tft.width());
  Serial.print(F("x"));
  Serial.println(tft.height());
}

void loop() {
  Serial.println(F("Drawing RED screen."));
  tft.fillScreen(TFT_RED);
  delay(1500);

  Serial.println(F("Drawing GREEN screen."));
  tft.fillScreen(TFT_GREEN);
  delay(1500);

  Serial.println(F("Drawing BLUE screen."));
  tft.fillScreen(TFT_BLUE);
  delay(1500);

  Serial.println(F("Drawing TEXT test."));
  tft.fillScreen(TFT_BLACK);
  
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  #ifdef ESP32
    tft.setTextSize(1);
    tft.setCursor(10, 40);
    tft.println("HARDWARE IS");
    tft.setCursor(10, 60);
    tft.println("WORKING!!!");
    tft.setCursor(10, 90);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.println("ESP32 + ST7735R");
  #else
    tft.setTextSize(3);
    tft.setCursor(20, 100);
    tft.println("HARDWARE IS");
    tft.setCursor(20, 140);
    tft.println("WORKING!!!");
  #endif
  
  delay(3000);
}
