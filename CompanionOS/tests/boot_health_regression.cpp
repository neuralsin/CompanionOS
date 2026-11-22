#define ESP32
#include "boot_host/esp_system.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <type_traits>
esp_reset_reason_t hostReset=ESP_RST_POWERON;
uint32_t hostMillis=0;
uint32_t millis() { return hostMillis; }
unsigned uxTaskGetStackHighWaterMark(void*) { return 4096; }
struct HostSerial {
  template<class... A> void printf(const char*, A...) {}
  void flush() {}
} Serial;
struct HostESP {
  int restarts=0;
  unsigned getFreeHeap() { return 100000; }
  unsigned getMaxAllocHeap() { return 60000; }
  void restart() { ++restarts; }
} ESP;
#include "../arduino/CompanionOS_Main/boot_health.h"
int main() {
  using namespace boot_health;
  static_assert(maxConsecutiveFaults==10,"Requested recovery threshold");
  static_assert(std::is_volatile<decltype(record.stage)>::value,"Crash checkpoint must not be optimized away");
  record={};record.failures=999;record.stage=999;begin();assert(!recovery&&record.failures==0&&previousStage==0);
  hostReset=ESP_RST_PANIC;
  for(unsigned i=1;i<=10;++i) {
    checkpoint(53);begin();
    assert(record.failures==i&&recovery==(i==10)&&previousStage==53);
    assert(strcmp(reasonText(),"CPU exception")==0);
    assert(previousMetricStage==53&&previousFreeHeap==100000&&previousLargestBlock==60000&&previousStackFree==4096);
  }
  begin();assert(record.failures==10&&recovery);
  hostMillis=60000;healthy();assert(record.failures==10); // Recovery cannot clear itself.
  retry();assert(record.failures==0&&ESP.restarts==1);
  hostReset=ESP_RST_SW;hostMillis=0;begin();assert(!recovery);
  hostReset=ESP_RST_TASK_WDT;stage(56);begin();assert(record.failures==1&&previousStage==56);
  hostMillis=59999;healthy();assert(record.failures==1);
  hostMillis=60000;healthy();assert(record.failures==0);
  hostReset=ESP_RST_BROWNOUT;begin();assert(record.failures==1&&!recovery);
  hostReset=ESP_RST_POWERON;begin();assert(record.failures==0&&!recovery);
  puts("PASS boot: ten-fault threshold, stage capture, recovery retry, healthy timeout and reset reasons");
}
