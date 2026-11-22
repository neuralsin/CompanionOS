# Dr.Hack Diagnostic Repairs

## Scope

This pass repairs menus, passive discovery, hardware-status reporting and
receive-only diagnostics. Deauthentication, jamming, credential collection and
other transmit/attack implementations were not upgraded or validated. The
presence of 48 menu entries is not a claim that 48 capabilities work.

No ESP32 firmware build, flashable binary, flashing, live RF test or external
account operation was performed. Existing unrelated changes were preserved.

## Repairs

- WiFi scanner sorted using the total result count after storing at most 20
  records. More than 20 results could access memory past the array. Sorting now
  uses the stored count, and negative scan results are bounded safely.
- Passive WiFi discovery no longer explicitly disconnects the associated network.
  Scan flags request passive discovery and hidden-network results.
- WiFi/BLE visible row counts now match the actual screen/footer space. Scroll
  positions and counts are bounded, long names are fitted, and the BLE RSSI column
  reserves its measured width. Names and list rows no longer collide or overflow.
- Menu labels use two bounded lines instead of colliding with numeric indices.
  The category/page indicator fits its header, and the selected full name appears
  in the footer. The blocking entry scanline was removed.
- Virtual left/right were ignored on the passive result pages. They now route to
  list navigation; select returns to the menu, while long select on the menu exits.
  Opening presses do not immediately close the passive receiver/input monitor.
- System Info previously used large-screen row spacing and the selected menu item
  opened a separate reduced, blocking screen. It now uses the actual bounded
  nine-row renderer. About and Hardware also remain responsive page-driven screens.
- BLE handles missing controller/results without dereferencing null or leaving
  stale results behind. Its passive scan releases stack objects using reversible
  `BLEDevice::deinit(false)` cleanup, retaining the ability to initialize again.
- Hardware diagnostics deselect all shared SPI devices before probing, keep nRF
  CE lines low, check chip connectivity and power radios down afterwards.
  A missing optional second nRF is not treated as proof that the first failed.
- CC1101 status is based on read-only part/version register responses, rejecting
  invalid/unready results. Merely displaying configured pin numbers is no longer
  presented as successful detection. IR/GDO0 values are raw GPIO levels, not proof
  of a connected or functional sensor.
- Receive-only nRF spectrum peaks are clamped to the plot, preventing old peaks
  from painting the header. The 3200-byte waterfall history exists only during
  that tool's lifetime and is released on exit. Exit powers down the receiver.
- Input Monitor starts from actual pin levels, counts observed changes rather
  than inventing an initial event, and does not repaint while input is unchanged.
  Passive monitor/receiver cancellation respects Home and does not redraw the
  toolkit menu over the destination page.
- Hardware errors wrap multiline messages within bounded content space instead
  of drawing an unbounded centered string. Empty errors have a real fallback.

## WiFi/BLE Runtime Follow-Up

- Verified the installed Arduino-ESP32 3.3.8 SDK: the blocking BLE scan overload
  returns a result pointer even if scan startup fails. Both passive BLE pages now
  use the asynchronous boolean-start API, check initialization/startup, poll with
  a deadline, and accept cancellation while servicing remote input.
- WiFi discovery uses asynchronous passive scanning with a six-second deadline.
  Rejected starts, initialization failures, timeouts and cancellation have distinct
  status messages rather than appearing as zero nearby networks.
- Passive WiFi scans lease the connection state machine, cancelling an incomplete
  association without erasing saved profiles. An existing connection is left
  intact; reconnect attempts resume after the lease is released on every exit.
- BLE advertisement callbacks deduplicate into 20 fixed-size records. On the
  installed classic ESP32 Bluedroid backend, duplicate callbacks avoid the SDK's
  growing result map. Later packets without metadata do not erase names already
  observed. The callback buffer is detached under a cross-task lock before being
  freed, so late callbacks cannot use released storage.
- BLE initialization failures clean up partial Bluedroid/controller state. Normal
  completion, startup errors, timeouts and cancellation release the BLE stack
  reversibly. No `deinit(true)` or permanent BT-memory release is added.
- BLE Inspector now shares that scan path and uses page-driven list/details
  controls. It no longer retains the BLE stack in a blocking UI loop. Detail
  fields report advertised UUID counts, not a fabricated GATT-service inventory.
- Passive channel/RSSI tools share the WiFi discovery error handling. Home stops
  their UI from repainting the destination page; four-position sampling waits
  respect cancellation and timeout. Radar signal values fit their column, and
  the graphic no longer invents a rotating bearing or claims RSSI is distance.
- The former "WiFi Config" page is honestly labelled "WiFi Status". It is
  nonblocking and displays the live associated SSID rather than an old compiled
  default. It is not presented as an interactive configuration editor.
- The separate legacy WiFi scan helper used by a transmit tool was not upgraded
  in this follow-up. Attack/emitter bodies and their target selection are unchanged.

## Evidence

Run from the repository root:

```powershell
python -B CompanionOS/tests/dr_hack_diagnostics_test.py
python -B CompanionOS/tests/button_actions_test.py
python -B CompanionOS/tests/wifi_connectivity_test.py
python -B -m unittest discover -s CompanionOS/tests -p '*_test.py'
ruff check CompanionOS/python CompanionOS/tools CompanionOS/tests --select E9,F63,F7,F82
```

- 116 diagnostic layouts at 160x128 and 116 at 320x240 passed, including all 48
  menu selection positions at each size; 16,849 layout checks passed in total.
- Actual source functions are extracted and executed, rather than reimplementing
  their rendering/scan loops. Transmit/attack bodies are excluded; attempting to
  dispatch an excluded tool fails the test.
- Fixtures cover 100 discovered WiFi records against 20 stored slots, 2000 BLE
  advertisements against a 20-record capture, invalid scrolls, BLE start rejection,
  scan/init failure, timeouts, Home/Select cancellation, late callbacks, duplicate
  advertisements with missing metadata, list/detail navigation and live SSID,
  single/absent/dual nRF responses, CC register failures, quiet/changing GPIO,
  receiver mode switching, peak clipping, opening-press handling and Home cleanup.
- Real virtual-button handling and physical SELECT parity pass native tests.
  Connection-lease/retry tests pass for ESP32 and ESP8266 host configurations,
  including connected-link preservation and no profile writes during discovery.
- 100 existing Python unit tests pass; critical Ruff checks and `git diff --check`
  pass. Native diagnostic compilation uses `-Wall -Wextra -Werror` with test-only
  unused-function/variable/sign-compare suppressions.
- Representative rendered menu, result, System Info, Hardware, spectrum, BLE
  Inspector, channel, RSSI and WiFi Status frames were visually inspected. They
  are host raster output, not TFT screenshots.

## Hardware Gates

SPI probe success does not establish antenna performance or actual RF reception.
The repaired passive WiFi/BLE acquisition waits are polled and cancellable; BLE
stack initialization/teardown itself still calls synchronous SDK APIs. The SDK
WiFi result cache can still allocate for all discovered APs; only application
copies are capped. BLE cache bounding was verified against Bluedroid, not NimBLE.
IR protocol capture/replay and CC1101 receive/transmit tools beyond the read-only
presence probe were not repaired here. Four-position RSSI comparison cannot prove
a physical bearing, and a passive BLE scan cannot discover non-advertising devices
or names provided only in active scan responses.
The nRF spectrum displays energy-detector hit counts, not calibrated RSSI or
decoded WiFi/Bluetooth packets. Actual module wiring, power stability, SDK radio
coexistence, repeated BLE initialization and post-tool network recovery require
testing on the user's device.
