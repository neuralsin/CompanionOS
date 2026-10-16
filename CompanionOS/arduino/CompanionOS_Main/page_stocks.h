#ifndef PAGE_STOCKS_H
#define PAGE_STOCKS_H
#include "globals.h"
#include "ui_components.h"

extern void drawPageIndicator(int current, int total);

#define STK_CARD_BG  0x1082
#define STK_BORDER   0x2945
#define STK_GREEN    0x2E8B
#define STK_RED      0xE8C4
#define STK_DIM      0x4A49
#define STK_DIVIDER  0x2104
#define STK_BG       0x0841

static bool stockPageDrawn = false;
static String stkLastContent = "";

void resetStockDrawState() {
  stockPageDrawn = false;
  stkLastContent = "";
}

static String stk_contentKey() {
  String key = String(uiPageGeneration) + "\n" + stockSymbol + "\n" + stockPrice + "\n" + stockDelta + "\n" +
               stockPctChg + "/" + String(stockIsUp);
  const int count = min((int)stockHistoryLen, (int)(sizeof(stockHistory) / sizeof(stockHistory[0])));
  key += "/" + String(count);
  for (int i = 0; i < count; i++) key += "/" + String(stockHistory[i]);
  for (int i = 0; i < 3; i++) {
    key += "\n" + String(wlSymbol[i]) + "/" + wlPrice[i] + "/" + wlDelta[i] + "/" + String(wlIsUp[i]);
  }
  return key;
}

void drawSparkline(int ox, int oy, int w, int h) {
  const int count = min((int)stockHistoryLen, (int)(sizeof(stockHistory) / sizeof(stockHistory[0])));
  if (count < 2) {
    drawTruncatedText(ox + 2, oy + max(0, (h - tft.fontHeight(1)) / 2),
                      "No chart data", w - 4, STK_DIM, 1, STK_BG);
    return;
  }
  if (w < 4 || h < 4) return;
  int32_t low = stockHistory[0], high = stockHistory[0];
  for (int i = 1; i < count; i++) {
    low = min(low, (int32_t)stockHistory[i]);
    high = max(high, (int32_t)stockHistory[i]);
  }
  const int32_t range = max((int32_t)1, high - low);
  const uint16_t color = stockIsUp ? STK_GREEN : STK_RED;
  int lastX = 0, lastY = 0;
  for (int i = 0; i < count; i++) {
    int x = ox + 1 + (long)i * (w - 3) / (count - 1);
    int y = high == low ? oy + h / 2 :
            oy + h - 2 - ((int32_t)stockHistory[i] - low) * (h - 3) / range;
    if (i > 0) tft.drawLine(lastX, lastY, x, y, color);
    lastX = x;
    lastY = y;
  }
  tft.fillCircle(lastX, lastY, 1, TFT_WHITE);
}

void drawStocksPage() {
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  tft.fillRect(0, UI_HEADER_H, SCREEN_W, SCREEN_H - UI_HEADER_H, STK_BG);
  const bool compact = SCREEN_W <= 200;
  const int pad = 4;
  const int width = SCREEN_W - pad * 2;
  const int lineH = tft.fontHeight(1) + 2;
  const int symbolFont = compact ? 1 : 2;
  const int priceFont = compact ? 1 : 4;
  const int symbolY = UI_HEADER_H + 14;
  const int priceY = symbolY + tft.fontHeight(symbolFont) + 2;
  const int deltaY = priceY + tft.fontHeight(priceFont) + 2;
  const int chartY = deltaY + lineH + 2;
  const int watchY = SCREEN_H - UI_FOOTER_H - 4 * lineH;
  const int chartH = watchY - chartY - 4;
  const bool hasQuote = stockPrice[0] && strcmp(stockPrice, "---") != 0;
  const uint16_t priceColor = hasQuote ? (stockIsUp ? STK_GREEN : STK_RED) : STK_DIM;

  drawTruncatedText(pad, UI_HEADER_H + 2, "STOCKS", width, STK_DIM, 1, STK_BG);
  drawTruncatedText(pad, symbolY, stockSymbol[0] ? stockSymbol : "No symbol selected",
                    width, TFT_WHITE, symbolFont, STK_BG);
  drawTruncatedText(pad, priceY, hasQuote ? stockPrice : "No quote data",
                    width, priceColor, priceFont, STK_BG);
  if (hasQuote && (stockDelta[0] || stockPctChg[0])) {
    drawTruncatedText(pad, deltaY, stockDelta, width / 2 - 3, priceColor, 1, STK_BG);
    drawTruncatedText(pad + width / 2 + 3, deltaY, stockPctChg, width / 2 - 3, STK_DIM, 1, STK_BG);
  } else {
    drawTruncatedText(pad, deltaY, "Change unavailable", width, STK_DIM, 1, STK_BG);
  }
  if (chartH >= tft.fontHeight(1)) drawSparkline(pad, chartY, width, chartH);
  drawTruncatedText(pad, watchY, "WATCHLIST", width, STK_DIM, 1, STK_BG);
  int y = watchY + lineH;
  bool anyWatch = false;
  for (int i = 0; i < 3; i++) {
    if (!wlSymbol[i][0]) continue;
    anyWatch = true;
    const bool hasPrice = wlPrice[i][0] && strcmp(wlPrice[i], "---") != 0;
    const char* price = hasPrice ? wlPrice[i] : "--";
    int priceW = min(width / 2, (int)tft.textWidth(price, 1));
    drawTruncatedText(pad, y, wlSymbol[i], width - priceW - 6, TFT_WHITE, 1, STK_BG);
    drawTruncatedText(pad + width - priceW, y, price, priceW,
                      hasPrice ? (wlIsUp[i] ? STK_GREEN : STK_RED) : STK_DIM, 1, STK_BG);
    y += lineH;
  }
  if (!anyWatch) drawTruncatedText(pad, y, "No watchlist symbols", width, STK_DIM, 1, STK_BG);
  drawPageIndicator(STATE_STOCKS, STATE_COUNT);
  stockPageDrawn = true;
  stkLastContent = stk_contentKey();
}

void redrawStocksPartial() {
  if (currentState != STATE_STOCKS) return;
  if (!stockPageDrawn || stk_contentKey() != stkLastContent) drawStocksPage();
}

#endif
