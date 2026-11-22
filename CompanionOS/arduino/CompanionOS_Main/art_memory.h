#ifndef COMPANION_ART_MEMORY_H
#define COMPANION_ART_MEMORY_H

#include "memory_policy.h"
#include <stdlib.h>
#include <string.h>
extern bool artDrawn;

inline bool albumArtPageNearby() {
  return companion_memory::nearby(int(currentState), int(STATE_SPOTIFY), int(STATE_COUNT)) ||
         companion_memory::nearby(int(currentState), int(STATE_LYRICS), int(STATE_COUNT));
}
inline void discardIncomingArt() {
  free(albumArtIncoming); albumArtIncoming = nullptr;
  receivingArt = false; albumArtRowsComplete = 0;
  memset(albumArtRowsReceived, 0, sizeof(albumArtRowsReceived));
}
inline void releaseDistantAlbumArt() {
  if (albumArtPageNearby()) return;
  discardIncomingArt();
  if (albumArt) { free(albumArt); albumArt = nullptr; ++albumArtRevision; }
  galleryImage = nullptr;
  albumArtReady = false; artDrawn = false;
}
inline bool prepareIncomingArt() {
  if (!albumArtPageNearby()) return false;
  if (!albumArtIncoming) {
    // A new transfer invalidates the displayed cover, so reuse its storage.
    albumArtReady = false; artDrawn = false;
    albumArtIncoming = albumArt; albumArt = nullptr; galleryImage = nullptr;
    if (!albumArtIncoming && companion_memory::visualAllowed(ALBUM_ART_BYTES))
      albumArtIncoming = static_cast<uint16_t*>(malloc(ALBUM_ART_BYTES));
  }
  return albumArtIncoming != nullptr;
}
inline bool commitIncomingArt() {
  if (!albumArtIncoming || albumArtRowsComplete != ALBUM_ART_H) return false;
  free(albumArt);
  albumArt = albumArtIncoming; albumArtIncoming = nullptr; galleryImage = albumArt;
  return true;
}
#endif
