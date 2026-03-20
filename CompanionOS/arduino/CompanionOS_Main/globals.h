#ifndef GLOBALS_H
#define GLOBALS_H

#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <WiFiUdp.h>
#include "config.h"

// ═══════════════════════════════════════════════════════════
// GLOBAL OBJECTS (instantiated here, extern'd elsewhere)
// ═══════════════════════════════════════════════════════════

TFT_eSPI tft = TFT_eSPI();
XPT2046_Touchscreen ts(TOUCH_CS);
WiFiUDP udp;

// System state
enum AppState {
  STATE_EYES,
  STATE_SPOTIFY,
  STATE_GITHUB,
  STATE_NOTES
};

extern AppState currentState;

#endif

