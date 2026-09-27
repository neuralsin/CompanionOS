// ═══════════════════════════════════════════════════════════
// COMPANION OS v7.0 — CONFIGURATION (Smart Hardware Router)
// Auto-detects ESP32 vs ESP8266 and includes correct config.
// ═══════════════════════════════════════════════════════════
#ifndef CONFIG_H
#define CONFIG_H

// [LEGACY - v6.0] Original config.h contents:
// Hardcoded for ESP8266 only with TOUCH_LEFT/TOUCH_RIGHT,
// 320x240 ILI9341, no platform detection.
// Now replaced with auto-routing below.

// ═══════════════════════════════════════════════════════════
// HARDWARE AUTO-DETECTION ROUTER
// ═══════════════════════════════════════════════════════════

#ifdef ESP32
  #include "config_esp32.h"
  #define PLATFORM_NAME "ESP32"
  #define HAS_TOUCH false
  #define HAS_BLUETOOTH true
  #define DISPLAY_DRIVER_ST7735R true
#elif defined(ESP8266)
  #include "config_esp8266.h"
  #define PLATFORM_NAME "ESP8266"
  #define HAS_TOUCH true
  #define HAS_BLUETOOTH false
  #define DISPLAY_DRIVER_ILI9341 true
#else
  #error "Unknown hardware platform. Define ESP32 or ESP8266."
#endif

// ═══════════════════════════════════════════════════════════
// SHARED CONFIG — Applies to both platforms
// ═══════════════════════════════════════════════════════════

// WiFi Credentials
const char* WIFI_SSID = "Stuart";
const char* WIFI_PASS = "123456789";

// Priority 2 WiFi Network
const char* WIFI_SSID2 = "Pierre Fauchard 2.4GHz";
const char* WIFI_PASS2 = "Jenkins_Madarchod";

// Network Settings
const char* DEFAULT_PC_IP = "192.168.1.100";
const int UDP_PORT_RX = 8888;
const int UDP_PORT_TX = 8889;

// Button Active Logic Defaults (if not specified by platform config)
#ifndef BTN_ACTIVE_LEVEL
#define BTN_ACTIVE_LEVEL      HIGH
#endif
#ifndef BTN_UNPRESSED_LEVEL
#define BTN_UNPRESSED_LEVEL   LOW
#endif
#ifndef BTN_PIN_MODE
#define BTN_PIN_MODE          INPUT
#endif

// NTP
#define NTP_SERVER       "pool.ntp.org"
#define NTP_OFFSET       19800   // IST UTC+5:30

// Feature Toggles
#define ENABLE_EYES true
#define ENABLE_SPOTIFY true
#define ENABLE_GALLERY true
#define ENABLE_MIC true

// ═══════════════════════════════════════════════════════════
// STANDALONE DIRECT APIS (Used when host device is absent)
// ═══════════════════════════════════════════════════════════

// Spotify Web API Credentials
#define SPOTIFY_CLIENT_ID       "450a2e6ab1ed4ce4ac9738f654814240"
#define SPOTIFY_CLIENT_SECRET   "78200536e7804c8ca427102a6ba2138f"
// Hardcoded token fallback from Python script (.cache)
#define DEFAULT_SPOTIFY_REFRESH_TOKEN "AQCDzzl6TnG-FpdJKuWddUHoX_wTx2jA1mMHJR3okGYjllb_DnifVtaplec0Do9EJWTZippa7R5DCejRbR_eJgexej9BEmsFbCdE3o6zwgXCs4tXHOth3eOT6NksbNi1lSs"
#define DEFAULT_SPOTIFY_ACCESS_TOKEN  "BQCNa_jBDQmPUroD1WVTgAyt9Eq60wJ7tXGqgElBSDFZywXo11mKCBQ7NcpsK8QT4jIJzlQNPjMi8Lgno601-FU_9bD1o6KMa9REzSVxqqXtLMSoiYNKju3jBPH-5vg-sC8vhTakaClrVipZjpzT6d0cw4mGvqid7LFqOH_rfJwL-2K7f99CQvYL5dKUhcM85adLeBXLdCjhng8A27oI0PyC0VsUVa2x9LEKRkZguylaiOKqqwGFq4tG-oU2gQxmrU7RJOhNHVJh8TdkDF2tZUCwxVdQ7w"
#define SPOTIFY_POLL_INTERVAL_ACTIVE_MS   3500UL  // 3.5s when on Spotify page / playing
#define SPOTIFY_POLL_INTERVAL_IDLE_MS     10000UL // 10s when idle on another page

// Standalone Weather (Open-Meteo API - No key required)
#define WEATHER_CITY_NAME           "Delhi"
#define WEATHER_LATITUDE            28.6139
#define WEATHER_LONGITUDE           77.2090
#define WEATHER_POLL_INTERVAL_MS    (15UL * 60UL * 1000UL) // 15 mins

// Host Device Silence Threshold (determines when host is disconnected)
#define HOST_TIMEOUT_MS             20000UL // 20 seconds silence -> host absent

// EEPROM addresses
#define EEPROM_EXOTIC_ADDR 0        // 1 byte: legacy
#define EEPROM_THEME_ADDR 1         // 1 byte: unused (themes removed)
#define EEPROM_ACTIVE_THEME_ADDR 2  // 1 byte: 0 = Theme 1, 1 = Theme 2, 2 = Theme 3 (RoboEyes)
#define EEPROM_WIFI_MAGIC_ADDR 10   // 2 bytes: 0xC0 0x55
#define EEPROM_WIFI_SSID_ADDR  16   // 32 bytes (null terminated)
#define EEPROM_WIFI_PASS_ADDR  48   // 64 bytes (null terminated)
#define EEPROM_SIZE 512             // Total EEPROM bytes used (for up to 4 multi-wifi profiles)

// ═══════════════════════════════════════════════════════════
// V7 COLOR SYSTEM — Layered Design Tokens
// ═══════════════════════════════════════════════════════════

// Primary palette (dark theme)
#define CLR_BG         0x0841   // near-black with blue tint
#define CLR_SURFACE    0x1082   // card/panel background
#define CLR_SURFACE_2  0x18A3   // elevated card
#define CLR_BORDER     0x2104   // subtle borders
#define CLR_PRIMARY    0x05FF   // cyan accent (brand)
#define CLR_SECONDARY  0xF800   // red (danger/Dr.Hack)
#define CLR_DANGER     0xF800   // red (danger alert)
#define CLR_SUCCESS    0x07E0   // green
#define CLR_WARNING    0xFFE0   // yellow
#define CLR_TEXT_HI    0xFFFF   // white (primary text)
#define CLR_TEXT_MED   0xC618   // light gray (secondary)
#define CLR_TEXT_LO    0x8410   // dark gray (hint text)

// Accent gradient presets (for cards)
#define CLR_GRAD_BLUE_TOP    0x041F  // deep blue
#define CLR_GRAD_BLUE_BOT    0x001F  // midnight blue
#define CLR_GRAD_PURPLE_TOP  0x600F
#define CLR_GRAD_PURPLE_BOT  0x300A

// Legacy color aliases (backward compatibility with v6 code)
#define COLOR_BG        CLR_BG
#define COLOR_EYE       CLR_PRIMARY
#define COLOR_PUPIL     0x0000   // Black
#define COLOR_HIGHLIGHT CLR_TEXT_HI
#define COLOR_ACCENT    0x051D   // Deep Blue (preserved from v6)

#endif
