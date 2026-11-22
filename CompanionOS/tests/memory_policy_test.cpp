#define COMPANION_HOST_MEMORY
#include "../arduino/CompanionOS_Main/memory_policy.h"
#include <cassert>
#include <iostream>
#include <vector>
namespace companion_memory { Snapshot testSnapshot = {128 * 1024, 96 * 1024}; }
struct Sprite {
  int depth = 0;
  std::vector<int> attempts;
  void setColorDepth(int value) { depth = value; }
  bool createSprite(int, int) { attempts.push_back(depth); return true; }
};
int main() {
  using namespace companion_memory;
  for (unsigned page = 0; page < 17; ++page) {
    uint32_t tag = encodeBootPage(page);
    assert(decodeBootPage(tag, true, 17) == int(page));
    assert(decodeBootPage(tag, false, 17) == 0);
    assert(decodeBootPage(tag ^ 1, true, 17) == 0);
    assert(decodeBootPage(tag ^ 0x10000, true, 17) == 0);
  }
  assert(decodeBootPage(encodeBootPage(17), true, 17) == 0);
  assert(tlsReady({tlsReserve, tlsBlock}));
  assert(!tlsReady({tlsReserve - 1, tlsBlock}));
  assert(!tlsReady({tlsReserve, tlsBlock - 1}));
  assert(permitsVisual({tlsReserve + 4096, tlsBlock + 4096}, 4096));
  assert(!permitsVisual({tlsReserve + 4095, tlsBlock + 4096}, 4096));
  assert(!permitsVisual({tlsReserve + 4096, tlsBlock + 4095}, 4096));
  assert(!permitsVisual({SIZE_MAX, SIZE_MAX}, SIZE_MAX));
  for (int page = 0; page < 17; ++page)
    assert(nearby(page, 1, 17) == (page >= 0 && page <= 2));
  assert(nearby(16, 0, 17) && nearby(0, 16, 17));
  Sprite sprite;
  networkBusy.store(true);
  assert(!createSprite(sprite, 16, 16) && sprite.attempts.empty());
  networkBusy.store(false);
  testSnapshot = {tlsReserve + 20000, tlsBlock + 20000};
  assert(createSprite(sprite, 160, 102));
  assert(sprite.depth == 8 && sprite.attempts.size() == 1); // 16bpp would starve TLS.
  testSnapshot = {tlsReserve, tlsBlock};
  assert(!createSprite(sprite, 160, 102));
  assert(sprite.attempts.size() == 1); // No allocation attempt below the reserve.
  std::cout << "memory policy tests passed\n";
}
