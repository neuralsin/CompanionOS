# 📱 CompanionOS - Feature & Display Architecture

The CompanionOS GUI is driven by a unified state machine located in `pages.h`. The system divides the 240x320 pixel TFT screen into **four distinct active pages**. 

You can cycle through these pages using the physical Left/Right capacitive touch sensors (`D3` and `D4`) or via the on-screen buttons when active.

---

## 👁️ Page 0: The Emotional Engine

**Purpose:** Acts as the default idle face of your desk companion.
**UDP Payload Trigger:** `EMOTION:[TYPE]`

### Display Elements:
- **Top Bar:** "Companion OS" with a 4-dot page indicator.
- **Center Canvas:** Dynamic eyes rendering engine (`eyes.h`).
- **Animations:** Fully simulated blinking engine fires randomly every ~3 seconds.
- **Emotions Supported:**
  - `HAPPY` (Curved smiles, cyan)
  - `SAD` (Droopy eyes, teardrops)
  - `EXCITED` (Wide-open with rendering sparkles)
  - `LOVE` (Pink geometric hearts)
  - `SLEEPY` (Half-closed slits with floating 'Z's)
  - `ANGRY` (Red slanted triangles)
  - `SURPRISED` (Wide irises)
  - `NEUTRAL` (Standard oval shape)

**Interaction:** Tapping the physical hardware "ears" cycles pages. Tapping the TFT screen itself cycles the current emotion manually for testing.

---

## 🎵 Page 1: Spotify Playback Hub

**Purpose:** A rich multimedia player for your connected Spotify account.
**UDP Payload Triggers:** `TRACK:{}`, `STATE:{}`, `LYRICS:{}`, `ART_CHUNK:`

### Display Elements:
- **Top Bar:** "Spotify Player".
- **Album Art (Y: 40-160):** A 120x120 pixel perfectly mapped RGB565 canvas. The Python controller downloads the image and chunks it out to the ESP8266 where it is painted pixel-by-pixel.
- **Track Metadata (Y: 170-200):** Centered text displaying the current Track Name (Green) and Artist Name (White).
- **Synchronized Lyrics (Y: 215):** The active line of the song parsed from the local Spotify Lyrics proxy (Light Grey).
- **Progress Bar (Y: 240):** A live-updating horizontal bar mapping current millisecond progress against total track duration.
- **Transport Controls (Y: 270):** Three distinct touch targets (Prev, Play/Pause, Next).

**Interaction:** Pressing the physical ears skips tracks forward/backward. Pressing the on-screen buttons triggers exact playback controls (Pause/Play).

---

## 📊 Page 2: GitHub Statistics

**Purpose:** A developer dashboard tracking your open-source presence.
**UDP Payload Trigger:** `GITHUB:{}`

### Display Elements:
- **Top Bar:** "GitHub Stats".
- **Username (Y: 80):** Large white text displaying your fetched GitHub handle (e.g., `@torvalds`).
- **Metrics (Y: 130-160):** 
  - Total Public Repositories (Sky Blue)
  - Total Followers (Sky Blue)
  
*Note: If data is unavailable, it gracefully defaults to displaying "Awaiting Data..." in dark grey.*

---

## 📝 Page 3: Live Notes & Reminders

**Purpose:** A quick-glance checklist for daily tasks.
**UDP Payload Trigger:** `NOTES:[]`

### Display Elements:
- **Top Bar:** "Quick Notes".
- **List Items (Y: 50-140):** A vertical stack of up to 4 lines rendered in bright yellow.

**Interaction:** This page acts as a mirror to the `notes.txt` file located in your Python controller directory. Simply typing into that text file on your PC will automatically fire a UDP JSON sync event, repainting the ESP's screen instantaneously without needing any compilation.
