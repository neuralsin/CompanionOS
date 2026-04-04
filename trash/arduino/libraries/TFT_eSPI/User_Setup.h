/**
 * TFT_eSPI — User_Setup.h
 * ========================
 * SPOTIFY DESK COMPANION - Complete Pin Configuration
 * ESP8266 NodeMCU + ILI9341 TFT + XPT2046 Touch
 *
 * HOW TO INSTALL:
 *   1. Find your TFT_eSPI library folder:
 *      - Windows: Documents/Arduino/libraries/TFT_eSPI/
 *      - macOS  : ~/Documents/Arduino/libraries/TFT_eSPI/
 *      - Linux  : ~/Arduino/libraries/TFT_eSPI/
 *   2. BACKUP the original User_Setup.h
 *   3. REPLACE with this file
 *   4. Do NOT edit User_Setup_Select.h
 *
 * ═══════════════════════════════════════════════════════════════════════
 * COMPLETE WIRING DIAGRAM
 * ═══════════════════════════════════════════════════════════════════════
 *
 * TFT DISPLAY (ILI9341):
 *   VCC  → 3.3V
 *   GND  → GND
 *   SCK  → D5  (GPIO14) - SPI Clock
 *   MOSI → D7  (GPIO13) - SPI Data Out
 *   MISO → D6  (GPIO12) - SPI Data In
 *   CS   → D2  (GPIO4)  - TFT Chip Select
 *   DC   → D1  (GPIO5)  - Data/Command ⚠️ NOT D3!
 *   RST  → D0  (GPIO16) - Reset ⚠️ NOT D4!
 *   LED  → 3.3V         - Backlight always on
 *
 * TOUCH CONTROLLER (XPT2046):
 *   T_CLK → D5  (GPIO14) - Shared with TFT
 *   T_DIN → D7  (GPIO13) - Shared with TFT
 *   T_DO  → D6  (GPIO12) - Shared with TFT
 *   T_CS  → D8  (GPIO15) - Touch Chip Select (DEDICATED)
 *   T_IRQ → Not connected (optional)
 *   VCC   → 3.3V
 *   GND   → GND
 *
 * CAPACITIVE TOUCH SENSORS (2x):
 *   Left Sensor  I/O → D3 (GPIO0)
 *   Right Sensor I/O → D4 (GPIO2)
 *   Both VCC → 3.3V, GND → GND
 *
 * MICROPHONE (If Analog):
 *   OUT → A0 (ADC)
 *   VCC → 3.3V or 5V (check module)
 *   GND → GND
 *
 * DISTANCE SENSOR (HC-SR04) - To be added:
 *   TRIG → (User will specify)
 *   ECHO → (User will specify)
 *
 * ═══════════════════════════════════════════════════════════════════════
 */

// ─── Driver ────────────────────────────────────────────────────────────
#define ILI9341_DRIVER      // 2.4" 240×320 TFT

// ─── TFT Pin Definitions ───────────────────────────────────────────────
#define TFT_MISO 12         // D6  (GPIO12)
#define TFT_MOSI 13         // D7  (GPIO13)
#define TFT_SCLK 14         // D5  (GPIO14)
#define TFT_CS   4          // D2  (GPIO4)  - TFT Chip Select
#define TFT_DC   5          // D1  (GPIO5)  - Data/Command
#define TFT_RST  16         // D0  (GPIO16) - Reset

// ─── Touch Controller Pin (XPT2046) ────────────────────────────────────
#define TOUCH_CS 15         // D8  (GPIO15) - Touch Chip Select
// Note: T_CLK, T_DIN, T_DO share TFT SPI pins above

// ─── Display Dimensions ────────────────────────────────────────────────
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// ─── SPI Clock Speed ───────────────────────────────────────────────────
// 40 MHz is the maximum supported by ILI9341. Do NOT go higher.
// If you see display corruption, step down to 27000000.
#define SPI_FREQUENCY       40000000    // 40 MHz (maximum)
#define SPI_READ_FREQUENCY  20000000    // 20 MHz for reading
#define SPI_TOUCH_FREQUENCY  2500000    // 2.5 MHz for XPT2046 touch

// ─── Colour Depth & Order ──────────────────────────────────────────────
// 16-bit RGB565 format
#define TFT_RGB_ORDER TFT_BGR          // Changed to BGR to fix inverted color bug

// ─── Font Support ──────────────────────────────────────────────────────
// Load fonts needed for the companion interface
#define LOAD_GLCD   // Font 1. Original Adafruit 8 pixel font
#define LOAD_FONT2  // Font 2. Small 16 pixel high font
#define LOAD_FONT4  // Font 4. Medium 26 pixel high font
#define LOAD_FONT6  // Font 6. Large 48 pixel font
#define LOAD_FONT7  // Font 7. 7 segment 48 pixel font
#define LOAD_FONT8  // Font 8. Large 75 pixel font
#define LOAD_GFXFF  // FreeFonts - include the GFX Free Font

#define SMOOTH_FONT // Enable anti-aliased fonts

// ─── Optional Features ─────────────────────────────────────────────────
#define SUPPORT_TRANSACTIONS  // Enable SPI transaction support

// Uncomment if display colors are inverted
// #define TFT_INVERSION_ON
// #define TFT_INVERSION_OFF

// ─── Performance Optimizations ─────────────────────────────────────────
// Use hardware SPI port for better performance
#define USE_HSPI_PORT

// Allow SPI overlap with WiFi (advanced - comment out if glitches occur)
// #define TFT_SPI_OVERLAP

// ═══════════════════════════════════════════════════════════════════════
// END OF USER SETUP
// ═══════════════════════════════════════════════════════════════════════
