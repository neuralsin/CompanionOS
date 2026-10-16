#ifndef PAGE_SOCIAL_H
#define PAGE_SOCIAL_H
#include "globals.h"
#include "ui_components.h"

extern void drawPageIndicator(int current, int total);

#define SOC_BG       0x0841
#define SOC_CARD     0x1082
#define SOC_CARD_HI  0x18E3
#define SOC_DIM      0x4A49
#define SOC_BLUE     0x2A9F
#define SOC_RED_HEART 0xF800
#define SOC_DIVIDER  0x2104

static bool socialPageDrawn = false;
static String socLastContent = "";

void resetSocialDrawState() {
  socialPageDrawn = false;
  socLastContent = "";
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
  drawTruncatedText(cx - 3, cy - 4, initial, 6, TFT_WHITE, 1, color);
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

static String soc_contentKey() {
  return String(uiPageGeneration) + "\n" + socialUser + "\n" + socialApp + "\n" + socialBody + "\n" +
         socialTime + "/" + String(socialLikes) + "/" + String(socialComments);
}

void drawSocialPage() {
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  tft.fillRect(0, UI_HEADER_H, SCREEN_W, SCREEN_H - UI_HEADER_H, SOC_BG);
  drawTruncatedText(4, UI_HEADER_H + 2, "SOCIAL", SCREEN_W - 8, SOC_BLUE, 1, SOC_BG);

  if (!socialUser[0] && !socialBody[0] && !socialApp[0]) {
    drawWrappedText(8, UI_HEADER_H + 20, "No recent activity", SCREEN_W - 16,
                    SCREEN_H - UI_HEADER_H - UI_FOOTER_H - 24, SOC_DIM, 1, SOC_BG);
  } else {
    const int cardX = 4;
    const int cardY = UI_HEADER_H + 14;
    const int cardW = SCREEN_W - 8;
    const int cardH = SCREEN_H - UI_FOOTER_H - cardY - 2;
    const int userFont = SCREEN_W > 200 ? 2 : 1;
    const int userY = cardY + 4;
    const int appY = userY + tft.fontHeight(userFont) + 4;
    const int dividerY = appY + tft.fontHeight(1) + 4;
    const int bodyY = dividerY + 4;
    const int actionY = cardY + cardH - tft.fontHeight(1) - 6;
    tft.fillRoundRect(cardX, cardY, cardW, cardH, 4, SOC_CARD);
    tft.drawRoundRect(cardX, cardY, cardW, cardH, 4, SOC_CARD_HI);

    uint16_t appColor = getSocialAppColor(socialApp);
    drawSocialAvatar(cardX + 12, userY + 8, 8, socialUser, appColor);
    String user = socialUser[0] ? String(socialUser) : String("Unknown user");
    if (socialUser[0] && socialUser[0] != '@') user = "@" + user;
    drawTruncatedText(cardX + 26, userY, user.c_str(), cardW - 32, TFT_WHITE, userFont, SOC_CARD);

    int timeW = min(cardW / 2, (int)tft.textWidth(socialTime, 1));
    drawTruncatedText(cardX + 6, appY, socialApp[0] ? socialApp : "Unknown app",
                      cardW - timeW - 18, appColor, 1, SOC_CARD);
    if (timeW > 0) {
      drawTruncatedText(cardX + cardW - timeW - 6, appY, socialTime, timeW, SOC_DIM, 1, SOC_CARD);
    }
    tft.drawFastHLine(cardX + 6, dividerY, cardW - 12, SOC_DIVIDER);
    drawWrappedText(cardX + 6, bodyY, socialBody[0] ? socialBody : "No message text",
                    cardW - 12, actionY - bodyY - 6, TFT_WHITE, userFont, SOC_CARD);

    tft.drawFastHLine(cardX + 6, actionY - 4, cardW - 12, SOC_DIVIDER);
    drawSocialHeart(cardX + 12, actionY + 3, SOC_DIM, false);
    char count[8];
    snprintf(count, sizeof(count), "%u", (unsigned)socialLikes);
    drawTruncatedText(cardX + 24, actionY, count, cardW / 2 - 28, SOC_DIM, 1, SOC_CARD);
    int commentX = cardX + cardW / 2 + 8;
    drawSocialComment(commentX, actionY + 3, SOC_DIM);
    snprintf(count, sizeof(count), "%u", (unsigned)socialComments);
    drawTruncatedText(commentX + 10, actionY, count, cardW / 2 - 24, SOC_DIM, 1, SOC_CARD);
  }
  drawPageIndicator(STATE_SOCIAL, STATE_COUNT);
  socialPageDrawn = true;
  socLastContent = soc_contentKey();
}

void redrawSocialPartial() {
  if (currentState != STATE_SOCIAL) return;
  if (!socialPageDrawn || soc_contentKey() != socLastContent) drawSocialPage();
}

#endif
