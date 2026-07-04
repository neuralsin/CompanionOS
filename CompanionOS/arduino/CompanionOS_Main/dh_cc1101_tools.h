// ═══════════════════════════════════════════════════════════
// COMPANION OS v7.0 — DR. HACK: CC1101 SUB-GHZ TOOLS
// 12 tools using CC1101 radio via raw SPI (no external lib)
// Adapted from ESP32-TOOLS-PRO for 160×128 ST7735R
// ═══════════════════════════════════════════════════════════
#ifndef DH_CC1101_TOOLS_H
#define DH_CC1101_TOOLS_H

#ifdef ESP32

#include "globals.h"
#include "ui_components.h"
#include <SPI.h>

// ═══════════════════════════════════════════════════════════
// CC1101 SPI REGISTER MAP
// ═══════════════════════════════════════════════════════════

static const uint8_t CC_READ_SINGLE = 0x80;
static const uint8_t CC_READ_BURST  = 0xC0;

static const uint8_t CC_IOCFG0   = 0x02;
static const uint8_t CC_PKTCTRL1 = 0x07;
static const uint8_t CC_PKTCTRL0 = 0x08;
static const uint8_t CC_FSCTRL1  = 0x0B;
static const uint8_t CC_FSCTRL0  = 0x0C;
static const uint8_t CC_FREQ2    = 0x0D;
static const uint8_t CC_FREQ1    = 0x0E;
static const uint8_t CC_FREQ0    = 0x0F;
static const uint8_t CC_MDMCFG4  = 0x10;
static const uint8_t CC_MDMCFG3  = 0x11;
static const uint8_t CC_MDMCFG2  = 0x12;
static const uint8_t CC_MDMCFG1  = 0x13;
static const uint8_t CC_MDMCFG0  = 0x14;
static const uint8_t CC_DEVIATN  = 0x15;
static const uint8_t CC_MCSM0    = 0x18;
static const uint8_t CC_FOCCFG   = 0x19;
static const uint8_t CC_BSCFG    = 0x1A;
static const uint8_t CC_AGCCTRL2 = 0x1B;
static const uint8_t CC_AGCCTRL1 = 0x1C;
static const uint8_t CC_AGCCTRL0 = 0x1D;
static const uint8_t CC_FREND1   = 0x21;
static const uint8_t CC_FREND0   = 0x22;
static const uint8_t CC_FSCAL3   = 0x23;
static const uint8_t CC_FSCAL2   = 0x24;
static const uint8_t CC_FSCAL1   = 0x25;
static const uint8_t CC_FSCAL0   = 0x26;
static const uint8_t CC_TEST2    = 0x2C;
static const uint8_t CC_TEST1    = 0x2D;
static const uint8_t CC_TEST0    = 0x2E;
static const uint8_t CC_PATABLE  = 0x3E;

// Status registers (burst read)
static const uint8_t CC_PARTNUM   = 0x30;
static const uint8_t CC_VERSION   = 0x31;
static const uint8_t CC_LQI       = 0x33;
static const uint8_t CC_RSSI_REG  = 0x34;
static const uint8_t CC_MARCSTATE = 0x35;
static const uint8_t CC_PKTSTATUS = 0x38;

// Strobe commands
static const uint8_t CC_SRES  = 0x30;
static const uint8_t CC_SCAL  = 0x33;
static const uint8_t CC_SRX   = 0x34;
static const uint8_t CC_STX   = 0x35;
static const uint8_t CC_SIDLE = 0x36;
static const uint8_t CC_SFRX  = 0x3A;
static const uint8_t CC_SFTX  = 0x3B;

static const uint8_t CC_GDO_CARRIER_SENSE = 0x0E;
static const uint8_t CC_GDO_ASYNC_DATA    = 0x0D;
static const uint32_t CC_XTAL_HZ = 26000000UL;

static SPISettings dhCcSpi(4000000, MSBFIRST, SPI_MODE0);

// ═══════════════════════════════════════════════════════════
// LOW-LEVEL SPI FUNCTIONS
// ═══════════════════════════════════════════════════════════

static void dhCcInitPins() {
  pinMode(TFT_CS, OUTPUT); digitalWrite(TFT_CS, HIGH);
  pinMode(NRF1_CSN_PIN, OUTPUT); digitalWrite(NRF1_CSN_PIN, HIGH);
  pinMode(NRF2_CSN_PIN, OUTPUT); digitalWrite(NRF2_CSN_PIN, HIGH);
  pinMode(CC1101_CSN_PIN, OUTPUT); digitalWrite(CC1101_CSN_PIN, HIGH);
  pinMode(CC1101_GDO0_PIN, INPUT);
  pinMode(CC1101_TX_DATA_PIN, INPUT);
}

static void dhCcSelect() {
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(NRF1_CSN_PIN, HIGH);
  digitalWrite(NRF2_CSN_PIN, HIGH);
  SPI.beginTransaction(dhCcSpi);
  digitalWrite(CC1101_CSN_PIN, LOW);
  delayMicroseconds(5);
}

static void dhCcDeselect() {
  digitalWrite(CC1101_CSN_PIN, HIGH);
  SPI.endTransaction();
}

static uint8_t dhCcRead(uint8_t reg, bool* ready = nullptr) {
  dhCcSelect();
  uint8_t mode = (reg >= 0x30) ? CC_READ_BURST : CC_READ_SINGLE;
  uint8_t status = SPI.transfer(reg | mode);
  uint8_t value = SPI.transfer(0x00);
  dhCcDeselect();
  if (ready) *ready = ((status & 0x80) == 0) && status != 0xFF;
  return value;
}

static void dhCcWrite(uint8_t reg, uint8_t value) {
  dhCcSelect();
  SPI.transfer(reg);
  SPI.transfer(value);
  dhCcDeselect();
}

static void dhCcWriteBurst(uint8_t reg, const uint8_t* data, uint8_t len) {
  dhCcSelect();
  SPI.transfer(reg | 0x40);
  for (uint8_t i = 0; i < len; i++) SPI.transfer(data[i]);
  dhCcDeselect();
}

static uint8_t dhCcStrobe(uint8_t command) {
  dhCcSelect();
  uint8_t status = SPI.transfer(command);
  dhCcDeselect();
  return status;
}

static int dhCcRssiDbm(uint8_t raw) {
  int rssi = raw;
  if (rssi >= 128) rssi -= 256;
  return (rssi / 2) - 74;
}

static uint32_t dhCcFreqWord(uint32_t freqKHz) {
  uint64_t freqHz = (uint64_t)freqKHz * 1000ULL;
  return (uint32_t)((freqHz << 16) / CC_XTAL_HZ);
}

static void dhCcSetFreq(uint32_t freqKHz) {
  uint32_t w = dhCcFreqWord(freqKHz);
  dhCcWrite(CC_FREQ2, (w >> 16) & 0xFF);
  dhCcWrite(CC_FREQ1, (w >> 8) & 0xFF);
  dhCcWrite(CC_FREQ0, w & 0xFF);
}

static String dhCcFmtFreq(uint32_t freqKHz) {
  return String(freqKHz / 1000) + "." + String((freqKHz % 1000) / 10) + "M";
}

static const char* dhCcMarcName(uint8_t marc) {
  switch (marc & 0x1F) {
    case 0x00: return "SLEEP"; case 0x01: return "IDLE"; case 0x02: return "XOFF";
    case 0x0D: return "RX";    case 0x13: return "TX";   default: return "OTHER";
  }
}

static void dhCcReset() {
  digitalWrite(CC1101_CSN_PIN, HIGH); delayMicroseconds(30);
  digitalWrite(CC1101_CSN_PIN, LOW);  delayMicroseconds(30);
  digitalWrite(CC1101_CSN_PIN, HIGH); delayMicroseconds(45);
  dhCcStrobe(CC_SRES); delay(3);
}

static void dhCcConfigOokRx() {
  dhCcWrite(CC_IOCFG0,   CC_GDO_CARRIER_SENSE);
  dhCcWrite(CC_PKTCTRL1, 0x04);
  dhCcWrite(CC_PKTCTRL0, 0x32);
  dhCcWrite(CC_FSCTRL1,  0x06);
  dhCcWrite(CC_FSCTRL0,  0x00);
  dhCcWrite(CC_MDMCFG4,  0xF5);
  dhCcWrite(CC_MDMCFG3,  0x83);
  dhCcWrite(CC_MDMCFG2,  0x30);
  dhCcWrite(CC_MDMCFG1,  0x22);
  dhCcWrite(CC_MDMCFG0,  0xF8);
  dhCcWrite(CC_DEVIATN,  0x00);
  dhCcWrite(CC_MCSM0,    0x18);
  dhCcWrite(CC_FOCCFG,   0x16);
  dhCcWrite(CC_BSCFG,    0x6C);
  dhCcWrite(CC_AGCCTRL2, 0x43);
  dhCcWrite(CC_AGCCTRL1, 0x40);
  dhCcWrite(CC_AGCCTRL0, 0x91);
  dhCcWrite(CC_FREND1,   0x56);
  dhCcWrite(CC_FREND0,   0x11);
  dhCcWrite(CC_FSCAL3,   0xE9);
  dhCcWrite(CC_FSCAL2,   0x2A);
  dhCcWrite(CC_FSCAL1,   0x00);
  dhCcWrite(CC_FSCAL0,   0x1F);
  dhCcWrite(CC_TEST2,    0x81);
  dhCcWrite(CC_TEST1,    0x35);
  dhCcWrite(CC_TEST0,    0x09);
}

static bool dhCcPrepareRx(uint32_t freqKHz) {
  dhCcInitPins();
  dhCcReset();
  dhCcConfigOokRx();
  dhCcSetFreq(freqKHz);
  dhCcStrobe(CC_SIDLE); delay(1);
  dhCcStrobe(CC_SFRX);
  dhCcStrobe(CC_SCAL); delay(3);
  dhCcStrobe(CC_SRX); delay(4);
  bool ready = false;
  uint8_t part = dhCcRead(CC_PARTNUM, &ready);
  uint8_t ver = dhCcRead(CC_VERSION);
  return ready && part != 0xFF && ver != 0xFF;
}

static void dhCcRetune(uint32_t freqKHz) {
  dhCcStrobe(CC_SIDLE); delayMicroseconds(400);
  dhCcSetFreq(freqKHz);
  dhCcStrobe(CC_SCAL); delay(2);
  dhCcStrobe(CC_SRX); delay(4);
}

static int dhCcReadRssiAvg() {
  int total = 0;
  for (int i = 0; i < 3; i++) { total += dhCcRssiDbm(dhCcRead(CC_RSSI_REG)); delay(2); }
  return total / 3;
}

static uint16_t dhCcRssiColor(int dbm) {
  if (dbm > -60) return CLR_SECONDARY;
  if (dbm > -75) return CLR_WARNING;
  if (dbm > -90) return CLR_SUCCESS;
  return CLR_TEXT_LO;
}

// ═══════════════════════════════════════════════════════════
// SCAN BANDS
// ═══════════════════════════════════════════════════════════

struct DH_CcBand { const char* name; uint32_t startKHz; uint16_t stepKHz; uint8_t points; };
static const DH_CcBand DH_CC_BANDS[] = {
  {"315", 300000, 1000, 31},
  {"433", 420000, 500,  41},
  {"868", 860000, 1000, 21},
  {"915", 902000, 1000, 27}
};
#define DH_CC_BAND_COUNT 4

struct DH_CcMonFreq { const char* name; uint32_t freqKHz; };
static const DH_CcMonFreq DH_CC_MON_FREQS[] = {
  {"315.00", 315000}, {"390.00", 390000}, {"433.92", 433920},
  {"868.35", 868350}, {"915.00", 915000}
};
#define DH_CC_MON_COUNT 5

// ═══════════════════════════════════════════════════════════
// RF CAPTURE DATA
// ═══════════════════════════════════════════════════════════

#define DH_RF_RAW_MAX 256

struct DH_RfCapture {
  uint16_t raw[DH_RF_RAW_MAX];
  uint16_t count;
  uint32_t durationUs;
  uint32_t hash;
  uint8_t startLevel;
  bool overflow;
};

static DH_RfCapture dhRfCapture;
static uint32_t dhRfCaptureFreqKHz = 433920;
static bool dhHasRfCapture = false;

// ═══════════════════════════════════════════════════════════
// CC1101 ERROR SCREEN
// ═══════════════════════════════════════════════════════════

static bool dhCcShowError(const char* title) {
  char errMsg[64];
  sprintf(errMsg, "CC1101 Disconnected\nCSN:%d GDO0:%d", CC1101_CSN_PIN, CC1101_GDO0_PIN);
  dhShowError(errMsg);
  return false;
}

// ═══════════════════════════════════════════════════════════
// TOOL 1: CC1101 Hardware Diag
// ═══════════════════════════════════════════════════════════

static void dhRunCcDiag() {
  if (!dhCcPrepareRx(433920)) { dhCcShowError("CC DIAG"); return; }

  int lastGdo = digitalRead(CC1101_GDO0_PIN);
  unsigned long edges = 0, lastDraw = 0;
  unsigned long holdStart = 0; bool holding = false;

  while (true) {
    extern void handleNetwork(); handleNetwork();
    int gdo = digitalRead(CC1101_GDO0_PIN);
    if (gdo != lastGdo) { edges++; lastGdo = gdo; }

    if (millis() - lastDraw >= 200) {
      bool ready = false;
      uint8_t part = dhCcRead(CC_PARTNUM, &ready);
      uint8_t ver = dhCcRead(CC_VERSION);
      uint8_t marc = dhCcRead(CC_MARCSTATE);
      int rssi = dhCcRssiDbm(dhCcRead(CC_RSSI_REG));
      uint8_t lqi = dhCcRead(CC_LQI);

      tft.fillScreen(CLR_BG);
      drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_WARNING, darkenColor(CLR_WARNING, 50), 0);
      tft.setTextColor(CLR_BG);
      tft.drawString("CC1101 DIAG", SCALE_X(4), SCALE_Y(2), 1);

      int y = SCALE_Y(18); int lineH = SCALE_Y(11);
      char buf[32];

      tft.setTextColor(ready ? CLR_SUCCESS : CLR_SECONDARY);
      tft.drawString(ready ? "SPI READY" : "SPI FAIL", SCALE_X(4), y, 1); y += lineH;
      sprintf(buf, "PART: 0x%02X", part);
      tft.setTextColor(CLR_TEXT_MED); tft.drawString(buf, SCALE_X(4), y, 1); y += lineH;
      sprintf(buf, "VER:  0x%02X", ver);
      tft.drawString(buf, SCALE_X(4), y, 1); y += lineH;
      sprintf(buf, "MARC: %s", dhCcMarcName(marc));
      tft.drawString(buf, SCALE_X(4), y, 1); y += lineH;
      sprintf(buf, "RSSI: %ddBm", rssi);
      tft.setTextColor(dhCcRssiColor(rssi)); tft.drawString(buf, SCALE_X(4), y, 1); y += lineH;
      sprintf(buf, "LQI:  0x%02X", lqi);
      tft.setTextColor(CLR_TEXT_LO); tft.drawString(buf, SCALE_X(4), y, 1); y += lineH;
      sprintf(buf, "GDO0: %d", gdo);
      tft.setTextColor(CLR_WARNING); tft.drawString(buf, SCALE_X(4), y, 1); y += lineH;
      sprintf(buf, "Edges: %lu", edges);
      tft.setTextColor(CLR_TEXT_MED); tft.drawString(buf, SCALE_X(4), y, 1);

      tft.setTextColor(CLR_TEXT_LO);
      tft.drawString("HOLD SEL: exit", SCALE_X(4), SCR_H - SCALE_Y(10), 1);
      lastDraw = millis();
    }

    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) {
      if (!holding) { holdStart = millis(); holding = true; }
      if (millis() - holdStart > 800) { dhCcStrobe(CC_SIDLE); return; }
    } else { holding = false; }
    delay(5);
  }
}

// ═══════════════════════════════════════════════════════════
// TOOL 2: Spectrum Scan
// ═══════════════════════════════════════════════════════════

static void dhRunCcSpectrum() {
  int bandIdx = 1;
  int rssiVals[48];

  while (true) {
    extern void handleNetwork(); handleNetwork();
    const DH_CcBand& band = DH_CC_BANDS[bandIdx];
    if (!dhCcPrepareRx(band.startKHz)) { dhCcShowError("SPECTRUM"); return; }

    // Sweep
    uint8_t peakIdx = 0; int peakDbm = -127;
    for (int i = 0; i < band.points && i < 48; i++) {
      uint32_t freq = band.startKHz + band.stepKHz * i;
      dhCcRetune(freq);
      rssiVals[i] = dhCcReadRssiAvg();
      if (rssiVals[i] > peakDbm) { peakDbm = rssiVals[i]; peakIdx = i; }
    }

    // Draw
    tft.fillScreen(CLR_BG);
    drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_WARNING, darkenColor(CLR_WARNING, 50), 0);
    tft.setTextColor(CLR_BG);
    char hdr[20]; sprintf(hdr, "SPEC %sM", band.name);
    tft.drawString(hdr, SCALE_X(4), SCALE_Y(2), 1);

    int plotY = SCALE_Y(18), plotH = SCALE_Y(70);
    tft.drawRect(SCALE_X(2), plotY, SCR_W - SCALE_X(4), plotH, CLR_BORDER);

    for (int i = 0; i < band.points && i < 48; i++) {
      int x = SCALE_X(4) + (i * (SCR_W - SCALE_X(8))) / band.points;
      int h = map(constrain(rssiVals[i], -110, -45), -110, -45, 2, plotH - 4);
      uint16_t col = (i == peakIdx) ? CLR_WARNING : dhCcRssiColor(rssiVals[i]);
      tft.drawFastVLine(x, plotY + plotH - h - 2, h, col);
    }

    char peak[32]; sprintf(peak, "Peak:%s %ddB", dhCcFmtFreq(band.startKHz + band.stepKHz * peakIdx).c_str(), peakDbm);
    tft.setTextColor(CLR_WARNING); tft.drawString(peak, SCALE_X(4), SCALE_Y(92), 1);

    tft.setTextColor(CLR_TEXT_LO);
    tft.drawString("</>:Band SEL:Rescan", SCALE_X(2), SCR_H - SCALE_Y(20), 1);
    tft.drawString("HOLD SEL: exit", SCALE_X(2), SCR_H - SCALE_Y(10), 1);

    unsigned long holdStart = 0; bool holding = false;
    bool stay = true;
    while (stay) {
      if ((digitalRead(BTN_LEFT) == LOW || (virtualLeftPressed ? (virtualLeftPressed=false, true) : false))) { bandIdx = (bandIdx + DH_CC_BAND_COUNT - 1) % DH_CC_BAND_COUNT; stay = false; delay(200); }
      if ((digitalRead(BTN_RIGHT) == LOW || (virtualRightPressed ? (virtualRightPressed=false, true) : false))) { bandIdx = (bandIdx + 1) % DH_CC_BAND_COUNT; stay = false; delay(200); }
      if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) {
        if (!holding) { holdStart = millis(); holding = true; }
        if (millis() - holdStart > 800) { dhCcStrobe(CC_SIDLE); return; }
      } else {
        if (holding && millis() - holdStart < 800) { stay = false; }
        holding = false;
      }
      delay(10);
    }
  }
}

// ═══════════════════════════════════════════════════════════
// TOOL 3: Waterfall
// ═══════════════════════════════════════════════════════════

#define DH_WF_ROWS 40
#define DH_WF_COLS 41

static void dhRunCcWaterfall() {
  int bandIdx = 1;
  static int8_t wf[DH_WF_ROWS][DH_WF_COLS];
  memset(wf, -120, sizeof(wf));
  int wfRow = 0;

  if (!dhCcPrepareRx(DH_CC_BANDS[bandIdx].startKHz)) { dhCcShowError("WATERFALL"); return; }

  unsigned long holdStart = 0; bool holding = false;

  while (true) {
    extern void handleNetwork(); handleNetwork();
    const DH_CcBand& band = DH_CC_BANDS[bandIdx];
    int pts = min((int)band.points, DH_WF_COLS);

    // Sweep one row
    for (int i = 0; i < pts; i++) {
      dhCcRetune(band.startKHz + band.stepKHz * i);
      wf[wfRow][i] = (int8_t)constrain(dhCcRssiDbm(dhCcRead(CC_RSSI_REG)), -120, -30);
    }
    wfRow = (wfRow + 1) % DH_WF_ROWS;

    // Draw
    tft.fillScreen(CLR_BG);
    drawGradientCard(0, 0, SCR_W, SCALE_Y(12), CLR_WARNING, darkenColor(CLR_WARNING, 50), 0);
    tft.setTextColor(CLR_BG);
    char hdr[20]; sprintf(hdr, "WF %sM", band.name);
    tft.drawString(hdr, SCALE_X(2), SCALE_Y(1), 1);

    int plotY = SCALE_Y(14), plotH = SCALE_Y(80);
    for (int row = 0; row < DH_WF_ROWS; row++) {
      int r = (wfRow + row) % DH_WF_ROWS;
      int y = plotY + (row * plotH) / DH_WF_ROWS;
      for (int col = 0; col < pts; col++) {
        int x = (col * SCR_W) / pts;
        int dbm = wf[r][col];
        if (dbm > -100) {
          uint16_t c = dbm > -60 ? CLR_SECONDARY : (dbm > -80 ? CLR_WARNING : CLR_SUCCESS);
          tft.drawPixel(x, y, c);
        }
      }
    }

    tft.setTextColor(CLR_TEXT_LO);
    tft.drawString("</>:Band HOLD:Back", SCALE_X(2), SCR_H - SCALE_Y(10), 1);

    if ((digitalRead(BTN_LEFT) == LOW || (virtualLeftPressed ? (virtualLeftPressed=false, true) : false))) { bandIdx = (bandIdx+3) % 4; dhCcPrepareRx(DH_CC_BANDS[bandIdx].startKHz); memset(wf,-120,sizeof(wf)); wfRow=0; delay(200); }
    if ((digitalRead(BTN_RIGHT) == LOW || (virtualRightPressed ? (virtualRightPressed=false, true) : false))) { bandIdx = (bandIdx+1) % 4; dhCcPrepareRx(DH_CC_BANDS[bandIdx].startKHz); memset(wf,-120,sizeof(wf)); wfRow=0; delay(200); }
    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) {
      if (!holding) { holdStart = millis(); holding = true; }
      if (millis() - holdStart > 800) { dhCcStrobe(CC_SIDLE); return; }
    } else { holding = false; }
    delay(5);
  }
}

// ═══════════════════════════════════════════════════════════
// TOOL 4: Frequency Monitor
// ═══════════════════════════════════════════════════════════

static void dhRunCcFreqMon() {
  int freqIdx = 2;

  while (true) {
    extern void handleNetwork(); handleNetwork();
    const DH_CcMonFreq& f = DH_CC_MON_FREQS[freqIdx];
    if (!dhCcPrepareRx(f.freqKHz)) { dhCcShowError("FREQ MON"); return; }

    int peakDbm = -127, lastGdo = digitalRead(CC1101_GDO0_PIN);
    unsigned long edges = 0, lastHit = 0, lastDraw = 0;
    unsigned long holdStart = 0; bool holding = false;
    bool retune = false;

    while (!retune) {
      int gdo = digitalRead(CC1101_GDO0_PIN);
      if (gdo != lastGdo) { edges++; lastGdo = gdo; lastHit = millis(); }

      if (millis() - lastDraw >= 200) {
        int rssi = dhCcRssiDbm(dhCcRead(CC_RSSI_REG));
        if (rssi > peakDbm) peakDbm = rssi;

        tft.fillScreen(CLR_BG);
        drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_WARNING, darkenColor(CLR_WARNING, 50), 0);
        tft.setTextColor(CLR_BG);
        char hdr[20]; sprintf(hdr, "MON %sM", f.name);
        tft.drawString(hdr, SCALE_X(2), SCALE_Y(2), 1);

        char buf[32];
        int y = SCALE_Y(20); int lineH = SCALE_Y(14);
        sprintf(buf, "RSSI: %ddBm", rssi);
        tft.setTextColor(dhCcRssiColor(rssi)); tft.drawString(buf, SCALE_X(4), y, 1); y += lineH;

        // RSSI bar
        int barPct = map(constrain(rssi, -110, -45), -110, -45, 0, 100);
        tft.drawRect(SCALE_X(4), y, SCR_W - SCALE_X(8), SCALE_Y(8), CLR_BORDER);
        int fillW = ((SCR_W - SCALE_X(10)) * barPct) / 100;
        if (fillW > 0) tft.fillRect(SCALE_X(5), y+1, fillW, SCALE_Y(6), dhCcRssiColor(rssi));
        y += SCALE_Y(14);

        sprintf(buf, "Peak: %ddBm", peakDbm);
        tft.setTextColor(CLR_TEXT_MED); tft.drawString(buf, SCALE_X(4), y, 1); y += lineH;
        sprintf(buf, "GDO0: %d  Edges: %lu", gdo, edges);
        tft.drawString(buf, SCALE_X(4), y, 1); y += lineH;

        String last = lastHit == 0 ? "--" : String((millis() - lastHit) / 1000) + "s ago";
        tft.setTextColor(CLR_TEXT_LO);
        tft.drawString(("Last: " + last).c_str(), SCALE_X(4), y, 1);

        tft.setTextColor(CLR_TEXT_LO);
        tft.drawString("</>:Freq HOLD:Back", SCALE_X(2), SCR_H - SCALE_Y(10), 1);
        lastDraw = millis();
      }

      if ((digitalRead(BTN_LEFT) == LOW || (virtualLeftPressed ? (virtualLeftPressed=false, true) : false))) { freqIdx = (freqIdx + DH_CC_MON_COUNT - 1) % DH_CC_MON_COUNT; retune = true; delay(200); }
      if ((digitalRead(BTN_RIGHT) == LOW || (virtualRightPressed ? (virtualRightPressed=false, true) : false))) { freqIdx = (freqIdx + 1) % DH_CC_MON_COUNT; retune = true; delay(200); }
      if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) {
        if (!holding) { holdStart = millis(); holding = true; }
        if (millis() - holdStart > 800) { dhCcStrobe(CC_SIDLE); return; }
      } else { holding = false; }
      delay(5);
    }
  }
}

// ═══════════════════════════════════════════════════════════
// TOOL 5: Frequency Finder
// ═══════════════════════════════════════════════════════════

static void dhRunCcFinder() {
  // Calibrate noise floor
  int noise[DH_CC_BAND_COUNT][48];
  tft.fillScreen(CLR_BG);
  tft.setTextColor(CLR_WARNING);
  tft.drawCentreString("CALIBRATING", SCR_CX, SCALE_Y(30), 1);
  tft.setTextColor(CLR_TEXT_MED);
  tft.drawCentreString("Don't press remote", SCR_CX, SCALE_Y(50), 1);

  for (int b = 0; b < DH_CC_BAND_COUNT; b++) {
    const DH_CcBand& band = DH_CC_BANDS[b];
    if (!dhCcPrepareRx(band.startKHz)) { dhCcShowError("FINDER"); return; }
    for (int i = 0; i < band.points && i < 48; i++) {
      dhCcRetune(band.startKHz + band.stepKHz * i);
      noise[b][i] = dhCcReadRssiAvg();
    }
  }

  tft.fillScreen(CLR_BG);
  tft.setTextColor(CLR_SUCCESS);
  tft.drawCentreString("NOW press remote!", SCR_CX, SCALE_Y(30), 1);
  tft.setTextColor(CLR_TEXT_MED);
  tft.drawCentreString("Scanning all bands...", SCR_CX, SCALE_Y(50), 1);
  delay(500);

  // Search loop
  uint32_t bestFreq = 0; int bestDelta = 0;
  int sweepCount = 0;
  unsigned long holdStart = 0; bool holding = false;

  while (true) {
    extern void handleNetwork(); handleNetwork();
    for (int b = 0; b < DH_CC_BAND_COUNT; b++) {
      const DH_CcBand& band = DH_CC_BANDS[b];
      dhCcPrepareRx(band.startKHz);
      for (int i = 0; i < band.points && i < 48; i++) {
        dhCcRetune(band.startKHz + band.stepKHz * i);
        int rssi = dhCcReadRssiAvg();
        int delta = rssi - noise[b][i];
        if (delta > bestDelta) {
          bestDelta = delta;
          bestFreq = band.startKHz + band.stepKHz * i;
        }

        if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) {
          if (!holding) { holdStart = millis(); holding = true; }
          if (millis() - holdStart > 800) { dhCcStrobe(CC_SIDLE); return; }
        } else { holding = false; }
      }
    }
    sweepCount++;

    // Draw results
    tft.fillScreen(CLR_BG);
    drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_SUCCESS, darkenColor(CLR_SUCCESS, 50), 0);
    tft.setTextColor(CLR_TEXT_HI);
    char hdr[20]; sprintf(hdr, "FINDER #%d", sweepCount);
    tft.drawString(hdr, SCALE_X(4), SCALE_Y(2), 1);

    if (bestDelta >= 8) {
      tft.setTextColor(CLR_SUCCESS);
      tft.drawCentreString(dhCcFmtFreq(bestFreq).c_str(), SCR_CX, SCALE_Y(28), 2);
      char buf[20]; sprintf(buf, "+%ddB", bestDelta);
      tft.setTextColor(CLR_WARNING); tft.drawCentreString(buf, SCR_CX, SCALE_Y(50), 1);
    } else {
      tft.setTextColor(CLR_TEXT_MED);
      tft.drawCentreString("Searching...", SCR_CX, SCALE_Y(35), 1);
    }

    tft.setTextColor(CLR_TEXT_LO);
    tft.drawString("HOLD SEL: exit", SCALE_X(4), SCR_H - SCALE_Y(10), 1);
  }
}

// ═══════════════════════════════════════════════════════════
// TOOL 6: Brute Search — Wide frequency sweep
// ═══════════════════════════════════════════════════════════

static void dhRunCcBrute() {
  tft.fillScreen(CLR_BG);
  tft.setTextColor(CLR_WARNING);
  tft.drawCentreString("BRUTE SEARCH", SCR_CX, SCALE_Y(20), 1);
  tft.setTextColor(CLR_TEXT_MED);
  tft.drawCentreString("Wide sweep 300-920M", SCR_CX, SCALE_Y(40), 1);
  tft.drawCentreString("Hold remote button!", SCR_CX, SCALE_Y(56), 1);
  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("SEL:Start", SCALE_X(4), SCR_H - SCALE_Y(10), 1);

  dhWaitSelectPress();
  delay(200);

  if (!dhCcPrepareRx(300000)) { dhCcShowError("BRUTE"); return; }

  struct { uint32_t freqKHz; int rssi; } hits[6];
  int hitCount = 0;
  int peakDbm = -127; uint32_t peakFreq = 300000;

  uint32_t startKHz = 300000, stopKHz = 920000, stepKHz = 200;
  int totalSteps = (stopKHz - startKHz) / stepKHz;
  int stepsDone = 0;

  unsigned long holdStart = 0; bool holding = false;

  for (uint32_t f = startKHz; f <= stopKHz; f += stepKHz) {
    dhCcRetune(f);
    int rssi = dhCcRssiDbm(dhCcRead(CC_RSSI_REG));
    stepsDone++;

    if (rssi > peakDbm) { peakDbm = rssi; peakFreq = f; }
    if (rssi > -80 && hitCount < 6) {
      hits[hitCount].freqKHz = f;
      hits[hitCount].rssi = rssi;
      hitCount++;
    }

    if (stepsDone % 50 == 0) {
      tft.fillRect(0, SCALE_Y(70), SCR_W, SCALE_Y(30), CLR_BG);
      int pct = (stepsDone * 100) / totalSteps;
      tft.drawRect(SCALE_X(8), SCALE_Y(72), SCR_W - SCALE_X(16), SCALE_Y(10), CLR_BORDER);
      tft.fillRect(SCALE_X(9), SCALE_Y(73), ((SCR_W - SCALE_X(18)) * pct) / 100, SCALE_Y(8), CLR_SUCCESS);
      char buf[16]; sprintf(buf, "%d%%", pct);
      tft.setTextColor(CLR_TEXT_MED); tft.drawCentreString(buf, SCR_CX, SCALE_Y(86), 1);
    }

    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) {
      if (!holding) { holdStart = millis(); holding = true; }
      if (millis() - holdStart > 800) { dhCcStrobe(CC_SIDLE); return; }
    } else { holding = false; }
  }

  // Show results
  tft.fillScreen(CLR_BG);
  drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_SUCCESS, darkenColor(CLR_SUCCESS, 50), 0);
  tft.setTextColor(CLR_TEXT_HI);
  tft.drawString("BRUTE DONE", SCALE_X(4), SCALE_Y(2), 1);

  char buf[32]; sprintf(buf, "Peak: %s %ddB", dhCcFmtFreq(peakFreq).c_str(), peakDbm);
  tft.setTextColor(CLR_WARNING); tft.drawString(buf, SCALE_X(4), SCALE_Y(22), 1);

  sprintf(buf, "Hits >-80dB: %d", hitCount);
  tft.setTextColor(CLR_TEXT_MED); tft.drawString(buf, SCALE_X(4), SCALE_Y(36), 1);

  int y = SCALE_Y(50);
  for (int i = 0; i < hitCount; i++) {
    sprintf(buf, "%s %ddB", dhCcFmtFreq(hits[i].freqKHz).c_str(), hits[i].rssi);
    tft.setTextColor(dhCcRssiColor(hits[i].rssi));
    tft.drawString(buf, SCALE_X(8), y, 1);
    y += SCALE_Y(12);
    if (y > SCR_H - SCALE_Y(14)) break;
  }

  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("SEL: back", SCALE_X(4), SCR_H - SCALE_Y(10), 1);
  dhWaitSelectPress();
  delay(200);
  dhCcStrobe(CC_SIDLE);
}

// ═══════════════════════════════════════════════════════════
// TOOL 7: Code Check — Fixed vs rolling code detector
// ═══════════════════════════════════════════════════════════

static void dhRunCcCodeCheck() {
  if (!dhCcPrepareRx(433920)) { dhCcShowError("CODE CHECK"); return; }

  tft.fillScreen(CLR_BG);
  drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_PRIMARY, darkenColor(CLR_PRIMARY, 50), 0);
  tft.setTextColor(CLR_TEXT_HI);
  tft.drawString("CODE CHECK", SCALE_X(4), SCALE_Y(2), 1);
  tft.setTextColor(CLR_TEXT_MED);
  tft.drawString("Press remote 3x", SCALE_X(4), SCALE_Y(24), 1);
  tft.drawString("at 433.92MHz", SCALE_X(4), SCALE_Y(36), 1);

  // Capture 3 signals
  uint32_t hashes[3] = {0, 0, 0};
  int captured = 0;

  for (int c = 0; c < 3; c++) {
    char buf[16]; sprintf(buf, "Press #%d...", c+1);
    tft.fillRect(SCALE_X(4), SCALE_Y(54), SCR_W - SCALE_X(8), SCALE_Y(14), CLR_BG);
    tft.setTextColor(CLR_WARNING); tft.drawString(buf, SCALE_X(4), SCALE_Y(54), 1);

    // Wait for GDO0 edges (signal detected)
    int lastGdo = digitalRead(CC1101_GDO0_PIN);
    int edgeCount = 0;
    unsigned long timeout = millis();
    while (millis() - timeout < 10000) {
      int gdo = digitalRead(CC1101_GDO0_PIN);
      if (gdo != lastGdo) { edgeCount++; lastGdo = gdo; }
      if (edgeCount >= 10) break;
      if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) { dhCcStrobe(CC_SIDLE); return; }
      delay(1);
    }

    if (edgeCount < 10) continue;

    // Simple hash from edge timings
    uint32_t hash = 0x811C9DC5;
    unsigned long t0 = micros();
    for (int i = 0; i < 32; i++) {
      unsigned long wait = micros();
      while (digitalRead(CC1101_GDO0_PIN) == lastGdo && (micros() - wait) < 5000) {}
      lastGdo = !lastGdo;
      uint32_t dt = (uint32_t)(micros() - wait);
      hash ^= dt; hash *= 0x01000193;
    }
    hashes[c] = hash;
    captured++;

    sprintf(buf, "Got #%d: %08X", c+1, hash);
    tft.fillRect(SCALE_X(4), SCALE_Y(68 + c * 12), SCR_W - SCALE_X(8), SCALE_Y(12), CLR_BG);
    tft.setTextColor(CLR_SUCCESS);
    tft.drawString(buf, SCALE_X(4), SCALE_Y(68 + c * 12), 1);
    delay(500);
  }

  // Compare
  tft.fillRect(SCALE_X(4), SCALE_Y(106), SCR_W - SCALE_X(8), SCALE_Y(14), CLR_BG);
  if (captured >= 2) {
    bool allSame = true;
    for (int i = 1; i < captured; i++)
      if (hashes[i] != hashes[0]) allSame = false;

    if (allSame) {
      tft.setTextColor(CLR_SUCCESS);
      tft.drawString("FIXED CODE", SCALE_X(4), SCALE_Y(106), 1);
      tft.setTextColor(CLR_TEXT_MED);
      tft.drawString("Same hash each time", SCALE_X(4), SCALE_Y(118), 1);
    } else {
      tft.setTextColor(CLR_WARNING);
      tft.drawString("ROLLING CODE", SCALE_X(4), SCALE_Y(106), 1);
      tft.setTextColor(CLR_TEXT_MED);
      tft.drawString("Different each time", SCALE_X(4), SCALE_Y(118), 1);
    }
  } else {
    tft.setTextColor(CLR_SECONDARY);
    tft.drawString("Not enough captures", SCALE_X(4), SCALE_Y(106), 1);
  }

  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("SEL: back", SCALE_X(4), SCR_H - SCALE_Y(10), 1);
  dhWaitSelectPress();
  delay(200);
  dhCcStrobe(CC_SIDLE);
}

// ═══════════════════════════════════════════════════════════
// TOOL 8: RF Analyzer — Pulse timing analysis
// ═══════════════════════════════════════════════════════════

static void dhRunCcRfAnalyze() {
  if (!dhCcPrepareRx(433920)) { dhCcShowError("RF ANALYZE"); return; }

  tft.fillScreen(CLR_BG);
  drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_PRIMARY, darkenColor(CLR_PRIMARY, 50), 0);
  tft.setTextColor(CLR_TEXT_HI);
  tft.drawString("RF ANALYZE", SCALE_X(4), SCALE_Y(2), 1);
  tft.setTextColor(CLR_TEXT_MED);
  tft.drawString("Press remote...", SCALE_X(4), SCALE_Y(24), 1);

  // Wait for signal
  int lastGdo = digitalRead(CC1101_GDO0_PIN);
  uint16_t pulses[DH_RF_RAW_MAX];
  int pulseCount = 0;
  unsigned long timeout = millis();

  while (millis() - timeout < 15000) {
    int gdo = digitalRead(CC1101_GDO0_PIN);
    if (gdo != lastGdo) {
      unsigned long start = micros();
      // Capture pulse train
      while (pulseCount < DH_RF_RAW_MAX) {
        unsigned long wait = micros();
        while (digitalRead(CC1101_GDO0_PIN) == gdo && (micros() - wait) < 9000) {}
        uint16_t duration = (uint16_t)(micros() - wait);
        if (duration >= 9000) break;
        pulses[pulseCount++] = duration;
        gdo = !gdo;
      }
      break;
    }
    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) { dhCcStrobe(CC_SIDLE); return; }
    delay(1);
  }

  if (pulseCount < 10) {
    tft.setTextColor(CLR_WARNING);
    tft.drawCentreString("No signal", SCR_CX, SCR_CY, 1);
    delay(2000); dhCcStrobe(CC_SIDLE); return;
  }

  // Analyze
  uint32_t sumHi = 0, sumLo = 0;
  int hiCount = 0, loCount = 0;
  uint16_t minP = 65535, maxP = 0;
  uint32_t hash = 0x811C9DC5;

  for (int i = 0; i < pulseCount; i++) {
    if (i % 2 == 0) { sumHi += pulses[i]; hiCount++; }
    else { sumLo += pulses[i]; loCount++; }
    if (pulses[i] < minP) minP = pulses[i];
    if (pulses[i] > maxP) maxP = pulses[i];
    hash ^= pulses[i]; hash *= 0x01000193;
  }

  uint32_t totalUs = 0;
  for (int i = 0; i < pulseCount; i++) totalUs += pulses[i];

  tft.fillScreen(CLR_BG);
  drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_SUCCESS, darkenColor(CLR_SUCCESS, 50), 0);
  tft.setTextColor(CLR_TEXT_HI);
  tft.drawString("RF ANALYSIS", SCALE_X(4), SCALE_Y(2), 1);

  int y = SCALE_Y(18); int lineH = SCALE_Y(11);
  char buf[32];
  sprintf(buf, "Pulses: %d", pulseCount);
  tft.setTextColor(CLR_TEXT_MED); tft.drawString(buf, SCALE_X(4), y, 1); y += lineH;
  sprintf(buf, "Dur: %lums", totalUs / 1000);
  tft.drawString(buf, SCALE_X(4), y, 1); y += lineH;
  sprintf(buf, "Min: %uus", minP);
  tft.drawString(buf, SCALE_X(4), y, 1); y += lineH;
  sprintf(buf, "Max: %uus", maxP);
  tft.drawString(buf, SCALE_X(4), y, 1); y += lineH;
  sprintf(buf, "AvgHi: %luus", hiCount > 0 ? sumHi/hiCount : 0);
  tft.drawString(buf, SCALE_X(4), y, 1); y += lineH;
  sprintf(buf, "AvgLo: %luus", loCount > 0 ? sumLo/loCount : 0);
  tft.drawString(buf, SCALE_X(4), y, 1); y += lineH;
  sprintf(buf, "Type: OOK/ASK");
  tft.setTextColor(CLR_PRIMARY); tft.drawString(buf, SCALE_X(4), y, 1); y += lineH;
  sprintf(buf, "Hash: %08X", hash);
  tft.setTextColor(CLR_WARNING); tft.drawString(buf, SCALE_X(4), y, 1);

  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("SEL: back", SCALE_X(4), SCR_H - SCALE_Y(10), 1);
  dhWaitSelectPress();
  delay(200);
  dhCcStrobe(CC_SIDLE);
}

// ═══════════════════════════════════════════════════════════
// TOOL 9: RF Raw View — Pulse waveform display
// ═══════════════════════════════════════════════════════════

static void dhRunCcRawView() {
  if (!dhHasRfCapture) {
    tft.fillScreen(CLR_BG);
    tft.setTextColor(CLR_WARNING);
    tft.drawCentreString("No RF capture!", SCR_CX, SCALE_Y(40), 1);
    tft.drawCentreString("Use RF Analyze first", SCR_CX, SCALE_Y(56), 1);
    tft.setTextColor(CLR_TEXT_LO);
    tft.drawString("SEL: back", SCALE_X(4), SCR_H - SCALE_Y(10), 1);
    dhWaitSelectPress();
    delay(200); return;
  }

  int page = 0;
  int pulsesPerPage = 40;
  int totalPages = (dhRfCapture.count + pulsesPerPage - 1) / pulsesPerPage;
  if (totalPages < 1) totalPages = 1;

  auto drawPage = [&]() {
    tft.fillScreen(CLR_BG);
    drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_PRIMARY, darkenColor(CLR_PRIMARY, 50), 0);
    tft.setTextColor(CLR_TEXT_HI);
    char hdr[20]; sprintf(hdr, "RAW %d/%d", page+1, totalPages);
    tft.drawString(hdr, SCALE_X(4), SCALE_Y(2), 1);

    int plotY = SCALE_Y(18), plotH = SCALE_Y(76);
    int start = page * pulsesPerPage;
    int end = min(start + pulsesPerPage, (int)dhRfCapture.count);
    int count = end - start;
    if (count <= 0) return;

    // Find max for scaling
    uint16_t maxUs = 1;
    for (int i = start; i < end; i++)
      if (dhRfCapture.raw[i] > maxUs) maxUs = dhRfCapture.raw[i];

    int barW = max(1, (SCR_W - SCALE_X(8)) / count);
    for (int i = 0; i < count; i++) {
      int x = SCALE_X(4) + i * barW;
      int h = (dhRfCapture.raw[start + i] * (plotH - 4)) / maxUs;
      h = max(1, h);
      bool isHigh = ((start + i) % 2 == 0);
      uint16_t col = isHigh ? CLR_SUCCESS : CLR_SECONDARY;
      tft.fillRect(x, plotY + plotH - h - 2, max(1, barW - 1), h, col);
    }

    tft.setTextColor(CLR_TEXT_LO);
    tft.drawString("</>:Page HOLD:Back", SCALE_X(2), SCR_H - SCALE_Y(10), 1);
  };

  drawPage();
  unsigned long holdStart = 0; bool holding = false;

  while (true) {
    extern void handleNetwork(); handleNetwork();
    if ((digitalRead(BTN_LEFT) == LOW || (virtualLeftPressed ? (virtualLeftPressed=false, true) : false))) { page = (page + totalPages - 1) % totalPages; drawPage(); delay(200); }
    if ((digitalRead(BTN_RIGHT) == LOW || (virtualRightPressed ? (virtualRightPressed=false, true) : false))) { page = (page + 1) % totalPages; drawPage(); delay(200); }
    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) {
      if (!holding) { holdStart = millis(); holding = true; }
      if (millis() - holdStart > 800) return;
    } else { holding = false; }
    delay(10);
  }
}

// ═══════════════════════════════════════════════════════════
// TOOL 10: RF Live — Carrier detect + RSSI + event counter
// ═══════════════════════════════════════════════════════════

static void dhRunCcRfLive() {
  if (!dhCcPrepareRx(433920)) { dhCcShowError("RF LIVE"); return; }

  int lastGdo = digitalRead(CC1101_GDO0_PIN);
  unsigned long edges = 0, events = 0, lastDraw = 0;
  int peakDbm = -127;
  unsigned long holdStart = 0; bool holding = false;

  while (true) {
    extern void handleNetwork(); handleNetwork();
    int gdo = digitalRead(CC1101_GDO0_PIN);
    if (gdo != lastGdo) { edges++; lastGdo = gdo; }
    if (gdo == HIGH && edges > 2) { events++; edges = 0; }

    if (millis() - lastDraw >= 200) {
      int rssi = dhCcRssiDbm(dhCcRead(CC_RSSI_REG));
      if (rssi > peakDbm) peakDbm = rssi;

      tft.fillScreen(CLR_BG);
      drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_PRIMARY, darkenColor(CLR_PRIMARY, 50), 0);
      tft.setTextColor(CLR_TEXT_HI);
      tft.drawString("RF LIVE 433M", SCALE_X(4), SCALE_Y(2), 1);

      // Big RSSI display
      tft.setTextColor(dhCcRssiColor(rssi));
      char buf[16]; sprintf(buf, "%ddBm", rssi);
      tft.drawCentreString(buf, SCR_CX, SCALE_Y(22), 2);

      // Carrier sense indicator
      bool carrier = gdo == HIGH;
      tft.fillCircle(SCR_CX, SCALE_Y(52), SCALE_Y(8), carrier ? CLR_SECONDARY : CLR_SURFACE);
      tft.setTextColor(CLR_TEXT_MED);
      tft.drawCentreString(carrier ? "SIGNAL" : "quiet", SCR_CX, SCALE_Y(66), 1);

      sprintf(buf, "Events: %lu", events);
      tft.setTextColor(CLR_TEXT_MED); tft.drawString(buf, SCALE_X(4), SCALE_Y(80), 1);
      sprintf(buf, "Peak: %ddB", peakDbm);
      tft.drawString(buf, SCALE_X(4), SCALE_Y(92), 1);

      tft.setTextColor(CLR_TEXT_LO);
      tft.drawString("HOLD SEL: exit", SCALE_X(4), SCR_H - SCALE_Y(10), 1);
      lastDraw = millis();
    }

    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) {
      if (!holding) { holdStart = millis(); holding = true; }
      if (millis() - holdStart > 800) { dhCcStrobe(CC_SIDLE); return; }
    } else { holding = false; }
    delay(2);
  }
}

// ═══════════════════════════════════════════════════════════
// TOOL 11: Lab Replay — OOK replay for fixed-code devices
// ═══════════════════════════════════════════════════════════

static void dhRunCcLabReplay() {
  tft.fillScreen(CLR_BG);
  tft.drawRect(0, 0, SCR_W, SCR_H, CLR_WARNING);
  tft.setTextColor(CLR_WARNING);
  tft.drawCentreString("LAB REPLAY", SCR_CX, SCALE_Y(8), 1);
  tft.setTextColor(CLR_TEXT_MED);
  tft.drawCentreString("OOK/ASK replay", SCR_CX, SCALE_Y(30), 1);
  tft.drawCentreString("YOUR devices only!", SCR_CX, SCALE_Y(46), 1);
  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("SEL:Accept <:Cancel", SCALE_X(2), SCR_H - SCALE_Y(12), 1);

  while (true) {
    extern void handleNetwork(); handleNetwork();
    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) { delay(200); break; }
    if ((digitalRead(BTN_LEFT) == LOW || (virtualLeftPressed ? (virtualLeftPressed=false, true) : false))) { delay(200); return; }
    delay(20);
  }

  // First capture
  if (!dhCcPrepareRx(433920)) { dhCcShowError("LAB REPLAY"); return; }

  tft.fillScreen(CLR_BG);
  tft.setTextColor(CLR_PRIMARY);
  tft.drawCentreString("Press remote...", SCR_CX, SCALE_Y(40), 1);

  int lastGdo = digitalRead(CC1101_GDO0_PIN);
  dhRfCapture.count = 0;
  unsigned long timeout = millis();

  while (millis() - timeout < 15000) {
    int gdo = digitalRead(CC1101_GDO0_PIN);
    if (gdo != lastGdo) {
      while (dhRfCapture.count < DH_RF_RAW_MAX) {
        unsigned long wait = micros();
        while (digitalRead(CC1101_GDO0_PIN) == gdo && (micros() - wait) < 9000) {}
        uint16_t dur = (uint16_t)(micros() - wait);
        if (dur >= 9000) break;
        dhRfCapture.raw[dhRfCapture.count++] = dur;
        gdo = !gdo;
      }
      break;
    }
    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) { dhCcStrobe(CC_SIDLE); return; }
    delay(1);
  }

  if (dhRfCapture.count < 24) {
    tft.setTextColor(CLR_WARNING);
    tft.drawCentreString("Too short!", SCR_CX, SCR_CY, 1);
    delay(2000); dhCcStrobe(CC_SIDLE); return;
  }

  dhHasRfCapture = true;
  dhRfCaptureFreqKHz = 433920;

  // Setup for TX
  dhCcStrobe(CC_SIDLE); delay(1);
  dhCcWrite(CC_IOCFG0, CC_GDO_ASYNC_DATA);
  dhCcWrite(CC_PKTCTRL0, 0x32);
  uint8_t patable[] = {0xC0};  // Max power for 433MHz
  dhCcWriteBurst(CC_PATABLE, patable, 1);
  dhCcSetFreq(433920);
  pinMode(CC1101_TX_DATA_PIN, OUTPUT);
  digitalWrite(CC1101_TX_DATA_PIN, LOW);
  dhCcStrobe(CC_SFTX);
  dhCcStrobe(CC_SCAL); delay(3);

  tft.fillScreen(CLR_BG);
  drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_WARNING, darkenColor(CLR_WARNING, 50), 0);
  tft.setTextColor(CLR_TEXT_HI);
  tft.drawString("REPLAY READY", SCALE_X(4), SCALE_Y(2), 1);

  char buf[24]; sprintf(buf, "Pulses: %d", dhRfCapture.count);
  tft.setTextColor(CLR_TEXT_MED); tft.drawString(buf, SCALE_X(4), SCALE_Y(24), 1);
  tft.setTextColor(CLR_TEXT_MED);
  tft.drawString("SEL: Transmit", SCALE_X(4), SCALE_Y(44), 1);
  tft.drawString("HOLD: exit", SCALE_X(4), SCALE_Y(56), 1);

  int txCount = 0;
  unsigned long holdStart = 0; bool holding = false;

  while (true) {
    extern void handleNetwork(); handleNetwork();
    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) {
      if (!holding) { holdStart = millis(); holding = true; }
      if (millis() - holdStart > 800) break;
    } else {
      if (holding && millis() - holdStart < 800) {
        // Transmit 6 repeats
        dhCcStrobe(CC_STX);
        for (int rep = 0; rep < 6; rep++) {
          for (int i = 0; i < dhRfCapture.count; i++) {
            bool high = (i % 2 == 0);
            digitalWrite(CC1101_TX_DATA_PIN, high ? HIGH : LOW);
            delayMicroseconds(dhRfCapture.raw[i]);
          }
          digitalWrite(CC1101_TX_DATA_PIN, LOW);
          delayMicroseconds(11000);
        }
        dhCcStrobe(CC_SIDLE);
        txCount++;

        tft.fillRect(SCALE_X(4), SCALE_Y(72), SCR_W - SCALE_X(8), SCALE_Y(14), CLR_BG);
        sprintf(buf, "Sent! (x%d)", txCount);
        tft.setTextColor(CLR_SUCCESS); tft.drawString(buf, SCALE_X(4), SCALE_Y(72), 1);
      }
      holding = false;
    }
    delay(10);
  }

  digitalWrite(CC1101_TX_DATA_PIN, LOW);
  dhCcStrobe(CC_SIDLE);
  pinMode(CC1101_TX_DATA_PIN, INPUT);
}

// ═══════════════════════════════════════════════════════════
// TOOL 12: Test Beacon — TX burst to verify RF output
// ═══════════════════════════════════════════════════════════

static void dhRunCcTestBeacon() {
  dhCcInitPins();
  dhCcReset();
  dhCcConfigOokRx();
  dhCcSetFreq(433920);
  dhCcWrite(CC_IOCFG0, CC_GDO_ASYNC_DATA);
  dhCcWrite(CC_PKTCTRL0, 0x32);
  uint8_t patable[] = {0xC0};
  dhCcWriteBurst(CC_PATABLE, patable, 1);
  pinMode(CC1101_TX_DATA_PIN, OUTPUT);
  digitalWrite(CC1101_TX_DATA_PIN, LOW);
  dhCcStrobe(CC_SIDLE); delay(1);
  dhCcStrobe(CC_SFTX);
  dhCcStrobe(CC_SCAL); delay(3);

  bool ready = false;
  dhCcRead(CC_PARTNUM, &ready);
  if (!ready) { dhCcShowError("BEACON"); return; }

  tft.fillScreen(CLR_BG);
  drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_WARNING, darkenColor(CLR_WARNING, 50), 0);
  tft.setTextColor(CLR_TEXT_HI);
  tft.drawString("TEST BEACON", SCALE_X(4), SCALE_Y(2), 1);
  tft.setTextColor(CLR_TEXT_MED);
  tft.drawString("433.92 MHz OOK", SCALE_X(4), SCALE_Y(24), 1);
  tft.drawString("Sending 3 bursts...", SCALE_X(4), SCALE_Y(40), 1);

  dhCcStrobe(CC_STX);

  for (int burst = 0; burst < 3; burst++) {
    char buf[16]; sprintf(buf, "Burst %d/3", burst+1);
    tft.fillRect(SCALE_X(4), SCALE_Y(60), SCR_W - SCALE_X(8), SCALE_Y(14), CLR_BG);
    tft.setTextColor(CLR_SUCCESS); tft.drawString(buf, SCALE_X(4), SCALE_Y(60), 1);

    // Toggle TX data at ~2.5kHz for 1s
    unsigned long end = millis() + 1000;
    while (millis() < end) {
      digitalWrite(CC1101_TX_DATA_PIN, HIGH);
      delayMicroseconds(200);
      digitalWrite(CC1101_TX_DATA_PIN, LOW);
      delayMicroseconds(200);
    }
    delay(250);
  }

  dhCcStrobe(CC_SIDLE);
  digitalWrite(CC1101_TX_DATA_PIN, LOW);
  pinMode(CC1101_TX_DATA_PIN, INPUT);

  tft.setTextColor(CLR_SUCCESS);
  tft.drawString("Done! Check with", SCALE_X(4), SCALE_Y(78), 1);
  tft.drawString("spectrum analyzer.", SCALE_X(4), SCALE_Y(90), 1);

  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("SEL: back", SCALE_X(4), SCR_H - SCALE_Y(10), 1);
  dhWaitSelectPress();
  delay(200);
}

#endif // ESP32
#endif // DH_CC1101_TOOLS_H
