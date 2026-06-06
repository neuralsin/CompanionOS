#ifndef PAGE_STOCKS_H
#define PAGE_STOCKS_H
#include "globals.h"

extern uint16_t blendColor(uint16_t c1, uint16_t c2, float t);
extern void drawPageIndicator(int current, int total);

// ═══════════════════════════════════════════════════════════
// STOCKS DASHBOARD — V6 Premium Financial Page
// ═══════════════════════════════════════════════════════════

#define STK_PAD       10
#define STK_CARD_BG   0x1082
#define STK_BORDER    0x2945
#define STK_GREEN     0x2E8B
#define STK_RED       0xE8C4
#define STK_DIM       0x4A49
#define STK_DIVIDER   0x2104

static char lastStockPrice[12] = "";
static char lastStockDelta[12] = "";
static bool stockPageDrawn = false;

void resetStockDrawState() {
  lastStockPrice[0] = 0;
  lastStockDelta[0] = 0;
  stockPageDrawn = false;
}

void drawSparkline(int ox, int oy, int w, int h) {
  if (stockHistoryLen < 2) {
    tft.setTextColor(STK_DIM);
    tft.drawCentreString("No chart data", ox + w/2, oy + h/2 - 4, 1);
    return;
  }

  int16_t vMin = stockHistory[0], vMax = stockHistory[0];
  for (int i = 1; i < stockHistoryLen; i++) {
    if (stockHistory[i] < vMin) vMin = stockHistory[i];
    if (stockHistory[i] > vMax) vMax = stockHistory[i];
  }
  int16_t range = max((int16_t)1, (int16_t)(vMax - vMin));

  uint16_t lineColor = stockIsUp ? STK_GREEN : STK_RED;
  uint16_t fillColor = stockIsUp ? 0x0341 : 0x4000;

  int step = w / (stockHistoryLen - 1);
  for (int i = 0; i < stockHistoryLen - 1; i++) {
    int x1 = ox + i * step;
    int y1 = oy + h - (int)((long)(stockHistory[i] - vMin) * h / range);
    int x2 = ox + (i + 1) * step;
    int y2 = oy + h - (int)((long)(stockHistory[i+1] - vMin) * h / range);

    for (int fx = x1; fx < x2; fx++) {
      int fy = y1 + (y2 - y1) * (fx - x1) / max(1, x2 - x1);
      if (fy < oy + h) {
        tft.drawFastVLine(fx, fy, oy + h - fy, fillColor);
      }
    }

    tft.drawLine(x1, y1, x2, y2, lineColor);
    tft.drawLine(x1, y1 + 1, x2, y2 + 1, lineColor);
  }

  int lastX = ox + (stockHistoryLen - 1) * step;
  int lastY = oy + h - (int)((long)(stockHistory[stockHistoryLen-1] - vMin) * h / range);
  tft.fillCircle(lastX, lastY, 3, lineColor);
  tft.fillCircle(lastX, lastY, 1, TFT_WHITE);
}

void drawStockArrow(int x, int y, bool up, uint16_t color) {
  if (up) {
    tft.fillTriangle(x, y - 5, x - 4, y + 2, x + 4, y + 2, color);
  } else {
    tft.fillTriangle(x, y + 5, x - 4, y - 2, x + 4, y - 2, color);
  }
}

void drawStocksPage() {
  tft.fillScreen(0x0841);

  tft.setTextColor(STK_DIM);
  tft.drawString("STOCKS", STK_PAD, 3, 1);

  bool marketOpen = true;
  tft.fillCircle(SCREEN_W - 20, 7, 3, marketOpen ? STK_GREEN : STK_RED);
  tft.setTextColor(marketOpen ? STK_GREEN : STK_RED);
  tft.drawRightString(marketOpen ? "OPEN" : "CLOSED", SCREEN_W - 28, 2, 1);

  tft.drawFastHLine(0, 15, SCREEN_W, STK_DIVIDER);

  tft.setTextColor(TFT_WHITE);
  tft.drawString(stockSymbol, STK_PAD, 22, 4);

  uint16_t priceColor = stockIsUp ? STK_GREEN : STK_RED;
  tft.setTextColor(priceColor);
  tft.drawString(stockPrice, STK_PAD, 50, 4);

  int deltaY = 82;
  drawStockArrow(STK_PAD + 6, deltaY + 6, stockIsUp, priceColor);
  tft.setTextColor(priceColor);
  tft.drawString(stockDelta, STK_PAD + 16, deltaY, 2);

  if (stockPctChg[0]) {
    tft.setTextColor(STK_DIM);
    tft.drawString(stockPctChg, STK_PAD + 16, deltaY + 18, 1);
  }

  tft.drawFastHLine(STK_PAD, 118, 145, STK_DIVIDER);
  drawSparkline(STK_PAD, 124, 145, 70);

  tft.setTextColor(STK_DIM);
  tft.drawString("1D", STK_PAD, 198, 1);
  tft.drawRightString("Now", 155, 198, 1);

  int rx = 165;
  tft.drawFastVLine(160, 16, SCREEN_H - 32, STK_DIVIDER);

  tft.setTextColor(STK_DIM);
  tft.drawString("WATCHLIST", rx, 20, 1);
  tft.drawFastHLine(rx, 32, 145, STK_DIVIDER);

  for (int i = 0; i < 3; i++) {
    int wy = 40 + i * 52;
    tft.fillRoundRect(rx, wy, 145, 44, 4, STK_CARD_BG);
    uint16_t wColor = wlIsUp[i] ? STK_GREEN : STK_RED;
    tft.fillRect(rx, wy, 3, 44, wColor);
    tft.setTextColor(TFT_WHITE);
    tft.drawString(wlSymbol[i], rx + 10, wy + 5, 2);
    tft.setTextColor(wColor);
    tft.drawString(wlPrice[i], rx + 10, wy + 24, 2);
    if (wlDelta[i][0]) {
      tft.drawRightString(wlDelta[i], rx + 138, wy + 5, 1);
    }
    drawStockArrow(rx + 134, wy + 28, wlIsUp[i], wColor);
  }

  drawPageIndicator(STATE_STOCKS, STATE_COUNT);
  stockPageDrawn = true;
}

void redrawStocksPartial() {
  if (currentState != STATE_STOCKS) return;

  if (strcmp(stockPrice, lastStockPrice) != 0) {
    uint16_t priceColor = stockIsUp ? STK_GREEN : STK_RED;

    tft.fillRect(STK_PAD, 50, 150, 26, 0x0841);
    tft.setTextColor(priceColor);
    tft.drawString(stockPrice, STK_PAD, 50, 4);

    tft.fillRect(STK_PAD, 82, 150, 24, 0x0841);
    drawStockArrow(STK_PAD + 6, 88, stockIsUp, priceColor);
    tft.setTextColor(priceColor);
    tft.drawString(stockDelta, STK_PAD + 16, 82, 2);

    if (stockPctChg[0]) {
      tft.setTextColor(STK_DIM);
      tft.drawString(stockPctChg, STK_PAD + 16, 100, 1);
    }

    strcpy(lastStockPrice, stockPrice);
    strcpy(lastStockDelta, stockDelta);

    tft.fillRect(STK_PAD, 124, 145, 70, 0x0841);
    drawSparkline(STK_PAD, 124, 145, 70);
  }

  int rx = 165;
  for (int i = 0; i < 3; i++) {
    int wy = 40 + i * 52;
    uint16_t wColor = wlIsUp[i] ? STK_GREEN : STK_RED;
    tft.fillRoundRect(rx, wy, 145, 44, 4, STK_CARD_BG);
    tft.fillRect(rx, wy, 3, 44, wColor);
    tft.setTextColor(TFT_WHITE);
    tft.drawString(wlSymbol[i], rx + 10, wy + 5, 2);
    tft.setTextColor(wColor);
    tft.drawString(wlPrice[i], rx + 10, wy + 24, 2);
    if (wlDelta[i][0]) {
      tft.drawRightString(wlDelta[i], rx + 138, wy + 5, 1);
    }
    drawStockArrow(rx + 134, wy + 28, wlIsUp[i], wColor);
  }
}

#endif
