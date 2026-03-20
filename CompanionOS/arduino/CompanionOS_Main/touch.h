#ifndef TOUCH_H
#define TOUCH_H

#include "globals.h"
#include "pages.h"
#include "network.h"

// ═══════════════════════════════════════════════════════════
// TOUCH & GESTURE INPUT
// ═══════════════════════════════════════════════════════════

void checkPhysicalSensors() {
  static unsigned long lastSensorTouch = 0;
  
  if (millis() - lastSensorTouch > 500) {  
    if (digitalRead(TOUCH_LEFT) == HIGH) {
      if (currentState == STATE_SPOTIFY) sendCommand("PREV");
      else changePage(-1);

      setEmotion(EMO_EXCITED);
      lastSensorTouch = millis();
    }
    
    if (digitalRead(TOUCH_RIGHT) == HIGH) {
      if (currentState == STATE_SPOTIFY) sendCommand("NEXT");
      else changePage(1);

      setEmotion(EMO_HAPPY);
      lastSensorTouch = millis();
    }
  }
}

// Swipe gesture tracking
bool isTouching = false;
int touchStartX = 0;
int touchStartY = 0;
int lastTouchX = 0;
int lastTouchY = 0;
unsigned long lastTouchTime = 0;

void handleTouch() {
  checkPhysicalSensors();
  
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    int x = map(p.x, 200, 3800, 0, SCREEN_W);
    int y = map(p.y, 200, 3800, 0, SCREEN_H);
    
    // Valid point constraints to avoid spurious noise spikes
    if (x >= 0 && x <= SCREEN_W && y >= 0 && y <= SCREEN_H) {
      lastTouchX = x;
      lastTouchY = y;
      
      // Record start of touch for swipe gestures
      if (!isTouching) {
        isTouching = true;
        touchStartX = x;
        touchStartY = y;
      }
      
      // Debounced UI Taps (instead of delay(300))
      if (millis() - lastTouchTime > 300) {
        if (currentState == STATE_SPOTIFY) {
          if (y > 260 && y < 300) {
            if (x > 20 && x < 80) sendCommand("PREV");
            else if (x > 90 && x < 150) sendCommand("PLAY_PAUSE");
            else if (x > 160 && x < 220) sendCommand("NEXT");
            lastTouchTime = millis();
          }
        } 
        else if (currentState == STATE_EYES) {
            setEmotion((Emotion)((currentEmotion + 1) % EMO_COUNT));
            lastTouchTime = millis();
        }
      }
    }
  } else {
    // Touch released - Evaluate Swipe Gesture
    if (isTouching) {
      isTouching = false;
      int deltaX = lastTouchX - touchStartX;
      int deltaY = lastTouchY - touchStartY;
      
      // Only process swipe if it's mostly horizontal and large enough
      if (abs(deltaX) > 60 && abs(deltaX) > abs(deltaY) * 2) {
        if (deltaX > 0) {
          changePage(-1); // Swipe Right -> Go back
        } else {
          changePage(1);  // Swipe Left -> Go forward
        }
      }
    }
  }
}

#endif
