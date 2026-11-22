#ifndef DH_DISCOVERY_H
#define DH_DISCOVERY_H

#include "globals.h"
#include <memory>
#include <new>
#ifndef DH_DISCOVERY_HOST
#include <WiFi.h>
#include <esp_wifi.h>
#if ENABLE_BLE_TOOLS
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <esp_bt_main.h>
#include <esp32-hal-bt.h>
#endif
#endif

enum DhDiscoveryError {
  DH_DISCOVERY_CANCELLED = -10, DH_DISCOVERY_TIMEOUT = -11,
  DH_DISCOVERY_INIT_FAILED = -12, DH_DISCOVERY_START_FAILED = -13,
  DH_DISCOVERY_NO_MEMORY = -14, DH_DISCOVERY_BUSY = -15, DH_DISCOVERY_DISABLED = -16
};

static const char* dhDiscoveryErrorText(int code) {
  switch (code) {
    case DH_DISCOVERY_CANCELLED: return "Scan cancelled";
    case DH_DISCOVERY_TIMEOUT: return "Scan timed out";
    case DH_DISCOVERY_INIT_FAILED: return "Radio initialization failed";
    case DH_DISCOVERY_START_FAILED: return "Radio rejected scan";
    case DH_DISCOVERY_NO_MEMORY: return "Not enough scan memory";
    case DH_DISCOVERY_BUSY: return "Radio is already in use";
    case DH_DISCOVERY_DISABLED: return "Bluetooth disabled";
    default: return "Scan failed";
  }
}

struct DhDiscoveryWait {
  DrHackSubState owner = dhCurrentState;
  bool armed = digitalRead(BTN_SELECT) != BTN_ACTIVE_LEVEL;
  uint32_t pressedAt = 0;
  bool pressed = false;
  bool cancelled() {
    extern void handleNetwork(); handleNetwork();
    if (currentState != STATE_DR_HACK || dhCurrentState != owner || virtualSelectPressed) {
      virtualSelectPressed = false;
      return true;
    }
    bool down = digitalRead(BTN_SELECT) == BTN_ACTIVE_LEVEL;
    if (!down) { armed = true; pressed = false; }
    else if (armed) {
      if (!pressed) { pressed = true; pressedAt = millis(); }
      if (uint32_t(millis() - pressedAt) >= 50) return true;
    }
    return false;
  }
};

extern bool beginWiFiDiagnosticScan();
extern void endWiFiDiagnosticScan();

// Only passive discovery callers use this lease; transmit tools are unchanged.
static int dhPassiveWifiScan(uint8_t channel = 0) {
  if (!beginWiFiDiagnosticScan()) return DH_DISCOVERY_BUSY;
  struct Lease { ~Lease() { endWiFiDiagnosticScan(); } } lease;
  DhDiscoveryWait wait;
  if (!WiFi.mode(WIFI_STA)) return DH_DISCOVERY_INIT_FAILED;
  esp_wifi_scan_stop(); WiFi.scanDelete();
  int count = WiFi.scanNetworks(true, true, true, 160, channel);
  const uint32_t started = millis();
  while (count == WIFI_SCAN_RUNNING) {
    if (wait.cancelled()) { count = DH_DISCOVERY_CANCELLED; break; }
    if (uint32_t(millis() - started) >= 6000) { count = DH_DISCOVERY_TIMEOUT; break; }
    delay(10); yield();
    count = WiFi.scanComplete();
  }
  if (count < 0) {
    esp_wifi_scan_stop(); WiFi.scanDelete();
    if (count == WIFI_SCAN_FAILED) count = DH_DISCOVERY_START_FAILED;
  }
  Serial.printf("[dr:discovery:wifi] result=%d elapsed=%lu\n", count, (unsigned long)(millis() - started));
  return count; // Successful result storage belongs to the caller until scanDelete().
}

struct DhBleRecord {
  char name[40] = {}, address[18] = {};
  int16_t rssi = -127;
  uint16_t company = 0xFFFF, appearance = 0xFFFF;
  uint8_t services = 0;
};
constexpr size_t DH_BLE_RECORD_LIMIT = 20;
#if ENABLE_BLE_TOOLS
struct DhBleCapture {
  DhBleRecord records[DH_BLE_RECORD_LIMIT];
  size_t count = 0;
};
static portMUX_TYPE dhBleCaptureMux = portMUX_INITIALIZER_UNLOCKED;
static DhBleCapture* dhBleCapture = nullptr;

class DhBleReceiver : public BLEAdvertisedDeviceCallbacks {
 public:
  void onResult(BLEAdvertisedDevice device) override {
    DhBleRecord record;
    String name = device.haveName() ? device.getName() : String();
    String address = device.getAddress().toString();
    strncpy(record.name, name.c_str(), sizeof(record.name) - 1);
    strncpy(record.address, address.c_str(), sizeof(record.address) - 1);
    record.rssi = constrain(device.getRSSI(), -127, 0);
    if (device.haveManufacturerData()) {
      String data = device.getManufacturerData();
      if (data.length() >= 2) record.company = uint8_t(data[0]) | (uint16_t(uint8_t(data[1])) << 8);
    }
    if (device.haveAppearance()) record.appearance = device.getAppearance();
    record.services = constrain(device.getServiceUUIDCount(), 0, 255);
    // Only fixed-size POD data is accessed while the short cross-task lock is held.
    portENTER_CRITICAL(&dhBleCaptureMux);
    if (dhBleCapture) {
      auto& capture = *dhBleCapture;
      size_t slot = 0;
      while (slot < capture.count && strcmp(capture.records[slot].address, record.address)) ++slot;
      if (slot < capture.count) {
        const auto& previous = capture.records[slot];
        // Individual advertising packets may omit fields already observed.
        if (!record.name[0]) memcpy(record.name, previous.name, sizeof(record.name));
        if (record.company == 0xFFFF) record.company = previous.company;
        if (record.appearance == 0xFFFF) record.appearance = previous.appearance;
        record.services = max(record.services, previous.services);
      } else if (capture.count < DH_BLE_RECORD_LIMIT) ++capture.count;
      if (slot < capture.count) capture.records[slot] = record;
    }
    portEXIT_CRITICAL(&dhBleCaptureMux);
  }
};
static DhBleReceiver dhBleReceiver;

static int dhPassiveBleScan(DhBleRecord* output, size_t capacity, uint32_t seconds = 5) {
  if (!output || !capacity) return DH_DISCOVERY_NO_MEMORY;
  seconds = constrain(seconds, 1U, 15U);
  portENTER_CRITICAL(&dhBleCaptureMux);
  bool occupied = dhBleCapture != nullptr;
  portEXIT_CRITICAL(&dhBleCaptureMux);
  if (occupied) return DH_DISCOVERY_BUSY;
  #if HAS_BLUETOOTH
  extern bool btInitialized;
  if (btInitialized) return DH_DISCOVERY_BUSY;
  #endif
  // The previous toolkit role may have left an initialized BLE stack behind.
  if (BLEDevice::getInitialized()) BLEDevice::deinit(false);
  std::unique_ptr<DhBleCapture> capture(new (std::nothrow) DhBleCapture());
  if (!capture) return DH_DISCOVERY_NO_MEMORY;
  auto ram = companion_memory::snapshot();
  Serial.printf("[dr:discovery:ble] init free=%u largest=%u\n", unsigned(ram.free), unsigned(ram.largest));
  BLEDevice::init("");
  if (!BLEDevice::getInitialized()) {
    #if defined(CONFIG_BLUEDROID_ENABLED)
    // BLEDevice::deinit returns early after a partial initialization failure.
    if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_ENABLED) esp_bluedroid_disable();
    if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_INITIALIZED) esp_bluedroid_deinit();
    btStop();
    #endif
    Serial.printf("[dr:discovery:ble] initialization failed\n");
    return DH_DISCOVERY_INIT_FAILED;
  }
  BLEScan* scan = BLEDevice::getScan();
  if (!scan) { BLEDevice::deinit(false); return DH_DISCOVERY_INIT_FAILED; }
  scan->setActiveScan(false); scan->setInterval(100); scan->setWindow(60);
  // With the ESP32 Bluedroid backend, duplicate callbacks avoid its unbounded
  // result map. This receiver deduplicates into the fixed 20-record buffer.
  scan->setAdvertisedDeviceCallbacks(&dhBleReceiver, true);
  portENTER_CRITICAL(&dhBleCaptureMux);
  dhBleCapture = capture.get();
  portEXIT_CRITICAL(&dhBleCaptureMux);
  DhDiscoveryWait wait;
  const uint32_t started = millis();
  int result = 0;
  if (!scan->start(seconds, static_cast<void (*)(BLEScanResults)>(nullptr), false)) result = DH_DISCOVERY_START_FAILED;
  while (result == 0 && scan->isScanning()) {
    if (wait.cancelled()) { result = DH_DISCOVERY_CANCELLED; break; }
    if (uint32_t(millis() - started) >= seconds * 1000 + 2000) { result = DH_DISCOVERY_TIMEOUT; break; }
    delay(10); yield();
  }
  portENTER_CRITICAL(&dhBleCaptureMux);
  dhBleCapture = nullptr; // Late advertising callbacks cannot access freed storage.
  portEXIT_CRITICAL(&dhBleCaptureMux);
  if (scan->isScanning()) scan->stop();
  scan->setAdvertisedDeviceCallbacks(nullptr, true);
  if (result == 0) {
    result = min(capacity, capture->count);
    for (int i = 0; i < result; ++i) output[i] = capture->records[i];
  }
  scan->clearResults(); BLEDevice::deinit(false);
  Serial.printf("[dr:discovery:ble] result=%d elapsed=%lu\n", result, (unsigned long)(millis() - started));
  return result;
}
#else
static int dhPassiveBleScan(DhBleRecord*, size_t, uint32_t = 5) { return DH_DISCOVERY_DISABLED; }
#endif

#endif
