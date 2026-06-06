# 🔧 CompanionOS - Troubleshooting Guide

If things aren't working smoothly, start here!

## Hardware Issues ⚠️

### **Screen is completely White or Garbage Data**
- Double check your 3.3V power limits.
- The `User_Setup.h` file MUST be physically copied to `Documents/Arduino/libraries/TFT_eSPI/User_Setup.h`. Ensure you compiled using our exact pin configurations.
- Ensure the `SPI_FREQUENCY` isn't too high for your specific TFT model. If garbage persists, drop it to `27000000`.

### **Touch buttons aren't doing anything**
- Ensure your `T_CS` (Touch Chip Select) is specifically connected to `D8` on the ESP8266.
- In `touch.h`, ensure the physical sensors (ears) aren't registering false touches due to a floating ground.

## Software Issues 🐛

### **ESP Freezes on "Connecting WiFi" forever**
- You might have entered the wrong password into the WiFiManager portal.
- Try rebooting the ESP. It will attempt to fall back to the "CompanionOS-Setup" AP after ~20 seconds of failure. Re-enter your credentials.

### **Python Controller: "Placeholders Detected"**
- You must replace `"abc123def456ghi789jkl012mno345pq"` with your REAL Spotify Client ID!
- Use our `tools/install.py` script to generate an `.env` file that bypasses the config.json defaults locally.

### **Companion screen doesn't respond to PC Spotify changes**
- Ensure your PC and the ESP8266 are on the **exact same Wi-Fi subnet**.
- Windows Defender/Firewall often blocks UDP Port 8888 broadcasts. Allow Python through your local UDP Firewall.
- The modular V2 system automatically captures your PC's IP when the PC sends its first ping. Wait a moment for dynamic resolution.
