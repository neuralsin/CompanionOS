# 🎯 SPOTIFY DESK COMPANION - COMPLETE BUILD GUIDE
## The Ultimate Step-by-Step Manual to Build Your Own Desk Bot

**Version:** 1.0  
**Last Updated:** March 2026  
**Difficulty:** Intermediate  
**Time to Complete:** 2-3 days  
**Cost:** ~$40 in parts  

---

## 📋 TABLE OF CONTENTS

### PART 1: PREPARATION
1.1 What You're Building  
1.2 Required Components  
1.3 Required Software  
1.4 Skills Required  
1.5 Before You Begin  

### PART 2: HARDWARE ASSEMBLY
2.1 Component Overview  
2.2 Pin Mapping Reference  
2.3 Step-by-Step Wiring  
2.4 Testing Each Component  
2.5 Common Wiring Mistakes  

### PART 3: ARDUINO SETUP
3.1 Installing Arduino IDE  
3.2 Installing Required Libraries  
3.3 Configuring TFT_eSPI  
3.4 Understanding the Code Structure  
3.5 Uploading Firmware  

### PART 4: PYTHON CONTROLLER
4.1 Python Environment Setup  
4.2 Getting API Keys (Spotify, GitHub)  
4.3 Configuration Files  
4.4 Running the Controller  

### PART 5: SYSTEM FEATURES
5.1 Eyes/Emotional Face System  
5.2 Spotify Integration  
5.3 GitHub Dashboard  
5.4 Notes App  
5.5 Audio Visualizer  
5.6 Settings & Hardware Monitor  

### PART 6: CUSTOMIZATION
6.1 Adding New Emotions  
6.2 Changing Colors/Themes  
6.3 Adding New Pages  
6.4 Custom Widgets  

### PART 7: TROUBLESHOOTING
7.1 Display Issues  
7.2 Touch Issues  
7.3 Network Issues  
7.4 Spotify Issues  
7.5 GitHub Issues  

### PART 8: ADVANCED TOPICS
8.1 Voice Commands  
8.2 OTA Updates  
8.3 Custom Animations  
8.4 3D Case Design  

---

# PART 1: PREPARATION

## 1.1 WHAT YOU'RE BUILDING

You are building a **complete desk companion system** that:

### **Core Features:**
- **Emotional Face:** Displays eyes with 15+ emotions (happy, sad, crying, love, puppy eyes, sleepy, angry, surprised, etc.)
- **Spotify Interface:** Shows album art, scrolling lyrics, playback controls
- **GitHub Dashboard:** Your profile stats, contribution graph, repository list
- **Notes App:** Quick note-taking with touchscreen keyboard
- **Audio Visualizer:** Real-time visualization from microphone
- **Settings Panel:** Brightness, WiFi, feature toggles, hardware tests
- **Status Monitor:** CPU, RAM, network stats, uptime

### **Interaction Methods:**
- **Touchscreen:** Tap buttons, swipe between pages, gesture controls
- **Physical Touch Sensors:** Two capacitive sensors act as "ears" (prev/next track)
- **Microphone:** Audio visualization, voice commands (optional)
- **Distance Sensor:** (Future expansion) Head-tilt parallax effects

### **Technical Specifications:**
- **Display:** 240x320 pixel TFT (ILI9341)
- **Processor:** ESP8266 @ 160MHz
- **RAM:** 80KB
- **Flash:** 4MB
- **Network:** WiFi 802.11 b/g/n (2.4GHz)
- **Framerate:** 30 FPS target
- **Languages:** C++ (Arduino), Python 3.8+

---

## 1.2 REQUIRED COMPONENTS

### **Essential Components (Must Have):**

**Main Components:**
```
[ ] ESP8266 NodeMCU v1.0 (or Wemos D1 Mini)
    Price: ~$5
    Notes: Must have 4MB flash, 2.4GHz WiFi
    Where: AliExpress, Amazon, local electronics store
    
[ ] ILI9341 2.4" TFT Display (240x320)
    Price: ~$8
    Notes: Must include XPT2046 touch controller
    Where: AliExpress, Amazon
    Part number: ILI9341 with touch
    
[ ] 2x Capacitive Touch Sensors
    Price: ~$2 for 2
    Notes: TTP223 modules work great
    Where: AliExpress, Amazon
    
[ ] Analog Microphone Module
    Price: ~$3
    Options: MAX9814, MAX4466, or similar
    Notes: Must be analog output (not I2S)
    Where: AliExpress, Adafruit, SparkFun
```

**Wiring & Power:**
```
[ ] Breadboard (830 point recommended)
    Price: ~$3
    
[ ] Jumper Wires (M-M, M-F, F-F set)
    Price: ~$5
    Notes: Get variety pack with all types
    
[ ] Micro USB Cable (data + power)
    Price: ~$3
    Notes: Must support data (not just charging)
    
[ ] USB Power Supply (5V, 1A minimum)
    Price: ~$5
    Notes: 2A recommended for stability
```

**Optional But Recommended:**
```
[ ] HC-SR04 Ultrasonic Distance Sensor
    Price: ~$2
    Notes: For future head-tilt parallax effect
    
[ ] 10kΩ and 2kΩ resistors (for voltage divider)
    Price: ~$1
    Notes: Needed if using HC-SR04
    
[ ] Small project box/case
    Price: ~$5-10
    Notes: For final assembly
```

### **Total Cost:** ~$40-50 USD

---

## 1.3 REQUIRED SOFTWARE

### **On Your Computer:**

**Arduino IDE:**
```
Version: 1.8.19 or 2.x
Download: https://www.arduino.cc/en/software
OS Support: Windows, macOS, Linux
```

**Python:**
```
Version: 3.8 or newer (3.11 recommended)
Download: https://www.python.org/downloads/
Must install: pip package manager
```

**Code Editor (Optional but helpful):**
```
VSCode: https://code.visualstudio.com/
Or any text editor you prefer
```

**Git (Optional):**
```
For version control and downloading repos
Download: https://git-scm.com/downloads
```

### **Arduino Libraries (Will install later):**
```
1. TFT_eSPI (by Bodmer)
2. XPT2046_Touchscreen (by Paul Stoffregen)
3. ArduinoJson (optional, for better JSON parsing)
```

### **Python Packages (Will install later):**
```
1. spotipy (Spotify API)
2. pillow (Image processing)
3. requests (HTTP requests)
4. python-dotenv (optional, for env vars)
```

---

## 1.4 SKILLS REQUIRED

### **What You MUST Know:**
- ✅ Basic electronics (can connect wires to breadboard)
- ✅ Can install software on computer
- ✅ Can copy/paste and edit text files
- ✅ Can use a web browser to get API keys

### **What You DON'T Need to Know:**
- ❌ Programming (I provide all code)
- ❌ Advanced electronics theory
- ❌ PCB design
- ❌ 3D modeling (optional case)

### **Learning Curve:**
```
Hour 1-2:   Hardware assembly & wiring
Hour 3-4:   Arduino setup & library installation
Hour 5-6:   Python setup & API configuration
Hour 7-8:   Testing & troubleshooting
Hour 9-10:  Customization & tweaking
```

**If you can build LEGO and install apps, you can build this.**

---

## 1.5 BEFORE YOU BEGIN

### **Critical Warnings:**

**⚠️ VOLTAGE WARNING:**
```
ESP8266 operates at 3.3V logic!
DO NOT connect 5V to any GPIO pins!
TFT display must receive 3.3V (NOT 5V!)
Connecting 5V to 3.3V pins WILL DAMAGE your components!
```

**⚠️ BOOT PIN WARNING:**
```
GPIO0 (D3) and GPIO15 (D8) are boot pins
Must be in correct state during boot:
- GPIO0 must be HIGH
- GPIO15 must be LOW
We're using these pins carefully to avoid boot issues
```

**⚠️ POWER WARNING:**
```
Total current draw: ~300mA
USB port provides 500mA - safe
Do NOT connect multiple power sources simultaneously
Always use quality USB cables (cheap cables = unstable power)
```

### **Safety Checklist:**
```
[ ] Read all warnings above
[ ] Work on non-conductive surface (wood table, not metal)
[ ] Disconnect power before changing wiring
[ ] Double-check connections before applying power
[ ] Have fire extinguisher nearby (rare, but be safe)
[ ] Work in well-ventilated area
```

### **Workspace Setup:**
```
[ ] Clear, well-lit workspace
[ ] Anti-static mat (recommended)
[ ] Good lighting (desk lamp)
[ ] Computer nearby with internet
[ ] Notepad for tracking progress
[ ] Camera/phone for documenting wiring
```

### **Download This Entire Guide:**
```
Save this document offline
Print critical sections (wiring diagram)
Have backup internet access for troubleshooting
```

---

# PART 2: HARDWARE ASSEMBLY

## 2.1 COMPONENT OVERVIEW

### **ESP8266 NodeMCU Pin Layout:**

```
                    ┌─────────┐
                    │  Micro  │
                    │   USB   │
                    └─────────┘
                         │
    ┌────────────────────┴────────────────────┐
    │                                         │
    │        ESP8266 NodeMCU v1.0             │
    │                                         │
    ├───┬───┬───┬───┬───┬───┬───┬───┬───┬───┤
    │A0 │RSV│RSV│SD3│SD2│SD1│CMD│SD0│CLK│GND│
    ├───┼───┼───┼───┼───┼───┼───┼───┼───┼───┤
    │3V3│EN │RST│GND│VIN│   │   │   │   │   │
    └───┴───┴───┴───┴───┘   └───────────────┘

Left Side (Top to Bottom):
┌────────┬─────┬──────────────────┐
│ Label  │ GPIO│ Function         │
├────────┼─────┼──────────────────┤
│ A0     │ ADC │ Analog Input     │
│ RSV    │  -  │ Reserved         │
│ RSV    │  -  │ Reserved         │
│ SD3    │ 10  │ Flash (don't use)│
│ SD2    │  9  │ Flash (don't use)│
│ SD1    │  8  │ Flash (don't use)│
│ CMD    │ 11  │ Flash (don't use)│
│ SD0    │  7  │ Flash (don't use)│
│ CLK    │  6  │ Flash (don't use)│
│ GND    │  -  │ Ground           │
└────────┴─────┴──────────────────┘

Right Side (Top to Bottom):
┌────────┬─────┬──────────────────┐
│ Label  │ GPIO│ Function         │
├────────┼─────┼──────────────────┤
│ D0     │ 16  │ GPIO (Wake)      │
│ D1     │  5  │ GPIO, SCL        │
│ D2     │  4  │ GPIO, SDA        │
│ D3     │  0  │ GPIO, Boot       │
│ D4     │  2  │ GPIO, Boot, LED  │
│ 3V3    │  -  │ 3.3V Output      │
│ GND    │  -  │ Ground           │
│ D5     │ 14  │ GPIO, SCK        │
│ D6     │ 12  │ GPIO, MISO       │
│ D7     │ 13  │ GPIO, MOSI       │
│ D8     │ 15  │ GPIO, Boot       │
│ RX     │  3  │ UART RX          │
│ TX     │  1  │ UART TX          │
│ GND    │  -  │ Ground           │
│ 3V3    │  -  │ 3.3V Output      │
└────────┴─────┴──────────────────┘
```

### **ILI9341 TFT Display Pin Layout:**

```
Display Back View:
┌─────────────────────────┐
│                         │
│   ILI9341 TFT Module    │
│   240 x 320 pixels      │
│                         │
│   [SD Card Slot]        │
│                         │
└─────────────────────────┘
         │││││││││││
         VVV VVV VVV
    Pin Headers (may vary by model):

Standard Pin Order (verify yours!):
┌─────┬──────────────────────────┐
│ Pin │ Function                 │
├─────┼──────────────────────────┤
│ VCC │ 3.3V Power              │
│ GND │ Ground                   │
│ CS  │ Chip Select (TFT)       │
│ RST │ Reset                    │
│ DC  │ Data/Command            │
│ SDI │ MOSI (SPI Data In)      │
│ SCK │ SPI Clock                │
│ LED │ Backlight (3.3V or PWM) │
│ SDO │ MISO (SPI Data Out)     │
└─────┴──────────────────────────┘

Touch Controller Pins (if separate):
┌─────┬──────────────────────────┐
│ Pin │ Function                 │
├─────┼──────────────────────────┤
│T_CLK│ Touch SPI Clock         │
│T_CS │ Touch Chip Select       │
│T_DIN│ Touch Data In (MOSI)    │
│T_DO │ Touch Data Out (MISO)   │
│T_IRQ│ Touch Interrupt (opt)   │
└─────┴──────────────────────────┘

CRITICAL: Verify your module's pinout!
Some modules have different pin orders!
Check the silkscreen labels on YOUR module!
```

### **TTP223 Capacitive Touch Sensor:**

```
Front View (sensor pad):
┌───────────────┐
│   ◉           │  ← Touch sensitive area
│               │
│  [ TTP223 ]   │
└───────────────┘

Back View (pins):
┌───────────────┐
│  VCC OUT GND  │  ← 3 pins
└───────────────┘

Pin Functions:
VCC = Power (3.3V or 5V)
OUT = Output (HIGH when touched)
GND = Ground

Jumper Settings (if present):
┌─────┬────────────────┐
│ A   │ Output Mode    │
├─────┼────────────────┤
│ Open│ Toggle (latch) │
│Short│ Direct (better)│
└─────┴────────────────┘

┌─────┬────────────────┐
│ B   │ Power          │
├─────┼────────────────┤
│ Open│ High power     │
│Short│ Low power      │
└─────┴────────────────┘

For our use: A=Short (direct mode)
```

### **MAX9814/MAX4466 Microphone Module:**

```
Front View:
┌───────────────┐
│   [MIC]       │  ← Microphone capsule
│               │
│  [ Gain Pot ] │  ← Optional gain adjustment
│               │
└───────────────┘

Pin Configuration:
┌─────┬──────────────────────┐
│ Pin │ Function             │
├─────┼──────────────────────┤
│ VCC │ Power (3.3V or 5V)  │
│ GND │ Ground               │
│ OUT │ Analog Audio Output  │
│ GAIN│ Gain Control (opt)   │
└─────┴──────────────────────┘

Output: 0-3.3V analog signal
Frequency: 20Hz - 20kHz
Gain: Adjustable via potentiometer
```

---

## 2.2 PIN MAPPING REFERENCE

### **COMPLETE PIN MAPPING TABLE:**

This is the EXACT wiring you will use:

```
╔══════════════════════════════════════════════════════════════╗
║                    COMPLETE PIN MAPPING                      ║
╠══════════════════════════════════════════════════════════════╣
║ ESP Pin  │ GPIO │ Connects To        │ Notes               ║
╠══════════╪══════╪════════════════════╪═════════════════════╣
║ 3.3V     │  -   │ TFT VCC            │ Power (3.3V!)      ║
║ 3.3V     │  -   │ Touch VCC          │ Shared power       ║
║ 3.3V     │  -   │ Touch Sensor L VCC │ Shared power       ║
║ 3.3V     │  -   │ Touch Sensor R VCC │ Shared power       ║
║ 3.3V or  │  -   │ Microphone VCC     │ Check module spec  ║
║ GND      │  -   │ All GNDs           │ Common ground      ║
║          │      │                    │                    ║
║ D0       │ 16   │ TFT RST            │ Reset pin          ║
║ D1       │ 5    │ TFT DC             │ Data/Command       ║
║ D2       │ 4    │ TFT CS             │ Chip Select        ║
║ D3       │ 0    │ Touch Sensor L OUT │ Left "ear"         ║
║ D4       │ 2    │ Touch Sensor R OUT │ Right "ear"        ║
║ D5       │ 14   │ TFT SCK            │ SPI Clock (shared) ║
║ D5       │ 14   │ Touch T_CLK        │ Shared with TFT    ║
║ D6       │ 12   │ TFT MISO           │ SPI Data (shared)  ║
║ D6       │ 12   │ Touch T_DO         │ Shared with TFT    ║
║ D7       │ 13   │ TFT MOSI           │ SPI Data (shared)  ║
║ D7       │ 13   │ Touch T_DIN        │ Shared with TFT    ║
║ D8       │ 15   │ Touch T_CS         │ Touch Chip Select  ║
║ A0       │ ADC  │ Microphone OUT     │ Analog audio       ║
║ RX       │ 3    │ [Available]        │ Future expansion   ║
║ TX       │ 1    │ [Available]        │ Future expansion   ║
╚══════════╧══════╧════════════════════╧═════════════════════╝
```

### **Component Connection Summary:**

**TFT Display (ILI9341):**
```
TFT Pin  →  ESP8266 Pin  →  GPIO
─────────────────────────────────
VCC      →  3.3V         →  -
GND      →  GND          →  -
SCK      →  D5           →  14
MOSI     →  D7           →  13
MISO     →  D6           →  12
CS       →  D2           →  4
DC       →  D1           →  5
RST      →  D0           →  16
LED      →  3.3V         →  - (always on)
```

**Touch Controller (XPT2046):**
```
Touch Pin →  ESP8266 Pin  →  GPIO
─────────────────────────────────
T_CLK    →  D5           →  14 (shared)
T_DIN    →  D7           →  13 (shared)
T_DO     →  D6           →  12 (shared)
T_CS     →  D8           →  15 (dedicated)
T_IRQ    →  (not used)   →  -
VCC      →  3.3V         →  -
GND      →  GND          →  -
```

**Touch Sensors (2x TTP223):**
```
Sensor       Pin  →  ESP8266 Pin  →  GPIO
──────────────────────────────────────────
Left Sensor:
  VCC        →  3.3V         →  -
  GND        →  GND          →  -
  OUT        →  D3           →  0

Right Sensor:
  VCC        →  3.3V         →  -
  GND        →  GND          →  -
  OUT        →  D4           →  2
```

**Microphone (MAX9814/MAX4466):**
```
Mic Pin  →  ESP8266 Pin  →  GPIO
─────────────────────────────────
VCC      →  3.3V or 5V   →  - (check module)
GND      →  GND          →  -
OUT      →  A0           →  ADC
```

---

## 2.3 STEP-BY-STEP WIRING

### **CRITICAL: Read Before Wiring!**

```
⚠️  DISCONNECT POWER before making any connections!
⚠️  Double-check every connection before applying power!
⚠️  Use the correct voltage (3.3V, NOT 5V for TFT)!
⚠️  Take photos of your wiring for reference!
```

### **Recommended Wiring Order:**

**STEP 1: Power Rails Setup (5 minutes)**

```
1. Place ESP8266 on breadboard
   - Orient with USB port facing you
   - Leave space on both sides for wires
   - Note which rows each pin occupies

2. Set up power rails:
   - Use breadboard power rails (+ and -)
   - Connect ESP 3.3V to (+) rail  [RED wire]
   - Connect ESP GND to (-) rail   [BLACK wire]
   
3. Verify:
   - 3.3V rail has voltage (use multimeter if available)
   - GND rail connected properly
```

**STEP 2: TFT Display - SPI Connections (10 minutes)**

```
Connect in this order:

1. D5 → TFT SCK  [YELLOW wire]
   - This is SPI clock
   - Verify: ESP D5 to display SCK pin
   
2. D7 → TFT MOSI [ORANGE wire]
   - This is SPI data out
   - Verify: ESP D7 to display SDI/MOSI pin
   
3. D6 → TFT MISO [BLUE wire]
   - This is SPI data in
   - Verify: ESP D6 to display SDO/MISO pin
   - Note: Some displays label this SDO
   
4. Verify SPI wiring:
   - Three wires: SCK, MOSI, MISO
   - All connected to correct pins
   - No loose connections
```

**STEP 3: TFT Display - Control Pins (5 minutes)**

```
Connect control pins:

1. D2 → TFT CS  [GREEN wire]
   - Chip select for TFT
   - Verify: ESP D2 (GPIO4) to TFT CS
   
2. D1 → TFT DC  [PURPLE wire]
   - Data/Command select
   - Verify: ESP D1 (GPIO5) to TFT DC/RS pin
   - Some displays label this RS
   
3. D0 → TFT RST [WHITE wire]
   - Reset pin
   - Verify: ESP D0 (GPIO16) to TFT RST/RESET
   
4. Take photo of these connections!
```

**STEP 4: TFT Display - Power (CRITICAL!) (3 minutes)**

```
⚠️  CRITICAL: 3.3V ONLY! NOT 5V!

1. 3.3V rail → TFT VCC [RED wire]
   - Connect breadboard (+) rail to TFT VCC
   - VERIFY voltage is 3.3V, NOT 5V!
   
2. GND rail → TFT GND [BLACK wire]
   - Connect breadboard (-) rail to TFT GND
   
3. 3.3V rail → TFT LED [RED wire]
   - Backlight always on
   - If TFT has separate LED pin, connect to 3.3V
   - If no LED pin, backlight may be internal
   
4. BEFORE POWERING ON:
   [ ] Verify VCC is 3.3V (NOT 5V!)
   [ ] Verify all GND connected
   [ ] Verify no short circuits
   [ ] Take photo of power connections
```

**STEP 5: Touch Controller (XPT2046) (5 minutes)**

```
Touch controller shares SPI bus with TFT:

1. Share these connections (already done):
   - D5 → T_CLK  (shared with TFT SCK)
   - D7 → T_DIN  (shared with TFT MOSI)
   - D6 → T_DO   (shared with TFT MISO)
   
2. Add dedicated chip select:
   - D8 → T_CS [BROWN wire]
   - Verify: ESP D8 (GPIO15) to Touch T_CS
   
3. Touch power (if separate):
   - If your touch has separate power pins:
     3.3V → T_VCC [RED wire]
     GND → T_GND  [BLACK wire]
   - Many displays have touch powered internally
   
4. T_IRQ:
   - Leave unconnected (we use polling mode)
   - If your module has it, just leave it empty
   
5. Verify:
   [ ] T_CS connected to D8
   [ ] SPI pins shared with TFT
   [ ] Power connected (if applicable)
```

**STEP 6: Capacitive Touch Sensors (10 minutes)**

```
Left Touch Sensor ("Left Ear"):

1. Identify pins on sensor:
   - Look for labels: VCC, OUT, GND
   - Usually in that order
   
2. Connect Left Sensor:
   - Sensor VCC → 3.3V rail [RED wire]
   - Sensor GND → GND rail  [BLACK wire]
   - Sensor OUT → D3        [YELLOW wire]
   
3. Verify Left Sensor:
   [ ] VCC to 3.3V
   [ ] GND to ground
   [ ] OUT to D3 (GPIO0)

Right Touch Sensor ("Right Ear"):

4. Connect Right Sensor:
   - Sensor VCC → 3.3V rail [RED wire]
   - Sensor GND → GND rail  [BLACK wire]
   - Sensor OUT → D4        [ORANGE wire]
   
5. Verify Right Sensor:
   [ ] VCC to 3.3V
   [ ] GND to ground
   [ ] OUT to D4 (GPIO2)

6. Jumper Configuration (if present):
   - Set jumper A to SHORT (direct mode)
   - Jumper B can stay OPEN
   
7. Physical Placement:
   - Position sensors where you can easily touch them
   - These act as "left ear" and "right ear"
   - Keep wires long enough to reach
```

**STEP 7: Microphone Module (5 minutes)**

```
1. Identify microphone pins:
   - Usually: VCC, GND, OUT
   - Check your specific module datasheet
   
2. Check voltage requirement:
   - MAX9814: Can use 3.3V or 5V
   - MAX4466: Usually 3.3V
   - Read your module's specs!
   
3. Connect Microphone:
   - Mic VCC → 3.3V rail [RED wire]
     (or 5V if module requires it - use ESP VIN pin)
   - Mic GND → GND rail  [BLACK wire]
   - Mic OUT → A0        [GRAY wire]
   
4. Gain adjustment (if present):
   - Small potentiometer on module
   - Start at middle position
   - We'll adjust this during testing
   
5. Verify:
   [ ] Power connected (check voltage!)
   [ ] GND connected
   [ ] OUT to A0 (analog pin)
   [ ] Gain pot at middle position
```

**STEP 8: Final Verification (10 minutes)**

```
Before powering on, verify EVERY connection:

TFT Display:
[ ] SCK  = D5  ✓
[ ] MOSI = D7  ✓
[ ] MISO = D6  ✓
[ ] CS   = D2  ✓
[ ] DC   = D1  ✓
[ ] RST  = D0  ✓
[ ] VCC  = 3.3V ✓ (NOT 5V!)
[ ] GND  = GND  ✓
[ ] LED  = 3.3V ✓

Touch Controller:
[ ] T_CLK = D5 (shared) ✓
[ ] T_DIN = D7 (shared) ✓
[ ] T_DO  = D6 (shared) ✓
[ ] T_CS  = D8 ✓
[ ] VCC   = 3.3V ✓
[ ] GND   = GND  ✓

Touch Sensors:
[ ] Left OUT  = D3  ✓
[ ] Right OUT = D4  ✓
[ ] Both VCC  = 3.3V ✓
[ ] Both GND  = GND  ✓

Microphone:
[ ] OUT = A0  ✓
[ ] VCC = 3.3V (or 5V if required) ✓
[ ] GND = GND ✓

Power Distribution:
[ ] All 3.3V from ESP 3.3V pin ✓
[ ] All GND connected together ✓
[ ] No short circuits visible ✓

Physical Check:
[ ] All wires firmly inserted ✓
[ ] No loose connections ✓
[ ] Components not touching each other ✓
[ ] Breadboard stable on desk ✓

Documentation:
[ ] Photos taken of all connections ✓
[ ] Pin numbers written down ✓
[ ] Ready to power on ✓
```

---

## 2.4 TESTING EACH COMPONENT

### **First Power-On (The Moment of Truth!)**

```
1. Connect USB cable to ESP8266
   - Use a quality USB cable (data + power)
   - Connect to computer USB port
   
2. Observe:
   - ESP8266 blue LED should blink briefly
   - TFT backlight should turn on (white screen)
   - No smoke, no burning smell!
   
3. If problems:
   - Immediately disconnect power
   - Check for shorts
   - Verify 3.3V connections
   - Review wiring

4. If success:
   - TFT backlight on = good!
   - ESP boots = good!
   - Ready for software testing
```

### **Testing TFT Display**

We'll use Arduino IDE test sketches:

```
1. Open Arduino IDE

2. Install TFT_eSPI library:
   - Tools → Manage Libraries
   - Search: "TFT_eSPI"
   - Install by Bodmer
   
3. Configure User_Setup.h:
   - Find library folder:
     Documents/Arduino/libraries/TFT_eSPI/
   - Open User_Setup.h
   - Set driver: #define ILI9341_DRIVER
   - Set pins to match our wiring:
     #define TFT_MISO 12
     #define TFT_MOSI 13
     #define TFT_SCLK 14
     #define TFT_CS   4
     #define TFT_DC   5
     #define TFT_RST  16
     
4. Upload test sketch:
   - File → Examples → TFT_eSPI → Test and diagnostics → Colour_Test
   - Select Board: NodeMCU 1.0 (ESP-12E Module)
   - Select Port: (your ESP's port)
   - Click Upload
   
5. Expected result:
   - Rainbow gradient bars
   - Text at top
   - Colors correct (not inverted)
   
6. If display shows:
   - Nothing: Check wiring, especially CS, DC, RST
   - Garbage: Check SPI pins (SCK, MOSI, MISO)
   - Wrong colors: Try TFT_RGB or TFT_BGR in User_Setup.h
   - Upside down: Change rotation in code
```

### **Testing Touch Screen**

```
1. Install XPT2046_Touchscreen library:
   - Tools → Manage Libraries
   - Search: "XPT2046"
   - Install by Paul Stoffregen
   
2. Upload touch test:
   - File → Examples → XPT2046_Touchscreen → TouchTest
   - Modify if needed:
     #define CS_PIN 15  // D8
     
3. Open Serial Monitor:
   - Tools → Serial Monitor
   - Set baud to 115200
   
4. Touch the screen:
   - Should see X, Y coordinates
   - Values change when touching different areas
   - Example output:
     X = 2450, Y = 1823
     
5. If not working:
   - Check T_CS connection to D8
   - Verify shared SPI pins
   - Some screens need pressure to register
```

### **Testing Capacitive Touch Sensors**

```
1. Upload test sketch:

void setup() {
  Serial.begin(115200);
  pinMode(0, INPUT);  // D3 - Left sensor
  pinMode(2, INPUT);  // D4 - Right sensor
}

void loop() {
  if (digitalRead(0) == HIGH) {
    Serial.println("LEFT TOUCHED!");
  }
  if (digitalRead(2) == HIGH) {
    Serial.println("RIGHT TOUCHED!");
  }
  delay(100);
}

2. Open Serial Monitor (115200 baud)

3. Touch each sensor:
   - Left sensor → "LEFT TOUCHED!"
   - Right sensor → "RIGHT TOUCHED!"
   
4. If always triggered:
   - Check jumper A is in SHORT position
   - Verify 3.3V (not 5V)
   
5. If never triggers:
   - Verify OUT pins connected
   - Check sensor power
   - Try adjusting sensitivity (if pot available)
```

### **Testing Microphone**

```
1. Upload test sketch:

void setup() {
  Serial.begin(115200);
  pinMode(A0, INPUT);
}

void loop() {
  int micValue = analogRead(A0);
  Serial.println(micValue);
  delay(50);
}

2. Open Serial Monitor

3. Make sounds near microphone:
   - Should see values change: 300-700 range typically
   - Silent: lower values
   - Loud: higher values
   
4. Adjust gain (if available):
   - Turn pot clockwise = more sensitive
   - Turn counter-clockwise = less sensitive
   - Find sweet spot where talking gives 400-600 range
   
5. If no response:
   - Check OUT connected to A0
   - Verify power connections
   - Check if module has enable pin
```

### **Testing WiFi**

```
1. Upload WiFi test:

#include <ESP8266WiFi.h>

const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.print("Connected! IP: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  // Nothing
}

2. Edit YOUR_WIFI_NAME and YOUR_WIFI_PASSWORD

3. Upload and check Serial Monitor:
   - Should connect and show IP address
   - Note this IP for later!
   
4. If fails:
   - Verify SSID/password correct
   - Must be 2.4GHz network (not 5GHz)
   - Check router allows ESP connections
```

### **Component Test Checklist**

```
HARDWARE TESTS:
[ ] TFT Display shows rainbow bars ✓
[ ] Touch screen responds to touches ✓
[ ] Left touch sensor triggers ✓
[ ] Right touch sensor triggers ✓
[ ] Microphone reads audio levels ✓
[ ] WiFi connects and gets IP ✓
[ ] All components powered correctly ✓
[ ] No overheating issues ✓

If all checked, hardware is READY! ✓
If any unchecked, troubleshoot that component before continuing.
```

---

## 2.5 COMMON WIRING MISTAKES

### **Problem: Display is blank/white**

**Possible Causes:**
```
1. Wrong voltage:
   - Check: VCC connected to 3.3V (NOT 5V!)
   - Fix: Move VCC wire to 3.3V pin
   
2. Wrong pins:
   - Check: CS, DC, RST on correct pins
   - Fix: Verify against pin mapping table
   
3. SPI pins swapped:
   - Check: MOSI and MISO not swapped
   - Fix: SCK=D5, MOSI=D7, MISO=D6
   
4. User_Setup.h not configured:
   - Check: ILI9341_DRIVER defined
   - Fix: Edit User_Setup.h with correct pins
```

**Debug Steps:**
```
1. Disconnect power
2. Verify each wire with multimeter (continuity mode)
3. Check User_Setup.h pin definitions match physical wiring
4. Re-upload Colour_Test example
5. If still blank, try different SPI frequency (lower)
```

### **Problem: Display shows garbage/random pixels**

**Possible Causes:**
```
1. Loose SPI connections:
   - Check: SCK, MOSI, MISO firmly connected
   - Fix: Reseat wires in breadboard
   
2. SPI frequency too high:
   - Check: SPI_FREQUENCY in User_Setup.h
   - Fix: Try 27MHz instead of 40MHz
   
3. Poor quality jumper wires:
   - Check: Wire quality, length
   - Fix: Use shorter, better quality wires
   
4. Power supply insufficient:
   - Check: USB cable quality
   - Fix: Use different USB port/cable
```

### **Problem: Colors are wrong/inverted**

**Possible Causes:**
```
1. Color order setting:
   - Check: TFT_RGB_ORDER in User_Setup.h
   - Fix: Try TFT_BGR instead of TFT_RGB (or vice versa)
   
2. Wrong driver:
   - Check: ILI9341_DRIVER defined
   - Fix: Verify your display is actually ILI9341
```

### **Problem: Touch doesn't work**

**Possible Causes:**
```
1. T_CS not connected:
   - Check: D8 connected to T_CS
   - Fix: Add this connection
   
2. Shared SPI pins missing:
   - Check: Touch shares D5, D6, D7 with TFT
   - Fix: Verify these connections
   
3. Need more pressure:
   - Check: Some screens need firm press
   - Fix: Press harder, or adjust touch sensitivity in code
   
4. Coordinates inverted:
   - Check: Touch rotation setting
   - Fix: Adjust ts.setRotation() in code
```

### **Problem: Capacitive sensors always triggered**

**Possible Causes:**
```
1. Toggle mode enabled:
   - Check: Jumper A position
   - Fix: Set to SHORT (direct mode)
   
2. Voltage too high:
   - Check: Connected to 3.3V (not 5V)
   - Fix: Move to 3.3V rail
   
3. Floating input:
   - Check: Proper ground connection
   - Fix: Verify GND connected
```

### **Problem: Microphone no response**

**Possible Causes:**
```
1. Gain too low:
   - Check: Gain potentiometer position
   - Fix: Turn clockwise to increase
   
2. Wrong pin:
   - Check: Connected to A0 (not digital pin)
   - Fix: Move to A0 pin
   
3. Module requires 5V:
   - Check: Module specifications
   - Fix: Connect VCC to ESP VIN (5V) instead of 3.3V
```

### **Problem: ESP won't boot / keeps resetting**

**Possible Causes:**
```
1. Boot pins in wrong state:
   - Check: GPIO0 (D3) and GPIO15 (D8) states
   - Fix: Disconnect touch sensor and T_CS during boot
   - Re-connect after boot completes
   
2. Insufficient power:
   - Check: USB cable quality
   - Fix: Use powered USB hub or better cable
   
3. Short circuit:
   - Check: No wires touching
   - Fix: Inspect breadboard carefully
```

### **Problem: WiFi won't connect**

**Possible Causes:**
```
1. Wrong SSID/password:
   - Check: Case-sensitive, spaces, special characters
   - Fix: Copy-paste from router settings
   
2. 5GHz network:
   - Check: ESP8266 only supports 2.4GHz
   - Fix: Connect to 2.4GHz network or enable 2.4GHz band
   
3. Router blocking:
   - Check: MAC filtering, firewall
   - Fix: Add ESP MAC to allowed devices
   
4. Weak signal:
   - Check: Distance from router
   - Fix: Move closer or use WiFi extender
```

---

**HARDWARE ASSEMBLY COMPLETE!** ✅

If all components test successfully, you're ready for Part 3: Arduino Setup.

Continue to next section...

---

# PART 3: ARDUINO SETUP

## 3.1 INSTALLING ARDUINO IDE

[Content continues with detailed Arduino installation, library setup, code upload instructions...]

---

**TO BE CONTINUED...**

This guide continues with:
- Complete Arduino IDE setup (every click documented)
- All library installations with screenshots
- Complete code with every line explained
- Python setup (virtual env, pip, packages)
- Getting every API key (step-by-step with images)
- Configuration files (what each line does)
- Feature documentation (how each page works)
- Customization guide (change anything)
- Troubleshooting (every error explained)
- Advanced topics (OTA, voice, custom animations)

**Total expected length: 50,000+ words, 200+ pages**

Would you like me to continue with the next sections?
