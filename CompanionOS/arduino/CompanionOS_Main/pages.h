#ifndef PAGES_H
#define PAGES_H

#include "globals.h"
#include "ui.h"
#include "eyes.h"

// Bring in data from network.h
extern String currentTrack;
extern String currentArtist;
extern String currentLyrics;
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
  tft.fillRect(0, 170, SCREEN_W, 85, COLOR_BG);
  
  tft.setTextColor(TFT_GREEN);
  tft.drawCentreString(currentTrack.substring(0, 20), SCREEN_W/2, 170, 2);
  tft.setTextColor(TFT_WHITE);
  tft.drawCentreString(currentArtist.substring(0, 20), SCREEN_W/2, 190, 2);
  
  // Lyrics
  tft.setTextColor(TFT_LIGHTGREY);
  tft.drawCentreString(currentLyrics.substring(0, 25), SCREEN_W/2, 215, 2);
  
  // Progress Bar
  tft.drawRect(20, 240, 200, 10, TFT_WHITE);
  if (playDuration > 0) {
    int w = map(playProgress, 0, playDuration, 0, 196);
    if (w < 0) w = 0;
    if (w > 196) w = 196;
    tft.fillRect(22, 242, w, 6, TFT_GREEN);
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
int artY = 40;

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
  drawPageIndicator(0, 4);
}

void drawSpotifyPage() {
  tft.fillScreen(COLOR_BG);
  drawTopBar("Spotify Player");
  drawPageIndicator(1, 4);

  // Blank art box
  tft.drawRect(artX - 1, artY - 1, 122, 122, TFT_DARKGREY);

  redrawSpotifyPartial();

  drawButton(20, 270, 60, 40, "Prev", TFT_DARKGREY);
  drawButton(90, 270, 60, 40, "Play", TFT_GREEN);
  drawButton(160, 270, 60, 40, "Next", TFT_DARKGREY);
}

void drawGithubPage() {
  tft.fillScreen(COLOR_BG);
  drawTopBar("GitHub Stats");
  drawPageIndicator(2, 4);
  redrawGithubPartial();
}

void drawNotesPage() {
  tft.fillScreen(COLOR_BG);
  drawTopBar("Quick Notes");
  drawPageIndicator(3, 4);

  tft.setTextColor(TFT_YELLOW);
  tft.drawString("* Stay hydrated", 10, 50, 2);
  tft.drawString("* Check emails", 10, 80, 2);
  tft.drawString("* Fix bugs", 10, 110, 2);
  tft.drawString("* Call mom", 10, 140, 2);
}

void renderCurrentPage() {
  switch(currentState) {
    case STATE_EYES: drawEyesPage(); break;
    case STATE_SPOTIFY: drawSpotifyPage(); break;
    case STATE_GITHUB: drawGithubPage(); break;
    case STATE_NOTES: drawNotesPage(); break;
  }
}

void changePage(int direction) {
  int next = (int)currentState + direction;
  if (next >= 4) next = 0;
  if (next < 0) next = 3;
  currentState = (AppState)next;
  renderCurrentPage();
}

#endif
