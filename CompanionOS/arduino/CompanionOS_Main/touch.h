// ═══════════════════════════════════════════════════════════
// COMPANION OS v7.0 — TOUCH INPUT (ESP8266 ONLY)
// Guarded by #if HAS_TOUCH — ESP32 uses buttons.h instead.
// ═══════════════════════════════════════════════════════════
#ifndef TOUCH_H
#define TOUCH_H

#if HAS_TOUCH

#include "globals.h"
#include "pages.h"
#include "companion_net.h"

// ═══════════════════════════════════════════════════════════
// TOUCH & GESTURE INPUT V7
// [LEGACY - v6.0] checkPhysicalSensors() for GPIO0/GPIO2 cap
// sensors has been REMOVED. Those pins are freed (see config_esp8266.h).
// ═══════════════════════════════════════════════════════════

// Touchscreen gesture tracking
bool isTouching = false;
int touchStartX = 0;
int touchStartY = 0;
int lastTouchX = 0;
int lastTouchY = 0;
unsigned long lastTouchTime = 0;
unsigned long lastRealContactTime = 0;

extern int settingsScrollY;

void handleTouch() {
  // [LEGACY - v6.0] checkPhysicalSensors() call REMOVED
  // Cap touch pins TOUCH_LEFT (GPIO0/D3) and TOUCH_RIGHT (GPIO2/D4)
  // conflicted with boot mode and are now freed.

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

      if (!isTouching) {
        isTouching = true;
        touchStartX = x;
        touchStartY = y;
      }
    }
  } else {
    if (isTouching && (millis() - lastRealContactTime > 50)) {
      isTouching = false;
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
        if (settingsScrollY > 280) settingsScrollY = 280;
        redrawSettingsPartial();
      }
      // TAP detection - wider threshold for noisy touchscreens
      else if (abs(deltaX) < 25 && abs(deltaY) < 25) {
        if (millis() - lastTouchTime > 300) {
          if (currentState == STATE_SPOTIFY) {
            #define TOUCH_CIRCLE(tx, ty, cx, cy, r) (((tx - (cx)) * (tx - (cx)) + (ty - (cy)) * (ty - (cy))) <= ((r) * (r)))

            if (activeTheme == 1) {
              // Theme 2 Touch Zones
              int ctrlY = 184;
              int ctrlCX = SCREEN_W / 2;

              if (TOUCH_CIRCLE(lastTouchX, lastTouchY, ctrlCX - 50, ctrlY, 22)) sendCommand("PREV");
              else if (TOUCH_CIRCLE(lastTouchX, lastTouchY, ctrlCX, ctrlY, 22)) sendCommand("PLAY_PAUSE");
              else if (TOUCH_CIRCLE(lastTouchX, lastTouchY, ctrlCX + 50, ctrlY, 22)) sendCommand("NEXT");
              else if (TOUCH_CIRCLE(lastTouchX, lastTouchY, ctrlCX - 30, ctrlY - 20, 15)) sendCommand("SHUFFLE:TOGGLE");
              else if (TOUCH_CIRCLE(lastTouchX, lastTouchY, ctrlCX + 30, ctrlY - 20, 15)) sendCommand("REPEAT:TOGGLE");
              else if (lastTouchY >= 150 && lastTouchY <= 175 && playDuration > 0) {
                if (lastTouchX >= 30 && lastTouchX <= 290) {
                  int seekPos = map(lastTouchX, 30, 290, 0, playDuration);
                  char seekCmd[32];
                  sprintf(seekCmd, "SEEK:%d", seekPos);
                  sendCommand(seekCmd);
                  playProgress = seekPos;
                  extern void t2_redrawSpotifyPartial();
                  t2_redrawSpotifyPartial();
                }
              }
            } else {
              // Theme 1 Touch Zones
              if (lastTouchY > 100 && lastTouchY < 130) {
                if (lastTouchX >= 115 && lastTouchX < 145) sendCommand("PREV");
                else if (lastTouchX >= 145 && lastTouchX < 175) sendCommand("PLAY_PAUSE");
                else if (lastTouchX >= 175 && lastTouchX < 205) sendCommand("NEXT");
              }
              else if (lastTouchY >= 130 && lastTouchY < 148) {
                if (lastTouchX >= 120 && lastTouchX < 160) sendCommand("SHUFFLE:TOGGLE");
                else if (lastTouchX >= 160 && lastTouchX < 200) sendCommand("REPEAT:TOGGLE");
              }
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
          }
          else if (currentState == STATE_EYES) {
            if (activeTheme == 2) {
              t3_nextExpression();
            } else if (activeTheme == 1) {
              t2_nextExpression();
            } else {
              setEmotion((Emotion)((currentEmotion + 1) % EMO_COUNT));
            }
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
          // V6: New page tap interactions
          else if (currentState == STATE_STOCKS) {
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
            if (lastTouchX > 30 && lastTouchX < 80 && lastTouchY > 170) {
              sendCommand("SOCIAL:LIKE");
            }
          }
          else if (currentState == STATE_PRODUCTIVITY) {
            if (lastTouchX > 150 && lastTouchY > 36 && lastTouchY < 94) {
              sendCommand("TASK:DONE");
            }
          }
          // Theme toggle in settings
          else if (currentState == STATE_SETTINGS) {
            int themeSliderY = 25 - settingsScrollY + 20 + 14 + 20 + 20 + 14 + 14 + 20 + 20 + 14 + 20 + 20 + 40 + 22;
            if (lastTouchY >= themeSliderY && lastTouchY < themeSliderY + 28
                && lastTouchX >= 10 && lastTouchX <= 130) {
              activeTheme = (activeTheme + 1) % THEME_COUNT;
              EEPROM.begin(EEPROM_SIZE);
              EEPROM.write(EEPROM_ACTIVE_THEME_ADDR, activeTheme);
              EEPROM.commit();
              EEPROM.end();
              extern void resetSpotifyDrawState();
              extern void t2_resetSpotifyDrawState();
              extern bool t2_initialized;
              resetSpotifyDrawState();
              t2_resetSpotifyDrawState();
              t2_initialized = false;
              // Animate theme slider transition
              int segW = 120 / THEME_COUNT;
              for (int f = 0; f < 6; f++) {
                int targetX = 12 + (activeTheme * segW);
                int thumbW = segW - 4;
                uint16_t trackCol = (activeTheme == 0) ? 0x2104 : (activeTheme == 1) ? 0x4810 : 0x1848;
                tft.fillRoundRect(10, themeSliderY, 120, 24, 12, trackCol);
                tft.fillRoundRect(targetX, themeSliderY + 2, thumbW, 20, 10, TFT_WHITE);
                delay(25);
              }
              redrawSettingsPartial();
            }
          }

          lastTouchTime = millis();
        }
      }
    }
  }
}

#endif // HAS_TOUCH
#endif // TOUCH_H
