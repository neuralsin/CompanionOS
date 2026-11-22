#ifndef COMPANION_MEMORY_POLICY_H
#define COMPANION_MEMORY_POLICY_H

#include <stddef.h>
#include <stdint.h>
#include <atomic>
#if defined(ESP32) && !defined(COMPANION_HOST_MEMORY)
#include <esp_heap_caps.h>
#endif

namespace companion_memory {
inline bool& radioWorkspace() { static bool active = false; return active; }
static std::atomic<bool> networkBusy{false};
constexpr uint32_t bootTag = 0xC05A0000UL;
inline uint32_t encodeBootPage(unsigned page) {
  return bootTag | ((page & 255U) << 8) | ((page ^ 255U) & 255U);
}
inline int decodeBootPage(uint32_t tag, bool softwareReset, int count) {
  unsigned page = (tag >> 8) & 255U;
  return softwareReset && (tag & 0xffff0000UL) == bootTag &&
         ((tag & 255U) == (page ^ 255U)) && page < unsigned(count) ? int(page) : 0;
}
// These are internal SRAM budgets, not the flash partition's free space.
constexpr size_t tlsReserve = 80 * 1024;
// The installed core has two 16 KiB TLS I/O buffers, plus allocation overhead.
constexpr size_t tlsBlock = 24 * 1024;
struct Snapshot { size_t free, largest; };
inline bool permitsVisual(Snapshot ram, size_t bytes) {
  return ram.free >= tlsReserve && bytes <= ram.free - tlsReserve &&
         ram.largest >= tlsBlock && bytes <= ram.largest - tlsBlock;
}
// A graphics budget, NOT a TLS admission test. TLS allocates several smaller
// blocks; the old 40 KiB admission requirement incorrectly rejected valid heaps.
inline bool tlsReady(Snapshot ram) { return ram.free >= tlsReserve && ram.largest >= tlsBlock; }
inline bool nearby(int current, int owner, int count) {
  int distance = current > owner ? current - owner : owner - current;
  return distance <= 1 || distance == count - 1;
}
inline Snapshot snapshot() {
#if defined(COMPANION_HOST_MEMORY)
  extern Snapshot testSnapshot;
  return testSnapshot;
#elif defined(ESP32)
  const uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
  return {heap_caps_get_free_size(caps), heap_caps_get_largest_free_block(caps)};
#else
  return {SIZE_MAX, SIZE_MAX};
#endif
}
inline bool visualAllowed(size_t bytes) {
  return !networkBusy.load(std::memory_order_relaxed) && permitsVisual(snapshot(), bytes);
}
template <class Sprite> bool createSprite(Sprite& sprite, int w, int h) {
  if (w <= 0 || h <= 0) return false;
  for (int depth = 16; depth >= 8; depth /= 2) {
    size_t bytes = size_t(w) * size_t(h) * size_t(depth / 8) + 64;
    if (!visualAllowed(bytes)) continue;
    sprite.setColorDepth(depth);
    if (sprite.createSprite(w, h)) return true;
  }
  return false;
}
}
#endif
