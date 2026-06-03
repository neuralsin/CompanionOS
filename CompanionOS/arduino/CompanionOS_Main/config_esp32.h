#ifndef CONFIG_ESP32_H
#define CONFIG_ESP32_H

// ═══════════════════════════════════════════════════════════
// COMPANION OS — ESP32 38-PIN DEVKIT V1 HARDWARE CONFIG
// Display: ST7735R 1.8" 160×128px LANDSCAPE (NO touchscreen)
// ═══════════════════════════════════════════════════════════

// ┌──────────────────────────────────────────────────────────┐
// │ ⚠️  TFT_eSPI User_Setup.h MUST BE CONFIGURED MANUALLY  │
// │                                                          │
// │ Open: Arduino/libraries/TFT_eSPI/User_Setup.h           │
// │ Make the following changes:                              │
// │                                                          │
// │   1. Comment out:                                        │
// │      // #define ILI9341_DRIVER                           │
// │                                                          │
// │   2. Uncomment / add:                                    │
// │      #define ST7735_DRIVER                               │
// │      #define TFT_WIDTH  128                              │
// │      #define TFT_HEIGHT 160                              │
// │      #define ST7735_GREENTAB3  // See note below         │
// │      // #define CGRAM_OFFSET   // Try if colors wrong    │
// │                                                          │
// │   3. Set SPI pins:                                       │
// │      #define TFT_MOSI  23                                │
// │      #define TFT_SCLK  18                                │
// │      #define TFT_CS     5                                │
// │      #define TFT_DC     2                                │
// │      #define TFT_RST    4                                │
// │                                                          │
// │ NOTE: ST7735R modules vary. If colors are wrong or       │
// │ display is offset, try changing ST7735_GREENTAB3 to:     │
// │   ST7735_BLACKTAB    — black PCB back label              │
// │   ST7735_GREENTAB    — green tab on flex cable           │
// │   ST7735_GREENTAB2   — green tab, 2nd revision          │
// │   ST7735_REDTAB      — red tab on flex cable             │
// │ Also try toggling CGRAM_OFFSET on/off.                   │
// │ Check the back of your specific module's PCB for clues.  │
// └──────────────────────────────────────────────────────────┘

// === ESP32 ST7735R SPI Wiring ===
#define TFT_CS     5     // GPIO5
#define TFT_DC     2     // GPIO2
#define TFT_RST    4     // GPIO4
#define TFT_MOSI  23     // GPIO23 (VSPI MOSI)
#define TFT_SCLK  18     // GPIO18 (VSPI SCK)
#define TFT_MISO  19     // GPIO19 (VSPI MISO) — not needed for write-only
#define TFT_BL    15     // GPIO15 PWM backlight (optional)

// NO XPT2046 touchscreen on ESP32 variant
// Navigation done via physical buttons

// ═══════════════════════════════════════════════════════════
// Physical Navigation Buttons
// ═══════════════════════════════════════════════════════════
// 🔴 BUG-01 FIX: GPIO34/35/36 are INPUT-ONLY on ESP32
// (no internal pullup/pulldown). Those pins would float and
// generate random triggers. Using bidirectional GPIO pins
// that support INPUT_PULLUP:
#define BTN_LEFT    13   // GPIO13 — prev page / back
#define BTN_RIGHT   14   // GPIO14 — next page / enter
#define BTN_SELECT  27   // GPIO27 — select / confirm
// All three support INPUT_PULLUP. Wire buttons to GND.

// LDR ambient light sensor
#define LDR_PIN     39   // GPIO39 ADC1 (input-only OK for analog read)

// ═══════════════════════════════════════════════════════════
// Screen Dimensions — LANDSCAPE orientation (rotation=1)
// 🔴 BUG-03 FIX: Landscape matches original ESP8266 aspect
// ratio (wider than tall). SCALE_X/SCALE_Y normalize cleanly:
//   SCALE_X(320) = 160*320/320 = 160 ✓
//   SCALE_Y(240) = 128*240/240 = 128 ✓
// ═══════════════════════════════════════════════════════════
#define SCREEN_W   160
#define SCREEN_H   128

// ═══════════════════════════════════════════════════════════
// DR. HACK PERIPHERAL PINS (ESP32 only)
// nRF24L01, CC1101 sub-GHz, M5Stack IR Unit
// ═══════════════════════════════════════════════════════════

// Shared SPI bus (same as TFT): SCK=18, MOSI=23, MISO=19
// Each module uses its own CS/CSN pin.

// nRF24L01 #1 — 2.4GHz radio (primary)
// NOTE: Original firmware used GPIO27/GPIO14 but those are
// CompanionOS buttons. Reassigned to GPIO32/GPIO33.
#define NRF1_CE_PIN   32
#define NRF1_CSN_PIN  33

// nRF24L01 #2 — 2.4GHz radio (secondary, for jammer)
#define NRF2_CE_PIN   17
#define NRF2_CSN_PIN  16

// M5Stack IR Unit
// GPIO26 = TX output from ESP32 to IR LED
// GPIO34 = RX input from IR receiver (input-only pin, correct)
#define IR_TX_PIN     26
#define IR_RX_PIN     34

// CC1101 sub-GHz radio (315/433/868/915 MHz)
#define CC1101_CSN_PIN     21
#define CC1101_GDO0_PIN    35   // RX data / edges (input-only, correct)
#define CC1101_TX_DATA_PIN 25   // Optional jumper for Lab Replay OOK TX

#endif
