#ifndef PAGE_SOCIAL_H
#define PAGE_SOCIAL_H
#include "globals.h"

extern uint16_t blendColor(uint16_t c1, uint16_t c2, float t);
extern void drawPageIndicator(int current, int total);

// ═══════════════════════════════════════════════════════════
// SOCIAL DASHBOARD — V6 Premium Notification Card Page
// ═══════════════════════════════════════════════════════════

#define SOC_BG       0x0841
#define SOC_CARD     0x1082
#define SOC_CARD_HI  0x18E3
#define SOC_DIM      0x4A49
#define SOC_BLUE     0x2A9F
#define SOC_RED_HEART 0xF800
#define SOC_DIVIDER  0x2104

static bool socialPageDrawn = false;
static char lastSocialBody[80] = "";

void resetSocialDrawState() {
  socialPageDrawn = false;
  lastSocialBody[0] = 0;
}

uint16_t getSocialAppColor(const char* app) {
  if (strstr(app, "Discord") || strstr(app, "discord")) return 0x5A9F;
  if (strstr(app, "Slack") || strstr(app, "slack"))     return 0x4C0B;
  if (strstr(app, "Mail") || strstr(app, "mail"))       return 0xFBE0;
  if (strstr(app, "Teams") || strstr(app, "teams"))     return 0x4A1F;
  if (strstr(app, "Twitter") || strstr(app, "X"))       return 0x07FF;
  return SOC_BLUE;
}

void drawSocialAvatar(int cx, int cy, int r, const char* user, uint16_t color) {
  tft.fillCircle(cx, cy, r, color);
  tft.drawCircle(cx, cy, r, blendColor(color, TFT_BLACK, 0.3f));
  char initial[2] = {0, 0};
  if (user[0] == '@' && user[1]) initial[0] = user[1];
  else if (user[0]) initial[0] = user[0];
  else initial[0] = '?';
  if (initial[0] >= 'a' && initial[0] <= 'z') initial[0] -= 32;
  tft.setTextColor(TFT_WHITE);
  tft.drawCentreString(initial, cx, cy - 5, 2);
}

void drawSocialHeart(int cx, int cy, uint16_t color, bool filled) {
  if (filled) {
    tft.fillCircle(cx - 3, cy - 1, 3, color);
    tft.fillCircle(cx + 3, cy - 1, 3, color);
    tft.fillTriangle(cx - 6, cy, cx + 6, cy, cx, cy + 6, color);
  } else {
    tft.drawCircle(cx - 3, cy - 1, 3, color);
    tft.drawCircle(cx + 3, cy - 1, 3, color);
    tft.drawLine(cx - 6, cy, cx, cy + 6, color);
    tft.drawLine(cx + 6, cy, cx, cy + 6, color);
  }
}

void drawSocialComment(int cx, int cy, uint16_t color) {
  tft.drawRoundRect(cx - 5, cy - 4, 10, 8, 2, color);
  tft.drawLine(cx - 2, cy + 4, cx, cy + 7, color);
  tft.drawLine(cx, cy + 7, cx + 1, cy + 4, color);
}

void drawSocialShare(int cx, int cy, uint16_t color) {
  tft.drawLine(cx - 4, cy, cx + 4, cy - 4, color);
  tft.drawLine(cx + 4, cy - 4, cx + 4, cy + 4, color);
  tft.drawLine(cx + 4, cy + 4, cx - 4, cy, color);
  tft.fillCircle(cx - 4, cy, 2, color);
  tft.fillCircle(cx + 4, cy - 4, 2, color);
  tft.fillCircle(cx + 4, cy + 4, 2, color);
}

int drawWrappedText(int x, int y, int maxW, const char* text, int font, uint16_t color, int maxLines) {
  tft.setTextColor(color);
  int lineCount = 0;
  int cx = x;
  int cy = y;
  int lineH = (font == 2) ? 16 : 10;
  String txt = String(text);
  int start = 0;
  while (start < (int)txt.length() && lineCount < maxLines) {
    int end = start;
    int lastSpace = start;
    while (end < (int)txt.length()) {
      if (txt[end] == ' ') lastSpace = end;
      String sub = txt.substring(start, end + 1);
      if (tft.textWidth(sub, font) > maxW) {
        if (lastSpace > start) end = lastSpace;
        break;
      }
      end++;
    }
    String line = txt.substring(start, end);
    if (lineCount == maxLines - 1 && end < (int)txt.length()) {
      line = line.substring(0, max(0, (int)line.length() - 3)) + "...";
    }
    tft.drawString(line, cx, cy, font);
    cy += lineH;
    lineCount++;
    start = end;
    if (start < (int)txt.length() && txt[start] == ' ') start++;
  }
  return lineCount;
}

void drawSocialPage() {
  tft.fillScreen(SOC_BG);

  tft.setTextColor(SOC_DIM);
  tft.drawString("SOCIAL", 10, 3, 1);
  tft.drawFastHLine(0, 15, SCREEN_W, SOC_DIVIDER);

  if (!socialUser[0] && !socialBody[0]) {
    tft.setTextColor(SOC_DIM);
    tft.drawCentreString("No recent activity", SCREEN_W/2, SCREEN_H/2 - 20, 2);
    tft.drawCentreString("Notifications will", SCREEN_W/2, SCREEN_H/2, 1);
    tft.drawCentreString("appear here", SCREEN_W/2, SCREEN_H/2 + 12, 1);
    drawPageIndicator(STATE_SOCIAL, STATE_COUNT);
    socialPageDrawn = true;
    return;
  }

  int cardX = 15, cardY = 24, cardW = SCREEN_W - 30, cardH = 185;
  tft.fillRoundRect(cardX + 2, cardY + 2, cardW, cardH, 8, 0x0421);
  tft.fillRoundRect(cardX, cardY, cardW, cardH, 8, SOC_CARD);
  tft.drawRoundRect(cardX, cardY, cardW, cardH, 8, SOC_CARD_HI);

  uint16_t appColor = getSocialAppColor(socialApp);
  drawSocialAvatar(cardX + 20, cardY + 22, 12, socialUser, appColor);

  tft.setTextColor(TFT_WHITE);
  char userDisp[18];
  snprintf(userDisp, sizeof(userDisp), "@%s", socialUser[0] ? socialUser : "user");
  tft.drawString(userDisp, cardX + 38, cardY + 14, 2);

  tft.setTextColor(appColor);
  tft.drawString(socialApp, cardX + 38, cardY + 32, 1);

  tft.setTextColor(SOC_DIM);
  tft.drawRightString(socialTime, cardX + cardW - 10, cardY + 14, 1);

  tft.drawFastHLine(cardX + 12, cardY + 48, cardW - 24, SOC_DIVIDER);

  int bodyY = cardY + 56;
  drawWrappedText(cardX + 14, bodyY, cardW - 28, socialBody, 2, TFT_WHITE, 4);

  int actionY = cardY + cardH - 30;
  tft.drawFastHLine(cardX + 12, actionY - 8, cardW - 24, SOC_DIVIDER);

  int heartX = cardX + 30;
  drawSocialHeart(heartX, actionY + 5, SOC_DIM, false);
  char likeStr[8]; sprintf(likeStr, "%d", socialLikes);
  tft.setTextColor(SOC_DIM);
  tft.drawString(likeStr, heartX + 12, actionY, 1);

  int commentX = cardX + 100;
  drawSocialComment(commentX, actionY + 4, SOC_DIM);
  char cmtStr[8]; sprintf(cmtStr, "%d", socialComments);
  tft.setTextColor(SOC_DIM);
  tft.drawString(cmtStr, commentX + 12, actionY, 1);

  int shareX = cardX + 170;
  drawSocialShare(shareX, actionY + 4, SOC_DIM);

  drawPageIndicator(STATE_SOCIAL, STATE_COUNT);
  socialPageDrawn = true;
}

void redrawSocialPartial() {
  if (currentState != STATE_SOCIAL) return;
  if (strcmp(socialBody, lastSocialBody) != 0) {
    drawSocialPage();
    strcpy(lastSocialBody, socialBody);
  }
}

#endif
