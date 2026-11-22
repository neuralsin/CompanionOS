#define COMPANION_HOST_MEMORY
#include "../arduino/CompanionOS_Main/memory_policy.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
using std::min;
using String = std::string;
namespace companion_memory { Snapshot testSnapshot = {128 * 1024, 96 * 1024}; }
uint32_t now = 100;
uint32_t millis() { return now; }
bool wifiConnected = true;
String pcIPStr = "192.0.2.2";
struct { template<class... T> void printf(const char*, T...) {} } Serial;
using portMUX_TYPE = int;
constexpr int portMUX_INITIALIZER_UNLOCKED = 0;
void enter(int* lock) { assert(!*lock); *lock = 1; }
void leave(int* lock) { assert(*lock); *lock = 0; }
#define portENTER_CRITICAL(lock) enter(lock)
#define portEXIT_CRITICAL(lock) leave(lock)
#define ESP32 1
#include "../arduino/CompanionOS_Main/csi_collector.h"

int main() {
  assert(!csiRuntime && udpInstances == 0 && !callback && !csiEnabled);
  int8_t sample[300];
  for (unsigned i = 0; i < sizeof(sample); ++i) sample[i] = int8_t(i);
  wifi_csi_info_t info = {sample, sizeof(sample), {-60, 14, -92}};
  for (unsigned cycle = 0; cycle < 2000; ++cycle) {
    resumeCSICollector();
    assert(csiRuntime && udpInstances == 1 && csiEnabled && callback);
    resumeCSICollector(); assert(udpInstances == 1);
    now += 50; tickCSICollector();
    assert(std::string(sentPacket.begin(), sentPacket.end()) == "COMPANION_CSI_PING");
    assert(csiRuntime->sequence == 0); // No fabricated CSI frame when there is no sample.
    callback(nullptr, &info);
    now += 50; tickCSICollector();
    assert(sentPacket.size() == 276 && sentPacket[6] == 128 && sentPacket[7] == 0);
    uint32_t magic, frequency;
    memcpy(&magic, sentPacket.data(), 4);
    memcpy(&frequency, sentPacket.data() + 8, 4);
    assert(magic == CSI_ADR018_MAGIC && frequency == 2484);
    assert(!memcmp(sentPacket.data() + 20, sample, 256));
    auto lateCallback = callback;
    pauseCSICollector();
    lateCallback(nullptr, &info); // Already queued Wi-Fi callback cannot use freed memory.
    assert(!csiRuntime && !callback && !csiEnabled && udpInstances == 0);
    pauseCSICollector();
  }
  configError = 1; resumeCSICollector(); assert(!csiRuntime && udpInstances == 0 && !csiEnabled);
  configError = 0; now += 2999; resumeCSICollector(); assert(!csiRuntime);
  now += 1; callbackError = 1; resumeCSICollector(); assert(!csiRuntime && udpInstances == 0);
  callbackError = 0; enableError = 1; now += 3000;
  resumeCSICollector(); assert(!csiRuntime && !callback && !csiEnabled && udpInstances == 0);
  enableError = 0; now += 3000;
  companion_memory::networkBusy.store(true);
  resumeCSICollector(); assert(!csiRuntime);
  companion_memory::networkBusy.store(false);
  resumeCSICollector(); assert(csiRuntime);
  wifiConnected = false; tickCSICollector(); assert(!csiRuntime && udpInstances == 0);
  std::cout << "CSI lifecycle: 2000 open/close cycles, no retained UDP/sample allocation\n";
}
