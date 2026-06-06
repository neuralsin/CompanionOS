# 🔌 COMPLETE WIRING DIAGRAM
## Spotify Desk Companion - All Connections

---

## 📋 **COMPONENT LIST**

- ESP8266 NodeMCU
- ILI9341 TFT Display (240x320) with XPT2046 Touch
- 2x Capacitive Touch Sensors (TTP223 or similar)
- Microphone Module (Analog or I2S)
- HC-SR04 Distance Sensor (to be added)
- Breadboard + Jumper Wires

---

## 🎨 **VISUAL WIRING DIAGRAM**

```
                           ┌─────────────────┐
                           │   ESP8266       │
                           │   NodeMCU       │
                           └────────┬────────┘
                                    │
        ┌───────────────┬───────────┼───────────┬───────────────┐
        │               │           │           │               │
        ↓               ↓           ↓           ↓               ↓
   ┌────────┐      ┌────────┐  ┌────────┐  ┌────────┐    ┌─────────┐
   │  TFT   │      │ Touch  │  │ Touch  │  │ Touch  │    │   Mic   │
   │Display │      │ Screen │  │Sensor L│  │Sensor R│    │ Module  │
   └────────┘      └────────┘  └────────┘  └────────┘    └─────────┘
```

---

## 📌 **PIN ASSIGNMENT TABLE**

```
┌──────────┬───────┬──────────────────────────────────────┐
│ NodeMCU  │ GPIO  │ Connection                           │
├──────────┼───────┼──────────────────────────────────────┤
│ 3.3V     │  -    │ TFT VCC, Touch VCC, Sensors VCC     │
│ GND      │  -    │ TFT GND, Touch GND, Sensors GND     │
│          │       │                                      │
│ D0       │ 16    │ TFT RST (Reset)                     │
│ D1       │ 5     │ TFT DC (Data/Command)               │
│ D2       │ 4     │ TFT CS (Chip Select)                │
│ D3       │ 0     │ TOUCH SENSOR LEFT I/O               │
│ D4       │ 2     │ TOUCH SENSOR RIGHT I/O              │
│ D5       │ 14    │ SPI CLK (shared: TFT + Touch)       │
│ D6       │ 12    │ SPI MISO (shared: TFT + Touch)      │
│ D7       │ 13    │ SPI MOSI (shared: TFT + Touch)      │
│ D8       │ 15    │ TOUCH SCREEN CS (XPT2046)           │
│          │       │                                      │
│ RX       │ 3     │ [AVAILABLE - Reserved for Serial]   │
│ TX       │ 1     │ [AVAILABLE - Reserved for Serial]   │
│ A0       │ ADC   │ MICROPHONE (if analog)              │
└──────────┴───────┴──────────────────────────────────────┘
```

---

## 🖥️ **1. TFT DISPLAY (ILI9341) CONNECTIONS**

```
TFT Pin      Wire Color     ESP8266 Pin     GPIO
─────────────────────────────────────────────────
VCC          Red            3.3V            -
GND          Black          GND             -
SCK          Yellow         D5              14
MOSI (SDI)   Orange         D7              13
MISO (SDO)   Blue           D6              12
CS           Green          D2              4
DC (RS)      Purple         D1              5
RST (RESET)  White          D0              16
LED (BL)     Red            3.3V            -
```

### **Important Notes:**
- **LED (Backlight):** Connect directly to 3.3V for always-on backlight
- **VCC:** Must be 3.3V (NOT 5V!)
- **MISO:** Optional if you're only writing to display, but recommended

---

## 👆 **2. TOUCH SCREEN (XPT2046) CONNECTIONS**

```
Touch Pin    Wire Color     ESP8266 Pin     GPIO     Notes
──────────────────────────────────────────────────────────
T_CLK        Yellow         D5              14       Shared with TFT
T_DIN        Orange         D7              13       Shared with TFT
T_DO         Blue           D6              12       Shared with TFT
T_CS         Brown          D8              15       DEDICATED
T_IRQ        -              Not connected   -        Optional interrupt
VCC          Red            3.3V            -
GND          Black          GND             -
```

### **Important Notes:**
- **Shared SPI Bus:** T_CLK, T_DIN, T_DO share same pins as TFT
- **T_CS:** MUST be on separate pin (D8/GPIO15)
- **T_IRQ:** Can be left unconnected (polling mode works fine)

---

## 🖐️ **3. CAPACITIVE TOUCH SENSORS (2x) CONNECTIONS**

### **Left Touch Sensor (TTP223 or similar)**

```
Sensor Pin   ESP8266 Pin     GPIO     Notes
────────────────────────────────────────────
VCC          3.3V            -        Can also use 5V
GND          GND             -
I/O          D3              0        Active HIGH when touched
```

### **Right Touch Sensor**

```
Sensor Pin   ESP8266 Pin     GPIO     Notes
────────────────────────────────────────────
VCC          3.3V            -        Can also use 5V
GND          GND             -
I/O          D4              2        Active HIGH when touched
```

### **Important Notes:**
- **TTP223 Toggle Mode:** Make sure jumper is set to **momentary mode** (not toggle)
- **Active State:** Most TTP223 modules output HIGH when touched
- **Debouncing:** Will be handled in software
- **Placement:** These will act as "left ear" and "right ear" touch points

---

## 🎤 **4. MICROPHONE MODULE CONNECTIONS**

### **Option A: Analog Microphone (MAX9814 / MAX4466)**

```
Mic Pin      ESP8266 Pin     GPIO     Notes
────────────────────────────────────────────
VCC          3.3V or 5V      -        Check module spec
GND          GND             -
OUT          A0              ADC      Analog audio signal
GAIN         -               -        Leave default or adjust pot
```

### **Option B: I2S Digital Microphone (INMP441)**

```
Mic Pin      ESP8266 Pin     GPIO     Notes
────────────────────────────────────────────
VCC          3.3V            -
GND          GND             -
WS (LRCK)    RX (GPIO3)      3        Word Select
SCK (BCLK)   TX (GPIO1)      1        Bit Clock
SD (DOUT)    (TBD)           ?        Serial Data
L/R          GND             -        Left channel
```

**⚠️ I2S NOTE:** Using RX/TX for I2S will disable Serial Monitor during operation!

### **Which Type Do You Have?**
- **MAX9814:** Analog with auto-gain
- **MAX4466:** Analog with manual gain pot
- **INMP441:** Digital I2S MEMS mic
- **Other:** Please specify model

---

## 📏 **5. HC-SR04 DISTANCE SENSOR (To Be Added)**

### **Recommended Pins (if using Serial Monitor):**

```
Option A (Disable Serial):
HC-SR04 Pin  ESP8266 Pin     GPIO     Notes
────────────────────────────────────────────
VCC          5V (VIN)        -        Needs 5V power
GND          GND             -
TRIG         RX (GPIO3)      3        Serial disabled
ECHO         TX (GPIO1)      1        Serial disabled
```

### **Option B (Keep Serial, no I2S mic):**

```
HC-SR04 Pin  ESP8266 Pin     GPIO     Notes
────────────────────────────────────────────
VCC          5V (VIN)        -        Needs 5V power
GND          GND             -
TRIG         RX (GPIO3)      3        If not using I2S
ECHO         TX (GPIO1)      1        If not using I2S
```

**⚠️ IMPORTANT:** HC-SR04 needs 5V power. ECHO pin outputs 5V which can damage ESP8266!

**ECHO Voltage Divider Required:**
```
HC-SR04 ECHO → 1kΩ resistor → ESP Pin
                  └→ 2kΩ resistor → GND
```
This creates 3.3V from 5V signal.

---

## 🔋 **POWER DISTRIBUTION**

```
Power Source: USB (5V)
    ↓
ESP8266 VIN (5V) ──→ HC-SR04 VCC (if added)
    ↓
ESP8266 3.3V Regulator
    ↓
    ├──→ TFT Display VCC
    ├──→ Touch Controller VCC
    ├──→ Touch Sensor Left VCC
    ├──→ Touch Sensor Right VCC
    └──→ Microphone VCC (if 3.3V)
```

### **Current Draw Estimate:**
```
ESP8266:       170 mA (WiFi active)
TFT Display:   100 mA (backlight on)
Touch Screen:  1 mA
Touch Sensors: 2 mA (both)
Microphone:    5 mA
HC-SR04:       15 mA
───────────────────────────
TOTAL:         ~293 mA
```

**USB provides 500mA, so plenty of headroom! ✓**

---

## ⚠️ **CRITICAL WARNINGS**

### **Boot Pin Concerns:**

**GPIO0 (D3):**
- Used for TOUCH SENSOR LEFT
- Must be HIGH during boot
- TTP223 outputs LOW when not touched → ✓ Safe
- If boot fails, disconnect touch sensor during upload

**GPIO2 (D4):**
- Used for TOUCH SENSOR RIGHT
- Must be HIGH during boot
- TTP223 outputs LOW when not touched → ✓ Safe
- If boot fails, disconnect touch sensor during upload

**GPIO15 (D8):**
- Used for TOUCH SCREEN CS
- Must be LOW during boot
- XPT2046 CS is normally HIGH → ⚠️ Risk
- **Solution:** Add 10kΩ pull-down resistor from D8 to GND

### **Voltage Levels:**
- **TFT Display:** 3.3V ONLY (NOT 5V!)
- **Touch Sensors:** Can handle 3.3V or 5V
- **HC-SR04:** Needs 5V power, ECHO needs voltage divider
- **ESP8266:** Operates at 3.3V logic

---

## ✅ **ASSEMBLY CHECKLIST**

```
[ ] TFT Display connected (7 wires + power)
[ ] Touch screen connected (T_CS to D8)
[ ] Left touch sensor connected (I/O to D3)
[ ] Right touch sensor connected (I/O to D4)
[ ] Microphone connected (OUT to A0 or I2S pins)
[ ] All VCC to 3.3V
[ ] All GND to GND
[ ] 10kΩ pull-down on D8 (optional but recommended)
[ ] Voltage divider on HC-SR04 ECHO (if adding distance sensor)
[ ] Double-check: NO 5V to TFT display!
[ ] Verify pin numbers match User_Setup.h
```

---

## 🧪 **TESTING SEQUENCE**

### **1. Display Test (Upload TFT_eSPI examples first)**
```
Arduino IDE → Examples → TFT_eSPI → Test and Diagnostics → Colour_Test
Should show: Rainbow gradient bars
```

### **2. Touch Test**
```
Arduino IDE → Examples → XPT2046_Touchscreen → TouchTest
Touch screen, Serial Monitor shows coordinates
```

### **3. Capacitive Touch Test**
```cpp
void loop() {
  if (digitalRead(0) == HIGH) Serial.println("LEFT TOUCHED");
  if (digitalRead(2) == HIGH) Serial.println("RIGHT TOUCHED");
  delay(100);
}
```

### **4. Microphone Test**
```cpp
void loop() {
  int level = analogRead(A0);
  Serial.println(level);  // Should vary with sound
  delay(50);
}
```

---

## 📝 **TROUBLESHOOTING**

### **Display shows nothing:**
- Check TFT_CS, TFT_DC, TFT_RST connections
- Verify 3.3V power (NOT 5V!)
- Confirm User_Setup.h pin definitions
- Try lower SPI frequency (27 MHz)

### **Touch not working:**
- Check T_CS connection (D8)
- Verify XPT2046_Touchscreen library installed
- Run calibration example
- Adjust touch mapping in code

### **Boot fails / won't upload:**
- Disconnect D3 (GPIO0) during upload
- Disconnect D8 (GPIO15) during upload
- Hold FLASH button while uploading
- Check for shorts on boot pins

### **Touch sensors always triggered:**
- Check if they're in toggle mode (should be momentary)
- Verify 3.3V power (excessive voltage can cause issues)
- Add 100nF capacitor between I/O and GND for stability

---

## 🎯 **NEXT STEPS**

Once wiring is confirmed:
1. Upload TFT test sketch → Verify display works
2. Upload touch test sketch → Verify touch works
3. Test capacitive sensors → Verify left/right work
4. Test microphone → Verify audio levels
5. Upload complete Companion OS code

---

**Wiring complete? Let's build the software!** 🚀
