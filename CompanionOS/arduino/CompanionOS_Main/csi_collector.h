#ifndef CSI_COLLECTOR_H
#define CSI_COLLECTOR_H
#ifdef ESP32
#include <esp_wifi.h>
#include <WiFiUdp.h>
#include <new>

constexpr uint32_t CSI_ADR018_MAGIC = 0xC5110001;
constexpr size_t CSI_ADR018_HDR_SIZE = 20;
constexpr size_t CSI_MAX_SUBCARRIERS = 128;
constexpr uint16_t CSI_TX_PORT = 8890;
constexpr uint32_t CSI_SEND_INTERVAL_MS = 50;

// Both the sample storage and UDP buffers exist only while RuView is visible.
struct CsiRuntime {
  WiFiUDP udp;
  int8_t iq[CSI_MAX_SUBCARRIERS * 2] = {};
  uint16_t len = 0;
  int8_t rssi = 0, noise = 0;
  uint8_t channel = 1;
  bool ready = false;
  uint32_t sequence = 0, lastSend = 0, lastProbe = 0;
};
static CsiRuntime* csiRuntime = nullptr;
static portMUX_TYPE csiMux = portMUX_INITIALIZER_UNLOCKED;
static uint32_t csiRetryAt = 0;

static void csiRxCallback(void*, wifi_csi_info_t* info) {
  if (!info || !info->buf || info->len < 2) return;
  portENTER_CRITICAL(&csiMux);
  if (csiRuntime) {
    auto& state = *csiRuntime;
    state.len = min(size_t(info->len), sizeof(state.iq)) & ~size_t(1);
    memcpy(state.iq, info->buf, state.len);
    state.rssi = info->rx_ctrl.rssi;
    state.channel = info->rx_ctrl.channel;
    state.noise = info->rx_ctrl.noise_floor;
    state.ready = true;
  }
  portEXIT_CRITICAL(&csiMux);
}

void pauseCSICollector() {
  if (!csiRuntime) return;
  esp_wifi_set_csi(false);
  esp_wifi_set_csi_rx_cb(nullptr, nullptr);
  portENTER_CRITICAL(&csiMux);
  CsiRuntime* old = csiRuntime;
  csiRuntime = nullptr;
  portEXIT_CRITICAL(&csiMux);
  old->udp.stop();
  delete old;
  csiRetryAt = 0;
}

void resumeCSICollector() {
  if (csiRuntime || !wifiConnected || companion_memory::networkBusy.load() ||
      (csiRetryAt && int32_t(millis() - csiRetryAt) < 0)) return;
  csiRetryAt = millis() + 3000;
  CsiRuntime* state = new (std::nothrow) CsiRuntime();
  if (!state) return;
  wifi_csi_config_t config = {};
  config.lltf_en = config.htltf_en = config.stbc_htltf2_en = config.ltf_merge_en = true;
  esp_err_t rc = esp_wifi_set_csi_config(&config);
  if (rc == ESP_OK) rc = esp_wifi_set_csi_rx_cb(csiRxCallback, nullptr);
  if (rc != ESP_OK) {
    delete state;
    Serial.printf("[ruview] CSI setup failed: %d\n", int(rc));
    return;
  }
  portENTER_CRITICAL(&csiMux);
  csiRuntime = state;
  portEXIT_CRITICAL(&csiMux);
  rc = esp_wifi_set_csi(true);
  if (rc != ESP_OK) {
    pauseCSICollector();
    csiRetryAt = millis() + 3000;
    Serial.printf("[ruview] CSI start failed: %d\n", int(rc));
  }
}

void tickCSICollector() {
  if (!wifiConnected) { pauseCSICollector(); return; }
  resumeCSICollector();
  if (!csiRuntime) return;
  auto& state = *csiRuntime;
  uint32_t now = millis();
  extern String pcIPStr;
  // CSI needs incoming Wi-Fi frames. A small echo exchange supplies real AP
  // traffic even when Spotify is idle; probes never count as CSI measurements.
  for (int i = 0; i < 4 && state.udp.parsePacket(); ++i) state.udp.flush();
  if (uint32_t(now - state.lastProbe) >= CSI_SEND_INTERVAL_MS && pcIPStr.length()) {
    state.lastProbe = now;
    const char ping[] = "COMPANION_CSI_PING";
    if (state.udp.beginPacket(pcIPStr.c_str(), CSI_TX_PORT)) {
      state.udp.write(reinterpret_cast<const uint8_t*>(ping), sizeof(ping) - 1);
      state.udp.endPacket();
    }
  }
  if (uint32_t(now - state.lastSend) < CSI_SEND_INTERVAL_MS) return;
  uint8_t packet[CSI_ADR018_HDR_SIZE + CSI_MAX_SUBCARRIERS * 2] = {};
  uint16_t bytes = 0;
  portENTER_CRITICAL(&csiMux);
  if (state.ready) {
    bytes = state.len;
    packet[16] = uint8_t(state.rssi);
    packet[17] = uint8_t(state.noise);
    uint32_t frequency = state.channel == 14 ? 2484 : 2412 + (state.channel - 1) * 5;
    memcpy(packet + 8, &frequency, 4);
    memcpy(packet + CSI_ADR018_HDR_SIZE, state.iq, bytes);
    state.ready = false;
  }
  portEXIT_CRITICAL(&csiMux);
  if (!bytes) return;
  state.lastSend = now;
  memcpy(packet, &CSI_ADR018_MAGIC, 4);
  packet[5] = 1;
  packet[6] = uint8_t(bytes / 2);
  memcpy(packet + 12, &state.sequence, 4);
  ++state.sequence;
  if (!pcIPStr.length()) return;
  if (state.udp.beginPacket(pcIPStr.c_str(), CSI_TX_PORT)) {
    state.udp.write(packet, CSI_ADR018_HDR_SIZE + bytes);
    state.udp.endPacket();
  }
}
#endif
#endif
