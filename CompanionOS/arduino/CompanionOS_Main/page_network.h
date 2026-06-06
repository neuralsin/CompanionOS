#ifndef PAGE_NETWORK_H
#define PAGE_NETWORK_H
#include "globals.h"
// WiFi.h / ESP8266WiFi.h already included via config.h → globals.h

extern void drawPageIndicator(int current, int total);

// ═══════════════════════════════════════════════════════════
// NETWORK STATS DASHBOARD — V6
// ═══════════════════════════════════════════════════════════

#define NET_BG       0x0841
#define NET_CARD     0x1082
#define NET_BORDER   0x2945
#define NET_CYAN     0x07FF
#define NET_GREEN    0x2E8B
#define NET_YELLOW   0xFE60
#define NET_ORANGE   0xFC00
#define NET_RED      0xF800
#define NET_DIM      0x4A49
#define NET_DIVIDER  0x2104
#define NET_PAD      8

static bool networkPageDrawn = false;
static int lastRSSI = 1;  // impossible initial value to force first redraw

void resetNetworkDrawState() {
  networkPageDrawn = false;
  lastRSSI = 1;
}

// Get signal quality label and color from RSSI
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

// Draw signal strength arc indicator
void drawSignalArc(int cx, int cy, int rssi) {
  int bars = 0;
  if (rssi >= -50) bars = 5;
  else if (rssi >= -60) bars = 4;
  else if (rssi >= -70) bars = 3;
  else if (rssi >= -75) bars = 2;
  else if (rssi >= -85) bars = 1;
  
  uint16_t activeColor = getSignalColor(rssi);
  
  // Draw 5 bars, progressively taller
  for (int i = 0; i < 5; i++) {
    int bx = cx + i * 8;
    int bh = 6 + i * 6;     // heights: 6, 12, 18, 24, 30
    int by = cy + 30 - bh;  // align to bottom
    uint16_t c = (i < bars) ? activeColor : 0x2104;
    tft.fillRoundRect(bx, by, 5, bh, 1, c);
  }
}

// Draw a labeled info row inside a card
void drawNetRow(int x, int y, const char* label, const char* value, uint16_t valColor = TFT_WHITE) {
  tft.setTextColor(NET_DIM);
  tft.drawString(label, x, y, 1);
  tft.setTextColor(valColor);
  tft.drawString(value, x + 55, y, 1);
}

void drawNetworkPage() {
  tft.fillScreen(NET_BG);
  
  // ── Header ──
  tft.setTextColor(NET_DIM);
  tft.drawString("NETWORK", NET_PAD, 3, 1);
  
  // Connection status dot + label
  bool connected = (WiFi.status() == WL_CONNECTED);
  tft.fillCircle(SCREEN_W - 20, 7, 3, connected ? NET_GREEN : NET_RED);
  tft.setTextColor(connected ? NET_GREEN : NET_RED);
  tft.drawRightString(connected ? "ONLINE" : "OFFLINE", SCREEN_W - 28, 2, 1);
  
  tft.drawFastHLine(0, 15, SCREEN_W, NET_DIVIDER);
  
  if (!connected) {
    tft.setTextColor(NET_RED);
    tft.drawCentreString("No WiFi", SCREEN_W / 2, SCREEN_H / 2 - 10, 2);
    drawPageIndicator(STATE_NETWORK, STATE_COUNT);
    networkPageDrawn = true;
    return;
  }
  
  int rssi = WiFi.RSSI();
  
  // ═══════════════════════════════════════════════════════════
  // RESOLUTION-AWARE LAYOUT
  // ═══════════════════════════════════════════════════════════

  if (SCREEN_W > 200) {
    // ── LARGE SCREEN (320×240): Original dual-column layout ──
    int lx = NET_PAD, ly = 20;
    tft.fillRoundRect(lx, ly, 148, 95, 4, NET_CARD);
    drawSignalArc(lx + 10, ly + 8, rssi);
    
    char dbmStr[12];
    sprintf(dbmStr, "%d", rssi);
    tft.setTextColor(getSignalColor(rssi));
    tft.drawString(dbmStr, lx + 55, ly + 10, 4);
    tft.setTextColor(NET_DIM);
    tft.drawString("dBm", lx + 55 + tft.textWidth(dbmStr, 4) + 4, ly + 18, 1);
    
    tft.setTextColor(getSignalColor(rssi));
    tft.drawString(getSignalLabel(rssi), lx + 10, ly + 50, 2);
    
    int pct = min(100, max(0, 2 * (rssi + 100)));
    char pctStr[8]; sprintf(pctStr, "%d%%", pct);
    tft.setTextColor(TFT_WHITE);
    tft.drawRightString(pctStr, lx + 140, ly + 50, 2);
    
    int barW = 130;
    int filledW = barW * pct / 100;
    tft.fillRoundRect(lx + 10, ly + 75, barW, 8, 3, 0x2104);
    tft.fillRoundRect(lx + 10, ly + 75, filledW, 8, 3, getSignalColor(rssi));
    
    // Right panel
    int rx = 164, ry = 20;
    tft.fillRoundRect(rx, ry, 148, 95, 4, NET_CARD);
    
    char buf[32];
    tft.setTextColor(NET_DIM);
    tft.drawString("SSID", rx + 8, ry + 6, 1);
    tft.setTextColor(NET_CYAN);
    String ssid = WiFi.SSID();
    if (ssid.length() > 16) ssid = ssid.substring(0, 16);
    tft.drawString(ssid.c_str(), rx + 8, ry + 18, 2);
    
    tft.drawFastHLine(rx + 8, ry + 36, 132, NET_DIVIDER);
    
    sprintf(buf, "CH %d", WiFi.channel());
    tft.setTextColor(NET_DIM);
    tft.drawString("Channel", rx + 8, ry + 42, 1);
    tft.setTextColor(TFT_WHITE);
    tft.drawRightString(buf, rx + 140, ry + 42, 1);
    
    tft.setTextColor(NET_DIM);
    tft.drawString("BSSID", rx + 8, ry + 56, 1);
    tft.setTextColor(TFT_WHITE);
    String bssid = WiFi.BSSIDstr();
    if (bssid.length() > 8) bssid = bssid.substring(bssid.length() - 8);
    tft.drawRightString(bssid.c_str(), rx + 140, ry + 56, 1);
    
    tft.setTextColor(NET_DIM);
    tft.drawString("Host", rx + 8, ry + 70, 1);
    tft.setTextColor(TFT_WHITE);
    tft.drawRightString("CompanionOS", rx + 140, ry + 70, 1);
    
    sprintf(buf, "%dB", ESP.getFreeHeap());
    tft.setTextColor(NET_DIM);
    tft.drawString("Heap", rx + 8, ry + 82, 1);
    tft.setTextColor(NET_GREEN);
    tft.drawRightString(buf, rx + 140, ry + 82, 1);
    
    // Bottom panel
    int by2 = 122;
    tft.fillRoundRect(NET_PAD, by2, 304, 90, 4, NET_CARD);
    
    int col1x = NET_PAD + 10;
    int col2x = NET_PAD + 160;
    
    tft.setTextColor(NET_DIM);
    tft.drawString("IP Address", col1x, by2 + 8, 1);
    tft.setTextColor(NET_CYAN);
    tft.drawString(WiFi.localIP().toString().c_str(), col1x, by2 + 20, 2);
    
    tft.setTextColor(NET_DIM);
    tft.drawString("Gateway", col2x, by2 + 8, 1);
    tft.setTextColor(TFT_WHITE);
    tft.drawString(WiFi.gatewayIP().toString().c_str(), col2x, by2 + 20, 2);
    
    tft.drawFastHLine(col1x, by2 + 40, 284, NET_DIVIDER);
    
    tft.setTextColor(NET_DIM);
    tft.drawString("Subnet", col1x, by2 + 46, 1);
    tft.setTextColor(TFT_WHITE);
    tft.drawString(WiFi.subnetMask().toString().c_str(), col1x, by2 + 58, 1);
    
    tft.setTextColor(NET_DIM);
    tft.drawString("DNS", col2x, by2 + 46, 1);
    tft.setTextColor(TFT_WHITE);
    tft.drawString(WiFi.dnsIP().toString().c_str(), col2x, by2 + 58, 1);
    
    tft.setTextColor(NET_DIM);
    tft.drawString("MAC", col1x, by2 + 72, 1);
    tft.setTextColor(TFT_WHITE);
    tft.drawString(WiFi.macAddress().c_str(), col1x + 28, by2 + 72, 1);
    
    unsigned long uptimeSec = millis() / 1000;
    int uh = uptimeSec / 3600;
    int um = (uptimeSec % 3600) / 60;
    int us = uptimeSec % 60;
    char uptStr[16];
    sprintf(uptStr, "%02d:%02d:%02d", uh, um, us);
    tft.setTextColor(NET_DIM);
    tft.drawString("Uptime", col2x, by2 + 72, 1);
    tft.setTextColor(NET_GREEN);
    tft.drawRightString(uptStr, col2x + 130, by2 + 72, 1);

  } else {
    // ── SMALL SCREEN (160×128): Single-column compact layout ──
    int y = 20;
    int pad = 4;
    int cardW = SCREEN_W - pad * 2;
    char buf[32];
    
    // Signal hero row
    tft.fillRoundRect(pad, y, cardW, 25, 3, NET_CARD);
    // Draw smaller signal arc
    int bars = 0;
    if (rssi >= -50) bars = 5;
    else if (rssi >= -60) bars = 4;
    else if (rssi >= -70) bars = 3;
    else if (rssi >= -75) bars = 2;
    else if (rssi >= -85) bars = 1;
    uint16_t activeColor = getSignalColor(rssi);
    for (int i = 0; i < 5; i++) {
      int bx = pad + 6 + i * 6;
      int bh = 4 + i * 4;     // heights: 4, 8, 12, 16, 20
      int by = y + 22 - bh;
      uint16_t c = (i < bars) ? activeColor : 0x2104;
      tft.fillRoundRect(bx, by, 4, bh, 1, c);
    }
    
    char dbmStr[12]; sprintf(dbmStr, "%d dBm", rssi);
    tft.setTextColor(activeColor);
    tft.drawString(dbmStr, pad + 45, y + 5, 2);
    
    tft.setTextColor(activeColor);
    tft.drawRightString(getSignalLabel(rssi), SCREEN_W - pad - 6, y + 5, 2);
    
    y += 35; // 25px card + 10px spacing (min 15px from text baseline)
    
    // Info rows
    tft.fillRoundRect(pad, y, cardW, 70, 3, NET_CARD);
    int ix = pad + 6;
    int iy = y + 5;
    int rowH = 15; // 8px font + 7px spacing = 15px (matches requirement)
    
    // SSID
    tft.setTextColor(NET_DIM);
    tft.drawString("SSID", ix, iy, 1);
    tft.setTextColor(NET_CYAN);
    String ssid = WiFi.SSID();
    if (ssid.length() > 12) ssid = ssid.substring(0, 12);
    tft.drawRightString(ssid.c_str(), SCREEN_W - pad - 6, iy, 1);
    iy += rowH;
    
    // IP
    tft.setTextColor(NET_DIM);
    tft.drawString("IP", ix, iy, 1);
    tft.setTextColor(TFT_WHITE);
    tft.drawRightString(WiFi.localIP().toString().c_str(), SCREEN_W - pad - 6, iy, 1);
    iy += rowH;
    
    // Channel & Heap (combined row)
    sprintf(buf, "CH %d", WiFi.channel());
    tft.setTextColor(NET_DIM);
    tft.drawString(buf, ix, iy, 1);
    
    sprintf(buf, "%dB", ESP.getFreeHeap());
    tft.setTextColor(NET_GREEN);
    tft.drawRightString(buf, SCREEN_W - pad - 6, iy, 1);
    iy += rowH;
    
    // Uptime
    unsigned long uptimeSec = millis() / 1000;
    sprintf(buf, "%02d:%02d:%02d", (int)(uptimeSec/3600), (int)((uptimeSec%3600)/60), (int)(uptimeSec%60));
    tft.setTextColor(NET_DIM);
    tft.drawString("Up", ix, iy, 1);
    tft.setTextColor(NET_GREEN);
    tft.drawRightString(buf, SCREEN_W - pad - 6, iy, 1);
  }
  
  drawPageIndicator(STATE_NETWORK, STATE_COUNT);
  networkPageDrawn = true;
}

void redrawNetworkPartial() {
  if (currentState != STATE_NETWORK) return;
  
  int rssi = WiFi.RSSI();
  
  // Only redraw if signal changed by at least 2 dBm
  if (abs(rssi - lastRSSI) < 2) return;
  lastRSSI = rssi;
  
  int lx = NET_PAD, ly = 20;
  
  // Redraw signal bars
  tft.fillRect(lx + 10, ly + 8, 40, 32, NET_CARD);
  drawSignalArc(lx + 10, ly + 8, rssi);
  
  // Redraw dBm
  tft.fillRect(lx + 55, ly + 10, 90, 28, NET_CARD);
  char dbmStr[12];
  sprintf(dbmStr, "%d", rssi);
  tft.setTextColor(getSignalColor(rssi));
  tft.drawString(dbmStr, lx + 55, ly + 10, 4);
  tft.setTextColor(NET_DIM);
  tft.drawString("dBm", lx + 55 + tft.textWidth(dbmStr, 4) + 4, ly + 18, 1);
  
  // Redraw quality label
  tft.fillRect(lx + 10, ly + 50, 130, 18, NET_CARD);
  tft.setTextColor(getSignalColor(rssi));
  tft.drawString(getSignalLabel(rssi), lx + 10, ly + 50, 2);
  
  int pct = min(100, max(0, 2 * (rssi + 100)));
  char pctStr[8];
  sprintf(pctStr, "%d%%", pct);
  tft.setTextColor(TFT_WHITE);
  tft.drawRightString(pctStr, lx + 140, ly + 50, 2);
  
  // Redraw signal bar
  int barW = 130;
  int filledW = barW * pct / 100;
  tft.fillRoundRect(lx + 10, ly + 75, barW, 8, 3, 0x2104);
  tft.fillRoundRect(lx + 10, ly + 75, filledW, 8, 3, getSignalColor(rssi));
  
  // Redraw uptime
  int col2x = NET_PAD + 160;
  int by2 = 122;
  unsigned long uptimeSec = millis() / 1000;
  int uh = uptimeSec / 3600;
  int um = (uptimeSec % 3600) / 60;
  int us = uptimeSec % 60;
  char uptStr[16];
  sprintf(uptStr, "%02d:%02d:%02d", uh, um, us);
  tft.fillRect(col2x + 50, by2 + 72, 80, 12, NET_CARD);
  tft.setTextColor(NET_GREEN);
  tft.drawRightString(uptStr, col2x + 130, by2 + 72, 1);
  
  // Redraw free heap
  int rx = 164, ry = 20;
  char buf[16];
  sprintf(buf, "%dB", ESP.getFreeHeap());
  tft.fillRect(rx + 80, ry + 82, 60, 12, NET_CARD);
  tft.setTextColor(NET_GREEN);
  tft.drawRightString(buf, rx + 140, ry + 82, 1);
}

#endif
