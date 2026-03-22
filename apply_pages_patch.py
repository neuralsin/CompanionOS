import re

with open(r'CompanionOS\arduino\CompanionOS_Main\pages.h', 'r', encoding='utf-8') as f:
    content = f.read()

new_block = """// --- Vector Icons Helper Functions ---
void drawIconPlay(int cx, int cy, uint16_t color) {
  tft.fillTriangle(cx - 3, cy - 5, cx - 3, cy + 5, cx + 5, cy, color);
}
void drawIconPause(int cx, int cy, uint16_t color) {
  tft.fillRect(cx - 4, cy - 4, 3, 8, color);
  tft.fillRect(cx + 2, cy - 4, 3, 8, color);
}
void drawIconPrev(int cx, int cy, uint16_t color) {
  tft.fillRect(cx - 6, cy - 4, 2, 8, color);
  tft.fillTriangle(cx + 4, cy - 4, cx + 4, cy + 4, cx - 3, cy, color);
}
void drawIconNext(int cx, int cy, uint16_t color) {
  tft.fillTriangle(cx - 4, cy - 4, cx - 4, cy + 4, cx + 3, cy, color);
  tft.fillRect(cx + 4, cy - 4, 2, 8, color);
}
void drawIconShuffle(int cx, int cy, uint16_t color) {
  tft.drawLine(cx - 4, cy - 2, cx + 4, cy + 2, color);
  tft.drawLine(cx - 4, cy + 2, cx + 4, cy - 2, color);
  tft.drawLine(cx + 2, cy + 2, cx + 4, cy + 2, color);
  tft.drawLine(cx + 4, cy, cx + 4, cy + 2, color);
  tft.drawLine(cx + 2, cy - 2, cx + 4, cy - 2, color);
  tft.drawLine(cx + 4, cy, cx + 4, cy - 2, color);
}
void drawIconRepeat(int cx, int cy, uint16_t color) {
  tft.drawRect(cx - 4, cy - 3, 8, 6, color);
  tft.drawPixel(cx - 4, cy, COLOR_BG);
  tft.drawPixel(cx + 3, cy, COLOR_BG);
  tft.drawLine(cx + 2, cy - 4, cx + 3, cy - 3, color);
  tft.drawLine(cx + 2, cy - 2, cx + 3, cy - 3, color);
  tft.drawLine(cx - 3, cy + 2, cx - 4, cy + 3, color);
  tft.drawLine(cx - 3, cy + 4, cx - 4, cy + 3, color);
}
void drawIconHeart(int cx, int cy, uint16_t color) {
  tft.fillCircle(cx - 2, cy - 1, 2, color);
  tft.fillCircle(cx + 2, cy - 1, 2, color);
  tft.fillTriangle(cx - 4, cy, cx + 4, cy, cx, cy + 4, color);
}

// --- Layout Definitions ---
#define ALBUM_X 10
#define ALBUM_Y 25
#define ALBUM_SIZE 96

void redrawSpotifyPartial() {
  if (currentState != STATE_SPOTIFY) return;
  
  // ── LEFT COLUMN (Title/Artist/Heart) ──
  tft.fillRect(0, ALBUM_Y + ALBUM_SIZE + 5, 115, 60, COLOR_BG);
  tft.setTextColor(TFT_WHITE);
  tft.drawString(currentTrack.substring(0, 14), 10, ALBUM_Y + ALBUM_SIZE + 10, 2);
  tft.setTextColor(0x8410); // Dim grey
  tft.drawString(currentArtist.substring(0, 14), 10, ALBUM_Y + ALBUM_SIZE + 30, 2);
  // Heart Icon
  drawIconHeart(95, ALBUM_Y + ALBUM_SIZE + 20, 0x8410); 
  
  // ── CENTER COLUMN (Playback Controls) ──
  int ctrlY = 85;
  int ctrlX = 160; // Center of screen
  
  // Clear Center Area
  tft.fillRect(115, ctrlY - 25, 100, 50, COLOR_BG);
  
  // Secondary Icons
  drawIconShuffle(125, ctrlY, 0x8410);
  drawIconPrev(140, ctrlY, TFT_WHITE);
  drawIconNext(180, ctrlY, TFT_WHITE);
  drawIconRepeat(195, ctrlY, 0x8410);
  
  // Play/Pause Circle
  tft.fillCircle(ctrlX, ctrlY, 18, TFT_WHITE); 
  if (isPlaying) {
    drawIconPause(ctrlX, ctrlY, COLOR_BG);
  } else {
    drawIconPlay(ctrlX + 1, ctrlY, COLOR_BG);
  }
  
  // ── PROGRESS BAR ──
  int barX = 120;
  int barY = 135;
  int barW = 80;
  
  tft.fillRect(barX - 10, barY - 10, barW + 20, 30, COLOR_BG); // Clear
  tft.fillRect(barX, barY, barW, 3, 0x4208); // Track line
  
  if (playDuration > 0) {
    int w = map(playProgress, 0, playDuration, 0, barW);
    if (w < 0) w = 0;
    if (w > barW) w = barW;
    tft.fillRect(barX, barY, w, 3, TFT_WHITE); // Fill line
    tft.fillCircle(barX + w, barY + 1, 3, TFT_WHITE); // Thumb
  }
  
  // Draw Time Labels (Dim)
  tft.setTextColor(0x8410);
  char t1[8]; char t2[8];
  sprintf(t1, "%d:%02d", (playProgress/1000)/60, (playProgress/1000)%60);
  sprintf(t2, "%d:%02d", (playDuration/1000)/60, (playDuration/1000)%60);
  tft.drawString(t1, barX - 5, barY + 10, 1);
  tft.drawRightString(t2, barX + barW + 5, barY + 10, 1);
  
  // ── RIGHT COLUMN (Lyrics) ──
  tft.fillRect(215, 20, 105, 200, COLOR_BG);
  
  tft.setTextColor(0x8410);
  tft.drawRightString("LYRICS ->", 310, 25, 1);
  
  tft.setTextColor(0x8410); 
  tft.drawString("...", 215, 50, 2); // Simulating past string
  
  tft.setTextColor(TFT_WHITE);
  tft.drawString(currentLyrics.substring(0, 15), 215, 80, 2); // Active
  
  if (currentLyricsLine2.length() > 0) {
    tft.setTextColor(0x6B4D); // Slightly dimmer than white
    tft.drawString(currentLyricsLine2.substring(0, 15), 215, 110, 2); // Future 1
    tft.setTextColor(0x4208); // Much dimmer
    tft.drawString("...", 215, 140, 2); // Future 2
  }
}

// ═══════════════════════════════════════════════════════════
// ALBUM ART PROCESSING (96x96 Scale)
// ═══════════════════════════════════════════════════════════

bool receivingArt = false;

void prepareAlbumArt() {
  if (currentState == STATE_SPOTIFY) {
    receivingArt = true;
    tft.fillRect(ALBUM_X, ALBUM_Y, ALBUM_SIZE, ALBUM_SIZE, COLOR_BG);
  }
}

void processArtChunk(int chunkIdx, String hexData) {
  if (!receivingArt || currentState != STATE_SPOTIFY) return;
  
  int basePixel = chunkIdx * 100; // Python sends 100 pixels (200 bytes) per chunk
  int len = hexData.length();
  
  for (int i=0; i<len; i+=4) {
    if (i+4 <= len) {
      String hexPixel = hexData.substring(i, i+4);
      uint16_t color = strtol(hexPixel.c_str(), NULL, 16);
      
      int currentPixel = basePixel + (i / 4);
      int x = currentPixel % ALBUM_SIZE;
      int y = currentPixel / ALBUM_SIZE;
      
      if (x < ALBUM_SIZE && y < ALBUM_SIZE) {
        tft.drawPixel(ALBUM_X + x, ALBUM_Y + y, color);
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
  drawPageIndicator(1, 6);
  redrawSpotifyPartial(); // This renders the entire UI 1:1 natively
}

void drawGithubPage() {"""

# Find the void redrawSpotifyPartial() and replace up to drawGithubPage()
import re
pattern = re.compile(r'void redrawSpotifyPartial\(\) \{.*?(?=void drawGithubPage\(\) \{)', re.DOTALL)
new_content = pattern.sub(new_block, content)

with open(r'CompanionOS\arduino\CompanionOS_Main\pages.h', 'w', encoding='utf-8') as f:
    f.write(new_content)
print("Updated pages.h successfully.")
