# Native Firmware UI Regressions

This harness includes and executes the real firmware `ui_components.h` and all
nine page headers: clock dashboard, retro watch, gaming, social, productivity,
network, stocks, PC dashboard, and RuView. It also directly includes `pages.h`, `theme2_spotify.h`, and
`ui.h`, including the real shared status bar and page indicator. It does not copy
their text fitting, rendering, cache, or page-transition functions.
All scaffolding and generated output stay under `CompanionOS/tests` by default.

## Run

From the repository root on Windows:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File CompanionOS/tests/run_ui_host.ps1 -Render
```

The runner builds **both 160x128 and 320x240**, then executes both even if one
fails. Exit status is 0 for success, 1 for compile/test failure. Each executable
also returns 2 for invalid arguments or a filter matching no cases. Build logs,
executables, and optional binary PPM images are in ignored `host-output/`.
MinGW's bin directory is temporarily prepended to PATH for compiler subprocesses
and runtime DLLs. Defaults are this machine's `C:/mingw/bin/g++.exe` and installed
`~/OneDrive/Documents/Arduino/libraries/TFT_eSPI`. Override with `-Compiler` and
`-TftEsPi` as needed; missing font files are a hard error, never a silent fallback.

Focused runs:

```powershell
./CompanionOS/tests/run_ui_host.ps1 -Filter helpers/
./CompanionOS/tests/run_ui_host.ps1 -Filter pages/stocks -Render
./CompanionOS/tests/host-output/ui-host-160x128.exe --filter marquee
```

Optional PNG conversion and labelled contact sheets, after `-Render`:

```powershell
python CompanionOS/tests/render_ui_contact_sheet.py CompanionOS/tests/host-output/160x128
python CompanionOS/tests/render_ui_contact_sheet.py CompanionOS/tests/host-output/320x240
```

Only conversion needs Pillow. The compiler and native tests have no Python,
Arduino, ESP SDK, graphics-window, or network dependency.

## What Is Asserted

- `uiText`: null/empty inputs, ASCII, controls, nonbreaking spaces, curly quotes,
  dashes, ellipsis, common accents, music/bullet symbols, unsupported CJK/emoji,
  incomplete and malformed UTF-8 tails, and long input.
- `fitUiText` / real `drawTruncatedText`: installed fonts 1/2/4, every width from
  -2 through 90, exact fits, smaller-than-ellipsis boxes, null/empty data,
  screen-right-edge bounds, background forwarding, and long/UTF-8/newline text.
- Real `drawWrappedText`: tiny/zero/negative width and height, hard newlines,
  blank lines, whitespace-only text, long words, maximum row count, measured
  width/height containment, and background forwarding.
- Real gradient rendering: zero/negative extents submit no drawing; height one
  paints exactly its assigned width with the top color.
- Real marquee rendering: bounce positions, pause/end/cycle timing, 32-bit time
  rollover, tiny/invalid boxes, pixel clipping, and restoration of both relative
  and absolute viewports. The marquee now paints a reusable 16-bit sprite strip
  instead of changing the display viewport. Tests assert pixel confinement,
  size-keyed allocation reuse, deletion/recreation after resizing, explicit
  backgrounds, screen-edge clamping, and fitted-text fallback when allocation
  fails. The false-datum test guards against the original bug: TFT_eSPI's public
  logical getters cannot round-trip a custom absolute clip.
- The seven general pages: empty, maximally populated fixed-size ASCII fields, and UTF-8
  fields; full render plus changed-data partial refresh and inactive-page guard.
  Text is measured before clipping, must be printable bitmap-font ASCII, and
  must stay on-screen above the reserved footer. Full-frame text rectangles must
  not collide. Long fixtures respect the firmware's actual buffer capacities,
  including valid NUL termination; clipping a UTF-8 fixture at a buffer boundary
  also exercises incomplete codepoints. These tests report existing firmware
  defects as failures rather than hiding them with expected-failure allowances.
- `pages.h` integration: Classic and Theme 2 Spotify empty/long/UTF-8 first
  paint, measured text bounds, collisions, visible real play/pause geometry,
  state changes, marquee ticks, and identical content after leaving/reentering
  with unchanged data through real `setPage`/`renderCurrentPage` functions.
- Album art transactions: incomplete input is not published; completed input
  copies and paints real image pixels; a new revision updates same-title art.
  Classic and Theme 2 compare all 4096 thumbnail pixels with a deterministic
  RGB565 quadrant/grid fixture, including orientation and same-data reentry.
  Cover and updated-cover render frames are saved for both themes. These are
  synthetic transfer fixtures, not downloaded Spotify album covers.
- Real notification, notes, and settings functions: empty/long/UTF-8 data,
  bounds, full-frame collisions, same-data reentry, scroll clipping/clamping,
  viewport restoration. A Pomodoro same-data reentry test exercises generation
  invalidation of its cached initial paint.
- Actual weather page: fifteen WeatherAPI/OpenWeather-style code fixtures,
  including clear, partly cloudy, overcast, rain, storm, snow, fog, and unknown
  conditions. Native icons must paint pixels only inside their assigned box.
  Assertions check actual temperature, feels-like, humidity, wind, high/low,
  sunrise and sunset values; all text bounds, collisions, and same-data reentry.
  Empty, whitespace-only, failed, offline, cached/stale, long UTF-8, and extreme
  numeric fixtures cannot show fake readings or indefinitely claim an update.
- Real eyes-page shared header: morning/afternoon/evening/night graphics,
  six-second title/artist alternation, measured single-line bounds, and actual
  progress underline with duration clamping. Eye animation remains a stub;
  the header itself is the firmware implementation with real sprite composition.
- Actual PC dashboard: all four subpages, offline/normal/extreme metrics, long
  process names, absent readings, timeout, text collisions and quiet unchanged updates.
- Actual RuView: offline/occupied/motion/calibration/extreme inputs, bounded
  confidence/variance/PPS labels, unchanged-frame suppression and feed expiry.
- Focus: ready/running/paused/break/completed/invalid/extreme-duration states,
  fitted timer digits and retained elapsed ring while paused.

## Structural Regression Suites

```powershell
python -B CompanionOS/tests/eyes_pixel_test.py
python -B CompanionOS/tests/tile_input_test.py
python -B CompanionOS/tests/button_actions_test.py
python -B -m unittest discover -s CompanionOS/tests -p '*_test.py'
```

`eyes_pixel_test.py` uses the real Classic/Soft/Robo/thought engines with the
bitmap rasterizer. It compares buffered and denied-canvas pixels at both sizes,
requires visible eyes in both halves, protects header/footer pixels and checks
the thought box before the scheduler can hide an intermediate-frame erase.
The command-sink lifetime/context suites remain separate. These are local test
executables, not Arduino firmware or flashable `.bin` files.

`tile_input_test.py` extracts and executes the actual current handlers using the
installed ArduinoJson: timer validation/atomic leases, duplicate notification
banners, shrinking notes/watchlists/recent games, numerical bounds and valid
versus absent CSI. Python fixtures test demand gating, probe echoes, stale
classifier state, timer commands, Social reconnect/clear, and local tasks.

## Accuracy And Limits

The separate `dr_hack_diagnostics_test.py` runner extracts the current menu,
passive WiFi/BLE scan, hardware/system/about/error renderers, receive-only nRF
spectrum and GPIO input monitor. It compiles no transmitter/attack implementation.
Non-diagnostic tool dispatches throw in the host harness. It checks all 48 menu
positions, list bounds, failure states, radio cleanup and virtual navigation at
both display sizes; actual RF reception and wiring still require device checks.

`host_globals.h` predefines the actual firmware `GLOBALS_H` guard and supplies
only the globals required by these pages. This intentionally bypasses hardware
initialization. `host_arduino.h` implements a small byte-oriented Arduino String
surface; it is not an Arduino allocator, timing, ABI, or out-of-bounds-access emulator.
Global types and scaling macros mirror the current firmware and may need small
updates when that firmware contract changes.

`host_tft.h` directly compiles the **installed TFT_eSPI** GLCD, Font16, and Font32
width tables and glyph bitmaps. Font 1 has the real six-pixel advance; font 2 is
proportional and 16 pixels high; font 4 is proportional and 26 pixels high. The
sink decodes the installed font-4 RLE bitmap rather than drawing text rectangles.
The host `TFT_eSprite` owns an independent raster surface, transfers actual pixels
through the parent's current clip/datum, and translates text traces on push.
Only the next sprite allocation can be deliberately failed by a test.
The byte-based width behavior for built-in fonts follows the installed library.
Other fonts fail explicitly. Pixel clipping and logical viewport getters model
TFT_eSPI; raw UTF-8 glyph drawing does not emulate TFT_eSPI's decoder because
normalized output is expected to be ASCII. Bounds for raw UTF-8 are diagnostic,
not a claim of Unicode renderer fidelity.

PPMs are real page draw calls through this sink, not screenshots of a device.
ASCII glyphs use actual bitmap data; circles, triangles, and rounded corners use
simple host rasterizers and can differ at edge pixels from TFT_eSPI. Bounds are
advance rectangles, not precise ink boxes: text-collision checks are conservative.
Text antialiasing, smooth/free fonts, SPI transfer, display rotation, physical
panel offsets, ESP concurrency, and framebuffer persistence on hardware are not
covered. `host_page_guards.h` replaces eye animation in this UI suite; the
separate eye raster/lifetime/context suites execute those actual engines.
PC dashboard and RuView page bodies are no longer stubbed. ESP32-only RF
probing is excluded, so settings coverage is the common
non-RF path. Weather now uses supported fonts 1/2/4 and its full page is executed;
other unsupported fonts still fail explicitly if called. Shared status-bar sprite composition and
the real page indicator are now exercised, not stubbed. Spotify and the selected
`pages.h` reentry tests compare actual framebuffer pixels; standalone partial
refreshes check bounds, not every possible stale-cache transition. The native harness complements an Arduino
build and a device check; it does not replace either.
