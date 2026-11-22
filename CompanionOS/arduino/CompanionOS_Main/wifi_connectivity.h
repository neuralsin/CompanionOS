#ifndef COMPANION_WIFI_CONNECTIVITY_H
#define COMPANION_WIFI_CONNECTIVITY_H
#include <atomic>
#include "startup_policy.h"

struct WiFiProfile { String ssid, pass; };
#define MAX_WIFI_PROFILES 4
WiFiProfile wifiProfiles[MAX_WIFI_PROFILES];
int wifiProfileCount = 0;
String currentWiFiSSID = WIFI_SSID, currentWiFiPASS = WIFI_PASS;
static constexpr int wifiConfigAddress = EEPROM_SIZE - 8;
static_assert(EEPROM_WIFI_SSID_ADDR + MAX_WIFI_PROFILES * 96 <= wifiConfigAddress,
              "Wi-Fi profiles overlap config metadata");

bool validWiFiProfile(const String& ssid, const String& pass) {
  if (!ssid.length() || ssid.length() > 32) return false;
  for (unsigned i = 0; i < ssid.length(); ++i)
    if (uint8_t(ssid[i]) < 32 || uint8_t(ssid[i]) > 126) return false;
  if (!pass.length()) return true;
  if (pass.length() < 8 || pass.length() > 64) return false;
  for (unsigned i = 0; i < pass.length(); ++i) {
    uint8_t c = pass[i];
    if (c < 32 || c > 126) return false;
    if (pass.length() == 64 && !((c >= '0' && c <= '9') ||
        (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
  }
  return true;
}
static void addWiFiProfile(WiFiProfile* profiles, int& count, const String& ssid,
                           const String& pass, bool replace = true) {
  if (!validWiFiProfile(ssid, pass)) return;
  for (int i = 0; i < count; ++i) if (profiles[i].ssid == ssid) {
    if (replace) profiles[i].pass = pass;
    return;
  }
  if (count < MAX_WIFI_PROFILES) profiles[count++] = {ssid, pass};
}
uint32_t wifiConfigFingerprint() {
  uint32_t value = 2166136261UL;
  const char* fields[] = {WIFI_SSID, WIFI_PASS, WIFI_SSID2, WIFI_PASS2};
  for (const char* field : fields) {
    for (; *field; ++field) { value ^= uint8_t(*field); value *= 16777619UL; }
    value ^= 0; value *= 16777619UL;
  }
  return value;
}
bool savedWiFiConfigMatches(uint32_t value) {
  if (EEPROM.read(wifiConfigAddress) != 0xC0 || EEPROM.read(wifiConfigAddress + 1) != 0x57) return false;
  for (int i = 0; i < 4; ++i)
    if (EEPROM.read(wifiConfigAddress + 2 + i) != int((value >> (i * 8)) & 255)) return false;
  return true;
}
static bool stageWiFiProfiles(const WiFiProfile* profiles, int count) {
  if (!EEPROM.begin(EEPROM_SIZE)) return false;
  EEPROM.write(EEPROM_WIFI_MAGIC_ADDR, 0xC0);
  EEPROM.write(EEPROM_WIFI_MAGIC_ADDR + 1, 0x56);
  EEPROM.write(EEPROM_WIFI_MAGIC_ADDR + 2, count);
  int address = EEPROM_WIFI_SSID_ADDR;
  for (int p = 0; p < count; ++p) {
    for (int i = 0; i < 32; ++i)
      EEPROM.write(address++, unsigned(i) < profiles[p].ssid.length() ? profiles[p].ssid[i] : 0);
    for (int i = 0; i < 64; ++i)
      EEPROM.write(address++, unsigned(i) < profiles[p].pass.length() ? profiles[p].pass[i] : 0);
  }
  uint32_t fingerprint = wifiConfigFingerprint();
  EEPROM.write(wifiConfigAddress, 0xC0); EEPROM.write(wifiConfigAddress + 1, 0x57);
  for (int i = 0; i < 4; ++i) EEPROM.write(wifiConfigAddress + 2 + i, (fingerprint >> (i * 8)) & 255);
  bool saved = EEPROM.commit(); EEPROM.end();
  return saved;
}
void loadWiFiCredentials() {
  wifiProfileCount = 0;
  const bool preferSecond = String(WIFI_PREFERRED_SSID) == WIFI_SSID2;
  addWiFiProfile(wifiProfiles, wifiProfileCount, preferSecond ? WIFI_SSID2 : WIFI_SSID,
                 preferSecond ? WIFI_PASS2 : WIFI_PASS);
  addWiFiProfile(wifiProfiles, wifiProfileCount, preferSecond ? WIFI_SSID : WIFI_SSID2,
                 preferSecond ? WIFI_PASS : WIFI_PASS2);
  bool available = EEPROM.begin(EEPROM_SIZE), sameConfig = false;
  if (available) {
    sameConfig = savedWiFiConfigMatches(wifiConfigFingerprint());
    if (EEPROM.read(EEPROM_WIFI_MAGIC_ADDR) == 0xC0) {
      int version = EEPROM.read(EEPROM_WIFI_MAGIC_ADDR + 1);
      int count = version == 0x56 ? min(MAX_WIFI_PROFILES, int(EEPROM.read(EEPROM_WIFI_MAGIC_ADDR + 2))) :
                  (version == 0x55 ? 1 : 0);
      int address = EEPROM_WIFI_SSID_ADDR;
      for (int p = 0; p < count; ++p) {
        char ssid[33] = {}, pass[65] = {};
        for (int i = 0; i < 32; ++i) ssid[i] = char(EEPROM.read(address++));
        if (version == 0x55) address = EEPROM_WIFI_PASS_ADDR;
        for (int i = 0; i < 64; ++i) pass[i] = char(EEPROM.read(address++));
        addWiFiProfile(wifiProfiles, wifiProfileCount, ssid, pass, sameConfig);
      }
    }
    EEPROM.end();
  }
  // Compiled password updates win once; subsequent intentional saved edits win.
  if (available && !sameConfig && wifiProfileCount) stageWiFiProfiles(wifiProfiles, wifiProfileCount);
  currentWiFiSSID = wifiProfileCount ? wifiProfiles[0].ssid : String();
  currentWiFiPASS = wifiProfileCount ? wifiProfiles[0].pass : String();
}
void saveMultiWiFiCredentials(const std::vector<std::pair<String, String>>& networks) {
  WiFiProfile profiles[MAX_WIFI_PROFILES];
  int count = 0;
  for (const auto& network : networks) addWiFiProfile(profiles, count, network.first, network.second);
  if (!count || !stageWiFiProfiles(profiles, count)) return;
  loadWiFiCredentials();
}
void saveWiFiCredentials(String ssid, String pass) {
  if (validWiFiProfile(ssid, pass)) saveMultiWiFiCredentials({{ssid, pass}});
}

static bool wifiServicesStarted = false, wifiRoutesRegistered = false;
static bool wifiAttemptActive = false, wifiCycleWaiting = false;
static bool wifiDiagnosticScanActive = false;
static int wifiProfileIndex = 0;
static uint32_t wifiAttemptAt = 0, wifiCycleFinishedAt = 0;
static std::atomic<uint32_t> wifiDisconnectGeneration{0};
static uint32_t wifiObservedDisconnect = 0;
static bool wifiEventsRegistered = false;
static bool wifiDriverReady = false;
static uint32_t wifiDriverAttemptAt = 0;
// Implemented by the UI owner; only the main task may release its canvases.
bool prepareWiFiMemory();
static void wifiStartupStage(uint32_t value) {
#ifdef COMPANION_BOOT_HEALTH_H
  boot_health::checkpoint(value);
#endif
  Serial.printf("[boot:wifi] step=%u\n", unsigned(value));
}
#ifdef ESP8266
static WiFiEventHandler wifiDisconnectHandler;
#endif
extern bool pcFound;
extern unsigned long lastDiscoveryShout;
extern bool pcDashListening;

bool beginWiFiDiagnosticScan() {
  if (!wifiDriverReady || wifiDiagnosticScanActive || dhNetPaused) return false;
  wifiDiagnosticScanActive = true;
  if (WiFi.status() != WL_CONNECTED) {
    // An association in progress makes esp_wifi_scan_start reject discovery.
    #ifdef ESP32
    WiFi.disconnect(false, false, 0);
    #else
    WiFi.disconnect(false, false);
    #endif
    wifiAttemptActive = wifiCycleWaiting = false;
  }
  return true;
}
void endWiFiDiagnosticScan() { wifiDiagnosticScanActive = false; }

static void stopWiFiServices() {
  companion_startup::gate.observeLink(false, millis());
  if (!wifiServicesStarted) return;
  sysWebServer.stop(); udp.stop(); udpPcDash.stop();
  pcDashListening = false;
  wifiServicesStarted = false; pcFound = false;
}
#include "remote_screen.h"
#include "planner_store.h"
static void startWiFiServices() {
  if (!wifiRoutesRegistered) {
    wifiRoutesRegistered = true;
    registerRemoteScreenRoutes();
    companion_planner::registerRoutes();
    sysWebServer.on("/", HTTP_GET, [](){ sysWebServer.send(200, "text/html", MEMORIES_HTML); });
    sysWebServer.on("/memories", HTTP_GET, [](){ sysWebServer.send(200, "text/html", MEMORIES_HTML); });
    sysWebServer.on("/upload", HTTP_POST, [](){
      sysWebServer.send(customEyeReady ? 200 : 400, "text/plain", customEyeReady ? "OK" : "Invalid image");
    }, [](){
      HTTPUpload& upload = sysWebServer.upload();
      if (upload.status == UPLOAD_FILE_START) {
        memoryUploadValid = prepareCustomEyeImage();
        customEyeActive = customEyeReady = false;
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        const size_t limit = 160 * 128 * 2;
        if (memoryUploadValid && customEyeImg && upload.totalSize <= limit &&
            upload.currentSize <= limit - upload.totalSize)
          memcpy(reinterpret_cast<uint8_t*>(customEyeImg) + upload.totalSize, upload.buf, upload.currentSize);
        else memoryUploadValid = false;
      } else if (upload.status == UPLOAD_FILE_END) {
        customEyeReady = memoryUploadValid && upload.totalSize == 160 * 128 * 2;
        customEyeActive = customEyeReady;
        if (currentState == STATE_EYES) renderCurrentPage();
      } else if (upload.status == UPLOAD_FILE_ABORTED) {
        memoryUploadValid = customEyeReady = customEyeActive = false;
      }
    });
  }
  sysWebServer.begin(); udp.begin(UDP_PORT_RX);
  #ifdef ESP32
  configTime(NTP_OFFSET, 0, NTP_SERVER);
  #else
  timeClient.begin();
  #endif
  pcFound = false; lastDiscoveryShout = millis();
  udp.beginPacket("255.255.255.255", UDP_PORT_TX);
  const char* hello = "HELLO_COMPANION";
  udp.write(reinterpret_cast<const uint8_t*>(hello), strlen(hello)); udp.endPacket();
  wifiServicesStarted = true;
}
static void beginWiFiAttempt(int profile, uint32_t now) {
  if (!wifiDriverReady || profile < 0 || profile >= wifiProfileCount) return;
  wifiProfileIndex = profile;
  wifiStartupStage(55);
  #ifdef ESP32
  WiFi.disconnect(false, false, 0);
  #else
  WiFi.disconnect(false, false);
  #endif
  currentWiFiSSID = wifiProfiles[profile].ssid; currentWiFiPASS = wifiProfiles[profile].pass;
  wifiStartupStage(56);
  WiFi.begin(currentWiFiSSID.c_str(), currentWiFiPASS.c_str());
  wifiAttemptAt = now; wifiAttemptActive = true; wifiCycleWaiting = false;
  wifiStartupStage(57);
}
static bool startWiFiDriver(uint32_t now) {
  wifiDriverAttemptAt = now;
  wifiDriverReady = false;
  wifiStartupStage(52);
  #ifdef ESP32
  if (!prepareWiFiMemory()) {
    Serial.println("[wifi] Insufficient startup RAM; staying offline for 120s");
    return false;
  }
  #endif
  WiFi.persistent(false);
  wifiStartupStage(53);
  if (!WiFi.mode(WIFI_STA)) {
    Serial.println("[wifi] Driver initialization failed; staying offline for 120s");
    return false;
  }
  WiFi.setAutoReconnect(false);
  wifiStartupStage(54);
  if (!wifiEventsRegistered) {
    #ifdef ESP32
    wifiEventsRegistered = WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t) {
      wifiDisconnectGeneration.fetch_add(1, std::memory_order_relaxed);
    }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED) != 0;
    #else
    wifiDisconnectHandler = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected&) {
      wifiDisconnectGeneration.fetch_add(1, std::memory_order_relaxed);
    });
    wifiEventsRegistered = bool(wifiDisconnectHandler);
    #endif
  }
  if (!wifiEventsRegistered) {
    Serial.println("[wifi] Event registration failed; staying offline for 120s");
    return false;
  }
  wifiDriverReady = true;
  return true;
}
void setupWiFi() {
  wifiStartupStage(50);
  stopWiFiServices();
  wifiConnected = wifiAttemptActive = wifiCycleWaiting = false;
  wifiDriverReady = false;
  wifiDriverAttemptAt = millis();
  wifiStartupStage(51);
  loadWiFiCredentials();
  companion_startup::gate.observeLink(false, millis());
  if (wifiProfileCount && !dhNetPaused && startWiFiDriver(millis())) beginWiFiAttempt(0, millis());
}
void tickWiFiConnectivity() {
  if (wifiDiagnosticScanActive) return;
  uint32_t now = millis();
  if (dhNetPaused) {
    stopWiFiServices(); wifiConnected = wifiAttemptActive = wifiCycleWaiting = false;
    return;
  }
  if (!wifiDriverReady) {
    if (!wifiProfileCount || uint32_t(now - wifiDriverAttemptAt) < 120000) return;
    if (startWiFiDriver(now)) beginWiFiAttempt(0, now);
    return;
  }
  uint32_t generation = wifiDisconnectGeneration.load(std::memory_order_relaxed);
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true; wifiAttemptActive = wifiCycleWaiting = false;
    if (generation != wifiObservedDisconnect) stopWiFiServices();
    wifiObservedDisconnect = generation;
    companion_startup::gate.observeLink(true, now);
    if (!wifiServicesStarted && companion_startup::gate.services(now)) startWiFiServices();
    return;
  }
  wifiConnected = false; stopWiFiServices();
  companion_startup::gate.observeLink(false, now);
  if (!wifiProfileCount) return;
  if (wifiAttemptActive) {
    if (uint32_t(now - wifiAttemptAt) < 12000) return;
    if (wifiProfileIndex + 1 < wifiProfileCount) beginWiFiAttempt(wifiProfileIndex + 1, now);
    else { wifiAttemptActive = false; wifiCycleWaiting = true; wifiCycleFinishedAt = now; }
  } else if (!wifiCycleWaiting || uint32_t(now - wifiCycleFinishedAt) >= 120000) {
    beginWiFiAttempt(0, now);
  }
}
#endif
