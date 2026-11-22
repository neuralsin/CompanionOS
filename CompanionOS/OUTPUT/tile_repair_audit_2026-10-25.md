# Structural Tile Repair Audit

## Scope

Source repairs and host verification only. No Arduino build, flashable binary,
ZIP, flashing, external-account mutation or RF attack was performed in this pass.
Existing unrelated staged/unstaged changes and private configuration were kept.
This is not a claim that every feature is production-ready on the physical device.

## Root Causes Repaired

- Low-memory Soft/Robo paths substituted Classic eyes. They now use the selected
  renderer directly when a canvas is denied; model state survives buffer eviction.
- Generic idle/yawn emotion calls entered the Classic renderer in other themes.
  Dispatch now follows the selected theme, with wrong-theme entry guards.
- Classic sprite footprints erased the thought box before repainting it. Face,
  decoration and thought painting now have separate regions; emotion changes no
  longer clear the thought band. The tail points toward the right eye.
- Compact Classic brows/mouth used large-screen fixed offsets. Their positions
  and expanded-eye heights now fit the available face region. Theme changes reset
  transition/blink state without making every temporary canvas eviction a reset.
- Focus digits/labels could collide or exceed the ring. Timer text is fitted,
  sessions bounded, duration changes invalidate the cache, and paused progress
  remains visible. Invalid timer packets cannot renew the host-control lease.
- Host timer resume reset elapsed work; reset was unhandled; skip counted as a
  completed session. These command paths now preserve or reset state explicitly.
- Shorter stock/recent-game lists retained old rows; numerical casts wrapped
  progress/playtime/counters. Lists are cleared and values bounded before display.
- Identical notification polls repeatedly restarted the banner. Only a changed
  leading notification triggers it. Wrong-shaped notes/notification data is ignored.
- Social deduplication suppressed recovery and empty-source updates. Active feeds
  resend real content or an empty card; unchanged packets do not force repaint.
- Notes resync reset its timer but kept its equality cache, leaving rebooted devices
  blank. Reconnect now invalidates the content cache too.
- Local task fallback invented a demonstration schedule and could crash on bad
  rows or list future tasks out of order. New files are empty; invalid/done rows
  are ignored and future tasks are chronological. Existing task files are untouched.
- RuView's receiver defaulted off despite tile entry. It now defaults enabled,
  with both receive/publish loops gated by the active tile lease and its off switch.
  Small UDP probe echoes create real return traffic; they never count as samples.
  Expired CSI clears occupancy/confidence, and the tile has a 12-second feed timeout.
- RuView's zone/status/variance/PPS text could overlap. Labels are fitted, values
  bounded, and its unchanged-state checks avoid repeated heap-backed String keys.
- Partial PC dashboard CSV packets were accepted as complete. Only the original
  17-field or complete extended 23-field forms are accepted.
- The small Dr.Hack home indicator's label extended below the compact display.
  Its geometry now stays within the reserved footer; attack logic was not changed.

## Tile Matrix

"Host" below means actual source execution with test hardware/services, not a
connected ESP32, TFT, account, game or room. Previous repairs were rechecked where
covered; the matrix does not imply every row needed new source changes this pass.

| Tile | Current source/host evidence | Remaining live gate |
| --- | --- | --- |
| Eyes | All 3 engines; low-RAM pixel equality; thought intermediate-frame stability; time/light/manual moods | Panel SPI timing and real heap pressure |
| Spotify | Art transactions/reentry, lyrics, control/auth parser and mocked native request state | Real Spotify TLS/playback/lyrics and rapid track changes |
| Focus | Layout extremes; pause ring; validated host packets; resume/reset/skip | Physical button timing and long offline session |
| Weather | Real icon/layout fixtures, stale/failure states; mocked native requests | Live weather provider and actual TLS heap |
| Notifications | Layout, shrinking list, duplicate-banner suppression | Windows notification permission/capture |
| Notes | Layout, packet shape, shorter list, reconnect cache repair | Live edited-note delivery after device reboot |
| Stocks | Layout extremes, cleared watchlists, validated array shapes | Configured quote provider and account/network availability |
| Gaming | Layout, recent-list reset, numeric bounds; process-detection fixtures | Actual running game and host process access |
| Social | Layout, empty/reconnect feed delivery | Notification capture; social-network write APIs are not implemented |
| Productivity | Layout/progress bounds, real local-task parsing/order/no demo schedule | Google authorization/timezone behavior; task-completion command remains log-only |
| Network | Layout and offline/connected fields; WiFi retry state tests | P202_4G association, DHCP and RF environment |
| Settings | Common layout/scroll/theme controls | ESP32 sensor and nRF24L01 electrical/probe checks |
| PC Dashboard | Real renderer, all 4 subpages, missing/extreme readings, timeout | Actual telemetry sender and machine sensor availability |
| RuView | Real renderer/parser, lease-gated receiver, echo/no fake samples, stale-state timeout; 2000 collector open/close cycles | Actual CSI packets, room calibration; exact upstream web-dashboard migration is not certified |
| Clock Dashboard | Real layout, clock/media/lyrics refresh fixtures | Wall-clock synchronization on device |
| Retro Watch | Real dial/layout/time/media refresh fixtures | Display cadence and physical panel |
| Dr.Hack | Follow-up [passive diagnostic repairs](dr_hack_diagnostics_audit_2026-10-25.md), with 200 layout cases | Live hardware unverified; transmit/attack capabilities not upgraded or certified |

## Verification

- UI: 104 cases at 160x128 and 104 at 320x240; 51,907 assertions, zero failures.
  PC Dashboard and RuView now execute their actual renderers, not empty stubs.
- Eye raster: all 3 themes at both sizes, buffered/direct pixel comparison,
  stable thought footprint before scheduler repaint and through emotion changes.
- Eye lifetime/context: both sizes and ESP32/ESP8266 host variants passed;
  368 context checks per configuration.
- Python: 100 unit tests passed, including local loopback authorization fixtures.
- Native actual-handler tests passed for tile data, virtual/physical buttons,
  WiFi offline retry/migration, clock input and Spotify authorization input.
- CSI lifecycle: 2000 allocation/release cycles passed; probes do not fabricate CSI.
- Artwork ownership: 2000 transfer/page cycles passed without retained allocations.
- Memory policy, standalone detail parsing and mocked standalone request/state
  tests with Spotify both enabled and disabled passed.
- Ruff critical checks (`E9,F63,F7,F82`) passed across Python, tools and tests.
  `git diff --check` passed. ArduinoJson 7 compatibility deprecation warnings remain;
  this pass did not migrate the firmware's JSON API or claim a warning-free build.

Native test runners require local MinGW and the installed TFT_eSPI/ArduinoJson
headers. The Windows restricted sandbox cannot launch this MinGW correctly;
approved host test runs used the normal execution environment. External provider
calls are mocked, and rendering uses a host rasterizer rather than a TFT screenshot.
