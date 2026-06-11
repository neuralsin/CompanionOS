# CompanionOS — Lint Errors & Code Issues Log

> Manually identified during full codebase audit on 2026-06-11.  
> Severity: 🔴 Critical | 🟠 Major | 🟡 Minor | ⚪ Info

---

## 🔴 Critical Errors

### ERR-001: `ieee80211_raw_frame_sanity_check` commented out
- **File**: [page_dr_hack.h:422-425](file:///c:/Users/shaan/OneDrive/Documents/C++/esp/CompanionOS/arduino/CompanionOS_Main/page_dr_hack.h#L422-L425)
- **Impact**: ALL raw 802.11 TX tools silently fail (Deauth, Beacon Spam, KARMA)
- **Comment says**: "ESP32 Core 3.x defines this internally" — this is **incorrect**
- **Fix**: Uncomment the `extern "C"` function override

### ERR-002: WiFi init sequence wrong for raw frame TX
- **Files**: 
  - [page_dr_hack.h:491-496](file:///c:/Users/shaan/OneDrive/Documents/C++/esp/CompanionOS/arduino/CompanionOS_Main/page_dr_hack.h#L491-L496) (Beacon Spam)
  - [page_dr_hack.h:631-636](file:///c:/Users/shaan/OneDrive/Documents/C++/esp/CompanionOS/arduino/CompanionOS_Main/page_dr_hack.h#L631-L636) (Deauth)
- **Impact**: `esp_wifi_80211_tx()` may silently fail even with ERR-001 fixed
- **Fix**: Use full low-level init: `WIFI_MODE_NULL → esp_wifi_init → set_mode(STA) → start → set_promiscuous`

### ERR-003: Missing WiFi cleanup after tool exit
- **Files**: 
  - [page_dr_hack.h:565](file:///c:/Users/shaan/OneDrive/Documents/C++/esp/CompanionOS/arduino/CompanionOS_Main/page_dr_hack.h#L565) (Beacon Spam)
  - [page_dr_hack.h:694](file:///c:/Users/shaan/OneDrive/Documents/C++/esp/CompanionOS/arduino/CompanionOS_Main/page_dr_hack.h#L694) (Deauth)
- **Impact**: WiFi left in broken promiscuous state; CompanionOS WiFi never reconnects
- **Fix**: Add `esp_wifi_stop(); esp_wifi_deinit();` after `esp_wifi_set_promiscuous(false)`

---

## 🟠 Major Errors

### ERR-004: Menu page/tool count mismatch
- **File**: [globals.h:103-105](file:///c:/Users/shaan/OneDrive/Documents/C++/esp/CompanionOS/arduino/CompanionOS_Main/globals.h#L103-L105)
- **Impact**: `DH_MENU_PAGES=2`, `DH_TOTAL_TOOLS=16` but `DrHackSubState` enum (L107-127) defines 48 states across 6 pages. All IR, CC1101, Radio, and System tools are **completely unreachable**.
- **Fix**: Set `DH_MENU_PAGES=6`, `DH_TOTAL_TOOLS=48`; add corresponding tool names/colors/switch cases

### ERR-005: Tool routing mismatch — "NRF Test" → HW Diagnostics
- **File**: [page_dr_hack.h:49](file:///c:/Users/shaan/OneDrive/Documents/C++/esp/CompanionOS/arduino/CompanionOS_Main/page_dr_hack.h#L49) vs [page_dr_hack.h:1168](file:///c:/Users/shaan/OneDrive/Documents/C++/esp/CompanionOS/arduino/CompanionOS_Main/page_dr_hack.h#L1168)
- **Impact**: Menu says "NRF Test" but runs `dhRunHwDiag()` instead
- **Fix**: Route to correct function or rename menu label

### ERR-006: `drawDrHackTile()` not called on Theme 2 and Theme 3
- **File**: [CompanionOS_Main.ino:305-323](file:///c:/Users/shaan/OneDrive/Documents/C++/esp/CompanionOS/arduino/CompanionOS_Main/CompanionOS_Main.ino#L305-L323)
- **Impact**: Dr.Hack entry tile ("H") is invisible when theme2 or theme3 is active
- **Fix**: Add `drawDrHackTile()` calls within the `activeTheme == 1` and `activeTheme == 2` blocks

### ERR-007: `dhWaitSelectPress()` doesn't check virtual button flag
- **File**: [ui_components.h:15-22](file:///c:/Users/shaan/OneDrive/Documents/C++/esp/CompanionOS/arduino/CompanionOS_Main/ui_components.h#L15-L22)
- **Impact**: When using web remote to control dr.hack, virtual button presses don't work for any tool that uses `dhWaitSelectPress()`. Only physical button works.
- **Fix**: Add `virtualSelectPressed` check to the wait loop

---

## 🟡 Minor Errors

### ERR-008: Packet Monitor has mixed button handling
- **File**: [page_dr_hack.h:799-806](file:///c:/Users/shaan/OneDrive/Documents/C++/esp/CompanionOS/arduino/CompanionOS_Main/page_dr_hack.h#L799-L806)
- **Impact**: LEFT button checks `virtualLeftPressed` flag; RIGHT button on line 803 does NOT. Channel change via web remote only works in one direction.
- **Fix**: Add `virtualRightPressed` check to the RIGHT button condition

### ERR-009: Channel Scan uses bare `digitalRead` without virtual button checks
- **File**: [dh_wifi_tools.h:153](file:///c:/Users/shaan/OneDrive/Documents/C++/esp/CompanionOS/arduino/CompanionOS_Main/dh_wifi_tools.h#L153)
- **Impact**: RIGHT navigation in Channel Scan doesn't respond to web remote
- **Fix**: Add `(virtualRightPressed ? (virtualRightPressed=false, true) : false)` check

### ERR-010: WiFi Radar RIGHT button missing virtual button check
- **File**: [dh_wifi_tools.h:381](file:///c:/Users/shaan/OneDrive/Documents/C++/esp/CompanionOS/arduino/CompanionOS_Main/dh_wifi_tools.h#L381)
- **Impact**: Same as ERR-009 but for WiFi Radar tool
- **Fix**: Add virtual button check

### ERR-011: Probe Sniffer RIGHT button missing virtual button check
- **File**: [dh_wifi_tools.h:684](file:///c:/Users/shaan/OneDrive/Documents/C++/esp/CompanionOS/arduino/CompanionOS_Main/dh_wifi_tools.h#L684)
- **Impact**: Same as ERR-009 but for Probe Sniffer
- **Fix**: Add virtual button check

### ERR-012: BLE Inspector RIGHT button missing virtual button check
- **File**: [dh_ble_tools.h:238](file:///c:/Users/shaan/OneDrive/Documents/C++/esp/CompanionOS/arduino/CompanionOS_Main/dh_ble_tools.h#L238)
- **Impact**: BLE Inspector RIGHT navigation only works with physical button
- **Fix**: Add virtual button check

### ERR-013: BLE Spam RIGHT button missing virtual button check
- **File**: [dh_ble_tools.h:411](file:///c:/Users/shaan/OneDrive/Documents/C++/esp/CompanionOS/arduino/CompanionOS_Main/dh_ble_tools.h#L411)
- **Fix**: Add virtual button check

### ERR-014: BT Disruptor RIGHT button missing virtual button check
- **File**: [dh_ble_tools.h:650](file:///c:/Users/shaan/OneDrive/Documents/C++/esp/CompanionOS/arduino/CompanionOS_Main/dh_ble_tools.h#L650)
- **Fix**: Add virtual button check

### ERR-015: BT Disruptor attack mode RIGHT button missing virtual check
- **File**: [dh_ble_tools.h:696](file:///c:/Users/shaan/OneDrive/Documents/C++/esp/CompanionOS/arduino/CompanionOS_Main/dh_ble_tools.h#L696)
- **Fix**: Add virtual button check

### ERR-016: IR Remote RIGHT button missing virtual button check
- **File**: [dh_ir_tools.h:640](file:///c:/Users/shaan/OneDrive/Documents/C++/esp/CompanionOS/arduino/CompanionOS_Main/dh_ir_tools.h#L640)
- **Fix**: Add virtual button check

### ERR-017: IR Saved RIGHT button missing virtual button check
- **File**: [dh_ir_tools.h:728](file:///c:/Users/shaan/OneDrive/Documents/C++/esp/CompanionOS/arduino/CompanionOS_Main/dh_ir_tools.h#L728)
- **Fix**: Add virtual button check

### ERR-018: Radio Scanner LEFT/RIGHT merged check
- **File**: [dh_radio_tools.h:316](file:///c:/Users/shaan/OneDrive/Documents/C++/esp/CompanionOS/arduino/CompanionOS_Main/dh_radio_tools.h#L316)
- **Impact**: Uses `||` to combine LEFT and RIGHT into one toggle — RIGHT has no virtual button check
- **Fix**: Add virtual button check for RIGHT

### ERR-019: Jammer RIGHT button missing virtual button check
- **File**: [dh_radio_tools.h:129](file:///c:/Users/shaan/OneDrive/Documents/C++/esp/CompanionOS/arduino/CompanionOS_Main/dh_radio_tools.h#L129)
- **Fix**: Add virtual button check

---

## ⚪ Info / Style Issues

### INF-001: `EvilPortalHTML.h` is empty (0 bytes)
- **File**: [EvilPortalHTML.h](file:///c:/Users/shaan/OneDrive/Documents/C++/esp/CompanionOS/arduino/CompanionOS_Main/EvilPortalHTML.h)
- **Impact**: None — HTML is defined inline in `dh_evil_portal.h`
- **Note**: Dead file, can be removed

### INF-002: `network.h.bak` backup file in source tree
- **File**: [network.h.bak](file:///c:/Users/shaan/OneDrive/Documents/C++/esp/CompanionOS/arduino/CompanionOS_Main/network.h.bak)
- **Impact**: None — backup file, not included
- **Note**: Can be removed for cleanliness

### INF-003: `exoticMode` is dead code
- **File**: [globals.h:302](file:///c:/Users/shaan/OneDrive/Documents/C++/esp/CompanionOS/arduino/CompanionOS_Main/globals.h#L302)
- **Impact**: None — defined as `false` constant, all conditionals evaluate false
- **Note**: Already documented in code comment

### INF-004: `dhAbout()` hardcodes "Tools: 48" but only 16 are accessible
- **File**: [page_dr_hack.h:1091](file:///c:/Users/shaan/OneDrive/Documents/C++/esp/CompanionOS/arduino/CompanionOS_Main/page_dr_hack.h#L1091)
- **Impact**: UI shows incorrect tool count
- **Fix**: Will be correct after ARCH-01 is fixed

### INF-005: Port scan target uses `extern String pcIPStr` which may be uninitialized
- **File**: [page_dr_hack.h:376](file:///c:/Users/shaan/OneDrive/Documents/C++/esp/CompanionOS/arduino/CompanionOS_Main/page_dr_hack.h#L376)
- **Impact**: Port scan targets empty string if PC IP was never received
- **Fix**: Fall back to gateway IP if pcIPStr is empty

### INF-006: Multiple `extern void handleButtons(); handleButtons();` in tool loops
- **Files**: Multiple locations in page_dr_hack.h, dh_wifi_tools.h
- **Impact**: None — valid C++ but verbose pattern
- **Note**: Style issue only, works correctly
