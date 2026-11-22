#include "../arduino/CompanionOS_Main/planner_core.h"
#include "../arduino/CompanionOS_Main/thought_personality.h"
#include <cassert>
#include <iostream>
using namespace companion_planner;
int main() {
  Plan p;
  assert(valid(p));
  const uint32_t monday = 1736121600; // 2025-01-06 00:00 UTC, Monday
  p.eventCount = 2;
  p.events[0].id=1; p.events[0].at=monday+3600; strcpy(p.events[0].title,"Physics");
  p.events[1].id=2; p.events[1].at=monday+7200; p.events[1].kind=1; strcpy(p.events[1].title,"Project");
  assert(valid(p) && nextEvent(p, monday) == 0);
  assert(nextEvent(p, 0) == -1);
  p.events[0].done = 1; assert(nextEvent(p, monday) == 1);
  assert(nextEvent(p, monday, true) == -1);
  p.events[0].done = 0; p.events[0].quietUntil = monday + 100;
  assert(nextEvent(p, monday, true) == -1);
  assert(nextEvent(p, monday + 101, true) == 0);
  assert(nextEvent(p, monday + 7201) == 1);
  assert(alertInterval(259200) == 3600 && alertInterval(172800) == 1800);
  assert(alertInterval(86400) == 900 && alertInterval(21600) == 300);
  assert(alertInterval(-1000) == 300);
  p.classCount = 1;
  p.classes[0].id=3; strcpy(p.classes[0].title,"Maths"); strcpy(p.classes[0].place,"A101");
  p.classes[0].start=540; p.classes[0].duration=60; p.classes[0].day=1;
  uint32_t at = 0;
  assert(valid(p));
  assert(nextClass(p, monday, at, true) == -1); // 9am IST is 3:30 UTC
  assert(nextClass(p, monday + 5400, at, true) == 0 && at == monday + 12600);
  assert(nextClass(p, monday + 15000, at, true) == 0); // In progress
  assert(nextClass(p, monday + 16200, at, true) == -1); // End is exclusive
  assert(nextClass(p, monday + 16200, at) == 0 && at == monday + 12600 + 604800);
  p.classes[0].quietUntil = monday + 17000;
  assert(nextClass(p, monday + 15000, at, true) == -1);
  p.classes[0].day = 7; assert(!valid(p)); p.classes[0].day = 1;
  p.classes[0].start = 1440; assert(!valid(p)); p.classes[0].start = 540;
  p.events[1].id = 1; assert(!valid(p)); p.events[1].id = 2;
  p.eventCount = 13; assert(!valid(p)); p.eventCount = 2;
  memset(p.events[0].title, 'a', sizeof(p.events[0].title)); assert(!valid(p));
  p.offset = -300; p.classes[0].quietUntil = 0;
  assert(occurrence(p.classes[0], p.offset, monday) == monday + 14 * 3600);
  p.classes[0].day = 0; p.classes[0].start = 23 * 60 + 30; p.classes[0].duration = 120;
  assert(occurrence(p.classes[0], 0, monday - 1800) == monday - 1800);
  assert(occurrence(p.classes[0], 0, monday) == monday - 1800); // Cross-midnight class
  using namespace thought_personality;
  for (uint8_t level = 0; level <= 2; ++level) {
    flirtLevel = level; sequence = 0; context[0] = 0;
    for (unsigned n = 0; n < 90; ++n) {
      const char* text = choose(true, 2, false, false, 0, n);
      assert(text[0] && strlen(text) < 80);
      if (level && (n + 1) % 3 == 0) assert(text == (level == 1 ? playful[n % 12] : bold[n % 12]));
      else assert(strstr(text, "night") || strstr(text, "Late") || strstr(text, "Quiet"));
    }
  }
  char blank[80] = " \t\r\n\xff "; sanitize(blank, sizeof(blank)); assert(!blank[0]);
  char spaced[80] = "  a   b  "; sanitize(spaced, sizeof(spaced)); assert(!strcmp(spaced, "a b"));
  std::cout << "PASS planner deadlines, recurrence, snooze, validation and 270 contextual thoughts; RAM plan=" << sizeof(Plan) << " bytes\n";
}
