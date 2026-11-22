#ifndef COMPANION_PLANNER_CORE_H
#define COMPANION_PLANNER_CORE_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace companion_planner {
constexpr int eventLimit = 12, classLimit = 16;
constexpr uint32_t earliestTime = 1704067200UL;
static bool overlayActive = false;
struct Event {
  uint32_t id, at, quietUntil;
  char title[41];
  uint8_t kind, done; // 0 exam, 1 reminder
};
struct Class {
  uint32_t id, quietUntil;
  char title[41], place[25];
  uint16_t start, duration; // minutes, local wall clock
  uint8_t day; // 0 Sunday
};
struct Plan {
  uint32_t magic = 0x504C4E31;
  uint32_t revision = 0;
  int16_t offset = 330;
  uint8_t flirt = 1, eventCount = 0, classCount = 0;
  Event events[eventLimit] = {};
  Class classes[classLimit] = {};
};
inline bool validText(const char* text, size_t capacity, bool empty = false) {
  if (!empty && !text[0]) return false;
  for (size_t i = 0; i < capacity; ++i) {
    if (!text[i]) return true;
    if (uint8_t(text[i]) < 32 || uint8_t(text[i]) > 126) return false;
  }
  return false;
}
inline bool valid(const Plan& p) {
  if (p.magic != 0x504C4E31 || p.offset < -720 || p.offset > 840 || p.flirt > 2 ||
      p.eventCount > eventLimit || p.classCount > classLimit) return false;
  for (int i = 0; i < p.eventCount; ++i) {
    const auto& e = p.events[i];
    if (!e.id || e.at < earliestTime || e.at > 4102444800UL || e.kind > 1 || e.done > 1 ||
        !validText(e.title, sizeof(e.title))) return false;
    for (int j = 0; j < i; ++j) if (p.events[j].id == e.id) return false;
  }
  for (int i = 0; i < p.classCount; ++i) {
    const auto& c = p.classes[i];
    if (!c.id || c.day > 6 || c.start >= 1440 || !c.duration || c.duration > 720 ||
        !validText(c.title, sizeof(c.title)) || !validText(c.place, sizeof(c.place), true)) return false;
    for (int j = 0; j < i; ++j) if (p.classes[j].id == c.id) return false;
  }
  return true;
}
inline int nextEvent(const Plan& p, uint32_t now, bool alertsOnly = false) {
  int best = -1;
  if (now < earliestTime) return best;
  for (int i = 0; i < p.eventCount; ++i) {
    const auto& e = p.events[i];
    if (e.done || int64_t(now) > int64_t(e.at) + (e.kind == 0 ? 3600 : 86400)) continue;
    if (alertsOnly && (e.quietUntil > now || int64_t(e.at) - now > (e.kind == 0 ? 259200 : 1800))) continue;
    if (best < 0 || e.at < p.events[best].at) best = i;
  }
  return best;
}
inline uint32_t alertInterval(int64_t remaining) {
  return remaining > 172800 ? 3600 : remaining > 86400 ? 1800 : remaining > 21600 ? 900 : 300;
}
inline uint32_t occurrence(const Class& c, int offset, uint32_t now) {
  if (now < earliestTime) return 0;
  const int64_t local = int64_t(now) + offset * 60;
  const int64_t today = local / 86400;
  const int weekday = int((today + 4) % 7);
  int64_t at = (today + (c.day - weekday + 7) % 7) * 86400 + c.start * 60 - offset * 60;
  if (at - 604800 <= now && at - 604800 + c.duration * 60 > now) at -= 604800;
  if (at + c.duration * 60 <= now) at += 7 * 86400;
  return uint32_t(at);
}
inline int nextClass(const Plan& p, uint32_t now, uint32_t& at, bool alertsOnly = false) {
  at = 0;
  if (now < earliestTime) return -1;
  int best = -1;
  for (int i = 0; i < p.classCount; ++i) {
    const auto& c = p.classes[i];
    uint32_t candidate = occurrence(c, p.offset, now);
    if (alertsOnly && (c.quietUntil > now || int64_t(candidate) - now > 7200)) continue;
    if (best < 0 || candidate < at) { best = i; at = candidate; }
  }
  return best;
}
}
#endif
