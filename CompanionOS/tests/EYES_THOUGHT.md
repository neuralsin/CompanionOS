# Native Eyes And Thought Regressions

From the repository root on Windows:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File CompanionOS/tests/run_eyes_thought_host.ps1
```

Requires `g++` on PATH. Override it with `-Compiler C:/mingw/bin/g++.exe`.
The script temporarily adds the compiler's directory to PATH for MinGW runtime
DLLs. It builds and runs four configurations: 160x128 and 320x240, each with the
ESP8266/raw-ADC and ESP32/12-bit-ADC branches. It returns 1 on any build or test
failure and 0 only when all four pass. Assertions cannot be disabled.
Artifacts go to `$env:TEMP/companion-eyes-thought-host`; override `-OutputDir`
to retain them elsewhere. No Arduino CLI, firmware build, device, network,
installed font tables, or Python is required.

`eyes_thought_host_regression.cpp` includes the actual firmware `eyes.h`,
`thought_engine.h`, `theme2_eyes.h`, and `theme3_eyes.h`, using repository-relative
paths. It executes their real functions, not copies of their implementations.
`eyes_thought_host.h` reuses `host_arduino.h` for String, deterministic random,
and a controllable clock. Its separate command-sink display fake adds the sprite
APIs and depth-specific allocation failures absent from the shared UI fake,
without changing that suite or its globals.

## Coverage

- Five-minute display and one-second fade, monotonic alpha, repeated calls at
  the same timestamp, final deactivation, and duplicate transport pushes that
  clear `active` without restarting `shownAt`.
- All 79 printable bytes retained, long unbroken words, wrapping widths, 720
  whitespace fixtures per configuration, and multi-page rotation.
- Direct-render caching, repaint after eye redraw, suppression of the redundant
  TFT pass for sprite themes, text-state restoration, and scheduler disable.
- Scheduler deadline and fade lifetime across 32-bit clock rollover.
- Fixed classic-eye sprite dimensions across emotions/blinks, allocation reuse,
  16-to-8-bit fallback, total failure, retry backoff, recovery, and theme release.
- Theme-2 captured-source transitions and close/hold/reopen blinking.
- Theme-3 nonaccumulating shake offsets, integer tween convergence, sleepy blink
  reopening, transient expression reset, and cyclops constraints.
- Real theme render paths preserve header/footer and active overlay regions;
  full body transfers resume when overlays disappear.
- Dark threshold at normalized values 99/100, plus the actual `updateEyes()`
  ADC read: 500 stays 500 on ESP8266 and becomes 125 on ESP32.

## Limits

This is an instrumented command/state regression test, not a raster screenshot,
font-metric test, device heap, SPI timing, MCU ABI, or concurrency emulator.
Primitive footprints are conservative bounding rectangles; sprite transfers
record and validate their bounds rather than rasterizing pixels. Viewport
logical getters match TFT_eSPI's false-datum semantics. The shared header/footer
and thought configuration/type definitions mirror the firmware contract and
must be updated when that contract changes. The host's `unsigned long` may be
64-bit; rollover cases deliberately wrap the shared 32-bit clock and exercise
the production functions' explicit 32-bit arithmetic, not every MCU expression.
Color interpolation and the status bar are dependency stubs. Use the separate
UI host suite for raster/font checks and hardware for physical rendering.

## Parent Personality Integration

Run the additional integration checks from the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File CompanionOS/tests/run_eyes_thought_context.ps1
```

This also runs all four native configurations. It extracts the current
`initThoughtScheduler()` body and the ambient-context block from
`CompanionOS_Main.ino` into a generated temporary header, with original source
line mappings. It compiles those bodies with the actual four eye/thought headers;
there is no hand-maintained copy of the parent's timing or dispatch algorithm.
If the extraction anchors change, the runner fails explicitly.

Additional checks cover the 15-45 second first local thought, local teasing and
ambient banks with no clock/music/weather/override input, repeat scheduling,
all 24 hourly mood choices in all three themes, LDR priority, the strict
`>60000ms` manual lease, five-second ambient cadence, visual mood after page/theme
reentry, and Classic's frame-before-ambient ordering. Frame submissions follow
main's 33ms cadence, rather than bypassing theme3's frame throttle.

The integration runner deliberately returns nonzero for actual regressions;
these cases are not expected-failure allowances. The five original failures
(Soft preset reset, two Robo sleepy-state checks, and two Classic automatic
manual-lease checks) are fixed and now pass in all four configurations.
Reentry tests additionally assert immediate visual restoration before main
dispatch and preservation of both active manual and expired automatic leases.
Classic animation must leave ambient dispatch and manual timestamps untouched.
Offline scheduling and standalone hourly/manual dispatch checks also pass.
Artifacts go to `$env:TEMP/companion-eyes-thought-context`.

The extracted loop slice does not execute networking, NTP synchronization, the
full setup/loop, or the shared header renderer. Wall-clock input is simulated:
without valid time, mood is neutral unless the LDR selects sleepy. Offline
thoughts do not require a clock, network, companion host, music, or weather.
