# CompanionOS v7.0

CompanionOS is an advanced, multi-functional firmware designed for ESP32 microcontrollers featuring an integrated suite of hardware testing, diagnostic, and offensive radio tools ("Dr. Hack"). It supports a 160x128 ST7735R TFT display, interactive UI with custom animations, and a powerful backend for interacting with external radios (nRF24L01, CC1101, IR).

## ✨ Features

- **Dr. Hack Tool Suite (48 Tools)**
  - **Wi-Fi Diagnostics**: Karma Attacks, Evil Portal, Beacon Spam, Deauth (Targeted & Broadcast), Probe Sniffer, Wi-Fi Radar.
  - **2.4GHz Operations**: Multi-module nRF24L01 Jammers (RfClown Port), Spectrum Analyzers (Waterfall and Bar graphs).
  - **Bluetooth/BLE Tools**: BLE Advertising Jammers, MAC Spoofing, BLE Beacons.
  - **Sub-1GHz**: CC1101 integrated signal replay and raw capturing.
  - **IR**: Infrared signal capture, playback, and analysis.
- **Web Dashboard**: Host a remote-control web server for headless interaction and real-time hardware status querying.
- **Hardware Agnostic UI**: Custom-built graphics engine optimized for ST7735R, supporting both tactile button inputs and XPT2046 touch screens.
- **Concurrency Safe**: Employs rigorous state management (`dhNetPaused`) to prevent watchdog resets and driver panics when switching between background TCP/UDP services and raw promiscuous mode operations.

## 🛠 Hardware Requirements

- **Microcontroller**: ESP32 (e.g., ESP32-WROOM, ESP32-WROVER)
- **Display**: ST7735R 160x128 SPI TFT display
- **Radios (Optional but Recommended)**:
  - 2x nRF24L01+ modules (for 2.4GHz Jamming and Spectrum Analysis)
  - 1x CC1101 module (for Sub-1GHz RF)
  - IR Receiver / Transmitter (e.g., TSOP38238 / standard IR LED)
- **Input**: Tactile buttons (Left, Select, Right) or XPT2046 Touch Screen

## ⚙️ Pin Configuration

The default pinouts can be modified in `config_esp32.h` or `globals.h`. 

**Default SPI / nRF24:**
- `NRF1_CE`: GPIO 32
- `NRF1_CSN`: GPIO 33
- `NRF2_CE`: GPIO 12
- `NRF2_CSN`: GPIO 14
*(Note: Ensure your nRF modules are connected properly. The firmware includes hardware checks to alert you if initialization fails).*

## 🚀 Getting Started

### 1. Installation & Compilation
1. Install the [Arduino IDE](https://www.arduino.cc/en/software) or use `arduino-cli`.
2. Add the ESP32 board manager URL in preferences and install the ESP32 core (`v3.x` recommended).
3. Install required libraries:
   - `TFT_eSPI` (Configure `User_Setup.h` for ST7735, 160x128, appropriate pins).
   - `RF24` (For nRF24L01).
   - `ELECHOUSE_CC1101_SRC_DRV` (For CC1101).
   - `IRremote` (For infrared).
4. Open `CompanionOS_Main.ino` in the Arduino IDE.
5. Select your ESP32 board and compile/upload.

### 2. Using the Dr. Hack Suite
- Navigate to the **Dr. Hack** section via the main menu.
- Use the **Left/Right** buttons to scroll through tools and **Select** to activate.
- **Hold Select** to exit any running tool or return to the previous menu.
- **Wi-Fi Tools**: The system automatically switches into AP or Promiscuous mode and pauses background web-server tasks (`dhNetPaused`). Upon exiting the tool, standard networking resumes.

### 3. Web Dashboard
- Navigate to **Web Dashboard** in the Dr. Hack menu.
- Connect your phone/PC to the `CompanionOS-Hack` Wi-Fi AP.
- Navigate to `http://192.168.4.1` in your browser.
- Use the dashboard to view live telemetry, run remote Wi-Fi scans, simulate button presses, or run a hardware test on your nRF24 modules.

## ⚠️ Disclaimer

**CompanionOS and the "Dr. Hack" tool suite are provided strictly for educational purposes and authorized auditing only.** 
The offensive capabilities included in this repository (e.g., Deauth, Jamming, Evil Portals) are illegal to use on networks or devices you do not explicitly own or have permission to test. The developers assume no liability for misuse or damage caused by this software. Use responsibly.

## 🤝 Contributing

Contributions, bug reports, and feature requests are welcome! If you're submitting a pull request for a new radio tool, please ensure it adheres to the existing concurrency model (checking/setting `dhNetPaused` when using raw Wi-Fi functions) to prevent system crashes.

## 📝 License

This project is licensed under the MIT License - see the LICENSE file for details.
