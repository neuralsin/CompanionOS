# CompanionOS v7.0 — Function Audit & Verification Checklist

## 1. Global & State Management (`globals.h`, `CompanionOS_Main.ino`)
- [ ] `setup()`: Verifies SPI initialization, TFT rotation, GPIO13/14/27 as `INPUT_PULLUP`
- [ ] `loop()`: Non-blocking 50ms frame timer routes to correct theme eye renderer
- [ ] `SCALE_X(v)` / `SCALE_Y(v)`: Integer math does not truncate to 0 for small values
- [ ] `THEME_COUNT`: Confirmed 3, used in all modular `% THEME_COUNT` cycles
- [ ] `activeTheme` sanitize: `>= THEME_COUNT` catches invalid EEPROM on first boot

## 2. Hardware Interfaces (`buttons.h`, `touch.h`)
- [ ] `handleButtons()`: 3-tier press logic routes to T1/T2/T3 expression cycling
- [ ] `handleTouch()`: Spotify touch zones fall through correctly for Theme 3
- [ ] Settings touch toggle: Uses `(activeTheme + 1) % THEME_COUNT` modular cycle

## 3. Eye Engine — Theme 1 (`eyes.h`)
- [ ] `drawAlmondEye()`: 5-tier gradient blending, no SPI overflow
- [ ] `drawStarfield()`: XOR-shift procedural generator, bounded to eye zone
- [ ] `updateShootingStar()`: Non-blocking state machine
- [ ] `drawHeartbeatPulse()`: `sin()` wave via `blendColor()` without blocking

## 4. Eye Engine — Theme 2 (`theme2_eyes.h`)
- [ ] `T2EyeConfig` 12-field struct: 18 expression presets
- [ ] `T2EyeTransition/Variation/Blink`: Pipeline executes without flicker
- [ ] Double-buffered sprite rendering

## 5. Eye Engine — Theme 3 (`theme3_eyes.h`) ★ NEW
- [ ] `T3EyeVariantConfig` struct: 6 variants (Classic/Wide/Tall/Round/Cyclops/Pixel)
- [ ] `t3_applyVariant()`: Correctly resets all geometry and animation state
- [ ] `t3_drawEyesInternal()`: Partial redraw (eye zone only, y=16 to bottom-16)
- [ ] `t3_updateEyes()`: Frame-rate limited via `t3_fpsTimer`, no `delay()`
- [ ] `t3_setEmotion()`: Maps all CompanionOS emotions to RoboEyes moods
- [ ] `t3_nextExpression()`: Cycles through 6 variants via SELECT button
- [ ] Auto-blink: Random 3-6s interval, non-blocking timer
- [ ] Idle mode: Random position targeting, bounded by screen constraints
- [ ] Curious mode: Outer eye height increases at screen edges
- [ ] Cyclops mode: Right eye zeroed, spacing zeroed
- [ ] Tired eyelids: Triangle overlay from top-left (droopy)
- [ ] Angry eyelids: Triangle overlay from top-right (V-shaped)
- [ ] Happy eyelids: Bottom clip with rounded rect
- [ ] Confused animation: Horizontal flicker for 500ms
- [ ] Laugh animation: Vertical flicker for 500ms
- [ ] Variant label: Displays name in bottom-left of eye zone
- [ ] No `display->clearDisplay()` calls (OLED-only, not used)
- [ ] No `display->display()` calls (OLED-only, not used)
- [ ] TFT_eSPI compatible: Uses `tft.fillRoundRect()` and `tft.fillTriangle()`
- [ ] Eye zone bounded: Does not overwrite status bar (y=0–15) or page dots

## 6. UI Components (`ui_components.h`, `ui.h`)
- [ ] `blendColor()`: RGB565 bit-shifting accurate
- [ ] `drawStatusBar()`: WiFi/BT/time/notifications positioned correctly
- [ ] `drawPageIndicator()`: Dynamic centering for 13 states

## 7. Network & Comms (`network.h`)
- [ ] `handleCommand()` THEME: handler accepts 0, 1, 2
- [ ] `handleCommand()` SELECT: routes to t3_nextExpression for theme 2
- [ ] `handleCommand()` EMOTION: routes to t3_setEmotion for theme 2
- [ ] Theme toggle in settings: `(activeTheme + 1) % THEME_COUNT`
- [ ] Spotify partial redraws: Theme 3 falls through to Theme 1 (correct)

## 8. Settings Page (`pages.h`)
- [ ] 3-way theme selector: OG | T2 | RB segments
- [ ] Track background color: 0x2104 (T1), 0x4810 (T2), 0x1848 (T3)
- [ ] Theme label: "Original" / "Alternate" / "RoboEyes"
- [ ] Thumb position: `sliderX + 2 + (activeTheme * segW)`

## 9. Dr. Hack Suite (`page_dr_hack.h`)
- [ ] `dhDrawMenu()`: 2×4 grid fits 160×128
- [ ] `dhRunPortScanner()`: 300ms timeout prevents WDT reset
- [ ] `dhRunDeauth()` / `dhRunBeaconSpam()`: Raw 802.11 frame construction
- [ ] Navigation: Works with all 3 themes (no theme dependency)

## 10. Dashboards
- [ ] `drawGamingPage()`: Steam-inspired layout, no theme dependency
- [ ] `page_stocks.h`: Sparkline charts, no theme dependency
- [ ] `page_social.h`: Social card layout, no theme dependency
- [ ] `page_productivity.h`: Timeline bar, no theme dependency
- [ ] `page_network.h`: RSSI arcs, no theme dependency

## 11. Thought Engine (`thought_engine.h`)
- [ ] `updateThought()`: Fade-in/out state machine
- [ ] `pushThought()`: Text truncation for bubble bounds
- [ ] Works with all 3 themes (renders on eyes page regardless of theme)

## 12. Python Bridge
- [ ] `companion_controller.py`: `THEME:2` command accepted and forwarded
- [ ] `steam_tracker.py`: `psutil` in requirements.txt
- [ ] Twitch credentials injected into `os.environ`

---

# 🚨 HALF-BAKED / MISSING FEATURES (V7 Prompt Delta)

### 🔴 Critical
- [x] **psutil dependency** — Added to requirements.txt ✓
- [x] **Twitch env injection** — Added to companion_controller.py ✓
- [x] **Theme 3 (RoboEyes)** — Fully implemented ✓

### 🟡 Medium / Incomplete
- [ ] **Steam Tracker Tier 3**: Xbox Registry Game Bar scan missing
- [ ] **Game Cover PIL Fallback**: Colored title card generation missing
- [ ] **Gaming Dominant Color**: No 4×4 center sampling for bg tint
- [ ] **GameSession OOP Class**: Using dict instead of class

### ⚪ Entirely Missing
- [ ] **Slideshow/Gallery Redesign (Section 6)**: Cinematic frame, transitions, Ken Burns
- [ ] **BLE Phone Control (Section 8)**: GATT server unwritten
- [ ] **BT Discovery QR Mode (Section 8)**: QR code on TFT missing
- [ ] **Weather Animations (Section 9.5)**: Rain pixels, drifting clouds, forecast strip
- [ ] **Pomodoro Enhancement (Section 9.6)**: Center digits, session dots, color toggles
- [ ] **Advanced UI Components (Section 9.2)**: `drawGradientCard`, `drawProgressPill` stubbed
- [ ] **Boot Wipe Transition (Section 9.4)**: Phase 4 wipe animation missing
- [ ] **`bluetooth_bridge.py`**: Python BT COM port auto-detect module missing
