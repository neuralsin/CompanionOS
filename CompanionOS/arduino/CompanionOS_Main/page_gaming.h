#ifndef PAGE_GAMING_H
#define PAGE_GAMING_H
#include "globals.h"
#include "ui_components.h"

extern void drawPageIndicator(int current, int total);

#define GAM_BG       0x0841
#define GAM_CARD     0x1082
#define GAM_BORDER   0x18E3
#define GAM_CYAN     0x07DF
#define GAM_GREEN    0x2E8B
#define GAM_DIM      0x4A49
#define GAM_BAR_BG   0x2104
#define GAM_PURPLE   0x79FD

static bool gamingPageDrawn = false;
static String gamLastContent = "";

void resetGamingDrawState() {
  gamingPageDrawn = false;
  gamLastContent = "";
}

static String gam_contentKey() {
  String key = String(uiPageGeneration) + "\n" + gameTitle + "\n" + gameStatus + "\n" + sessionTime + "/" +
               String(gameActive) + "/" + String(friendsOnline) + "/" + String(achievePct);
  for (int i = 0; i < 3; i++) key += "\n" + String(recentGame[i]) + "/" + String(recentPlaytime[i]);
  return key;
}

void drawAchievementBar(int x, int y, int w, int h, uint8_t pct) {
  tft.fillRect(x, y, w, h, GAM_BAR_BG);
  int fillW = (long)min(100, (int)pct) * w / 100;
  if (fillW > 0) tft.fillRect(x, y, fillW, h, GAM_PURPLE);
}

void drawGamingPage() {
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  tft.fillRect(0, UI_HEADER_H, SCREEN_W, SCREEN_H - UI_HEADER_H, GAM_BG);
  const int pad = 4;
  const int width = SCREEN_W - 2 * pad;
  const int lineH = tft.fontHeight(1) + 2;
  const int titleFont = SCREEN_W > 200 ? 2 : 1;
  int y = UI_HEADER_H + 12;

  drawTruncatedText(pad, UI_HEADER_H + 2, "GAMING", width, GAM_CYAN, 1, GAM_BG);
  drawTruncatedText(pad, y, gameTitle[0] ? gameTitle : "No game reported",
                    width, TFT_WHITE, titleFont, GAM_BG);
  y += tft.fontHeight(titleFont) + 2;
  const int stateW = tft.textWidth(gameActive ? "PLAYING" : "IDLE", 1);
  drawTruncatedText(pad, y, gameTitle[0] ? gameStatus : "Waiting for game data",
                    width - stateW - 8, GAM_DIM, 1, GAM_BG);
  drawTruncatedText(SCREEN_W - pad - stateW, y, gameActive ? "PLAYING" : "IDLE",
                    stateW, gameActive ? GAM_GREEN : GAM_DIM, 1, GAM_BG);
  y += lineH;

  const int friendX = SCREEN_W / 2 - 4;
  const int achieveX = SCREEN_W - 30;
  const int sessionW = friendX - pad - 6;
  drawTruncatedText(pad, y, "SESSION", sessionW, GAM_DIM, 1, GAM_BG);
  drawTruncatedText(friendX, y, "FRIENDS", achieveX - friendX - 4, GAM_DIM, 1, GAM_BG);
  drawTruncatedText(achieveX, y, "ACH", 26, GAM_DIM, 1, GAM_BG);
  y += lineH;
  drawTruncatedText(pad, y, gameTitle[0] && sessionTime[0] ? sessionTime : "--",
                    sessionW, GAM_CYAN, 1, GAM_BG);
  char value[12];
  snprintf(value, sizeof(value), "%u", (unsigned)friendsOnline);
  drawTruncatedText(friendX, y, value, achieveX - friendX - 4, GAM_GREEN, 1, GAM_BG);
  if (gameTitle[0]) snprintf(value, sizeof(value), "%u%%", (unsigned)min(100, (int)achievePct));
  else snprintf(value, sizeof(value), "--");
  drawTruncatedText(achieveX, y, value, 26, TFT_WHITE, 1, GAM_BG);
  y += lineH;
  drawAchievementBar(pad, y, width, 4, gameTitle[0] ? achievePct : 0);
  y += 10;

  drawTruncatedText(pad, y, "RECENT", width, GAM_DIM, 1, GAM_BG);
  y += lineH;
  bool anyRecent = false;
  for (int i = 0; i < 3 && y + tft.fontHeight(1) <= SCREEN_H - UI_FOOTER_H; i++) {
    if (!recentGame[i][0]) continue;
    anyRecent = true;
    char playtime[12];
    if (recentPlaytime[i] >= 60) snprintf(playtime, sizeof(playtime), "%uh", (unsigned)(recentPlaytime[i] / 60));
    else snprintf(playtime, sizeof(playtime), "%um", (unsigned)recentPlaytime[i]);
    int timeW = tft.textWidth(playtime, 1);
    drawTruncatedText(pad, y, recentGame[i], width - timeW - 8, GAM_CYAN, 1, GAM_BG);
    tft.setTextColor(GAM_DIM, GAM_BG);
    tft.drawRightString(playtime, SCREEN_W - pad, y, 1);
    y += lineH;
  }
  if (!anyRecent) drawTruncatedText(pad, y, "No recent games", width, GAM_DIM, 1, GAM_BG);
  drawPageIndicator(STATE_GAMING, STATE_COUNT);
  gamingPageDrawn = true;
  gamLastContent = gam_contentKey();
}

void redrawGamingPartial() {
  if (currentState != STATE_GAMING) return;
  if (!gamingPageDrawn || gam_contentKey() != gamLastContent) drawGamingPage();
}

#endif
