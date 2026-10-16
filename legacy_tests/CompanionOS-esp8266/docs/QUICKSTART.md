# 🚀 CompanionOS - Quickstart Guide

Get your desk bot up and running in 5 minutes after hardware assembly!

## 1. Arduino Upload 🤖

1. Connect your ESP8266 via USB.
2. Open `arduino/CompanionOS_Main/CompanionOS_Main.ino` in the Arduino IDE.
3. Select **NodeMCU 1.0 (ESP-12E Module)** and your exact COM Port.
4. Click **Upload**. Wait for "Done Uploading".
5. *(No more hardcoded WiFi keys! 🥳)* The ESP will start a Wi-Fi Hotspot called `"CompanionOS-Setup"`. Connect your phone/laptop to it, and a captive portal will open. Enter your real Wi-Fi credentials there!

## 2. Python Setup 🐍

1. Open a terminal in `python/`.
2. Generate your `.env` secrets file safely by running:
   ```bash
   python ../tools/install.py
   ```
   *(Paste your API keys here so you don't save them in your git history!)*
3. Setup the virtual environment:
   ```bash
   python -m venv venv
   # Windows:
   venv\Scripts\activate
   # Mac/Linux:
   source venv/bin/activate
   ```
4. Install requirements:
   ```bash
   pip install -r requirements.txt
   ```
5. Run the controller!
   ```bash
   python companion_controller.py
   ```

## 3. Enjoy! 🎉

The controller dynamically tracks the hardware on the network. The screen on the Companion will change as soon as you play a song on Spotify!
