#ifndef ROTARY_KNOB_H
#define ROTARY_KNOB_H

#ifdef ESP32

#include "globals.h"
#include "config_esp32.h"

// ═══════════════════════════════════════════════════════════
// COMPANION OS — ROTATIONAL POTENTIOMETER KNOB TILE SELECTOR
// Features:
// - Hardware: 10K/50K/100K Analog Potentiometer on ESP32 ADC1
// - Multi-sample oversampling (16x) + Exponential Moving Average
// - Dynamic Auto-Calibration & Persistence (EEPROM)
// - Deadband zone margins for easy reaching of first/last tile
// - Anti-jitter Schmitt hysteresis between tile boundaries
// ═══════════════════════════════════════════════════════════

#ifndef POT_KNOB_PIN
#define POT_KNOB_PIN 36 // GPIO36 (ADC1_CH0 - VP)
#endif

// EEPROM addresses for calibration storage
#define EEPROM_POT_MAGIC_ADDR 450 // 2 bytes: 0x50, 0x4B ('PK')
#define EEPROM_POT_MIN_ADDR   452 // 2 bytes: uint16_t
#define EEPROM_POT_MAX_ADDR   454 // 2 bytes: uint16_t

// Knob state
static uint16_t potMinCalibrated = 100;
static uint16_t potMaxCalibrated = 3950;
static float potFilteredVal = -1.0f;
static int lastKnobPage = -1;
static unsigned long lastKnobMoveTime = 0;
static bool potAutoCalibratedChanged = false;
static unsigned long lastEepromSaveTime = 0;

// Forward declaration
extern void setPage(int targetPage);

inline void loadPotCalibration() {
  EEPROM.begin(EEPROM_SIZE);
  if (EEPROM.read(EEPROM_POT_MAGIC_ADDR) == 0x50 && EEPROM.read(EEPROM_POT_MAGIC_ADDR + 1) == 0x4B) {
    uint16_t savedMin = (EEPROM.read(EEPROM_POT_MIN_ADDR) << 8) | EEPROM.read(EEPROM_POT_MIN_ADDR + 1);
    uint16_t savedMax = (EEPROM.read(EEPROM_POT_MAX_ADDR) << 8) | EEPROM.read(EEPROM_POT_MAX_ADDR + 1);
    if (savedMin < savedMax && savedMax <= 4095) {
      potMinCalibrated = savedMin;
      potMaxCalibrated = savedMax;
      Serial.printf("[KNOB] Loaded calibration from EEPROM: Min=%d, Max=%d\n", potMinCalibrated, potMaxCalibrated);
    }
  }
  EEPROM.end();
}

inline void savePotCalibration() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(EEPROM_POT_MAGIC_ADDR, 0x50);
  EEPROM.write(EEPROM_POT_MAGIC_ADDR + 1, 0x4B);
  EEPROM.write(EEPROM_POT_MIN_ADDR, (potMinCalibrated >> 8) & 0xFF);
  EEPROM.write(EEPROM_POT_MIN_ADDR + 1, potMinCalibrated & 0xFF);
  EEPROM.write(EEPROM_POT_MAX_ADDR, (potMaxCalibrated >> 8) & 0xFF);
  EEPROM.write(EEPROM_POT_MAX_ADDR + 1, potMaxCalibrated & 0xFF);
  EEPROM.commit();
  EEPROM.end();
  Serial.printf("[KNOB] Saved new calibration to EEPROM: Min=%d, Max=%d\n", potMinCalibrated, potMaxCalibrated);
}

inline void initRotaryKnob() {
  pinMode(POT_KNOB_PIN, INPUT);
  analogSetPinAttenuation(POT_KNOB_PIN, ADC_11db); // 0V to ~3.3V range
  loadPotCalibration();
  
  // Prime filter with initial reading
  uint32_t sum = 0;
  for (int i = 0; i < 16; i++) {
    sum += analogRead(POT_KNOB_PIN);
    delayMicroseconds(50);
  }
  potFilteredVal = (float)(sum / 16);
  lastKnobPage = (int)currentState;
}

// 16x oversampled reading with thermal noise filtering
inline uint16_t readPotOversampled() {
  uint32_t sum = 0;
  for (int i = 0; i < 16; i++) {
    sum += analogRead(POT_KNOB_PIN);
    delayMicroseconds(20);
  }
  return (uint16_t)(sum / 16);
}

inline void tickRotaryKnob() {
  #ifndef ENABLE_ROTARY_KNOB
  return;
  #endif

  uint16_t raw = readPotOversampled();

  // 1. Dynamic Auto-Calibration: Automatically adapt to the physical travel of any potentiometer
  if (raw < potMinCalibrated) {
    potMinCalibrated = raw;
    potAutoCalibratedChanged = true;
  }
  if (raw > potMaxCalibrated) {
    potMaxCalibrated = raw;
    potAutoCalibratedChanged = true;
  }

  // Periodic lazy save of auto-calibration (after 10s of quiet time)
  if (potAutoCalibratedChanged && millis() - lastKnobMoveTime > 10000 && millis() - lastEepromSaveTime > 15000) {
    savePotCalibration();
    potAutoCalibratedChanged = false;
    lastEepromSaveTime = millis();
  }

  // 2. Exponential Moving Average (EMA) smoothing: alpha = 0.25
  if (potFilteredVal < 0) potFilteredVal = raw;
  else potFilteredVal = (0.25f * raw) + (0.75f * potFilteredVal);

  // 3. Margin Deadband Normalization (3% margin at ends to guarantee hitting page 0 and maxPage)
  float effectiveMin = (float)potMinCalibrated + ((float)(potMaxCalibrated - potMinCalibrated) * 0.03f);
  float effectiveMax = (float)potMaxCalibrated - ((float)(potMaxCalibrated - potMinCalibrated) * 0.03f);
  if (effectiveMax <= effectiveMin) effectiveMax = effectiveMin + 1.0f;

  float norm = (potFilteredVal - effectiveMin) / (effectiveMax - effectiveMin);
  norm = constrain(norm, 0.0f, 1.0f);

  // 4. Calculate target page index with Schmitt Hysteresis
  int totalPages = STATE_COUNT;
  #ifndef ESP32
  totalPages = STATE_DR_HACK;
  #endif

  // Bin width in normalized space
  float binWidth = 1.0f / (float)totalPages;
  float hysteresis = binWidth * 0.18f; // 18% bin hysteresis margin to prevent boundary flutter

  int candidatePage = (int)(norm * totalPages);
  if (candidatePage >= totalPages) candidatePage = totalPages - 1;
  if (candidatePage < 0) candidatePage = 0;

  // Apply hysteresis against current page
  if (lastKnobPage >= 0 && candidatePage != lastKnobPage) {
    float lowerBoundary = (float)candidatePage * binWidth;
    float upperBoundary = lowerBoundary + binWidth;

    bool validTransition = false;
    if (candidatePage > lastKnobPage && norm > (lowerBoundary + hysteresis)) {
      validTransition = true;
    } else if (candidatePage < lastKnobPage && norm < (upperBoundary - hysteresis)) {
      validTransition = true;
    }

    if (validTransition) {
      lastKnobPage = candidatePage;
      lastKnobMoveTime = millis();
    }
  } else if (lastKnobPage < 0) {
    lastKnobPage = candidatePage;
  }

  // 5. Debounced Page Switch: Trigger page change once knob settles for 80ms
  if (lastKnobPage != (int)currentState && millis() - lastKnobMoveTime >= 80) {
    Serial.printf("[KNOB] Switching to Tile Index: %d (Norm=%.2f, Raw=%d)\n", lastKnobPage, norm, raw);
    setPage(lastKnobPage);
  }
}

// Interactive 3-second Self-Calibration routine
inline void runPotKnobSelfCalibration() {
  Serial.println(F("[KNOB] Starting 3-second Potentiometer Calibration... Turn knob fully Left and Right!"));
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(TFT_CYAN, COLOR_BG);
  tft.drawCentreString("KNOB CALIBRATION", SCREEN_W / 2, 20, 2);
  tft.setTextColor(TFT_WHITE, COLOR_BG);
  tft.drawCentreString("Turn knob full LEFT", SCREEN_W / 2, 45, 1);
  tft.drawCentreString("then full RIGHT...", SCREEN_W / 2, 60, 1);

  uint16_t curMin = 4095;
  uint16_t curMax = 0;
  unsigned long start = millis();

  while (millis() - start < 3500) {
    uint16_t raw = readPotOversampled();
    if (raw < curMin) curMin = raw;
    if (raw > curMax) curMax = raw;

    int progress = map(millis() - start, 0, 3500, 0, 120);
    tft.fillRect(20, 85, 120, 6, 0x2104);
    tft.fillRect(20, 85, progress, 6, 0x07E0);
    delay(20);
  }

  if (curMax > curMin + 200) {
    potMinCalibrated = curMin;
    potMaxCalibrated = curMax;
    savePotCalibration();
    tft.setTextColor(TFT_GREEN, COLOR_BG);
    tft.drawCentreString("CALIBRATION OK!", SCREEN_W / 2, 100, 1);
  } else {
    tft.setTextColor(TFT_RED, COLOR_BG);
    tft.drawCentreString("FAILED: CHECK WIRING", SCREEN_W / 2, 100, 1);
  }
  delay(1200);
  renderCurrentPage();
}

#endif // ESP32
#endif // ROTARY_KNOB_H
