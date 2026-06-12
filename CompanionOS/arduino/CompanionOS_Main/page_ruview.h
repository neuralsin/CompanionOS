#ifndef PAGE_RUVIEW_H
#define PAGE_RUVIEW_H
#include "globals.h"

extern void drawPageIndicator(int current, int total);
extern void drawStatusBar();

// ═══════════════════════════════════════════════════════════
// RUVIEW CSI PRESENCE PAGE — V8
// WiFi-based presence/motion detection display
// ═══════════════════════════════════════════════════════════

// ── Color Palette ──
#define RV_BG        0x0841   // near-black
#define RV_CARD      0x1082   // card background
#define RV_BORDER    0x2104   // subtle borders
#define RV_DIM       0x4A49   // dim text
#define RV_CYAN      0x07FF   // active/online
#define RV_GREEN     0x2E8B   // occupied
#define RV_RED       0xF800   // motion alert
#define RV_YELLOW    0xFE60   // calibrating
#define RV_ORANGE    0xFC00   // warning
#define RV_PURPLE    0x781F   // header accent
#define RV_WHITE     0xFFFF

// ── Data received from Python bridge ──
bool    rvOccupied      = false;
bool    rvMotion        = false;
float   rvConfidence    = 0.0f;
float   rvVariance      = 0.0f;
int8_t  rvRSSI          = 0;
bool    rvCalibrating   = false;
float   rvPPS           = 0.0f;
int     rvSubcarriers   = 0;
char    rvStatus[24]    = "Offline";
char    rvZoneLabel[16] = "Room";

// Track whether initial full draw has been done
static bool rvPageDrawn = false;
// Previous state for partial redraws
static char rvLastStatus[24] = "";
static float rvLastConfidence = -1.0f;
static float rvLastVariance   = -1.0f;

void resetRuviewDrawState() {
  rvPageDrawn = false;
  memset(rvLastStatus, 0, sizeof(rvLastStatus));
  rvLastConfidence = -1.0f;
  rvLastVariance   = -1.0f;
}

// ── Status icon drawing ──
void drawPresenceIcon(int cx, int cy, bool occupied, bool motion, bool calibrating) {
  if (calibrating) {
    // Pulsing yellow circle for calibration
    uint16_t col = (millis() / 500 % 2 == 0) ? RV_YELLOW : RV_ORANGE;
    tft.fillCircle(cx, cy, 10, col);
    tft.drawCircle(cx, cy, 12, RV_YELLOW);
    tft.setTextColor(RV_BG);
    tft.drawCentreString("C", cx, cy - 4, 1);
  } else if (motion) {
    // Red pulsing circle with motion lines
    tft.fillCircle(cx, cy, 10, RV_RED);
    tft.drawCircle(cx, cy, 12, RV_RED);
    tft.drawCircle(cx, cy, 14, 0x7800);  // dark red ring
    // Motion lines
    tft.drawLine(cx - 16, cy - 6, cx - 20, cy - 10, RV_RED);
    tft.drawLine(cx - 16, cy,     cx - 20, cy,      RV_RED);
    tft.drawLine(cx - 16, cy + 6, cx - 20, cy + 10, RV_RED);
    tft.drawLine(cx + 16, cy - 6, cx + 20, cy - 10, RV_RED);
    tft.drawLine(cx + 16, cy,     cx + 20, cy,      RV_RED);
    tft.drawLine(cx + 16, cy + 6, cx + 20, cy + 10, RV_RED);
  } else if (occupied) {
    // Green solid circle
    tft.fillCircle(cx, cy, 10, RV_GREEN);
    tft.drawCircle(cx, cy, 12, RV_GREEN);
  } else {
    // Dim empty circle
    tft.drawCircle(cx, cy, 10, RV_DIM);
    tft.drawCircle(cx, cy, 12, RV_DIM);
  }
}

// ── Confidence bar ──
void drawConfidenceBar(int x, int y, int w, int h, float confidence) {
  tft.fillRoundRect(x, y, w, h, 2, RV_BORDER);
  int fillW = (int)(w * confidence / 100.0f);
  if (fillW > w) fillW = w;
  if (fillW > 0) {
    uint16_t barColor = RV_GREEN;
    if (confidence > 70) barColor = RV_RED;
    else if (confidence > 40) barColor = RV_YELLOW;
    tft.fillRoundRect(x, y, fillW, h, 2, barColor);
  }
}

// ═══════════════════════════════════════════════════════════
// MAIN DRAW FUNCTION
// ═══════════════════════════════════════════════════════════

void drawRuviewPage() {
  tft.fillScreen(RV_BG);

  int pad = 4;
  int cardW = SCREEN_W - pad * 2;
  char buf[32];

  if (SCREEN_W > 200) {
    // ── LARGE SCREEN (320×240) ──
    // Header
    tft.setTextColor(RV_PURPLE);
    tft.drawString("RUVIEW", 8, 3, 1);
    tft.setTextColor(RV_DIM);
    tft.drawRightString("CSI Presence", SCREEN_W - 8, 3, 1);
    tft.drawFastHLine(0, 15, SCREEN_W, RV_BORDER);

    // Zone label card
    tft.fillRoundRect(8, 20, 150, 90, 5, RV_CARD);
    tft.setTextColor(RV_DIM);
    tft.drawString("ZONE", 16, 25, 1);
    tft.setTextColor(RV_CYAN);
    tft.drawString(rvZoneLabel, 16, 38, 2);

    // Status icon (centered in left card)
    drawPresenceIcon(82, 75, rvOccupied, rvMotion, rvCalibrating);

    // Status text
    uint16_t statusColor = RV_DIM;
    if (rvCalibrating) statusColor = RV_YELLOW;
    else if (rvMotion) statusColor = RV_RED;
    else if (rvOccupied) statusColor = RV_GREEN;
    tft.setTextColor(statusColor);
    tft.drawCentreString(rvStatus, 82, 95, 1);

    // Right card — metrics
    tft.fillRoundRect(166, 20, 146, 90, 5, RV_CARD);
    int ry = 25;
    int rowH = 18;

    tft.setTextColor(RV_DIM);
    tft.drawString("Confidence", 174, ry, 1);
    sprintf(buf, "%.0f%%", rvConfidence);
    tft.setTextColor(RV_WHITE);
    tft.drawRightString(buf, 304, ry, 1);
    ry += rowH;

    drawConfidenceBar(174, ry, 122, 6, rvConfidence);
    ry += 14;

    tft.setTextColor(RV_DIM);
    tft.drawString("Variance", 174, ry, 1);
    sprintf(buf, "%.3f", rvVariance);
    tft.setTextColor(RV_WHITE);
    tft.drawRightString(buf, 304, ry, 1);
    ry += rowH;

    tft.setTextColor(RV_DIM);
    tft.drawString("RSSI", 174, ry, 1);
    sprintf(buf, "%d dBm", rvRSSI);
    tft.setTextColor(rvRSSI >= -60 ? RV_GREEN : (rvRSSI >= -75 ? RV_YELLOW : RV_RED));
    tft.drawRightString(buf, 304, ry, 1);

    // Bottom card — diagnostics
    tft.fillRoundRect(8, 118, 304, 46, 5, RV_CARD);
    int bx = 16, by = 125;

    tft.setTextColor(RV_DIM);
    tft.drawString("PPS", bx, by, 1);
    sprintf(buf, "%.0f", rvPPS);
    tft.setTextColor(RV_CYAN);
    tft.drawString(buf, bx + 30, by, 1);

    tft.setTextColor(RV_DIM);
    tft.drawString("Subcarriers", bx + 80, by, 1);
    sprintf(buf, "%d", rvSubcarriers);
    tft.setTextColor(RV_WHITE);
    tft.drawString(buf, bx + 155, by, 1);

    by += 16;
    tft.setTextColor(RV_DIM);
    tft.drawString("Mode", bx, by, 1);
    tft.setTextColor(RV_CYAN);
    tft.drawString(rvCalibrating ? "Calibrating" : "Active", bx + 35, by, 1);

    // Warning text
    tft.setTextColor(RV_DIM);
    tft.drawCentreString("! Avoid fans/microwaves near CSI node", SCREEN_W / 2, 175, 1);

  } else {
    // ── SMALL SCREEN (160×128): Compact single-column layout ──
    int y = 2;

    // Header
    tft.setTextColor(RV_PURPLE);
    tft.drawString("RUVIEW", pad, y, 1);
    tft.setTextColor(RV_DIM);
    tft.drawRightString("CSI", SCREEN_W - pad, y, 1);
    y += 12;
    tft.drawFastHLine(0, y, SCREEN_W, RV_BORDER);
    y += 4;

    // Zone label + status icon
    tft.fillRoundRect(pad, y, cardW, 38, 3, RV_CARD);

    // Small status icon (left side)
    drawPresenceIcon(pad + 16, y + 19, rvOccupied, rvMotion, rvCalibrating);

    // Zone label + status text (right of icon)
    tft.setTextColor(RV_CYAN);
    tft.drawString(rvZoneLabel, pad + 34, y + 4, 2);

    uint16_t statusColor = RV_DIM;
    if (rvCalibrating) statusColor = RV_YELLOW;
    else if (rvMotion) statusColor = RV_RED;
    else if (rvOccupied) statusColor = RV_GREEN;
    tft.setTextColor(statusColor);
    tft.drawString(rvStatus, pad + 34, y + 22, 1);

    y += 44;

    // Confidence bar
    tft.setTextColor(RV_DIM);
    tft.drawString("Conf", pad + 2, y, 1);
    sprintf(buf, "%.0f%%", rvConfidence);
    tft.setTextColor(RV_WHITE);
    tft.drawRightString(buf, SCREEN_W - pad - 2, y, 1);
    y += 10;

    drawConfidenceBar(pad + 2, y, cardW - 4, 5, rvConfidence);
    y += 10;

    // Metrics card
    tft.fillRoundRect(pad, y, cardW, 42, 3, RV_CARD);
    int iy = y + 4;
    int rowH = 13;

    tft.setTextColor(RV_DIM);
    tft.drawString("Var", pad + 6, iy, 1);
    sprintf(buf, "%.3f", rvVariance);
    tft.setTextColor(RV_WHITE);
    tft.drawRightString(buf, SCREEN_W - pad - 6, iy, 1);
    iy += rowH;

    tft.setTextColor(RV_DIM);
    tft.drawString("RSSI", pad + 6, iy, 1);
    sprintf(buf, "%d dBm", rvRSSI);
    tft.setTextColor(rvRSSI >= -60 ? RV_GREEN : (rvRSSI >= -75 ? RV_YELLOW : RV_RED));
    tft.drawRightString(buf, SCREEN_W - pad - 6, iy, 1);
    iy += rowH;

    tft.setTextColor(RV_DIM);
    tft.drawString("PPS", pad + 6, iy, 1);
    sprintf(buf, "%.0f", rvPPS);
    tft.setTextColor(RV_CYAN);
    tft.drawString(buf, pad + 30, iy, 1);

    tft.setTextColor(RV_DIM);
    tft.drawString("Sub", pad + 60, iy, 1);
    sprintf(buf, "%d", rvSubcarriers);
    tft.setTextColor(RV_WHITE);
    tft.drawRightString(buf, SCREEN_W - pad - 6, iy, 1);
  }

  drawPageIndicator(STATE_RUVIEW, STATE_COUNT);
  rvPageDrawn = true;
  strncpy(rvLastStatus, rvStatus, sizeof(rvLastStatus));
  rvLastConfidence = rvConfidence;
  rvLastVariance = rvVariance;
}

// ── Partial redraw (called from loop) ──
void redrawRuviewPartial() {
  if (currentState != STATE_RUVIEW) return;
  if (!rvPageDrawn) return;

  // Only redraw if state changed
  bool statusChanged = (strcmp(rvStatus, rvLastStatus) != 0);
  bool confChanged = (fabs(rvConfidence - rvLastConfidence) > 1.0f);
  bool varChanged = (fabs(rvVariance - rvLastVariance) > 0.001f);

  if (!statusChanged && !confChanged && !varChanged) return;

  // Full redraw on state change (simpler and more reliable than surgical updates)
  drawRuviewPage();
}

// ── Full draw wrapper ──
void drawRuviewPageFull() {
  resetRuviewDrawState();
  drawRuviewPage();
}

#endif
