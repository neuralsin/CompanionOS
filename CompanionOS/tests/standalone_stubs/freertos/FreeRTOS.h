#pragma once
#include <deque>
#include <stdint.h>
constexpr int pdTRUE = 1, pdPASS = 1;
constexpr uint32_t portMAX_DELAY = UINT32_MAX;
struct MockQueue { std::deque<uint8_t> bytes; };
using QueueHandle_t = MockQueue*;
using TaskHandle_t = void*;
inline QueueHandle_t xQueueCreate(unsigned, unsigned) { return new MockQueue; }
inline void vQueueDelete(QueueHandle_t q) { delete q; }
inline int xQueueSend(QueueHandle_t q, const void* value, uint32_t) {
  if (!q->bytes.empty()) return 0;
  q->bytes.push_back(*static_cast<const uint8_t*>(value)); return 1;
}
inline int xQueueReceive(QueueHandle_t q, void* value, uint32_t) {
  if (q->bytes.empty()) return 0;
  *static_cast<uint8_t*>(value) = q->bytes.front(); q->bytes.pop_front(); return 1;
}
inline int xTaskCreate(void (*)(void*), const char*, unsigned, void*, unsigned, TaskHandle_t* task) { *task = reinterpret_cast<void*>(1); return 1; }
inline void vTaskDelay(unsigned) {}
