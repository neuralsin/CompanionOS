#ifndef TOUCH_H
#define TOUCH_H

#include "globals.h"
#include "pages.h"
#include "network.h"

// ═══════════════════════════════════════════════════════════
// TOUCH & GESTURE INPUT V3
// Fixed debouncing for GPIO0/GPIO2 capacitive sensors
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
    if (currentState == STATE_SPOTIFY) sendCommand("PREV");
    else changePage(-1);
  }
  
  // Right sensor: same
  if (currentRight == HIGH && lastRightState == LOW && (now - lastRightTrigger > CAP_DEBOUNCE_MS)) {
    lastRightTrigger = now;
    if (currentState == STATE_SPOTIFY) sendCommand("NEXT");
    else changePage(1);
  }
  
  lastLeftState = currentLeft;
  lastRightState = currentRight;
}

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
        if (settingsScrollY > 200) settingsScrollY = 200;
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
            // ANY tap on the Pomodoro page toggles start/pause
            // The whole screen is the timer — no Y-zone restriction needed
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
          
          lastTouchTime = millis();
        }
      }
    }
  }
}
#endif
