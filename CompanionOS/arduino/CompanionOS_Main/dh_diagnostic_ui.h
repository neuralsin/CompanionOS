#ifndef DH_DIAGNOSTIC_UI_H
#define DH_DIAGNOSTIC_UI_H

#include "globals.h"
#include "ui_components.h"

static int dhDiagnosticTop() { return max(16, SCALE_Y(14)) + 4; }
static int dhDiagnosticBottom() { return SCR_H - 12; }

static void dhDiagnosticHeader(const char* title, uint16_t accent) {
  tft.resetViewport();
  tft.setTextSize(1); tft.setTextDatum(TL_DATUM); tft.setTextPadding(0);
  tft.fillScreen(CLR_BG);
  const int height = dhDiagnosticTop() - 4;
  tft.fillRect(0, 0, SCR_W, height, CLR_SURFACE);
  tft.drawFastHLine(0, height - 1, SCR_W, accent);
  drawTruncatedText(4, (height - tft.fontHeight(1)) / 2, title, SCR_W - 8, accent, 1, CLR_SURFACE);
}

static void dhDiagnosticRow(const char* label, const String& value, int y, uint16_t color) {
  const int valueX = SCR_W / 3;
  if (y < dhDiagnosticTop() || y + tft.fontHeight(1) > dhDiagnosticBottom()) return;
  drawTruncatedText(4, y, label, valueX - 8, CLR_TEXT_LO, 1);
  drawTruncatedText(valueX, y, value.c_str(), SCR_W - valueX - 4, color, 1);
}

static void dhDiagnosticFooter(const char* status) {
  const int y = dhDiagnosticBottom();
  tft.fillRect(0, y, SCR_W, SCR_H - y, CLR_BG);
  tft.drawFastHLine(0, y, SCR_W, CLR_BORDER);
  drawTruncatedText(4, y + 3, status, SCR_W - 8, CLR_TEXT_LO, 1);
}

static void dhDiagnosticStatus(const char* title, const char* message) {
  dhDiagnosticHeader(title, CLR_WARNING);
  drawWrappedText(8, dhDiagnosticTop() + 4, message, SCR_W - 16,
                  dhDiagnosticBottom() - dhDiagnosticTop() - 8, CLR_TEXT_HI, 1);
  dhDiagnosticFooter("Unavailable");
}

#endif
