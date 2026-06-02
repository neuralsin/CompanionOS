# COMPANION OS v7.0 — FULL ENGINEERING ENHANCEMENT PROMPT
### For: Claude Code (VS Code) | Target: ESP32 38-pin DevKit V1 (Primary) + ESP8266 (Secondary)

---

## ⚠️ READ THIS ENTIRE DOCUMENT BEFORE WRITING A SINGLE LINE OF CODE

You are continuing development on **Companion OS v6.0**, a deeply integrated embedded desk companion running on ESP32/ESP8266 microcontrollers with a TFT display, Python PC bridge daemon, and multiple cloud integrations. A full software audit of the existing codebase is below. You must read and deeply internalize it before starting any task.

---

## SECTION 0 — FULL CODEBASE AUDIT SUMMARY

### Architecture
- **ESP MCU** (C++/Arduino): Renders UI on TFT, handles touch, animates eyes, receives data via UDP
- **Python PC Bridge** (`companion_controller.py`): Scrapes APIs (Spotify, Steam, Weather, Stocks, Google Cal, GitHub), sends data over UDP/8888 to ESP, listens on UDP/8889 for touch commands from ESP
- **Communication**: UDP binary (RGB565 image chunks prefixed `0xFE`) + UTF-8 JSON command strings
- **Display**: SPI TFT, double-buffered sprite rendering (TFT_eSPI library)
- **Pages**: 12 dashboard states in `AppState` enum — EYES, SPOTIFY, POMODORO, WEATHER, NOTIFICATIONS, NOTES, STOCKS, GAMING, SOCIAL, PRODUCTIVITY, NETWORK, SETTINGS

### Arduino File Map
| File | Role |
|---|---|
| `CompanionOS_Main.ino` | Entry point: `setup()`, `loop()`, boot diagnostics, 50ms/1s/2s/60s timers |
| `config.h` | ALL hardware pins, WiFi creds, colors, feature flags, screen dims |
| `globals.h` | All shared global variables, enums (`AppState`, `Emotion`), TFT/touch/UDP objects, 9216-byte `albumArt[]` buffer |
| `network.h` | WiFi setup, NTP (IST UTC+5:30), UDP tx/rx, `handleCommand()` string parser, binary `0xFE` image stream renderer |
| `eyes.h` | Theme1 eye engine: almond eyes, spring physics pupil tracking, starfield bg, 8 emotion presets, Zzz animation |
| `theme2_eyes.h` | Theme2 eye engine: 12-field `T2EyeConfig`, 18 expression presets, `T2EyeTransition/Variation/Blink` pipeline, double-buffered |
| `touch.h` | `handleTouch()` gesture processor, `checkPhysicalSensors()` for cap touch pins D3/D4 |
| `ui.h` | Status bar, loading screen, page dots, `drawAgentOverlay()`, `drawButton()` |
| `pages.h` | All page orchestration: `renderCurrentPage()`, `changePage()`, all `redraw*Partial()` wrappers |
| `page_stocks.h` | Sparkline chart, stock price cards, watchlist sidebar |
| `page_gaming.h` | Steam dashboard: session timer, achievement bar, friend count, cover art |
| `page_social.h` | Social notification cards, wrapped text renderer, avatar circles |
| `page_productivity.h` | Dot-matrix clock, task list, timeline bar |
| `page_network.h` | RSSI arc gauge, signal bars, IP/MAC info cards |
| `theme2_spotify.h` | Alt Spotify layout: full-screen art, circular progress arc, volume slider, lyrics sidebar |

### Python File Map
| File | Role |
|---|---|
| `companion_controller.py` | Master daemon: spawns all threads, main UDP loop, discovery handshake, `command_listener()`, `fetch_heavy_assets()` |
| `spotify_integration.py` | SpotifyOAuth, `get_current_track()`, `control_playback()`, `get_lyrics()` (LRCLib), `process_album_art()` → RGB565 array |
| `steam_tracker.py` | GPU DLL scanning, `detect_local_game()`, `get_achievements()`, `get_friends_online()`, `get_game_cover_url()` — **CURRENTLY BROKEN** |
| `stock_manager.py` | `StockManager` + `StockManagerGUI` (tkinter), yfinance live quotes, sparkline data, UDP push |
| `google_cal.py` | OAuth2 Google Calendar, `LocalTaskList` fallback, task progress % |
| `github_integration.py` | GitHub REST profile fetch |
| `theme2_bridge.py` | Theme2 Spotify volume/shuffle/device daemon |

### Critical Data Flows
- **Image rendering**: PC packs 96×96 RGB565 art → splits into chunks → prefixes byte `0xFE` + 2-byte index → UDP → ESP `network.h` binary sniffer assembles into `albumArt[]` → `completeAlbumArt()` triggers page redraw
- **Touch→Command**: ESP touch.h detects tap → `sendCommand("STOCK:SET:AAPL")` → UDP/8889 → Python `command_listener()` parses and acts
- **NTP**: IST offset 19800s, re-syncs every 5min, clock drift tracked natively between syncs
- **EEPROM addr 2**: Theme preference (0=Theme1, 1=Theme2)

### Known Bugs & Constraints
- `steam_tracker.py` broken — GPU DLL scan + Steam API not delivering live cover art
- `TOUCH_LEFT` (GPIO0/D3) and `TOUCH_RIGHT` (GPIO2/D4) cap sensors conflict with boot mode on ESP8266 and are being removed
- ESP8266 RAM near limit (9216-byte static `albumArt[]` + sprite buffers)
- WiFi hardcoded in `config.h` (SSID/pass)
- `capture_windows_notifications()` is Windows-only PowerShell

---

## SECTION 1 — PRIMARY TARGET HARDWARE: ESP32 38-PIN DEVKIT V1

All new development is **primarily optimized for ESP32 (38-pin DevKit V1)**. ESP8266 support is preserved via compile-time detection.

### ESP32 38-Pin Display: ST7735R, 1.8", 128×160px (NO touchscreen)
```
Driver IC : ST7735R
Interface : 4-wire SPI
Resolution: 128 × 160 px (portrait) — ALL UI must be redesigned for this resolution
```

### ESP32 Pin Mapping (create this as `config_esp32.h`)
```cpp
// === ESP32 ST7735R (1.8" 128x160, no touch) ===
#define TFT_CS     5     // GPIO5
#define TFT_DC     2     // GPIO2
#define TFT_RST    4     // GPIO4
#define TFT_MOSI  23     // GPIO23 (VSPI MOSI)
#define TFT_SCLK  18     // GPIO18 (VSPI SCK)
#define TFT_MISO  19     // GPIO19 (VSPI MISO) — not needed for write-only
#define TFT_BL    32     // GPIO32 PWM backlight (optional)

// NO XPT2046 touchscreen on ESP32 variant
// Navigation done via BLE commands + physical buttons

// Physical Navigation Buttons (active LOW, internal pullup)
#define BTN_LEFT    34   // GPIO34 — prev page / back
#define BTN_RIGHT   35   // GPIO35 — next page / enter
#define BTN_SELECT  36   // GPIO36 — select / confirm

// LDR ambient light
#define LDR_PIN     39   // GPIO39 ADC1 (input only)

// Bluetooth / WiFi: built-in, no extra pins

// Screen dimensions for ST7735R
#define SCREEN_W   128
#define SCREEN_H   160

// TFT_eSPI user setup for ST7735R
// In User_Setup.h: #define ST7735_DRIVER, CGRAM_OFFSET
// Init sequence: Initr_BLACKTAB or Initr_18GREENTAB depending on your module
```

### ESP8266 Pin Mapping (preserved in `config_esp8266.h`)
```cpp
// === ESP8266 ILI9341 2.8" 320x240 with XPT2046 touch ===
#define TOUCH_CS     15  // D8 / GPIO15
// TOUCH_LEFT and TOUCH_RIGHT REMOVED — pins freed
#define MIC_PIN      A0
#define SCREEN_W    320
#define SCREEN_H    240
// TFT SPI: MISO=D6, MOSI=D7, SCK=D5, CS=D2, DC=D1, RST=D0
```

---

## SECTION 2 — HARDWARE AUTO-DETECTION SYSTEM

### Task 2.1: Create `config.h` as Smart Router
Replace the current `config.h` with a hardware-detection router:
```cpp
// config.h — Hardware auto-router
#pragma once

#ifdef ESP32
  #include "config_esp32.h"
  #define PLATFORM_NAME "ESP32"
  #define HAS_TOUCH false
  #define HAS_BLUETOOTH true
  #define DISPLAY_DRIVER_ST7735R true
#elif defined(ESP8266)
  #include "config_esp8266.h"
  #define PLATFORM_NAME "ESP8266"
  #define HAS_TOUCH true
  #define HAS_BLUETOOTH false
  #define DISPLAY_DRIVER_ILI9341 true
#else
  #error "Unknown hardware platform. Define ESP32 or ESP8266."
#endif

// Shared config
#define WIFI_SSID        "your_ssid"
#define WIFI_PASS        "your_password"
#define DEFAULT_PC_IP    "192.168.1.100"
#define UDP_PORT_RX      8888
#define UDP_PORT_TX      8889
#define NTP_SERVER       "pool.ntp.org"
#define NTP_OFFSET       19800   // IST UTC+5:30
#define EEPROM_THEME_ADDR 2
```

### Task 2.2: Resolution-Aware UI Macros
All drawing code must use these macros instead of hardcoded 320/240:
```cpp
// In globals.h — resolution-aware layout helpers
#define SCR_W       SCREEN_W
#define SCR_H       SCREEN_H
#define SCR_CX      (SCREEN_W / 2)
#define SCR_CY      (SCREEN_H / 2)
#define SCALE_X(v)  ((v) * SCREEN_W / 320)   // normalize from ESP8266 reference
#define SCALE_Y(v)  ((v) * SCREEN_H / 240)
```

### Task 2.3: Conditional Compilation Guards
Wrap ALL touch-specific code in `#if HAS_TOUCH`:
```cpp
#if HAS_TOUCH
  #include <XPT2046_Touchscreen.h>
  XPT2046_Touchscreen ts(TOUCH_CS);
#endif
```
Wrap ALL ESP32 BLE code in `#ifdef ESP32`.
Wrap ALL ESP8266 networking in `#ifdef ESP8266`.

### Task 2.4: `setup()` Platform Detection Log
In `runBootSequence()`, display PLATFORM_NAME, display driver, touch availability, BT availability, and all configured pin assignments on screen for 3 seconds during boot.

---

## SECTION 3 — DR. HACK MODE

### Overview
Add a new full-featured "Dr. Hack" mode accessible from a dedicated tile on the main eye/home screen. This implements the hacking/network tools from the project files in folder `hack update files/` (based on https://github.com/pepeangell5/ESP32-TOOLS-PRO-480x320-V2.0, adapted for ST7735R 128×160).

**Dr. Hack is ESP32-only** (`#ifdef ESP32` guard on everything).

### Task 3.1: Add `STATE_DR_HACK` to `AppState` Enum
In `globals.h`:
```cpp
enum AppState {
  STATE_EYES, STATE_SPOTIFY, STATE_POMODORO, STATE_WEATHER,
  STATE_NOTIFICATIONS, STATE_NOTES, STATE_STOCKS, STATE_GAMING,
  STATE_SOCIAL, STATE_PRODUCTIVITY, STATE_NETWORK, STATE_SETTINGS,
  STATE_DR_HACK   // NEW — ESP32 only
};
```

### Task 3.2: Dr. Hack Entry Tile on Eyes Page
On the **eyes page**, add a small glowing tile in the **bottom-right corner** of the display (20×16px on 128×160):
- Icon: skull + circuit pattern (SVG-style pixel art drawn with TFT lines)
- Label: "DR.HACK" in tiny red font
- Glows with a subtle red pulse animation (cycle every 2s)
- Tap (or BTN_SELECT while cursor on it) → transition to `STATE_DR_HACK`
- Entry animation: red scanline sweep down the screen

### Task 3.3: Dr. Hack Sub-Menu System
Create `page_dr_hack.h`. This page has its own internal sub-state machine:
```cpp
enum DrHackSubState {
  DH_MENU,          // Main tool selector grid
  DH_WIFI_SCANNER,  // Scan nearby APs, RSSI, channel, encryption
  DH_PORT_SCANNER,  // TCP port scan target IP
  DH_BEACON_SPAM,   // Broadcast fake AP names
  DH_DEAUTH,        // Deauth frames (ESP32 only, legal on your own network)
  DH_PACKET_MONITOR,// Raw 802.11 packet counter by type
  DH_BT_SCANNER,    // BLE device scanner
  DH_INFO           // System info: chip ID, MAC, free heap, CPU freq
};
```

### Task 3.4: Dr. Hack UI Layout (128×160)
- **Header bar** (top 14px): Red gradient bar, "DR.HACK" title left, battery/signal right
- **Tool grid** (main area): 2×4 grid of tool tiles, each 58×30px with icon + label
- **Exit button** (bottom 14px): Green pill button labeled "EXIT" — returns to `STATE_EYES` with reverse scanline animation
- **Back button** within sub-tools: always top-left corner "<" arrow

### Task 3.5: Dr. Hack Navigation (No Touch on ESP32)
- `BTN_LEFT` (GPIO34): navigate cursor left/up in grid
- `BTN_RIGHT` (GPIO35): navigate cursor right/down in grid  
- `BTN_SELECT` (GPIO36): enter selected tool or confirm action
- Long-press `BTN_LEFT` (800ms): always exits to `DH_MENU`
- Long-press `BTN_SELECT` (1500ms): exits Dr. Hack entirely → `STATE_EYES`
- Cursor is a red highlight border on the selected tile

### Task 3.6: Integrate `hack update files/`
Read all files from `hack update files/` folder. For each tool:
- Adapt ESP32-TOOLS-PRO source for 128×160 (original was 480×320 — scale all coords by 128/480, 160/320)
- Strip any TFT_eSPI calls that reference the original display size
- Re-implement drawing routines using `SCALE_X()` / `SCALE_Y()` macros
- Preserve all core WiFi/BLE scanning logic verbatim — only replace UI layer

### Task 3.7: Dr. Hack Python Bridge Extension
In `companion_controller.py`, add a new command handler in `command_listener()`:
```python
elif msg.startswith("HACK:"):
    # HACK:SCAN_WIFI → trigger PC-side network scan, push results back
    # HACK:PORT_SCAN:192.168.1.1 → run python socket port scan
    # HACK:BT_SCAN → trigger PC bluetoothctl/pybluez scan
    handle_hack_command(msg[5:])
```

---

## SECTION 4 — THOUGHT BUBBLE ENGINE

### Overview
The companion should have ambient "thoughts" — small aesthetic speech bubbles that appear above the right eye, triggered by contextual data (time, weather, last played song), appearing randomly at most once per hour, staying 2 minutes, then fading out gracefully.

### Task 4.1: Create `thought_engine.h`
```cpp
// thought_engine.h
// Manages ambient thought bubble generation and rendering

#define THOUGHT_MIN_INTERVAL_MS  (45 * 60 * 1000UL)  // minimum 45 min between thoughts
#define THOUGHT_MAX_INTERVAL_MS  (90 * 60 * 1000UL)  // max 90 min (random in range)
#define THOUGHT_DISPLAY_MS       (2 * 60 * 1000UL)   // visible for 2 minutes
#define THOUGHT_FADE_STEPS       20                   // fade in/out over 20 frames

struct ThoughtBubble {
  char text[80];           // thought text (max 80 chars)
  bool active;             // currently displayed
  unsigned long shownAt;   // millis() when shown
  uint8_t fadeAlpha;       // 0–255 opacity
  bool fadingIn;
  bool fadingOut;
};

extern ThoughtBubble activeBubble;
```

### Task 4.2: Thought Generation Logic
```cpp
void generateThought() {
  // Context-aware thought selector. Uses:
  // - displayHour, displayMinute (time context)
  // - weatherTemp, weatherDesc (from globals)
  // - trackTitle, trackArtist (last played Spotify track)
  // - currentEmotion (pet mood)
  
  // Time-based thoughts (20% weight)
  // Weather-based thoughts (25% weight)  
  // Music-based thoughts (35% weight)
  // Random ambient thoughts (20% weight)
}
```

Implement a rich thought bank with 60+ entries across categories:

**Time-based (examples):**
- 03:00 → `"who's awake at 3am... us."` 
- Morning → `"coffee first, questions later"`
- Late night → `"the best ideas hit at midnight"`

**Weather-based (examples):**
- Rain → `"rain = lofi music mandatory"`
- Hot → `"${temp}°C... melting fr"`
- Clear + night → `"good night for stargazing"`

**Music-based (examples):**
- Track playing → `"${trackArtist} really said that"`
- No track → `"silence is also a vibe"`
- Track title contains "love" → `"caught feelings again"`

**Ambient (examples):**
- `"just vibing"`
- `"processing..."`
- `"404: nap not found"`
- `"why is the sky blue tho"`

### Task 4.3: Thought Bubble Visual Design
The bubble appears **above the right eye**, offset to not overlap pupils.

Layout (ESP8266 320×240):
```
Position: x=180, y=18 to y=70 (above right eye region, EYE_Y=110)
Bubble width: dynamic, max 120px
Bubble height: auto based on text lines (max 3 lines)

Visual elements:
1. Rounded rectangle body — semi-transparent dark background (COLOR_BG blended 80%)
2. Thin glowing border (COLOR_ACCENT, 1px)
3. Small triangle "tail" pointing down-left toward right eye
4. Tiny thought dots (3 circles, size 2/3/4px) below tail, leading toward eye
5. Text: white, tiny font (size 1), word-wrapped
6. Subtle inner glow on border (2px outer ring at 30% opacity)
```

For ESP32 128×160, scale all coordinates proportionally.

### Task 4.4: Fade In/Out Animation
- **Fade in**: 20 frames over 1 second — interpolate border alpha and text alpha from 0→255
- **Display**: Static for `THOUGHT_DISPLAY_MS` (2 min)
- **Fade out**: 20 frames — interpolate 255→0, then `activeBubble.active = false`
- Rendering happens in `updateEyes()` / `t2_updateEyes()` every 50ms tick
- Use `tft.drawRoundRect()` with color blending via `blendColor()` already in `eyes.h`

### Task 4.5: Thought Scheduler in `loop()`
```cpp
// In loop() — add thought scheduling timer
static unsigned long nextThoughtTime = 0;
if (!activeBubble.active && millis() > nextThoughtTime && currentState == STATE_EYES) {
  generateThought();
  activeBubble.active = true;
  activeBubble.shownAt = millis();
  activeBubble.fadingIn = true;
  // Schedule next thought: random between 45–90 min from now
  nextThoughtTime = millis() + random(THOUGHT_MIN_INTERVAL_MS, THOUGHT_MAX_INTERVAL_MS);
}
```

### Task 4.6: Python-Side Thought Triggers
In `companion_controller.py`, add a new UDP command prefix `"THOUGHT:"` that the PC can push to the ESP with a fully pre-generated thought string (e.g., triggered when Spotify track changes or weather updates). The ESP stores this as an override thought that fires on the next scheduled slot.

---

## SECTION 5 — GAMING DASHBOARD OVERHAUL

### Problem
`steam_tracker.py` is broken. `detect_local_game()` uses fragile GPU DLL scanning. Game cover art delivery is unreliable. The ESP-side `page_gaming.h` has no data to render.

### Task 5.1: Rewrite `steam_tracker.py` with Triple-Detection
Replace the current broken approach with a 3-tier waterfall:

**Tier 1 — Steam Web API (most reliable if API key set):**
```python
def get_steam_current_game(steam_id, api_key):
    # GET https://api.steampowered.com/ISteamUser/GetPlayerSummaries/v2/
    # Returns gameid, gameextrainfo (game name) if user is playing
    # This is the ground truth — use if available
```

**Tier 2 — Windows process scan via psutil (no API key needed):**
```python
def detect_game_via_process():
    # Iterate psutil.process_iter(['pid', 'name', 'exe', 'cpu_percent'])
    # Filter: cpu_percent > 5%, not in SYSTEM_PROCESSES blocklist
    # Check loaded DLLs via ctypes.windll.psapi.EnumProcessModulesEx
    # Look for: d3d11.dll, d3d12.dll, vulkan-1.dll, opengl32.dll
    # Use exe path to derive game title via _title_from_exe_path()
```

**Tier 3 — Xbox/Game Bar via Windows Registry:**
```python
def get_xbox_game_bar_status():
    # Read HKEY_CURRENT_USER\Software\Microsoft\GameBar
    # Key: LastTitleId — contains app ID of last focused game
    # Also check: HKCU\System\GameConfigStore\Children for registered games
    import winreg
    # Fallback: check Windows gaming overlay active process
```

### Task 5.2: Game Cover Art — Fix `push_game_cover()`
```python
def fetch_game_cover(title, appid=None):
    """3-source waterfall for game cover art"""
    
    # Source 1: Steam CDN (if appid known)
    if appid:
        url = f"https://cdn.cloudflare.steamstatic.com/steam/apps/{appid}/header.jpg"
        # 460x215px — crop and resize to 96x56
    
    # Source 2: Steam store search
    url = f"https://store.steampowered.com/api/storesearch/?term={title}&l=en&cc=IN"
    # Parse 'items' array, get first match's tiny_image
    
    # Source 3: IGDB via Twitch API (best quality fallback)
    # POST https://api.igdb.com/v4/covers with Bearer token
    # Resize result to 96x56
    
    # Source 4: Last resort — generate a colored title card
    # Create 96x56 PIL image, gradient bg, title text, game controller icon
    return rgb565_array  # packed for UDP stream
```

### Task 5.3: Session Timer Fix
```python
class GameSession:
    def __init__(self):
        self.game_name = None
        self.start_time = None
        self.appid = None
    
    def update(self, detected_game, appid=None):
        if detected_game != self.game_name:
            self.game_name = detected_game
            self.start_time = time.time() if detected_game else None
            self.appid = appid
    
    def get_session_string(self):
        if not self.start_time:
            return "Not playing"
        elapsed = int(time.time() - self.start_time)
        h, m = divmod(elapsed // 60, 60)
        return f"{h}h {m:02d}m" if h else f"{m}m"
```

### Task 5.4: ESP-Side `page_gaming.h` Enhancement
Redesign the gaming page with these visual improvements:

**Layout (ESP8266 320×240):**
```
Top strip (0–55px): Game cover art (96×56), right of it: game title (bold),
                    session time with pulsing green "LIVE" dot,
                    achievement % bar
                    
Middle card (60–140px): 3-column stats grid:
  [ACHIEVEMENTS: 47%]  [SESSION: 1h 23m]  [FRIENDS: 3 online]

Bottom (145–240px): Last 3 played games list (mini covers + names + playtime)

Overlay elements:
  - "NOW PLAYING" badge top-right (animated pulse)
  - Xbox/Steam logo watermark (tiny, top-left corner of art)
  - Background: game-cover dominant color extracted + darkened gradient
```

**Dominant Color Extraction**: When cover art is received in `albumArt[]`, sample the center 4×4 pixel region, average the RGB565 values, and use that as the page background tint. This makes each game feel unique.

### Task 5.5: Python `gaming_feed_loop()` Overhaul
```python
def gaming_feed_loop():
    session = GameSession()
    while True:
        state = get_gaming_state_v2()  # new combined function
        
        if state['is_playing']:
            if state['title'] != session.game_name:
                # Game changed — push new cover art
                cover_data = fetch_game_cover(state['title'], state.get('appid'))
                send_game_cover_binary(cover_data)  # existing binary stream
                
                # Push thought to ESP about new game
                send_udp(f"THOUGHT:just booted up {state['title']}")
            
            session.update(state['title'], state.get('appid'))
            
            payload = {
                "title": state['title'][:20],
                "session": session.get_session_string(),
                "achievements": state.get('achievements', 0),
                "friends": state.get('friends_online', 0),
                "appid": state.get('appid', 0)
            }
            send_udp("GAMING:" + json.dumps(payload))
        
        time.sleep(8)
```

---

## SECTION 6 — SLIDESHOW ENHANCEMENT

### Current State
The gallery/slideshow tile displays uploaded images via the `/api/gallery` Flask endpoint. Images are pushed as binary RGB565 chunks to `albumArt[]`. The slideshow page has basic rendering.

### Task 6.1: Modern Slideshow UI in `pages.h`
Redesign the slideshow page (call it `redrawGalleryPartial()`):

**Cinematic Frame Treatment:**
```cpp
void drawCinematicFrame(uint16_t* imgData, int imgW, int imgH) {
  // 1. Draw letterbox bars top and bottom (pure black, 20px each)
  // 2. Apply vignette: darken corners using radial gradient blend
  //    (sample edge pixels, blend toward black over 15px inward)
  // 3. Draw image in center with 2px white border
  // 4. Subtle drop shadow (draw offset rectangle in dark gray before image)
}
```

**Metadata Overlay (bottom-left):**
```cpp
// Semi-transparent pill badge showing:
// - Image number "3 / 12" 
// - Filename (truncated to 16 chars)
// - Tiny calendar icon + date uploaded (if metadata available)
```

**Transition Animations (implement all 4, cycle randomly):**
```cpp
enum SlideTransition { SLIDE_LEFT, SLIDE_FADE, WIPE_DOWN, ZOOM_IN };

void animateTransition(uint16_t* oldImg, uint16_t* newImg, SlideTransition type) {
  // SLIDE_LEFT: scroll old image left, new enters from right — 16 steps, 20ms each
  // SLIDE_FADE: cross-dissolve via blendColor() per-pixel — 32 steps
  // WIPE_DOWN: horizontal wipe bar reveals new image — 24 steps
  // ZOOM_IN: scale new image from 50% center outward — 16 steps (use sprite scaling)
}
```

**Ken Burns Effect (auto-play):**
```cpp
// When slideshow auto-advances (5s interval):
// Slowly pan + scale the current image over 5 seconds before transition
// Implemented as: increment a pan offset (0–8px) and scale factor (1.0→1.1)
// Requires drawing sub-region of albumArt[] buffer
```

**Progress Bar:**
- Thin 2px line at very bottom of screen
- Fills from left to right over the 5-second display time
- Color matches the dominant color of the current image

**Navigation UI:**
- Left/right chevron arrows on sides (semi-transparent, appear on touch/button press)
- Dot indicators at bottom (current slide highlighted)

---

## SECTION 7 — REMOVE TOUCH LEFT/RIGHT, ADD BUTTON NAVIGATION

### Task 7.1: Remove Cap Touch Pins Entirely
From `config_esp8266.h`: **DELETE** `TOUCH_LEFT` and `TOUCH_RIGHT` definitions.
From `touch.h`: **DELETE** `checkPhysicalSensors()` function entirely.
From `CompanionOS_Main.ino`: Remove `pinMode(TOUCH_LEFT, INPUT_PULLUP)` and `pinMode(TOUCH_RIGHT, INPUT_PULLUP)`.
From `loop()`: Remove the `checkPhysicalSensors()` call.

The freed GPIOs (GPIO0/D3 and GPIO2/D4 on ESP8266) are now available. Document them as "RESERVED — available for future expansion" in `config_esp8266.h`.

### Task 7.2: ESP32 Physical Button Navigation
In `config_esp32.h`, buttons are at GPIO34/35/36 (defined in Section 1).

Create `buttons.h` (ESP32-only):
```cpp
#ifdef ESP32
// buttons.h — Physical button handler for ESP32 (no touch screen)
#define BTN_DEBOUNCE_MS  50
#define BTN_LONG_PRESS_MS 800

struct ButtonState {
  uint8_t pin;
  bool pressed;
  bool longPressed;
  unsigned long pressedAt;
  bool handled;
};

extern ButtonState btnLeft, btnRight, btnSelect;

void initButtons();
void handleButtons();
// handleButtons() replaces handleTouch() for ESP32
// Maps:
//   btnLeft short → changePage(-1)
//   btnRight short → changePage(+1)
//   btnSelect short → enter/confirm on current page
//   btnLeft long → go to STATE_EYES (home)
//   btnRight long → go to STATE_SETTINGS
//   btnSelect long → cycle through next emotion
#endif
```

---

## SECTION 8 — ESP32 WIFI + BLUETOOTH DUAL STACK

### Architecture
ESP32 runs **WiFi as primary** and **Classic BT Serial + BLE as backup/secondary**:
- **WiFi**: all existing UDP functionality, exact same protocol
- **Bluetooth Classic (SPP)**: fallback transport — same command strings over BT Serial
- **BLE**: low-energy connection for phone control (navigation commands, thought triggers)

### Task 8.1: Dual Transport Layer in `network.h` (ESP32 branch)
```cpp
#ifdef ESP32
#include <BluetoothSerial.h>
#include <BLEDevice.h>

BluetoothSerial btSerial;
bool btConnected = false;
bool wifiConnected = false;

// Transport selection logic:
// 1. Try WiFi UDP (existing code path)
// 2. If WiFi fails after 15s, activate BT
// 3. If BT client connects → use BT as data channel
// 4. If WiFi reconnects → silently switch back to WiFi
// 5. Both can be active simultaneously (WiFi preferred for binary image streams)

void sendCommand(String cmd) {
  if (wifiConnected) {
    udp.beginPacket(pc_ip, UDP_PORT_TX);
    udp.print(cmd);
    udp.endPacket();
  }
  if (btConnected) {
    btSerial.println(cmd);  // BT fallback
  }
}
```

### Task 8.2: Python Bridge BT Listener
Add to `companion_controller.py`:
```python
import serial  # pyserial for BT COM port

class BluetoothBridge:
    def __init__(self):
        self.port = None  # auto-detect COM port of "CompanionOS" BT device
        self.serial = None
    
    def auto_detect_bt_port(self):
        # Scan COM ports for one named "CompanionOS" or paired device
        import serial.tools.list_ports
        for p in serial.tools.list_ports.comports():
            if "CompanionOS" in p.description or "ESP32" in p.description:
                return p.device
        return None
    
    def send(self, message):
        if self.serial and self.serial.is_open:
            self.serial.write((message + "\n").encode())
    
    def listen_loop(self):
        # Mirror command_listener() but reading from BT serial
        while True:
            if self.serial and self.serial.in_waiting:
                line = self.serial.readline().decode().strip()
                handle_command_from_device(line)
```

### Task 8.3: BLE Service for Phone Control
```cpp
// BLE GATT service on ESP32
// Service UUID: "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
// Characteristic: commands (write), status (notify)
// 
// Allows a phone/tablet to:
//   - Send navigation commands (page swipe, button press emulation)
//   - Trigger custom thought bubbles
//   - Push song recommendations
//   - Query device status (battery, uptime, current page)

#define BLE_SERVICE_UUID    "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define BLE_CMD_CHAR_UUID   "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define BLE_STATUS_CHAR_UUID "1c95d5e3-d8f4-4b49-bf1f-8c0b9fa75a72"
```

### Task 8.4: Connection Status UI
In `drawStatusBar()` — replace the single WiFi indicator with a dual-transport indicator:
```
[WiFi icon: 3-arc if connected, X if not] [BT icon: filled if BT active]
```
Color coding:
- Both active: WiFi cyan, BT blue
- WiFi only: WiFi cyan
- BT only: BT orange (fallback mode)  
- Neither: red "NO CONN"

### Task 8.5: BT Discovery Mode
When WiFi fails:
1. Activate BT and start advertising as "CompanionOS"
2. Show a QR code on screen (pre-rendered) with the BT pairing PIN
3. Python bridge auto-scans for "CompanionOS" BT device and connects
4. Once paired, send the WiFi credentials over BT → ESP reconnects WiFi

---

## SECTION 9 — GLOBAL VISUAL ENHANCEMENT PASS

This section applies to ALL pages, both themes, both platforms.

### Task 9.1: Color System Upgrade
In `config.h`, replace the flat color palette with a layered design system:
```cpp
// Primary palette (dark theme)
#define CLR_BG         0x0841   // near-black with blue tint
#define CLR_SURFACE    0x1082   // card/panel background
#define CLR_SURFACE_2  0x18A3   // elevated card
#define CLR_BORDER     0x2104   // subtle borders
#define CLR_PRIMARY    0x05FF   // cyan accent (brand)
#define CLR_SECONDARY  0xF800   // red (danger/Dr.Hack)
#define CLR_SUCCESS    0x07E0   // green
#define CLR_WARNING    0xFFE0   // yellow
#define CLR_TEXT_HI    0xFFFF   // white (primary text)
#define CLR_TEXT_MED   0xC618   // light gray (secondary)
#define CLR_TEXT_LO    0x8410   // dark gray (hint text)

// Accent gradient presets (for cards)
#define CLR_GRAD_BLUE_TOP    0x041F  // deep blue
#define CLR_GRAD_BLUE_BOT    0x001F  // midnight blue
#define CLR_GRAD_PURPLE_TOP  0x600F
#define CLR_GRAD_PURPLE_BOT  0x300A
```

### Task 9.2: Card Component System
Create `ui_components.h` with reusable components:
```cpp
void drawCard(int x, int y, int w, int h, uint16_t bgColor, uint16_t borderColor, int radius);
// Draws a rounded card with gradient fill (top lighter than bottom by 10%)
// Adds 1px border and optional top-left colored accent bar

void drawGradientCard(int x, int y, int w, int h, uint16_t topColor, uint16_t botColor, int radius);
// Vertical gradient fill using column-by-column color interpolation

void drawPillBadge(int x, int y, const char* text, uint16_t bgColor, uint16_t textColor);
// Compact pill-shaped label (auto-width, fixed 12px height)

void drawIconButton(int x, int y, int size, uint8_t iconType, uint16_t color, bool active);
// Square icon button with rounded corners and active highlight ring

void drawSeparator(int x, int y, int w, uint16_t color);
// 1px horizontal line with fade-out ends (decorative separator)

void drawProgressPill(int x, int y, int w, int h, float percent, uint16_t fgColor, uint16_t bgColor);
// Rounded progress bar with glow effect on the fill portion
```

### Task 9.3: Status Bar Redesign
Current `drawStatusBar()` — redesign with this layout:
```
|[WiFi/BT icons] [HH:MM] [track♫name] [notif count badge] [BT icon]|
Height: 16px (up from current)
Background: CLR_SURFACE_2 with 1px bottom border in CLR_BORDER
Clock: CLR_TEXT_HI, size 1 font, bold
Track name: CLR_TEXT_MED, truncated to 12 chars, tiny scrolling marquee if > 12
Notif badge: red circle with white count number
```

### Task 9.4: Boot Sequence Enhancement
`runBootSequence()` — completely redesign:
```
Phase 1 (0.5s): ASCII art "C/OS" logo fades in (cyan, large pixel font)
Phase 2 (1.0s): System stats slide in from right:
  CPU: ESP32 240MHz | RAM: 312KB free | Flash: 4MB
  Platform: ESP32 | Display: ST7735R 128×160 | BT: Active
  IP: 192.168.1.xxx | NTP: Synced | Theme: 1
Phase 3 (1.0s): Horizontal progress bar fills left→right with label:
  "Initializing subsystems..." → "Loading eyes..." → "Connecting..."
Phase 4: Transition wipe to eyes page
```

### Task 9.5: Weather Page Enhancement
Current weather page renders basic metrics. Add:
- **Animated weather icon**: Sun (rotating rays), Rain (falling pixels), Cloud (drifting), Snow (drifting flakes)
- **7-day mini forecast strip**: 7 small column cards at the bottom, each showing day abbreviation, icon, high/low
- **Sunrise/sunset arc**: semi-circular arc with current sun position dot based on time
- **Wind direction compass**: small N/S/E/W compass rose with arrow

### Task 9.6: Pomodoro Enhancement
Current: circular ring timer. Add:
- Center of ring: large countdown digits (MM:SS)
- Outer ring: session count dots (●●●○ for 3/4 sessions)
- Below ring: current task name from productivity page (shared data)
- Subtle ticking animation: ring fills have a 1-pixel "brush stroke" shimmer every second
- Work/Break mode toggle: tap changes between orange (work) and blue (break) colorway

### Task 9.7: Eyes Page Enhancement
- Add **reactive idle animations**: when no touch for 30s, eyes start slowly looking around more
- Add **ambient particle system** to starfield: occasional shooting star (streak of 3–4 pixels moving diagonally, 5% chance per render tick)
- **Emotion indicator**: tiny colored dot bottom-left corner showing current emotion color
- **Heartbeat pulse**: very subtle 2px ring that pulses around each eye at resting rate (60bpm) when emotion is NEUTRAL

---

## SECTION 10 — IMPLEMENTATION ORDER & CONSTRAINTS

### Critical Constraints — DO NOT VIOLATE
1. **Memory**: ESP8266 has ~50KB free heap after init. Do NOT add any static buffer larger than 2KB. All new sprite work must use existing `eyeSpr` or `t2Spr` handles, not new allocations.
2. **`albumArt[]` is shared**: stocks sparkline preview, gaming cover, gallery images, and Spotify art ALL use this 9216-byte buffer. Never have two sources write to it simultaneously.
3. **50ms loop budget**: All animation code called from the 50ms timer must complete in < 10ms. No `delay()` in animation paths.
4. **`changePage()` owns navigation**: All state transitions MUST go through `changePage()` or `renderCurrentPage()`. Never set `currentState` directly without calling the page init function.
5. **EEPROM writes**: Only write EEPROM in `command_listener` when theme/config changes. NEVER write EEPROM in the animation loop.
6. **`handleCommand()` is the single entry point** for all UDP data. If adding a new data type, add its `"PREFIX:"` case there.

### Recommended Build Order
```
Step 1: config.h split → config_esp32.h + config_esp8266.h + smart router config.h
Step 2: globals.h — add STATE_DR_HACK, resolution macros, ThoughtBubble struct
Step 3: buttons.h — ESP32 physical button handler  
Step 4: network.h — ESP32 dual transport (WiFi + BT Serial)
Step 5: thought_engine.h — thought bubble system
Step 6: ui_components.h — card/badge/pill component library
Step 7: page_dr_hack.h — Dr. Hack full implementation
Step 8: pages.h + ui.h — visual enhancement pass, status bar redesign
Step 9: page_gaming.h — new gaming layout
Step 10: Gallery/slideshow enhancement in pages.h
Step 11: Python: steam_tracker.py rewrite
Step 12: Python: bluetooth_bridge.py new module
Step 13: Python: companion_controller.py — THOUGHT: command + HACK: commands + gaming_feed_loop overhaul
Step 14: Boot sequence redesign
Step 15: Full test pass on both hardware targets
```

### Files to Create (New)
- `arduino/CompanionOS_Main/config_esp32.h`
- `arduino/CompanionOS_Main/config_esp8266.h`
- `arduino/CompanionOS_Main/buttons.h` (ESP32 only)
- `arduino/CompanionOS_Main/thought_engine.h`
- `arduino/CompanionOS_Main/ui_components.h`
- `arduino/CompanionOS_Main/page_dr_hack.h`
- `python/bluetooth_bridge.py`

### Files to Heavily Modify
- `arduino/CompanionOS_Main/config.h` (becomes router)
- `arduino/CompanionOS_Main/globals.h` (new state, new structs)
- `arduino/CompanionOS_Main/network.h` (dual transport)
- `arduino/CompanionOS_Main/touch.h` (remove cap sensors, add `#if HAS_TOUCH`)
- `arduino/CompanionOS_Main/ui.h` (new status bar, new components)
- `arduino/CompanionOS_Main/pages.h` (slideshow enhancement, all visual upgrades)
- `arduino/CompanionOS_Main/page_gaming.h` (full redesign)
- `arduino/CompanionOS_Main/CompanionOS_Main.ino` (thought scheduler, BT init, buttons)
- `python/companion_controller.py` (BT bridge, thought push, gaming overhaul, hack commands)
- `python/steam_tracker.py` (full rewrite — 3-tier detection)

---

## START COMMAND

Read ALL files in the project workspace. Then start with **Step 1** (config split). After each step, confirm completion and list any ambiguities found before proceeding to Step 2.

For any function you modify, preserve the original as a comment block above the new version tagged `// [LEGACY - v6.0]` so rollback is possible.

All new code must compile without warnings under:
- `ESP32 Dev Module` board, Arduino IDE 2.x, `esp32` core 2.x
- `NodeMCU 1.0 (ESP-12E)` board, `esp8266` core 3.x
