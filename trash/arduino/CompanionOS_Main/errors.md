# V5 Implementation Error Audit

This document logs errors found during a deep static analysis and dry-run of the V5 multi-theme implementation.

## 1. Compilation & Header Order Errors
*   **Undefined `TR_` Macros in `theme_eyes.h`:** `TR_SUCCESS`, `TR_CYAN`, `TR_BASE`, etc., are defined in `theme_transition.h`, but used in `theme_eyes.h` (specifically in `drawSystemEyes()`). Because `eyes.h` includes `theme_eyes.h` *before* `theme_transition.h`, compiling this will immediately fail with "was not declared in this scope".
*   **Undefined `saveThemeToEEPROM`:** `theme_transition.h` calls `saveThemeToEEPROM()`, but this function is defined later in `ui.h`. It requires a forward declaration.

## 2. Rendering Pipeline & Frame Logic Errors
*   **Screen Freeze for V5 Themes:** In `updateEyes()`, the function `drawEyes()` is computationally expensive for V4, so it is only called when `moved == true` (the pupil springs shift). However, V5 themes use procedural internal animations (like `drawMoodEyes` fluid shifting based on `millis()`). Because they are gated by `moved == true`, V5 themes will remain statically frozen on screen instead of animating continuously at 20fps.
*   **Blinking Glitch (Black Boxes):** The global `isBlinking` logic in `updateEyes()` uses hardcoded V3/V4 almond eye coordinates and `COLOR_BG` (which is zero/black). If a V5 theme is active (e.g., Pikachu with a yellow background), the blink timer will periodically trigger and draw massive solid black rectangles over the yellow screen, causing severe visual corruption.

## 3. Discrepancies with JSON Constraints
*   **Missing Blink Mechanics:** The JSON blueprints request specific blink behaviors (e.g., `fast_and_irregular` for Pikachu, `lazy fade blinks` for Chill). The current `theme_eyes.h` renderers draw the eyes statically without acknowledging the `isBlinking` state or the `activeTheme.blinkIntervalMs` parameters.

## Proposed Fixes
1.  **Header Fix:** Move the `TR_` color definitions to `themes.h` so they are globally accessible to the entire theme system.
2.  **Forward Decl:** Add `extern void saveThemeToEEPROM();` to `theme_transition.h`.
3.  **Frame Loop Fix:** Update `eyes.h` inside `updateEyes()`: bypass the `moved` check for `currentThemeId >= 2` so `drawEyes()` is called continuously.
4.  **Blink Fix:** Rewrite the `isBlinking` block in `updateEyes()` to handle V5 dimensions and `activeTheme.bg`, or isolate V5 blinking into `drawEyes()`.
