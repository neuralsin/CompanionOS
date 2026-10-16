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
extern bool flashNotifActive;

// ═══════════════════════════════════════════════════════════
// V7 STATUS BAR — Redesigned: clock, WiFi+BT, music, notif
// ═══════════════════════════════════════════════════════════

void drawStatusBar() {
  if (currentState == STATE_DR_HACK || currentState == STATE_RUVIEW ||
      currentState == STATE_PC_DASH || (customEyeActive && customEyeReady)) return;
  const char* titles[] = {"CompanionOS", "Spotify", "Focus", "Weather",
    "Notifications", "Notes", "Stocks", "Gaming", "Social", "Schedule", "Network", "System",
    "PC dashboard", "RuView", "Clock", "Watch", "Dr.Hack"};
  String title = (int)currentState < STATE_COUNT ? titles[(int)currentState] : "CompanionOS";
  const bool nowPlaying = currentState == STATE_EYES && musicPlaying && currentTrack.length();
  const bool night = timeReceived && (displayHour >= 20 || displayHour < 6);
  if (currentState == STATE_EYES) {
    if (nowPlaying) {
      title = currentArtist.length() && (millis() / 6000) % 2 ? currentArtist : currentTrack;
    } else if (!timeReceived) title = "Hello there";
    else if (displayHour < 6 || displayHour >= 22) title = "Quiet hours";
    else if (displayHour < 12) title = "Good morning";
    else if (displayHour < 18) title = "Good afternoon";
    else title = "Good evening";
  }
  char clock[12];
  snprintf(clock, sizeof(clock), "%02d:%02d", constrain(displayHour, 0, 23), constrain(displayMinute, 0, 59));
  if (!timeReceived) strcpy(clock, "--:--");
  String key = title + clock + String(wifiConnected) + String(musicPlaying) +
               String(notifCount) + String(uiPageGeneration) + String(night);
  const int progressWidth = nowPlaying && playDuration > 0 ?
      (int)((int64_t)constrain(playProgress, 0, playDuration) * (SCR_W - 8) / playDuration) : 0;
  key += String(progressWidth);
  static String previous;
  if (key == previous) return;
  previous = key;
  static TFT_eSprite canvas(&tft);
  static bool tried = false, buffered = false;
  if (!tried) {
    tried = true;
    canvas.setColorDepth(16);
    buffered = canvas.createSprite(SCR_W, UI_HEADER_H) != nullptr;
  }
  auto paint = [&](auto& target) {
    target.fillRect(0, 0, SCR_W, UI_HEADER_H, CLR_BG);
    target.setTextSize(1);
    int titleX = 4;
    if (currentState == STATE_EYES) {
      titleX = 17;
      if (nowPlaying) {
        target.drawFastVLine(8, 3, 7, CLR_SUCCESS);
        target.drawFastHLine(8, 3, 5, CLR_SUCCESS);
        target.fillCircle(6, 10, 2, CLR_SUCCESS);
      } else if (night) {
        target.fillCircle(8, 7, 4, CLR_PRIMARY);
        target.fillCircle(10, 5, 3, CLR_BG);
      } else {
        target.drawCircle(8, 7, 3, CLR_WARNING);
        target.drawFastVLine(8, 1, 2, CLR_WARNING);
        target.drawFastVLine(8, 12, 2, CLR_WARNING);
        target.drawFastHLine(2, 7, 2, CLR_WARNING);
        target.drawFastHLine(13, 7, 2, CLR_WARNING);
      }
    }
    target.setTextColor(nowPlaying ? CLR_TEXT_HI : CLR_TEXT_MED, CLR_BG);
    target.drawString(fitUiText(title.c_str(), SCR_W - 47 - titleX, 1), titleX, 4, 1);
    target.setTextColor(CLR_TEXT_HI, CLR_BG);
    target.drawRightString(clock, SCR_W - 13, 4, 1);
    target.fillCircle(SCR_W - 6, 7, 2, wifiConnected ? CLR_SUCCESS : CLR_TEXT_LO);
    target.drawFastHLine(4, UI_HEADER_H - 1, SCR_W - 8, CLR_BORDER);
    if (progressWidth > 0) target.drawFastHLine(4, UI_HEADER_H - 1, progressWidth, CLR_SUCCESS);
  };
  if (buffered) { paint(canvas); canvas.pushSprite(0, 0); }
  else paint(tft);
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
  // The shared bar owns its title and clock so refreshes cannot erase titles.
  (void)title;
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
  unsigned long elapsed = millis() - agentStatusStart;
  unsigned long duration = agentStatus == "done" ? 5000UL : agentStatus == "error" ? 8000UL : 15000UL;
  if (elapsed >= duration) {
    agentOverlayActive = false;
    return;
  }
  if (flashNotifActive) return;
  uint16_t accent = agentStatus == "error" ? CLR_DANGER :
                    agentStatus == "done" ? CLR_SUCCESS : CLR_PRIMARY;
  int x = 4, h = 28, y = SCR_H - h - UI_FOOTER_H, w = SCR_W - 8;
  tft.fillRoundRect(x, y, w, h, 4, CLR_SURFACE);
  tft.fillRect(x, y + 4, 2, h - 8, accent);
  int dot = 2 + (agentStatus == "thinking" ? (millis() / 400) % 3 : 0);
  tft.fillCircle(x + 8, y + 8, dot, accent);
  drawWrappedText(x + 16, y + 4, agentStatusText.c_str(), w - 21, 22, CLR_TEXT_HI, 1, CLR_SURFACE);
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
  if (!flashNotifActive || currentState != STATE_EYES) return;
  unsigned long elapsed = millis() - flashNotifStart;
  if (elapsed >= 3000) return;
  const int h = 28;
  int slide = elapsed < 180 ? (int)((180 - elapsed) * h / 180) : 0;
  int y = SCR_H - h + slide;
  tft.fillRect(0, y, SCR_W, h - slide, CLR_SURFACE_2);
  tft.drawFastHLine(0, y, SCR_W, CLR_PRIMARY);
  drawWrappedText(6, y + 4, flashNotifText.c_str(), SCR_W - 12, max(0, h - slide - 4), CLR_TEXT_HI, 1, CLR_SURFACE_2);
}

#endif
