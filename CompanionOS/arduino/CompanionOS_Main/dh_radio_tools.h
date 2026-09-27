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
// TOOL: 2.4GHz BlueJammer — 6-mode nRF24L01 channel jammer
// Aligned to ESP32-BlueJammer lab specifications:
//   Mode 1: Bluetooth Classic (79 CH, 2402-2480 MHz)
//   Mode 2: BLE (40 CH, 2400-2483.5 MHz)
//   Mode 3: BT + BLE (Dual Radio 79 + 40 CH)
//   Mode 4: BT + WiFi (Dual Radio 79 + 14 CH)
//   Mode 5: WiFi (14 CH, 2412-2484 MHz)
//   Mode 6: RC/Drone (126 CH, 2400-2525 MHz)
// ═══════════════════════════════════════════════════════════

// Spec-correct channel arrays (nRF24 channel = freq_MHz - 2400)

// Bluetooth Classic: 79 channels, 2402-2480 MHz, 1 MHz spacing
static const uint8_t ch_bt_classic[79] = {
   2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,17,18,19,20,
  21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
  41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,
  61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80
};
#define CH_BT_COUNT 79

// BLE: 40 channels, 2 MHz spacing
// Data channels 0-36 mapped to nRF24 channels + 3 advertising channels
static const uint8_t ch_ble_full[40] = {
   4, 6, 8,10,12,14,16,18,20,22,   // BLE data CH 0-9
  24,28,30,32,34,36,38,40,42,44,   // BLE data CH 10-19 (skip 26=adv CH38)
  46,48,50,52,54,56,58,60,62,64,   // BLE data CH 20-29
  66,68,70,72,74,76,78,             // BLE data CH 30-36
   2,26,80                          // BLE advertising CH 37,38,39
};
#define CH_BLE_COUNT 40

// WiFi: 14 channels, center frequencies mapped to nRF24 channels
// CH1=2412->nRF12, CH2=2417->nRF17, ... CH13=2472->nRF72, CH14=2484->nRF84
static const uint8_t ch_wifi_spec[14] = {
  12,17,22,27,32,37,42,47,52,57,62,67,72,84
};
#define CH_WIFI_COUNT 14

// RC/Drone: nRF24 supports channels 0-125 (2400-2525 MHz, 1 MHz spacing)
// No array needed - sweep 0..125 directly
#define CH_RC_COUNT 126

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

  // SPI initialized globally

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
    char errMsg[64];
    sprintf(errMsg, "nRF24 Init Failed\nCE:%d CSN:%d", NRF1_CE_PIN, NRF1_CSN_PIN);
    dhShowError(errMsg);
    return;
  }

  // BlueJammer 6-mode architecture matching lab specifications:
  // 1. Bluetooth_80_CH (Classic BT: 2.402 - 2.480 GHz)
  // 2. BluetoothLowEnergy_40_CH (BLE: 2.400 - 2.4835 GHz)
  // 3. Bluetooth-BluetoothLowEnergy_40-80_CH (BT & BLE dual band)
  // 4. Bluetooth-WiFi_14-80_CH (BT & WiFi dual band)
  // 5. WiFi_14_CH (WiFi 1-14: 2412 - 2484 MHz)
  // 6. RC/Drone (126 CH: 2400 - 2525 MHz)
  const char* modeNames[] = {"Bluetooth", "BLE", "BT+BLE", "BT+WiFi", "WiFi", "RC/Drone"};
  const char* modeSpecs[] = {"79 CH (1MHz)", "40 CH (2MHz)", "79+40 CH", "79+14 CH", "14 CH (5MHz)", "126 CH (1MHz)"};
  const int modeCounts[] = {CH_BT_COUNT, CH_BLE_COUNT, CH_BT_COUNT, CH_BT_COUNT, CH_WIFI_COUNT, CH_RC_COUNT};
  const int totalModes = 6;
  int currentMode = 0;
  bool isAttacking = false;
  uint8_t noise[32]; // Randomized per hop

  auto drawGauge = [&]() {
    tft.fillScreen(CLR_BG);
    tft.drawRect(0, 0, SCR_W, SCR_H, isAttacking ? CLR_SECONDARY : CLR_PRIMARY);

    drawGradientCard(2, 2, SCR_W-4, SCALE_Y(14), isAttacking ? CLR_SECONDARY : CLR_PRIMARY,
                     darkenColor(isAttacking ? CLR_SECONDARY : CLR_PRIMARY, 50), 0);
    tft.setTextColor(CLR_TEXT_HI);
    tft.drawString("BLUEJAMMER", SCALE_X(4), SCALE_Y(3), 1);
    tft.setTextColor(isAttacking ? CLR_SECONDARY : CLR_SUCCESS);
    tft.drawString(isAttacking ? "ACTIVE" : "READY", SCR_W - SCALE_X(38), SCALE_Y(3), 1);

    // Mode display
    tft.setTextColor(CLR_WARNING);
    tft.drawCentreString(modeNames[currentMode], SCR_CX, SCALE_Y(22), 2);

    tft.setTextColor(CLR_PRIMARY);
    tft.drawCentreString(modeSpecs[currentMode], SCR_CX, SCALE_Y(42), 1);

    // Channel bar showing mode's frequency range
    tft.drawRect(SCALE_X(8), SCALE_Y(56), SCR_W - SCALE_X(16), SCALE_Y(8), CLR_BORDER);
    int fillW = ((SCR_W - SCALE_X(18)) * modeCounts[currentMode]) / CH_RC_COUNT;
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
    tft.drawString("</>::Mode HOLD:Back", SCALE_X(2), SCR_H - SCALE_Y(10), 1);
  };

  drawGauge();
  unsigned long holdStart = 0; bool holding = false;
  unsigned long lastDraw = millis();

  // Wait for Select button release to prevent bleed-through from menu selection
  while (digitalRead(BTN_SELECT) == BTN_ACTIVE_LEVEL) {
    delay(10);
  }
  virtualSelectPressed = false;

  while (true) {
    extern void handleNetwork(); handleNetwork();

    // Mode switching (only when idle)
    if (!isAttacking) {
      if ((digitalRead(BTN_LEFT) == BTN_ACTIVE_LEVEL || (virtualLeftPressed ? (virtualLeftPressed=false, true) : false))) {
        currentMode = (currentMode == 0) ? totalModes - 1 : currentMode - 1;
        drawGauge(); delay(180);
      }
      if ((digitalRead(BTN_RIGHT) == BTN_ACTIVE_LEVEL || (virtualRightPressed ? (virtualRightPressed=false, true) : false))) {
        currentMode = (currentMode == totalModes - 1) ? 0 : currentMode + 1;
        drawGauge(); delay(180);
      }
    }

    if ((digitalRead(BTN_SELECT) == BTN_ACTIVE_LEVEL || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) {
      if (!holding) { holdStart = millis(); holding = true; }
      if (millis() - holdStart > 800) {
        if (isAttacking) {
          if (j1Ok) jam1.stopConstCarrier();
          if (j2Ok) { jam2.stopConstCarrier(); jam2.flush_tx(); }
        }
        if (j1Ok) jam1.powerDown();
        if (j2Ok) jam2.powerDown();
        return;
      }
    } else {
      if (holding && millis() - holdStart < 800) {
        isAttacking = !isAttacking;
        if (!isAttacking) {
          if (j1Ok) jam1.stopConstCarrier();
          if (j2Ok) { jam2.stopConstCarrier(); jam2.flush_tx(); }
        }
        drawGauge();
      }
      holding = false;
    }

    if (isAttacking) {
      static unsigned long lastHop = 0;
      static int hopIdx1 = 0;
      int chCount = modeCounts[currentMode];

      // High-speed channel hopping - 1ms per hop for maximum disruption
      // Matches BlueJammer's tight-loop sweep approach
      if (millis() - lastHop >= 1) {
        // Randomize noise payload every hop (not static 0x55AA)
        for (int i = 0; i < 32; i++) noise[i] = (uint8_t)random(0, 256);

        uint8_t ch1 = 0;
        uint8_t ch2 = 0;
        switch (currentMode) {
          case 0: // Bluetooth Classic (79 CH, 2.402-2.480 GHz)
            ch1 = ch_bt_classic[hopIdx1 % CH_BT_COUNT];
            ch2 = ch_bt_classic[(hopIdx1 + CH_BT_COUNT / 2) % CH_BT_COUNT];
            break;
          case 1: // BLE (40 CH, 2.400-2.4835 GHz)
            ch1 = ch_ble_full[hopIdx1 % CH_BLE_COUNT];
            ch2 = ch_ble_full[(hopIdx1 + CH_BLE_COUNT / 2) % CH_BLE_COUNT];
            break;
          case 2: // Bluetooth + BLE combo (2.402-2.480 GHz & 2.400-2.4835 GHz)
            ch1 = ch_bt_classic[hopIdx1 % CH_BT_COUNT];
            ch2 = ch_ble_full[hopIdx1 % CH_BLE_COUNT];
            break;
          case 3: // Bluetooth + WiFi combo (2.402-2.480 GHz & 2.400-2.4835 GHz)
            ch1 = ch_bt_classic[hopIdx1 % CH_BT_COUNT];
            ch2 = ch_wifi_spec[hopIdx1 % CH_WIFI_COUNT];
            break;
          case 4: // WiFi (14 CH, 2.400-2.4835 GHz)
            ch1 = ch_wifi_spec[hopIdx1 % CH_WIFI_COUNT];
            ch2 = ch_wifi_spec[(hopIdx1 + CH_WIFI_COUNT / 2) % CH_WIFI_COUNT];
            break;
          case 5: // RC / Drone (126 CH, 2.400-2.525 GHz)
            ch1 = (uint8_t)(hopIdx1 % CH_RC_COUNT);
            ch2 = (uint8_t)((hopIdx1 + CH_RC_COUNT / 2) % CH_RC_COUNT);
            break;
        }

        // Radio 1: transmit carrier wave
        if (j1Ok) {
          jam1.stopConstCarrier();
          jam1.setChannel(ch1);
          jam1.startConstCarrier(RF24_PA_MAX, ch1);
        }

        // Radio 2: packet flooding
        if (j2Ok) {
          jam2.setChannel(ch2);
          for (int i = 0; i < 3; i++) jam2.startWrite(noise, 32, true);
        }

        hopIdx1 = (hopIdx1 + 1) % chCount;
        lastHop = millis();
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

    delay(isAttacking ? 1 : 50); // 1ms delay allows FreeRTOS scheduler/TWDT to yield safely
  }
}

// ═══════════════════════════════════════════════════════════
// TOOL: RfClown Jammer (Multi-Protocol)
// Port of the RfClown 2.4GHz jammer logic for 2 radios.
// ═══════════════════════════════════════════════════════════

static void dhRunRfClownJammer() {
  RF24 jam1(NRF1_CE_PIN, NRF1_CSN_PIN);
  RF24 jam2(NRF2_CE_PIN, NRF2_CSN_PIN);

  pinMode(TFT_CS, OUTPUT); digitalWrite(TFT_CS, HIGH);
  pinMode(NRF1_CSN_PIN, OUTPUT); digitalWrite(NRF1_CSN_PIN, HIGH);
  pinMode(NRF2_CSN_PIN, OUTPUT); digitalWrite(NRF2_CSN_PIN, HIGH);
  pinMode(NRF1_CE_PIN, OUTPUT); digitalWrite(NRF1_CE_PIN, LOW);
  pinMode(NRF2_CE_PIN, OUTPUT); digitalWrite(NRF2_CE_PIN, LOW);

  // SPI initialized globally

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
    char errMsg[64];
    sprintf(errMsg, "nRF24 Init Failed\nCE:%d CSN:%d", NRF1_CE_PIN, NRF1_CSN_PIN);
    dhShowError(errMsg);
    return;
  }

  // RfClown modes and channel maps (mapped 1:1)
  const char* modeNames[] = {"WiFi", "Video TX", "RC", "BLE", "Bluetooth", "USB Wireless", "Zigbee", "NRF24"};
  int currentMode = 0;
  bool isAttacking = false;

  // Wi-Fi 14 CH: Center frequencies mapped to nRF24 channel registers (2400 + reg = freq)
  // CH 1 (2412MHz)->12, CH 6 (2437MHz)->37, CH 11 (2462MHz)->62, CH 13 (2472MHz)->72, CH 14 (2484MHz)->84
  const byte ch_wifi[] = {12, 17, 22, 27, 32, 37, 42, 47, 52, 57, 62, 67, 72, 84};
  const byte ch_video[] = {70, 75, 80};
  const byte ch_rc[] = {1, 3, 5, 7};
  // BLE 40 CH: 3 advertising channels (37, 38, 39 -> 2, 26, 80) + 37 data channels (0-36 -> 4,6..78)
  const byte ch_ble[] = {
     2,  4,  6,  8, 10, 12, 14, 16, 18, 20,
    22, 24, 26, 28, 30, 32, 34, 36, 38, 40,
    42, 44, 46, 48, 50, 52, 54, 56, 58, 60,
    62, 64, 66, 68, 70, 72, 74, 76, 78, 80
  };
  // Bluetooth Classic 79 CH: 2402 to 2480 MHz (nRF24 reg = freq - 2400 -> 2..80)
  const byte ch_bt[] = {
     2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60,
    61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80
  };
  const byte ch_usb[] = {40, 50, 60};
  // Zigbee IEEE 802.15.4 channels 11-26 (2405 to 2480 MHz, 5 MHz spacing -> nRF24 reg 5, 10, 15..80)
  const byte ch_zigbee[] = {5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80};
  const byte ch_nrf24[] = {76, 78, 79};

  auto getChannelData = [&](int mode, const byte*& arr, int& size) {
    switch (mode) {
      case 0: arr = ch_wifi; size = sizeof(ch_wifi); break;
      case 1: arr = ch_video; size = sizeof(ch_video); break;
      case 2: arr = ch_rc; size = sizeof(ch_rc); break;
      case 3: arr = ch_ble; size = sizeof(ch_ble); break;
      case 4: arr = ch_bt; size = sizeof(ch_bt); break;
      case 5: arr = ch_usb; size = sizeof(ch_usb); break;
      case 6: arr = ch_zigbee; size = sizeof(ch_zigbee); break;
      case 7: arr = ch_nrf24; size = sizeof(ch_nrf24); break;
    }
  };

  auto drawUI = [&]() {
    tft.fillScreen(CLR_BG);
    drawGradientCard(2, 2, SCR_W-4, SCALE_Y(14), isAttacking ? CLR_SECONDARY : CLR_PRIMARY,
                     darkenColor(isAttacking ? CLR_SECONDARY : CLR_PRIMARY, 50), 0);
    tft.setTextColor(CLR_TEXT_HI);
    tft.drawString("RF-CLOWN", SCALE_X(4), SCALE_Y(3), 1);
    tft.setTextColor(isAttacking ? CLR_SECONDARY : CLR_SUCCESS);
    tft.drawString(isAttacking ? "ACTIVE" : "READY", SCR_W - SCALE_X(38), SCALE_Y(3), 1);

    tft.setTextColor(CLR_WARNING);
    tft.drawCentreString(modeNames[currentMode], SCR_CX, SCALE_Y(30), 2);

    const byte* arr; int size;
    getChannelData(currentMode, arr, size);
    char stats[32]; sprintf(stats, "%d Channels", size);
    tft.setTextColor(CLR_TEXT_MED);
    tft.drawCentreString(stats, SCR_CX, SCALE_Y(50), 1);

    char radios[20]; sprintf(radios, "Radios: %d/2", activeCount);
    tft.setTextColor(activeCount > 0 ? CLR_SUCCESS : CLR_SECONDARY);
    tft.drawString(radios, SCALE_X(4), SCALE_Y(70), 1);

    if (!isAttacking) {
      tft.drawRect(SCALE_X(20), SCALE_Y(80), SCR_W - SCALE_X(40), SCALE_Y(14), CLR_BORDER);
      tft.setTextColor(CLR_SUCCESS);
      tft.drawCentreString("HOLD SEL: START", SCR_CX, SCALE_Y(83), 1);
    }
    tft.setTextColor(CLR_TEXT_LO);
    tft.drawString("</>:Mode", SCALE_X(4), SCR_H - SCALE_Y(10), 1);
  };

  drawUI();
  unsigned long holdStart = 0; bool holding = false;
  unsigned long lastDraw = 0;
  const uint8_t noise[32] = {
    0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,
    0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,
    0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,
    0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA
  };

  // Wait for Select button release to prevent bleed-through from menu selection
  while (digitalRead(BTN_SELECT) == BTN_ACTIVE_LEVEL) {
    delay(10);
  }
  virtualSelectPressed = false;

  while (true) {
    extern void handleNetwork(); handleNetwork();
    
    if (!isAttacking) {
      if ((digitalRead(BTN_LEFT) == BTN_ACTIVE_LEVEL || (virtualLeftPressed ? (virtualLeftPressed=false, true) : false))) {
        currentMode = (currentMode == 0) ? 7 : currentMode - 1;
        drawUI(); delay(180);
      }
      if ((digitalRead(BTN_RIGHT) == BTN_ACTIVE_LEVEL || (virtualRightPressed ? (virtualRightPressed=false, true) : false))) {
        currentMode = (currentMode == 7) ? 0 : currentMode + 1;
        drawUI(); delay(180);
      }
    }

    if ((digitalRead(BTN_SELECT) == BTN_ACTIVE_LEVEL || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) {
      if (!holding) { holdStart = millis(); holding = true; }
      if (millis() - holdStart > 800) {
        isAttacking = !isAttacking;
        if (!isAttacking) {
          if (j1Ok) jam1.stopConstCarrier();
          if (j2Ok) { jam2.stopConstCarrier(); jam2.flush_tx(); }
        }
        drawUI();
        holding = false;
        delay(500); // debounce
      }
    } else {
      if (holding && millis() - holdStart < 800) {
        if (!isAttacking) {
          if (j1Ok) jam1.powerDown();
          if (j2Ok) jam2.powerDown();
          return;
        }
      }
      holding = false;
    }

    if (isAttacking) {
      const byte* arr; int size;
      getChannelData(currentMode, arr, size);
      
      // Hopping logic: rapidly cycle through all channels in the map.
      // Radio 1 takes lower half of the channels, Radio 2 takes upper half,
      // creating a wider spread of interference.
      static unsigned long lastHop = 0;
      static int hopIdx1 = 0;
      
      if (millis() - lastHop > 20) { // Hop every 20ms
        hopIdx1 = (hopIdx1 + 1) % size;
        int hopIdx2 = (hopIdx1 + size / 2) % size;
        
        byte ch1 = arr[hopIdx1];
        byte ch2 = arr[hopIdx2];
        
        if (j1Ok) {
          jam1.stopConstCarrier();
          jam1.setChannel(ch1);
          jam1.startConstCarrier(RF24_PA_MAX, ch1);
        }
        if (j2Ok) {
          jam2.setChannel(ch2);
          for (int i = 0; i < 5; i++) jam2.startWrite(noise, 32, true);
        }
        lastHop = millis();
      }

      if (millis() - lastDraw > 150) {
        tft.fillRect(SCALE_X(4), SCALE_Y(80), SCR_W - SCALE_X(8), SCALE_Y(24), CLR_BG);
        for (int i = 0; i < 12; i++) {
          int h = 4 + random(0, SCALE_Y(20));
          uint16_t c = h > SCALE_Y(14) ? CLR_SECONDARY : CLR_WARNING;
          tft.fillRect(SCALE_X(6) + i * SCALE_X(12), SCALE_Y(104) - h, SCALE_X(8), h, c);
        }
        lastDraw = millis();
      }
    }

    delay(isAttacking ? 5 : 50);
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
  pinMode(NRF2_CSN_PIN, OUTPUT); digitalWrite(NRF2_CSN_PIN, HIGH);
  pinMode(NRF1_CE_PIN, OUTPUT); digitalWrite(NRF1_CE_PIN, LOW);
  pinMode(NRF2_CE_PIN, OUTPUT); digitalWrite(NRF2_CE_PIN, LOW);

  // SPI initialized globally

  bool ok = radio.begin();
  if (!ok) {
    char errMsg[64];
    sprintf(errMsg, "nRF24 Init Failed\nCE:%d CSN:%d", NRF1_CE_PIN, NRF1_CSN_PIN);
    dhShowError(errMsg);
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

      // WiFi channel markers (14 channels mapped to spec center frequencies)
      for (int wch = 0; wch < CH_WIFI_COUNT; wch++) {
        int nrf = ch_wifi_spec[wch];
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
    if ((digitalRead(BTN_LEFT) == BTN_ACTIVE_LEVEL || (virtualLeftPressed ? (virtualLeftPressed=false, true) : false)) || (digitalRead(BTN_RIGHT) == BTN_ACTIVE_LEVEL || (virtualRightPressed ? (virtualRightPressed=false, true) : false))) {
      showWaterfall = !showWaterfall;
      delay(200);
    }
    if ((digitalRead(BTN_SELECT) == BTN_ACTIVE_LEVEL || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) {
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
