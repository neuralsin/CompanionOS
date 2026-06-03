# CompanionOS v7 Live Audit Ledger

Audited: 2026-06-04
Scope: CompanionOS firmware, Python bridge, web remote, config, and V7 prompt compliance.
Rule: Active checklist contains unresolved work only. Completed defects are removed from the active list and recorded in the change log.

## Active Checklist

- [ ] FW-05: Make 320x240-only pages usable on the 160x128 ESP32 mini display, starting with Stocks and Network.
- [ ] FW-06: Remove blocking Dr. Hack active-tool loops or add bounded timeouts and non-stuck exit paths.
- [ ] PY-01: Add missing `psutil` dependency to `requirements.txt`.
- [ ] PY-02: Remove committed real API secrets from `python/config.json`; use environment variables and safe config fallbacks.
- [ ] PY-03: Inject Twitch/IGDB credentials from config/environment before `steam_tracker.py` uses `TWITCH_CLIENT_ID` and `TWITCH_CLIENT_SECRET`.
- [ ] PY-04: Shrink or split `STOCKS:` and `GAMING:` UDP payloads so they cannot exceed the ESP parser budget.
- [ ] PY-05: Fix UDP port conflicts: `web_remote.py` discovery must not bind to controller port 8889, and `bluetooth_bridge.py` must not bind to ESP outbound port 8888.
- [ ] PY-06: Fix Pomodoro timing drift by tracking integer milliseconds instead of subtracting floats.
- [ ] UI-01: Polish no-touch UX: physical buttons and web e-buttons must cover navigation, page selection, Spotify, emotion, and thought workflows.
- [ ] VERIFY-01: Run Python compile/lint checks and document Arduino verification status. `arduino-cli` and `pio` are not installed locally as of this audit.

## Change Log

- 2026-06-04: Initialized this live audit ledger from the stale full report, keeping only verified unresolved issues as active checklist items.
- 2026-06-04: Verified Python syntax with `python -m compileall -q CompanionOS\python`; no Python syntax errors at audit start.
- 2026-06-04: Verified local Arduino build tooling is absent: `arduino-cli` and `pio` commands are not installed, so firmware compilation cannot be executed in this CLI until one is installed.
- 2026-06-04: Fixed firmware header-order blocker by including `config.h` before `#if HAS_TOUCH` in `arduino/CompanionOS_Main/globals.h`.
- 2026-06-04: Removed duplicate `showFlashNotification()` from `arduino/CompanionOS_Main/pages.h`; `ui.h` is now the single notification overlay implementation.
- 2026-06-04: Added `changePage`, `sendCommand`, `setEmotion`, `t2_nextExpression`, and `flashNotifEnabled` forward declarations in `arduino/CompanionOS_Main/network.h` for compile-safe virtual button handling.
- 2026-06-04: Replaced fragile `BTN:` handling in `arduino/CompanionOS_Main/network.h` with `handleVirtualButton()`, covering LEFT/RIGHT/SELECT/HOME for page navigation, Spotify control, Pomodoro start/pause, notification toggle, settings theme toggle, and Dr. Hack navigation.
- 2026-06-04: Hardened the binary `0xFE` image receiver in `arduino/CompanionOS_Main/network.h` by validating `startPixel`, image dimensions, and safe pixel count before writing into `albumArt[]`.
- 2026-06-04: Corrected RGB565 binary reassembly in `arduino/CompanionOS_Main/network.h` to match the Python sender's high-byte/low-byte packet order, fixing wrong-color album/game art.
- 2026-06-04: Added `getEffectivePageCount()` in `arduino/CompanionOS_Main/ui.h` and made `drawPageIndicator()` clamp to the platform-specific page count, removing the unreachable ESP32-only Dr. Hack dot from ESP8266 UI.
