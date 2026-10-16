#ifndef PAGE_PC_DASH_H
#define PAGE_PC_DASH_H

#include "globals.h"
#include "ui_components.h"
#include <ArduinoJson.h>
#include <math.h>
#include <limits.h>
#include <ctype.h>

extern void drawPageIndicator(int current, int total);

// ═══════════════════════════════════════════════════════════
// WIRELESS PC DASHBOARD TILE (Ported from innocircuitslab)
// Adapted for CompanionOS (Dual-resolution: 160x128 & 320x240)
// Includes CPU, RAM, GPU, VRAM, Laptop/GPU Temp, Disk, Net & History
// ═══════════════════════════════════════════════════════════

// --- Color Palette (Matching innocircuitslab aesthetic) ---
#define PCD_BG       0x0000 // ILI9341_BLACK
#define PCD_TEXT     0xFFFF // ILI9341_WHITE
#define PCD_ACCENT   0x07FF // ILI9341_CYAN
#define PCD_GREEN    0x07E0 // ILI9341_GREEN
#define PCD_YELLOW   0xFFE0 // ILI9341_YELLOW
#define PCD_RED      0xF800 // ILI9341_RED
#define PCD_GREY     0x8410 // Neutral grey
#define PCD_DARK     0x2104 // Dark card border
#define PCD_HEADER   0x0010 // Dark navy header bar

// Font size adaptors
#define PCD_SZ_TITLE (SCREEN_W >= 240 ? 2 : 1)
#define PCD_SZ_VAL   (SCREEN_W >= 240 ? 2 : 1)
#define PCD_SZ_SM    1

// --- Telemetry State Variables ---
static float pcd_cpu = NAN;
static float pcd_ram = NAN;
static float pcd_downMB = NAN;
static float pcd_upMB = NAN;
static unsigned long pcd_uptime = 0;
static bool pcd_hasUptime = false;
static float pcd_cpuFreq = NAN;
static float pcd_diskReadMB = NAN;
static float pcd_diskWriteMB = NAN;
static int   pcd_procCount = -1;
static float pcd_rxGB = NAN;
static float pcd_txGB = NAN;
static float pcd_latency = NAN;
static float pcd_diskFree = NAN;
static float pcd_topCpuPct = NAN;
static char  pcd_topCpuName[40] = "N/A";
static float pcd_topRamMB = NAN;
static char  pcd_topRamName[40] = "N/A";

// GPU & Temperature Extensions (User requested)
static float pcd_gpu = NAN;          // GPU utilization %
static float pcd_vramPct = NAN;      // VRAM utilization %
static float pcd_vramUsedMB = NAN;   // VRAM used MB
static float pcd_vramTotalMB = NAN;  // VRAM total MB
static float pcd_gpuTemp = NAN;      // GPU temperature °C
static float pcd_laptopTemp = NAN;   // Laptop / CPU temperature °C

// CPU History Graph Buffer (52 samples)
#define PCD_HISTORY_SIZE 52
static float pcd_cpuHistory[PCD_HISTORY_SIZE] = {0};
static int   pcd_historyCount = 0;

// Sub-pages (0: Overview, 1: Performance, 2: Network, 3: Health)
static int  pcd_subPage = 0;
const int   PCD_SUBPAGE_COUNT = 4;
static bool pcd_drawn = false;
static bool pcd_offline = true;
static bool pcd_offlineShown = false;
static bool pcd_hasPacket = false;
static bool pcd_dirty = true;
static unsigned long pcd_lastPacketMs = 0;
static unsigned long pcd_lastDrawMs = 0;
const unsigned long PCD_TIMEOUT_MS = 6000;
const unsigned long PCD_REDRAW_MS = 200;

// --- Health Status Determination ---
enum PcHealthLevel {
  PCD_HEALTH_OK = 0,
  PCD_HEALTH_WARN = 1,
  PCD_HEALTH_ALERT = 2,
  PCD_HEALTH_UNKNOWN = 3
};

static inline PcHealthLevel pcd_getHealthLevel() {
  if (pcd_latency < 0 || pcd_cpu >= 95 || pcd_ram >= 95 || pcd_gpu >= 95 || pcd_diskFree < 5 || (pcd_gpuTemp >= 88)) {
    return PCD_HEALTH_ALERT;
  }
  if (pcd_cpu >= 85 || pcd_ram >= 85 || pcd_gpu >= 85 || pcd_diskFree < 15 || pcd_latency > 150 || (pcd_gpuTemp >= 78)) {
    return PCD_HEALTH_WARN;
  }
  if (!isfinite(pcd_cpu) || !isfinite(pcd_ram) ||
      !isfinite(pcd_diskFree) || !isfinite(pcd_latency)) return PCD_HEALTH_UNKNOWN;
  return PCD_HEALTH_OK;
}

static inline uint16_t pcd_healthColor(PcHealthLevel level) {
  if (level == PCD_HEALTH_ALERT) return PCD_RED;
  if (level == PCD_HEALTH_WARN)  return PCD_YELLOW;
  if (level == PCD_HEALTH_UNKNOWN) return PCD_GREY;
  return PCD_GREEN;
}

static inline const char* pcd_healthText(PcHealthLevel level) {
  if (level == PCD_HEALTH_ALERT) return "ALERT";
  if (level == PCD_HEALTH_WARN)  return "WARN";
  if (level == PCD_HEALTH_UNKNOWN) return "N/A";
  return "OK";
}

// Reset drawing state upon page transition
void resetPcDashDrawState() {
  pcd_drawn = false;
  pcd_offlineShown = false;
  pcd_dirty = true;
}

// Subpage cycling (called on Select button short-press or Touch tap)
void nextPcDashSubPage() {
  pcd_subPage = (pcd_subPage + 1) % PCD_SUBPAGE_COUNT;
  pcd_drawn = false;
  extern void renderCurrentPage();
  if (currentState == STATE_PC_DASH) {
    renderCurrentPage();
  }
}

// History push
static void pcd_addCpuHistory(float val) {
  if (!isfinite(val) || val < 0 || val > 100) return;
  if (pcd_historyCount < PCD_HISTORY_SIZE) {
    pcd_cpuHistory[pcd_historyCount++] = val;
  } else {
    for (int i = 0; i < PCD_HISTORY_SIZE - 1; i++) {
      pcd_cpuHistory[i] = pcd_cpuHistory[i + 1];
    }
    pcd_cpuHistory[PCD_HISTORY_SIZE - 1] = val;
  }
}

// --- Text & Bar Helpers ---
static String pcd_formatUptime(unsigned long sec) {
  unsigned long days = sec / 86400UL;
  sec %= 86400UL;
  unsigned long hours = sec / 3600UL;
  sec %= 3600UL;
  unsigned long minutes = sec / 60UL;
  char buf[32];
  if (days > 0) snprintf(buf, sizeof(buf), "%lud %02lu:%02lu", days, hours, minutes);
  else snprintf(buf, sizeof(buf), "%02lu:%02lu", hours, minutes);
  return String(buf);
}

// Always measure at the actual text size; SCALE_Y does not scale font pixels.
static void pcd_text(int x, int y, int w, int h, const char* text,
                     uint16_t color, int size = 1, bool right = false,
                     uint16_t bg = PCD_BG, bool center = false) {
  w = min(w, SCR_W - x);
  h = min(h, SCR_H - y);
  if (w <= 0 || h <= 0) return;
  tft.setTextSize(size);
  while (size > 1 && (tft.fontHeight(1) > h || tft.textWidth(text, 1) > w))
    tft.setTextSize(--size);
  if (tft.fontHeight(1) > h) return;
  tft.setTextDatum(TL_DATUM);
  tft.setTextPadding(0);
  String fitted = fitUiText(text, w, 1);
  if (right) x += w - tft.textWidth(fitted, 1);
  else if (center) x += (w - tft.textWidth(fitted, 1)) / 2;
  drawTruncatedText(x, y, fitted.c_str(), w, color, 1, bg);
}

static void pcd_value(int x, int y, int w, int h, const char* text,
                      uint16_t color, int size = PCD_SZ_VAL) {
  tft.fillRect(x, y, w, h, PCD_BG);
  pcd_text(x, y, w, h, text, color, size);
}

static void pcd_formatMetric(char* buf, size_t len, float value, const char* format) {
  if (isfinite(value)) snprintf(buf, len, format, value);
  else snprintf(buf, len, "N/A");
}

static void pcd_drawSubPageIndicator(const char* text) {
  tft.setTextSize(1);
  int h = tft.fontHeight(1);
  pcd_text(SCR_W - SCALE_X(60), SCR_H - max(SCALE_Y(16), h + 2),
           SCALE_X(52), h, text, PCD_GREY, 1, true);
  drawPageIndicator(STATE_PC_DASH, STATE_COUNT);
}

static void pcd_drawBar(int x, int y, int w, int h, float val, uint16_t color) {
  if (!isfinite(val)) val = 0;
  val = constrain(val, 0.0f, 100.0f);
  tft.drawRect(x, y, w, h, PCD_GREY);
  int inset = h < 6 ? 1 : 2;
  int innerW = max(0, w - 2 * inset);
  int innerH = max(0, h - 2 * inset);
  int fillW = (int)(innerW * (val / 100.0f));
  tft.fillRect(x + inset, y + inset, innerW, innerH, PCD_BG);
  if (fillW > 0) {
    tft.fillRect(x + inset, y + inset, fillW, innerH, color);
  }
}

static void pcd_drawHeader(const char* title, const char* pageInd) {
  int hdrH = max(16, SCALE_Y(30));
  tft.fillRect(0, 0, SCR_W, hdrH, PCD_HEADER);

  (void)pageInd;
  PcHealthLevel level = pcd_offline ? PCD_HEALTH_UNKNOWN : pcd_getHealthLevel();
  uint16_t c = pcd_healthColor(level);

  tft.setTextSize(1);
  int margin = max(4, SCALE_X(8));
  int statusW = tft.textWidth("ALERT", 1);
  int radius = SCALE_MIN(4);
  int statusX = SCR_W - margin - statusW;
  int dotX = statusX - radius - 4;
  int dotY = hdrH / 2;
  tft.fillCircle(dotX, dotY, radius, c);
  pcd_text(statusX, (hdrH - tft.fontHeight(1)) / 2, statusW, hdrH,
           pcd_healthText(level), c, 1, true, PCD_HEADER);
  tft.setTextSize(PCD_SZ_TITLE);
  pcd_text(margin, (hdrH - tft.fontHeight(1)) / 2,
           dotX - radius - 4 - margin, hdrH, title, PCD_TEXT,
           PCD_SZ_TITLE, false, PCD_HEADER);
}

// ═══════════════════════════════════════════════════════════
// OFFLINE SCREEN
// ═══════════════════════════════════════════════════════════
static void pcd_drawOfflineScreen() {
  tft.fillScreen(PCD_BG);
  pcd_drawHeader("PC DASHBOARD", "1/4");

  pcd_text(SCALE_X(10), SCALE_Y(70), SCR_W - SCALE_X(20), SCALE_Y(40),
           "OFFLINE", PCD_RED, SCREEN_W >= 240 ? 4 : 2, false, PCD_BG, true);
  pcd_text(SCALE_X(10), SCALE_Y(115), SCR_W - SCALE_X(20), SCALE_Y(24),
           "Waiting for PC...", PCD_GREY, PCD_SZ_VAL, false, PCD_BG, true);

  drawPageIndicator(STATE_PC_DASH, STATE_COUNT);
  pcd_offlineShown = true;
}

// ═══════════════════════════════════════════════════════════
// SUBPAGE 1: OVERVIEW (Main with GPU & VRAM & Laptop Temp)
// ═══════════════════════════════════════════════════════════
static void pcd_drawOverviewStatic() {
  tft.fillScreen(PCD_BG);
  pcd_drawHeader("PC DASHBOARD", "1/4");
  const char* labels[] = {"CPU", "RAM", "GPU", "VRAM"};
  for (int i = 0; i < 4; ++i)
    pcd_text(SCALE_X(10), SCALE_Y(32 + i * 30), max(26, SCALE_X(48)),
             8 * PCD_SZ_VAL, labels[i], PCD_TEXT);
  int colW = SCR_W / 2 - SCALE_X(15);
  pcd_text(SCALE_X(10), SCALE_Y(156), colW, 8, "DOWNLOAD", PCD_GREY);
  pcd_text(SCALE_X(165), SCALE_Y(156), colW, 8, "UPLOAD", PCD_GREY);
  pcd_text(SCALE_X(10), SCALE_Y(190), colW, 8, "UPTIME", PCD_GREY);
  pcd_text(SCALE_X(165), SCALE_Y(190), colW, 8, "LAPTOP TEMP", PCD_GREY);
  pcd_drawSubPageIndicator("1/4");
}

static void pcd_updateOverviewDynamic() {
  uint16_t cpuCol = (pcd_cpu >= 95) ? PCD_RED : (pcd_cpu >= 85) ? PCD_YELLOW : PCD_ACCENT;
  uint16_t ramCol = (pcd_ram >= 95) ? PCD_RED : (pcd_ram >= 85) ? PCD_YELLOW : PCD_ACCENT;
  uint16_t gpuCol = (pcd_gpu >= 90) ? PCD_RED : (pcd_gpu >= 80) ? PCD_YELLOW : PCD_GREEN;
  uint16_t vramCol = (pcd_vramPct >= 90) ? PCD_RED : (pcd_vramPct >= 80) ? PCD_YELLOW : PCD_ACCENT;

  char buf[32];
  float values[] = {pcd_cpu, pcd_ram, pcd_gpu, pcd_vramPct};
  uint16_t colors[] = {cpuCol, ramCol, gpuCol, vramCol};
  tft.setTextSize(PCD_SZ_VAL);
  int lineH = tft.fontHeight(1);
  int valueX = SCALE_X(10) + max(26, SCALE_X(48)) + 4;
  int valueW = SCR_W - SCALE_X(10) - valueX;
  for (int i = 0; i < 4; ++i) {
    int y = SCALE_Y(32 + i * 30);
    pcd_formatMetric(buf, sizeof(buf), values[i], "%.0f%%");
    if (i == 2 && isfinite(pcd_gpu) && isfinite(pcd_gpuTemp))
      snprintf(buf, sizeof(buf), "%.0f%% (%.0fC)", pcd_gpu, pcd_gpuTemp);
    if (i == 3 && isfinite(pcd_vramPct) && isfinite(pcd_vramUsedMB) && pcd_vramTotalMB > 0) {
      if (pcd_vramTotalMB >= 1024)
        snprintf(buf, sizeof(buf), "%.1fG/%.1fG (%.0f%%)", pcd_vramUsedMB / 1024, pcd_vramTotalMB / 1024, pcd_vramPct);
      else
        snprintf(buf, sizeof(buf), "%.0fM/%.0fM (%.0f%%)", pcd_vramUsedMB, pcd_vramTotalMB, pcd_vramPct);
    }
    tft.fillRect(valueX, y, valueW, lineH, PCD_BG);
    pcd_text(valueX, y, valueW, lineH, buf, isfinite(values[i]) ? colors[i] : PCD_GREY, PCD_SZ_VAL, true);
    pcd_drawBar(SCALE_X(10), y + lineH + 1, SCR_W - SCALE_X(20),
                max(4, SCALE_Y(8)), values[i], colors[i]);
  }
  int colW = SCR_W / 2 - SCALE_X(15);
  int rateY = max(SCALE_Y(168), SCALE_Y(156) + 9);
  pcd_formatMetric(buf, sizeof(buf), pcd_downMB, "%.2f MB/s");
  pcd_value(SCALE_X(10), rateY, colW, lineH, buf, PCD_GREEN);
  pcd_formatMetric(buf, sizeof(buf), pcd_upMB, "%.2f MB/s");
  pcd_value(SCALE_X(165), rateY, colW, lineH, buf, PCD_GREEN);
  int infoY = max(SCALE_Y(204), SCALE_Y(190) + 9);
  pcd_value(SCALE_X(10), infoY, colW, lineH,
            pcd_hasUptime ? pcd_formatUptime(pcd_uptime).c_str() : "N/A", PCD_TEXT);
  pcd_formatMetric(buf, sizeof(buf), pcd_laptopTemp, "%.0f C");
  pcd_value(SCALE_X(165), infoY, colW, lineH, buf, PCD_TEXT);
}

// ═══════════════════════════════════════════════════════════
// SUBPAGE 2: PERFORMANCE (Frequency, Procs, Disk, CPU History)
// ═══════════════════════════════════════════════════════════
static void pcd_drawPerformanceStatic() {
  tft.fillScreen(PCD_BG);
  pcd_drawHeader("PERFORMANCE", "2/4");

  int colW = SCR_W / 2 - SCALE_X(15);
  pcd_text(SCALE_X(10), SCALE_Y(36), colW, 8, "CPU FREQUENCY", PCD_GREY);
  pcd_text(SCALE_X(165), SCALE_Y(36), colW, 8, "PROCESSES", PCD_GREY);
  pcd_text(SCALE_X(10), SCALE_Y(72), colW, 8, "DISK READ", PCD_GREY);
  pcd_text(SCALE_X(165), SCALE_Y(72), colW, 8, "DISK WRITE", PCD_GREY);
  pcd_text(SCALE_X(10), SCALE_Y(108), SCR_W - SCALE_X(20), 8, "CPU HISTORY GRAPH", PCD_GREY);

  // History graph box
  int gx = SCALE_X(10);
  int gy = max(SCALE_Y(122), SCALE_Y(108) + 9);
  int gw = SCR_W - SCALE_X(20);
  int gh = SCALE_Y(68);
  tft.drawRect(gx, gy, gw, gh, PCD_GREY);
  tft.drawFastHLine(gx + 1, gy + gh / 2, gw - 2, PCD_DARK);

  pcd_drawSubPageIndicator("2/4");
}

static void pcd_drawCpuHistoryGraph() {
  int gx = SCALE_X(10);
  int gy = max(SCALE_Y(122), SCALE_Y(108) + 9);
  int gw = SCR_W - SCALE_X(20);
  int gh = SCALE_Y(68);

  tft.fillRect(gx + 1, gy + 1, gw - 2, gh - 2, PCD_BG);
  tft.drawFastHLine(gx + 1, gy + gh / 2, gw - 2, PCD_DARK);

  if (pcd_historyCount < 2) return;

  float step = (float)(gw - 4) / (float)(PCD_HISTORY_SIZE - 1);
  for (int i = 1; i < pcd_historyCount; i++) {
    int x1 = gx + 2 + (int)((i - 1) * step);
    int x2 = gx + 2 + (int)(i * step);
    int y1 = gy + gh - 3 - (int)((pcd_cpuHistory[i - 1] / 100.0f) * (gh - 6));
    int y2 = gy + gh - 3 - (int)((pcd_cpuHistory[i] / 100.0f) * (gh - 6));
    tft.drawLine(x1, y1, x2, y2, PCD_GREEN);
  }
}

static void pcd_updatePerformanceDynamic() {
  char buf[32];
  int colW = (SCR_W / 2) - SCALE_X(15);
  int y1 = max(SCALE_Y(48), SCALE_Y(36) + 9);
  int y2 = max(SCALE_Y(84), SCALE_Y(72) + 9);
  int h = 8 * PCD_SZ_VAL;
  pcd_formatMetric(buf, sizeof(buf), pcd_cpuFreq / 1000.0f, "%.2f GHz");
  pcd_value(SCALE_X(10), y1, colW, h, buf, PCD_ACCENT);
  if (pcd_procCount >= 0) snprintf(buf, sizeof(buf), "%d", pcd_procCount);
  else snprintf(buf, sizeof(buf), "N/A");
  pcd_value(SCALE_X(165), y1, colW, h, buf, PCD_ACCENT);
  pcd_formatMetric(buf, sizeof(buf), pcd_diskReadMB, "%.2f MB/s");
  pcd_value(SCALE_X(10), y2, colW, h, buf, PCD_ACCENT);
  pcd_formatMetric(buf, sizeof(buf), pcd_diskWriteMB, "%.2f MB/s");
  pcd_value(SCALE_X(165), y2, colW, h, buf, PCD_ACCENT);

  // Redraw CPU history
  pcd_drawCpuHistoryGraph();
}

// ═══════════════════════════════════════════════════════════
// SUBPAGE 3: NETWORK (Speeds, Latency, Wi-Fi RSSI, Total RX/TX)
// ═══════════════════════════════════════════════════════════
static void pcd_drawNetworkStatic() {
  tft.fillScreen(PCD_BG);
  pcd_drawHeader("NETWORK", "3/4");

  int colW = SCR_W / 2 - SCALE_X(15);
  pcd_text(SCALE_X(10), SCALE_Y(36), colW, 8, "DOWNLOAD", PCD_GREY);
  pcd_text(SCALE_X(165), SCALE_Y(36), colW, 8, "UPLOAD", PCD_GREY);
  pcd_text(SCALE_X(10), SCALE_Y(84), colW, 8, "LATENCY", PCD_GREY);
  pcd_text(SCALE_X(165), SCALE_Y(84), colW, 8, "WI-FI SIGNAL", PCD_GREY);
  pcd_text(SCALE_X(10), SCALE_Y(134), colW, 8, "TOTAL RX", PCD_GREY);
  pcd_text(SCALE_X(165), SCALE_Y(134), colW, 8, "TOTAL TX", PCD_GREY);
  pcd_drawSubPageIndicator("3/4");
}

static void pcd_updateNetworkDynamic() {
  char buf[32];
  int colW = (SCR_W / 2) - SCALE_X(15);

  int h = 8 * PCD_SZ_VAL;
  int y1 = max(SCALE_Y(50), SCALE_Y(36) + 9);
  int y2 = max(SCALE_Y(98), SCALE_Y(84) + 9);
  int y3 = max(SCALE_Y(148), SCALE_Y(134) + 9);
  pcd_formatMetric(buf, sizeof(buf), pcd_downMB, "%.2f MB/s");
  pcd_value(SCALE_X(10), y1, colW, h, buf, PCD_GREEN);
  pcd_formatMetric(buf, sizeof(buf), pcd_upMB, "%.2f MB/s");
  pcd_value(SCALE_X(165), y1, colW, h, buf, PCD_GREEN);
  uint16_t latCol = !isfinite(pcd_latency) ? PCD_GREY :
                    (pcd_latency < 0 || pcd_latency > 150) ? PCD_RED :
                    pcd_latency > 80 ? PCD_YELLOW : PCD_GREEN;
  pcd_formatMetric(buf, sizeof(buf), pcd_latency, "%.0f ms");
  pcd_value(SCALE_X(10), y2, colW, h, pcd_latency < 0 ? "OFFLINE" : buf, latCol);
  int rssi = WiFi.RSSI();
  uint16_t rssiCol = (rssi > -60) ? PCD_GREEN : (rssi > -75) ? PCD_YELLOW : PCD_RED;
  snprintf(buf, sizeof(buf), "%d dBm", rssi);
  bool wifiUp = WiFi.status() == WL_CONNECTED;
  pcd_value(SCALE_X(165), y2, colW, h, wifiUp ? buf : "N/A", wifiUp ? rssiCol : PCD_GREY);
  pcd_formatMetric(buf, sizeof(buf), pcd_rxGB, "%.2f GB");
  pcd_value(SCALE_X(10), y3, colW, h, buf, PCD_ACCENT);
  pcd_formatMetric(buf, sizeof(buf), pcd_txGB, "%.2f GB");
  pcd_value(SCALE_X(165), y3, colW, h, buf, PCD_ACCENT);
}

// ═══════════════════════════════════════════════════════════
// SUBPAGE 4: SYSTEM HEALTH (Top Processes & Subsystem Checks)
// ═══════════════════════════════════════════════════════════
static void pcd_drawHealthStatic() {
  tft.fillScreen(PCD_BG);
  pcd_drawHeader("SYSTEM HEALTH", "4/4");

  pcd_text(SCALE_X(10), SCALE_Y(36), SCR_W - SCALE_X(20), 8, "TOP CPU PROCESS", PCD_GREY);
  pcd_text(SCALE_X(10), SCALE_Y(70), SCR_W - SCALE_X(20), 8, "TOP RAM PROCESS", PCD_GREY);

  tft.drawFastHLine(SCALE_X(10), SCALE_Y(104), SCR_W - SCALE_X(20), PCD_DARK);

  pcd_drawSubPageIndicator("4/4");
}

static void pcd_drawStatusItem(int y, const char* label, const String& val, uint16_t valCol) {
  int x = SCALE_X(10), w = SCR_W - SCALE_X(20);
  int h = max(8, SCALE_Y(16));
  tft.fillRect(x, y, w, h, PCD_BG);
  tft.setTextSize(1);
  int labelW = min(w / 2, (int)tft.textWidth(label, 1));
  pcd_text(x, y, labelW, h, label, PCD_GREY);
  pcd_text(x + labelW + 4, y, w - labelW - 4, h, val.c_str(), valCol, PCD_SZ_VAL, true);
}

static void pcd_drawProcessRow(int y, const char* name, const char* value, uint16_t color) {
  int x = SCALE_X(10), w = SCR_W - SCALE_X(20), h = 8 * PCD_SZ_VAL;
  tft.fillRect(x, y, w, h, PCD_BG);
  tft.setTextSize(PCD_SZ_VAL);
  int valueW = min(w / 2, (int)tft.textWidth(value, 1));
  pcd_text(x, y, w - valueW - 4, h, name, color, PCD_SZ_VAL);
  pcd_text(x + w - valueW, y, valueW, h, value, color, PCD_SZ_VAL, true);
}

static void pcd_updateHealthDynamic() {
  char buf[48];

  // Top CPU Process
  uint16_t tcpCol = (pcd_topCpuPct >= 80) ? PCD_RED : (pcd_topCpuPct >= 50) ? PCD_YELLOW : PCD_TEXT;
  pcd_formatMetric(buf, sizeof(buf), pcd_topCpuPct, "%.0f%%");
  pcd_drawProcessRow(max(SCALE_Y(48), SCALE_Y(36) + 9), pcd_topCpuName, buf, tcpCol);

  // Top RAM Process
  if (pcd_topRamMB >= 1024.0f) snprintf(buf, sizeof(buf), "%.1fG", pcd_topRamMB / 1024.0f);
  else pcd_formatMetric(buf, sizeof(buf), pcd_topRamMB, "%.0fM");
  pcd_drawProcessRow(max(SCALE_Y(82), SCALE_Y(70) + 9), pcd_topRamName, buf, PCD_TEXT);

  // Status rows
  int yStart = SCALE_Y(112);
  int ySpacing = max(9, SCALE_Y(18));

  bool cpuOk = (pcd_cpu < 85);
  bool cpuWarn = (pcd_cpu < 95);
  pcd_drawStatusItem(yStart, "CPU LOAD", !isfinite(pcd_cpu) ? "N/A" : cpuOk ? "OK" : (cpuWarn ? "WARN" : "ALERT"), !isfinite(pcd_cpu) ? PCD_GREY : cpuOk ? PCD_GREEN : (cpuWarn ? PCD_YELLOW : PCD_RED));

  bool ramOk = (pcd_ram < 85);
  bool ramWarn = (pcd_ram < 95);
  pcd_drawStatusItem(yStart + ySpacing, "MEMORY", !isfinite(pcd_ram) ? "N/A" : ramOk ? "OK" : (ramWarn ? "WARN" : "ALERT"), !isfinite(pcd_ram) ? PCD_GREY : ramOk ? PCD_GREEN : (ramWarn ? PCD_YELLOW : PCD_RED));

  bool gpuKnown = isfinite(pcd_gpu) || isfinite(pcd_gpuTemp);
  bool gpuOk = ((!isfinite(pcd_gpu) || pcd_gpu < 85) && (!isfinite(pcd_gpuTemp) || pcd_gpuTemp < 78));
  bool gpuWarn = ((!isfinite(pcd_gpu) || pcd_gpu < 95) && (!isfinite(pcd_gpuTemp) || pcd_gpuTemp < 88));
  String gpuTxt = !gpuKnown ? "N/A" : gpuOk ? "OK" : (gpuWarn ? "WARN" : "ALERT");
  if (isfinite(pcd_gpuTemp)) gpuTxt += " (" + String((int)pcd_gpuTemp) + "C)";
  pcd_drawStatusItem(yStart + ySpacing * 2, "GPU & TEMP", gpuTxt, !gpuKnown ? PCD_GREY : gpuOk ? PCD_GREEN : (gpuWarn ? PCD_YELLOW : PCD_RED));

  bool diskOk = (pcd_diskFree >= 15);
  bool diskWarn = (pcd_diskFree >= 5);
  pcd_formatMetric(buf, sizeof(buf), pcd_diskFree, "%.0f%% FREE");
  pcd_drawStatusItem(yStart + ySpacing * 3, "DISK STORAGE", buf, !isfinite(pcd_diskFree) ? PCD_GREY : diskOk ? PCD_GREEN : (diskWarn ? PCD_YELLOW : PCD_RED));

  bool netOk = (pcd_latency >= 0 && pcd_latency <= 80);
  bool netWarn = (pcd_latency >= 0 && pcd_latency <= 150);
  pcd_formatMetric(buf, sizeof(buf), pcd_latency, "%.0fms");
  pcd_drawStatusItem(yStart + ySpacing * 4, "INTERNET", pcd_latency < 0 ? "OFFLINE" : buf, !isfinite(pcd_latency) ? PCD_GREY : netOk ? PCD_GREEN : (netWarn ? PCD_YELLOW : PCD_RED));
}

// ═══════════════════════════════════════════════════════════
// MASTER PAGE RENDERER
// ═══════════════════════════════════════════════════════════
void drawPcDashPage() {
  unsigned long now = millis();
  bool offline = !pcd_hasPacket || now - pcd_lastPacketMs >= PCD_TIMEOUT_MS;
  if (pcd_drawn && now - pcd_lastDrawMs < PCD_REDRAW_MS) return;
  if (offline != pcd_offline) {
    pcd_drawn = false;
    pcd_dirty = true;
  }
  pcd_offline = offline;
  if (pcd_offline) {
    if (!pcd_drawn || !pcd_offlineShown) pcd_drawOfflineScreen();
    pcd_drawn = true;
    pcd_dirty = false;
    pcd_lastDrawMs = now;
    return;
  }

  if (pcd_drawn && !pcd_dirty) return;
  pcd_offlineShown = false;

  if (!pcd_drawn) {
    if (pcd_subPage == 0) pcd_drawOverviewStatic();
    else if (pcd_subPage == 1) pcd_drawPerformanceStatic();
    else if (pcd_subPage == 2) pcd_drawNetworkStatic();
    else pcd_drawHealthStatic();
    pcd_drawn = true;
  } else {
    // Health changes with telemetry, even when the static subpage stays intact.
    const char* titles[] = {"PC DASHBOARD", "PERFORMANCE", "NETWORK", "SYSTEM HEALTH"};
    pcd_drawHeader(titles[pcd_subPage], "");
  }

  if (pcd_subPage == 0) pcd_updateOverviewDynamic();
  else if (pcd_subPage == 1) pcd_updatePerformanceDynamic();
  else if (pcd_subPage == 2) pcd_updateNetworkDynamic();
  else pcd_updateHealthDynamic();
  pcd_dirty = false;
  pcd_lastDrawMs = now;
}

void drawPcDashPageFull() {
  pcd_drawn = false;
  drawPcDashPage();
}

// Packets only mutate state. Coalesce bursts into at most five TFT updates/sec.
void tickPcDash() {
  if (currentState != STATE_PC_DASH) return;
  unsigned long now = millis();
  bool offline = !pcd_hasPacket || now - pcd_lastPacketMs >= PCD_TIMEOUT_MS;
  if (now - pcd_lastDrawMs < PCD_REDRAW_MS) return;
  if (!pcd_drawn || pcd_dirty || offline != pcd_offline) drawPcDashPage();
}

// ═══════════════════════════════════════════════════════════
// TELEMETRY PACKET PROCESSOR
// Supports:
// 1) Extended CSV with GPU & Temps (23 fields)
// 2) Original innocircuitslab CSV (17 fields)
// 3) JSON format (PCDASH:{"cpu":...})
// ═══════════════════════════════════════════════════════════
static double pcd_parseNumber(const char* text) {
  if (!text) return NAN;
  while (isspace((unsigned char)*text)) ++text;
  if (!*text) return NAN;
  char* end;
  double value = strtod(text, &end);
  if (end == text) return NAN;
  while (isspace((unsigned char)*end)) ++end;
  return !*end && isfinite(value) ? value : NAN;
}

static double pcd_boundNumber(double value, double low, double high) {
  return isfinite(value) && value >= low && value <= high ? value : NAN;
}

static void pcd_copyName(char* dest, size_t len, const char* src) {
  snprintf(dest, len, "%s", src && *src ? src : "N/A");
}

void processPcDashPacket(const char* packet) {
  if (!packet || !*packet) return;
  const char* payload = packet;
  if (strncmp(payload, "PCDASH:", 7) == 0) payload += 7;
  while (isspace((unsigned char)*payload)) ++payload;

  // Same field order as the original 17-field and extended 23-field CSV.
  struct MetricField { const char* key; float* value; double low; double high; };
  const MetricField fields[] = {
    {"cpu", &pcd_cpu, 0, 100}, {"ram", &pcd_ram, 0, 100},
    {"down", &pcd_downMB, 0, 1e9}, {"up", &pcd_upMB, 0, 1e9},
    {"uptime", nullptr, 0, ULONG_MAX}, {"freq", &pcd_cpuFreq, 0.01, 100000},
    {"dread", &pcd_diskReadMB, 0, 1e9}, {"dwrite", &pcd_diskWriteMB, 0, 1e9},
    {"proc", nullptr, 0, INT_MAX}, {"rx", &pcd_rxGB, 0, 1e9},
    {"tx", &pcd_txGB, 0, 1e9}, {"latency", &pcd_latency, -1, 3600000},
    {"diskfree", &pcd_diskFree, 0, 100}, {"top_cpu_pct", &pcd_topCpuPct, 0, 100},
    {"top_cpu", nullptr, 0, 0}, {"top_ram_mb", &pcd_topRamMB, 0, 1e9},
    {"top_ram", nullptr, 0, 0}, {"gpu", &pcd_gpu, 0, 100},
    {"vram", &pcd_vramPct, 0, 100}, {"vram_used", &pcd_vramUsedMB, 0, 1e9},
    {"vram_total", &pcd_vramTotalMB, 0.01, 1e9},
    {"gpu_temp", &pcd_gpuTemp, 0.01, 150}, {"laptop_temp", &pcd_laptopTemp, 0.01, 150}
  };
  bool cpuUpdated = false;
  if (*payload == '{') {
    // JSON object storage includes keys and process strings, not just wire bytes.
    StaticJsonDocument<1536> doc;
    if (deserializeJson(doc, payload) != DeserializationError::Ok || !doc.is<JsonObject>()) return;
    bool recognized = false;
    for (int i = 0; i < 23; ++i) {
      const char* key = fields[i].key;
      // Preserve partial JSON updates; explicit null/invalid values clear a metric.
      if (!doc.containsKey(key)) continue;
      recognized = true;
      JsonVariantConst item = doc[key];
      if (i == 14 || i == 16) {
        pcd_copyName(i == 14 ? pcd_topCpuName : pcd_topRamName,
                     sizeof(pcd_topCpuName), item.as<const char*>());
        continue;
      }
      double value = item.is<double>() ? item.as<double>() : NAN;
      value = pcd_boundNumber(value, fields[i].low, fields[i].high);
      if (i == 4) {
        pcd_hasUptime = isfinite(value) && floor(value) == value;
        pcd_uptime = pcd_hasUptime ? (unsigned long)value : 0;
      } else if (i == 8) {
        pcd_procCount = isfinite(value) && floor(value) == value ? (int)value : -1;
      } else {
        *fields[i].value = (float)value;
      }
      if (i == 0) cpuUpdated = true;
    }
    if (!recognized) return;
  } else {
    // Reject overlong CSV rather than accepting a silently truncated snapshot.
    char temp[1024];
    if (strlen(payload) >= sizeof(temp)) return;
    strcpy(temp, payload);
    char* tokens[24];
    int count = 0;
    char* ptr = temp;
    do {
      tokens[count++] = ptr;
      char* comma = strchr(ptr, ',');
      if (!comma) break;
      *comma = '\0';
      ptr = comma + 1;
    } while (count < 24);
    if (count < 17) return;
    for (int i = 0; i < 23; ++i) {
      const char* token = i < count ? tokens[i] : nullptr;
      if (i == 14 || i == 16) {
        pcd_copyName(i == 14 ? pcd_topCpuName : pcd_topRamName,
                     sizeof(pcd_topCpuName), token);
        continue;
      }
      double value = pcd_boundNumber(pcd_parseNumber(token), fields[i].low, fields[i].high);
      if (i == 4) {
        pcd_hasUptime = isfinite(value) && floor(value) == value;
        pcd_uptime = pcd_hasUptime ? (unsigned long)value : 0;
      } else if (i == 8) {
        pcd_procCount = isfinite(value) && floor(value) == value ? (int)value : -1;
      } else {
        *fields[i].value = (float)value;
      }
    }
    cpuUpdated = true;
  }
  if (cpuUpdated) pcd_addCpuHistory(pcd_cpu);
  pcd_lastPacketMs = millis();
  pcd_hasPacket = true;
  pcd_dirty = true;
}

void processPcDashPacket(const String& packet) {
  processPcDashPacket(packet.c_str());
}

#endif // PAGE_PC_DASH_H
