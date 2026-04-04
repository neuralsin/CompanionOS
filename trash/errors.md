# CompanionOS V4 - Technical Audit & Error Log

## [CRITICAL] Memory Corruption: Sprite Clipping in `eyes.h`
**File:** `eyes.h` (line 129-132)
**Status:** ✅ FIXED
**Description:** `eyeSpr` was allocated once with the first call's dimensions. If the user switched from Legacy to Exotic mode, asymmetric sizing could request a larger sprite, writing out of bounds.
**Fix Applied:** Sprite now tracks its allocated dimensions (`eyeSprAllocW`, `eyeSprAllocH`) and reallocates with 10% headroom if a larger size is requested.

## [MAJOR] Missing Global Definition: `weatherCode`
**File:** `globals.h`, `pages.h`
**Status:** ❌ NOT A BUG
**Verification:** `weatherCode` is declared `extern` in `globals.h` (line 80), but it IS defined as `int weatherCode = 0;` in `pages.h` (line 30). The linker resolves this correctly. No fix needed.

## [MEDIUM] State Race Condition: `isPetting`
**File:** `touch.h`
**Status:** 🟡 ACCEPTED RISK
**Verification:** Both `checkPhysicalSensors` and `handleTouch` guard on `!isPetting` before setting it to `true`, and the 400ms debounce on physical sensors makes simultaneous activation extremely unlikely in practice. No fix needed.

## [MINOR] Performance Bottleneck: `drawAuroraBackground()`
**File:** `eyes.h` (line 385)
**Status:** 🟡 ACCEPTED (mitigated)
**Verification:** The `x += 4` stride already limits iterations to ~80 per frame, not 240. The `sin()` calls are the bottleneck but are acceptable at 20 FPS on 160MHz. Could be improved with a LUT but not critical.

## [MINOR] Sprite Leaks: `zSpr`
**File:** `eyes.h` (line 325)
**Status:** ❌ NOT A BUG
**Verification:** `zSpr` is `static` and allocated exactly once. No leak. No fix needed.

## [MAJOR] Binary Pixel Corruption: Sign Extension
**File:** `network.h` (line 157)
**Status:** ❌ ALREADY FIXED
**Verification:** Line 158 already has explicit `(unsigned char)` casts: `dest[i] = (unsigned char)udpBuffer[offset] | ((unsigned char)udpBuffer[offset+1] << 8);`. No additional fix needed.

## [CRITICAL] Performance: O(N²) Vector Ring
**File:** `pages.h` (line 392)
**Status:** ✅ FIXED
**Description:** `drawSmoothRing` used a 111×111 brute-force loop (12,321 iterations) with `sqrt()` and `atan2()` per pixel per frame.
**Fix Applied:** Replaced with midpoint-circle algorithm that iterates only over actual ring pixels (~700 iterations). Still uses `atan2` for angle but on 10x fewer pixels.

## [MEDIUM] Visual Glitch: Incomplete Flash Recovery
**File:** `pages.h` (line 629)
**Status:** ✅ FIXED
**Description:** `updateFlashNotification` was calling `drawEyes()` which missed starfield, aurora, and status bar restoration.
**Fix Applied:** Now calls `drawEyesPage()` which fully rebuilds the page including background effects and status bar.

## [PYTHON] `fast_poll_now` NameError
**File:** `companion_controller.py`
**Status:** ✅ FIXED
**Description:** `fast_poll_now` was referenced in `command_listener()` (background thread) but only initialized inside `main()`. Pressing any Spotify control button crashed the controller with `NameError`.
**Fix Applied:** Moved `fast_poll_now = False` to module-level scope (line 63) and added `global fast_poll_now` to `command_listener()`.

## [CRITICAL] RAM Exhaustion: Duplicate Buffers
**File:** `globals.h` (lines 42, 74)
**Status:** ✅ FIXED
**Description:** `albumArt[9216]` and `galleryImage[9216]` were separate 18KB arrays despite using the identical 96×96 RGB565 format and never being displayed simultaneously.
**Fix Applied:** `galleryImage` is now a pointer alias to `albumArt`, saving 18,432 bytes of RAM.

## [MAJOR] Physics Logic: Mood Increment Spam
**File:** `touch.h` (lines 50, 139)
**Status:** ❌ NOT A BUG
**Verification:** Both touch paths guard with `if (!isPetting)` before setting `isPetting = true` and incrementing mood. The mood only increments once per long-press cycle, not every frame. The `!isPetting` check prevents re-entry.

## [MINOR] Starfield Boundary Clipping
**File:** `eyes.h` (line 77)
**Status:** ✅ FIXED
**Description:** Starfield Y range was `seed % 150 + 30` (max 180), leaving the bottom 60px of the 240px screen empty. Parallax shifts had no lower boundary clamping.
**Fix Applied:** Changed to `seed % 210 + 16` (range 16–226, below status bar) with clamped bounds.

## [MINOR] Blink "Void" Marks
**File:** `eyes.h` (line 843)
**Status:** 🟡 ACCEPTED
**Verification:** The blink animation uses `fillRect(COLOR_BG)` to simulate eyelids closing. In exotic mode this briefly erases aurora pixels, but the blink lasts only ~200ms (4 frames) and `drawEyes()` is already called on blink completion to fully restore the background. Fixing mid-blink would require redrading aurora+starfield every frame during blinks — an unacceptable performance cost for a 200ms visual artifact.

## [MAJOR] Python: PowerShell String Overflow
**File:** `companion_controller.py` (line 193)
**Status:** ✅ FIXED
**Description:** Windows notifications with null `AppId` or empty `Content.GetElementsByTagName('text')` would crash the scraper thread.
**Fix Applied:** Added null guards (`if ($n.AppId -eq $null) { continue }`) and text element count check before accessing `.InnerText`.

## [MINOR] Variable Scope Inconsistency: `artDrawn`
**File:** `network.h` (line 28), `pages.h` (line 116)
**Status:** ❌ NOT A BUG
**Verification:** Arduino IDE compiles all `.h` files into a single translation unit. `extern bool artDrawn;` in `network.h` correctly forward-declares the definition `bool artDrawn = false;` in `pages.h`. Standard pattern for this codebase.

## [CRITICAL] V6: Missing Forward Declarations for Cross-Page Variables
**File:** `page_stocks.h`, `page_gaming.h`, `page_social.h`, `page_productivity.h`
**Status:** ✅ FIXED
**Description:** `blendColor` and `drawPageIndicator` were defined in `eyes.h` and `ui.h` but called in newly inserted page headers before the compiler fully processed their translation scopes, leading to implicit declaration bounds errors.
**Fix Applied:** Migrated `extern void drawPageIndicator` and `extern uint16_t blendColor` directly to the file-scope header of every new V6 dashboard page.

## [MAJOR] V6: Fragile Cross-Page Define Referencing
**File:** `page_gaming.h`
**Status:** ✅ FIXED
**Description:** The gaming page relied on `STK_RED` which was defined inside `page_stocks.h`. While this worked due to alphabetized/manual `pages.h` inclusion order (`page_stocks.h` first), it was extremely fragile.
**Fix Applied:** Hardcoded the fallback hex `0xE8C4` directly into `page_gaming.h` to decouple the pages.

## [MINOR] V6: Redundant Scope `extern`
**File:** All new `page_*.h`
**Status:** ✅ FIXED
**Description:** Redundant `extern void drawPageIndicator(int current, int total);` scattered across 8 separate rendering functions internally.
**Fix Applied:** Extracted to the top of the header files. Validated time-variable links (`displayHour`, `timeReceived`) against `globals.h`. No compile linkage warnings remaining.

## [CRITICAL] V6: Network Buffer `strncpy` Null Pointer Dereference
**File:** `network.h` (lines 350-430)
**Status:** ✅ FIXED
**Description:** Calling `doc["key"].as<const char*>()` on a null JSON value returns `nullptr`. Passing `nullptr` as the source string to `strncpy` causes an immediate hardware exception and ESP reboot. While `doc.containsKey()` checks for the key's existence, it does not guarantee the value is non-null.
**Fix Applied:** Wrapped all V6 `strncpy` extractions in a `const char* param = doc["key"]; if (param) { strncpy(...) }` pattern to safely absorb nulls.
## [CRITICAL] Compiler Error: Enum Out of Bounds (`Emotion`)
**File:** `theme_eyes.h`, `eyes.h`
**Status:** ✅ FIXED
**Description:** Extracted all legacy V4 rendering functions into a separate `theme_eyes.h` file, but `enum Emotion` was still defined inside `eyes.h`. Since `eyes.h` includes `theme_eyes.h` at the top, the compiler threw "does not name a type" errors because `Emotion` was parsed after it was required.
**Fix Applied:** Migrated `enum Emotion` and `extern Emotion currentEmotion` entirely into `globals.h` (before `eyes.h` and `theme_eyes.h` are processed).

## [CRITICAL] Compiler Error: Macro Assignment (`exoticMode`)
**File:** `network.h` (line 220)
**Status:** ✅ FIXED
**Description:** UDP parameter `EXOTIC:TOGGLE` was assigning `exoticMode = !exoticMode;` which triggered an "lvalue required" exception. `exoticMode` is no longer a boolean but a hardcoded backwards-compatible macro (`#define exoticMode (currentThemeId == 1)`).
**Fix Applied:** Rewrote the toggle block to invoke `playThemeTransition(...)` and physically swap between `currentThemeId` 0 (Legacy) and 1 (Exotic) without touching the macro.

## [CRITICAL] Logic Bomb: Touch Sensor Petting Trap (`isPetting`)
**File:** `touch.h` (line 45)
**Status:** ✅ FIXED
**Description:** Physical pin sensors `TOUCH_LEFT` and `TOUCH_RIGHT` trigger `isPetting = true` if held > 1.5 seconds. The original V4 check was `if ((currentLeft == HIGH || currentRight == HIGH) && (now - lastLeftTrigger > 1500 || now - lastRightTrigger > 1500))`. Due to poor operator evaluation, touching *one* pin evaluated the other pin's old, untouched trigger timestamp as `>1500`, securely trapping `isPetting = true` permanently. This caused floating hearts to spawn infinitely without the screen-clearing hook (`drawEyesPage()`) ever firing.
**Fix Applied:** Rewrote the check strictly mapping trigger states: `bool leftHeld = (currentLeft == HIGH && lastLeftState == HIGH && (now - lastLeftTrigger > 1500));` so that floating capacitive noise cannot lock the device state machine.

## [CRITICAL] Visual Bug: Blank Legacy Blinking
**File:** `eyes.h`
**Status:** ✅ FIXED
**Description:** In Legacy/Exotic themes, `isBlinking` was painting over the sprites with a black `COLOR_BG` rectangle to simulate an eyelid, but was missing a mid-frame `drawEyes()` call to regenerate the un-covered eye beneath it as the blink completed. This caused the legacy eyes to disappear entirely during blinks and not redraw until a subsequent motion event.
**Fix Applied:** Injected `drawEyes()` explicitly into the closing phase of the legacy `isBlinking` block to ensure pixel continuity under the eyelid rectangles.
