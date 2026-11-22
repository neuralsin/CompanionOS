# Apple Design And Lyrics Audit

Completed 2026-10-18. Target: classic ESP32, ST7735 160x128; host geometry checks
also cover 320x240. The requested emilkowalski/skills apple-design skill is
installed at C:/Users/shaan/.codex/skills/apple-design and was read and applied.

## Applied Findings

| Finding | Change | Verification |
| --- | --- | --- |
| Native Spotify never fetched lyrics | LRCLIB metadata lookup in the existing worker, independent of Spotify authentication | Request/state fixture tests, public HTTP endpoint smoke check |
| Every poll replaced the lyric area with Updating / Now playing | Silent ordinary polling; only actual errors/actions have status | 100 unchanged frames cause zero screen writes, all three themes |
| Text and small panel styling competed with artwork | Neutral black, restrained accent, readable secondary text, flat surfaces, single artwork outline | Rendered software previews inspected |
| Time labels/short titles repainted at frame rate | Cache time text and avoid marquee work for fitting titles | Quiet-playback pixel-write regression |
| Lyrics could arrive after a skip | Epoch, item identity, track/artist and page ownership checks | Stale result discarded in native tests |
| Unsynced text was a placeholder in the PC helper | Actual reading pages explicitly marked TEXT; same distinction in native lyrics | Native + Python fixtures and all-theme geometry checks |
| PC helper called all missing lyrics Instrumental | Only provider-confirmed instrumental records get that label | Python fixtures |
| PC intro displayed the first verse early | Upcoming line is a preview; current line stays empty until its timestamp | Boundary/seek tests |
| Search could select another artist's first result | Require title and artist match before accepting a search result | Unrelated-first-result fixture |

The firmware fetches only near the music tile, retaining at most 8 KiB of text.
The response cap is 24 KiB; the temporary JSON allocator cap is 16 KiB after TLS
closes. Some long responses can be rejected earlier by that budget. Cache handoff
reuses the response allocation; no permanent lyric framebuffer/index table is
added. No lyrics are fetched on the display thread. Controls/playback/artwork
remain higher priority. An album mismatch gets one metadata lookup without album,
still constrained by title, artist and duration. Ordinary absence is cached for
the track, not retried every poll. Transport failures use a backoff of at least
30 seconds. Selected persistent malformed/oversize responses are not spam-retried.

Synced lyric selection handles intro, blank timed breaks, pause, backward/forward
seeks, repeated tags, offsets and UTF-8-safe truncation. Plain lyrics are reading
pages, not generated timestamps passed off as synchronized lyrics. The PC bridge
sends explicit mode/status fields and still accepts legacy lyric-array senders.

## Design Judgment

Applied the skill's restraint, stable placement, contrast, feedback on meaningful
events, and low-latency update principles. Kept existing navigation and theme
identities. Did not add blur, large scene transitions, spring motion, fabricated
audio meters, new full-screen framebuffers, or asset-heavy decorative surfaces.
Those would compete with legibility, input latency and TLS SRAM on this hardware.

This is an Apple-guided finish, not a pixel-identical iOS port. The bitmap font
does not provide SF typography or universal script shaping. Transliteration/
fallback remains limited to the existing font path. Geometry regressions cover
music, weather, focus, notifications, notes, stocks, gaming, social, schedule,
network, settings, clock/watch, and shared header/components. Physical RF tools,
PC telemetry and real CSI sensing are not certified by these host render tests.
No offensive radio-tool behavior was changed or tested in this pass.

## Acceptance

- 87 Python fixtures pass, including eight new lyric/bridge regressions.
- Native request/state tests pass with Spotify enabled and disabled.
- Native LRC parser tests cover timing, offsets, seeks, repeated/empty lines and truncation.
- UI regression covers 80 cases per resolution, including all-theme quiet/plain lyric states.
- Eye/thought tests and context tests pass in all four size/platform host configurations.
- Targeted Ruff F/E9 checks pass for changed Python helper/test modules.
- Final firmware/source/image/ZIP checks are recorded in firmware/manifest.json.

The deskbot is on another computer. Actual panel flicker, provider catalogue
coverage, lyric accuracy and playback synchronization still require a flashed
device check; no physical-device pass is claimed.

References: https://github.com/emilkowalski/skills/tree/main/skills/apple-design
and https://lrclib.net/docs . The provider receives public track metadata only;
Spotify bearer credentials are never sent to LRCLIB. Existing setInsecure TLS
certificate-verification debt is unchanged and this is not a security certification.
