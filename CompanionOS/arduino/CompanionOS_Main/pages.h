#ifndef PAGES_H
#define PAGES_H

#include "globals.h"
#include "ui.h"
#include "eyes.h"

// Bring in data from network.h
extern String currentTrack;
extern String currentArtist;
extern String currentLyrics;
extern String currentLyricsLine2;
extern int playProgress;
extern int playDuration;
extern bool isPlaying;

extern String ghUser;
extern String ghRepos;
extern String ghFollowers;

// ═══════════════════════════════════════════════════════════
// UI PAGES
// ═══════════════════════════════════════════════════════════

void redrawSpotifyPartial() {
  if (currentState != STATE_SPOTIFY) return;
  
  // Clear the text area below the album art
  tft.fillRect(0, 145, SCREEN_W, 95, COLOR_BG);
  
  tft.setTextColor(TFT_GREEN);
  tft.drawCentreString(currentTrack.substring(0, 20), SCREEN_W/2, 145, 2);
  tft.setTextColor(TFT_WHITE);
  tft.drawCentreString(currentArtist.substring(0, 20), SCREEN_W/2, 160, 2);
  
  // Lyrics
  tft.setTextColor(TFT_LIGHTGREY);
  tft.drawCentreString(currentLyrics.substring(0, 25), SCREEN_W/2, 175, 2);
  tft.drawCentreString(currentLyricsLine2.substring(0, 25), SCREEN_W/2, 190, 2);
  
  // Progress Bar
  tft.drawRect(60, 205, 200, 10, TFT_WHITE);
  if (playDuration > 0) {
    int w = map(playProgress, 0, playDuration, 0, 196);
    if (w < 0) w = 0;
    if (w > 196) w = 196;
    tft.fillRect(62, 207, w, 6, TFT_GREEN);
  }
}

void redrawGithubPartial() {
  if (currentState != STATE_GITHUB) return;
  
  tft.fillRect(0, 50, SCREEN_W, 200, COLOR_BG);
  if (ghUser.length() > 0) {
    tft.setTextColor(TFT_WHITE);
    tft.drawCentreString("@" + ghUser, SCREEN_W/2, 80, 4);
    
    tft.setTextColor(TFT_SKYBLUE);
    tft.drawCentreString("Repositories: " + ghRepos, SCREEN_W/2, 130, 2);
    tft.drawCentreString("Followers: " + ghFollowers, SCREEN_W/2, 160, 2);
  } else {
    tft.setTextColor(TFT_DARKGREY);
    tft.drawCentreString("Awaiting Data...", SCREEN_W/2, 160, 2);
  }
}

void redrawNotesPartial() {
  if (currentState != STATE_NOTES) return;
  tft.fillRect(0, 40, SCREEN_W, 200, COLOR_BG);
  tft.setTextColor(TFT_YELLOW);
  for(int i=0; i<4; i++) {
    if(currentNotes[i].length() > 0) {
      tft.drawString(currentNotes[i], 10, 50 + (i * 30), 2);
    }
  }
}

// ═══════════════════════════════════════════════════════════
// ALBUM ART PROCESSING
// ═══════════════════════════════════════════════════════════

bool receivingArt = false;
int artX = (SCREEN_W - 120) / 2;
int artY = 20;

void prepareAlbumArt() {
  if (currentState == STATE_SPOTIFY) {
    receivingArt = true;
    tft.fillRect(artX, artY, 120, 120, COLOR_BG);
  }
}

void processArtChunk(int chunkIdx, String hexData) {
  if (!receivingArt || currentState != STATE_SPOTIFY) return;
  
  // Each chunk represents 200 pixels
  int basePixel = chunkIdx * 200;
  int len = hexData.length();
  
  for (int i=0; i<len; i+=4) {
    if (i+4 <= len) {
      String hexPixel = hexData.substring(i, i+4);
      uint16_t color = strtol(hexPixel.c_str(), NULL, 16);
      
      int currentPixel = basePixel + (i / 4);
      int x = currentPixel % 120;
      int y = currentPixel / 120;
      
      if (x < 120 && y < 120) {
        tft.drawPixel(artX + x, artY + y, color);
      }
    }
  }
}

void completeAlbumArt() {
  receivingArt = false;
}

// ═══════════════════════════════════════════════════════════
// PAGE DRAWING
// ═══════════════════════════════════════════════════════════

void drawEyesPage() {
  drawEyes();
  drawTopBar("Companion OS");
  drawPageIndicator(0, 6);
}

void drawSpotifyPage() {
  tft.fillScreen(COLOR_BG);
  drawTopBar("Spotify Player");
  drawPageIndicator(1, 6);

  tft.drawRect(artX - 1, artY - 1, 122, 122, TFT_DARKGREY);
  redrawSpotifyPartial();

  drawButton(60, 218, 60, 20, "Prev", TFT_DARKGREY);
  drawButton(130, 218, 60, 20, "Play", TFT_GREEN);
  drawButton(200, 218, 60, 20, "Next", TFT_DARKGREY);
}

void drawGithubPage() {
  tft.fillScreen(COLOR_BG);
  drawTopBar("GitHub Stats");
  drawPageIndicator(2, 6);
  redrawGithubPartial();
}

void drawNotesPage() {
  tft.fillScreen(COLOR_BG);
  drawTopBar("Quick Notes");
  drawPageIndicator(3, 6);
  redrawNotesPartial();
}

// ── NEW: AUDIO VISUALIZER ────────────────────────────────
void drawVisualizerPage() {
  tft.fillScreen(COLOR_BG);
  drawTopBar("Audio Visualizer");
  drawPageIndicator(4, 6);
  
  // Draw base frequency lines
  for (int i=0; i<10; i++) {
    int x = 30 + (i * 26);
    tft.fillRect(x, 220, 15, 4, TFT_DARKGREY);
  }
}

void drawSettingsPage() {
  tft.fillScreen(COLOR_BG);
  drawTopBar("System Monitor");
  drawPageIndicator(5, 6);
  
  tft.setTextColor(TFT_WHITE);
  tft.drawString("Network IP:", 20, 60, 2);
  tft.setTextColor(TFT_GREEN);
  tft.drawString(WiFi.localIP().toString(), 20, 80, 4);

  tft.setTextColor(TFT_WHITE);
  tft.drawString("Free Memory (RAM):", 20, 130, 2);
  tft.setTextColor(TFT_CYAN);
  tft.drawNumber(ESP.getFreeHeap(), 20, 150, 4);
  
  tft.setTextColor(TFT_WHITE);
  tft.drawString("System Uptime:", 20, 200, 2);
  
  unsigned long secs = millis() / 1000;
  int h = secs / 3600;
  int m = (secs % 3600) / 60;
  int s = secs % 60;
  char upBuf[16];
  sprintf(upBuf, "%02dh %02dm %02ds", h, m, s);
  
  tft.setTextColor(TFT_YELLOW);
  tft.drawString(upBuf, 20, 220, 4);
}

// ═══════════════════════════════════════════════════════════
// STATE ORCHESTRATION
// ═══════════════════════════════════════════════════════════

void renderCurrentPage() {
  switch(currentState) {
    case STATE_EYES: drawEyesPage(); break;
    case STATE_SPOTIFY: drawSpotifyPage(); break;
    case STATE_GITHUB: drawGithubPage(); break;
    case STATE_NOTES: drawNotesPage(); break;
    case STATE_VISUALIZER: drawVisualizerPage(); break;
    case STATE_SETTINGS: drawSettingsPage(); break;
  }
}

void changePage(int direction) {
  int next = (int)currentState + direction;
  if (next >= 6) next = 0;
  if (next < 0) next = 5;
  currentState = (AppState)next;
  renderCurrentPage();
}

// Visualizer continuous loop called from main sketch
void updateVisualizer() {
  int sample = analogRead(MIC_PIN);
  int mappedHeight = map(sample, 0, 1024, 0, 180);
  if (mappedHeight < 0) mappedHeight = 0;
  
  // Randomly distribute to create a fake EQ since we don't have FFT
  int bar = random(0, 10);
  int x = 20 + (bar * 20);
  
  // Draw new peak
  tft.fillRect(x, 260 - mappedHeight, 15, mappedHeight, TFT_CYAN);
  // Erase trail above peak directly to simulate gravity drop decay
  tft.fillRect(x, 80, 15, 180 - mappedHeight, COLOR_BG); 
}

#endif
