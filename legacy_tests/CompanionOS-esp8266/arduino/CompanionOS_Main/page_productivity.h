#ifndef PAGE_PRODUCTIVITY_H
#define PAGE_PRODUCTIVITY_H
#include "globals.h"

extern uint16_t blendColor(uint16_t c1, uint16_t c2, float t);
extern void drawPageIndicator(int current, int total);

// ═══════════════════════════════════════════════════════════
// PRODUCTIVITY HUB — V6 Premium Clock + Agenda Page
// ═══════════════════════════════════════════════════════════

#define PRD_BG       0x0841
#define PRD_CARD     0x1082
#define PRD_ACTIVE   0x18E3
#define PRD_RED      0xF800
#define PRD_BLUE     0x2A9F
#define PRD_DIM      0x4A49
#define PRD_DIVIDER  0x2104
#define PRD_GREEN    0x2E8B

static bool prodPageDrawn = false;
static int lastProdMinute = -1;
static char lastTaskCurrent[32] = "";

void resetProductivityDrawState() {
  prodPageDrawn = false;
  lastProdMinute = -1;
  lastTaskCurrent[0] = 0;
}

const uint8_t dotFont[10][5] = {
  {0b111, 0b101, 0b101, 0b101, 0b111}, // 0
  {0b010, 0b110, 0b010, 0b010, 0b111}, // 1
  {0b111, 0b001, 0b111, 0b100, 0b111}, // 2
  {0b111, 0b001, 0b111, 0b001, 0b111}, // 3
  {0b101, 0b101, 0b111, 0b001, 0b001}, // 4
  {0b111, 0b100, 0b111, 0b001, 0b111}, // 5
  {0b111, 0b100, 0b111, 0b101, 0b111}, // 6
  {0b111, 0b001, 0b010, 0b010, 0b010}, // 7
  {0b111, 0b101, 0b111, 0b101, 0b111}, // 8
  {0b111, 0b101, 0b111, 0b001, 0b111}  // 9
};

void drawDotDigit(int x, int y, int val, uint16_t color) {
  if (val < 0 || val > 9) return;
  int pitch = 14;
  int radius = 5;
  for (int row = 0; row < 5; row++) {
    uint8_t bits = dotFont[val][row];
    for (int col = 0; col < 3; col++) {
      int dx = x + (2 - col) * pitch;
      int dy = y + row * pitch;
      if (bits & (1 << col)) {
        tft.fillCircle(dx, dy, radius, color);
      } else {
        tft.fillCircle(dx, dy, 2, 0x2104);
      }
    }
  }
}

void drawProductivityClock(int bx, int by) {
  int h = timeReceived ? displayHour : 0;
  int m = timeReceived ? displayMinute : 0;
  drawDotDigit(bx, by, h / 10, TFT_WHITE);
  drawDotDigit(bx + 52, by, h % 10, TFT_WHITE);
  drawDotDigit(bx, by + 85, m / 10, TFT_WHITE);
  drawDotDigit(bx + 52, by + 85, m % 10, TFT_WHITE);
}

void drawTaskProgress(int x, int y, int w, uint8_t pct) {
  tft.fillRect(x, y, w, 3, PRD_DIVIDER);
  int fillW = (int)((long)pct * w / 100);
  if (fillW > 0) {
    tft.fillRect(x, y, fillW, 3, PRD_BLUE);
    tft.fillCircle(x + fillW, y + 1, 2, TFT_WHITE);
  }
}

void drawProductivityPage() {
  tft.fillScreen(PRD_BG);

  tft.setTextColor(PRD_RED);
  tft.drawString("Productivity", 10, 2, 2);

  if (timeReceived) {
    char hdrTime[6];
    sprintf(hdrTime, "%02d:%02d", displayHour, displayMinute);
    tft.setTextColor(TFT_WHITE);
    tft.drawRightString(hdrTime, SCREEN_W - 10, 3, 2);
  }

  tft.drawFastHLine(0, 18, SCREEN_W, PRD_DIVIDER);

  drawProductivityClock(30, 45);

  tft.setTextColor(PRD_BLUE);
  tft.drawString("THU", 30, 208, 2);
  tft.setTextColor(PRD_DIM);
  tft.drawString("APR 03", 75, 208, 2);

  int rx = 150;
  tft.drawFastVLine(145, 19, SCREEN_H - 35, PRD_DIVIDER);

  tft.setTextColor(PRD_BLUE);
  tft.drawString("NOW", rx, 24, 1);

  if (taskCurrent[0]) {
    tft.fillRoundRect(rx, 36, 162, 58, 4, PRD_ACTIVE);
    tft.drawRoundRect(rx, 36, 162, 58, 4, PRD_BLUE);
    drawTaskProgress(rx + 4, 38, 154, taskProgressPct);
    tft.setTextColor(TFT_WHITE);
    String tTitle = String(taskCurrent);
    tft.drawString(tTitle.substring(0, 20), rx + 8, 46, 2);
    tft.setTextColor(PRD_BLUE);
    tft.drawString(taskCurrentTime, rx + 8, 66, 1);
    float pulse = (sin(millis() * 0.003f) + 1.0f) * 0.5f;
    uint16_t dotColor = blendColor(PRD_BLUE, TFT_WHITE, pulse);
    tft.fillCircle(rx + 150, 46, 3, dotColor);
  } else {
    tft.fillRoundRect(rx, 36, 162, 58, 4, PRD_CARD);
    tft.setTextColor(PRD_DIM);
    tft.drawCentreString("No active task", rx + 81, 55, 2);
  }

  tft.setTextColor(PRD_DIM);
  tft.drawString("NEXT", rx, 102, 1);
  tft.drawFastHLine(rx, 114, 160, PRD_DIVIDER);

  if (taskNext1[0]) {
    tft.fillCircle(rx + 6, 127, 2, PRD_BLUE);
    tft.setTextColor(TFT_WHITE);
    tft.drawString(String(taskNext1).substring(0, 18), rx + 14, 120, 2);
    tft.setTextColor(PRD_DIM);
    tft.drawRightString(taskNext1Time, rx + 158, 121, 1);
  }

  tft.drawFastHLine(rx + 14, 140, 146, PRD_DIVIDER);

  if (taskNext2[0]) {
    tft.fillCircle(rx + 6, 154, 2, PRD_DIM);
    tft.setTextColor(0x8410);
    tft.drawString(String(taskNext2).substring(0, 18), rx + 14, 147, 2);
    tft.setTextColor(PRD_DIM);
    tft.drawRightString(taskNext2Time, rx + 158, 148, 1);
  }

  tft.drawFastHLine(rx, 172, 160, PRD_DIVIDER);
  tft.setTextColor(PRD_DIM);
  tft.drawString("Today", rx, 178, 1);

  int timelineY = 192;
  tft.fillRect(rx, timelineY, 160, 4, PRD_DIVIDER);
  if (timeReceived) {
    int pos = map(displayHour * 60 + displayMinute, 0, 1440, 0, 160);
    tft.fillRect(rx, timelineY, pos, 4, PRD_BLUE);
    tft.fillCircle(rx + pos, timelineY + 2, 3, TFT_WHITE);
  }

  tft.setTextColor(PRD_DIM);
  tft.drawString("6AM", rx, timelineY + 8, 1);
  tft.drawRightString("12AM", rx + 158, timelineY + 8, 1);

  drawPageIndicator(STATE_PRODUCTIVITY, STATE_COUNT);
  prodPageDrawn = true;
}

void redrawProductivityPartial() {
  if (currentState != STATE_PRODUCTIVITY) return;

  if (displayMinute != lastProdMinute && timeReceived) {
    // Clear and redraw the clock at the SAME position as initial draw
    tft.fillRect(28, 43, 80, 160, PRD_BG);
    drawProductivityClock(30, 45);
    
    // Update the header time (top-right)
    tft.fillRect(SCREEN_W - 60, 2, 55, 16, PRD_BG);
    char hdrTime[6];
    sprintf(hdrTime, "%02d:%02d", displayHour, displayMinute);
    tft.setTextColor(TFT_WHITE);
    tft.drawRightString(hdrTime, SCREEN_W - 10, 3, 2);
    
    // Update the timeline bar
    int rx = 150;
    int timelineY = 192;
    tft.fillRect(rx, timelineY, 160, 4, PRD_DIVIDER);
    int pos = map(displayHour * 60 + displayMinute, 0, 1440, 0, 160);
    tft.fillRect(rx, timelineY, pos, 4, PRD_BLUE);
    tft.fillCircle(rx + pos, timelineY + 2, 3, TFT_WHITE);
    
    lastProdMinute = displayMinute;
  }

  if (strcmp(taskCurrent, lastTaskCurrent) != 0) {
    drawProductivityPage();
    strcpy(lastTaskCurrent, taskCurrent);
  }

  if (taskActive && taskCurrent[0]) {
    int rx = 150;
    drawTaskProgress(rx + 4, 38, 154, taskProgressPct);
  }
}

#endif
