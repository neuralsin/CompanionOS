#ifndef CONFIG_H
#define CONFIG_H

// ═══════════════════════════════════════════════════════════
// COMPANION OS - CONFIGURATION
// ═══════════════════════════════════════════════════════════

// The WiFi Manager handles credentials dynamically!
// If it fails to connect, it will create an Access Point called "CompanionOS-Setup".
// Connect to that AP to configure your WiFi network securely, eliminating placeholders.

// Hardcoded WiFi Credentials
const char* WIFI_SSID = "stuart";
const char* WIFI_PASS = "12345678";

// Network Settings
// Broadcast UDP will be used initially, or you can supply the PC IP directly if stable.
const char* DEFAULT_PC_IP = "192.168.1.100";  
const int UDP_PORT_RX = 8888;
const int UDP_PORT_TX = 8889;

// Pin Definitions (match your wiring)
#define TOUCH_CS 15      // D8 - Touch screen chip select
#define TOUCH_LEFT 0     // D3 - Left capacitive sensor
#define TOUCH_RIGHT 2    // D4 - Right capacitive sensor
#define MIC_PIN A0       // A0 - Microphone

// Feature Toggles
#define ENABLE_EYES true
#define ENABLE_SPOTIFY true
#define ENABLE_DIRECT_SPOTIFY true
#define ENABLE_GALLERY true
#define ENABLE_MIC true

// Direct Spotify Credentials (standalone mode - no PC companion needed)
const char* SPOTIFY_CLIENT_ID = "450a2e6ab1ed4ce4ac9738f654814240";
const char* SPOTIFY_CLIENT_SECRET = "78200536e7804c8ca427102a6ba2138f";
const char* SPOTIFY_REFRESH_TOKEN = "AQB8r90xaOmCyjq7yMJhe7ir6KYXGPGCGsh8W3gqo87ijaDZIbline98_67zEJQnf6DOiLCEhKBZszkmORPUSleKB95o1QLfME-o-6DjBJkx9zFRm39nyMG8Hia2ZPQY5EA";

// EEPROM addresses
#define EEPROM_EXOTIC_ADDR 0    // 1 byte: legacy
#define EEPROM_THEME_ADDR 1     // 1 byte: unused (themes removed)
#define EEPROM_ACTIVE_THEME_ADDR 2  // 1 byte: 0 = Theme 1, 1 = Theme 2
#define EEPROM_SIZE 8           // Total EEPROM bytes used

// Display constants
#define SCREEN_W 320
#define SCREEN_H 240

// Colors
#define COLOR_BG 0x0000      // Black
#define COLOR_EYE 0x07FF     // Cyan
#define COLOR_PUPIL 0x0000   // Black  
#define COLOR_HIGHLIGHT 0xFFFF // White
#define COLOR_ACCENT 0x051D  // Deep Blue

#endif
