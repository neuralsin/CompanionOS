# Deskbot Rendering And Standalone Services Audit

Date: 2026-10-15

Follow-up: 2026-10-16, including live browser authorization and Wi-Fi recovery.

This pass targets the current ESP32 ST7735 deskbot, its three eye themes,
standalone Spotify/weather, navigation, and display pages. Existing staged
Android/Python/dashboard work was preserved. No firmware was uploaded.

## TLS RAM Follow-Up, 2026-10-16

### V2 Supersedes The V1 Admission Guard

The later hardware log `free=65920 largest=38900` showed v1 never attempted
TLS: its guessed 40 KiB largest-block gate rejected every request. V2 removes
that admission test, retains only a graphics headroom policy (80/24 KiB), blocks
new canvases while networking, and trims canvases before worker allocation when
needed. Affordable active canvases are preserved to avoid repeated eye flicker.
It also releases Bluetooth controller/host RAM before Wi-Fi in normal use.
The explicit radio workspace uses a one-shot RTC restart to reserve it again;
exiting restarts into the requested normal tile. Long boot animation is skipped
for these switches; volatile timer/UI state resets. No remote tool command is
stored/replayed, and no attack behavior was improved or validated.

Spotify jobs now precede weather, which is demand-driven. CSI sample/UDP state
is allocated only for RuView and freed/unregistered on exit. Optional PC feeds
require renewable page leases, and Steam construction is lazy. The PC dashboard
UDP listener runs only near its tile. Read-only assets remain flash-backed;
small mutable fields, radio-tool tables and SDK state still occupy RAM. This
is not a claim that arbitrary mutable state has been relocated to ROM.

Use `CompanionOS-ESP32-TLS-RAM-v2-private.zip` and its current manifest. All
v1 build figures and validation below are historical. V2 physical TLS remains
unverified because the deskbot is connected to another computer.

V2 validation: Arduino ESP32 core 3.3.8 / huge_app builds successfully at
2,236,438 bytes flash and 103,936 bytes static RAM. The generated sketch and
headers match current source, esptool validates the application checksum/hash,
and all five packaged image hashes match the ZIP manifest. The ELF exports a
strong `btInUse` override, so normal setup owns the Bluetooth release decision.
Host checks pass: 79 Python tests, native services with Spotify on/off, memory
policy/boot-tag checks, 2000 artwork cycles, 2000 CSI open/close cycles including
late callbacks and failure recovery, both Wi-Fi/button platform branches,
credential and clock input tests, eye/thought/context tests, and UI geometry
regressions. Targeted Ruff F/E9 checks pass for the changed helper modules.
None of these checks measure actual post-release heap or certify hardware TLS.

### V1 History

The user supplied real device logs showing `TLS -32512: SSL - Memory allocation
failed` on both weather and Spotify. This is a confirmed internal-SRAM allocation
failure, not an OAuth rejection. Longer timeouts and an extra DNS lookup cannot
repair it.

- Removed the permanently reserved worker cover pixels (8 KiB) and both eager
  boot-time album allocations (16 KiB). Artwork is allocated on demand, handed
  off without a second thumbnail copy, and discarded outside Spotify's neighbors.
- Optional sprites must leave 64 KiB free internal RAM and a 40 KiB contiguous
  block. Full color is preferred when affordable; 8-bit buffers are the next
  choice. Main trims graphics before a service job; the worker never frees TFT
  memory. HTTP bodies grow toward their cap instead of allocating the maximum.
- Custom image and marquee buffers now have page-scoped lifetimes. No page
  framebuffer is retained for every tile. Code/fonts/read-only assets already
  reside in flash; Wi-Fi/TLS and small mutable live state still require SRAM.
- Mapped mbedTLS allocation errors to memory failures, kept generic native error
  -1 distinct from TLS-specific errors, and added credential-free heap diagnostics.
  `[memory:TLS-RAM-v1]` identifies the new boot path.
- Current-source checks also exposed older blocking Wi-Fi and duplicate button
  paths. `wifi_connectivity.h` now owns the quiet 12-second/profile, 120-second
  retry cycle, preferred network merge, one-time credential migration and service
  restart. Virtual/physical action parity is restored. Host weather leases are
  acquired only on valid data; time/notification/game/CSI packets cannot change
  host identity. Valid Spotify snapshots notify/persist once and acknowledge sync.
- Memory policy and 2000 allocation-counted artwork/page cycles pass. Service
  tests pass with Spotify enabled and disabled, including low/fragmented SRAM,
  allocation-error classification, stale covers and zero-copy cover ownership.
- UI: 154 cases, 47,393 assertions, zero failures across the two tested layouts.
  Eye and context checks still pass in all four size/platform configurations.
  Wi-Fi and button native regressions pass for ESP32 and ESP8266; actual Spotify
  input parsing and clock parsing pass. The 73 Python fixture tests pass, including
  the game scanner's typed WinAPI boundary and strong-versus-weak evidence order.
- Live desktop checks still return Spotify refresh HTTP 200, playback HTTP 204
  (authorized, no active device), and weather HTTP 200. These are not an ESP32 TLS
  acceptance test. The deskbot is on another computer; nothing was flashed here.

The latest build/archive identity is in `OUTPUT/firmware/manifest.json` and
`CompanionOS-ESP32-TLS-RAM-v1-private.zip`. The earlier size figures below are
historical, not the TLS-memory revision. Use the current manifest after packaging.
The final TLS-RAM-v1 build passed with 2,235,426 bytes flash and 104,776 bytes
static RAM. Source headers and the reconstructed generated sketch match, esptool
checks the image checksum/hash, and all five archive images match their SHA-256
manifest entries. The extra two-second retries for denied graphics buffers are
included, so eye/header/title buffering can recover after TLS pressure subsides.

## Confirmed Causes

| Finding | Effect | Change |
| --- | --- | --- |
| Ground-wired buttons configured active HIGH with floating inputs | Random input and missed presses | User-confirmed active LOW and INPUT_PULLUP; debounce both edges |
| Knob expects 12-bit ADC, setup selects 10-bit | Wrong page ranges | Use 12-bit ADC and normalize LDR independently |
| Knob continuously enforces its previous page | Remote/button navigation snaps back | Switch only after a new settled physical movement |
| HTTPS Spotify/weather in handleNetwork | Display/input stalls for request duration | One bounded FreeRTOS worker; main exclusively owns UI and shared state |
| Any PC traffic suppresses direct Spotify | Telemetry disables standalone music | Separate playback-data lease from general host presence |
| Standalone Spotify fetches metadata only | Blank artwork; transport requires a host | Native JPEG download/decode, token refresh, and local playback controls |
| Mixed include-order scaling | Layout changes depending on included header | Restore scaling after the Dr.Hack header |
| Fonts do not shrink with scaled coordinates | Overlapping labels and cut-off rows | Measured text widths and font-based row heights |
| Marquee draws beyond its box | Neighbouring content erased | Clip in a small sprite without changing caller viewport |
| Thoughts painted in sprites and again on TFT | Blinking blue box | One compositor per theme; stable layout and bounded timed fade |
| Frame-dependent blinks/tweens/shake accumulation | Twitchy animation and stuck expressions | Time-based blinking, stable interpolation, render-only offsets |
| Incomplete album rows repeated as finished art | Stripes presented as a valid cover | Stage complete rows and commit only a complete 64x64 image |
| Title/art cache order differs between themes | Fresh cover erased or stale art retained | Revision-based invalidation and consistent first paint |
| Progress units guessed below 1000 | First second of playback jumps by minutes | Milliseconds throughout; elapsed-time ticker on every page |
| UI paints per PC telemetry packet | Repaint bursts | Dirty-state coalescing at at most 5 Hz |
| Empty pages/cache state survive re-entry | Blank first paint | Generation-based cache invalidation and explicit empty states |
| Upload completion marks truncated images ready | Partial/uninitialized image pixels | Validate allocation, length, abort, and upload completion |
| Stuart is first; saved Wi-Fi profiles mask configured changes | Preferred network is skipped or stale passwords remain | Prefer P202_4G, merge/deduplicate profiles, apply compiled credential changes once; preserve later explicit saved password edits |
| Five-second blocking Wi-Fi attempts and blocking retry screens | Slow router association fails; UI stalls | Non-blocking 12-second attempts; 120-second pause between failed cycles; no Bluetooth fallback |
| Raw HTTP stream reads do not decode chunked responses | Real weather response fails despite HTTP 200 | Bounded decoded HTTPClient stream, explicit identity encoding, deadline and overflow checks |
| General host presence suppresses native weather or local focus | Dashboard traffic disables unrelated features | Independent playback, weather, and Pomodoro ownership leases |
| TIME/NOTIF packets label any sender as Android | PC and native Spotify can both take ownership | Host identity comes only from explicit HOST/probe messages |
| Several virtual actions have no implementation; duplicate UP/DOWN branches | Select/long presses do nothing or differ from physical controls | Action parity, reachable settings scroll, dashboard Select, notification toggle |
| Expired OAuth cache and unsynchronized credential snapshots | Spotify never reconnects | Browser authorization broker, controller recovery request, unicast provisioning, NVS token persistence |

## Display Changes

- Shared 16-pixel header, page titles, clock, connection indication, and reserved footer.
- Measured truncation and bounded wrapping for long titles, notifications,
  notes, lyrics, status messages, and dashboard values.
- Explicit fallback for unsupported built-in-font Unicode; complete UTF-8
  codepoints are consumed before fitting. This is not full Unicode typography.
- A stable record graphic when artwork is absent, smooth bounded title
  scrolling, restrained playback motion, and a short alert entrance.
- Fixed settings layout with reachable theme controls and a clipped scroll area.
- Fitted clock/watch, gaming, social, task, network, stocks, PC dashboard,
  weather, notification, notes, and RuView layouts.
- Clock ticks update small regions; watch updates its hand sweep; transient
  banners have priority over the eye background. Inactive eye canvases are freed.
- Focus timer starts, pauses, resets, and counts down without a Python host.
- Idle eyes show a day/night greeting and clock. Playback replaces branding with
  alternating track/artist text and a real progress underline in all three themes.
- Local thoughts begin after 15-45 seconds, independent of networking. Ambient
  expressions respect time/light and leave manually selected moods alone briefly.
- Weather uses native sun/cloud/rain/snow/storm graphics, measured temperature,
  humidity/wind/range fields, and cached/offline/error states rather than blank space.
- RuView's existing controller publisher is now actually started alongside its
  CSI receiver. Previously processed room state was never sent to the display.

## Browser Authorization Recovery

The user completed the browser sign-in on 2026-10-16. Live checks then returned
HTTP 200 from Spotify token refresh, HTTP 204 from playback (authorized, no active
playback session), and HTTP 200 with a chunked identity-encoded weather response.
These are desktop network checks, not proof of ESP32 connectivity or playback.
The firmware's ignored `spotify_credentials.local.h` now contains that authorized
seed; the checked-in config no longer contains OAuth keys/tokens. A second live
check using the exact private firmware seed also passed refresh and playback.
The diagnostic reader accepts actual `#define` declarations, not just C++ assignments.

The controller now handles `SPOTIFY_AUTH_REQUEST` from its known deskbot without
blocking the UDP listener. It first tries cached authorization, otherwise opens a
bounded loopback callback server with random state validation. Successful consent
updates the controller cache and sends credentials only to the known unicast
device. Discovery also triggers provisioning. The ESP32 validates a complete
credential snapshot, invalidates stale work, and saves refresh-capable credentials
and rotated refresh tokens in one versioned/checksummed NVS record. It replies
`SPOTIFY_AUTH_OK`; no reply must be treated as unverified delivery, not success.

One-time sign-in without running the full controller:

```powershell
python CompanionOS/tools/authorize_spotify.py
python CompanionOS/tools/diagnose_services.py --refresh --source python
python CompanionOS/tools/provision_spotify.py --no-broadcast
```

The browser runs on the controller machine for the configured
`http://127.0.0.1:8888/callback` redirect. A controller on another machine needs a
registered redirect reachable from that browser; a loopback URL does not bridge
machines. Protocol follows Spotify's [authorization code flow](https://developer.spotify.com/documentation/web-api/tutorials/code-flow)
and [registered redirect requirements](https://developer.spotify.com/documentation/web-api/concepts/redirect_uri).

## Standalone Service Behaviour

Only the worker performs direct HTTPS requests. Jobs snapshot credentials and
ownership; results are consumed by the main task. Queues and payload sizes are
bounded. Old covers cannot replace a newer track. Transient errors and HTTP 429
back off; 401 can refresh once; playback commands are not endlessly replayed.
Authentication, rate-limit, no-device, queue, and transport failures have a visible
status instead of a success animation.

The original ESP8266 service fallback is retained. The full integrated build
target for this audit is ESP32; host rendering tests also exercise 320x240 layouts.

## Reproduce

From the repository root:

```powershell
./CompanionOS/tools/build_firmware.ps1
./CompanionOS/tests/run_ui_host.ps1 -Render
./CompanionOS/tests/run_eyes_thought_host.ps1
python CompanionOS/tests/wifi_connectivity_test.py
python CompanionOS/tests/clock_input_test.py
python CompanionOS/tests/button_actions_test.py
python CompanionOS/tests/spotify_authorization_test.py
python CompanionOS/tests/render_ui_contact_sheet.py CompanionOS/tests/host-output/160x128
python CompanionOS/tests/render_ui_contact_sheet.py CompanionOS/tests/host-output/320x240
g++ -std=c++11 -Wall -Wextra -Werror CompanionOS/tests/standalone_services_detail_test.cpp -o "$env:TEMP/standalone-services-detail-test.exe"
& "$env:TEMP/standalone-services-detail-test.exe"
```

The build script selects the 3 MB `huge_app` application partition on the
4 MB ESP32 board. The earlier standard application partition is too small for
this firmware. This partition trades away the second OTA application slot.
It uses installed libraries without changing their configuration.

## Verification

- First integrated ESP32 build: passed, 2,192,934 bytes flash and 100,056 bytes
  static RAM. These figures are from the first build, not a measurement of the
  runtime heap peak or of the final revision.
- Authorized-seed integrated build: passed, 2,242,398 bytes flash and 112,928 bytes
  static RAM. The subsequent durable saved-password migration is included in the
  packaged build; its final size is recorded with that package.
- Earlier pre-TLS-memory packaged build: passed, 2,243,126 bytes flash (71% of the 3 MB app
  partition), 112,928 bytes static RAM. App image is 2,243,280 bytes including
  image framing/padding. Espressif esptool 5.2.0 confirms ESP32, 4 MB/DIO/80 MHz,
  valid checksum and valid validation hash. Every copied header and the reconstructed
  generated `.ino.cpp` matched the source at that build, including the private seed.
  The archive contains five images with checked sizes and SHA256 manifest entries.
  See [flashing instructions](FLASHING.md); nothing was flashed to hardware here.
- Standalone helper tests: passed, including retry limits, clock rollover,
  bounded copies, progress, weather mapping, and JPEG MCU resampling.
- Host page previews use actual installed TFT_eSPI font metrics and bitmaps.
  They are software-rendered previews, not photographs of the device.
- Expanded page/header/weather host suite: 152 cases, 47,345 assertions, zero
  failures across 160x128 and 320x240. 142 nonblank rendered frames generated;
  contact sheets and representative individual pages were visually inspected.
- Browser callback suite: 22 local-only tests passed. Actual user consent and
  successful token refresh were also verified separately, not substituted by mocks.
- Actual-function Wi-Fi tests pass for ESP32 and ESP8266, including profile
  merging, preferred order, timing, disconnect/reconnect, socket restart and rollover.
  A version fingerprint in unused EEPROM bytes applies new build credentials once
  without breaking subsequent user password changes or open-network saves.
- Actual-function clock and button action tests pass; malformed clock input leaves
  the last good time intact.
- Eye/context regression: 368 checks per configuration pass in all four size/platform
  combinations. This includes offline thoughts, hourly mood selection, manual leases
  and expression restoration after page/theme re-entry.
- RuView publisher/wiring: three tests pass with synthetic state, not live CSI.
- Provisioning: 22 fixture/fake-UDP tests pass. A real unicast-only probe received
  no greeting from the saved device address; no credentials were sent in that probe.
- Python F/E9 checks pass for the changed service/controller/tools modules.
  The broader legacy style/security lint configuration is not clean repository-wide.

## Remaining Release Checks

This is a code and host-rendering verification pass, not a production
certification. Measure frame timing, minimum free heap, and stack high-water
under real Wi-Fi/TLS, repeated track skips, theme switching, and long uploads.
Confirm button wiring and actual TFT panel offsets on the deskbot after flashing.
No ambiguous serial port was selected or flashed.

Standalone HTTPS currently retains the existing `setInsecure()` policy.
Certificate validation and authenticated secret provisioning remain security release work.
Provisioning uses the existing plaintext LAN UDP protocol; it is unicast, not an
authenticated/encrypted pairing channel. NVS confidentiality needs flash encryption.
Lyrics supplied by a host remain supported; this pass does not add a native
lyrics-provider integration. Legacy UDP artwork lacks transfer IDs, so truly
interleaved transfers cannot be reliably distinguished by the old wire format.
Game artwork's unused reserved channel is isolated from Spotify.

Peripheral radio operation, CSI model quality, and original RuView dashboard
parity are not certified by a display-layout test. The historical all-fixed
claims in `errors.md` are not evidence of hardware acceptance.
