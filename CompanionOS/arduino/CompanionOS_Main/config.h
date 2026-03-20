#ifndef CONFIG_H
#define CONFIG_H

// ═══════════════════════════════════════════════════════════
// COMPANION OS - CONFIGURATION
// ═══════════════════════════════════════════════════════════

// The WiFi Manager handles credentials dynamically!
// If it fails to connect, it will create an Access Point called "CompanionOS-Setup".
// Connect to that AP to configure your WiFi network securely, eliminating placeholders.

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
#define ENABLE_GITHUB true
#define ENABLE_MIC true

// Display constants
#define SCREEN_W 240
#define SCREEN_H 320

// Colors
#define COLOR_BG 0x0000      // Black
#define COLOR_EYE 0x07FF     // Cyan
#define COLOR_PUPIL 0x0000   // Black  
#define COLOR_HIGHLIGHT 0xFFFF // White
#define COLOR_ACCENT 0x051D  // Deep Blue

#endif
