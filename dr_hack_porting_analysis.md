# Comprehensive Analysis & Comparative Audit: CompanionOS Dr. Hack vs. Original ESP32-TOOLS-PRO V2.0

---

## 1. Executive Summary

This document presents an exhaustive architectural dissection and comparative analysis between **CompanionOS Dr. Hack** (`CompanionOS/arduino/CompanionOS_Main/page_dr_hack.h` and associated `dh_*.h` modules) and the authoritative original project **ESP32-TOOLS-PRO V2.0** (`hack update files/`).

The investigation confirms that the current Dr. Hack implementation in CompanionOS is a **severely broken, incomplete, and fragmented port** of the original firmware. Critical vulnerabilities, logic inversions, missing modules, 0-byte ghost files, SPI bus collisions, and resolution scaling failures prevent the majority of tools from functioning correctly or cause system panics and lockups.

```
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                                   PORT HEALTH SUMMARY                                  │
├──────────────────────────────────────┬─────────────────────────────────────────────────┤
│ Total Tools in Original Base         │ 48 Sub-Tools across 5 Core Categories           │
│ Tools Implemented in CompanionOS     │ 48 Registered in Menu, only ~12 Partially Work │
│ Critical Hardware / Logic Bugs       │ 14 Major System-Level Bugs                      │
│ Completely Missing / Truncated Logic │ 32 Tools Missing Core Workflows                 │
│ 0-Byte / Ghost Source Files          │ 1 File (`EvilPortalHTML.h` is 0 bytes)          │
│ UI Resolution Mismatches             │ 480×320 / 320×240 coordinates clipped on 160×128│
└──────────────────────────────────────┴─────────────────────────────────────────────────┘
```

---

## 2. Core Architectural & System Differences

| Architectural Dimension | Original (`hack update files/`) | CompanionOS Port (`CompanionOS/`) |
| :--- | :--- | :--- |
| **Framework & Structure** | Modular PlatformIO C++ project with 45 `.cpp`/`.h` files, discrete classes, and namespace isolation | Monolithic Arduino header files (`dh_*.h`) included directly into `pages.h` with global namespace pollution |
| **Display & Target Resolution** | 3.5"–4.0" ILI9488 SPI TFT (480×320 / 320×240 viewport) with custom font rendering engine | 1.8" ST7735R SPI TFT (160×128 landscape) |
| **Input Navigation Model** | 3-button hierarchical menu system (Up, Down, OK / OK Long-press) with cursor memory and smooth list scroll | 2×4 icon tile grid (6 pages, 8 items/page) navigated via Left, Right, Select |
| **Data Persistence** | ESP32 `Preferences` (NVS) under dedicated namespaces (`esp32tools`, `irsignals`, `irremotes`, `wificonfig`) | Basic EEPROM for OS theme settings; IR captures partially ported to SPIFFS; Wi-Fi & Remotes missing persistence |
| **Audio Feedback** | Active tone buzzer driver (`SoundUtils.cpp` with `ledcWriteTone`) for tactile click feedback and alarm audio | No buzzer integration; sound calls completely omitted |
| **Text Entry System** | Full on-screen QWERTY & Symbol Virtual Keyboard (`VirtualKeyboard.cpp`) | Completely absent; hardcoded strings used everywhere |
| **OS Multi-tasking & Promiscuous Mode** | Standalone firmware dedicated to RF/Wi-Fi tools; full control over Wi-Fi hardware state | Multi-app OS with background UDP communication, Companion Controller telemetry, and FreeRTOS tasks |

---

## 3. Critical Hardware & Pin Mapping Contention

### 3.1. The Button Active-Logic Inversion Bug (Catastrophic)
In `config_esp32.h`, CompanionOS defines:
```cpp
#define BTN_ACTIVE_LEVEL HIGH // Configured for active-HIGH capacitive touch (TTP223)
```
However, throughout `dh_ble_tools.h`, `dh_cc1101_tools.h`, and `dh_ir_tools.h`, the port hardcodes:
```cpp
if (digitalRead(BTN_LEFT) == LOW) { ... }
if (digitalRead(BTN_RIGHT) == LOW) { ... }
if (digitalRead(BTN_SELECT) == LOW) { ... }
```
**Impact**: Because unpressed touch pins read `LOW`, these tools enter an infinite loop of false button triggers immediately upon opening. Conversely, on hardware with active-low mechanical buttons, tools expecting `BTN_ACTIVE_LEVEL` fail to register input.

### 3.2. Shared SPI Bus (VSPI) Contention & CS Pin Thrashing
The SPI bus (MOSI: 23, MISO: 19, SCLK: 18) is shared between four separate SPI peripherals:
1. **ST7735 TFT Display** (`TFT_CS: 5`, `TFT_DC: 2`, `TFT_RST: 4`)
2. **nRF24L01 Module #1** (`NRF1_CE: 32`, `NRF1_CSN: 33`)
3. **nRF24L01 Module #2** (`NRF2_CE: 17`, `NRF2_CSN: 16`)
4. **CC1101 Sub-GHz Radio** (`CC1101_CSN: 21`, `GDO0: 35`, `TX_DATA: 25`)

**Original Handling (`Pins.h` / `Main.cpp`)**:
The original codebase sets all CSN pins `HIGH` at boot and strictly wraps every radio transaction in `SPI.beginTransaction(SPISettings(...))` and asserts only the target CS pin `LOW`.

**CompanionOS Port Bug**:
- In `dh_radio_tools.h` and `dh_cc1101_tools.h`, subroutines often begin radio communication while leaving other CS pins floating or without deselecting the TFT (`TFT_CS`).
- Calling `tft.drawPixel` or `tft.drawString` inside tight RF scanning loops corrupts radio SPI register transfers, causing the CC1101 chip to return `0x00` or `0xFF` for part number and version.

### 3.3. Input-Only GPIO Hazards
- **GPIO 34 (`IR_RX_PIN`)** and **GPIO 35 (`CC1101_GDO0_PIN`)** on the ESP32 are input-only pins (GPI). They lack internal software pull-up and pull-down resistors (`INPUT_PULLUP` is invalid in hardware).
- If the external M5Stack IR Unit or CC1101 module lacks a hardware pullup, the pin floats, producing continuous spurious interrupts and false edge triggers in raw capture tools.

---

## 4. Module-by-Module Comparative Dissection

```
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                              MODULE-BY-MODULE AUDIT MATRIX                             │
├──────────────────────┬──────────────────────┬──────────────────────┬───────────────────┤
│ Module Category      │ Original Source File │ CompanionOS Header   │ Status / Verdict  │
├──────────────────────┼──────────────────────┼──────────────────────┼───────────────────┤
│ Wi-Fi Scanner        │ WifiScanner.cpp      │ page_dr_hack.h       │ ⚠️ Truncated Logic│
│ Channel Scanner      │ WifiChannelScanner.cpp│ dh_wifi_tools.h     │ ⚠️ Missing Drilldown│
│ Wi-Fi Radar          │ WifiRadar.cpp        │ dh_wifi_tools.h      │ ⚠️ Missing Tracking│
│ Direction Finder     │ WifiDirectionFinder.cpp│ dh_wifi_tools.h    │ ⚠️ Broken Vectoring│
│ Beacon Spam          │ BeaconSpam.cpp       │ page_dr_hack.h       │ ⚠️ Hardcoded Pools│
│ Deauther             │ Deauther.cpp         │ page_dr_hack.h       │ 🔴 Missing Sniffer│
│ Evil Portal          │ EvilPortal.cpp       │ dh_evil_portal.h     │ 🔴 0-Byte Ghost H │
│ Probe Sniffer        │ ProbeSniffer.cpp     │ dh_wifi_tools.h      │ ⚠️ Missing Parsers│
│ KARMA Attack         │ Karma.cpp            │ dh_wifi_tools.h      │ 🔴 Promisc Panic  │
│ Wi-Fi Config         │ WifiConfig.cpp       │ dh_wifi_tools.h      │ 🔴 Missing NVS/KB │
│ Packet Monitor       │ PacketMonitor.cpp    │ page_dr_hack.h       │ ⚠️ Buffer Desync  │
│ 2.4GHz Jammer        │ jammer.cpp           │ dh_radio_tools.h     │ ⚠️ 1-Radio Only   │
│ Spectrum / Waterfall │ RadioScanner.cpp     │ dh_radio_tools.h     │ ⚠️ No WiFi Quality│
│ Signal Tools (IR)    │ SignalTools.cpp      │ dh_ir_tools.h        │ 🔴 Inverted Duties│
│ IR Virtual Remotes   │ IrVirtualRemotes.cpp │ dh_ir_tools.h        │ 🔴 Missing NVS/Map│
│ CC1101 Sub-GHz Suite │ CC1101Tools.cpp      │ dh_cc1101_tools.h    │ 🔴 Button Lockup  │
│ BLE Device Radar     │ BLEDeviceRadar.cpp   │ dh_ble_tools.h       │ ⚠️ Missing Scope  │
│ BLE Inspector        │ BLEInspector.cpp     │ dh_ble_tools.h       │ ⚠️ Truncated UUIDs│
│ iPhone Remote        │ BLEIPhoneRemote.cpp  │ dh_ble_tools.h       │ 🔴 Missing Macros │
│ BLE Spam             │ BLESpam.cpp          │ dh_ble_tools.h       │ ⚠️ No Random MAC  │
│ BT Disruptor/Jammer  │ BTDisruptor/bt_jammer│ dh_ble_tools.h       │ ⚠️ Static Carrier │
│ Web Dashboard        │ WebDashboard.cpp     │ dh_web_dashboard.h   │ 🔴 Missing APIs   │
│ Settings & NVS       │ SettingsMenu/NVSStore│ pages.h (settings)   │ 🔴 Absent         │
│ Screensaver/Ajolote  │ Screensaver/Ajolote  │ pages.h (eyes)       │ 🔴 Missing Sprite │
└──────────────────────┴──────────────────────┴──────────────────────┴───────────────────┘
```

---

### 4.1. Wi-Fi Offensive & Defensive Tools

#### A. Wi-Fi Scanner (`WifiScanner.cpp` vs. `page_dr_hack.h`)
- **Original Feature**: Includes a 40+ vendor OUI lookup table (`OUI_TABLE` covering Huawei, Sercomm, Arcadyan, Askey, Nokia, Fiberhome, Arris, Technicolor, Hitron, ZTE, TP-Link, Apple, Samsung), channel-to-frequency conversion (MHz), signal bar generator, hidden SSID detection, and a dedicated `showDetails()` drill-down view.
- **CompanionOS Flaw**: Omits the OUI database entirely, omits frequency conversion, and has no detail drill-down modal. Text strings exceed the 160px screen width.

#### B. Channel Scanner (`WifiChannelScanner.cpp` vs. `dh_wifi_tools.h`)
- **Original Feature**: Scans channels 1–13, sorts networks by channel and signal, displays channel congestion bar charts, and allows pressing `OK` to open a drill-down list (`showChannelAps`) of all individual APs operating on that specific channel with security ratings.
- **CompanionOS Flaw**: Only displays a static summary graph. Pressing `SELECT` immediately terminates the tool; the entire AP inspection drill-down sub-state is missing.

#### C. Wi-Fi Radar & Direction Finder (`WifiRadar.cpp` / `WifiDirectionFinder.cpp` vs. `dh_wifi_tools.h`)
- **Original Feature**:
  1. Interactive AP list selection.
  2. Continuous active tracking mode of target BSSID.
  3. Real-time path-loss distance estimation formula:
     $$\text{Distance (m)} = 10^{\frac{-45 - \text{RSSI}}{10 \times 2.15}}$$
  4. Signal delta trend analyzer (`APPROACH`, `MOVING AWAY`, `STABLE`).
  5. 36-sample historical bar chart.
- **CompanionOS Flaw**: In CompanionOS, `dhRunWifiRadar` attempts to draw on a 160×128 screen using coordinates designed for 320×240 (`x=200`, `y=180`), resulting in massive visual corruption. Target selection is non-functional.

#### D. Deauther (`Deauther.cpp` vs. `page_dr_hack.h`)
- **Original Feature**:
  1. 10-second asynchronous AP scan.
  2. Promiscuous client sniffer (`snifferCallback`) that parses 802.11 Data and Management frames to extract MAC addresses of actively connected client devices.
  3. Multi-target selection: `[BROADCAST - ALL CLIENTS]`, individual client MAC, or `[RAMBO MODE]` (continuous channel-hopping deauth across all detected APs).
  4. Reason code selection (Reason 7: Class 3 frame, Reason 2: Auth expired, Reason 1: Unspecified).
  5. Animated live attack telemetry dashboard.
- **CompanionOS Flaw**:
  - The client sniffer is **100% missing**. CompanionOS can only send broadcast frames to the AP itself.
  - `RAMBO MODE` and Reason code customization are completely omitted.
  - Deauth transmission loop does not yield to FreeRTOS, causing task watchdog triggers.

#### E. Evil Portal (`EvilPortal.cpp`, `EvilPortalLogs.cpp`, `EvilPortalHTML.h` vs. `dh_evil_portal.h`)
- **Original Feature**:
  1. Multi-platform responsive phishing selector (`html_selector`): Facebook, Google, Instagram, TikTok with official SVG icons and dynamic `__SSID__` injection.
  2. Captive Portal Network Assistant hijacking (handles DNS requests for Apple `captive.apple.com`, Android `connectivitycheck.gstatic.com`, Windows `msftconnecttest.com`).
  3. Credential capture stored persistently to NVS (`EvilPortalLogs.cpp`).
  4. Integrated web logs viewer (`/logs`) and on-screen credentials review menu.
- **CompanionOS Flaw**:
  - **`EvilPortalHTML.h` is 0 bytes (empty file)** in `CompanionOS/arduino/CompanionOS_Main/`.
  - `dh_evil_portal.h` fallback has a single hardcoded basic HTML form in English.
  - No captive DNS routing for Android/Windows CNA.
  - Captured credentials stored only in volatile RAM (lost on reset).
  - No web credential viewer or NVS log export.

#### F. KARMA Attack (`Karma.cpp` vs. `dh_wifi_tools.h`)
- **Original Feature**:
  1. Integrates with `ProbeSniffer` history or runs a 15-second probe collection scan.
  2. User selects captured probe SSIDs.
  3. Transmits beacons with deterministic per-SSID MAC hashing ($MAC = \text{Hash}(SSID)$).
  4. Promiscuous RX loop tracks incoming client association requests and probe hits during attack.
- **CompanionOS Flaw & Severe Bug**:
  - In `dh_wifi_tools.h` (line 754), `esp_wifi_80211_tx` is called **directly inside the promiscuous RX callback interrupt** (`dhKarmaProbeCallback`). Calling raw Wi-Fi TX inside the driver's RX interrupt context causes stack overflows, kernel panics, or Wi-Fi hardware freezing.
  - No integration with Probe Sniffer history.

---

### 4.2. Bluetooth & BLE Tools

#### A. BLE Device Radar & Inspector (`BLEDeviceRadar.cpp` / `BLEInspector.cpp` vs. `dh_ble_tools.h`)
- **Original Feature**:
  1. Parses manufacturer data (Company IDs: Apple `0x004C`, Microsoft `0x0006`, Samsung `0x0075`, Google `0x00E0`, Sony, Nordic, Xiaomi, Espressif, Tile, etc.).
  2. Decodes Service UUIDs (HID `1812`, Battery `180F`, Heart Rate `180D`, Thermometer `1809`, Exposure `FD6F`, Google Fast Pair `FE2C/FEAA`).
  3. Decodes BLE Appearance categories (Phone, Watch, Computer, Tag, HID, Sensor).
  4. Radar tracking scope with rotating sweep animation, proximity trend calculation, and distance formula:
     $$\text{Distance (m)} = 10^{\frac{-59 - \text{RSSI}}{10 \times 2.3}}$$
- **CompanionOS Flaw**:
  - Truncated lookup table; omits appearance and service decoders.
  - Radar tracking scope is replaced with a static text line.
  - Hardcoded `digitalRead == LOW` button bug locks the tool on startup.

#### B. iPhone Remote (`BLEIPhoneRemote.cpp` vs. `dh_ble_tools.h`)
- **Original Feature**:
  - Full BLE HID Keyboard + Consumer Media device with standard HID Report Map descriptor.
  - Spotlight shortcut automation: Command+Space $\rightarrow$ Type App Name $\rightarrow$ Enter to launch Safari, YouTube, Spotify, WhatsApp, Instagram, Photos, Notes, or Camera.
  - Dedicated Media submenu: Play/Pause, Track Next/Prev, Stop, Volume Live (interactive Up/Down volume bar), Mute.
  - Dedicated Camera submenu: Open Camera, Volume+ shutter trigger, Video recording toggle, Burst 3x, 3-second countdown timer.
  - Pairing / Bonding security via `BLESecurity`.
- **CompanionOS Flaw**:
  - Stripped down to a 50-line stub that only sends Volume Up / Down.
  - Spotlight app injection, camera triggers, media submenus, and security bonding are **100% missing**.

#### C. BLE Spam (`BLESpam.cpp` vs. `dh_ble_tools.h`)
- **Original Feature**:
  - Full payload arrays for Apple Continuity (13 devices: AirPods Pro 1/2, AirPods Max, Beats Solo 3, Beats Studio Pro, Powerbeats), Samsung Easy Setup (7 Galaxy Buds models), Microsoft Swift Pair (Surface Keyboard, Mouse, Xbox Controller), Google Fast Pair (Pixel Buds, Bose NC700, Sony WH-1000XM4), and Chaos Mode.
  - Random Static MAC regeneration (`esp_ble_gap_set_rand_addr`) on every burst to prevent OS-level MAC filtering on target phones.
- **CompanionOS Flaw**:
  - Static BLE address used across advertisements; target phones quickly silence repeated notifications.
  - Payload arrays are truncated or missing device profiles.

---

### 4.3. Radio (2.4GHz & Sub-GHz) Tools

#### A. 2.4GHz Jammer & RfClown (`jammer.cpp` vs. `dh_radio_tools.h`)
- **Original Feature**:
  - Supports dual nRF24L01 modules running simultaneously across distinct hopping tables.
  - High-speed constant carrier mode (`RF24_PA_MAX`, `RF24_2MBPS`, CRC disabled).
  - Synchronized frequency gauge with channel-to-frequency mapping ($F = 2400 + (\text{CH} \times 5) + 2\text{ MHz}$).
- **CompanionOS Flaw**:
  - Only initializes a single radio module even when dual hardware is present.
  - Fails to cleanly restore SPI bus state upon exit, disabling display redraws.

#### B. Radio Spectrum / Waterfall Scanner (`RadioScanner.cpp` vs. `dh_radio_tools.h`)
- **Original Feature**:
  - 3 operating modes: Spectrum Bar View, Waterfall History View, and **WiFi Channel Quality Analyzer**.
  - Dual-radio interleaving for double sweep speed (80 MHz sweep across 2.400–2.480 GHz).
  - Channel evaluation algorithm analyzes interference across all 13 Wi-Fi channels and automatically outputs `lastRecommendedCh` (Clean / Busy / Crowded rating).
- **CompanionOS Flaw**:
  - Mode 3 (WiFi Channel Quality Analyzer & Recommendation Engine) is completely missing.
  - Single-radio sweep only; sweep refresh rate is halved.
  - Waterfall buffer math hardcoded for 320 width; overflows the ST7735 160-pixel display width.

#### C. CC1101 Sub-GHz Suite (`CC1101Tools.cpp` vs. `dh_cc1101_tools.h`)
- **Original Features (12 Discrete Tools)**:
  1. *Hardware Diag*: CC1101 Partnum, Version, MARCSTATE, SPI loopback verification.
  2. *Spectrum Scan*: Multi-band sweep (315, 433, 868, 915 MHz) with RSSI peak detection.
  3. *Waterfall*: 96-row historical Sub-GHz heat map.
  4. *Frequency Monitor*: Real-time continuous RSSI listening on specific frequency presets.
  5. *Frequency Finder*: Two-stage coarse + fine ($\pm 350\text{ kHz}$ in $25\text{ kHz}$ steps) auto-tuner.
  6. *Brute Search*: High-density bin search across entire bands.
  7. *Code Check*: Detects preamble patterns (0xAA/0x55) and sync words.
  8. *RF Analyzer*: Measures carrier duration, pulse timing, and gap analysis.
  9. *RF Raw View*: Pulse-by-pulse waveform oscilloscope and timing table.
  10. *RF Live*: Real-time edge monitor on GDO0 pin.
  11. *Lab Replay*: OOK TX replay of captured raw pulse trains.
  12. *Test Beacon*: Continuous or pulsed test beacon at maximum PA (+10 dBm).
- **CompanionOS Flaw**:
  - **Severe Button Lockup Bug**: All sub-tools check `digitalRead == LOW` rather than `BTN_ACTIVE_LEVEL`, rendering the CC1101 suite inoperable on touch-based hardware.
  - Sub-GHz Lab Replay uses raw bit-banging on `CC1101_TX_DATA_PIN` (GPIO 25) without configuring CC1101 into Asynchronous TX mode (`CC_IOCFG0 = 0x0D`), resulting in unmodulated carrier output.
  - RSSI conversion formulas misaligned with CC1101 datasheet curve ($RSSI_{\text{dBm}} = \frac{\text{raw}}{2} - 74$).

---

### 4.4. Infrared (IR) Tools

#### A. Raw IR Capture & Replay (`SignalTools.cpp` vs. `dh_ir_tools.h`)
- **Original Feature**:
  - Microsecond-precise pulse/space capture using ESP32 hardware timers / RMT / LEDC carrier generator.
  - Active-low carrier generation at 38 kHz with 33% mark duty cycle (`IR_LEDC_MARK_DUTY = 170`, `IR_LEDC_SPACE_DUTY = 255`).
  - Persistent NVS storage for up to 6 custom IR captures under namespace `irsignals`.
- **CompanionOS Flaw**:
  - Inverted mark/space LEDC duty cycle logic in `dh_ir_tools.h`, causing M5Stack IR transmitter LED to remain continuously energized (overheating the IR LED and draining power).
  - SPIFFS-based capture format incompatible with original NVS schema.

#### B. IR Virtual Remotes (`IrVirtualRemotes.cpp` vs. `dh_ir_tools.h`)
- **Original Feature**:
  - Multi-remote management system (up to 4 remotes, 8 buttons per remote).
  - Interactive button mapping to saved raw IR slots.
  - Text naming via on-screen Virtual Keyboard.
- **CompanionOS Flaw**:
  - Tool 29 (`DH_IR_REMOTES`) in CompanionOS is a static placeholder with non-functional button mappings and no persistent storage.

---

### 4.5. System, UI, & Web Dashboard Tools

#### A. Web Dashboard (`WebDashboard.cpp` vs. `dh_web_dashboard.h`)
- **Original Feature**:
  - Standalone SoftAP web server (`ESP32-TOOLS-PRO`, `192.168.4.1`) with a responsive dark-mode web application.
  - REST API Suite:
    - `/api/status`: System telemetry, RAM, Flash, chip temperature.
    - `/api/ir/list`, `/api/ir/play?slot=X`, `/api/ir/rename`: Remote IR capture management and web-triggered IR blasting.
    - `/api/cc1101/status`, `/api/cc1101/monitor?freq=X`: Sub-GHz RSSI and frequency tuner.
    - `/api/wifi/scan`, `/api/beacon/start`, `/api/beacon/stop`: Remote Wi-Fi scan and beacon spam control.
    - `/api/portal/logs`: Remote viewing of credentials captured by Evil Portal.
- **CompanionOS Flaw**:
  - `dh_web_dashboard.h` is a basic static HTML page with non-functional API bindings.
  - No background client handler; entering the dashboard freezes CompanionOS telemetry.

#### B. Virtual Keyboard (`VirtualKeyboard.cpp`)
- **Original Feature**: 4-row QWERTY + Symbol on-screen virtual keyboard with Shift, Space, Delete, OK, and Cancel buttons navigated via Up, Down, and OK.
- **CompanionOS Flaw**: **Completely omitted**. CompanionOS has no text input engine, which breaks Wi-Fi Config, Evil Portal SSID customization, and IR Remote button naming.

---

## 5. UI Layout & Resolution Scaling Breakdowns

The original project was designed for a **320×240 viewport** (or 480×320 on ILI9488 displays). CompanionOS operates on a **160×128 ST7735 display**.

```
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                          DISPLAY VIEWPORT COMPARISON                                   │
├────────────────────────────────────────────────────────────────────────────────────────┤
│ Original Viewport: 320 × 240 (76,800 pixels)                                          │
│ ┌────────────────────────────────────────────────────────────────────────────────────┐ │
│ │ Header (320 × 32)                                                                  │ │
│ │ Content Area (320 × 172) - 6 visible rows, 280px wide cards, 24px line height      │ │
│ │ Footer (320 × 36) - Navigation guides and status pills                             │ │
│ └────────────────────────────────────────────────────────────────────────────────────┘ │
│                                                                                        │
│ CompanionOS Viewport: 160 × 128 (20,480 pixels) - 73.3% LESS SCREEN REAL ESTATE       │
│ ┌──────────────────────────┐                                                           │
│ │ Header (160 × 14)        │  <- Critical: Unscaled 320x240 coordinates result in      │
│ │ Content (160 × 100)      │     text rendering past X=160 (invisible) and             │
│ │ Footer (160 × 14)        │     vertical overlap when Y exceeds 128!                  │
│ └──────────────────────────┘                                                           │
└────────────────────────────────────────────────────────────────────────────────────────┘
```

### Specific Coordinate Breakdowns in Ported Headers:
1. **`page_dr_hack.h` (Lines 462–623, Beacon Spam & Deauth)**:
   - Draws progress bars at `x = 10, w = 300` $\rightarrow$ Clamped/wrapped at 160px, causing display driver memory corruption.
   - Text rendered at `y = 145`, `y = 180`, `y = 210` $\rightarrow$ Outside physical panel bounds ($Y_{\text{max}} = 128$).
2. **`dh_wifi_tools.h` (Lines 168–400, WiFi Radar)**:
   - Radar circle drawn at radius $R = 60$ at $(X=160, Y=120)$ $\rightarrow$ 75% of the circle is drawn off-screen.
3. **`dh_cc1101_tools.h` (Spectrum & Waterfall)**:
   - Spectrum bins hardcoded to 48 columns with 6-pixel widths ($48 \times 6 = 288\text{ px}$) $\rightarrow$ Squeezes onto screen incorrectly or clips past right edge.

---

## 6. Concurrency, Task Scheduler, and Network Clashes

CompanionOS is a complex multi-state operating system running FreeRTOS tasks, UDP packet listener (`companion_net.h`), BLE/Spotify controllers, and dynamic page renderers.

### The Promiscuous Mode Collision:
- When Dr. Hack starts Wi-Fi offensive tools (Deauther, Beacon Spam, Evil Portal, Karma, Packet Monitor), it invokes `WiFi.mode(WIFI_STA)` and `esp_wifi_set_promiscuous(true)`.
- **The Bug**:
  1. Promiscuous mode disables the ESP32 station Wi-Fi TCP/IP stack. CompanionOS's UDP network connection (`handleNetwork()`) drops instantly.
  2. While `dhNetPaused = true` is set, several tools call `handleNetwork()` inside their tight loops, attempting to send UDP packets over a disabled interface.
  3. Upon exiting Dr. Hack sub-tools, the firmware fails to cleanly restore Wi-Fi STA mode, reconnect to the home router, and resume UDP telemetry, requiring a full hardware reboot.

---

## 7. Actionable Remediation Roadmap for Full Working Port

To achieve **100% full, bug-free functionality** of the original `hack update files` within CompanionOS Dr. Hack on the ST7735 160×128 display, the following systematic remediation plan must be executed:

```
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                             REMEDIATION EXECUTION ROADMAP                              │
├──────┬──────────────────────┬──────────────────────────────────────────────────────────┤
│ Step │ Phase                │ Core Deliverables & Key Changes                          │
├──────┼──────────────────────┼──────────────────────────────────────────────────────────┤
│ 1    │ Hardware Abstraction │ Unify `BTN_ACTIVE_LEVEL` across all sub-tools;           │
│      │ & Bus Protection     │ Implement SPI transaction guards & strict CS management  │
├──────┼──────────────────────┼──────────────────────────────────────────────────────────┤
│ 2    │ Display & UI Engine  │ Create high-density 160×128 UI layouts for all 48 tools; │
│      │                      │ Replace overflowing coordinates with responsive macros   │
├──────┼──────────────────────┼──────────────────────────────────────────────────────────┤
│ 3    │ Core Engine Porting  │ Port `VirtualKeyboard`, `NVSStore`, `EvilPortalHTML.h`,  │
│      │                      │ OUI vendor lookup table, and protocol decoder tables     │
├──────┼──────────────────────┼──────────────────────────────────────────────────────────┤
│ 4    │ Offensive Tool Logic │ Rebuild Deauther with Client Sniffer; fix KARMA callback;│
│      │                      │ Rebuild Evil Portal with multi-platform CNA hijacking    │
├──────┼──────────────────────┼──────────────────────────────────────────────────────────┤
│ 5    │ Radio & BLE Engines  │ Restore iPhone Remote Spotlight HID macros;              │
│      │                      │ Implement CC1101 12-tool suite with corrected SPI logic  │
├──────┼──────────────────────┼──────────────────────────────────────────────────────────┤
│ 6    │ Network & OS State   │ Implement clean promiscuous entry/exit lifecycle;        │
│      │                      │ Restore CompanionOS UDP link automatically on exit       │
└──────┴──────────────────────┴──────────────────────────────────────────────────────────┘
```

---
*Report prepared after exhaustive line-by-line inspection of CompanionOS (`CompanionOS/`) and original ESP32-TOOLS-PRO V2.0 (`hack update files/`).*
