// ═══════════════════════════════════════════════════════════
// COMPANION OS v7.0 — DR. HACK: IR TOOLS
// 12 IR tools using M5Stack IR Unit (TX=GPIO26, RX=GPIO34)
// Adapted from ESP32-TOOLS-PRO for 160×128 ST7735R
// ═══════════════════════════════════════════════════════════
#ifndef DH_IR_TOOLS_H
#define DH_IR_TOOLS_H

#ifdef ESP32

#include "globals.h"
#include "ui_components.h"
#include <SPIFFS.h>

// ═══════════════════════════════════════════════════════════
// IR HARDWARE ABSTRACTION — Raw GPIO timing
// No external library needed; uses direct edge detection.
// ═══════════════════════════════════════════════════════════

#define DH_IR_MAX_EDGES 512
#define DH_IR_CARRIER_HZ 38000
#define DH_IR_TIMEOUT_US 65000

static volatile uint32_t dhIrEdges[DH_IR_MAX_EDGES];
static volatile int dhIrEdgeCount = 0;
static volatile bool dhIrCapturing = false;
static volatile unsigned long dhIrLastEdge = 0;

static void IRAM_ATTR dhIrISR() {
  if (!dhIrCapturing) return;
  unsigned long now = micros();
  if (dhIrEdgeCount < DH_IR_MAX_EDGES) {
    dhIrEdges[dhIrEdgeCount++] = (uint32_t)(now - dhIrLastEdge);
  }
  dhIrLastEdge = now;
}

static void dhIrStartCapture() {
  dhIrEdgeCount = 0;
  dhIrLastEdge = micros();
  dhIrCapturing = true;
  attachInterrupt(digitalPinToInterrupt(IR_RX_PIN), dhIrISR, CHANGE);
}

static void dhIrStopCapture() {
  dhIrCapturing = false;
  detachInterrupt(digitalPinToInterrupt(IR_RX_PIN));
}

static void dhIrTransmit(uint32_t* timings, int count) {
  // Send IR signal at 38kHz carrier
  uint16_t halfPeriod = 500000 / DH_IR_CARRIER_HZ;
  for (int i = 0; i < count; i++) {
    unsigned long endTime = micros() + timings[i];
    if (i % 2 == 0) {
      // Mark: modulated carrier
      while (micros() < endTime) {
        digitalWrite(IR_TX_PIN, HIGH);
        delayMicroseconds(halfPeriod);
        digitalWrite(IR_TX_PIN, LOW);
        delayMicroseconds(halfPeriod);
      }
    } else {
      // Space: no signal
      while (micros() < endTime) {}
    }
  }
  digitalWrite(IR_TX_PIN, LOW);
}

// ═══════════════════════════════════════════════════════════
// IR Protocol Detection
// ═══════════════════════════════════════════════════════════

enum DH_IrProto { IR_UNKNOWN=0, IR_NEC, IR_SAMSUNG, IR_LG, IR_SONY, IR_RC5, IR_RC6 };
static const char* DH_IR_PROTO_NAMES[] = { "RAW", "NEC", "Samsung", "LG", "Sony", "RC5", "RC6" };

static DH_IrProto dhIrDetectProtocol() {
  if (dhIrEdgeCount < 4) return IR_UNKNOWN;
  uint32_t leader = dhIrEdges[0];
  uint32_t gap = dhIrEdgeCount > 1 ? dhIrEdges[1] : 0;

  // NEC: 9000us mark + 4500us space
  if (leader > 8000 && leader < 10000 && gap > 3500 && gap < 5500) return IR_NEC;
  // Samsung: 4500us mark + 4500us space
  if (leader > 3800 && leader < 5200 && gap > 3800 && gap < 5200) return IR_SAMSUNG;
  // LG: 4500us mark + 4200us space
  if (leader > 3800 && leader < 5200 && gap > 3400 && gap < 5000) return IR_LG;
  // Sony: 2400us mark + 600us space
  if (leader > 2000 && leader < 2800 && gap > 400 && gap < 800) return IR_SONY;
  // RC5/RC6: shorter timings
  if (leader > 700 && leader < 1100) return IR_RC5;
  if (leader > 2500 && leader < 3500 && gap > 800 && gap < 1200) return IR_RC6;

  return IR_UNKNOWN;
}

// ═══════════════════════════════════════════════════════════
// TOOL 1: IR Raw Capture
// ═══════════════════════════════════════════════════════════

static void dhRunIrCapture() {
  pinMode(IR_RX_PIN, INPUT);
  tft.fillScreen(CLR_BG);
  drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_SECONDARY, darkenColor(CLR_SECONDARY, 40), 0);
  tft.setTextColor(CLR_TEXT_HI);
  tft.drawString("IR CAPTURE", SCALE_X(4), SCALE_Y(2), 1);
  tft.setTextColor(CLR_TEXT_MED);
  tft.drawString("Point remote at RX", SCALE_X(4), SCALE_Y(22), 1);
  tft.drawString("Press any button", SCALE_X(4), SCALE_Y(34), 1);
  tft.setTextColor(CLR_PRIMARY);
  tft.drawString("Waiting...", SCALE_X(4), SCALE_Y(54), 1);
  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("SEL: back", SCALE_X(4), SCR_H - SCALE_Y(10), 1);

  dhIrStartCapture();
  unsigned long timeout = millis();
  bool captured = false;

  while (millis() - timeout < 30000) {
    if (dhIrEdgeCount > 10) {
      delay(200);
      if (dhIrEdgeCount > 10) { captured = true; break; }
    }
    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) { delay(200); break; }
    delay(10);
  }
  dhIrStopCapture();

  if (!captured) return;

  // Show results
  DH_IrProto proto = dhIrDetectProtocol();
  tft.fillScreen(CLR_BG);
  drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_SUCCESS, darkenColor(CLR_SUCCESS, 40), 0);
  tft.setTextColor(CLR_TEXT_HI);
  tft.drawString("CAPTURED!", SCALE_X(4), SCALE_Y(2), 1);

  int y = SCALE_Y(22);
  char buf[32];
  sprintf(buf, "Edges: %d", dhIrEdgeCount);
  tft.setTextColor(CLR_TEXT_MED); tft.drawString(buf, SCALE_X(4), y, 1); y += SCALE_Y(12);

  sprintf(buf, "Protocol: %s", DH_IR_PROTO_NAMES[proto]);
  tft.setTextColor(CLR_PRIMARY); tft.drawString(buf, SCALE_X(4), y, 1); y += SCALE_Y(12);

  uint32_t totalUs = 0;
  for (int i = 0; i < dhIrEdgeCount; i++) totalUs += dhIrEdges[i];
  sprintf(buf, "Duration: %lums", totalUs / 1000);
  tft.setTextColor(CLR_TEXT_MED); tft.drawString(buf, SCALE_X(4), y, 1); y += SCALE_Y(14);

  // Draw signal waveform
  int waveY = y, waveH = SCALE_Y(20);
  tft.drawRect(SCALE_X(2), waveY, SCR_W - SCALE_X(4), waveH, CLR_BORDER);
  int maxEdges = min((int)dhIrEdgeCount, (int)(SCR_W - SCALE_X(8)));
  int waveW = SCR_W - SCALE_X(8);
  for (int i = 0; i < maxEdges; i++) {
    int x = SCALE_X(4) + (i * waveW) / maxEdges;
    int h = (i % 2 == 0) ? waveH - 4 : 2;
    tft.drawFastVLine(x, waveY + waveH - h - 2, h, CLR_PRIMARY);
  }

  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("SEL: back", SCALE_X(4), SCR_H - SCALE_Y(10), 1);
  dhWaitSelectPress();
  delay(200);
}

// ═══════════════════════════════════════════════════════════
// TOOL 2: IR Replay — Replays last captured signal
// ═══════════════════════════════════════════════════════════

static void dhRunIrReplay() {
  pinMode(IR_TX_PIN, OUTPUT);
  digitalWrite(IR_TX_PIN, LOW);

  if (dhIrEdgeCount < 4) {
    tft.fillScreen(CLR_BG);
    tft.setTextColor(CLR_WARNING);
    tft.drawCentreString("No capture!", SCR_CX, SCALE_Y(40), 1);
    tft.drawCentreString("Use IR Capture first", SCR_CX, SCALE_Y(56), 1);
    tft.setTextColor(CLR_TEXT_LO);
    tft.drawString("SEL: back", SCALE_X(4), SCR_H - SCALE_Y(10), 1);
    dhWaitSelectPress();
    delay(200); return;
  }

  tft.fillScreen(CLR_BG);
  drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_SUCCESS, darkenColor(CLR_SUCCESS, 40), 0);
  tft.setTextColor(CLR_TEXT_HI);
  tft.drawString("IR REPLAY", SCALE_X(4), SCALE_Y(2), 1);

  char buf[24]; sprintf(buf, "Edges: %d", dhIrEdgeCount);
  tft.setTextColor(CLR_TEXT_MED); tft.drawString(buf, SCALE_X(4), SCALE_Y(24), 1);

  DH_IrProto proto = dhIrDetectProtocol();
  sprintf(buf, "Proto: %s", DH_IR_PROTO_NAMES[proto]);
  tft.setTextColor(CLR_PRIMARY); tft.drawString(buf, SCALE_X(4), SCALE_Y(38), 1);

  tft.setTextColor(CLR_TEXT_MED);
  tft.drawString("SEL: Transmit", SCALE_X(4), SCALE_Y(60), 1);
  tft.drawString("HOLD: exit", SCALE_X(4), SCALE_Y(72), 1);

  int txCount = 0;
  unsigned long holdStart = 0; bool holding = false;

  while (true) {
    extern void handleNetwork(); handleNetwork();
    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) {
      if (!holding) { holdStart = millis(); holding = true; }
      if (millis() - holdStart > 800) break;
    } else {
      if (holding && millis() - holdStart < 800) {
        // Transmit
        uint32_t timings[DH_IR_MAX_EDGES];
        memcpy(timings, (void*)dhIrEdges, dhIrEdgeCount * sizeof(uint32_t));

        tft.fillRect(SCALE_X(4), SCALE_Y(86), SCR_W - SCALE_X(8), SCALE_Y(14), CLR_BG);
        tft.setTextColor(CLR_SECONDARY);
        tft.drawString("TRANSMITTING...", SCALE_X(4), SCALE_Y(86), 1);

        dhIrTransmit(timings, dhIrEdgeCount);
        txCount++;

        tft.fillRect(SCALE_X(4), SCALE_Y(86), SCR_W - SCALE_X(8), SCALE_Y(14), CLR_BG);
        sprintf(buf, "Sent! (x%d)", txCount);
        tft.setTextColor(CLR_SUCCESS);
        tft.drawString(buf, SCALE_X(4), SCALE_Y(86), 1);
      }
      holding = false;
    }
    delay(10);
  }
}

// ═══════════════════════════════════════════════════════════
// TOOL 3: IR TX Test — Quick verification
// ═══════════════════════════════════════════════════════════

static void dhRunIrTxTest() {
  pinMode(IR_TX_PIN, OUTPUT);
  tft.fillScreen(CLR_BG);
  drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_PRIMARY, darkenColor(CLR_PRIMARY, 40), 0);
  tft.setTextColor(CLR_TEXT_HI);
  tft.drawString("IR TX TEST", SCALE_X(4), SCALE_Y(2), 1);

  tft.setTextColor(CLR_TEXT_MED);
  tft.drawString("Sending 3 pulses...", SCALE_X(4), SCALE_Y(30), 1);

  for (int p = 0; p < 3; p++) {
    char buf[16]; sprintf(buf, "Pulse %d/3", p+1);
    tft.fillRect(SCALE_X(4), SCALE_Y(50), SCR_W - SCALE_X(8), SCALE_Y(14), CLR_BG);
    tft.setTextColor(CLR_SUCCESS); tft.drawString(buf, SCALE_X(4), SCALE_Y(50), 1);

    // NEC-style leader pulse
    uint32_t test[] = {9000, 4500, 560, 560, 560, 560, 560, 560, 560, 1690, 560};
    dhIrTransmit(test, 11);
    delay(500);
  }

  tft.setTextColor(CLR_SUCCESS);
  tft.drawString("Done! Check with", SCALE_X(4), SCALE_Y(72), 1);
  tft.drawString("phone camera.", SCALE_X(4), SCALE_Y(84), 1);

  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("SEL: back", SCALE_X(4), SCR_H - SCALE_Y(10), 1);
  dhWaitSelectPress();
  delay(200);
}

// ═══════════════════════════════════════════════════════════
// TOOL 4: IR Analyzer — Live state detection
// ═══════════════════════════════════════════════════════════

static void dhRunIrAnalyzer() {
  pinMode(IR_RX_PIN, INPUT);
  tft.fillScreen(CLR_BG);
  drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_PRIMARY, darkenColor(CLR_PRIMARY, 40), 0);
  tft.setTextColor(CLR_TEXT_HI);
  tft.drawString("IR ANALYZER", SCALE_X(4), SCALE_Y(2), 1);

  int events = 0;
  unsigned long lastEdge = 0, lastDraw = 0;
  unsigned long holdStart = 0; bool holding = false;
  bool lastState = HIGH;

  while (true) {
    extern void handleNetwork(); handleNetwork();
    bool state = digitalRead(IR_RX_PIN);
    if (state != lastState) {
      lastEdge = micros();
      events++;
      lastState = state;
    }

    if (millis() - lastDraw > 200) {
      unsigned long idle = micros() - lastEdge;
      const char* status;
      uint16_t col;

      if (idle > 100000) { status = "IDLE"; col = CLR_TEXT_LO; }
      else if (idle > 50000) { status = "GAP"; col = CLR_WARNING; }
      else if (idle > 10000) { status = "REPEAT"; col = CLR_PRIMARY; }
      else { status = "FRAME"; col = CLR_SUCCESS; }

      tft.fillRect(0, SCALE_Y(20), SCR_W, SCALE_Y(80), CLR_BG);
      tft.setTextColor(col);
      tft.drawCentreString(status, SCR_CX, SCALE_Y(30), 2);

      char buf[32];
      sprintf(buf, "Events: %d", events);
      tft.setTextColor(CLR_TEXT_MED); tft.drawString(buf, SCALE_X(4), SCALE_Y(56), 1);
      sprintf(buf, "RX pin: %s", state ? "HIGH" : "LOW");
      tft.setTextColor(CLR_TEXT_MED); tft.drawString(buf, SCALE_X(4), SCALE_Y(68), 1);
      sprintf(buf, "Idle: %luus", idle);
      tft.setTextColor(CLR_TEXT_LO); tft.drawString(buf, SCALE_X(4), SCALE_Y(80), 1);

      // Activity indicator
      int barW = state ? 0 : SCR_W - SCALE_X(8);
      tft.fillRect(SCALE_X(4), SCALE_Y(94), SCR_W - SCALE_X(8), SCALE_Y(4), CLR_SURFACE);
      if (barW > 0) tft.fillRect(SCALE_X(4), SCALE_Y(94), barW, SCALE_Y(4), CLR_SECONDARY);

      tft.setTextColor(CLR_TEXT_LO);
      tft.drawString("HOLD SEL: exit", SCALE_X(4), SCR_H - SCALE_Y(10), 1);
      lastDraw = millis();
    }

    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) {
      if (!holding) { holdStart = millis(); holding = true; }
      if (millis() - holdStart > 800) break;
    } else { holding = false; }
    delay(2);
  }
}

// ═══════════════════════════════════════════════════════════
// TOOL 5: IR Sniffer — Log IR events with protocol
// ═══════════════════════════════════════════════════════════

#define DH_IRLOG_MAX 20
struct DH_IrLogEntry {
  DH_IrProto proto;
  int edges;
  unsigned long timestamp;
};
static DH_IrLogEntry dhIrLog[DH_IRLOG_MAX];
static int dhIrLogCount = 0;

static void dhRunIrSniffer() {
  pinMode(IR_RX_PIN, INPUT);
  dhIrLogCount = 0;

  unsigned long holdStart = 0; bool holding = false;
  unsigned long lastDraw = 0;

  while (true) {
    extern void handleNetwork(); handleNetwork();
    dhIrStartCapture();
    unsigned long waitStart = millis();
    while (millis() - waitStart < 500) {
      if (dhIrEdgeCount > 10) { delay(200); break; }
      if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) break;
      delay(5);
    }
    dhIrStopCapture();

    if (dhIrEdgeCount > 10 && dhIrLogCount < DH_IRLOG_MAX) {
      DH_IrLogEntry& e = dhIrLog[dhIrLogCount++];
      e.proto = dhIrDetectProtocol();
      e.edges = dhIrEdgeCount;
      e.timestamp = millis() / 1000;
    }

    // Draw
    tft.fillScreen(CLR_BG);
    drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_PRIMARY, darkenColor(CLR_PRIMARY, 40), 0);
    tft.setTextColor(CLR_TEXT_HI);
    char hdr[24]; sprintf(hdr, "IR SNIFF [%d]", dhIrLogCount);
    tft.drawString(hdr, SCALE_X(4), SCALE_Y(2), 1);

    if (dhIrLogCount == 0) {
      tft.setTextColor(CLR_TEXT_MED);
      tft.drawCentreString("Waiting for IR...", SCR_CX, SCR_CY, 1);
    } else {
      int startIdx = max(0, dhIrLogCount - 5);
      int y = SCALE_Y(18);
      for (int i = startIdx; i < dhIrLogCount; i++) {
        char line[40]; sprintf(line, "#%d %s e:%d @%lus",
          i+1, DH_IR_PROTO_NAMES[dhIrLog[i].proto], dhIrLog[i].edges, dhIrLog[i].timestamp);
        tft.setTextColor(CLR_TEXT_MED);
        tft.drawString(line, SCALE_X(2), y, 1);
        y += SCALE_Y(16);
      }
    }

    tft.setTextColor(CLR_TEXT_LO);
    tft.drawString("HOLD SEL: exit", SCALE_X(4), SCR_H - SCALE_Y(10), 1);

    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) {
      if (!holding) { holdStart = millis(); holding = true; }
      if (millis() - holdStart > 800) break;
    } else { holding = false; }
  }
}

// ═══════════════════════════════════════════════════════════
// TOOL 6: IR Protocol Scanner — Classify signal
// ═══════════════════════════════════════════════════════════

static void dhRunIrProtocol() {
  pinMode(IR_RX_PIN, INPUT);
  tft.fillScreen(CLR_BG);
  drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_PRIMARY, darkenColor(CLR_PRIMARY, 40), 0);
  tft.setTextColor(CLR_TEXT_HI);
  tft.drawString("PROTOCOL SCAN", SCALE_X(4), SCALE_Y(2), 1);
  tft.setTextColor(CLR_TEXT_MED);
  tft.drawString("Press remote button", SCALE_X(4), SCALE_Y(30), 1);

  dhIrStartCapture();
  unsigned long timeout = millis();
  while (millis() - timeout < 15000) {
    if (dhIrEdgeCount > 10) { delay(200); break; }
    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) { dhIrStopCapture(); delay(200); return; }
    delay(10);
  }
  dhIrStopCapture();

  if (dhIrEdgeCount < 4) {
    tft.setTextColor(CLR_WARNING);
    tft.drawCentreString("No signal", SCR_CX, SCR_CY, 1);
    delay(2000); return;
  }

  DH_IrProto proto = dhIrDetectProtocol();
  tft.fillScreen(CLR_BG);
  drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_SUCCESS, darkenColor(CLR_SUCCESS, 40), 0);
  tft.setTextColor(CLR_TEXT_HI);
  tft.drawString("IDENTIFIED", SCALE_X(4), SCALE_Y(2), 1);

  int y = SCALE_Y(24);
  tft.setTextColor(CLR_PRIMARY);
  tft.drawCentreString(DH_IR_PROTO_NAMES[proto], SCR_CX, y, 2); y += SCALE_Y(24);

  char buf[32];
  sprintf(buf, "Edges: %d", dhIrEdgeCount);
  tft.setTextColor(CLR_TEXT_MED); tft.drawString(buf, SCALE_X(4), y, 1); y += SCALE_Y(12);
  sprintf(buf, "Leader: %luus", (unsigned long)dhIrEdges[0]);
  tft.setTextColor(CLR_TEXT_MED); tft.drawString(buf, SCALE_X(4), y, 1); y += SCALE_Y(12);
  if (dhIrEdgeCount > 1) {
    sprintf(buf, "Gap: %luus", (unsigned long)dhIrEdges[1]);
    tft.drawString(buf, SCALE_X(4), y, 1); y += SCALE_Y(12);
  }

  // Average bit time
  uint32_t sum = 0;
  for (int i = 2; i < min((int)dhIrEdgeCount, 20); i++) sum += dhIrEdges[i];
  int avg = (dhIrEdgeCount > 2) ? sum / min((int)dhIrEdgeCount - 2, 18) : 0;
  sprintf(buf, "AvgBit: %dus", avg);
  tft.drawString(buf, SCALE_X(4), y, 1);

  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("SEL: back", SCALE_X(4), SCR_H - SCALE_Y(10), 1);
  dhWaitSelectPress();
  delay(200);
}

// ═══════════════════════════════════════════════════════════
// TOOL 7: Night IR Detector
// ═══════════════════════════════════════════════════════════

static void dhRunIrNight() {
  pinMode(IR_RX_PIN, INPUT);
  unsigned long holdStart = 0; bool holding = false;
  int pulseCount = 0;
  unsigned long lastPulse = 0;

  while (true) {
    extern void handleNetwork(); handleNetwork();
    tft.fillScreen(CLR_BG);
    drawGradientCard(0, 0, SCR_W, SCALE_Y(14), 0x780F, darkenColor(0x780F, 40), 0);
    tft.setTextColor(CLR_TEXT_HI);
    tft.drawString("NIGHT IR", SCALE_X(4), SCALE_Y(2), 1);

    // Sample IR RX for 200ms
    int activity = 0;
    unsigned long sample = millis();
    while (millis() - sample < 200) {
      if (digitalRead(IR_RX_PIN) == LOW) activity++;
      delayMicroseconds(100);
    }

    bool detected = activity > 50;
    if (detected) { pulseCount++; lastPulse = millis(); }

    tft.setTextColor(detected ? CLR_SECONDARY : CLR_SUCCESS);
    tft.drawCentreString(detected ? "IR DETECTED!" : "CLEAR", SCR_CX, SCALE_Y(30), 2);

    char buf[32];
    sprintf(buf, "Activity: %d", activity);
    tft.setTextColor(CLR_TEXT_MED); tft.drawString(buf, SCALE_X(4), SCALE_Y(56), 1);
    sprintf(buf, "Pulses: %d", pulseCount);
    tft.drawString(buf, SCALE_X(4), SCALE_Y(68), 1);

    if (detected) {
      int barW = min(activity, (int)(SCR_W - SCALE_X(8)));
      tft.fillRect(SCALE_X(4), SCALE_Y(84), barW, SCALE_Y(6), CLR_SECONDARY);
    }

    tft.setTextColor(CLR_TEXT_LO);
    tft.drawString("HOLD SEL: exit", SCALE_X(4), SCR_H - SCALE_Y(10), 1);

    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) {
      if (!holding) { holdStart = millis(); holding = true; }
      if (millis() - holdStart > 800) break;
    } else { holding = false; }
    delay(50);
  }
}

// ═══════════════════════════════════════════════════════════
// TOOL 8: IR Proximity Test
// ═══════════════════════════════════════════════════════════

static void dhRunIrProximity() {
  pinMode(IR_TX_PIN, OUTPUT);
  pinMode(IR_RX_PIN, INPUT);

  unsigned long holdStart = 0; bool holding = false;

  while (true) {
    extern void handleNetwork(); handleNetwork();
    // Send a quick pulse and measure bounce
    uint32_t pulse[] = {500, 500, 500};
    dhIrTransmit(pulse, 3);

    // Read response
    unsigned long start = micros();
    bool bounced = false;
    while (micros() - start < 5000) {
      if (digitalRead(IR_RX_PIN) == LOW) { bounced = true; break; }
    }
    unsigned long responseTime = micros() - start;

    tft.fillScreen(CLR_BG);
    drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_PRIMARY, darkenColor(CLR_PRIMARY, 40), 0);
    tft.setTextColor(CLR_TEXT_HI);
    tft.drawString("IR PROXIMITY", SCALE_X(4), SCALE_Y(2), 1);

    tft.setTextColor(bounced ? CLR_SUCCESS : CLR_TEXT_LO);
    tft.drawCentreString(bounced ? "DETECTED" : "NOTHING", SCR_CX, SCALE_Y(30), 2);

    char buf[32];
    sprintf(buf, "Resp: %luus", responseTime);
    tft.setTextColor(CLR_TEXT_MED); tft.drawString(buf, SCALE_X(4), SCALE_Y(56), 1);

    if (bounced) {
      int barW = map(responseTime, 0, 5000, SCR_W - SCALE_X(8), 0);
      barW = constrain(barW, 0, SCR_W - SCALE_X(8));
      tft.fillRect(SCALE_X(4), SCALE_Y(72), barW, SCALE_Y(8), CLR_SUCCESS);
    }

    tft.setTextColor(CLR_TEXT_LO);
    tft.drawString("HOLD SEL: exit", SCALE_X(4), SCR_H - SCALE_Y(10), 1);

    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) {
      if (!holding) { holdStart = millis(); holding = true; }
      if (millis() - holdStart > 800) break;
    } else { holding = false; }
    delay(200);
  }
}

// ═══════════════════════════════════════════════════════════
// TOOL 9: IR Virtual Remotes
// Simple remote with common power/vol/ch buttons
// ═══════════════════════════════════════════════════════════

static void dhRunIrRemotes() {
  pinMode(IR_TX_PIN, OUTPUT);

  // NEC protocol common codes
  struct DH_IrButton { const char* label; uint32_t code; };
  static const DH_IrButton buttons[] = {
    {"POWER",  0x00FFA25D},
    {"VOL+",   0x00FF629D},
    {"VOL-",   0x00FFA857},
    {"CH+",    0x00FF22DD},
    {"CH-",    0x00FFC23D},
    {"MUTE",   0x00FFE21D},
    {"OK",     0x00FF02FD},
    {"MENU",   0x00FF9867}
  };
  int btnCount = 8;
  int cursor = 0;

  auto drawRemote = [&]() {
    tft.fillScreen(CLR_BG);
    drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_PRIMARY, darkenColor(CLR_PRIMARY, 40), 0);
    tft.setTextColor(CLR_TEXT_HI);
    tft.drawString("IR REMOTE", SCALE_X(4), SCALE_Y(2), 1);

    for (int i = 0; i < btnCount; i++) {
      int col = i % 2, row = i / 2;
      int x = SCALE_X(4) + col * SCALE_X(76);
      int y = SCALE_Y(18) + row * SCALE_Y(22);
      bool sel = (i == cursor);

      if (sel) {
        tft.fillRect(x, y, SCALE_X(72), SCALE_Y(18), CLR_PRIMARY);
        tft.setTextColor(CLR_BG);
      } else {
        tft.drawRect(x, y, SCALE_X(72), SCALE_Y(18), CLR_BORDER);
        tft.setTextColor(CLR_TEXT_MED);
      }
      tft.drawCentreString(buttons[i].label, x + SCALE_X(36), y + SCALE_Y(4), 1);
    }

    tft.setTextColor(CLR_TEXT_LO);
    tft.drawString("</>:Move SEL:Send", SCALE_X(2), SCR_H - SCALE_Y(10), 1);
  };

  auto sendNEC = [&](uint32_t code) {
    uint32_t timings[68];
    int idx = 0;
    timings[idx++] = 9000; timings[idx++] = 4500;
    for (int bit = 31; bit >= 0; bit--) {
      timings[idx++] = 560;
      timings[idx++] = (code >> bit) & 1 ? 1690 : 560;
    }
    timings[idx++] = 560; timings[idx++] = 560;
    dhIrTransmit(timings, idx);
  };

  drawRemote();
  unsigned long holdStart = 0; bool holding = false;

  while (true) {
    extern void handleNetwork(); handleNetwork();
    if ((digitalRead(BTN_LEFT) == LOW || (virtualLeftPressed ? (virtualLeftPressed=false, true) : false))) { cursor = (cursor + btnCount - 1) % btnCount; drawRemote(); delay(180); }
    if ((digitalRead(BTN_RIGHT) == LOW || (virtualRightPressed ? (virtualRightPressed=false, true) : false))) { cursor = (cursor + 1) % btnCount; drawRemote(); delay(180); }
    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) {
      if (!holding) { holdStart = millis(); holding = true; }
      if (millis() - holdStart > 800) break;
    } else {
      if (holding && millis() - holdStart < 800) {
        sendNEC(buttons[cursor].code);
        tft.fillRect(SCALE_X(4), SCR_H - SCALE_Y(22), SCR_W - SCALE_X(8), SCALE_Y(10), CLR_BG);
        tft.setTextColor(CLR_SUCCESS);
        tft.drawString("Sent!", SCALE_X(4), SCR_H - SCALE_Y(22), 1);
        delay(300);
        drawRemote();
      }
      holding = false;
    }
    delay(10);
  }
}

// ═══════════════════════════════════════════════════════════
// TOOL 10: Saved IR Captures — SPIFFS storage
// ═══════════════════════════════════════════════════════════

#define DH_IR_SAVE_MAX 8

static void dhRunIrSaved() {
  if (!SPIFFS.begin(true)) {
    tft.fillScreen(CLR_BG);
    tft.setTextColor(CLR_SECONDARY);
    tft.drawCentreString("SPIFFS Error!", SCR_CX, SCR_CY, 1);
    delay(2000); return;
  }

  // List saved files
  struct DH_SavedEntry { String name; size_t size; };
  DH_SavedEntry entries[DH_IR_SAVE_MAX];
  int entryCount = 0;

  File root = SPIFFS.open("/ir");
  if (root && root.isDirectory()) {
    File f = root.openNextFile();
    while (f && entryCount < DH_IR_SAVE_MAX) {
      entries[entryCount].name = String(f.name());
      entries[entryCount].size = f.size();
      entryCount++;
      f = root.openNextFile();
    }
  }

  // Add "SAVE NEW" option
  int totalItems = entryCount + 1;
  int cursor = 0;

  auto drawSaved = [&]() {
    tft.fillScreen(CLR_BG);
    drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_PRIMARY, darkenColor(CLR_PRIMARY, 40), 0);
    tft.setTextColor(CLR_TEXT_HI);
    char hdr[20]; sprintf(hdr, "SAVED IR [%d]", entryCount);
    tft.drawString(hdr, SCALE_X(4), SCALE_Y(2), 1);

    int y = SCALE_Y(18);
    for (int i = 0; i < totalItems; i++) {
      bool sel = (i == cursor);
      if (sel) tft.fillRect(0, y, SCR_W, SCALE_Y(14), CLR_SURFACE_2);

      if (i == entryCount) {
        tft.setTextColor(sel ? CLR_SUCCESS : CLR_TEXT_MED);
        tft.drawString("+ SAVE CURRENT", SCALE_X(8), y + SCALE_Y(2), 1);
      } else {
        tft.setTextColor(sel ? CLR_TEXT_HI : CLR_TEXT_MED);
        drawTruncatedText(SCALE_X(4), y + SCALE_Y(2), entries[i].name.c_str(), SCALE_X(90),
                          sel ? CLR_TEXT_HI : CLR_TEXT_MED, 1);
        char sz[12]; sprintf(sz, "%dB", entries[i].size);
        tft.setTextColor(CLR_TEXT_LO); tft.drawString(sz, SCR_W - SCALE_X(28), y + SCALE_Y(2), 1);
      }
      y += SCALE_Y(16);
    }

    tft.setTextColor(CLR_TEXT_LO);
    tft.drawString("</>:Sel SEL:Load", SCALE_X(2), SCR_H - SCALE_Y(10), 1);
  };

  drawSaved();
  unsigned long holdStart = 0; bool holding = false;

  while (true) {
    extern void handleNetwork(); handleNetwork();
    if ((digitalRead(BTN_LEFT) == LOW || (virtualLeftPressed ? (virtualLeftPressed=false, true) : false))) { cursor = (cursor + totalItems - 1) % totalItems; drawSaved(); delay(180); }
    if ((digitalRead(BTN_RIGHT) == LOW || (virtualRightPressed ? (virtualRightPressed=false, true) : false))) { cursor = (cursor + 1) % totalItems; drawSaved(); delay(180); }
    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) {
      if (!holding) { holdStart = millis(); holding = true; }
      if (millis() - holdStart > 800) break;
    } else {
      if (holding && millis() - holdStart < 800) {
        if (cursor == entryCount) {
          // Save current capture
          if (dhIrEdgeCount < 4) {
            tft.setTextColor(CLR_WARNING);
            tft.drawCentreString("No capture!", SCR_CX, SCR_CY, 1);
            delay(1500);
          } else {
            char fname[32]; sprintf(fname, "/ir/cap_%lu.bin", millis());
            SPIFFS.mkdir("/ir");
            File f = SPIFFS.open(fname, FILE_WRITE);
            if (f) {
              f.write((uint8_t*)&dhIrEdgeCount, sizeof(int));
              f.write((uint8_t*)dhIrEdges, dhIrEdgeCount * sizeof(uint32_t));
              f.close();
              tft.setTextColor(CLR_SUCCESS);
              tft.drawCentreString("Saved!", SCR_CX, SCR_CY, 1);
              delay(1000);
            }
          }
        } else {
          // Load and replay
          File f = SPIFFS.open(entries[cursor].name, FILE_READ);
          if (f) {
            int count;
            f.read((uint8_t*)&count, sizeof(int));
            if (count > 0 && count <= DH_IR_MAX_EDGES) {
              uint32_t timings[DH_IR_MAX_EDGES];
              f.read((uint8_t*)timings, count * sizeof(uint32_t));
              f.close();
              pinMode(IR_TX_PIN, OUTPUT);
              dhIrTransmit(timings, count);
              tft.setTextColor(CLR_SUCCESS);
              tft.drawCentreString("Replayed!", SCR_CX, SCR_CY, 1);
              delay(1000);
            } else { f.close(); }
          }
        }
        drawSaved();
      }
      holding = false;
    }
    delay(10);
  }
}

// ═══════════════════════════════════════════════════════════
// TOOL 11: Hardware Diagnostics
// ═══════════════════════════════════════════════════════════

static void dhRunHwDiag() {
  tft.fillScreen(CLR_BG);
  drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_PRIMARY, darkenColor(CLR_PRIMARY, 40), 0);
  tft.setTextColor(CLR_TEXT_HI);
  tft.drawString("HW DIAG", SCALE_X(4), SCALE_Y(2), 1);

  int y = SCALE_Y(20);
  int lineH = SCALE_Y(12);

  // IR TX
  tft.setTextColor(CLR_TEXT_LO); tft.drawString("IR TX:", SCALE_X(4), y, 1);
  tft.setTextColor(CLR_SUCCESS);
  char buf[24]; sprintf(buf, "GPIO%d", IR_TX_PIN);
  tft.drawString(buf, SCALE_X(50), y, 1); y += lineH;

  // IR RX
  tft.setTextColor(CLR_TEXT_LO); tft.drawString("IR RX:", SCALE_X(4), y, 1);
  bool irRxState = digitalRead(IR_RX_PIN);
  tft.setTextColor(irRxState ? CLR_TEXT_MED : CLR_SUCCESS);
  sprintf(buf, "GPIO%d %s", IR_RX_PIN, irRxState ? "HIGH" : "LOW");
  tft.drawString(buf, SCALE_X(50), y, 1); y += lineH;

  // nRF24 #1
  tft.setTextColor(CLR_TEXT_LO); tft.drawString("nRF1:", SCALE_X(4), y, 1);
  RF24 t_jam1(NRF1_CE_PIN, NRF1_CSN_PIN);
  bool t_jam1Ok = t_jam1.begin();
  sprintf(buf, "CE%d CS%d %s", NRF1_CE_PIN, NRF1_CSN_PIN, t_jam1Ok ? "OK" : "FAIL");
  tft.setTextColor(t_jam1Ok ? CLR_SUCCESS : CLR_SECONDARY); tft.drawString(buf, SCALE_X(50), y, 1); y += lineH;

  // nRF24 #2
  tft.setTextColor(CLR_TEXT_LO); tft.drawString("nRF2:", SCALE_X(4), y, 1);
  RF24 t_jam2(NRF2_CE_PIN, NRF2_CSN_PIN);
  bool t_jam2Ok = t_jam2.begin();
  sprintf(buf, "CE%d CS%d %s", NRF2_CE_PIN, NRF2_CSN_PIN, t_jam2Ok ? "OK" : "FAIL");
  tft.setTextColor(t_jam2Ok ? CLR_SUCCESS : CLR_SECONDARY); tft.drawString(buf, SCALE_X(50), y, 1); y += lineH;

  // CC1101
  tft.setTextColor(CLR_TEXT_LO); tft.drawString("CC1101:", SCALE_X(4), y, 1);
  sprintf(buf, "CS%d G0:%d", CC1101_CSN_PIN, CC1101_GDO0_PIN);
  tft.setTextColor(CLR_WARNING); tft.drawString(buf, SCALE_X(50), y, 1); y += lineH;

  // GDO0 state
  bool gdo0 = digitalRead(CC1101_GDO0_PIN);
  tft.setTextColor(CLR_TEXT_LO); tft.drawString("GDO0:", SCALE_X(4), y, 1);
  tft.setTextColor(gdo0 ? CLR_SUCCESS : CLR_TEXT_MED);
  tft.drawString(gdo0 ? "HIGH" : "LOW", SCALE_X(50), y, 1); y += lineH;

  // Free heap
  sprintf(buf, "Heap: %dKB", ESP.getFreeHeap() / 1024);
  tft.setTextColor(CLR_TEXT_LO); tft.drawString(buf, SCALE_X(4), y, 1);

  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("SEL: back", SCALE_X(4), SCR_H - SCALE_Y(10), 1);
  dhWaitSelectPress();
  delay(200);
}

// ═══════════════════════════════════════════════════════════
// TOOL 12: Input Monitor — Live activity on IR/CC1101
// ═══════════════════════════════════════════════════════════

static void dhRunInputMonitor() {
  pinMode(IR_RX_PIN, INPUT);
  pinMode(CC1101_GDO0_PIN, INPUT);

  unsigned long holdStart = 0; bool holding = false;
  int irEvents = 0, ccEvents = 0;
  bool lastIR = HIGH, lastCC = HIGH;

  while (true) {
    extern void handleNetwork(); handleNetwork();
    bool ir = digitalRead(IR_RX_PIN);
    bool cc = digitalRead(CC1101_GDO0_PIN);
    if (ir != lastIR) { irEvents++; lastIR = ir; }
    if (cc != lastCC) { ccEvents++; lastCC = cc; }

    static unsigned long lastDraw = 0;
    if (millis() - lastDraw > 150) {
      tft.fillScreen(CLR_BG);
      drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_PRIMARY, darkenColor(CLR_PRIMARY, 40), 0);
      tft.setTextColor(CLR_TEXT_HI);
      tft.drawString("INPUT MON", SCALE_X(4), SCALE_Y(2), 1);

      int y = SCALE_Y(22);
      // IR section
      tft.setTextColor(CLR_TEXT_LO); tft.drawString("IR RX:", SCALE_X(4), y, 1);
      tft.setTextColor(ir ? CLR_TEXT_MED : CLR_SUCCESS);
      tft.drawString(ir ? "IDLE" : "ACTIVE", SCALE_X(60), y, 1); y += SCALE_Y(12);

      char buf[24]; sprintf(buf, "Events: %d", irEvents);
      tft.setTextColor(CLR_TEXT_MED); tft.drawString(buf, SCALE_X(8), y, 1); y += SCALE_Y(12);

      // IR activity bar
      tft.fillRect(SCALE_X(8), y, SCR_W - SCALE_X(16), SCALE_Y(6), CLR_SURFACE);
      if (!ir) tft.fillRect(SCALE_X(8), y, SCR_W - SCALE_X(16), SCALE_Y(6), CLR_SUCCESS);
      y += SCALE_Y(14);

      // CC1101 section
      tft.setTextColor(CLR_TEXT_LO); tft.drawString("CC GDO0:", SCALE_X(4), y, 1);
      tft.setTextColor(cc ? CLR_WARNING : CLR_TEXT_MED);
      tft.drawString(cc ? "HIGH" : "LOW", SCALE_X(60), y, 1); y += SCALE_Y(12);

      sprintf(buf, "Events: %d", ccEvents);
      tft.setTextColor(CLR_TEXT_MED); tft.drawString(buf, SCALE_X(8), y, 1); y += SCALE_Y(12);

      tft.fillRect(SCALE_X(8), y, SCR_W - SCALE_X(16), SCALE_Y(6), CLR_SURFACE);
      if (cc) tft.fillRect(SCALE_X(8), y, SCR_W - SCALE_X(16), SCALE_Y(6), CLR_WARNING);

      tft.setTextColor(CLR_TEXT_LO);
      tft.drawString("HOLD SEL: exit", SCALE_X(4), SCR_H - SCALE_Y(10), 1);
      lastDraw = millis();
    }

    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) {
      if (!holding) { holdStart = millis(); holding = true; }
      if (millis() - holdStart > 800) break;
    } else { holding = false; }
    delay(2);
  }
}

#endif // ESP32
#endif // DH_IR_TOOLS_H
