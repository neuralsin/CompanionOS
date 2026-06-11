// ═══════════════════════════════════════════════════════════
// COMPANION OS v7.0 — DR. HACK: RADIO TOOLS
// 2.4GHz Jammer (nRF24L01), Radio Scanner (Spectrum)
// Adapted from ESP32-TOOLS-PRO for 160×128 ST7735R
// ═══════════════════════════════════════════════════════════
#ifndef DH_RADIO_TOOLS_H
#define DH_RADIO_TOOLS_H

#ifdef ESP32

#include "globals.h"
#include "ui_components.h"
#include <SPI.h>
#include <RF24.h>

// ═══════════════════════════════════════════════════════════
// TOOL: 2.4GHz Jammer — Dual nRF24L01 Wi-Fi channel jammer
// ═══════════════════════════════════════════════════════════

static uint8_t dhJamChToNrf(int wifiCh) {
  return (uint8_t)((wifiCh * 5) + 2);
}

static void dhRunJammer24() {
  RF24 jam1(NRF1_CE_PIN, NRF1_CSN_PIN);
  RF24 jam2(NRF2_CE_PIN, NRF2_CSN_PIN);

  // Ensure TFT CS is high before nRF24 SPI access
  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
  pinMode(NRF1_CSN_PIN, OUTPUT); digitalWrite(NRF1_CSN_PIN, HIGH);
  pinMode(NRF2_CSN_PIN, OUTPUT); digitalWrite(NRF2_CSN_PIN, HIGH);
  pinMode(NRF1_CE_PIN, OUTPUT); digitalWrite(NRF1_CE_PIN, LOW);
  pinMode(NRF2_CE_PIN, OUTPUT); digitalWrite(NRF2_CE_PIN, LOW);

  SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI);
  delay(20);

  bool j1Ok = jam1.begin();
  if (j1Ok) {
    jam1.powerUp(); jam1.setAddressWidth(3); jam1.setRetries(0,0);
    jam1.setDataRate(RF24_2MBPS); jam1.setPALevel(RF24_PA_MAX);
    jam1.setCRCLength(RF24_CRC_DISABLED); jam1.setAutoAck(false);
    jam1.openWritingPipe((uint8_t*)"JAM"); jam1.stopListening();
  }
  bool j2Ok = jam2.begin();
  if (j2Ok) {
    jam2.powerUp(); jam2.setAddressWidth(3); jam2.setRetries(0,0);
    jam2.setDataRate(RF24_2MBPS); jam2.setPALevel(RF24_PA_MAX);
    jam2.setCRCLength(RF24_CRC_DISABLED); jam2.setAutoAck(false);
    jam2.openWritingPipe((uint8_t*)"JAM"); jam2.stopListening();
  }

  int activeCount = (j1Ok?1:0) + (j2Ok?1:0);
  if (activeCount == 0) {
    tft.fillScreen(CLR_BG);
    tft.setTextColor(CLR_SECONDARY);
    tft.drawCentreString("nRF24 ERROR", SCR_CX, SCALE_Y(40), 1);
    tft.setTextColor(CLR_TEXT_MED);
    tft.drawCentreString("Check SPI wiring", SCR_CX, SCALE_Y(60), 1);
    char pins[40]; sprintf(pins, "CE:%d/%d CSN:%d/%d", NRF1_CE_PIN, NRF2_CE_PIN, NRF1_CSN_PIN, NRF2_CSN_PIN);
    tft.setTextColor(CLR_TEXT_LO);
    tft.drawCentreString(pins, SCR_CX, SCALE_Y(80), 1);
    delay(3000);
    return;
  }

  int jamCh = 1;
  bool isAttacking = false;
  const uint8_t noise[32] = {
    0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,
    0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,
    0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,
    0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA
  };

  auto drawGauge = [&]() {
    tft.fillScreen(CLR_BG);
    tft.drawRect(0, 0, SCR_W, SCR_H, isAttacking ? CLR_SECONDARY : CLR_PRIMARY);

    drawGradientCard(2, 2, SCR_W-4, SCALE_Y(14), isAttacking ? CLR_SECONDARY : CLR_PRIMARY,
                     darkenColor(isAttacking ? CLR_SECONDARY : CLR_PRIMARY, 50), 0);
    tft.setTextColor(CLR_TEXT_HI);
    tft.drawString("JAMMER", SCALE_X(4), SCALE_Y(3), 1);
    tft.setTextColor(isAttacking ? CLR_SECONDARY : CLR_SUCCESS);
    tft.drawString(isAttacking ? "ACTIVE" : "READY", SCR_W - SCALE_X(38), SCALE_Y(3), 1);

    // Channel display
    char chStr[16]; sprintf(chStr, "CH %d", jamCh);
    tft.setTextColor(CLR_WARNING);
    tft.drawCentreString(chStr, SCR_CX, SCALE_Y(22), 2);

    char freqStr[20]; sprintf(freqStr, "%d MHz NRF", 2400 + dhJamChToNrf(jamCh));
    tft.setTextColor(CLR_PRIMARY);
    tft.drawCentreString(freqStr, SCR_CX, SCALE_Y(42), 1);

    // Channel bar
    int pct = 7 + ((jamCh - 1) * 93) / 13;
    tft.drawRect(SCALE_X(8), SCALE_Y(56), SCR_W - SCALE_X(16), SCALE_Y(8), CLR_BORDER);
    int fillW = ((SCR_W - SCALE_X(18)) * pct) / 100;
    tft.fillRect(SCALE_X(9), SCALE_Y(57), fillW, SCALE_Y(6), isAttacking ? CLR_SECONDARY : CLR_SUCCESS);

    // Radio status
    char radios[20]; sprintf(radios, "Radios: %d/2", activeCount);
    tft.setTextColor(activeCount > 0 ? CLR_SUCCESS : CLR_SECONDARY);
    tft.drawString(radios, SCALE_X(4), SCALE_Y(70), 1);

    if (!isAttacking) {
      tft.drawRect(SCALE_X(20), SCALE_Y(80), SCR_W - SCALE_X(40), SCALE_Y(14), CLR_BORDER);
      tft.setTextColor(CLR_SUCCESS);
      tft.drawCentreString("SEL: START", SCR_CX, SCALE_Y(83), 1);
    }

    tft.setTextColor(CLR_TEXT_LO);
    tft.drawString("</>:CH HOLD:Back", SCALE_X(2), SCR_H - SCALE_Y(10), 1);
  };

  drawGauge();
  unsigned long holdStart = 0; bool holding = false;
  unsigned long lastDraw = millis();

  while (true) {
    extern void handleNetwork(); handleNetwork();
    if ((digitalRead(BTN_LEFT) == LOW || (virtualLeftPressed ? (virtualLeftPressed=false, true) : false))) {
      jamCh = (jamCh == 1) ? 14 : jamCh - 1;
      if (isAttacking && j1Ok) jam1.startConstCarrier(RF24_PA_MAX, dhJamChToNrf(jamCh));
      drawGauge(); delay(180);
    }
    if ((digitalRead(BTN_RIGHT) == LOW || (virtualRightPressed ? (virtualRightPressed=false, true) : false))) {
      jamCh = (jamCh == 14) ? 1 : jamCh + 1;
      if (isAttacking && j1Ok) jam1.startConstCarrier(RF24_PA_MAX, dhJamChToNrf(jamCh));
      drawGauge(); delay(180);
    }

    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) {
      if (!holding) { holdStart = millis(); holding = true; }
      if (millis() - holdStart > 800) {
        if (isAttacking) { if (j1Ok) jam1.stopConstCarrier(); if (j2Ok) jam2.stopConstCarrier(); }
        if (j1Ok) jam1.powerDown();
        if (j2Ok) jam2.powerDown();
        return;
      }
    } else {
      if (holding && millis() - holdStart < 800) {
        isAttacking = !isAttacking;
        uint8_t freq = dhJamChToNrf(jamCh);
        if (isAttacking) {
          if (j1Ok) jam1.startConstCarrier(RF24_PA_MAX, freq);
          if (j2Ok) jam2.setChannel(freq);
        } else {
          if (j1Ok) jam1.stopConstCarrier();
          if (j2Ok) jam2.stopConstCarrier();
        }
        drawGauge();
      }
      holding = false;
    }

    if (isAttacking) {
      uint8_t freq = dhJamChToNrf(jamCh);
      if (j2Ok) {
        jam2.setChannel(freq);
        for (int i = 0; i < 20; i++) jam2.startWrite(noise, 32, true);
      }

      if (millis() - lastDraw > 250) {
        // Animated activity bars at bottom
        tft.fillRect(SCALE_X(4), SCALE_Y(80), SCR_W - SCALE_X(8), SCALE_Y(24), CLR_BG);
        for (int i = 0; i < 12; i++) {
          int h = 4 + random(0, SCALE_Y(20));
          uint16_t c = h > SCALE_Y(14) ? CLR_SECONDARY : CLR_WARNING;
          tft.fillRect(SCALE_X(6) + i * SCALE_X(12), SCALE_Y(104) - h, SCALE_X(8), h, c);
        }
        lastDraw = millis();
      }
    }

    delay(5);
  }
}

// ═══════════════════════════════════════════════════════════
// TOOL: Radio Scanner — 2.4GHz spectrum analyzer
// Sweeps all 126 nRF24 channels, shows RSSI as bar graph
// ═══════════════════════════════════════════════════════════

#define DH_SCAN_CHANNELS 126
#define DH_SCAN_DWELL_US 170

static uint8_t dhScanValues[DH_SCAN_CHANNELS];
static uint8_t dhScanPeaks[DH_SCAN_CHANNELS];

static void dhRunRadioScanner() {
  RF24 radio(NRF1_CE_PIN, NRF1_CSN_PIN);

  pinMode(TFT_CS, OUTPUT); digitalWrite(TFT_CS, HIGH);
  pinMode(NRF1_CSN_PIN, OUTPUT); digitalWrite(NRF1_CSN_PIN, HIGH);

  SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI);
  delay(20);

  bool ok = radio.begin();
  if (!ok) {
    tft.fillScreen(CLR_BG);
    tft.setTextColor(CLR_SECONDARY);
    tft.drawCentreString("nRF24 ERROR", SCR_CX, SCALE_Y(40), 1);
    delay(2500);
    return;
  }

  radio.setAutoAck(false);
  radio.setDataRate(RF24_2MBPS);
  radio.setPALevel(RF24_PA_MIN);
  radio.stopListening();

  memset(dhScanValues, 0, DH_SCAN_CHANNELS);
  memset(dhScanPeaks, 0, DH_SCAN_CHANNELS);

  unsigned long holdStart = 0; bool holding = false;
  bool showWaterfall = false;
  uint32_t sweepCount = 0;

  // Waterfall buffer: 40 rows of compressed data
  #define DH_WF_ROWS 40
  #define DH_WF_COLS 80
  static uint8_t waterfall[DH_WF_ROWS][DH_WF_COLS];
  int wfHead = 0;
  memset(waterfall, 0, sizeof(waterfall));

  while (true) {
    extern void handleNetwork(); handleNetwork();
    // Sweep all channels
    for (int ch = 0; ch < DH_SCAN_CHANNELS; ch++) {
      radio.setChannel(ch);
      radio.startListening();
      delayMicroseconds(DH_SCAN_DWELL_US);
      radio.stopListening();

      bool detected = radio.testCarrier();
      if (detected) {
        if (dhScanValues[ch] < 255) dhScanValues[ch]++;
        if (dhScanValues[ch] > dhScanPeaks[ch]) dhScanPeaks[ch] = dhScanValues[ch];
      } else {
        if (dhScanValues[ch] > 0) dhScanValues[ch]--;
      }
    }
    sweepCount++;

    // Store waterfall row (compress 126→80)
    for (int i = 0; i < DH_WF_COLS; i++) {
      int srcCh = (i * DH_SCAN_CHANNELS) / DH_WF_COLS;
      waterfall[wfHead][i] = dhScanValues[srcCh];
    }
    wfHead = (wfHead + 1) % DH_WF_ROWS;

    // Draw
    tft.fillScreen(CLR_BG);
    drawGradientCard(0, 0, SCR_W, SCALE_Y(12), CLR_SUCCESS, darkenColor(CLR_SUCCESS, 50), 0);
    tft.setTextColor(CLR_TEXT_HI);
    char hdr[24]; sprintf(hdr, "%s #%lu", showWaterfall ? "WATERFALL" : "SPECTRUM", sweepCount);
    tft.drawString(hdr, SCALE_X(2), SCALE_Y(1), 1);

    if (!showWaterfall) {
      // Spectrum bar view
      int plotY = SCALE_Y(16), plotH = SCALE_Y(80);
      int barW = max(1, (int)(SCR_W / DH_SCAN_CHANNELS));
      int maxVal = 1;
      for (int i = 0; i < DH_SCAN_CHANNELS; i++)
        if (dhScanValues[i] > maxVal) maxVal = dhScanValues[i];

      for (int i = 0; i < DH_SCAN_CHANNELS; i++) {
        int x = (i * SCR_W) / DH_SCAN_CHANNELS;
        int h = (dhScanValues[i] * plotH) / max(maxVal, 1);
        if (h > 0) {
          uint16_t col = h > plotH * 3 / 4 ? CLR_SECONDARY :
                         (h > plotH / 2 ? CLR_WARNING : CLR_SUCCESS);
          tft.drawFastVLine(x, plotY + plotH - h, h, col);
        }
        // Peak dot
        int ph = (dhScanPeaks[i] * plotH) / max(maxVal, 1);
        if (ph > 0) tft.drawPixel(x, plotY + plotH - ph, CLR_TEXT_LO);
      }

      // Channel labels
      tft.setTextColor(CLR_TEXT_LO);
      tft.drawString("2400", SCALE_X(0), plotY + plotH + SCALE_Y(2), 1);
      tft.drawString("2525", SCR_W - SCALE_X(24), plotY + plotH + SCALE_Y(2), 1);

      // WiFi channel markers
      for (int wch = 1; wch <= 13; wch++) {
        int nrf = (wch * 5) + 2;
        int x = (nrf * SCR_W) / DH_SCAN_CHANNELS;
        tft.drawPixel(x, plotY + plotH + 1, CLR_PRIMARY);
      }
    } else {
      // Waterfall view
      int plotY = SCALE_Y(16), plotH = SCALE_Y(80);
      for (int row = 0; row < DH_WF_ROWS; row++) {
        int r = (wfHead + row) % DH_WF_ROWS;
        int y = plotY + (row * plotH) / DH_WF_ROWS;
        for (int col = 0; col < DH_WF_COLS; col++) {
          int x = (col * SCR_W) / DH_WF_COLS;
          uint8_t v = waterfall[r][col];
          if (v > 0) {
            uint16_t c = v > 5 ? CLR_SECONDARY : (v > 2 ? CLR_WARNING : CLR_SUCCESS);
            tft.drawPixel(x, y, c);
          }
        }
      }
    }

    tft.setTextColor(CLR_TEXT_LO);
    tft.drawString("</>:Mode HOLD:Back", SCALE_X(2), SCR_H - SCALE_Y(10), 1);

    // Input
    if ((digitalRead(BTN_LEFT) == LOW || (virtualLeftPressed ? (virtualLeftPressed=false, true) : false)) || (digitalRead(BTN_RIGHT) == LOW || (virtualRightPressed ? (virtualRightPressed=false, true) : false))) {
      showWaterfall = !showWaterfall;
      delay(200);
    }
    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) {
      if (!holding) { holdStart = millis(); holding = true; }
      if (millis() - holdStart > 800) {
        radio.powerDown();
        return;
      }
    } else { holding = false; }

    delay(5);
  }
}

#endif // ESP32
#endif // DH_RADIO_TOOLS_H
