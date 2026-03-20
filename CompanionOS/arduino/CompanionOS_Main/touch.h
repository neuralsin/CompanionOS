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

void handleTouch() {
  checkPhysicalSensors();
  
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    int x = map(p.x, 200, 3800, 0, SCREEN_W);
    int y = map(p.y, 200, 3800, 0, SCREEN_H);
    
    if (currentState == STATE_SPOTIFY) {
      if (y > 260 && y < 300) {
        if (x > 20 && x < 80) sendCommand("PREV");
        else if (x > 90 && x < 150) sendCommand("PLAY_PAUSE");
        else if (x > 160 && x < 220) sendCommand("NEXT");
        delay(300); // UI Debounce
      }
    } 
    else if (currentState == STATE_EYES) {
        setEmotion((Emotion)((currentEmotion + 1) % EMO_COUNT));
        delay(300);
    }
  }
}

#endif
