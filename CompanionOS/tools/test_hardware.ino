// Quick pin verification sketch
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);
  Serial.println("Pin configuration:");
  Serial.print("TFT_MISO: "); Serial.println(TFT_MISO);
  Serial.print("TFT_MOSI: "); Serial.println(TFT_MOSI);
  Serial.print("TFT_SCLK: "); Serial.println(TFT_SCLK);
  Serial.print("TFT_CS:   "); Serial.println(TFT_CS);
  Serial.print("TFT_DC:   "); Serial.println(TFT_DC);
  Serial.print("TFT_RST:  "); Serial.println(TFT_RST);
  
  Serial.println("\nInitializing display...");
  tft.init();
  tft.fillScreen(TFT_BLUE);
  Serial.println("Display initialized!");
}

void loop() {
  // Nothing
}
