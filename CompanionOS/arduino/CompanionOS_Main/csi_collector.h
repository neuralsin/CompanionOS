// ═══════════════════════════════════════════════════════════
// COMPANION OS v7.0 — CSI COLLECTOR (ESP32 ONLY)
//
// Collects WiFi Channel State Information (CSI) from the
// ESP32's own WiFi connection and transmits ADR-018 binary
// frames to the Python bridge on UDP port 8890.
//
// This eliminates the need for a separate "CSI node" ESP32.
// The display ESP32 acts as BOTH the CSI sensor and the
// display unit. Python processes the CSI → presence/motion
// and sends RUVIEW:{json} back to the ESP.
//
// Architecture:
//   [WiFi AP] ---packets---> [ESP32 WiFi chip CSI callback]
//       --> Pack ADR-018 frame
//       --> UDP:8890 --> [Python ruview_processor.py]
//       --> RUVIEW:{json} --> [ESP32 display]
// ═══════════════════════════════════════════════════════════
#ifndef CSI_COLLECTOR_H
#define CSI_COLLECTOR_H

#ifdef ESP32

#include "esp_wifi.h"
#include <WiFiUdp.h>

// ═══════════════════════════════════════════════════════════
// ADR-018 PROTOCOL CONSTANTS
// ═══════════════════════════════════════════════════════════

#define CSI_ADR018_MAGIC     0xC5110001
#define CSI_ADR018_HDR_SIZE  20
#define CSI_TX_PORT          8890
#define CSI_MAX_SUBCARRIERS  128
#define CSI_SEND_INTERVAL_MS 50   // ~20 Hz send rate

// ═══════════════════════════════════════════════════════════
// DOUBLE BUFFER — ISR writes one, loop reads the other
// ═══════════════════════════════════════════════════════════

static volatile bool     _csi_data_ready = false;
static int8_t            _csi_iq_buf[CSI_MAX_SUBCARRIERS * 2];  // I/Q pairs
static volatile int      _csi_iq_len     = 0;    // bytes of I/Q data
static volatile int8_t   _csi_rssi       = 0;
static volatile uint8_t  _csi_channel    = 1;
static volatile uint8_t  _csi_noise      = 0;
static uint32_t          _csi_sequence   = 0;
static WiFiUDP           _csi_udp;
static bool              _csi_started    = false;
static unsigned long     _csi_last_send  = 0;

// ═══════════════════════════════════════════════════════════
// CSI RX CALLBACK — Called from WiFi task context
// Copies the latest CSI snapshot into our buffer.
// We do NOT send UDP here — that happens in the main loop.
// ═══════════════════════════════════════════════════════════

static void _csi_rx_callback(void* ctx, wifi_csi_info_t* info) {
    if (!info || !info->buf || info->len < 2) return;

    // Copy I/Q data (capped at our max)
    int copy_len = info->len;
    if (copy_len > (int)sizeof(_csi_iq_buf)) {
        copy_len = sizeof(_csi_iq_buf);
    }
    memcpy(_csi_iq_buf, info->buf, copy_len);
    _csi_iq_len  = copy_len;
    _csi_rssi    = (int8_t)info->rx_ctrl.rssi;
    _csi_channel = info->rx_ctrl.channel;
    _csi_noise   = info->rx_ctrl.noise_floor;
    _csi_data_ready = true;
}

// ═══════════════════════════════════════════════════════════
// SETUP — Register the CSI callback and enable collection
// Call this AFTER WiFi is connected (in setup() or lazy-init)
// ═══════════════════════════════════════════════════════════

void setupCSICollector() {
    if (_csi_started) return;

    wifi_csi_config_t csi_config;
    memset(&csi_config, 0, sizeof(csi_config));
    csi_config.lltf_en           = true;   // Legacy Long Training Field
    csi_config.htltf_en          = true;   // HT Long Training Field
    csi_config.stbc_htltf2_en    = true;   // STBC HT-LTF2
    csi_config.ltf_merge_en      = true;   // Merge adjacent LTF subcarriers
    csi_config.channel_filter_en = false;  // Don't filter (get all subcarriers)
    csi_config.manu_scale        = false;  // Auto-scale
    csi_config.shift             = false;  // No bit shift

    esp_err_t rc;

    rc = esp_wifi_set_csi_config(&csi_config);
    if (rc != ESP_OK) {
        Serial.printf("CSI config failed: 0x%x\n", rc);
        return;
    }

    rc = esp_wifi_set_csi_rx_cb(_csi_rx_callback, NULL);
    if (rc != ESP_OK) {
        Serial.printf("CSI callback reg failed: 0x%x\n", rc);
        return;
    }

    rc = esp_wifi_set_csi(true);
    if (rc != ESP_OK) {
        Serial.printf("CSI enable failed: 0x%x\n", rc);
        return;
    }

    _csi_started = true;
    Serial.println(F("📡 CSI Collector started — sending ADR-018 to PC:8890"));
}

// ═══════════════════════════════════════════════════════════
// LOOP TICK — Call from loop() to drain buffer and send UDP
// Throttled to ~20 Hz to match RuView's expected rate.
// ═══════════════════════════════════════════════════════════

void tickCSICollector() {
    if (!_csi_started) return;
    if (!_csi_data_ready) return;

    unsigned long now = millis();
    if (now - _csi_last_send < CSI_SEND_INTERVAL_MS) return;
    _csi_last_send = now;

    // Snapshot the volatile data
    int iq_len  = _csi_iq_len;
    int8_t rssi = _csi_rssi;
    uint8_t ch  = _csi_channel;
    uint8_t nf  = _csi_noise;
    _csi_data_ready = false;

    if (iq_len < 2) return;

    int n_sub = iq_len / 2;  // number of I/Q pairs
    if (n_sub > CSI_MAX_SUBCARRIERS) n_sub = CSI_MAX_SUBCARRIERS;

    // Build ADR-018 packet: 20-byte header + I/Q payload
    int pkt_len = CSI_ADR018_HDR_SIZE + n_sub * 2;
    uint8_t pkt[CSI_ADR018_HDR_SIZE + CSI_MAX_SUBCARRIERS * 2];

    // [0..3] Magic (little-endian)
    uint32_t magic = CSI_ADR018_MAGIC;
    memcpy(&pkt[0], &magic, 4);

    // [4] Node ID = 0 (self — this ESP32)
    pkt[4] = 0;

    // [5] Number of antennas = 1
    pkt[5] = 1;

    // [6..7] Number of subcarriers (LE u16)
    pkt[6] = (uint8_t)(n_sub & 0xFF);
    pkt[7] = (uint8_t)((n_sub >> 8) & 0xFF);

    // [8..11] Frequency MHz (LE u32) — approximate from channel
    uint32_t freq_mhz = 2412 + (ch - 1) * 5;
    memcpy(&pkt[8], &freq_mhz, 4);

    // [12..15] Sequence number (LE u32)
    memcpy(&pkt[12], &_csi_sequence, 4);
    _csi_sequence++;

    // [16] RSSI (i8)
    pkt[16] = (uint8_t)rssi;

    // [17] Noise floor (i8)
    pkt[17] = (uint8_t)nf;

    // [18..19] Reserved
    pkt[18] = 0;
    pkt[19] = 0;

    // [20..] I/Q data (already signed int8 pairs from ESP-IDF)
    memcpy(&pkt[20], _csi_iq_buf, n_sub * 2);

    // Send to PC's IP on port 8890
    extern String pcIPStr;
    _csi_udp.beginPacket(pcIPStr.c_str(), CSI_TX_PORT);
    _csi_udp.write(pkt, pkt_len);
    _csi_udp.endPacket();

    // Debug print every 500 frames (~25s at 20 Hz)
    if (_csi_sequence % 500 == 0) {
        Serial.printf("📡 CSI: seq=%u sub=%d rssi=%d ch=%d → PC:%d\n",
                      _csi_sequence, n_sub, rssi, ch, CSI_TX_PORT);
    }
}

#endif // ESP32
#endif // CSI_COLLECTOR_H
