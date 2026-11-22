#ifndef COMPANION_PAGE_PLANNER_H
#define COMPANION_PAGE_PLANNER_H
#include "planner_store.h"
#include "ui_components.h"
extern void drawPageIndicator(int, int);
extern void renderCurrentPage();
namespace companion_planner {
static int cursor = 0;
static uint32_t drawnMinute = 0, drawnRevision = ~0UL;
inline String countdown(uint32_t at, uint32_t now) {
  int64_t seconds = int64_t(at) - now;
  if (seconds <= 0) return "Now / due";
  if (seconds < 3600) return String(int((seconds + 59) / 60)) + " min";
  if (seconds < 86400) return String(int((seconds + 3599) / 3600)) + " hr";
  return String(int((seconds + 86399) / 86400)) + " days";
}
inline void draw(bool classes) {
  const bool compact = SCREEN_W < 240;
  const int top = UI_HEADER_H + 5, bottom = SCREEN_H - UI_FOOTER_H;
  const uint32_t now = uint32_t(time(nullptr));
  drawnMinute = now / 60; drawnRevision = revision;
  tft.fillRect(0, UI_HEADER_H, SCREEN_W, bottom - UI_HEADER_H, CLR_BG);
  tft.setTextDatum(TL_DATUM); tft.setTextSize(1);
  const int count = classes ? plan.classCount : plan.eventCount;
  drawTruncatedText(8, top, classes ? "WEEKLY CLASSES" : "NEXT 14 DAYS", SCREEN_W - 64, CLR_PRIMARY, 1, CLR_BG);
  if (now < earliestTime || !count) {
    drawWrappedText(8, top + 24, now < earliestTime ? "Waiting for clock sync" :
        classes ? "No classes scheduled" : "No exams or reminders", SCREEN_W - 16, bottom - top - 24, CLR_TEXT_MED, compact ? 1 : 2, CLR_BG);
    drawPageIndicator(int(currentState), STATE_COUNT); return;
  }
  int order[classLimit]; uint32_t dates[classLimit];
  for (int i = 0; i < count; ++i) {
    order[i] = i; dates[i] = classes ? occurrence(plan.classes[i], plan.offset, now) : plan.events[i].at;
  }
  for (int i = 0; i < count; ++i) for (int j = i + 1; j < count; ++j) {
    bool earlier = dates[order[j]] < dates[order[i]];
    if (!classes && plan.events[order[i]].done != plan.events[order[j]].done)
      earlier = plan.events[order[i]].done;
    if (earlier) { int tmp = order[i]; order[i] = order[j]; order[j] = tmp; }
  }
  cursor = cursor % count;
  drawTruncatedText(SCREEN_W - 52, top, (String(cursor + 1) + "/" + count).c_str(), 48, CLR_TEXT_MED, 1, CLR_BG);
  int y = top + 18;
  if (!classes) {
    int bins[14] = {}, peak = 1;
    const int64_t today = (int64_t(now) + plan.offset * 60) / 86400;
    for (int i = 0; i < count; ++i) {
      const auto& e = plan.events[i];
      int day = int((int64_t(e.at) + plan.offset * 60) / 86400 - today);
      if (!e.done && day >= 0 && day < 14) peak = max(peak, ++bins[day]);
    }
    const int height = compact ? 18 : 38, step = (SCREEN_W - 16) / 14;
    tft.drawFastHLine(8, y + height, SCREEN_W - 16, CLR_BORDER);
    for (int day = 0; day < 14; ++day) {
      int h = bins[day] * height / peak;
      if (h) tft.fillRect(8 + day * step, y + height - h, max(2, step - 3), h, day < 3 ? TFT_RED : CLR_PRIMARY);
    }
    y += height + 8;
  }
  const int rowH = compact ? 30 : 44;
  const int rows = max(1, (bottom - y - 2) / rowH);
  static const char* const days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  for (int row = 0; row < rows && cursor + row < count; ++row) {
    int index = order[cursor + row];
    const char* title = classes ? plan.classes[index].title : plan.events[index].title;
    drawTruncatedText(8, y, title, SCREEN_W - 16, CLR_TEXT_HI, compact ? 1 : 2, CLR_BG);
    String detail;
    if (classes) {
      const auto& c = plan.classes[index]; char when[24];
      snprintf(when, sizeof(when), "%s %02d:%02d", days[c.day], c.start / 60, c.start % 60);
      detail = String(when) + "  " + (c.place[0] ? String(c.place) : countdown(dates[index], now));
    } else detail = plan.events[index].done ? "Completed" :
        String(plan.events[index].kind ? "Reminder  " : "Exam  ") + countdown(dates[index], now);
    drawTruncatedText(8, y + (compact ? 12 : 22), detail.c_str(), SCREEN_W - 16, CLR_TEXT_MED, 1, CLR_BG);
    y += rowH;
  }
  drawPageIndicator(int(currentState), STATE_COUNT);
}
inline bool input(int step) {
  if (currentState != STATE_CALENDAR && currentState != STATE_TIMETABLE) return false;
  int count = currentState == STATE_TIMETABLE ? plan.classCount : plan.eventCount;
  cursor = count ? (cursor + step + count) % count : 0;
  draw(currentState == STATE_TIMETABLE); return true;
}
inline void tick() {
  static uint32_t lastTick = 0;
  uint32_t ms = millis();
  if (uint32_t(ms - lastTick) < 1000) return;
  lastTick = ms;
  uint32_t now = uint32_t(time(nullptr));
  thought_personality::context[0] = 0;
  int upcoming = nextEvent(plan, now);
  if (upcoming >= 0) snprintf(thought_personality::context, sizeof(thought_personality::context),
      "%s: %s. One useful step today.", plan.events[upcoming].title, countdown(plan.events[upcoming].at, now).c_str());
  if ((currentState == STATE_CALENDAR || currentState == STATE_TIMETABLE) &&
      (drawnMinute != now / 60 || drawnRevision != revision)) draw(currentState == STATE_TIMETABLE);
  if (alertVisible && (currentState != STATE_EYES || uint32_t(ms - alertStarted) >= 8000)) {
    alertVisible = false;
    if (currentState == STATE_EYES) renderCurrentPage();
  }
  if (currentState != STATE_EYES || alertVisible || flashNotifActive || agentOverlayActive || now < earliestTime) return;
  int event = nextEvent(plan, now, true); uint32_t at = 0;
  int cls = nextClass(plan, now, at, true);
  if (cls >= 0 && (event < 0 || at < plan.events[event].at)) event = -1;
  if (event < 0 && cls < 0) return;
  uint32_t interval = event >= 0 ? alertInterval(int64_t(plan.events[event].at) - now) : 1800;
  if (lastAlert && uint32_t(ms - lastAlert) < interval * 1000) return;
  if (event >= 0) {
    const auto& e = plan.events[event];
    snprintf(alertText, sizeof(alertText), "%s: %s - %s", e.kind ? "Reminder" : "Exam", e.title, countdown(e.at, now).c_str());
    alertRed = e.kind == 0;
  } else {
    snprintf(alertText, sizeof(alertText), "Class: %s - %s", plan.classes[cls].title, countdown(at, now).c_str());
    alertRed = false;
  }
  alertVisible = true; alertStarted = lastAlert = ms;
}
inline void drawAlert() {
  if (!alertVisible || currentState != STATE_EYES) return;
  const int y = SCREEN_H - UI_FOOTER_H - 28;
  uint16_t bg = alertRed ? 0x7800 : CLR_SURFACE_2;
  tft.fillRect(0, y, SCREEN_W, 28, bg);
  tft.drawFastHLine(0, y, SCREEN_W, alertRed ? TFT_RED : CLR_PRIMARY);
  drawWrappedText(6, y + 3, alertText, SCREEN_W - 12, 24, TFT_WHITE, 1, bg);
}
}
inline bool dismissEyesOverlays() {
  bool visible = companion_planner::dismissAlerts();
  visible = dismissThought() || visible;
  visible = visible || flashNotifActive || agentOverlayActive;
  flashNotifActive = false; agentOverlayActive = false;
  if (visible && currentState == STATE_EYES) renderCurrentPage();
  return visible;
}
inline bool plannerButton(int step) { return companion_planner::input(step); }
#endif
