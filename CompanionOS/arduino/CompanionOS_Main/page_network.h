#ifndef PAGE_NETWORK_H
#define PAGE_NETWORK_H
#include "globals.h"
#include "ui_components.h"

extern void drawPageIndicator(int current, int total);

#define NET_BG       0x0841
#define NET_CARD     0x1082
#define NET_DIM      0x4A49
#define NET_DIVIDER  0x2104
#define NET_GREEN    0x2E8B
#define NET_CYAN     0x07DF
#define NET_YELLOW   0xFFE0
#define NET_ORANGE   0xFC00
#define NET_RED      0xF800

static bool networkPageDrawn = false;

void resetNetworkDrawState() {
  networkPageDrawn = false;
}

const char* getSignalLabel(int rssi) {
  if (rssi >= -50) return "Excellent";
  if (rssi >= -60) return "Very Good";
  if (rssi >= -70) return "Good";
  if (rssi >= -75) return "Average";
  if (rssi >= -80) return "Poor";
  if (rssi >= -90) return "Weak";
  return "Very Weak";
}

uint16_t getSignalColor(int rssi) {
  if (rssi >= -50) return NET_GREEN;
  if (rssi >= -60) return NET_CYAN;
  if (rssi >= -70) return NET_GREEN;
  if (rssi >= -75) return NET_YELLOW;
  if (rssi >= -80) return NET_ORANGE;
  return NET_RED;
}

static void net_drawRow(int x, int y, int w, const char* label, const char* value,
                        uint16_t color = TFT_WHITE) {
  const int labelW = SCREEN_W <= 200 ? 36 : 72;
  drawTruncatedText(x, y, label, labelW, NET_DIM, 1, NET_BG);
  drawTruncatedText(x + labelW + 6, y, value, w - labelW - 6, color, 1, NET_BG);
}

void drawNetworkPage() {
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  tft.fillRect(0, UI_HEADER_H, SCREEN_W, SCREEN_H - UI_HEADER_H, NET_BG);
  const int pad = 4;
  const int width = SCREEN_W - pad * 2;
  const bool connected = WiFi.status() == WL_CONNECTED;
  drawTruncatedText(pad, UI_HEADER_H + 2, "NETWORK", width - 48, NET_CYAN, 1, NET_BG);
  tft.setTextColor(connected ? NET_GREEN : NET_RED, NET_BG);
  tft.drawRightString(connected ? "ONLINE" : "OFFLINE", SCREEN_W - pad, UI_HEADER_H + 2, 1);

  if (!connected) {
    drawWrappedText(8, UI_HEADER_H + 20, "WiFi disconnected", SCREEN_W - 16,
                    SCREEN_H - UI_HEADER_H - UI_FOOTER_H - 24, NET_DIM, 1, NET_BG);
  } else {
    const int rssi = WiFi.RSSI();
    const uint16_t signalColor = getSignalColor(rssi);
    char value[32];
    snprintf(value, sizeof(value), "%d dBm", rssi);
    drawTruncatedText(pad, UI_HEADER_H + 16, value, SCREEN_W / 2 - 8, signalColor, 2, NET_BG);
    drawTruncatedText(SCREEN_W / 2 + 4, UI_HEADER_H + 20, getSignalLabel(rssi),
                      SCREEN_W / 2 - 8, signalColor, 1, NET_BG);
    const int barY = UI_HEADER_H + 38;
    const int pct = constrain(2 * (rssi + 100), 0, 100);
    tft.fillRect(pad, barY, width, 3, NET_DIVIDER);
    if (pct > 0) tft.fillRect(pad, barY, (long)width * pct / 100, 3, signalColor);

    int y = UI_HEADER_H + 48;
    const int rowH = SCREEN_W <= 200 ? 10 : 18;
    String ssid = WiFi.SSID();
    net_drawRow(pad, y, width, "SSID", ssid.length() ? ssid.c_str() : "Unavailable", NET_CYAN);
    y += rowH;
    net_drawRow(pad, y, width, "IP", WiFi.localIP().toString().c_str());
    y += rowH;
    if (SCREEN_W > 200) {
      net_drawRow(pad, y, width, "Gateway", WiFi.gatewayIP().toString().c_str());
      y += rowH;
      net_drawRow(pad, y, width, "Subnet", WiFi.subnetMask().toString().c_str());
      y += rowH;
      net_drawRow(pad, y, width, "DNS", WiFi.dnsIP().toString().c_str());
      y += rowH;
      net_drawRow(pad, y, width, "MAC", WiFi.macAddress().c_str());
      y += rowH;
    }
    snprintf(value, sizeof(value), "%d", WiFi.channel());
    net_drawRow(pad, y, width, "CH", value);
    y += rowH;
    snprintf(value, sizeof(value), "%lu B", (unsigned long)ESP.getFreeHeap());
    net_drawRow(pad, y, width, "HEAP", value, NET_GREEN);
    y += rowH;
    unsigned long sec = millis() / 1000;
    snprintf(value, sizeof(value), "%luh %02lum %02lus", sec / 3600, (sec % 3600) / 60, sec % 60);
    net_drawRow(pad, y, width, "UP", value, NET_GREEN);
  }
  drawPageIndicator(STATE_NETWORK, STATE_COUNT);
  networkPageDrawn = true;
}

void redrawNetworkPartial() {
  if (currentState != STATE_NETWORK) return;
  // Connection, addressing and uptime can change without an RSSI change.
  drawNetworkPage();
}

#endif
