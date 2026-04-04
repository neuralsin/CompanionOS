#ifndef TOUCH_H
#define TOUCH_H

#include "globals.h"
#include "pages.h"
#include "network.h"

// ═══════════════════════════════════════════════════════════
// TOUCH & GESTURE INPUT V3
// Fixed debouncing for GPIO0/GPIO2 capacitive sensors
// + V4: Long-press petting, ink-drop transitions
// ═══════════════════════════════════════════════════════════

// Capacitive sensors need extra debounce because GPIO0/GPIO2 are boot pins
unsigned long lastLeftTrigger = 0;
unsigned long lastRightTrigger = 0;
#define CAP_DEBOUNCE_MS 400  // Much longer debounce for noisy caps

bool lastLeftState = LOW;
bool lastRightState = LOW;

void checkPhysicalSensors() {
  unsigned long now = millis();
  
  bool currentLeft = digitalRead(TOUCH_LEFT);
  bool currentRight = digitalRead(TOUCH_RIGHT);
  
  // Left sensor: only trigger on HIGH transition with long debounce
  if (currentLeft == HIGH && lastLeftState == LOW && (now - lastLeftTrigger > CAP_DEBOUNCE_MS)) {
    lastLeftTrigger = now;
    lastInteractionTime = now;
    changePage(-1);
  }
  
  // Right sensor: same
  if (currentRight == HIGH && lastRightState == LOW && (now - lastRightTrigger > CAP_DEBOUNCE_MS)) {
    lastRightTrigger = now;
    lastInteractionTime = now;
    changePage(1);
  }
  
  // V4: Long press on physical sensors = petting
  if (currentState == STATE_EYES) {
    bool leftHeld = (currentLeft == HIGH && lastLeftState == HIGH && (now - lastLeftTrigger > 1500));
    bool rightHeld = (currentRight == HIGH && lastRightState == HIGH && (now - lastRightTrigger > 1500));
    
    if (leftHeld || rightHeld) {
      if (!isPetting) {
        isPetting = true;
        petMoodLevel = min(petMoodLevel + 20, 100);
        lastInteractionTime = now;
      }
    } else if (currentLeft == LOW && currentRight == LOW && isPetting) {
      isPetting = false;
      // Redraw normal eyes after petting ends
      extern void drawEyesPage();
      drawEyesPage();
    }
  }
  
  lastLeftState = currentLeft;
  lastRightState = currentRight;
}

// ── V4: Ink-drop page transition ──────────────────────────
void inkDropTransition() {
  if (!exoticMode) return;  // Only in exotic mode
  
  int cx = SCREEN_W / 2;
  int cy = SCREEN_H / 2;
  
  // Accent color based on destination page
  uint16_t dropColor;
  switch (currentState) {
    case STATE_EYES:          dropColor = 0x0013; break;
    case STATE_SPOTIFY:       dropColor = 0x07E0; break;
    case STATE_POMODORO:      dropColor = 0xDF26; break;
    case STATE_WEATHER:       dropColor = TFT_YELLOW; break;
    case STATE_NOTIFICATIONS: dropColor = TFT_MAGENTA; break;
    case STATE_NOTES:         dropColor = TFT_CYAN; break;
    case STATE_STOCKS:        dropColor = 0x2E8B; break;  // V6: green
    case STATE_GAMING:        dropColor = 0x07DF; break;  // V6: cyan
    case STATE_SOCIAL:        dropColor = 0x5A9F; break;  // V6: blurple
    case STATE_PRODUCTIVITY:  dropColor = 0xF800; break;  // V6: red
    case STATE_SETTINGS:      dropColor = 0x4208; break;
    default:                  dropColor = 0x0821; break;
  }
  
  // Expand circle from center — ~300ms total
  for (int r = 5; r <= 200; r += 15) {
    tft.fillCircle(cx, cy, r, dropColor);
    delay(8);
  }
  
  // Fill to solid then clear
  tft.fillScreen(COLOR_BG);
}

// Touchscreen gesture tracking
bool isTouching = false;
int touchStartX = 0;
int touchStartY = 0;
int lastTouchX = 0;
int lastTouchY = 0;
unsigned long lastTouchTime = 0;
unsigned long lastRealContactTime = 0;
unsigned long touchStartTime = 0;  // V4: For long-press detection

extern int settingsScrollY;

void handleTouch() {
  checkPhysicalSensors();
  
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    int x = map(p.x, 200, 3800, SCREEN_W, 0);
    int y = map(p.y, 200, 3800, SCREEN_H, 0);
    
    // Hardware auto-calibration compensation for resistive drift
    x += 30;
    y -= 15;
    
    if (x >= 0 && x <= SCREEN_W && y >= 0 && y <= SCREEN_H) {
      lastTouchX = x;
      lastTouchY = y;
      lastRealContactTime = millis();
      lastInteractionTime = millis();  // V4: Pet interaction
      
      if (!isTouching) {
        isTouching = true;
        touchStartX = x;
        touchStartY = y;
        touchStartTime = millis();  // V4: Record press start
      }
      
      // V4: Long-press petting detection on eyes page
      if (currentState == STATE_EYES && !isPetting) {
        unsigned long holdTime = millis() - touchStartTime;
        int moveX = abs(lastTouchX - touchStartX);
        int moveY = abs(lastTouchY - touchStartY);
        if (holdTime > 1500 && moveX < 20 && moveY < 20) {
          isPetting = true;
          petMoodLevel = min(petMoodLevel + 20, 100);
        }
      }
    }
  } else {
    if (isTouching && (millis() - lastRealContactTime > 50)) {
      isTouching = false;
      
      // V4: End petting mode on release
      if (isPetting) {
        isPetting = false;
        extern void drawEyesPage();
        drawEyesPage();
        lastTouchTime = millis();
        return;  // Don't process swipe/tap after petting
      }
      
      int deltaX = lastTouchX - touchStartX;
      int deltaY = lastTouchY - touchStartY;
      
      // SWIPE detection
      if (abs(deltaX) > 50 && abs(deltaX) > abs(deltaY)) {
        if (deltaX > 0) changePage(-1);
        else changePage(1);
      }
      // VERTICAL SWIPE on settings page (scroll)
      else if (abs(deltaY) > 30 && abs(deltaY) > abs(deltaX) && currentState == STATE_SETTINGS) {
        settingsScrollY += (deltaY < 0) ? 40 : -40;
        if (settingsScrollY < 0) settingsScrollY = 0;
        if (settingsScrollY > 500) settingsScrollY = 500;
        redrawSettingsPartial();
      }
      // TAP detection - wider threshold for noisy touchscreens
      else if (abs(deltaX) < 25 && abs(deltaY) < 25) {
        if (millis() - lastTouchTime > 300) {
          
          if (currentState == STATE_SPOTIFY) {
            // Top Row (Y: 100-130) -> Prev, Play, Next
            if (lastTouchY > 100 && lastTouchY < 130) {
              if (lastTouchX >= 115 && lastTouchX < 145) sendCommand("PREV");
              else if (lastTouchX >= 145 && lastTouchX < 175) sendCommand("PLAY_PAUSE");
              else if (lastTouchX >= 175 && lastTouchX < 205) sendCommand("NEXT");
            }
            // Bottom Row (Y: 130-148) -> Shuffle, Repeat
            else if (lastTouchY >= 130 && lastTouchY < 148) {
              if (lastTouchX >= 120 && lastTouchX < 160) sendCommand("SHUFFLE:TOGGLE");
              else if (lastTouchX >= 160 && lastTouchX < 200) sendCommand("REPEAT:TOGGLE");
            }
            // Progress bar seek zone (Y: 148-175)
            else if (lastTouchY >= 148 && lastTouchY <= 175) {
              if (lastTouchX < 115) sendCommand("LIKE:TOGGLE");
              else if (lastTouchX >= 118 && lastTouchX <= 208 && playDuration > 0) {
                int seekPos = map(lastTouchX, 118, 208, 0, playDuration);
                char seekCmd[32];
                sprintf(seekCmd, "SEEK:%d", seekPos);
                sendCommand(seekCmd);
                playProgress = seekPos;
                redrawSpotifyPartial();
              }
            }
          }
          else if (currentState == STATE_EYES) {
            setEmotion((Emotion)((currentEmotion + 1) % EMO_COUNT));
          }
          else if (currentState == STATE_POMODORO) {
            if (pomoActive) sendCommand("POMO:PAUSE");
            else sendCommand("POMO:START");
          }
          else if (currentState == STATE_NOTIFICATIONS) {
            if (lastTouchY > SCREEN_H - 40) {
              flashNotifEnabled = !flashNotifEnabled;
              redrawNotificationsPartial();
            }
            else if (lastTouchY > SCREEN_H - 60 && lastTouchY <= SCREEN_H - 40) {
              sendCommand("NOTIF:CLEAR");
            }
          }
          else if (currentState == STATE_SETTINGS) {
            // V5: Theme Selector — tap detection for theme list rows
            int themeListStartY = 47 - settingsScrollY;
            int themeListEndY = themeListStartY + (THEME_COUNT * 32); // Matched expanded Y-spacing
            
            // Restrict X-axis so left-side scrolling doesn't accidentally trigger a theme swap
            if (lastTouchX > 100 && lastTouchY >= max(18, themeListStartY) && lastTouchY < themeListEndY) {
              int tappedTheme = (lastTouchY - themeListStartY) / 32;
              if (tappedTheme >= 0 && tappedTheme < THEME_COUNT && tappedTheme != currentThemeId) {
                extern void playThemeTransition(uint8_t fromId, uint8_t toId);
                playThemeTransition(currentThemeId, (uint8_t)tappedTheme);
                extern void drawSettingsPage();
                drawSettingsPage();
              }
            }
          }
          // ── V6: New page tap interactions ──────────────
          else if (currentState == STATE_STOCKS) {
            // Tap watchlist items (right pane, y: 40-196)
            if (lastTouchX > 160 && lastTouchY > 40 && lastTouchY < 196) {
              int idx = (lastTouchY - 40) / 52;
              if (idx >= 0 && idx < 3) {
                char cmd[24];
                sprintf(cmd, "STOCK:SET:%s", wlSymbol[idx]);
                sendCommand(cmd);
              }
            }
          }
          else if (currentState == STATE_SOCIAL) {
            // Tap heart zone (bottom-left of card)
            if (lastTouchX > 30 && lastTouchX < 80 && lastTouchY > 170) {
              sendCommand("SOCIAL:LIKE");
            }
          }
          else if (currentState == STATE_PRODUCTIVITY) {
            // Tap active task card to mark done
            if (lastTouchX > 150 && lastTouchY > 36 && lastTouchY < 94) {
              sendCommand("TASK:DONE");
            }
          }
          
          lastTouchTime = millis();
        }
      }
    }
  }
}
#endif
