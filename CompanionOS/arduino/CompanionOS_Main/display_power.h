#ifndef COMPANION_DISPLAY_POWER_H
#define COMPANION_DISPLAY_POWER_H
#include "startup_policy.h"
namespace display_power {
static bool pwm = false, visible = false;
static uint32_t started = 0;
static int lastDuty = -1;
#if defined(ESP32) && defined(TFT_BL) && TFT_BL >= 0
#if defined(TFT_BACKLIGHT_ON) && TFT_BACKLIGHT_ON == LOW
constexpr bool activeHigh = false;
#else
constexpr bool activeHigh = true;
#endif
inline void off() { pinMode(TFT_BL, OUTPUT); digitalWrite(TFT_BL, activeHigh ? LOW : HIGH); }
inline void attach() {
  // TFT init may call pinMode/digitalWrite and detach PWM on core 3.x.
  off();
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  pwm = ledcAttach(TFT_BL, 20000, 8);
#else
  pwm = ledcSetup(0, 20000, 8) > 0;
  if (pwm) ledcAttachPin(TFT_BL, 0);
#endif
}
inline void write(int brightness) {
  int duty = activeHigh ? brightness : 255 - brightness;
  if (duty == lastDuty) return;
  lastDuty = duty;
  if (pwm) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(TFT_BL, duty);
#else
    ledcWrite(0, duty);
#endif
  } else digitalWrite(TFT_BL, brightness ? (activeHigh ? HIGH : LOW) : (activeHigh ? LOW : HIGH));
}
#else
inline void off() {} inline void attach() {} inline void write(int) {}
#endif
inline void show() { started = millis(); visible = true; write(0); }
inline void tick() {
  if (visible) write(int(180 * companion_startup::ease(float(uint32_t(millis() - started)) / 900)));
}
}
#endif
