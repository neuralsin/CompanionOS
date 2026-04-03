#ifndef PAGE_PRODUCTIVITY_H
#define PAGE_PRODUCTIVITY_H
#include "globals.h"

extern uint16_t blendColor(uint16_t c1, uint16_t c2, float t);
extern void drawPageIndicator(int current, int total);

// ═══════════════════════════════════════════════════════════
// PRODUCTIVITY HUB — V6 Premium Clock + Agenda Page
//
// Inspired by reference images: massive red/blue clock digits,
// date label, and a side panel with today's task agenda.
// Color scheme: deep charcoal bg, red/blue clock, cyan accents
// ═══════════════════════════════════════════════════════════

#define PRD_BG       0x0841   // Dark charcoal
#define PRD_CARD     0x1082   // Card surface
#define PRD_ACTIVE   0x18E3   // Active task highlight
#define PRD_RED      0xF800   // Clock red (from reference)
#define PRD_BLUE     0x2A9F   // Accent blue
#define PRD_DIM      0x4A49   // Muted text
#define PRD_DIVIDER  0x2104
#define PRD_GREEN    0x2E8B   // Progress fill

extern int displayHour;
extern int displayMinute;
extern bool timeReceived;

static bool prodPageDrawn = false;
static int lastProdMinute = -1;
static char lastTaskCurrent[32] = "";

void resetProductivityDrawState() {
  prodPageDrawn = false;
  lastProdMinute = -1;
  lastTaskCurrent[0] = 0;
}

// ── Draw massive 7-segment style digit ───────────────────
// Each digit is drawn as filled rectangles for a premium LED look
void drawBigDigit(int x, int y, int digit, int w, int h, uint16_t color) {
  int seg = h / 7;    // Segment thickness
  int gap = 2;        // Gap between segments

  // 7-segment encoding: top, topR, topL, mid, botR, botL, bot
  //                       a     b     f    g     c     e    d
  bool segs[10][7] = {
    {1,1,1,0,1,1,1}, // 0
    {0,1,0,0,1,0,0}, // 1
    {1,1,0,1,0,1,1}, // 2
    {1,1,0,1,1,0,1}, // 3
    {0,1,1,1,1,0,0}, // 4
    {1,0,1,1,1,0,1}, // 5
    {1,0,1,1,1,1,1}, // 6
    {1,1,0,0,1,0,0}, // 7
    {1,1,1,1,1,1,1}, // 8
    {1,1,1,1,1,0,1}, // 9
  };

  if (digit < 0 || digit > 9) return;

  int halfH = (h - seg) / 2;

  // a: top horizontal
  if (segs[digit][0])
    tft.fillRect(x + seg + gap, y, w - 2*seg - 2*gap, seg, color);

  // b: top-right vertical
  if (segs[digit][1])
    tft.fillRect(x + w - seg, y + seg + gap, seg, halfH - seg - gap, color);

  // f: top-left vertical
  if (segs[digit][2])
    tft.fillRect(x, y + seg + gap, seg, halfH - seg - gap, color);

  // g: middle horizontal
  if (segs[digit][3])
    tft.fillRect(x + seg + gap, y + halfH, w - 2*seg - 2*gap, seg, color);

  // c: bottom-right vertical
  if (segs[digit][4])
    tft.fillRect(x + w - seg, y + halfH + seg + gap, seg, halfH - seg - gap, color);

  // e: bottom-left vertical
  if (segs[digit][5])
    tft.fillRect(x, y + halfH + seg + gap, seg, halfH - seg - gap, color);

  // d: bottom horizontal
  if (segs[digit][6])
    tft.fillRect(x + seg + gap, y + h - seg, w - 2*seg - 2*gap, seg, color);
}

// ── Draw the massive clock ───────────────────────────────
void drawProductivityClock(int ox, int oy) {
  int dw = 40;    // Digit width
  int dh = 72;    // Digit height
  int spacing = 4;

  int h = timeReceived ? displayHour : 0;
  int m = timeReceived ? displayMinute : 0;

  // Top row: hours
  drawBigDigit(ox, oy, h / 10, dw, dh, PRD_RED);
  drawBigDigit(ox + dw + spacing, oy, h % 10, dw, dh, PRD_RED);

  // Bottom row: minutes (more muted)
  uint16_t minColor = blendColor(PRD_RED, PRD_BG, 0.3f);
  drawBigDigit(ox, oy + dh + 8, m / 10, dw, dh, minColor);
  drawBigDigit(ox + dw + spacing, oy + dh + 8, m % 10, dw, dh, minColor);
}

// ── Draw task progress bar ───────────────────────────────
void drawTaskProgress(int x, int y, int w, uint8_t pct) {
  tft.fillRect(x, y, w, 3, PRD_DIVIDER);
  int fillW = (int)((long)pct * w / 100);
  if (fillW > 0) {
    tft.fillRect(x, y, fillW, 3, PRD_BLUE);
    // Glow dot at end
    tft.fillCircle(x + fillW, y + 1, 2, TFT_WHITE);
  }
}

// ── Full Page Draw ───────────────────────────────────────
void drawProductivityPage() {
  tft.fillScreen(PRD_BG);

  // ── Header ──
  tft.setTextColor(PRD_RED);
  tft.drawString("Productivity", 10, 2, 2);

  // Time in header (small)
  if (timeReceived) {
    char hdrTime[6];
    sprintf(hdrTime, "%02d:%02d", displayHour, displayMinute);
    tft.setTextColor(TFT_WHITE);
    tft.drawRightString(hdrTime, SCREEN_W - 10, 3, 2);
  }

  tft.drawFastHLine(0, 18, SCREEN_W, PRD_DIVIDER);

  // ═══════════════════════════════════════════════════════
  // LEFT PANE: Massive Clock (0—140)
  // Exactly like reference images: giant red LED digits
  // ═══════════════════════════════════════════════════════

  drawProductivityClock(12, 28);

  // Date label below clock
  // Day of week from basic millis approximation or use stored
  tft.setTextColor(PRD_BLUE);
  tft.drawString("THU", 18, 188, 2);

  tft.setTextColor(PRD_DIM);
  tft.drawString("APR 03", 18, 206, 1);

  // ═══════════════════════════════════════════════════════
  // RIGHT PANE: Agenda (150—310)
  // ═══════════════════════════════════════════════════════

  int rx = 150;

  // Vertical separator
  tft.drawFastVLine(145, 19, SCREEN_H - 35, PRD_DIVIDER);

  // ── NOW label ──
  tft.setTextColor(PRD_BLUE);
  tft.drawString("NOW", rx, 24, 1);

  if (taskCurrent[0]) {
    // Active task card
    tft.fillRoundRect(rx, 36, 162, 58, 4, PRD_ACTIVE);
    tft.drawRoundRect(rx, 36, 162, 58, 4, PRD_BLUE);

    // Progress bar at top of card
    drawTaskProgress(rx + 4, 38, 154, taskProgressPct);

    // Task title
    tft.setTextColor(TFT_WHITE);
    String tTitle = String(taskCurrent);
    tft.drawString(tTitle.substring(0, 20), rx + 8, 46, 2);

    // Task time
    tft.setTextColor(PRD_BLUE);
    tft.drawString(taskCurrentTime, rx + 8, 66, 1);

    // Active dot
    float pulse = (sin(millis() * 0.003f) + 1.0f) * 0.5f;
    uint16_t dotColor = blendColor(PRD_BLUE, TFT_WHITE, pulse);
    tft.fillCircle(rx + 150, 46, 3, dotColor);
  } else {
    tft.fillRoundRect(rx, 36, 162, 58, 4, PRD_CARD);
    tft.setTextColor(PRD_DIM);
    tft.drawCentreString("No active task", rx + 81, 55, 2);
  }

  // ── NEXT label ──
  tft.setTextColor(PRD_DIM);
  tft.drawString("NEXT", rx, 102, 1);
  tft.drawFastHLine(rx, 114, 160, PRD_DIVIDER);

  // Task 1
  if (taskNext1[0]) {
    tft.fillCircle(rx + 6, 127, 2, PRD_BLUE);
    tft.setTextColor(TFT_WHITE);
    tft.drawString(String(taskNext1).substring(0, 18), rx + 14, 120, 2);
    tft.setTextColor(PRD_DIM);
    tft.drawRightString(taskNext1Time, rx + 158, 121, 1);
  }

  // Separator
  tft.drawFastHLine(rx + 14, 140, 146, PRD_DIVIDER);

  // Task 2
  if (taskNext2[0]) {
    tft.fillCircle(rx + 6, 154, 2, PRD_DIM);
    tft.setTextColor(0x8410);
    tft.drawString(String(taskNext2).substring(0, 18), rx + 14, 147, 2);
    tft.setTextColor(PRD_DIM);
    tft.drawRightString(taskNext2Time, rx + 158, 148, 1);
  }

  // ── Minimal day overview at bottom ──
  tft.drawFastHLine(rx, 172, 160, PRD_DIVIDER);
  tft.setTextColor(PRD_DIM);
  tft.drawString("Today", rx, 178, 1);

  // Mini timeline bar
  int timelineY = 192;
  tft.fillRect(rx, timelineY, 160, 4, PRD_DIVIDER);
  // Current time position
  if (timeReceived) {
    int pos = map(displayHour * 60 + displayMinute, 0, 1440, 0, 160);
    tft.fillRect(rx, timelineY, pos, 4, PRD_BLUE);
    tft.fillCircle(rx + pos, timelineY + 2, 3, TFT_WHITE);
  }

  tft.setTextColor(PRD_DIM);
  tft.drawString("6AM", rx, timelineY + 8, 1);
  tft.drawRightString("12AM", rx + 158, timelineY + 8, 1);

  // Page dots
  drawPageIndicator(STATE_PRODUCTIVITY, STATE_COUNT);

  prodPageDrawn = true;
}

// ── Partial Update (clock tick + task progress) ──────────
void redrawProductivityPartial() {
  if (currentState != STATE_PRODUCTIVITY) return;

  // Clock update every minute
  if (displayMinute != lastProdMinute && timeReceived) {
    // Clear clock area and redraw
    tft.fillRect(12, 28, 90, 158, PRD_BG);
    drawProductivityClock(12, 28);
    lastProdMinute = displayMinute;
  }

  // Task update
  if (strcmp(taskCurrent, lastTaskCurrent) != 0) {
    drawProductivityPage();  // Full redraw on task change (infrequent)
    strcpy(lastTaskCurrent, taskCurrent);
  }

  // Active task progress bar (updates per frame for smooth animation)
  if (taskActive && taskCurrent[0]) {
    int rx = 150;
    drawTaskProgress(rx + 4, 38, 154, taskProgressPct);
  }
}

#endif
