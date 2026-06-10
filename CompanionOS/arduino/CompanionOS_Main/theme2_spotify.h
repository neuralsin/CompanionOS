#ifndef THEME2_SPOTIFY_H
#define THEME2_SPOTIFY_H

/*
 * ═══════════════════════════════════════════════════════════
 *  THEME 2 SPOTIFY UI
 *  Inspired by esp32-spotify-remote-master (ThingPulse)
 *
 *  Full-screen album art + overlay text + circular progress
 *  Reads from SAME data vars as Theme 1 — no new data needed
 *
 *  Color palette: Deep blues / gold accents (from DisplayUI.h)
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

// ── T2 Spotify Color Palette (from TFTColor enum) ──
#define T2S_BG          0x0000  // Black
#define T2S_ACCENT      0x07E0  // Spotify Green
#define T2S_DIM         0x4208  // Mid-dark grey
#define T2S_FRAME       0x2104  // Dark grey border
#define T2S_BLUE        0x0336  // ThingPulse blue
#define T2S_GREEN       0x07E0  // Spotify green

// ── T2 Spotify extended data (sent via T2SPOT: UDP) ──
uint8_t t2_volume = 100;
bool    t2_shuffle = false;
uint8_t t2_repeat = 0; // 0=off, 1=context, 2=track
char    t2_device[24] = "Unknown";

// ── Layout: Scale-to-Fit Album Art ──
// Album art on the left (64x64), info on the right
#define T2S_ART_SIZE    64
#define T2S_ART_X       4
#define T2S_ART_Y       20

// ── State trackers (independent from Theme 1) ──
static String t2s_lastTrack = "";
static String t2s_lastArtist = "";
static String t2s_lastLyric = "";
static String t2s_lastLyric2 = "";
static String t2s_lastPrevLyric = "";
static bool   t2s_artDrawn = false;
bool          t2s_overlayDrawn = false;
static int    t2s_lastProgress = -1;
static int    t2s_lastProgW = -1;

// ═══════════════════════════════════════════════════════════
// DRAWING HELPERS
// ═══════════════════════════════════════════════════════════

// Draw a circular progress arc (quarter-degree resolution)
void t2s_drawProgressArc(int cx, int cy, int r, int rInner, 
                         float progressFrac, uint16_t fgColor, uint16_t bgColor) {
  // Draw track (full circle, dim)
  for (int angle = 0; angle < 360; angle += 3) {
    float rad = angle * PI / 180.0;
    int x1 = cx + cos(rad) * rInner;
    int y1 = cy + sin(rad) * rInner;
    int x2 = cx + cos(rad) * r;
    int y2 = cy + sin(rad) * r;
    tft.drawLine(x1, y1, x2, y2, bgColor);
  }
  // Draw progress (filled portion, bright)
  int endAngle = (int)(progressFrac * 360.0);
  // Start from top (-90 degrees)
  for (int angle = 0; angle < endAngle; angle += 3) {
    float rad = (angle - 90) * PI / 180.0;
    int x1 = cx + cos(rad) * rInner;
    int y1 = cy + sin(rad) * rInner;
    int x2 = cx + cos(rad) * r;
    int y2 = cy + sin(rad) * r;
    tft.drawLine(x1, y1, x2, y2, fgColor);
  }
}

// ═══════════════════════════════════════════════════════════
// MAIN RENDER FUNCTIONS
// ═══════════════════════════════════════════════════════════

void t2_redrawSpotifyPartial() {
  if (currentState != STATE_SPOTIFY || activeTheme != 1) return;

  // ── Album Art (centered, scale-to-fit at native 96px) ──
  if (!t2s_artDrawn && albumArtReady) {
    // Draw art frame border
    tft.drawRect(T2S_ART_X - 2, T2S_ART_Y - 2, T2S_ART_SIZE + 4, T2S_ART_SIZE + 4, T2S_FRAME);
    tft.drawRect(T2S_ART_X - 1, T2S_ART_Y - 1, T2S_ART_SIZE + 2, T2S_ART_SIZE + 2, T2S_DIM);
    // Push the actual album art with correct endianness
    tft.setSwapBytes(true);
    tft.pushImage(T2S_ART_X, T2S_ART_Y, T2S_ART_SIZE, T2S_ART_SIZE, albumArt);
    tft.setSwapBytes(false);
    t2s_artDrawn = true;
  }

  // ── Track Info — To the right of album art ──
  int infoX = T2S_ART_X + T2S_ART_SIZE + 6;
  int infoY = T2S_ART_Y;
  String title = currentTrack.substring(0, 14); // Shorter for side layout
  String artist = currentArtist.substring(0, 16);

  if (forceSpotifyRedraw || title != t2s_lastTrack) {
    tft.fillRect(infoX, infoY, SCREEN_W - infoX, 10, T2S_BG);
    tft.setTextColor(TFT_WHITE, T2S_BG);
    tft.drawString(title, infoX, infoY, 1);
    
    if (title != t2s_lastTrack) {
      tft.fillRect(T2S_ART_X, T2S_ART_Y, T2S_ART_SIZE, T2S_ART_SIZE, T2S_BG); // Clear old art
      t2s_artDrawn = false; // Force complete album art redraw for new song
      albumArtReady = false; // Prevent ghosting of old art
    }
    t2s_lastTrack = title;
  }
  if (forceSpotifyRedraw || artist != t2s_lastArtist) {
    tft.fillRect(infoX, infoY + 12, SCREEN_W - infoX, 10, T2S_BG);
    tft.setTextColor(T2S_DIM, T2S_BG);
    tft.drawString(artist, infoX, infoY + 12, 1);
    t2s_lastArtist = artist;
  }

  // ── Progress Bar (horizontal, gold theme) ──
  int progY = infoY + 30;
  int progX = infoX;
  int progW = SCREEN_W - infoX - 4;

  if (playDuration > 0) {
    int w = map(playProgress, 0, playDuration, 0, progW);
    w = constrain(w, 0, progW);
    
    if (w != t2s_lastProgW) {
      // Clear old knob specifically
      if (t2s_lastProgW >= 0) {
         tft.fillCircle(progX + t2s_lastProgW, progY + 1, 4, T2S_BG);
      }
      // Redraw track bg only where needed (ahead of scrubber)
      tft.fillRect(progX + w, progY, progW - w, 3, T2S_FRAME);
      // Redraw filled portion
      tft.fillRect(progX, progY, w, 3, T2S_ACCENT);
      // Draw new knob
      tft.fillCircle(progX + w, progY + 1, 4, T2S_ACCENT);
      t2s_lastProgW = w;
    }
  } else {
    tft.fillRect(progX, progY, progW, 3, T2S_FRAME);
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
  tft.drawString(pt1, progX, progY + 6, 1);
  tft.drawRightString(pt2, progX + progW, progY + 6, 1);

  // ── Playback Controls Row ──
  int ctrlY = progY + 22;
  int ctrlCX = infoX + (progW / 2);

  // Clear control row
  tft.fillRect(ctrlCX - 35, ctrlY - 8, 70, 16, T2S_BG);

  // Prev
  drawIconPrev(ctrlCX - 20, ctrlY, TFT_WHITE);
  // Play/Pause circle (gold)
  tft.fillCircle(ctrlCX, ctrlY, 8, T2S_ACCENT);
  if (isPlaying) drawIconPause(ctrlCX, ctrlY, T2S_BG);
  else drawIconPlay(ctrlCX + 1, ctrlY, T2S_BG);
  // Next
  drawIconNext(ctrlCX + 20, ctrlY, TFT_WHITE);

  // ── Lyrics Panel — Bottom row ──
  int lyrX = 4;
  int lyrY = T2S_ART_Y + T2S_ART_SIZE + 6;
  int lyrW = SCREEN_W - 8;

  bool lyricsChanged = (currentLyrics != t2s_lastLyric ||
                        currentLyricsLine2 != t2s_lastLyric2);

  if (lyricsChanged) {
    if (currentLyrics.length() > 0) {
      int yC = lyrY;

      auto padString = [](String s, int len) -> String {
        while (s.length() < len) s += " ";
        return s;
      };

      // Current line (bright gold)
      tft.setTextColor(T2S_ACCENT, T2S_BG);
      String c1 = padString(currentLyrics.substring(0, 26), 26);
      tft.drawString(c1, lyrX, yC, 1); yC += 10;

      // Next line (dim)
      tft.setTextColor(T2S_DIM, T2S_BG);
      String n = padString(currentLyricsLine2.substring(0, 26), 26);
      tft.drawString(n, lyrX, yC, 1);
    } else {
      // Clear lyrics area
      tft.fillRect(0, lyrY, lyrW, 20, T2S_BG);
    }

    t2s_lastLyric = currentLyrics;
    t2s_lastLyric2 = currentLyricsLine2;
  }
}

  t2s_artDrawn = false;
  t2s_overlayDrawn = false;
  t2s_lastProgress = -1;
  t2s_lastProgW = -1;
  extern bool forceSpotifyRedraw;
  forceSpotifyRedraw = true;
}

void t2_drawSpotifyPage() {
  tft.fillScreen(T2S_BG);
  t2_resetSpotifyDrawState();

  // Draw a subtle top border line for visual separation from status bar
  tft.drawFastHLine(0, 16, SCREEN_W, T2S_FRAME);

  // Initial full draw
  t2_redrawSpotifyPartial();
}

#endif
