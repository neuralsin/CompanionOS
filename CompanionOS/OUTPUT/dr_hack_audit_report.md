# Dr. Hack Integration Audit Report

This report compares the original **ESP32-TOOLS-PRO-480x320-V2.0** firmware (located in `hack update files`) against what is currently implemented in **CompanionOS v7.0** (`page_dr_hack.h`).

## Summary of Findings
The original firmware is a massive hardware-heavy toolkit designed for a 480x320 TFT, dual nRF24L01 radios, CC1101 Sub-GHz module, and an M5Stack IR unit. It contains over 40 distinct features.

**CompanionOS** currently implements a simplified "Dr. Hack" suite adapted for its 160x128 screen and native ESP32 hardware (WiFi/BLE only). 
- **Implemented:** 7 features (+1 custom feature)
- **Skipped / Missing:** 38 features (mostly due to missing CC1101, IR, and nRF24L01 hardware in CompanionOS).

---

## Comprehensive Functionality Comparison Table

| Category | Feature from ESP32-TOOLS-PRO | Status in CompanionOS | Notes |
|----------|------------------------------|-----------------------|-------|
| **WiFi Tools** | WiFi Scanner | ✅ **Implemented** | Available as `DH_WIFI_SCANNER` |
| | Channel Scan | ❌ **Skipped** | |
| | WiFi Radar | ❌ **Skipped** | |
| | WiFi Direction Finder | ❌ **Skipped** | |
| | WiFi Config | ❌ **Skipped** | Config handled by CompanionOS `Settings` |
| | Beacon Spam | ✅ **Implemented** | Available as `DH_BEACON_SPAM` |
| | Deauther | ✅ **Implemented** | Available as `DH_DEAUTH` |
| | Evil Portal | ❌ **Skipped** | |
| | Probe Sniffer | ❌ **Skipped** | |
| | KARMA Attack | ❌ **Skipped** | |
| **Radio (2.4GHz)** | Jammer | ❌ **Skipped** | Requires dual nRF24L01 modules |
| | Radio Scanner | ❌ **Skipped** | Requires nRF24L01 |
| **IR / Signal** | Hardware Diag | ❌ **Skipped** | Requires M5Stack IR Unit |
| | Input Monitor | ❌ **Skipped** | Requires M5Stack IR Unit |
| | IR Raw Capture | ❌ **Skipped** | Requires M5Stack IR Unit |
| | IR Replay | ❌ **Skipped** | Requires M5Stack IR Unit |
| | IR TX Test | ❌ **Skipped** | Requires M5Stack IR Unit |
| | Saved Captures | ❌ **Skipped** | Requires M5Stack IR Unit |
| | IR Remotes | ❌ **Skipped** | Requires M5Stack IR Unit |
| | IR Analyzer | ❌ **Skipped** | Requires M5Stack IR Unit |
| | Protocol Scan | ❌ **Skipped** | Requires M5Stack IR Unit |
| | IR Sniffer | ❌ **Skipped** | Requires M5Stack IR Unit |
| | Night IR | ❌ **Skipped** | Requires M5Stack IR Unit |
| | IR Proximity | ❌ **Skipped** | Requires M5Stack IR Unit |
| **Sub-GHz (CC1101)** | Hardware Diag | ❌ **Skipped** | Requires CC1101 |
| | Spectrum Scan | ❌ **Skipped** | Requires CC1101 |
| | Waterfall | ❌ **Skipped** | Requires CC1101 |
| | Frequency Mon | ❌ **Skipped** | Requires CC1101 |
| | Freq Finder | ❌ **Skipped** | Requires CC1101 |
| | Brute Search | ❌ **Skipped** | Requires CC1101 |
| | Code Check | ❌ **Skipped** | Requires CC1101 |
| | RF Analyzer | ❌ **Skipped** | Requires CC1101 |
| | RF Raw View | ❌ **Skipped** | Requires CC1101 |
| | RF Live | ❌ **Skipped** | Requires CC1101 |
| | Lab Replay | ❌ **Skipped** | Requires CC1101 |
| | Test Beacon | ❌ **Skipped** | Requires CC1101 |
| **Bluetooth** | BLE Device Radar | ⚠️ **Half Embedded** | Basic scan implemented as `DH_BT_SCANNER`, but lacks the advanced "Radar" tracking of specific devices. |
| | BLE Inspector | ❌ **Skipped** | |
| | iPhone Remote | ❌ **Skipped** | |
| | BLE Spam | ❌ **Skipped** | |
| | BT Disruptor | ❌ **Skipped** | |
| | BT Jammer | ❌ **Skipped** | Requires dual nRF24L01 modules |
| **System Tools** | Settings | ⚠️ **Not Mapped** | Replaced by CompanionOS's native Settings |
| | System Info | ✅ **Implemented** | Available as `DH_INFO` |
| | Clock & Weather | ⚠️ **Not Mapped** | Replaced by CompanionOS's native Weather/Time UI |
| | Web Dashboard | ❌ **Skipped** | No web dashboard in CompanionOS Dr. Hack |
| | Packet Monitor | ✅ **Implemented** | Available as `DH_PACKET_MONITOR` (Original was separate from Sys Tools but present) |
| | About | ✅ **Implemented** | Available as `DH_ABOUT` |
| **Custom** | Port Scanner | 🌟 **Exclusive** | Added specifically for CompanionOS (`DH_PORT_SCANNER`) |

---

## Detailed Analysis of "Half-Baked" or Modified Implementations

### 1. BLE Scanner (`DH_BT_SCANNER`) vs BLE Device Radar
**Original**: Scans, lists devices, shows RSSI and Manufacturer data, and allows selecting a device to enter a dedicated "Radar" tracking screen with historical charts.
**CompanionOS**: Only implements the list view (Name, MAC, RSSI, Company). The radar tracking screen and history are completely skipped.

### 2. UI Simplification
**Original**: 480x320 layout with complex charts, waterfalls, and touch navigation.
**CompanionOS**: Highly compressed into a 160x128 space. The Dr. Hack main menu uses a 2x4 grid to fit exactly 8 tools. Adding more tools would require redesigning the Dr. Hack menu into a scrolling list or multi-page grid.

### 3. Hardware Limitations
CompanionOS relies strictly on the native ESP32 WiFi and BLE transceivers. 
Because of this, **100% of the Radio (Jammer), IR, and CC1101 (Sub-GHz) tools have been skipped**. The code for these tools is entirely absent from `page_dr_hack.h`.

### 4. Custom Port Scanner (`DH_PORT_SCANNER`)
CompanionOS added a custom TCP Port Scanner that was not part of the standard ESP32-TOOLS-PRO menu suite. It tests 12 common ports (22, 80, 443, etc.) on the connected PC's IP address.

## Conclusion
The current `page_dr_hack.h` is **not a 1:1 port** of the `hack update files`. It is a highly curated "Lite" version that extracts only the tools that do not require external hardware modules (nRF24, CC1101, IR) and compress them to fit a 160x128 ST7735 display. 

If you want the remaining features ported, you will need to:
1. Add the external hardware modules to the CompanionOS build.
2. Massively redesign the UI rendering for each missing tool to fit the 160x128 screen.
3. Build a scrolling menu system for Dr. Hack, as the 2x4 grid is currently full.
