# Deployment Guide — CompanionOS

This guide covers setting up the ESP32 hardware and the Python PC Bridge required for full CompanionOS functionality.

## 1. Hardware Requirements
- **ESP32 Node**: ESP32 DevKit V1 (or equivalent). ESP32-S3 is supported but requires core downgrade if you intend to use the raw Wi-Fi TX (Deauth/Beacon Spam) bypasses.
- **Display**: 160x128 ST7735 TFT or 320x240 ILI9341 via SPI.
- **Modules**: CC1101 (for sub-GHz radio tools) and IR LED/Receiver for Dr.Hack suite.

## 2. Firmware Flashing (Arduino IDE)
1. **Library Dependencies**:
   - `TFT_eSPI` (Configure `User_Setup.h` with your specific TFT pins).
   - `ArduinoJson` (v6 or v7).
   - `SmartRC-CC1101-Driver-Lib`.
   - `IRremoteESP8266`.
2. **Wi-Fi Settings**: Create a `secrets.h` file alongside `CompanionOS_Main.ino`:
   ```cpp
   #define WIFI_SSID "YourNetwork"
   #define WIFI_PASS "YourPassword"
   #define PC_IP "192.168.1.50" // The IP of the computer running the Python bridge
   ```
3. **Arduino IDE Board Settings**:
   - **Board**: `ESP32 Dev Module` (or `DOIT ESP32 DEVKIT V1`)
   - **Flash Size**: `4MB (32Mb)`
   - **Partition Scheme**: `Huge APP (3MB No OTA/1MB SPIFFS)`  *(Required because BLE + WiFi + Dr.Hack uses ~2.0MB)*
   - **Core Debug Level**: `None` (or `Error`)
4. Compile and flash to the ESP32.

## 3. Python Bridge Setup
The Python daemon is required for Spotify, Steam tracking, Weather, and RuView processing.

1. **Install Dependencies**:
   ```bash
   pip install flask requests spotipy psutil pillow
   ```
2. **Environment Variables**:
   You must set the following variables before running the controller:
   - `SPOTIPY_CLIENT_ID` and `SPOTIPY_CLIENT_SECRET` (from Spotify Developer Dashboard).
   - `SPOTIPY_REDIRECT_URI` (usually `http://localhost:8080`).
   - `OPENWEATHER_API_KEY` (from OpenWeatherMap).
   - `TWITCH_CLIENT_ID` and `TWITCH_CLIENT_SECRET` (Optional: for IGDB game cover fetching).
3. **Run the Daemon**:
   ```bash
   python companion_controller.py
   ```

## 4. RuView CSI Setup (Optional)
If you are deploying a secondary ESP32 node for RuView CSI presence detection:
1. Flash the RuView node with the ESP-IDF `ruview` firmware.
2. Ensure the node is on the same network subnet as the Python bridge.
3. The Python daemon automatically listens for `ADR-018` packets and will populate the Web UI Map at `http://localhost:5000/ruview`.
