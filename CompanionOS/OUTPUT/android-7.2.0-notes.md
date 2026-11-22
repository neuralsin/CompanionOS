# CompanionOS 7.2.0

Built 2026-11-09. APK: `CompanionOS-7.2.0.apk` (7,377,945 bytes).

## Eye corrections

- The absent LDR is disabled by default; disabled sensor inputs are not sampled.
- Idle yawns finish after three seconds and cannot immediately retrigger.
- Manual expressions remain selected until `EMOTION:AUTO` is requested.
- Manual appearance changes wake automatic sleepy mode without cancelling an explicitly chosen Sleepy expression.
- Agent activity no longer overrides manual expressions.

## Android changes

- Offline bundled interface, local icons, 18 tile selectors, actual selected-page feedback, remote buttons, theme/expression controls, photos, music controls and focus timer commands.
- Spotify client ID, refresh/access tokens and optional imported-token secret are editable. Blank secret fields keep existing saved values. WiFi profiles can also be updated.
- Spotify browser authorization uses PKCE and validates state. Register `companionos://callback` in the Spotify application's redirect URI list before authorizing.
- Credentials are stored with Android Keystore-backed encryption, excluded from backups, and sent only to a selected/discovered private LAN address after confirmation. No credentials are shipped as defaults. Rotate previously exposed credentials.
- Native bot playback is the default. Phone media/weather/crypto relays are opt-in; Bluetooth fallback and automatic dummy task/social/gaming feeds are not started.
- The old GitHub feed is not exposed as a working bot control: its `GITHUB:` packet has no receiver in the current firmware.
- Playback commands now match the firmware's command path. Android receiver flags and foreground-service type were corrected.

## Deployment and limits

- Android 8.0+ is declared supported; target SDK 35. The APK contains no native ABI-specific libraries. Bluetooth hardware is optional.
- Signing certificate matches `CompanionOS-Master-v7.1.apk`; that release can be updated without uninstalling. Old debug-signed installations may have a different certificate.
- Install the updated firmware source once for eye fixes, Auto mode, corrected playback commands and the screen endpoint. No firmware binary was generated.
- Subsequent Spotify/WiFi changes use the existing persistent configuration protocol and do not require reflashing. A lost UDP acknowledgement is reported, not treated as success.
- Screen preview reads actual TFT pixels as bounded BMP snapshots, not an emulated UI. It needs working display MISO/readback wiring. Unsupported readback is reported, and captures pause during TLS work. This is not a full-frame-rate video mirror.
- LAN control/provisioning is still cleartext and unauthenticated at the firmware transport layer. Use a trusted private network; do not expose ports 8080/8888/8889 to the Internet.
- Tiles requiring a PC host, external services or additional hardware still require those dependencies. Navigation access is not proof that every physical tool works.

## Verification

- Signed release build and four Android protocol unit tests passed.
- Android lint: zero errors, 26 warnings (including dependency/target metadata, existing icons/resources and intentional LAN cleartext support). See `android-lint.html`.
- 54 browser UI checks passed at 320, 393 and 720 px widths, with no external assets; these use a mock native bridge.
- Eye host tests passed at both display sizes and both platform configurations; 1,468 parent-loop context checks passed.
- Full sketch syntax passed against the installed ESP32 3.3.8/TFT libraries, without generating firmware.
- The APK installed on a disposable emulator, but its Android system UI crashed independently. A successful app launch, live Spotify authorization, TFT capture and physical bot behavior remain unverified. The disposable emulator was stopped and removed.
- SHA-256: `FA4999F427651D0E7A3A057CC499BF2A02388FCC9417D048A4556BCCF3F7D9D2`.

Rebuild with `powershell -NoProfile -ExecutionPolicy Bypass -File CompanionOS/tools/build_android.ps1 -Offline` after dependencies are cached. Generated intermediates use a temporary directory to avoid OneDrive file locks.

References: [Spotify PKCE](https://developer.spotify.com/documentation/web-api/tutorials/code-pkce-flow), [Android foreground-service types](https://developer.android.com/develop/background-work/services/fgs/service-types).
