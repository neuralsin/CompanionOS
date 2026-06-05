# CompanionOS v7 Live Audit Ledger

Audited: 2026-06-05
Scope: CompanionOS firmware, Python bridge, web remote, config, and V7 prompt compliance.
Rule: Active checklist contains unresolved work only. Completed defects are removed from the active list and recorded in the change log.

## Active Checklist

- [ ] FW-06: Remove blocking Dr. Hack active-tool loops or add bounded timeouts and non-stuck exit paths. (Confirmed: `while (true)` loops remain in `page_dr_hack.h`, `dh_wifi_tools.h`, `dh_ir_tools.h`, `dh_evil_portal.h`, `dh_cc1101_tools.h`, `dh_rfid_tools.h`).
- [ ] FW-07: Make `page_social.h` usable on the 160x128 ESP32 mini display. Currently uses an unscaled, hardcoded layout (e.g., `cardH = 185`, `cardY = 24`) that clips on 160x128 screens.
- [ ] FW-08: Potential Memory Waste. `theme2_eyes.h` allocates a `TFT_eSprite` using `new` but provides no `delete` mechanism, wasting RAM when switching themes.
- [ ] PY-01: Add missing `psutil` dependency to `requirements.txt`.
- [ ] PY-02: Remove committed real API secrets from `python/config.json` (GitHub PAT, Spotify secrets, Weather API key); use environment variables and safe config fallbacks.
- [ ] PY-03: `config.json` lacks a `twitch_client_id` and `twitch_client_secret` under the `gaming` block, rendering the `TWITCH_` environment variable injection in `companion_controller.py` partially incomplete.
- [ ] PY-04: Shrink or split `STOCKS:` and `GAMING:` UDP payloads in Python so they cannot exceed the ESP parser budget. No size-checking logic exists in `stock_manager.py` or `steam_tracker.py` UDP sends.
- [ ] PY-05: Fix UDP port conflicts: `web_remote.py` discovery (`LOCAL_PORT = 8889`), `bluetooth_bridge.py` (`UDP_PORT_RX = 8889`), and `companion_controller.py` (`PC_PORT_RX = 8889`) all conflict by binding to the same PC receive port.
- [ ] PY-06: Fix Pomodoro timing drift by tracking integer milliseconds instead of subtracting floats.
- [ ] UI-01: Polish no-touch UX: physical buttons and web e-buttons must cover navigation, page selection, Spotify, emotion, and thought workflows.
- [ ] VERIFY-01: Run Python compile/lint checks and document Arduino verification status.

## Change Log

- 2026-06-05: Fixed FW-05. Made `page_network.h`, `page_stocks.h`, `pages.h` (Spotify, Weather, Notifications), and `page_productivity.h` layout-aware for 160x128 via `SCREEN_W > 200` conditional logic.
- 2026-06-05: Audited the entire codebase for mismatch bugs, old code residue, and failures. Appended FW-07 and FW-08 to the active checklist. Confirmed presence of blocking loops (FW-06) and Python UDP port binding conflicts (PY-05).
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
