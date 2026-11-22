# Personalized ESP32 Firmware

Use `CompanionOS-ESP32-LYRICS-UI-v3-private.zip` for native lyrics and the UI
finish. It retains v2's Bluetooth/RAM fix and removes the routine Spotify
"Updating" / "Now playing" alternation. Older v1 builds self-deferred TLS.

This build targets a classic ESP32 with 4 MB flash and an ST7735 BLACKTAB
128x160 panel rotated to 160x128. It is not an ESP32-S3/C3 or ESP8266 binary.
TFT pins: MOSI 23, SCLK 18, CS 5, DC 2, reset 4. Buttons: GPIO 13/14/27 to GND.
The Wi-Fi access point must offer 2.4 GHz service for this classic ESP32.

The `firmware` folder contains personalized account and Wi-Fi credentials.
Keep it private. Do not upload its binaries or `spotify_credentials.local.h`
to a public repository, issue, or shared download.

## Flash

Use the four separate files to preserve the existing NVS region. Do not run
an erase-flash command. The generated `flash_args` records the actual offsets.
With Espressif esptool 5 installed, run from the firmware directory and replace
`COM7` with the serial port on the device-hosting computer:

```powershell
python -m esptool --chip esp32 --port COM7 --baud 460800 write-flash --flash-mode keep --flash-freq keep --flash-size keep 0x1000 CompanionOS_Main.ino.bootloader.bin 0x8000 CompanionOS_Main.ino.partitions.bin 0xe000 boot_app0.bin 0x10000 CompanionOS_Main.ino.bin
```

No Python controller is needed for normal on-device Spotify, weather, local
thoughts, clock after synchronization, or the focus timer. The Python controller
is still needed for PC telemetry, game detection, CSI processing, host feeds,
and future browser reauthorization requests.

This uses the 3 MB `huge_app` application partition, not the smaller default
application partition. There is no second OTA application slot. The supplied
merged image is a full-flash alternative and can overwrite saved NVS/filesystem
contents; prefer the separate files above.
`manifest.json` and `SHA256SUMS.txt` identify this package. The image's application
descriptor can retain the precompiled Arduino core's vendor build name/date;
it is not the timestamp of the current CompanionOS source build.

## First Boot Checks

1. Confirm `[memory:LYRICS-UI-v3]` appears in the 115200-baud serial log, including
   `Bluetooth release=0` and the actual reclaimed/free/largest memory figures. Confirm
   P202_4G joins before Stuart. The face stays usable while connecting.
2. Disconnect the router: local eyes/thoughts/focus remain available; background
   retries cycle known profiles every roughly 2-3 minutes without Bluetooth fallback.
3. Start Spotify playback on a controllable device. Check track/artist, progress,
   artwork, transport controls, and a rapid track change in each theme.
   Lyrics now come directly from LRCLIB without a Python helper. Check a song
   with available timed lyrics, pause, seek backward/forward, then skip tracks.
   Intros preview the upcoming line; instrumental breaks clear the previous line.
   Plain lyrics are reading pages marked TEXT, not fabricated synchronized lyrics.
   Unavailable/instrumental results are honest stable states, not polling labels.
4. Open Weather: its request runs on demand, after Spotify. Check it receives
   data and keeps the last sample during a disconnection.
5. Check the clock, all three eye themes, physical buttons, and virtual controls.
6. Move well away from Spotify and return. Artwork should reload; it is not kept
   in RAM on every tile. Check the serial log for any `Out of memory` or `TLS
   deferred` message; v2 no longer has a `TLS deferred` path. Capture numeric TLS
   errors and free/largest block sizes if a real handshake fails.

V2 releases unused Bluetooth controller/host memory before starting Wi-Fi.
Bluetooth is not an offline fallback. Explicitly opening the radio workspace
restarts into its reserved-memory profile; leaving it restarts into normal mode.
The target tile is a one-shot RTC request, not an EEPROM write or an auto-resumed
tool command. Workspace switches skip the long boot animation. Live UI state
such as a running focus timer is reset by these workspace restarts.
This restart is necessary because Espressif's full BT memory release is not
reversible in the same boot. No attack-tool behavior is changed or certified.

Spotify runs before weather. RuView allocates CSI/UDP state only on entry and
frees it on exit. The optional PC telemetry listener follows its tile/neighbors.
Updated Python helpers use `VIEW` leases to stop stock/game/social/task/PC feeds
when not needed; Steam tracking initializes on first demand. Restart that helper
after updating its source if using those PC-only features. Spotify needs no helper.

Graphics preserve an 80 KiB total/24 KiB contiguous headroom budget, and cannot
allocate new canvases while the HTTPS worker is busy. These are graphics limits,
not TLS admission checks. TLS always attempts the bounded real handshake, and
reports actual SDK errors. Code and immutable assets remain in flash; small
mutable page state and networking still require SRAM. Not all RAM can become ROM.
The real-device handshake remains unverified until the device is flashed.

V3 retains at most 8 KiB of lyrics near the music tile; the HTTP response is
capped at 24 KiB, and the temporary JSON parse budget is 16 KiB after TLS closes.
Requests have shorter lyrics-specific timeouts. Spotify controls/playback and
artwork take scheduling priority; late lyrics from skipped tracks are discarded.
Cached lyrics continue locally during a Wi-Fi interruption. Catalogue coverage,
lyrics accuracy, and scripts unsupported by the installed bitmap font are not
guaranteed. The Apple Design skill was applied to clarity, restraint, contrast,
stable placement and low-latency updates; a 160x128 bitmap panel cannot be a
pixel-identical iOS/SF-font/glass renderer. See UI-AUDIT.md in the package.
If using the PC helper, restart its updated Python source for truthful lyric
status, real plain-text lyrics, and the corrected intro timing.

Spotify authorization was verified on the build computer. Hardware radio,
panel wiring, account playback eligibility, and device rendering still require
these physical checks. A cold boot without any clock source shows `--:--`
rather than inventing the time.
