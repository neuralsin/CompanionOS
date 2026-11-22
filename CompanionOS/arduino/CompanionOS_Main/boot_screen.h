#ifndef COMPANION_BOOT_SCREEN_H
#define COMPANION_BOOT_SCREEN_H
#include "startup_policy.h"
inline void drawBootFrame(uint32_t elapsed, bool first) {
  const int width = SCREEN_W * 2 / 3, x = (SCREEN_W - width) / 2, y = SCREEN_H * 2 / 3;
  static int previous = 0;
  if (first) {
    previous = 0;
    tft.resetViewport(); tft.setTextSize(1); tft.setTextPadding(0); tft.setTextDatum(TL_DATUM);
    tft.fillScreen(CLR_BG);
    tft.setTextColor(CLR_TEXT_HI, CLR_BG);
    tft.drawCentreString("CompanionOS", SCREEN_W / 2, SCREEN_H / 3, SCREEN_W < 240 ? 2 : 4);
    tft.fillRect(x, y, width, 2, CLR_BORDER);
  }
  int filled = int(width * companion_startup::ease(elapsed / 900.0f));
  if (filled > previous) tft.fillRect(x + previous, y, filled - previous, 2, CLR_PRIMARY);
  previous = filled;
}
#endif
