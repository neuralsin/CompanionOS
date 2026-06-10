// ═══════════════════════════════════════════════════════════
// COMPANION OS v7.0 — UI COMPONENTS (Status Bar, Overlays)
// Resolution-aware, dual-transport status indicators.
// ═══════════════════════════════════════════════════════════
#ifndef UI_H
#define UI_H

#include "globals.h"
#include "ui_components.h"

extern int displayHour;
extern int displayMinute;
extern bool timeReceived;

// ═══════════════════════════════════════════════════════════
// V7 STATUS BAR — Redesigned: clock, WiFi+BT, music, notif
// ═══════════════════════════════════════════════════════════

void drawStatusBar() {
  int barH = SCALE_Y(15);
  tft.fillRect(0, 0, SCR_W, barH, CLR_BG);

  // ── Compute layout zones to prevent overlap ──
  // Right zone: time string (always rightmost), then indicator icons to its left.
  // Left zone: song name (eyes page) or clock (other pages).

  // Time string width: "HH:MM" in font 1 ≈ 30px on 160px screen
  int timeW = 30; // approximate width of "HH:MM" in font 1

  if (currentState == STATE_EYES) {
    // ── EYES PAGE LAYOUT ──
    // Far right: Clock
    // Right of center: indicator icons
    // Left: Song info (truncated to avoid overlap)

    // 1. Clock on far RIGHT
    if (timeReceived) {
      char timeStr[6];
      sprintf(timeStr, "%02d:%02d", displayHour, displayMinute);
      tft.setTextColor(CLR_TEXT_HI, CLR_BG);
      tft.drawRightString(timeStr, SCR_W - SCALE_X(2), SCALE_Y(2), 1);
    } else {
      tft.setTextColor(CLR_TEXT_LO, CLR_BG);
      tft.drawRightString("--:--", SCR_W - SCALE_X(2), SCALE_Y(2), 1);
    }

    // 2. Indicator icons in a zone just left of the clock
    int iconZoneRight = SCR_W - timeW - SCALE_X(4); // leave gap before clock
    int iconX = iconZoneRight;
    int iconY = SCALE_Y(5);

    // WiFi icon
    if (wifiConnected) {
      tft.fillCircle(iconX, iconY + SCALE_Y(4), 1, CLR_PRIMARY);
      tft.drawFastHLine(iconX - SCALE_X(2), iconY + SCALE_Y(2), SCALE_X(5), CLR_PRIMARY);
      tft.drawFastHLine(iconX - SCALE_X(3), iconY, SCALE_X(7), CLR_PRIMARY);
    } else {
      tft.fillCircle(iconX, iconY + SCALE_Y(4), 1, CLR_SECONDARY);
      tft.drawLine(iconX - 2, iconY, iconX + 2, iconY + 4, CLR_SECONDARY);
    }
    iconX -= SCALE_X(12);

    // BT icon (ESP32 only)
    #ifdef ESP32
    if (btConnected) {
      tft.drawFastVLine(iconX, iconY - SCALE_Y(4), SCALE_Y(8), 0x001F);
      tft.drawLine(iconX - 2, iconY - 2, iconX + 2, iconY + 2, 0x001F);
      tft.drawLine(iconX - 2, iconY + 2, iconX + 2, iconY - 2, 0x001F);
      iconX -= SCALE_X(10);
    }
    #endif

    // Music note icon
    if (musicPlaying) {
      tft.drawFastVLine(iconX, iconY - SCALE_Y(3), SCALE_Y(6), CLR_SUCCESS);
      tft.drawFastVLine(iconX + SCALE_X(2), iconY - SCALE_Y(4), SCALE_Y(7), CLR_SUCCESS);
      tft.fillCircle(iconX, iconY + SCALE_Y(3), 1, CLR_SUCCESS);
      tft.fillCircle(iconX + SCALE_X(2), iconY + SCALE_Y(3), 1, CLR_SUCCESS);
      iconX -= SCALE_X(10);
    }

    // Notification dot
    if (notifCount > 0) {
      tft.fillCircle(iconX, iconY, SCALE_X(3), TFT_MAGENTA);
      iconX -= SCALE_X(8);
    }

    // 3. Song name on LEFT — truncate before icons
    if (musicPlaying) {
      extern String currentTrack;
      extern String currentArtist;
      int maxSongW = iconX - SCALE_X(4); // all space left of icons
      String songInfo = currentArtist + " - " + currentTrack;
      tft.setTextColor(CLR_TEXT_LO, CLR_BG);
      drawTruncatedText(SCALE_X(2), SCALE_Y(2), songInfo.c_str(), maxSongW, CLR_TEXT_LO, 1);
    }

  } else {
    // ── NON-EYES PAGE LAYOUT ──
    // Left: clock, Center: song snippet, Right: indicator icons

    // Clock on left
    if (timeReceived) {
      char timeStr[6];
      sprintf(timeStr, "%02d:%02d", displayHour, displayMinute);
      tft.setTextColor(CLR_TEXT_HI, CLR_BG);
      tft.drawString(timeStr, SCALE_X(4), SCALE_Y(2), 1);
    } else {
      tft.setTextColor(CLR_TEXT_LO, CLR_BG);
      tft.drawString("--:--", SCALE_X(4), SCALE_Y(2), 1);
    }

    // Right-side indicator icons
    int iconX = SCR_W - SCALE_X(5);
    int iconY = SCALE_Y(5);

    // WiFi icon (rightmost)
    if (wifiConnected) {
      tft.fillCircle(iconX, iconY + SCALE_Y(4), 1, CLR_PRIMARY);
      tft.drawFastHLine(iconX - SCALE_X(2), iconY + SCALE_Y(2), SCALE_X(5), CLR_PRIMARY);
      tft.drawFastHLine(iconX - SCALE_X(3), iconY, SCALE_X(7), CLR_PRIMARY);
      tft.drawFastHLine(iconX - SCALE_X(4), iconY - SCALE_Y(2), SCALE_X(9), CLR_PRIMARY);
    } else {
      tft.fillCircle(iconX, iconY + SCALE_Y(4), 1, CLR_SECONDARY);
      tft.drawLine(iconX - 2, iconY, iconX + 2, iconY + 4, CLR_SECONDARY);
      tft.drawLine(iconX + 2, iconY, iconX - 2, iconY + 4, CLR_SECONDARY);
    }

    // Song name in center on non-eyes pages
    if (musicPlaying) {
      extern String currentTrack;
      String songSnippet = currentTrack;
      tft.setTextColor(CLR_TEXT_LO, CLR_BG);
      int songX = 36; // Clock is ~30px, so start song at 36px
      int maxSongW = iconX - SCALE_X(12) - songX;
      drawTruncatedText(songX, SCALE_Y(2), songSnippet.c_str(), maxSongW, CLR_TEXT_LO, 1);
    }
  }
}


// ═══════════════════════════════════════════════════════════
// V4/V7: Loading Screen for Mode Switching
// ═══════════════════════════════════════════════════════════

void showLoadingScreen(const char* message) {
  tft.fillScreen(CLR_BG);
  tft.setTextColor(CLR_PRIMARY);
  tft.drawCentreString(message, SCR_CX, SCR_CY - SCALE_Y(20), 1);

  // Progress bar animation
  int barX = SCR_CX - SCALE_X(60);
  int barY = SCR_CY + SCALE_Y(10);
  int barW = SCALE_X(120);
  int barH = SCALE_Y(8);

  tft.drawRect(barX, barY, barW, barH, CLR_BORDER);
  for (int i = 0; i < barW - 4; i += SCALE_X(4)) {
    tft.fillRect(barX + 2 + i, barY + 2, SCALE_X(4), barH - 4, CLR_PRIMARY);
    delay(15);
  }
  delay(200);
}

// ═══════════════════════════════════════════════════════════
// PAGE INDICATOR DOTS — Bottom center
// ═══════════════════════════════════════════════════════════

int getEffectivePageCount() {
  #ifdef ESP32
    return STATE_COUNT;
  #else
    return STATE_DR_HACK;
  #endif
}

void drawPageIndicator(int current, int total) {
  int effectiveTotal = getEffectivePageCount();
  if (total <= 0 || total > effectiveTotal) total = effectiveTotal;
  if (current >= total) current = total - 1;

  int spacing = SCALE_X(8);
  int startX = (SCR_W - ((total - 1) * spacing)) / 2;
  int y = SCR_H - SCALE_Y(6);

  for (int i = 0; i < total; i++) {
    int dotX = startX + (i * spacing);
    if (i == current) {
      tft.fillCircle(dotX, y, SCALE_X(2), CLR_TEXT_HI);
    } else {
      tft.fillCircle(dotX, y, SCALE_X(1), CLR_BORDER);
    }
  }
}

// ═══════════════════════════════════════════════════════════
// PAGE HEADER — Thin, minimal
// ═══════════════════════════════════════════════════════════

void drawPageHeader(const char* title) {
  tft.fillRect(0, 0, SCR_W - SCALE_X(40), SCALE_Y(16), CLR_BG);
  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString(title, SCALE_X(4), SCALE_Y(4), 1);
}

// ═══════════════════════════════════════════════════════════
// BUTTON HELPER (legacy, used by touch pages)
// ═══════════════════════════════════════════════════════════

void drawButton(int x, int y, int w, int h, const char* label, uint16_t color) {
  tft.fillRoundRect(x, y, w, h, 5, color);
  tft.setTextColor(CLR_TEXT_HI);
  tft.drawCentreString(label, x + (w/2), y + (h/2) - 8, 2);
}

// ═══════════════════════════════════════════════════════════
// AGENT STATUS OVERLAY
// ═══════════════════════════════════════════════════════════

void drawAgentOverlay() {
  if (!agentOverlayActive) return;

  uint16_t bgColor = CLR_SURFACE;
  uint16_t accentColor = CLR_PRIMARY;
  if (agentStatus == "error") {
    accentColor = CLR_SECONDARY;
  } else if (agentStatus == "done") {
    accentColor = CLR_SUCCESS;
  }

  int oy = SCR_H - SCALE_Y(42);
  int oh = SCALE_Y(34);
  int ox = SCALE_X(4);
  int ow = SCR_W - SCALE_X(8);

  tft.fillRoundRect(ox, oy, ow, oh, 4, bgColor);
  tft.fillRect(ox, oy, SCALE_X(3), oh, accentColor);

  if (agentStatus == "thinking") {
    float pulse = (sin(millis() * 0.005) + 1.0) * 0.5;
    uint16_t dotColor = blendColor(CLR_BORDER, accentColor, pulse);
    tft.fillCircle(ox + SCALE_X(10), oy + oh / 2, SCALE_X(3), dotColor);
  } else {
    tft.fillCircle(ox + SCALE_X(10), oy + oh / 2, SCALE_X(3), accentColor);
  }

  tft.setTextColor(CLR_TEXT_HI);
  int textX = ox + SCALE_X(18);
  int maxTextW = ow - SCALE_X(22);
  String displayText = agentStatusText.substring(0, (SCR_W < 200) ? 24 : 32);
  tft.drawString(displayText, textX, oy + SCALE_Y(4), 1);
  if (agentStatusText.length() > (unsigned int)((SCR_W < 200) ? 24 : 32)) {
    tft.drawString(agentStatusText.substring((SCR_W < 200) ? 24 : 32, (SCR_W < 200) ? 48 : 64), textX, oy + SCALE_Y(16), 1);
  }

  if (agentStatus == "done" && millis() - agentStatusStart > 5000) {
    agentOverlayActive = false;
  }
  if (agentStatus == "error" && millis() - agentStatusStart > 8000) {
    agentOverlayActive = false;
  }
}

// ═══════════════════════════════════════════════════════════
// FLASH NOTIFICATION — Full-screen flash for important notifs
// ═══════════════════════════════════════════════════════════

extern bool flashNotifActive;
extern unsigned long flashNotifStart;
extern String flashNotifText;
extern bool flashNotifEnabled;

void showFlashNotification(String text) {
  if (!flashNotifEnabled) return;
  flashNotifActive = true;
  flashNotifStart = millis();
  flashNotifText = text;
}

void drawFlashNotification() {
  if (!flashNotifActive) return;

  unsigned long elapsed = millis() - flashNotifStart;
  if (elapsed > 3000) {
    flashNotifActive = false;
    return;
  }

  // Pulsing notification banner at bottom
  float alpha = 1.0f;
  if (elapsed < 300) alpha = elapsed / 300.0f;
  else if (elapsed > 2700) alpha = (3000 - elapsed) / 300.0f;

  int bannerH = SCALE_Y(30);
  int bannerY = SCR_H - bannerH;

  uint16_t bgColor = blendColor(CLR_BG, 0x600F, alpha * 0.8f);
  uint16_t textColor = blendColor(CLR_BG, CLR_TEXT_HI, alpha);

  tft.fillRect(0, bannerY, SCR_W, bannerH, bgColor);
  tft.drawFastHLine(0, bannerY, SCR_W, TFT_MAGENTA);

  tft.setTextColor(textColor);
  drawTruncatedText(SCALE_X(8), bannerY + SCALE_Y(6), flashNotifText.c_str(), 
                    SCR_W - SCALE_X(16), textColor, 1);

  tft.setTextColor(blendColor(CLR_BG, CLR_TEXT_LO, alpha));
  tft.drawString("New notification", SCALE_X(8), bannerY + SCALE_Y(18), 1);
}

#endif
