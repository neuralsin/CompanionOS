// ═══════════════════════════════════════════════════════════
// COMPANION OS v7.0 — REUSABLE UI COMPONENTS
// Card, badge, pill, gradient, separator, progress components
// Resolution-aware via SCALE_X/SCALE_Y macros.
// ═══════════════════════════════════════════════════════════
#ifndef UI_COMPONENTS_H
#define UI_COMPONENTS_H

#include "globals.h"

// ── FW-06 FIX: Safe button wait with 30s timeout ────────
// Replaces bare `while (digitalRead(BTN_SELECT) == HIGH) delay(20);`
// which hangs forever if the button wire is disconnected.
#define DH_BTN_TIMEOUT_MS 30000
static inline void dhWaitSelectPress() {
  unsigned long _t = millis();
  while (digitalRead(BTN_SELECT) == BTN_UNPRESSED_LEVEL && !virtualSelectPressed && (millis() - _t < DH_BTN_TIMEOUT_MS)) {
    extern void handleNetwork(); handleNetwork();
    delay(20);
    yield();
  }
  if (virtualSelectPressed) virtualSelectPressed = false;
  delay(200); // debounce after press/timeout
}

// ═══════════════════════════════════════════════════════════
// COLOR UTILITIES
// ═══════════════════════════════════════════════════════════

// Blend two RGB565 colors by factor t (0.0 = c1, 1.0 = c2)
uint16_t blendColor(uint16_t c1, uint16_t c2, float t) {
  if (t <= 0.0f) return c1;
  if (t >= 1.0f) return c2;
  
  uint8_t r1 = (c1 >> 11) & 0x1F;
  uint8_t g1 = (c1 >> 5) & 0x3F;
  uint8_t b1 = c1 & 0x1F;
  
  uint8_t r2 = (c2 >> 11) & 0x1F;
  uint8_t g2 = (c2 >> 5) & 0x3F;
  uint8_t b2 = c2 & 0x1F;
  
  uint8_t r = r1 + (int)((r2 - r1) * t);
  uint8_t g = g1 + (int)((g2 - g1) * t);
  uint8_t b = b1 + (int)((b2 - b1) * t);
  
  return (r << 11) | (g << 5) | b;
}

// Lighten an RGB565 color by percentage (0-100)
uint16_t lightenColor(uint16_t color, int pct) {
  return blendColor(color, CLR_TEXT_HI, pct / 100.0f);
}

// Darken an RGB565 color by percentage (0-100)
uint16_t darkenColor(uint16_t color, int pct) {
  return blendColor(color, 0x0000, pct / 100.0f);
}

// ═══════════════════════════════════════════════════════════
// CARD COMPONENTS
// ═══════════════════════════════════════════════════════════

// Standard card with solid background, rounded corners, border
void drawCard(int x, int y, int w, int h, uint16_t bgColor, uint16_t borderColor, int radius) {
  // Fill with gradient (top lighter by ~10%)
  uint16_t topColor = lightenColor(bgColor, 10);
  tft.fillRoundRect(x, y, w, h, radius, bgColor);
  // Top highlight strip
  tft.fillRoundRect(x, y, w, h / 3, radius, topColor);
  tft.fillRect(x + radius, y + h / 3 - radius, w - 2 * radius, radius, topColor);
  // Border
  if (borderColor != bgColor) {
    tft.drawRoundRect(x, y, w, h, radius, borderColor);
  }
}

// Vertical gradient card using row-by-row color interpolation
void drawGradientCard(int x, int y, int w, int h, uint16_t topColor, uint16_t botColor, int radius) {
  // Draw row by row for gradient
  for (int row = 0; row < h; row++) {
    float t = (float)row / (float)(h - 1);
    uint16_t lineColor = blendColor(topColor, botColor, t);
    
    // Respect rounded corners
    if (row < radius || row >= h - radius) {
      // Inside corner zone — draw narrower
      int cornerRow = (row < radius) ? row : (h - 1 - row);
      // Approximate rounded corner with inset
      int inset = radius - (int)sqrt((float)(2 * radius * cornerRow - cornerRow * cornerRow));
      if (inset < 0) inset = 0;
      if (w - 2 * inset > 0) {
        tft.drawFastHLine(x + inset, y + row, w - 2 * inset, lineColor);
      }
    } else {
      tft.drawFastHLine(x, y + row, w, lineColor);
    }
  }
}

// ═══════════════════════════════════════════════════════════
// BADGE / PILL COMPONENTS
// ═══════════════════════════════════════════════════════════

// Compact pill-shaped label (auto-width, fixed 12px height)
void drawPillBadge(int x, int y, const char* text, uint16_t bgColor, uint16_t textColor) {
  int textW = strlen(text) * 6;  // approx width at font size 1
  int pillW = textW + SCALE_X(10);
  int pillH = SCALE_Y(12);
  int radius = pillH / 2;
  
  tft.fillRoundRect(x, y, pillW, pillH, radius, bgColor);
  tft.setTextColor(textColor);
  tft.drawString(text, x + SCALE_X(5), y + SCALE_Y(2), 1);
}

// Notification badge — red circle with count
void drawNotifBadge(int x, int y, int count) {
  if (count <= 0) return;
  int r = SCALE_X(6);
  tft.fillCircle(x, y, r, CLR_SECONDARY);
  tft.setTextColor(CLR_TEXT_HI);
  char buf[4];
  sprintf(buf, "%d", min(count, 9));
  tft.drawCentreString(buf, x, y - 3, 1);
}

// ═══════════════════════════════════════════════════════════
// ICON BUTTON COMPONENT
// ═══════════════════════════════════════════════════════════

// Square icon button with rounded corners and active highlight ring
// iconType: 0=play, 1=pause, 2=prev, 3=next, 4=shuffle, 5=repeat, 6=heart
void drawIconButton(int x, int y, int size, uint8_t iconType, uint16_t color, bool active) {
  int r = size / 4;
  
  if (active) {
    // Active highlight ring
    tft.drawRoundRect(x - 2, y - 2, size + 4, size + 4, r + 1, color);
    tft.fillRoundRect(x, y, size, size, r, darkenColor(color, 60));
  } else {
    tft.fillRoundRect(x, y, size, size, r, CLR_SURFACE);
    tft.drawRoundRect(x, y, size, size, r, CLR_BORDER);
  }
  
  int cx = x + size / 2;
  int cy = y + size / 2;
  int s = size / 4;  // icon scale
  
  // Draw icon based on type
  switch (iconType) {
    case 0: // Play triangle
      tft.fillTriangle(cx - s, cy - s, cx - s, cy + s, cx + s, cy, color);
      break;
    case 1: // Pause bars
      tft.fillRect(cx - s, cy - s, s - 1, s * 2, color);
      tft.fillRect(cx + 1, cy - s, s - 1, s * 2, color);
      break;
    case 2: // Prev
      tft.fillRect(cx - s, cy - s, 2, s * 2, color);
      tft.fillTriangle(cx + s, cy - s, cx + s, cy + s, cx - s + 3, cy, color);
      break;
    case 3: // Next
      tft.fillTriangle(cx - s, cy - s, cx - s, cy + s, cx + s - 3, cy, color);
      tft.fillRect(cx + s - 2, cy - s, 2, s * 2, color);
      break;
  }
}

// ═══════════════════════════════════════════════════════════
// SEPARATORS & DECORATIVE ELEMENTS
// ═══════════════════════════════════════════════════════════

// 1px horizontal line with fade-out ends
void drawSeparator(int x, int y, int w, uint16_t color) {
  int fadeZone = min(w / 6, SCALE_X(20));
  
  // Left fade
  for (int i = 0; i < fadeZone; i++) {
    float t = (float)i / fadeZone;
    tft.drawPixel(x + i, y, blendColor(CLR_BG, color, t));
  }
  // Middle solid
  tft.drawFastHLine(x + fadeZone, y, w - 2 * fadeZone, color);
  // Right fade
  for (int i = 0; i < fadeZone; i++) {
    float t = 1.0f - (float)i / fadeZone;
    tft.drawPixel(x + w - fadeZone + i, y, blendColor(CLR_BG, color, t));
  }
}

// ═══════════════════════════════════════════════════════════
// PROGRESS BARS
// ═══════════════════════════════════════════════════════════

// Rounded progress bar with glow effect
void drawProgressPill(int x, int y, int w, int h, float percent, uint16_t fgColor, uint16_t bgColor) {
  if (percent < 0.0f) percent = 0.0f;
  if (percent > 1.0f) percent = 1.0f;
  
  int r = h / 2;
  
  // Background track
  tft.fillRoundRect(x, y, w, h, r, bgColor);
  
  // Fill portion
  int fillW = (int)(w * percent);
  if (fillW < h) fillW = (percent > 0.01f) ? h : 0;  // minimum visible width
  
  if (fillW > 0) {
    tft.fillRoundRect(x, y, fillW, h, r, fgColor);
    
    // Glow highlight on fill (top edge)
    uint16_t glowColor = lightenColor(fgColor, 30);
    tft.drawFastHLine(x + r, y + 1, max(1, fillW - 2 * r), glowColor);
  }
}

// Thin progress line (2px) for slideshow etc.
void drawProgressLine(int x, int y, int w, float percent, uint16_t color) {
  if (percent < 0.0f) percent = 0.0f;
  if (percent > 1.0f) percent = 1.0f;
  
  tft.drawFastHLine(x, y, w, CLR_BORDER);
  tft.drawFastHLine(x, y + 1, w, CLR_BORDER);
  
  int fillW = (int)(w * percent);
  if (fillW > 0) {
    tft.drawFastHLine(x, y, fillW, color);
    tft.drawFastHLine(x, y + 1, fillW, color);
  }
}

// ═══════════════════════════════════════════════════════════
// TEXT HELPERS
// ═══════════════════════════════════════════════════════════

// Draw text that scrolls horizontally if wider than maxW (marquee)
// Returns true if scrolling is active
bool drawMarqueeText(int x, int y, const char* text, int maxW, uint16_t color, int font, unsigned long* scrollOffset) {
  int textW = tft.textWidth(text, font);
  
  if (textW <= maxW) {
    // Fits — draw normally
    tft.setTextColor(color, CLR_BG);
    tft.drawString(text, x, y, font);
    return false;
  }
  
  // Scrolling needed
  int scrollMax = textW - maxW + 20;  // extra pause at ends
  int scrollPos = (*scrollOffset / 3) % (scrollMax * 2);
  if (scrollPos > scrollMax) scrollPos = scrollMax * 2 - scrollPos;  // bounce
  
  // Clip region (manual — TFT_eSPI doesn't have clip)
  tft.fillRect(x, y, maxW, tft.fontHeight(font), CLR_BG);
  tft.setTextColor(color, CLR_BG);
  tft.drawString(text, x - scrollPos, y, font);
  
  (*scrollOffset)++;
  return true;
}

// Truncate text to fit within maxW pixels, adding "..." if needed
void drawTruncatedText(int x, int y, const char* text, int maxW, uint16_t color, int font) {
  int textW = tft.textWidth(text, font);
  tft.setTextColor(color, CLR_BG);
  
  if (textW <= maxW) {
    tft.drawString(text, x, y, font);
    return;
  }
  
  // Find how many chars fit
  char buf[64];
  int len = strlen(text);
  if (len > 60) len = 60;
  
  for (int i = len; i > 0; i--) {
    strncpy(buf, text, i);
    buf[i] = '\0';
    strcat(buf, "...");
    if (tft.textWidth(buf, font) <= maxW) {
      tft.drawString(buf, x, y, font);
      return;
    }
  }
  tft.drawString("...", x, y, font);
}

#endif
