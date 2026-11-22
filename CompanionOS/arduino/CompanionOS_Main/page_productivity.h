#ifndef PAGE_PRODUCTIVITY_H
#define PAGE_PRODUCTIVITY_H
#include "globals.h"
#include "ui_components.h"

extern void drawPageIndicator(int current, int total);

#define PRD_BG       0x0841
#define PRD_CARD     0x1082
#define PRD_ACTIVE   0x18E3
#define PRD_RED      0xF800
#define PRD_BLUE     0x2A9F
#define PRD_DIM      0x4A49
#define PRD_DIVIDER  0x2104
#define PRD_GREEN    0x2E8B

static bool prodPageDrawn = false;
static String prdLastContent = "";

void resetProductivityDrawState() {
  prodPageDrawn = false;
  prdLastContent = "";
}

static String prd_contentKey() {
  return String(uiPageGeneration) + "\n" + taskCurrent + "\n" + taskCurrentTime + "\n" + taskNext1 + "\n" +
         taskNext1Time + "\n" + taskNext2 + "\n" + taskNext2Time + "/" +
         String(taskActive) + "/" + String(taskProgressPct) + "/" +
         String(timeReceived) + "/" + String(displayHour) + "/" + String(displayMinute);
}

void drawTaskProgress(int x, int y, int w, uint8_t pct) {
  tft.fillRect(x, y, w, 3, PRD_DIVIDER);
  int fillW = (long)min(100, (int)pct) * w / 100;
  if (fillW > 0) tft.fillRect(x, y, fillW, 3, PRD_BLUE);
}

static void prd_drawNextRow(int x, int y, int w, const char* title, const char* time) {
  int timeW = min(w / 2, (int)tft.textWidth(time, 1));
  drawTruncatedText(x, y, title, w - timeW - (timeW ? 6 : 0), TFT_WHITE, 1, PRD_BG);
  if (timeW > 0) drawTruncatedText(x + w - timeW, y, time, timeW, PRD_DIM, 1, PRD_BG);
}

void drawProductivityPage() {
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  tft.fillRect(0, UI_HEADER_H, SCREEN_W, SCREEN_H - UI_HEADER_H, PRD_BG);
  const bool compact = SCREEN_W <= 200;
  const int x = compact ? 4 : 112;
  const int width = SCREEN_W - x - 4;
  const int y = UI_HEADER_H + 14;
  const int titleFont = compact ? 1 : 2;
  const int lineH = tft.fontHeight(1) + 2;
  const int titleY = y + 12;
  const int titleH = 2 * (tft.fontHeight(titleFont) + 2);
  const int timeY = titleY + titleH + 2;
  const int barY = timeY + lineH + 2;
  const int nextY = barY + 10;
  const int timelineY = SCREEN_H - UI_FOOTER_H - 4;

  drawTruncatedText(4, UI_HEADER_H + 2, "PRODUCTIVITY", SCREEN_W - 44, PRD_RED, 1, PRD_BG);
  char time[12];
  if (timeReceived) snprintf(time, sizeof(time), "%02d:%02d", displayHour, displayMinute);
  else snprintf(time, sizeof(time), "--:--");
  tft.setTextColor(TFT_WHITE, PRD_BG);
  tft.drawRightString(time, SCREEN_W - 4, UI_HEADER_H + 2, 1);
  if (!compact) {
    tft.drawCentreString(time, 52, UI_HEADER_H + 16, 4);
    drawWrappedText(8, UI_HEADER_H + 50, timeReceived ? "Local time" : "No time sync",
                    88, 40, PRD_DIM, 1, PRD_BG);
    tft.drawFastVLine(104, y, timelineY - y, PRD_DIVIDER);
  }

  drawTruncatedText(x, y, "NOW", width - 48, PRD_BLUE, 1, PRD_BG);
  tft.setTextColor(taskActive ? PRD_GREEN : PRD_DIM, PRD_BG);
  tft.drawRightString(taskActive && taskCurrent[0] ? "ACTIVE" : "IDLE", SCREEN_W - 4, y, 1);
  drawWrappedText(x, titleY, taskCurrent[0] ? taskCurrent : "No current task",
                  width, titleH, TFT_WHITE, titleFont, PRD_BG);
  drawTruncatedText(x, timeY, taskCurrent[0] && taskCurrentTime[0] ? taskCurrentTime : "No scheduled time",
                    width, PRD_DIM, 1, PRD_BG);
  drawTaskProgress(x, barY, width, taskCurrent[0] ? taskProgressPct : 0);

  drawTruncatedText(x, nextY, "NEXT", width, PRD_BLUE, 1, PRD_BG);
  int rowY = nextY + 12;
  if (!taskNext1[0] && !taskNext2[0]) {
    drawTruncatedText(x, rowY, "No upcoming tasks", width, PRD_DIM, 1, PRD_BG);
  } else {
    if (taskNext1[0]) {
      prd_drawNextRow(x, rowY, width, taskNext1, taskNext1Time);
      rowY += lineH;
    }
    if (taskNext2[0]) prd_drawNextRow(x, rowY, width, taskNext2, taskNext2Time);
  }

  if (!compact) tft.fillRect(x, timelineY, width, 2, PRD_DIVIDER);
  if (!compact && timeReceived) {
    int pos = constrain((long)(displayHour * 60 + displayMinute) * width / 1440, 0L, (long)width);
    if (pos > 0) tft.fillRect(x, timelineY, pos, 2, PRD_BLUE);
  }
  drawPageIndicator(STATE_PRODUCTIVITY, STATE_COUNT);
  prodPageDrawn = true;
  prdLastContent = prd_contentKey();
}

void redrawProductivityPartial() {
  if (currentState != STATE_PRODUCTIVITY) return;
  if (!prodPageDrawn || prd_contentKey() != prdLastContent) drawProductivityPage();
}

#endif
