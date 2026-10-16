#ifndef CONFIG_ESP8266_H
#define CONFIG_ESP8266_H

// ═══════════════════════════════════════════════════════════
// COMPANION OS — ESP8266 NODEMCU HARDWARE CONFIG
// Display: ILI9341 2.8" 320×240px with XPT2046 touch
// ═══════════════════════════════════════════════════════════

// === ESP8266 ILI9341 2.8" 320x240 with XPT2046 touch ===
// TFT SPI: MISO=D6, MOSI=D7, SCK=D5, CS=D2, DC=D1, RST=D0
#define TOUCH_CS     15  // D8 / GPIO15

// [LEGACY - v6.0] TOUCH_LEFT and TOUCH_RIGHT REMOVED
// These capacitive sensor pins conflicted with ESP8266 boot mode.
// GPIO0 (D3) and GPIO2 (D4) are now freed.
// #define TOUCH_LEFT  0   // D3 - REMOVED in v7
// #define TOUCH_RIGHT 2   // D4 - REMOVED in v7

// RESERVED — available for future expansion
// GPIO0 (D3): was TOUCH_LEFT — free
// GPIO2 (D4): was TOUCH_RIGHT — free

// Microphone / LDR (shared analog pin)
#define MIC_PIN      A0

// Screen dimensions for ILI9341
#define SCREEN_W    320
#define SCREEN_H    240

#endif
