# Standalone Services Integration

The production entry points are in
`arduino/CompanionOS_Main/standalone_services.h`; include that header once from
`companion_net.h`, after the four public OAuth Strings and `encodeBase64()`.
The integrated main sketch already wires these entry points. ESP8266 retains
its legacy implementation behind the existing platform branch.

## Parent Wiring

- Call `tickStandaloneServices()` on the main/Arduino task every loop, including
  when offline, a PC owns playback, or RF operations pause networking. It consumes
  completed jobs even when no new work is allowed. No explicit setup is needed.
- Call `queueStandaloneCommand(cmd)` from main for `PLAY_PAUSE`, `PREV`, `NEXT`,
  and `SHUFFLE:TOGGLE`. If it returns false, retain the existing plain host packet
  send with no new prefix or JSON wrapper. It never forwards a native Spotify
  command; it may send the separate authorization-broker request described below.
- Its return value means the command belongs to standalone routing, not that an
  HTTP operation succeeded. Busy/backoff/full intent-queue drops return true;
  unsupported commands or ineligible standalone mode return false. There are four
  pending intents, each expiring after three seconds. No failed intent is replayed.

### Command Ownership Contract

`queueStandaloneCommand()` returns **ownership acknowledgement**, not execution
success. A recognized command in native mode returns true even when rejected
because credentials are missing, a retry cooldown is active, or all four intent
slots are occupied. The public main-only status explains the rejection immediately
(`Spotify not configured`, `Spotify cooling down`, or `Spotify queue busy`).
An in-flight job may still accept intents into free slots; it does not imply
unbounded buffering or successful execution.

The caller must return immediately on true. **Never forward a locally rejected
intent to the host**, and never retry a toggle automatically: a remote or delayed
second toggle could undo a successful or uncertain first action. Only false may
continue through the parent's normal command routing. This service sends no host
Spotify command packet and provides no Bluetooth fallback. Its separate auth-broker
request does not forward or replay the rejected command.

| Situation | Result for recognized Spotify control |
| --- | --- |
| Wi-Fi and active PC playback lease | false; parent owns routing |
| Wi-Fi and Android host, including active playback lease | true; native owns it |
| Wi-Fi and no playback host | true; native owns it |
| Wi-Fi and only a non-playback host lease | true; native owns it |
| Offline, RF paused, or Spotify disabled | false; parent owns routing |
| Native owns it but queue full, cooling down, or unconfigured | true; visible local rejection, no forwarding |
| Unsupported command, including POMO commands | false; unrelated parent routing |

PC takeover invalidates/cancels queued native intents; it does not forward those
old intents to the PC. General host presence, host playback ownership, and host
weather ownership are separate predicates. No POMO lease behavior is changed here.

### Shared State

- `setupStandaloneServices()` optionally loads ESP32 Preferences before Wi-Fi.
  Tick/dispatch also load once lazily. Absent, wrong-sized, wrong-version, or corrupt
  records never replace boot globals.
- After accepting a changed, validated full `SPOTIFY_AUTH` snapshot, parent calls
  `notifyStandaloneCredentialsChanged()`. This persists live credentials, prevents
  a late NVS load from overwriting them, advances the epoch even if host ownership
  is unchanged, cancels old pending intents, and resets retry gates. In-flight old
  worker results are discarded; a new job follows their completion. Do not call
  this hook for unchanged periodic snapshots.
- `requestStandaloneSpotifyAuthorization()` is an optional explicit SELECT hook.
  Missing credentials and 401/`AuthInvalidClient`/`AuthSignInRequired` automatically
  request authorization at most once per five minutes. PLAY_PAUSE on invalid or
  missing credentials can request immediately, bounded to once per 15 seconds.
  Attempts, including failed sends, consume cooldown. Offline, RF pause, unknown
  or non-unicast controller IP, or disabled Spotify sends nothing.
- The payload is exactly `SPOTIFY_AUTH_REQUEST`, sent on main by UDP unicast to
  `pcIPStr` at `UDP_PORT_TX`, never broadcast or BT. Python owns browser launch,
  callback, and unicast `SPOTIFY_AUTH` delivery. Sending a request does not prove a
  browser opened or that authorization completed. Do not forward the original toggle.
- Preferences namespace `spotify-svc`, key `creds-v1`, stores one bounded versioned,
  CRC-checked full refresh-capable tuple. Only complete printable non-truncated
  refresh/client-ID records are saved. Syntax validation is not server-side OAuth
  verification. Successful rotated tokens save on main; identical snapshots skip
  writes. NVS failure retains live tokens but reports `save failed` / `Spotify
  credentials not saved`. Flash/NVS encryption is required for storage confidentiality.
  No secrets are logged. Mock tests cover limits, exact unicast payload, explicit
  throttling, epoch reset, rotated persistence, failed writes, and corrupt records.

- Public `String standaloneSpotifyStatus` is main-only. It contains brief feedback
  for 401/403/404/429, other failures, and busy/expired/cooling-down command drops.
  Parent may display it when there are no lyrics. Successful playback clears it;
  an empty playback sample reports no active device. No worker writes this String.
- Public `String standaloneWeatherStatus` is also main-only and exists on both
  ESP32 and ESP8266. On ESP32 it reports `Updating` only during a real weather job,
  `Offline`/`Paused` when appropriate, and a timed retry with its error category
  on failure. Empty means native data is ready or host weather owns the display.
  The non-ESP32 no-op API leaves this String to its parent weather implementation.
- Empty access plus missing refresh/client ID is detected on main: no Spotify
  HTTP jobs are sent and commands are acknowledged locally as unconfigured, never
  forwarded as a second host toggle. An access token alone is usable until 401;
  refresh without a secret uses the public/PKCE client-ID form.
- Main owns `unsigned long spotifyProgressUpdatedAt`. Native samples set it to
  the application time after compensating for result age. Parent host packet
  handlers must also set it when accepting progress. The main ticker should
  advance progress by elapsed `millis()`, cap at duration, and update this baseline;
  do not retain an unconditional `playProgress += 1000` ticker.
- The existing main state, credential, weather, and art globals must be declared
  before the include, except the theme-2 state and redraw/reset functions, for
  which the header supplies forward declarations. It expects the parent's new
  `uint32_t albumArtRevision` and `unsigned long albumArtStartedAt` globals.

Native cover results copy directly to allocated `albumArt`, mark all
64 rows complete, increment `albumArtRevision`, clear `albumArtStartedAt`, and
reset both Spotify draw states. They do not call `completeAlbumArt()` or alter
`albumArtIncoming`. Track changes invalidate readiness and reset both themes.
An active incoming host transfer or a changed art revision rejects a native
cover. Full track/artist fingerprints, item URI, ownership epoch, credentials, and queued skip
commands also guard against stale native results.
Display text is UTF-8-boundary truncated rather than blocking dispatch; toggle
state GETs filter out metadata entirely. Long host titles cannot wedge controls.

## Execution and Limits

One FreeRTOS worker, two one-entry byte queues, one exclusively owned job/result
slot, and a 10240-byte worker stack. Main enqueues POD snapshots and consumes
results using zero-wait queue calls. Worker functions never inspect TFT, host/UI
globals, or shared Arduino Strings. All UI updates and redraws run on main.
Weather runs on demand near its tile or on weather-bearing clock views when
`isHostWeatherActive()` is false, independently of general host presence or PC
dashboard/playback traffic. Parent grants its 15-minute weather
lease only on a valid weather payload, using `WEATHER_POLL_INTERVAL_MS`; a missing or
broken PC weather API cannot suppress native weather forever. Ownership bit 32
tracks this lease: a new valid host update invalidates an in-flight native result,
and lease expiry resumes native polling while preserving cached host data until then.
Spotify uses its separate playback lease, including native Android-host mode.
RF pause prevents new jobs and invalidates observed ownership.
Already executing HTTP requests cannot be unsent; their obsolete results are
discarded. Non-ESP32 builds provide no-op entry points.

Response byte caps: playback 24 KiB, token/weather 8 KiB, JPEG 48 KiB. These are
limits, not eager allocations: the decoded-body sink grows in bounded blocks.
The worker's RGB565 cover is allocated only after HTTPS closes, transferred to
the main task without a second thumbnail copy, or immediately freed if stale.
Parser
allocation is capped at 8 KiB via a custom allocator on ArduinoJson v7 and via
DynamicJsonDocument capacity on v6 (not by a v7 capacity argument). Allocator
bookkeeping adds small per-allocation headers; playback also uses a fixed-field
filter document. Oversized fields,
allocation failures, truncated bodies, malformed JSON/JPEG, and unexpected
content encodings fail closed with backoff. Requests use HTTP/1.1 and explicitly
request identity content encoding (unexpected gzip/deflate is rejected). Native
`HTTPClient::writeToStream()` decodes chunked, Content-Length, and close-delimited
bodies into a capped Stream sink. Sink overflow is latched across native short-write
retries. The body sink has a 15-second deadline; the same-worker client bounds
the whole request to 25 seconds. Socket/connect timeouts are eight seconds and
the TLS handshake timeout is fifteen seconds. No main-thread TLS teardown occurs.
GET redirects have a three-hop cap, require HTTPS, and reject userinfo, injected
headers, scheme-relative URLs, and non-443 ports. Authenticated GETs stay on the
same origin; no PUT/POST/control/token redirect is followed.

JPEG decoding uses the installed TJpg_Decoder library's low-level `JDEC` context,
not its global wrapper callbacks. Each job has its own workspace and callback
device pointer. Only staging RGB565 pixels are written on worker. Images up to
1024x1024 are reduced and nearest-neighbor sampled to 64x64, including small or
non-square sources; unsupported/progressive JPEGs keep the placeholder and back off.
`ENABLE_SPOTIFY=false` excludes OAuth, control, cover, and decoder code, leaving
weather enabled. No credentials or response bodies are logged.

## HTTP Policy

- Toggle controls first fetch actual state, then issue explicit play/pause or
  shuffle state writes; next/previous use POST. Success is determined by HTTP
  status. No playback state is changed optimistically; a subsequent GET reconciles.
- At most one refresh and one retry after a definite 401 per job, shared between
  a toggle's read and write. Refresh POSTs URL-encode the refresh token, support
  confidential or public/PKCE clients, and apply rotated tokens only on main if
  the credential snapshot still matches.
- Token endpoint 400/401 bodies are bounded and parsed only for whitelisted OAuth
  categories. `invalid_client` yields `AuthInvalidClient` ("Spotify app credentials
  invalid"); `invalid_grant` yields `AuthSignInRequired` ("Spotify sign-in required").
  Raw server error descriptions are never copied to UI or logs. Both use at least
  a one-minute backoff; expired/revoked credentials still require user reauthorization.
- 429 honors numeric Retry-After (one-minute fallback); commands are dropped,
  not replayed after waiting. Transport failures and ambiguous write outcomes
  also never replay commands. GET polling reconciles later. Other failures use
  capped exponential backoff; 400/401/403 wait at least one minute.
- Observed ownership epochs, including Wi-Fi reconnect, reset scheduling/backoff
  and invalidate in-flight results. Worker diagnostics contain constant service
  labels, numeric HTTP/TLS codes, and `HTTPClient::errorToString()` descriptions
  and native TLS error descriptions. Internal free RAM and the largest contiguous
  block are logged on transport failure. No URL, Authorization, token or HTTP
  body is logged. Generic native error -1 is not mislabeled as a TLS-specific error.
- Spotify playback controls require appropriate OAuth permissions, a controllable
  active device, and account/API eligibility. They do not create a Spotify player
  on the ESP32. TLS still uses `setInsecure()`: certificate validation remains
  known debt, not a verified security property.

Primary protocol references: [Spotify playback API](https://developer.spotify.com/documentation/web-api/reference/get-information-about-the-users-current-playback),
[playback controls](https://developer.spotify.com/documentation/web-api/reference/start-a-users-playback),
and [rate limits / Retry-After](https://developer.spotify.com/documentation/web-api/concepts/rate-limits).

## Internal RAM Policy

The reported device error `-32512` is mbedTLS `MBEDTLS_ERR_SSL_ALLOC_FAILED`
(`-0x7f00`), not an OAuth rejection. `memory_policy.h` protects 80 KiB free
internal SRAM and a 24 KiB contiguous block from optional graphics allocations.
It tries RGB565, then an 8-bit canvas only if that allocation leaves the reserve.
Denied eye/header/marquee allocations are retried after two seconds, so temporary
TLS pressure cannot permanently turn off buffered rendering or title scrolling.
Main trims optional canvases when needed before dispatch, budgeting the worker
stack before its first allocation. Affordable active canvases stay intact to
avoid eye/thought flicker on every playback poll. New graphics allocations are
suppressed until the result returns.
The worker does not reject a handshake using a guessed heap threshold: v1's
40 KiB largest-block gate self-deferred indefinitely at 65,920/38,900 bytes.
The regression reproduces those exact figures and verifies that the request
function invokes its mocked transport. Actual SDK allocation errors still back off normally.
This is a headroom policy, not a guarantee against concurrent SDK allocations;
on-device soak testing remains required.

Only current-page render buffers are retained. Spotify's thumbnail may remain
on its immediate neighboring tiles; it is discarded farther away. The staging
buffer is allocated on transfer, reusing the old thumbnail, and handed over only
after all 64 rows arrive. Custom images are similarly bounded to the eye tile
and its neighbors. Shared header and small live state remain in RAM; executable
code, fonts and read-only assets remain in flash. No claim is made that mutable
network state can be moved into ROM. Normal ESP32 setup calls
`esp_bt_mem_release(ESP_BT_MODE_BTDM)` before Wi-Fi. The explicit radio workspace
reserves Bluetooth instead and suspends standalone HTTPS. Crossing that workspace
boundary restarts with a validated, one-shot RTC page tag (software reset only),
skipping the decorative boot sequence. Commands are never persisted or replayed.
This is not hot-swappable Bluetooth memory; a restart resets volatile timer/UI
state. The API release is irreversible within the current boot.

RuView CSI runtime/UDP storage is allocated on entry and freed on exit, including
callback unregistration and a critical section protecting late Wi-Fi callbacks.
Setup/enable failures do not report an active collector and retry after 3 seconds.
The low-level JPEG decoder and its per-cover workspace are used directly. The
installed Arduino library still links its 3580-byte global wrapper constructor;
no RAM saving from changing that include is claimed. Spotify gets scheduling
priority; weather is eligible only near its tile or on weather-bearing clock views.
Optional host feeds require a renewable `VIEW` lease from the device; Steam is
constructed only when its feed is needed. Fixed small fields are not flash-paged.

`tests/memory_policy_test.cpp` exercises total/contiguous budgets and reduced-depth
selection; `tests/art_memory_test.cpp` exercises 2000 page/transfer cycles with
allocation accounting. `tests/spotify_input_test.py` extracts the real credential
input handler and checks atomic complete tuples, bounds/control/NUL rejection,
one persistence notification per change, duplicate ACKs and preserved host identity.
`tests/csi_lifecycle_test.cpp` adds 2000 collector/UDP open/close cycles, late
callbacks, bounded packet layout, and setup/start failure recovery.
`tests/optional_demand_test.py` checks lease expiry, sender/page validation, quiet
default behavior, adjacency, prompt activation, and lazy Steam initialization.
These tests do not perform an ESP32 TLS handshake.

## Checks

### Native Lyrics And Quiet Spotify

The Lyrics job shares the single worker, after controls/playback/artwork priority.
LRCLIB receives title/artist/album/duration, never Spotify Authorization. A 404
with album retries once without album; a definitive miss/instrumental is cached
for that track. Body/JSON/cache budgets are 24/16/8 KiB, respectively. There is no
permanent text allocation before a fetch. Handed-off text is released off-page
or on track change; stale results are discarded. Timed selection is local and
supports pauses, seeks, offsets, repeated tags and blank instrumental breaks.
Plain text is explicitly marked TEXT. Provider coverage and font shaping are not
guaranteed. Routine refreshes never replace lyric content with status labels.

`standalone_services_test.cpp` covers the real fetch/consume functions using
mock HTTP, ownership, no cross-origin bearer forwarding, cache handoff, page exit,
missing/instrumental/plain/oversize/malformed results, and retries. The detail
test covers LRC timing and bounds. `lyrics_test.py` exercises actual PC methods
and bounded mode-aware bridge frames without OAuth or running services.
UI tests assert zero screen writes over 100 unchanged short-title frames in
all three themes. These are host tests, not a physical-device timing pass.

Host-only helpers:

```powershell
g++ -std=c++11 -Wall -Wextra -Werror CompanionOS/tests/standalone_services_detail_test.cpp -o "$env:TEMP/standalone_services_detail_test.exe"
& "$env:TEMP/standalone_services_detail_test.exe"
g++ -std=c++17 -Wall -Wextra -Werror -Wno-deprecated-declarations -Wno-unused-function -Wno-address -ICompanionOS/tests/standalone_stubs -ICompanionOS/arduino/CompanionOS_Main -I<ArduinoJson-src> CompanionOS/tests/standalone_services_test.cpp -o "$env:TEMP/standalone_services_test.exe"
& "$env:TEMP/standalone_services_test.exe"
```

Repeat the request/state test compile with `-DENABLE_SPOTIFY=0`. These host tests
use the real installed ArduinoJson but mock Arduino, HTTPClient's decoded-stream
contract, TLS, and queues. They test production request/state functions for chunked
weather, bounded overflow/truncation/stall, compression rejection, safe redirects,
TLS errors, 401 refresh/rotation/one-retry budgets, OAuth error categories, huge
metadata controls, no-config acknowledgement, stale metadata rejection, redacted
logs, timed errors, and Wi-Fi reconnect recovery. They do not contact real services,
run actual FreeRTOS threads, or verify TLS certificate authenticity. Native chunk
decoding remains provided by the installed HTTPClient, not the test decoder.
Routing tests separately exercise active PC playback versus Android, standalone
and non-playback host leases, offline/paused/disabled modes, four pending intents
while a worker is busy, visible queue-full/cooldown/unconfigured rejection, and PC
takeover cancellation. A parent-style routing shim counts host forwards and asserts
none occur on any true acknowledgement, including rejected local intents.
Weather lease tests keep PC dashboard/playback presence active, verify native
dispatch without actual host weather, reject a native result on host-weather
takeover, retain host data without duplicate fetches during the lease, then resume
exactly one native fetch on 15-minute expiry despite continued PC traffic.

The optional parent-run compile-only sketch `tests/standalone_compile/standalone_compile.ino` contains
empty credentials and no TFT dependency. Build it with `esp32:esp32:esp32` and
`compiler.cpp.extra_flags=-I<absolute-path-to-arduino/CompanionOS_Main>` using the
installed ArduinoJson and TJpg_Decoder libraries. Also build with
`-DENABLE_SPOTIFY=0`. Do not upload this harness as the real application.

Helper tests exercise rollover-safe scheduling, Retry-After overflow and malformed
values, bounded copies, command parsing, clamped progress, weather mapping, and
MCU-block sampling at native/downscaled/upscaled/non-square dimensions. The
compile harness checks the actual installed ESP32 APIs and both feature gates;
it is not a hardware/network/concurrency test. Parent must compile the integrated
firmware and validate 401/429, host takeover, rapid skips, Wi-Fi loss, malformed
covers, memory pressure, and TFT responsiveness on hardware before release.
