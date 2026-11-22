#define COMPANION_HOST_MEMORY
#include "../arduino/CompanionOS_Main/memory_policy.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <cstring>
#include <stdlib.h>
#include <string.h>
namespace companion_memory { Snapshot testSnapshot = {128 * 1024, 96 * 1024}; }
enum State { STATE_EYES, STATE_SPOTIFY, STATE_LYRICS, STATE_POMODORO, STATE_WEATHER, STATE_COUNT = 18 };
State currentState = STATE_EYES;
constexpr int ALBUM_ART_H = 64;
constexpr size_t ALBUM_ART_BYTES = 64 * 64 * sizeof(uint16_t);
uint16_t* albumArt = nullptr;
uint16_t* albumArtIncoming = nullptr;
uint16_t* galleryImage = nullptr;
uint8_t albumArtRowsReceived[64] = {}, albumArtRowsComplete = 0;
uint32_t albumArtRevision = 0;
bool albumArtReady = false, artDrawn = false, receivingArt = false;
static unsigned allocations = 0;
static void* trackedMalloc(size_t bytes) { void* p = std::malloc(bytes); if (p) ++allocations; return p; }
static void trackedFree(void* p) { if (p) { assert(allocations); --allocations; } std::free(p); }
#define malloc trackedMalloc
#define free trackedFree
#include "../arduino/CompanionOS_Main/art_memory.h"
#undef malloc
#undef free
int main() {
  assert(!albumArt && !albumArtIncoming && !allocations); // Nothing reserved at boot.
  for (int pass = 0; pass < 2000; ++pass) {
    currentState = STATE_SPOTIFY;
    assert(prepareIncomingArt() && allocations == 1);
    uint16_t* bytes = albumArtIncoming;
    bytes[0] = uint16_t(pass); bytes[4095] = uint16_t(pass ^ 0xffff);
    albumArtRowsComplete = 63;
    assert(!commitIncomingArt() && !albumArt);
    albumArtRowsComplete = 64;
    assert(commitIncomingArt() && albumArt == bytes && galleryImage == bytes && !albumArtIncoming);
    albumArtReady = true;
    currentState = STATE_POMODORO; releaseDistantAlbumArt();
    assert(albumArt == bytes && allocations == 1); // Adjacent thumbnail is retained.
    assert(prepareIncomingArt() && albumArtIncoming == bytes && !albumArt && !galleryImage);
    assert(allocations == 1 && !albumArtReady); // No second 8KB image during transfer.
    receivingArt = true;
    currentState = STATE_WEATHER; releaseDistantAlbumArt();
    assert(!allocations && !albumArtIncoming && !albumArt && !galleryImage && !receivingArt);
    assert(!prepareIncomingArt() && !allocations); // Off-screen traffic cannot grow RAM.
  }
  currentState = STATE_EYES;
  companion_memory::testSnapshot = {65536, 40960};
  assert(!prepareIncomingArt() && !allocations);
  companion_memory::testSnapshot = {128 * 1024, 96 * 1024};
  assert(prepareIncomingArt()); discardIncomingArt();
  assert(!allocations);
  std::cout << "art memory tests passed: 2000 page/transfer cycles, zero retained allocations\n";
}
