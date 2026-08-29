#ifndef THEME2_SPOTIFY_H
#define THEME2_SPOTIFY_H

/*
 * ═══════════════════════════════════════════════════════════
 *  THEME 2 SPOTIFY UI
 *  Full-screen album art + overlay text + circular progress
 *  Reads from SAME data vars as Theme 1 — no new data needed
 * ═══════════════════════════════════════════════════════════
 */

#include "globals.h"
#include <math.h>

extern void drawIconPlay(int cx, int cy, uint16_t color);
extern void drawIconPause(int cx, int cy, uint16_t color);
extern void drawIconPrev(int cx, int cy, uint16_t color);
extern void drawIconNext(int cx, int cy, uint16_t color);
extern void drawIconShuffle(int cx, int cy, uint16_t color);
extern void drawIconRepeat(int cx, int cy, uint16_t color);

// ── T2 Spotify Color Palette ──
#define T2S_BG          0x0000  // Black
#define T2S_ACCENT      0x07E0  // Spotify Green
#define T2S_DIM         0x4208  // Mid-dark grey
#define T2S_FRAME       0x2104  // Dark grey border
#define T2S_BLUE        0x0336  // ThingPulse blue
#define T2S_GREEN       0x07E0  // Spotify green

// ── T2 Spotify extended data ──
uint8_t t2_volume = 100;
bool    t2_shuffle = false;
uint8_t t2_repeat = 0;
char    t2_device[24] = "Unknown";

// ── Layout: Scale-to-Fit Album Art ──
#define T2S_ART_SIZE    64
#define T2S_ART_X       4
#define T2S_ART_Y       15

// ── State trackers ──
static String t2s_lastTrack = "";
static String t2s_lastArtist = "";
static String t2s_lastLyric = "";
static String t2s_lastLyric2 = "";
static String t2s_lastPrevLyric = "";
static bool   t2s_artDrawn = false;
bool          t2s_overlayDrawn = false;
static int    t2s_lastProgress = -1;
static int    t2s_lastProgW = -1;

extern bool forceSpotifyRedraw;

void t2_redrawSpotifyPartial() {
  if (currentState != STATE_SPOTIFY || activeTheme != 1) return;

  // ── Album Art ──
  if (!t2s_artDrawn && albumArtReady) {
    tft.drawRect(T2S_ART_X - 2, T2S_ART_Y - 2, T2S_ART_SIZE + 4, T2S_ART_SIZE + 4, T2S_FRAME);
    tft.drawRect(T2S_ART_X - 1, T2S_ART_Y - 1, T2S_ART_SIZE + 2, T2S_ART_SIZE + 2, T2S_DIM);
    
    tft.setSwapBytes(true);
    tft.pushImage(T2S_ART_X, T2S_ART_Y, T2S_ART_SIZE, T2S_ART_SIZE, albumArt);
    tft.setSwapBytes(false);
    
    t2s_artDrawn = true;
  }

  // ── Track Info — Right of album art ──
  int infoX = T2S_ART_X + T2S_ART_SIZE + 6;
  int infoY = T2S_ART_Y;
  String title = currentTrack.length() > 0 ? currentTrack : "No Active Media";
  String artist = currentArtist.length() > 0 ? currentArtist : "CompanionOS";

  if (!forceSpotifyRedraw && title != t2s_lastTrack) {
    tft.fillRect(T2S_ART_X - 2, T2S_ART_Y - 2, T2S_ART_SIZE + 4, T2S_ART_SIZE + 4, T2S_BG);
    t2s_artDrawn = false;
    // Keep albumArtReady intact so background cached art is preserved!
  }

  if (forceSpotifyRedraw || title != t2s_lastTrack) {
    tft.fillRect(infoX, infoY, SCREEN_W - infoX, 10, T2S_BG);
    tft.setTextColor(TFT_WHITE, T2S_BG);
    drawTruncatedText(infoX, infoY, title.c_str(), SCREEN_W - infoX - 4, TFT_WHITE, 1);
    t2s_lastTrack = title;
  }
  if (forceSpotifyRedraw || artist != t2s_lastArtist) {
    tft.fillRect(infoX, infoY + 12, SCREEN_W - infoX, 10, T2S_BG);
    tft.setTextColor(T2S_DIM, T2S_BG);
    drawTruncatedText(infoX, infoY + 12, artist.c_str(), SCREEN_W - infoX - 4, T2S_DIM, 1);
    t2s_lastArtist = artist;
  }

  // ── Progress Bar ──
  int progY = infoY + 28;
  int progX = infoX;
  int progW = SCREEN_W - infoX - 4;

  if (playDuration > 0) {
    int w = map(playProgress, 0, playDuration, 0, progW);
    w = constrain(w, 0, progW);
    
    if (w != t2s_lastProgW || forceSpotifyRedraw) {
      if (t2s_lastProgW >= 0) {
         tft.fillCircle(progX + t2s_lastProgW, progY + 1, 3, T2S_BG);
      }
      tft.fillRect(progX + w, progY, progW - w, 2, T2S_FRAME);
      tft.fillRect(progX, progY, w, 2, T2S_ACCENT);
      tft.fillCircle(progX + w, progY + 1, 3, T2S_ACCENT);
      t2s_lastProgW = w;
    }
  } else {
    tft.fillRect(progX, progY, progW, 2, T2S_FRAME);
    t2s_lastProgW = -1;
  }

  // Time labels
  char t1[12], t2[12];
  sprintf(t1, "%d:%02d", (playProgress/1000)/60, (playProgress/1000)%60);
  sprintf(t2, "%d:%02d", (playDuration/1000)/60, (playDuration/1000)%60);
  tft.setTextColor(T2S_DIM, T2S_BG);
  char pt1[12], pt2[12];
  sprintf(pt1, "%-5s", t1);
  sprintf(pt2, "%5s", t2);
  tft.drawString(pt1, progX, progY + 5, 1);
  tft.drawRightString(pt2, progX + progW, progY + 5, 1);

  // ── Playback Controls Row ──
  int ctrlY = progY + 20;
  int ctrlCX = infoX + (progW / 2);

  tft.fillRect(ctrlCX - 35, ctrlY - 7, 70, 14, T2S_BG);

  drawIconPrev(ctrlCX - 20, ctrlY, TFT_WHITE);
  tft.fillCircle(ctrlCX, ctrlY, 7, T2S_ACCENT);
  if (isPlaying) drawIconPause(ctrlCX, ctrlY, T2S_BG);
  else drawIconPlay(ctrlCX + 1, ctrlY, T2S_BG);
  drawIconNext(ctrlCX + 20, ctrlY, TFT_WHITE);

  // ── Lyrics Panel — Bottom row ──
  int lyrX = 4;
  int lyrY = T2S_ART_Y + T2S_ART_SIZE + 6;
  int lyrW = SCREEN_W - 8;

  bool lyricsChanged = (currentLyrics != t2s_lastLyric ||
                        currentLyricsLine2 != t2s_lastLyric2 || forceSpotifyRedraw);

  if (lyricsChanged) {
    tft.fillRect(0, lyrY, SCREEN_W, 28, T2S_BG);
    if (currentLyrics.length() > 0) {
      int yC = lyrY;

      // Current line (bright accent)
      tft.setTextColor(T2S_ACCENT, T2S_BG);
      drawTruncatedText(lyrX, yC, currentLyrics.c_str(), lyrW, T2S_ACCENT, 1);
      yC += 10;

      // Next line (dim)
      if (currentLyricsLine2.length() > 0 && yC <= SCREEN_H - 8) {
        tft.setTextColor(T2S_DIM, T2S_BG);
        drawTruncatedText(lyrX, yC, currentLyricsLine2.c_str(), lyrW, T2S_DIM, 1);
      }
    }
    t2s_lastLyric = currentLyrics;
    t2s_lastLyric2 = currentLyricsLine2;
  }
}

void t2_resetSpotifyDrawState() {
  t2s_lastTrack = "";
  t2s_lastArtist = "";
  t2s_lastLyric = "";
  t2s_lastLyric2 = "";
  t2s_lastPrevLyric = "";
  t2s_artDrawn = false;
  t2s_overlayDrawn = false;
  t2s_lastProgress = -1;
  t2s_lastProgW = -1;
  forceSpotifyRedraw = true;
}

void t2_drawSpotifyPage() {
  tft.fillScreen(T2S_BG);
  t2_resetSpotifyDrawState();
  tft.drawFastHLine(0, 16, SCREEN_W, T2S_FRAME);
  t2_redrawSpotifyPartial();
}

#endif
