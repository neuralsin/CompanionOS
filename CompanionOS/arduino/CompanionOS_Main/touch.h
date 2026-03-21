#ifndef TOUCH_H
#define TOUCH_H

#include "globals.h"
#include "pages.h"
#include "network.h"

// ═══════════════════════════════════════════════════════════
// TOUCH & GESTURE INPUT
// ═══════════════════════════════════════════════════════════

void checkPhysicalSensors() {
  static bool lastLeftState = LOW;
  static bool lastRightState = LOW;
  static unsigned long lastSensorDebounce = 0;
  
  bool currentLeft = digitalRead(TOUCH_LEFT);
  bool currentRight = digitalRead(TOUCH_RIGHT);
  
  if (millis() - lastSensorDebounce > 50) {  
    // Trigger only on RISING edge (went from LOW to HIGH)
    if (currentLeft == HIGH && lastLeftState == LOW) {
      if (currentState == STATE_SPOTIFY) sendCommand("PREV");
      else changePage(-1);

      setEmotion(EMO_EXCITED);
      lastSensorDebounce = millis();
    }
    
    if (currentRight == HIGH && lastRightState == LOW) {
      if (currentState == STATE_SPOTIFY) sendCommand("NEXT");
      else changePage(1);

      setEmotion(EMO_HAPPY);
      lastSensorDebounce = millis();
    }
  }
  
  lastLeftState = currentLeft;
  lastRightState = currentRight;
}

// Swipe gesture tracking
bool isTouching = false;
int touchStartX = 0;
int touchStartY = 0;
int lastTouchX = 0;
int lastTouchY = 0;
unsigned long lastTouchTime = 0;
unsigned long lastRealContactTime = 0;

void handleTouch() {
  checkPhysicalSensors();
  
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    int x = map(p.x, 200, 3800, SCREEN_W, 0); // Inverted X Axis to match hardware
    int y = map(p.y, 200, 3800, SCREEN_H, 0); // Inverted Y Axis to match hardware
    
    // Valid point constraints to avoid spurious noise spikes
    if (x >= 0 && x <= SCREEN_W && y >= 0 && y <= SCREEN_H) {
      lastTouchX = x;
      lastTouchY = y;
      lastRealContactTime = millis();
      
      // Record start of touch for swipe gestures
      if (!isTouching) {
        isTouching = true;
        touchStartX = x;
        touchStartY = y;
      }
      
      // Debounced UI Taps (instead of delay(300))
      if (millis() - lastTouchTime > 300) {
        if (currentState == STATE_SPOTIFY) {
          if (y > 200 && y < 240) {
            if (x > 60 && x < 120) sendCommand("PREV");
            else if (x > 130 && x < 190) sendCommand("TOGGLE_PLAY");
            else if (x > 200 && x < 260) sendCommand("NEXT");
            
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
    // Touch released - Evaluate Swipe Gesture ONLY if physical contact dropped for >150ms
    if (isTouching && (millis() - lastRealContactTime > 150)) {
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
