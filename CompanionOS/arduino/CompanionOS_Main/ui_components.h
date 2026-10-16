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
  #ifdef ESP32
  while (digitalRead(BTN_SELECT) == BTN_UNPRESSED_LEVEL && !virtualSelectPressed && (millis() - _t < DH_BTN_TIMEOUT_MS)) {
    extern void handleNetwork(); handleNetwork();
    delay(20);
    yield();
  }
  #else
  while (!virtualSelectPressed && (millis() - _t < DH_BTN_TIMEOUT_MS)) {
    extern void handleNetwork(); handleNetwork();
    delay(20);
    yield();
  }
  #endif
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
  if (w <= 0 || h <= 0) return;
  radius = constrain(radius, 0, min(w, h) / 2);
  // Draw row by row for gradient
  for (int row = 0; row < h; row++) {
    float t = h > 1 ? (float)row / (float)(h - 1) : 0.0f;
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
  int textW = tft.textWidth(text ? text : "", 1);
  int pillW = textW + SCALE_X(10);
  int pillH = max(12, tft.fontHeight(1) + 4);
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
// Built-in fonts cannot render arbitrary Unicode. Consume complete UTF-8
// codepoints so a truncated multibyte title never leaves broken glyphs.
String uiText(const char* text) {
  String out;
  if (!text) return out;
  out.reserve(min((int)strlen(text), 512));
  const uint8_t* p = (const uint8_t*)text;
  while (*p && out.length() < 512) {
    uint32_t cp = *p++;
    if (cp >= 0xC2 && cp <= 0xF4) {
      int extra = cp < 0xE0 ? 1 : cp < 0xF0 ? 2 : 3;
      cp &= (1 << (6 - extra)) - 1;
      bool valid = true;
      for (int i = 0; i < extra; i++) {
        if ((*p & 0xC0) != 0x80) { valid = false; break; }
        cp = (cp << 6) | (*p++ & 0x3F);
      }
      if (!valid) cp = '?';
    }
    if (cp == '\n') out += '\n';
    else if (cp < 32 || cp == 0xA0) out += ' ';
    else if (cp < 127) out += (char)cp;
    else if (cp >= 0x2010 && cp <= 0x2015) out += '-';
    else if (cp == 0x2018 || cp == 0x2019) out += '\'';
    else if (cp == 0x201C || cp == 0x201D) out += '"';
    else if (cp == 0x2026) out += "...";
    else if (cp >= 0xC0 && cp <= 0xC5) out += 'A';
    else if (cp >= 0xE0 && cp <= 0xE5) out += 'a';
    else if (cp >= 0xC8 && cp <= 0xCB) out += 'E';
    else if (cp >= 0xE8 && cp <= 0xEB) out += 'e';
    else if (cp == 0xE7 || cp == 0xC7) out += cp == 0xE7 ? 'c' : 'C';
    else if (cp == 0x2022 || cp == 0x266A || cp == 0x266B) out += ' ';
    else out += '?';
  }
  return out;
}

String fitUiText(const char* text, int maxW, int font = 1) {
  String value = uiText(text);
  value.replace('\n', ' ');
  if (maxW <= 0) return "";
  if (tft.textWidth(value, font) <= maxW) return value;
  String suffix = "...";
  while (suffix.length() && tft.textWidth(suffix, font) > maxW)
    suffix.remove(suffix.length() - 1);
  while (value.length() && tft.textWidth(value + suffix, font) > maxW)
    value.remove(value.length() - 1);
  return value + suffix;
}

bool drawMarqueeText(int x, int y, const char* text, int maxW, uint16_t color, int font, unsigned long* scrollOffset, uint16_t bg = CLR_BG) {
  maxW = min(maxW, SCR_W - x);
  if (maxW <= 0 || !scrollOffset) return false;
  String value = uiText(text);
  value.replace('\n', ' ');
  int textW = tft.textWidth(value, font);
  int distance = max(0, textW - maxW);
  // Time-based, with a pause at both ends; independent of packet/frame rate.
  unsigned long phase = (millis() - *scrollOffset) % (2400UL + distance * 70UL);
  int offset = 0;
  if (distance && phase > 1200) {
    unsigned long travel = distance * 35UL;
    if (phase < 1200 + travel) offset = (phase - 1200) / 35;
    else if (phase < 2400 + travel) offset = distance;
    else offset = distance - (phase - 2400 - travel) / 35;
  }
  const int height = tft.fontHeight(font);
  if (height <= 0) return false;
  static TFT_eSprite strip(&tft);
  static int stripW = 0, stripH = 0;
  static bool allocated = false;
  if (stripW != maxW || stripH != height) {
    strip.deleteSprite();
    strip.setColorDepth(16);
    allocated = strip.createSprite(maxW, height) != nullptr;
    stripW = maxW;
    stripH = height;
  }
  // A sprite clips the moving text without changing the caller's viewport.
  // TFT_eSPI's false-datum viewport getters cannot round-trip its clip bounds.
  if (allocated) {
    strip.fillSprite(bg);
    strip.setTextDatum(TL_DATUM);
    strip.setTextSize(1);
    strip.setTextSize(max(1, height / max(1, (int)strip.fontHeight(font))));
    strip.setTextColor(color, bg);
    strip.drawString(value, -offset, 0, font);
    strip.pushSprite(x, y);
  } else {
    tft.fillRect(x, y, maxW, height, bg);
    tft.setTextColor(color, bg);
    tft.drawString(fitUiText(value.c_str(), maxW, font), x, y, font);
  }
  return distance > 0;
}

// Truncate text to fit within maxW pixels, adding "..." if needed
void drawTruncatedText(int x, int y, const char* text, int maxW, uint16_t color, int font, uint16_t bg = CLR_BG) {
  maxW = min(maxW, SCR_W - x);
  if (maxW <= 0) return;
  tft.setTextColor(color, bg);
  tft.drawString(fitUiText(text, maxW, font), x, y, font);
}

int drawWrappedText(int x, int y, const char* text, int maxW, int maxH, uint16_t color, int font = 1, uint16_t bg = CLR_BG) {
  if (maxW <= 0 || maxH <= 0) return 0;
  String value = uiText(text);
  int lineH = tft.fontHeight(font) + 2;
  int rows = maxH / lineH;
  int start = 0, drawn = 0;
  while (start < (int)value.length() && drawn < rows) {
    while (value[start] == ' ') start++;
    int end = start, space = -1;
    while (end < (int)value.length() && value[end] != '\n' &&
           tft.textWidth(value.substring(start, end + 1), font) <= maxW) {
      if (value[end] == ' ') space = end;
      end++;
    }
    if (end == start && value[end] != '\n') break;
    if (end < (int)value.length() && value[end] != '\n' && space > start) end = space;
    String line = value.substring(start, end);
    if (drawn == rows - 1 && end < (int)value.length()) line = value.substring(start);
    drawTruncatedText(x, y + drawn * lineH, line.c_str(), maxW, color, font, bg);
    drawn++;
    start = end;
    if (value[start] == '\n' || value[start] == ' ') start++;
  }
  return drawn;
}

#endif
