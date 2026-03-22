import textwrap
eyes_cpp = """#ifndef EYES_H
#define EYES_H

#include "globals.h"
#include <math.h>

// ═══════════════════════════════════════════════════════════
// EYE ANIMATION ENGINE - NEXT-GEN "Microsoft-level" Cat Eyes
//
// Features:
//   - Deep tier gradients mapped via vertical geometry line blasts
//   - Parabolic "Almond" eyes matching the user's high-res reference
//   - 8 distinct emotional expressions built on fluid geometry scaling
//   - Centralized sleek "nose" clock, masked perfectly from the canvas
// ═══════════════════════════════════════════════════════════

enum Emotion {
  EMO_NEUTRAL,
  EMO_HAPPY,
  EMO_SAD,
  EMO_EXCITED,
  EMO_LOVE,
  EMO_SLEEPY,
  EMO_ANGRY,
  EMO_SURPRISED,
  EMO_COUNT
};

extern Emotion currentEmotion;

// ── Layout Constants ─────────────────────────────────────
#define EYE_Y        110
#define LEFT_EYE_X   95
#define RIGHT_EYE_X  225
#define EYE_RX       48   // Width of the almond eye
#define EYE_RY       38   // Baseline Height curve
#define PUPIL_R      10

// ── Animation State ──────────────────────────────────────
extern unsigned long lastBlink;
extern bool isBlinking;
extern int blinkPhase;

// Pupil drift
float pupilOffsetX = 0;
float pupilOffsetY = 0;
float pupilTargetX = 0;
float pupilTargetY = 0;
unsigned long lastPupilMove = 0;

// Time state
int displayHour = 0;
int displayMinute = 0;
bool timeReceived = false;
unsigned long lastTimeDraw = 0;
unsigned long bootMillis = 0;

// ── Forward Declarations ─────────────────────────────────
void drawEyes();
void setEmotion(Emotion newEmotion);
void updateEyes();
void drawTimeDisplay();

// ── Advanced Geometry Renderers ──────────────────────────

// Core Almond/Cat-Eye generator wrapping 3-tier gradient segments over a parabola math envelope
void drawAlmondEye(int cx, int cy, float width, float height, float pX, float pY, uint16_t cTop, uint16_t cMid, uint16_t cBot, float tiltL, float tiltR) {
  float halfW = width / 2.0;

  for (int x = -halfW; x <= halfW; x++) {
    // Math: Normalized Parabola
    float nx = (float)x / halfW;
    float curveTop = 1.0 - (nx * nx);
    float curveBot = 1.0 - (nx * nx);
    
    // Apply asymmetrical tilt to mimic cat-eye outer slants via linear X multiplication
    float top_mod = (x < 0) ? (x * tiltL) : (x * tiltR);
    float bot_mod = (x < 0) ? (x * tiltL) : (x * tiltR);

    int y_top = (int)(-height * curveTop + top_mod);
    int y_bottom = (int)(height * curveBot - bot_mod);

    int h = y_bottom - y_top;
    
    if (h > 0) {
      // 3-Tier Gradient Segments
      int seg1 = h / 3;
      int seg2 = h / 3;
      int seg3 = h - (seg1 + seg2);

      tft.drawFastVLine(cx + x, cy + y_top, seg1, cTop);
      tft.drawFastVLine(cx + x, cy + y_top + seg1, seg2, cMid);
      tft.drawFastVLine(cx + x, cy + y_top + seg1 + seg2, seg3, cBot);
    }
  }

  // Draw Vertical Cat Pupil (Pill shape)
  int px = cx + (int)pX;
  int py = cy + (int)pY;

  // Masking pupil within eye bounds (crude but fast bounds estimation)
  tft.fillRoundRect(px - 6, py - 18, 12, 36, 6, 0x0000); 
  
  // High-contrast Reflection points
  tft.fillCircle(px + 2, py - 10, 4, TFT_WHITE);
  tft.fillCircle(px + 6, py - 3, 2, TFT_WHITE);
}

// ═══════════════════════════════════════════════════════════
// EMOTION RENDERERS
// ═══════════════════════════════════════════════════════════

// Gradient Colors
#define BLUE_TOP 0x011A
#define BLUE_MID 0x031D
#define BLUE_BOT TFT_CYAN

#define GOLD_TOP 0x8200
#define GOLD_MID 0xCA00
#define GOLD_BOT TFT_YELLOW

#define GREY_TOP 0x4208
#define GREY_MID 0x630C
#define GREY_BOT 0xA514

#define ROSE_TOP 0x900A
#define ROSE_MID 0xD00E
#define ROSE_BOT TFT_PINK

#define PURP_TOP 0x410A
#define PURP_MID 0x610E
#define PURP_BOT 0xA214

#define FIRE_TOP 0x8000
#define FIRE_MID 0xC000
#define FIRE_BOT TFT_RED

void drawNeutralEyes() {
  // Deep tracking Cat Eyes (The reference image)
  drawAlmondEye(LEFT_EYE_X, EYE_Y, EYE_RX*2, EYE_RY, pupilOffsetX, pupilOffsetY, BLUE_TOP, BLUE_MID, BLUE_BOT, 0.25, -0.25);
  drawAlmondEye(RIGHT_EYE_X, EYE_Y, EYE_RX*2, EYE_RY, pupilOffsetX, pupilOffsetY, BLUE_TOP, BLUE_MID, BLUE_BOT, 0.25, -0.25);
}

void drawHappyEyes() {
  // Squinted bottom, gold hue
  drawAlmondEye(LEFT_EYE_X, EYE_Y, EYE_RX*2, EYE_RY - 15, pupilOffsetX, pupilOffsetY, GOLD_TOP, GOLD_MID, GOLD_BOT, 0.4, -0.1);
  drawAlmondEye(RIGHT_EYE_X, EYE_Y, EYE_RX*2, EYE_RY - 15, pupilOffsetX, pupilOffsetY, GOLD_TOP, GOLD_MID, GOLD_BOT, 0.1, -0.4);
}

void drawSadEyes() {
  // Droopy angles outwards
  drawAlmondEye(LEFT_EYE_X, EYE_Y, EYE_RX*2, EYE_RY - 5, 0, 5, GREY_TOP, GREY_MID, GREY_BOT, -0.3, 0.3);
  drawAlmondEye(RIGHT_EYE_X, EYE_Y, EYE_RX*2, EYE_RY - 5, 0, 5, GREY_TOP, GREY_MID, GREY_BOT, -0.3, 0.3);
}

void drawExcitedEyes() {
  // Huge open cyan/magenta eyes
  drawAlmondEye(LEFT_EYE_X, EYE_Y, EYE_RX*2, EYE_RY + 15, pupilOffsetX*1.5, pupilOffsetY*1.5, ROSE_TOP, BLUE_MID, TFT_CYAN, 0.1, -0.1);
  drawAlmondEye(RIGHT_EYE_X, EYE_Y, EYE_RX*2, EYE_RY + 15, pupilOffsetX*1.5, pupilOffsetY*1.5, ROSE_TOP, BLUE_MID, TFT_CYAN, 0.1, -0.1);
}

void drawLoveEyes() {
  drawAlmondEye(LEFT_EYE_X, EYE_Y, EYE_RX*2, EYE_RY + 5, pupilOffsetX, pupilOffsetY, ROSE_TOP, ROSE_MID, ROSE_BOT, 0.3, -0.3);
  drawAlmondEye(RIGHT_EYE_X, EYE_Y, EYE_RX*2, EYE_RY + 5, pupilOffsetX, pupilOffsetY, ROSE_TOP, ROSE_MID, ROSE_BOT, 0.3, -0.3);
}

void drawSleepyEyes() {
  // Extremely thin purple slits
  drawAlmondEye(LEFT_EYE_X, EYE_Y, EYE_RX*2, EYE_RY - 25, 0, 0, PURP_TOP, PURP_MID, PURP_BOT, 0.1, -0.1);
  drawAlmondEye(RIGHT_EYE_X, EYE_Y, EYE_RX*2, EYE_RY - 25, 0, 0, PURP_TOP, PURP_MID, PURP_BOT, 0.1, -0.1);
}

void drawAngryEyes() {
  // Aggressive inner V slopes
  drawAlmondEye(LEFT_EYE_X, EYE_Y, EYE_RX*2, EYE_RY - 10, 5, -5, FIRE_TOP, FIRE_MID, FIRE_BOT, 0.5, 0.0);
  drawAlmondEye(RIGHT_EYE_X, EYE_Y, EYE_RX*2, EYE_RY - 10, -5, -5, FIRE_TOP, FIRE_MID, FIRE_BOT, 0.0, -0.5);
}

void drawSurprisedEyes() {
  drawAlmondEye(LEFT_EYE_X, EYE_Y, EYE_RX*2 - 20, EYE_RY + 20, 0, 0, GOLD_TOP, GOLD_MID, GOLD_BOT, 0.0, 0.0);
  drawAlmondEye(RIGHT_EYE_X, EYE_Y, EYE_RX*2 - 20, EYE_RY + 20, 0, 0, GOLD_TOP, GOLD_MID, GOLD_BOT, 0.0, 0.0);
}


// ═══════════════════════════════════════════════════════════
// MAIN DRAW CALL & CLOCK LOGIC
// ═══════════════════════════════════════════════════════════

void drawEyes() {
  switch (currentEmotion) {
    case EMO_HAPPY:     drawHappyEyes(); break;
    case EMO_SAD:       drawSadEyes(); break;
    case EMO_EXCITED:   drawExcitedEyes(); break;
    case EMO_LOVE:      drawLoveEyes(); break;
    case EMO_SLEEPY:    drawSleepyEyes(); break;
    case EMO_ANGRY:     drawAngryEyes(); break;
    case EMO_SURPRISED: drawSurprisedEyes(); break;
    default:            drawNeutralEyes(); break;
  }
  
  // Always safely redraw the nose-clock over the updated canvas
  drawTimeDisplay();
}

void drawTimeDisplay() {
  if (!timeReceived) return;
  
  char timeStr[6];
  sprintf(timeStr, "%02d:%02d", displayHour, displayMinute);
  
  // Sleek Nose Placement
  int cx = SCREEN_W / 2;
  int cy = 205; 
  
  // Paint subtle grey bounding rectangle (so time doesn't bleed)
  tft.fillRect(cx - 35, cy, 70, 25, COLOR_BG);
  
  // Thin silver font overlay
  tft.setTextColor(0xCE79);
  tft.drawCentreString(timeStr, cx, cy + 2, 2); 
}

void setEmotion(Emotion newEmotion) {
  if (currentEmotion != newEmotion) {
    currentEmotion = newEmotion;
    if (currentState == STATE_EYES) {
      tft.fillScreen(COLOR_BG); // Erase whole screen on emotion change!
      drawEyes();
    }
  }
}

// Fluid 60FPS autonomous tracking
void updateEyes() {
  if (currentState != STATE_EYES) return;
  
  unsigned long now = millis();
  
  // Track continuous pupil drift targeting
  if (now - lastPupilMove > 4000) {
    if (random(100) > 40) {
      pupilTargetX = random(-15, 15);
      pupilTargetY = random(-10, 10);
    } else {
      pupilTargetX = 0;
      pupilTargetY = 0;
    }
    lastPupilMove = now;
  }
  
  // Lerp tracking
  bool tracking = false;
  if (abs(pupilOffsetX - pupilTargetX) > 0.5 || abs(pupilOffsetY - pupilTargetY) > 0.5) {
    pupilOffsetX += (pupilTargetX - pupilOffsetX) * 0.15;
    pupilOffsetY += (pupilTargetY - pupilOffsetY) * 0.15;
    tracking = true;
  }
  
  // Blinking loop isolation
  if (now - lastBlink > 4000 && !isBlinking && random(10) > 3) {
    isBlinking = true;
    blinkPhase = 0;
  }
  
  if (isBlinking) {
    blinkPhase++;
    int blinkCoverY = EYE_Y - EYE_RY;
    int blinkHeight = 0;
    
    if (blinkPhase <= 3) blinkHeight = (EYE_RY * 2) * (blinkPhase / 3.0);
    else if (blinkPhase <= 6) blinkHeight = (EYE_RY * 2) * ((6 - blinkPhase) / 3.0);
    else {
      isBlinking = false;
      lastBlink = now;
      drawEyes(); // Restore eyes
      return;
    }
    
    tft.fillRect(LEFT_EYE_X - EYE_RX, blinkCoverY, EYE_RX * 2, blinkHeight, COLOR_BG);
    tft.fillRect(RIGHT_EYE_X - EYE_RX, blinkCoverY, EYE_RX * 2, blinkHeight, COLOR_BG);
  } else if (tracking && !isBlinking) {
    drawEyes(); // Heavy continuous interpolation
  }
  
  // 30-Second subtle clock interval (prevents aggressive flashing)
  if (now - lastTimeDraw > 30000 && timeReceived && !isBlinking) {
    drawTimeDisplay();
    lastTimeDraw = now;
  }
}

// Hook logic matching Network
void updateTimeFromUDP(String timeStr) {
  int colonIndex = timeStr.indexOf(':');
  if (colonIndex > 0) {
    displayHour = timeStr.substring(0, colonIndex).toInt();
    displayMinute = timeStr.substring(colonIndex + 1).toInt();
    timeReceived = true;
    
    if (currentState == STATE_EYES) {
      drawTimeDisplay();
    }
  }
}

#endif
"""

with open(r'CompanionOS\arduino\CompanionOS_Main\eyes.h', 'w', encoding='utf-8') as f:
    f.write(eyes_cpp)

print("Updated eyes.h with Next-Gen Microsoft geometry.")
