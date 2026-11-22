#ifndef COMPANION_LYRICS_STATE_H
#define COMPANION_LYRICS_STATE_H
#include <stdint.h>

namespace companion_lyrics {
struct Timing {
  int64_t start = -1, next = -1;
  int word = -1;
  bool wordTimed = false;
};
inline Timing& timing() { static Timing value; return value; }

// A manual departure lasts for the playback session, including brief pauses.
// Rearm only after a confirmed five-second stop, never on a track change.
struct Navigation {
  bool session = false, dismissed = false, stopping = false;
  uint32_t stoppedAt = 0;
  void manual(bool toLyrics) { dismissed = !toLyrics; }
  enum Action { None, Open, Home };
  Action tick(uint32_t now, bool playing, bool onLyrics, bool canOpen = true) {
    if (playing) {
      stopping = false;
      if (!session) {
        session = true;
        if (!dismissed && !onLyrics && canOpen) return Open;
      }
      return None;
    }
    if (!session) return None;
    if (!stopping) { stopping = true; stoppedAt = now; }
    if (uint32_t(now - stoppedAt) < 5000) return None;
    session = dismissed = stopping = false;
    return onLyrics ? Home : None;
  }
};
inline Navigation& navigation() { static Navigation value; return value; }
}
#endif
