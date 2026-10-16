# 🤖 HOW TO USE COMPANION OS

Welcome to your completed **Companion OS**! This guide will walk you through everything you need to know to interact with, control, and enjoy your new smart desk companion.

---

## ⚡ 1. Initial Power & Startup

### Step 1: Power the Device
Plug the Micro-USB cable into the ESP8266 and connect the other end to a USB power supply (or your computer). The boot sequence takes approximately 2-3 seconds. The display will light up and show the initialization screen.

### Step 2: Connect to WiFi (First Time Only)
Companion OS uses a smart, placeholder-free WiFi manager. 
1. If the device cannot find a known network, the screen will say **"Connecting WiFi..."**.
2. Grab your smartphone or laptop and open your WiFi settings.
3. Look for a new network called **`CompanionOS-Setup`** and connect to it.
4. A captive portal page will automatically open (if not, go to `192.168.4.1` in your browser).
5. Select your home WiFi network, enter your password, and click Save.
6. The device will automatically reboot and connect to your home network!

---

## 💻 2. Starting the Python Controller

Your ESP8266 hardware display relies on the Python Controller running on your PC to fetch heavy data (Spotify, lyrics, GitHub API).

1. Open a terminal or command prompt on your PC.
2. Navigate to the Python folder: `cd \CompanionOS\python\`
3. Ensure your `.env` file is populated with your API keys.
4. Run the controller:
   ```bash
   python companion_controller.py
   ```
5. The Python script will begin running in the background. Thanks to our dynamic **UDP Handshake Discovery**, neither device needs to know the other's IP address. They will automatically find each other on the network within 2-3 seconds!

---

## 👆 3. Navigating the Interface

Companion OS uses a 6-page paradigm. You can navigate the UI using the on-screen touch capabilities and your physical capacitive touch sensors ("ears").

### Changing Pages (Swiping)
Tap the left side of the screen to go to the **Previous Page**.
Tap the right side of the screen to go to the **Next Page**.

You can distinguish which page you are on by looking at the page indicator dots `[• · · · · ·]` at the top of the interface.

### Physical "Ear" Sensors
If you wired capacitive touch sensors to the Left and Right sides of the device (Pins D3 and D4):
- **Tap the Left Ear:** Skips to the *Previous Track* on Spotify.
- **Tap the Right Ear:** Skips to the *Next Track* on Spotify.

---

## 📱 4. The 6 Dashboard Pages

### Page 1: 👁️ The Eyes & Clock
- **What it does:** The default resting face of your companion. Features smooth tracking pupils, multi-phase blinking, and dynamic expressions.
- **Time Display:** A real-time digital clock displays at the bottom, automatically synced with your PC.
- **Emotions:** The eyes react to external commands (Happy, Sad, Angry, Love, Sleepy, Surprised, Excited).

### Page 2: 🎵 Spotify Player
- **What it does:** A robust 'Now Playing' dashboard for your Spotify account.
- **Features:**
  - Displays Album Art synced directly to your screen.
  - Shows Track Name, Artist, and real-time playback progress bar.
  - Fetches and displays **live synchronized lyrics** for the current song!
  - You can tap the on-screen "Prev", "Play", and "Next" buttons to control playback from the device.

### Page 3: 🐙 GitHub Stats
- **What it does:** Tracks your coding activity.
- **Features:** Displays your GitHub username, total public repository count, and follower count, automatically polling for updates every 60 minutes.

### Page 4: 📝 Quick Notes
- **What it does:** Transmits a local to-do list from your PC straight to your desk terminal.
- **How to edit:**
  1. Open the file `CompanionOS\python\notes.txt` on your PC.
  2. Type up to 4 lines of notes (e.g., "* Buy groceries", "* Fix bug 402").
  3. Save the file.
  4. Within 5 seconds, the Companion OS screen will update instantly without needing to restart anything!

### Page 5: 🎤 Audio Visualizer
- **What it does:** Reads data from your connected analog microphone (`MIC_PIN A0`).
- **Features:** Renders an ultra-fast, responsive cyan bar-graph on the screen that bounces to the sound of music, your voice, or ambient desk noise. 

### Page 6: ⚙️ System Monitor
- **What it does:** Displays the internal telemetry of your ESP8266 hardware.
- **Features:**
  - **Network IP:** Displays the local IP address assigned to the device.
  - **Free Memory:** Live tracking of the ESP8266's available Heap RAM space.
  - **System Uptime:** A ticking clock showing exactly how many Hours, Minutes, and Seconds your bot has been awake.

---

## 💡 5. Pro-Tips & Troubleshooting

* **Dynamic IPs? No Problem:** You never need to hardcode an IP address. If the router assigns your device a new IP, the Python Controller's UDP Handshake will automatically catch it.
* **Blank Spotify Art?** Ensure Spotify playback is currently active on your PC/Phone. If the art is dropping packets, wait for the next track to resume drawing it.
* **Want to change an Emotion?** The Python controller can be commanded to send UDP packets to change the emotion. For example, sending `EMOTION:SLEEPY` will put the companion to sleep.
* **Updating the WiFi:** If you move to a new apartment or office, simply wait for the 30-second WiFi timeout, and the device will spin up the `CompanionOS-Setup` portal again so you can enter the new password without touching the code.
