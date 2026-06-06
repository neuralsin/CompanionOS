#include <TFT_eSPI.h> // Hardware-specific library

TFT_eSPI tft = TFT_eSPI();       // Invoke custom library

void setup(void) {
  Serial.begin(115200);
  delay(1000);
  Serial.println(F("\n\n--- ILI9341 PHYSICAL DISPLAY TEST ---"));
  
  // Power up the display driver
  Serial.print(F("Initializing SPI Driver... "));
  tft.init();
  tft.setRotation(0); 
  Serial.println(F("Done."));
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
  tft.setTextSize(3);
  tft.setCursor(20, 100);
  tft.println("HARDWARE IS");
  tft.setCursor(20, 140);
  tft.println("WORKING!!!");
  
  delay(3000);
}
