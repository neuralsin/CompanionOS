#ifndef COMPANION_BOOT_HEALTH_H
#define COMPANION_BOOT_HEALTH_H
#ifdef ESP32
#include <esp_system.h>
namespace boot_health {
constexpr uint32_t maxConsecutiveFaults = 10;
constexpr uint32_t recordMagic = 0x434F5332;
constexpr const char* buildTag = "B23W2";
// These writes must reach RTC memory even when the CPU panics mid-function.
struct Record { volatile uint32_t magic, failures, stage, metricStage, freeHeap, largestBlock, stackFree; };
RTC_NOINIT_ATTR Record record;
static bool recovery = false;
static uint32_t previousStage = 0;
static uint32_t previousMetricStage = 0, previousFreeHeap = 0, previousLargestBlock = 0, previousStackFree = 0;
static esp_reset_reason_t lastReset = ESP_RST_UNKNOWN;
inline const char* reasonText() {
  switch (lastReset) {
    case ESP_RST_BROWNOUT: return "Low supply voltage";
    case ESP_RST_PANIC: return "CPU exception";
    case ESP_RST_INT_WDT: case ESP_RST_TASK_WDT: case ESP_RST_WDT: return "Watchdog timeout";
    default: return "Startup reset";
  }
}
inline void begin() {
  const auto reason = esp_reset_reason();
  bool fault = reason == ESP_RST_PANIC || reason == ESP_RST_INT_WDT ||
               reason == ESP_RST_TASK_WDT || reason == ESP_RST_WDT || reason == ESP_RST_BROWNOUT;
  if (record.magic != recordMagic || !fault) record = {recordMagic, 0, 0, 0, 0, 0, 0};
  if (fault && record.failures < maxConsecutiveFaults) ++record.failures;
  recovery = record.failures >= maxConsecutiveFaults;
  previousStage = record.stage; lastReset = reason;
  previousMetricStage = record.metricStage; previousFreeHeap = record.freeHeap;
  previousLargestBlock = record.largestBlock; previousStackFree = record.stackFree;
  Serial.printf("[boot] reset=%d previous_stage=%u consecutive_faults=%u recovery=%d heap=%u\n",
      int(reason), unsigned(record.stage), unsigned(record.failures), recovery, unsigned(ESP.getFreeHeap()));
  record.stage = 1;
  record.metricStage = record.freeHeap = record.largestBlock = record.stackFree = 0;
  Serial.printf("[boot:%s] last_metrics_stage=%u free=%u largest=%u stack=%u\n", buildTag,
      unsigned(previousMetricStage), unsigned(previousFreeHeap), unsigned(previousLargestBlock), unsigned(previousStackFree));
}
inline void stage(uint32_t stage) { record.stage = stage; }
inline void checkpoint(uint32_t value) {
  stage(value);
  record.metricStage = value;
  record.freeHeap = ESP.getFreeHeap();
  record.largestBlock = ESP.getMaxAllocHeap();
  record.stackFree = uxTaskGetStackHighWaterMark(nullptr);
  Serial.printf("[boot:%s] stage=%u free=%u largest=%u stack=%u\n", buildTag, unsigned(value),
      unsigned(record.freeHeap), unsigned(record.largestBlock), unsigned(record.stackFree));
}
inline void healthy() {
  if (!recovery && millis() >= 60000) record.failures = 0;
}
inline void retry() { record.failures = 0; Serial.flush(); ESP.restart(); }
}
#else
namespace boot_health {
static bool recovery = false;
inline void begin() {} inline void stage(uint32_t) {} inline void healthy() {}
inline void checkpoint(uint32_t) {}
inline void retry() { ESP.restart(); }
}
#endif
#endif
