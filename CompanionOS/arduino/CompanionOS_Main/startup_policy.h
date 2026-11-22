#ifndef COMPANION_STARTUP_POLICY_H
#define COMPANION_STARTUP_POLICY_H
#include <stdint.h>
namespace companion_startup {
constexpr uint32_t servicesDelay = 3500, apiDelay = 6500, linkDelay = 1500;
struct Gate {
  uint32_t shownAt = 0, linkedAt = 0;
  bool shown = false, linked = false;
  void firstFrame(uint32_t now) { shownAt = now; shown = true; linked = false; }
  void observeLink(bool connected, uint32_t now) {
    if (!connected) linked = false;
    else if (!linked) { linked = true; linkedAt = now; }
  }
  bool services(uint32_t now) const {
    return shown && linked && uint32_t(now - shownAt) >= servicesDelay && uint32_t(now - linkedAt) >= linkDelay;
  }
  bool apis(uint32_t now) const { return services(now) && uint32_t(now - shownAt) >= apiDelay && uint32_t(now - linkedAt) >= 3000; }
};
static Gate gate;
inline float ease(float t) { return t <= 0 ? 0 : t >= 1 ? 1 : t * t * (3 - 2 * t); }
}
#endif
