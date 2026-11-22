#ifndef COMPANION_PLANNER_STORE_H
#define COMPANION_PLANNER_STORE_H
#include "planner_core.h"
#include "thought_personality.h"
#include <time.h>
#include <memory>
#ifdef ESP32
#include <Preferences.h>
#include <sys/time.h>
#endif
namespace companion_planner {
static Plan plan;
static uint32_t revision = 0;
static bool& alertVisible = overlayActive;
static uint32_t alertStarted = 0, lastAlert = 0;
static char alertText[80] = {};
static bool alertRed = false;
inline bool persist(const Plan& value) {
#ifdef ESP32
  Preferences prefs;
  if (!prefs.begin("planner", false)) return false;
  bool ok = prefs.putBytes("v1", &value, sizeof(value)) == sizeof(value);
  prefs.end();
  return ok;
#else
  return false;
#endif
}
inline void init() {
#ifdef ESP32
  Preferences prefs;
  if (prefs.begin("planner", true)) {
    if (prefs.getBytesLength("v1") == sizeof(plan)) prefs.getBytes("v1", &plan, sizeof(plan));
    prefs.end();
  }
#endif
  if (!valid(plan)) plan = Plan();
  revision = plan.revision;
  thought_personality::flirtLevel = plan.flirt;
}
inline bool dismissAlerts() {
  const bool wasVisible = alertVisible;
  alertVisible = false;
  uint32_t now = uint32_t(time(nullptr));
  if (!wasVisible || now < earliestTime) return wasVisible;
  // Snooze all currently eligible events together; another due item must not
  // immediately replace the one the user just dismissed.
  for (int i = 0; i < plan.eventCount; ++i)
    if (int64_t(plan.events[i].at) - now <= 259200) plan.events[i].quietUntil = now + 3600;
  uint32_t at = 0;
  int c = nextClass(plan, now, at, true);
  if (c >= 0) plan.classes[c].quietUntil = at + plan.classes[c].duration * 60;
  plan.revision = ++revision;
  if (!persist(plan)) Serial.println("[planner] Snooze active until reboot; flash save failed");
  return wasVisible;
}
inline void writeJson(JsonDocument& doc) {
  doc["version"] = 1; doc["revision"] = revision; doc["offset"] = plan.offset;
  doc["flirt"] = plan.flirt; doc["epoch"] = uint32_t(time(nullptr));
  doc["device"] = WiFi.macAddress();
  JsonArray events = doc.createNestedArray("events");
  for (int i = 0; i < plan.eventCount; ++i) {
    auto& e = plan.events[i]; JsonObject v = events.createNestedObject();
    v["id"] = e.id; v["title"] = e.title; v["at"] = e.at;
    v["kind"] = e.kind; v["done"] = bool(e.done); v["quietUntil"] = e.quietUntil;
  }
  JsonArray classes = doc.createNestedArray("classes");
  for (int i = 0; i < plan.classCount; ++i) {
    auto& c = plan.classes[i]; JsonObject v = classes.createNestedObject();
    v["id"] = c.id; v["title"] = c.title; v["place"] = c.place; v["day"] = c.day;
    v["start"] = c.start; v["duration"] = c.duration; v["quietUntil"] = c.quietUntil;
  }
}
inline bool readJson(JsonDocument& doc, Plan& out) {
  if (doc["version"].as<int>() != 1 || !doc["events"].is<JsonArray>() || !doc["classes"].is<JsonArray>()) return false;
  int offset = doc["offset"] | 330, flirt = doc["flirt"] | 1;
  if (offset < -720 || offset > 840 || flirt < 0 || flirt > 2) return false;
  out.offset = offset; out.flirt = flirt;
  for (JsonObject v : doc["events"].as<JsonArray>()) {
    if (out.eventCount >= eventLimit || !v["id"].is<uint32_t>() || !v["at"].is<uint32_t>() ||
        !v["title"].is<const char*>() || strlen(v["title"]) > 40 ||
        !v["kind"].is<int>() || v["kind"].as<int>() < 0 || v["kind"].as<int>() > 1) return false;
    auto& e = out.events[out.eventCount++];
    e.id = v["id"]; e.at = v["at"]; e.quietUntil = v["quietUntil"] | 0UL;
    e.kind = v["kind"]; e.done = v["done"] | false;
    strcpy(e.title, v["title"]);
  }
  for (JsonObject v : doc["classes"].as<JsonArray>()) {
    if (out.classCount >= classLimit || !v["id"].is<uint32_t>() || !v["title"].is<const char*>() ||
        strlen(v["title"]) > 40 || !v["place"].is<const char*>() || strlen(v["place"]) > 24) return false;
    int day = v["day"] | -1, start = v["start"] | -1, duration = v["duration"] | 0;
    if (day < 0 || day > 6 || start < 0 || start >= 1440 || duration < 1 || duration > 720) return false;
    auto& c = out.classes[out.classCount++];
    c.id = v["id"]; c.quietUntil = v["quietUntil"] | 0UL;
    c.day = day; c.start = start; c.duration = duration;
    strcpy(c.title, v["title"]); strcpy(c.place, v["place"]);
  }
  return valid(out);
}
inline void registerRoutes() {
  sysWebServer.on("/api/planner", HTTP_GET, []() {
    if (companion_memory::networkBusy.load(std::memory_order_relaxed)) {
      sysWebServer.send(503, "text/plain", "Busy; retry shortly"); return;
    }
    DynamicJsonDocument doc(8192);
    writeJson(doc);
    if (doc.overflowed()) { sysWebServer.send(503, "text/plain", "Planner RAM busy"); return; }
    String body; serializeJson(doc, body);
    sysWebServer.sendHeader("Cache-Control", "no-store");
    sysWebServer.send(200, "application/json", body);
  });
  sysWebServer.on("/api/planner", HTTP_POST, []() {
    if (companion_memory::networkBusy.load(std::memory_order_relaxed)) {
      sysWebServer.send(503, "text/plain", "Busy; retry after playback sync"); return;
    }
    String body = sysWebServer.arg("plain");
    if (body.length() > 6000) { sysWebServer.send(413, "text/plain", "Planner too large"); return; }
    DynamicJsonDocument doc(8192);
    std::unique_ptr<Plan> staged(new (std::nothrow) Plan());
    if (!staged) { sysWebServer.send(503, "text/plain", "Planner RAM busy"); return; }
    if (deserializeJson(doc, body) || !readJson(doc, *staged)) {
      sysWebServer.send(400, "text/plain", "Invalid schedule; use printable titles and valid dates"); return;
    }
    if (!doc["revision"].is<uint32_t>() || doc["revision"].as<uint32_t>() != revision ||
        doc["device"].as<String>() != WiFi.macAddress()) {
      sysWebServer.send(409, "text/plain", "Bot schedule changed; reload before saving"); return;
    }
    staged->revision = revision + 1;
    if (!persist(*staged)) { sysWebServer.send(500, "text/plain", "Schedule could not be saved"); return; }
    plan = *staged; ++revision; alertVisible = false;
    thought_personality::flirtLevel = plan.flirt;
#ifdef ESP32
    uint32_t epoch = doc["epoch"] | 0UL;
    if (epoch >= earliestTime && epoch < 4102444800UL) {
      timeval tv = {time_t(epoch), 0}; settimeofday(&tv, nullptr);
    }
#endif
    doc.clear(); writeJson(doc);
    body = ""; serializeJson(doc, body);
    sysWebServer.send(200, "application/json", body);
  });
}
}
#endif
