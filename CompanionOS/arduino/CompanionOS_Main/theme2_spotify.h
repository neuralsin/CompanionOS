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
// Album art centered horizontally, scaled to max square that fits
// Max height: 240 - 16(status bar) - 60(bottom info) = 164px
// Max width: 320 - 20(margins) = 300px
// So max square side = min(164, 300) = 164, but our art is 96px native
// We'll render the 96px art centered and use the extra space for info
#define T2S_ART_SIZE    96
#define T2S_ART_X       140  // Shifted right to make room for lyrics
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
    // Push the actual album art
    tft.pushImage(T2S_ART_X, T2S_ART_Y, T2S_ART_SIZE, T2S_ART_SIZE, albumArt);
    t2s_artDrawn = true;
  }

  // ── Track Info — Below the album art ──
  int infoY = T2S_ART_Y + T2S_ART_SIZE + 8;
  String title = currentTrack.substring(0, 22);
  String artist = currentArtist.substring(0, 26);

  if (title != t2s_lastTrack) {
    tft.fillRect(10, infoY, SCREEN_W - 20, 20, T2S_BG);
    tft.setTextColor(TFT_WHITE, T2S_BG);
    tft.drawCentreString(title, SCREEN_W / 2, infoY, 2);
    t2s_lastTrack = title;
    t2s_artDrawn = false; // Force complete album art redraw for new song
  }
  if (artist != t2s_lastArtist) {
    tft.fillRect(10, infoY + 18, SCREEN_W - 20, 14, T2S_BG);
    tft.setTextColor(T2S_DIM, T2S_BG);
    tft.drawCentreString(artist, SCREEN_W / 2, infoY + 18, 1);
    t2s_lastArtist = artist;
  }

  // ── Progress Bar (horizontal, gold theme) ──
  int progY = infoY + 38;
  int progX = 30;
  int progW = SCREEN_W - 60;

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
  // Pad strings to 5 chars to overwrite previous frame's pixels
  char pt1[12], pt2[12];
  sprintf(pt1, "%-5s", t1);
  sprintf(pt2, "%5s", t2);
  tft.drawString(pt1, progX, progY + 7, 1);
  tft.drawRightString(pt2, progX + progW, progY + 7, 1);

  // ── Playback Controls Row ──
  int ctrlY = progY + 22;
  int ctrlCX = SCREEN_W / 2;

  // Clear control row
  tft.fillRect(ctrlCX - 70, ctrlY - 10, 140, 24, T2S_BG);

  // Prev
  drawIconPrev(ctrlCX - 50, ctrlY, TFT_WHITE);
  // Play/Pause circle (gold)
  tft.fillCircle(ctrlCX, ctrlY, 12, T2S_ACCENT);
  if (isPlaying) drawIconPause(ctrlCX, ctrlY, T2S_BG);
  else drawIconPlay(ctrlCX + 2, ctrlY, T2S_BG);
  // Next
  drawIconNext(ctrlCX + 50, ctrlY, TFT_WHITE);

  // Shuffle/Repeat indicators
  if (t2_shuffle) drawIconShuffle(ctrlCX - 30, ctrlY, T2S_GREEN);
  else            drawIconShuffle(ctrlCX - 30, ctrlY, T2S_DIM);
  if (t2_repeat > 0) drawIconRepeat(ctrlCX + 30, ctrlY, T2S_GREEN);
  else                drawIconRepeat(ctrlCX + 30, ctrlY, T2S_DIM);

  // ── Lyrics Panel — Left sidebar (compact) ──
  int lyrX = 4;
  int lyrY = 25;
  int lyrW = T2S_ART_X - 10;  // Left of album art
  int lyrH = T2S_ART_SIZE;

  bool lyricsChanged = (currentLyrics != t2s_lastLyric ||
                        currentLyricsLine2 != t2s_lastLyric2 ||
                        prevLyricsLine != t2s_lastPrevLyric);

  if (lyricsChanged) {
    if (currentLyrics.length() > 0) {
      int yC = lyrY + 4;

      // Helper lambda to pad strings to max width for seamless overwrite
      auto padString = [](String s, int len) -> String {
        while (s.length() < len) s += " ";
        return s;
      };

      // Previous line (dim)
      tft.setTextColor(T2S_DIM, T2S_BG);
      String p = padString(prevLyricsLine.substring(0, 12), 12);
      tft.drawString(p, lyrX, yC, 1);
      yC += 16;

      // Current line (bright gold)
      tft.setTextColor(T2S_ACCENT, T2S_BG);
      String c1 = padString(currentLyrics.substring(0, 12), 12);
      String c2 = padString(currentLyrics.length() > 12 ? currentLyrics.substring(12, 24) : "", 12);
      tft.drawString(c1, lyrX, yC, 2); yC += 16;
      tft.drawString(c2, lyrX, yC, 2); yC += 20;

      // Next line (dim)
      tft.setTextColor(T2S_DIM, T2S_BG);
      String n = padString(currentLyricsLine2.substring(0, 12), 12);
      tft.drawString(n, lyrX, yC, 1);
    } else {
      // Clear lyrics area when empty (e.g., new song without lyrics or instrumental break)
      tft.fillRect(0, lyrY, lyrW, 80, T2S_BG);
    }

    t2s_lastLyric = currentLyrics;
    t2s_lastLyric2 = currentLyricsLine2;
    t2s_lastPrevLyric = prevLyricsLine;
  }

  // ── Volume/Device indicator — Right sidebar ──
  int rsX = T2S_ART_X + T2S_ART_SIZE + 10;
  int rsY = 30;
  int rsW = SCREEN_W - rsX - 4;

  if (!t2s_overlayDrawn) {
    tft.fillRect(rsX, rsY, rsW, 80, T2S_BG);

    // Volume bar (vertical)
    tft.setTextColor(T2S_DIM, T2S_BG);
    tft.drawString("VOL", rsX, rsY, 1);
    int volBarY = rsY + 12;
    int volBarH = 50;
    tft.fillRect(rsX + 8, volBarY, 4, volBarH, T2S_FRAME);
    int volFill = map(t2_volume, 0, 100, 0, volBarH);
    tft.fillRect(rsX + 8, volBarY + volBarH - volFill, 4, volFill, T2S_ACCENT);
    // Volume percentage
    char volStr[6];
    sprintf(volStr, "%d%%", t2_volume);
    tft.drawString(volStr, rsX, volBarY + volBarH + 4, 1);

    t2s_overlayDrawn = true;
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
