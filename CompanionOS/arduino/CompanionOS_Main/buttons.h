// ═══════════════════════════════════════════════════════════
// COMPANION OS v7.0 — PHYSICAL BUTTON HANDLER (ESP32 only)
// Replaces touchscreen input for ST7735R 160×128 display.
// 3-button navigation: LEFT, RIGHT, SELECT
// 🔴 BUG-01 FIX: Using GPIO13/14/27 (pullup-capable).
// ═══════════════════════════════════════════════════════════
#ifndef BUTTONS_H
#define BUTTONS_H

#ifdef ESP32

#include "globals.h"

extern void redrawSettingsPartial();

// Timing
#define BTN_DEBOUNCE_MS     50
#define BTN_LONG_PRESS_MS   800
#define BTN_XLONG_PRESS_MS  1500

// ── Button State Tracking ───────────────────────────────

struct ButtonState {
  uint8_t pin;
  bool pressed;           // currently held
  bool wasPressed;        // edge-detected single press (consumed after read)
  bool longPressed;       // edge-detected long press (consumed after read)
  bool xlongPressed;      // edge-detected extra-long press
  unsigned long pressedAt;
  bool longFired;         // long press already reported this hold
  bool xlongFired;        // xlong press already reported this hold
};

ButtonState btnLeft   = {BTN_LEFT,   false, false, false, false, 0, false, false};
ButtonState btnRight  = {BTN_RIGHT,  false, false, false, false, 0, false, false};
ButtonState btnSelect = {BTN_SELECT, false, false, false, false, 0, false, false};

// Forward declarations
extern void changePage(int delta);
extern void renderCurrentPage();
extern void setEmotion(Emotion e);
extern void t2_nextExpression();
extern void t2_setEmotion(Emotion e);
extern void t3_nextExpression();
extern void t3_setEmotion(Emotion e);

// ── Init ─────────────────────────────────────────────────

void initButtons() {
  pinMode(BTN_LEFT,   BTN_PIN_MODE);
  pinMode(BTN_RIGHT,  BTN_PIN_MODE);
  pinMode(BTN_SELECT, BTN_PIN_MODE);
}

// ── Core State Machine ──────────────────────────────────

static void updateButtonState(ButtonState& btn) {
  bool raw = (digitalRead(btn.pin) == BTN_ACTIVE_LEVEL);  // Configured active state
  unsigned long now = millis();

  if (raw && !btn.pressed) {
    // Button just pressed — start tracking
    if (now - btn.pressedAt > BTN_DEBOUNCE_MS) {
      btn.pressed = true;
      btn.pressedAt = now;
      btn.longFired = false;
      btn.xlongFired = false;
    }
  }
  else if (raw && btn.pressed) {
    // Button held — check for long press thresholds
    unsigned long held = now - btn.pressedAt;
    if (held >= BTN_XLONG_PRESS_MS && !btn.xlongFired) {
      btn.xlongPressed = true;
      btn.xlongFired = true;
    }
    else if (held >= BTN_LONG_PRESS_MS && !btn.longFired) {
      btn.longPressed = true;
      btn.longFired = true;
    }
  }
  else if (!raw && btn.pressed) {
    // Button released
    unsigned long held = now - btn.pressedAt;
    if (!btn.longFired && !btn.xlongFired && held >= BTN_DEBOUNCE_MS) {
      // Short press — only if long/xlong didn't fire
      btn.wasPressed = true;
    }
    btn.pressed = false;
  }
}

// Consume a flag (returns true once, then clears)
static bool consumePress(ButtonState& btn) {
  if (btn.wasPressed) { btn.wasPressed = false; return true; }
  return false;
}
static bool consumeLong(ButtonState& btn) {
  if (btn.longPressed) { btn.longPressed = false; return true; }
  return false;
}
static bool consumeXLong(ButtonState& btn) {
  if (btn.xlongPressed) { btn.xlongPressed = false; return true; }
  return false;
}

// ── Main Handler — Called from loop() ────────────────────

void handleButtons() {
  updateButtonState(btnLeft);
  updateButtonState(btnRight);
  updateButtonState(btnSelect);

  // ── Extra-long press actions (highest priority) ──
  if (consumeXLong(btnSelect)) {
    // Cycle through emotions
    Emotion next = (Emotion)((currentEmotion + 1) % EMO_COUNT);
    if (activeTheme == 2) {
      t3_setEmotion(next);
    } else if (activeTheme == 1) {
      t2_setEmotion(next);
    } else {
      setEmotion(next);
    }
    lastInteractionTime = millis();
    return;
  }

  // ── Long press actions (Hold > 800ms) ──
  if (consumeLong(btnLeft)) {
    if (currentState == STATE_SPOTIFY) {
      extern void sendCommand(String cmd);
      sendCommand("PREV");
    } else if (currentState != STATE_EYES) {
      changePage(-(int)currentState);  // Jump to Home (Eyes)
    }
    lastInteractionTime = millis();
    return;
  }

  if (consumeLong(btnRight)) {
    if (currentState == STATE_SPOTIFY) {
      extern void sendCommand(String cmd);
      sendCommand("NEXT");
    } else if (currentState != STATE_SETTINGS) {
      int delta = STATE_SETTINGS - (int)currentState;
      changePage(delta);               // Jump to Settings
    }
    lastInteractionTime = millis();
    return;
  }

  if (consumeLong(btnSelect)) {
    if (currentState == STATE_DR_HACK) {
      extern DrHackSubState dhCurrentState;
      if (dhCurrentState != DH_MENU) {
        dhCurrentState = DH_MENU;
        renderCurrentPage();
      } else {
        changePage(-(int)currentState);  // Exit to eyes
      }
    } else if (currentState == STATE_EYES) {
      // Hold on eyes toggles emotion / interaction
      Emotion next = (Emotion)((currentEmotion + 1) % EMO_COUNT);
      if (activeTheme == 2) t3_setEmotion(next);
      else if (activeTheme == 1) t2_setEmotion(next);
      else setEmotion(next);
    } else if (currentState == STATE_SPOTIFY) {
      extern void sendCommand(String cmd);
      sendCommand("SHUFFLE:TOGGLE");
    } else if (currentState == STATE_POMODORO) {
      extern void sendCommand(String cmd);
      sendCommand("POMO:RESET");
    } else {
      changePage(-(int)currentState);  // Return to Eyes
    }
    lastInteractionTime = millis();
    return;
  }

  // ── Short press actions (page navigation) ──
  if (consumePress(btnLeft)) {
    if (currentState == STATE_DR_HACK) {
      extern void dhNavigate(int delta);
      dhNavigate(-1);
    } else {
      changePage(-1);
    }
    lastInteractionTime = millis();
    return;
  }

  if (consumePress(btnRight)) {
    if (currentState == STATE_DR_HACK) {
      extern void dhNavigate(int delta);
      dhNavigate(1);
    } else {
      changePage(1);
    }
    lastInteractionTime = millis();
    return;
  }

  if (consumePress(btnSelect)) {
    if (currentState == STATE_DR_HACK) {
      extern void dhSelect();
      dhSelect();
      lastInteractionTime = millis();
      return;
    }

    // Page-specific SELECT actions
    if (currentState == STATE_EYES) {
      if (customEyeActive) {
        customEyeActive = false;
        renderCurrentPage();
      } else if (activeTheme == 2) {
        t3_nextExpression();
      } else if (activeTheme == 1) {
        t2_nextExpression();
      } else {
        setEmotion((Emotion)((currentEmotion + 1) % EMO_COUNT));
      }
    }
    else if (currentState == STATE_SPOTIFY) {
      extern void sendCommand(String cmd);
      sendCommand("PLAY_PAUSE");
    }
    else if (currentState == STATE_POMODORO) {
      extern void sendCommand(String cmd);
      extern bool pomoActive;
      if (pomoActive) sendCommand("POMO:PAUSE");
      else sendCommand("POMO:START");
    }
    else if (currentState == STATE_SETTINGS) {
      // Cycle activeTheme
      activeTheme = (activeTheme + 1) % THEME_COUNT;
      EEPROM.begin(EEPROM_SIZE);
      EEPROM.write(EEPROM_ACTIVE_THEME_ADDR, activeTheme);
      EEPROM.commit();
      EEPROM.end();
      
      // Reset drawing states
      extern void resetSpotifyDrawState();
      extern void t2_resetSpotifyDrawState();
      extern bool t2_initialized;
      resetSpotifyDrawState();
      t2_resetSpotifyDrawState();
      t2_initialized = false;
      
      // Visual cue: Circular screen wipe
      int rMax = max(SCREEN_W, SCREEN_H) * 1.5;
      uint16_t wipeColor = (activeTheme == 0) ? 0x2104 : (activeTheme == 1) ? 0x4810 : 0x1848;
      for (int r = 0; r < rMax; r += 12) {
        tft.drawCircle(SCREEN_W / 2, SCREEN_H / 2, r, wipeColor);
        tft.drawCircle(SCREEN_W / 2, SCREEN_H / 2, r+1, wipeColor);
        tft.drawCircle(SCREEN_W / 2, SCREEN_H / 2, r+2, wipeColor);
        delay(2);
      }
      
      redrawSettingsPartial();
    }
    lastInteractionTime = millis();
    return;
  }
}

#endif // ESP32
#endif // BUTTONS_H
