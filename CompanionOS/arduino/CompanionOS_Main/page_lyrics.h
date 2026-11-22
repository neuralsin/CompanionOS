#ifndef COMPANION_PAGE_LYRICS_H
#define COMPANION_PAGE_LYRICS_H
#include "lyrics_state.h"
#include "ui_components.h"

namespace lyrics_page {
static TFT_eSprite row(&tft);
static bool buffered = false;
static uint32_t generation = UINT32_MAX, lastTick = 0, changedAt = 0, lastAllocation = 0, motionTime = 0;
static String track, artist, lines[2], rawLines[2], status;
static int widths[2] = {}, durationSeconds = -1;
static int64_t lineStart = -1;
static int offsets[2] = {-9999, -9999}, activeWord = -2, progress = -1, minute = -1, seconds = -1;
static bool playing = false, artReady = false, synced = false, wordTimed = false;
static uint32_t artRevision = UINT32_MAX;
static String outgoing;
static uint32_t transitionAt = 0;
static bool transitioning = false;
static int outgoingOffset = 0, paintedFade = -1;
constexpr bool compact = SCREEN_W < 240;
constexpr int artSide = compact ? 44 : 108;
constexpr int left = compact ? 8 : 128;
constexpr int width = SCREEN_W - left - 8;
constexpr int rowHeight = compact ? 18 : 42;
constexpr int top = compact ? 72 : 104;
constexpr int currentFont = compact ? 2 : 4;
constexpr int nextFont = compact ? 1 : 2;

inline void release() {
  if (buffered) row.deleteSprite();
  buffered = false;
  transitioning = false; outgoing = String(); paintedFade = -1;
  if (currentState != STATE_LYRICS) {
    track = String(); artist = String(); status = String();
    for (int i = 0; i < 2; ++i) { lines[i] = String(); rawLines[i] = String(); }
  }
}
inline void allocate() {
  lastAllocation = millis();
  // One reusable 8-bit row, never a full-screen double buffer.
  const size_t bytes = size_t(width) * rowHeight + 64;
  if (!buffered && companion_memory::visualAllowed(bytes)) {
    row.setColorDepth(8);
    buffered = row.createSprite(width, rowHeight) != nullptr;
  }
}
inline int wordGap(int textFont) { return max(4, int(tft.textWidth(" ", textFont))); }
inline int lineWidth(const String& text, bool current = true) {
  const int textFont = current ? currentFont : nextFont;
  const int gap = wordGap(textFont);
  int total = 0;
  for (unsigned i = 0; i < text.length();) {
    while (i < text.length() && text[i] == ' ') ++i;
    unsigned start = i;
    while (i < text.length() && text[i] != ' ') ++i;
    if (i > start) {
      char token[513];
      size_t length = min(size_t(i - start), sizeof(token) - 1);
      memcpy(token, text.c_str() + start, length); token[length] = 0;
      total += tft.textWidth(token, textFont) + gap;
    }
  }
  return max(0, total - gap);
}
inline int wordCenter(const String& text, int selected) {
  int cursor = 0, word = 0;
  for (unsigned i = 0; i < text.length();) {
    while (i < text.length() && text[i] == ' ') ++i;
    unsigned start = i;
    while (i < text.length() && text[i] != ' ') ++i;
    if (i == start) break;
    char token[513];
    size_t length = min(size_t(i - start), sizeof(token) - 1);
    memcpy(token, text.c_str() + start, length); token[length] = 0;
    int tw = tft.textWidth(token, currentFont);
    if (word++ == selected) return cursor + tw / 2;
    cursor += tw + wordGap(currentFont);
  }
  return 0;
}
template <class Target> void paintLine(Target& target, int x, int y, const String& text,
                                      bool current, int offset, uint8_t alpha = 255) {
  int cursor = x + 8 - offset;
  const int textFont = current ? currentFont : nextFont;
  const int textY = y + (rowHeight - tft.fontHeight(textFont)) / 2;
  target.setTextColor(blendColor(TFT_BLACK, current ? TFT_WHITE : 0x7bef, alpha / 255.0f), TFT_BLACK);
  for (unsigned i = 0; i < text.length();) {
    while (i < text.length() && text[i] == ' ') ++i;
    unsigned start = i;
    while (i < text.length() && text[i] != ' ') ++i;
    if (i == start) break;
    char token[513];
    size_t length = min(size_t(i - start), sizeof(token) - 1);
    memcpy(token, text.c_str() + start, length); token[length] = 0;
    target.drawString(token, cursor, textY, textFont);
    cursor += tft.textWidth(token, textFont) + wordGap(textFont);
  }
}
inline void paintArt() {
  const int side = artSide, x = 8, y = compact ? 20 : 26;
  tft.fillRect(x, y, side, side, TFT_BLACK);
  if (albumArtReady && albumArt) {
    // A scanline avoids a second cover copy and keeps RGB565 byte order explicit.
    uint16_t scan[artSide];
    tft.setSwapBytes(true);
    for (int dy = 0; dy < side; ++dy) {
      for (int dx = 0; dx < side; ++dx) scan[dx] = albumArt[(dy * 64 / side) * 64 + dx * 64 / side];
      tft.pushImage(x, y + dy, side, 1, scan);
    }
    tft.setSwapBytes(false);
  } else {
    tft.drawCircle(x + side / 2, y + side / 2, side / 3, 0x3186);
    tft.fillCircle(x + side / 2, y + side / 2, 3, 0x7bef);
  }
}
}

inline void releaseLyricsRenderingBuffers() { lyrics_page::release(); }
inline void redrawLyricsPage() {
  if (currentState != STATE_LYRICS) return;
  using namespace lyrics_page;
  uint32_t now = millis();
  bool fresh = generation != uiPageGeneration;
  if (!fresh && uint32_t(now - lastTick) < (transitioning ? 25U : 50U)) return;
  if (!fresh && playing && isPlaying) motionTime += min(uint32_t(now - lastTick), uint32_t(100));
  lastTick = now;
  if (fresh) {
    generation = uiPageGeneration;
    tft.fillScreen(TFT_BLACK);
    offsets[0] = offsets[1] = -9999; progress = minute = seconds = durationSeconds = -1;
    transitioning = false; outgoing = String(); paintedFade = -1;
    allocate();
  }
  bool canvasRecovered = false;
  if (!buffered && !fresh && uint32_t(now - lastAllocation) >= 1000) {
    allocate(); canvasRecovered = buffered;
  }
  if (fresh || track != currentTrack || artist != currentArtist) {
    track = currentTrack; artist = currentArtist;
    const int x = compact ? 60 : 8, y = compact ? 20 : 140;
    const int w = compact ? SCREEN_W - x - 8 : artSide;
    tft.fillRect(x, y, w, compact ? 48 : 66, TFT_BLACK);
    drawWrappedText(x, y, track.length() ? track.c_str() : "Nothing playing", w,
                    compact ? 36 : 56, TFT_WHITE, currentFont, TFT_BLACK);
    drawTruncatedText(x, y + (compact ? 38 : 58), artist.c_str(), w, 0x9cf3, 1, TFT_BLACK);
  }
  if (fresh || artRevision != albumArtRevision || artReady != albumArtReady) {
    paintArt(); artRevision = albumArtRevision; artReady = albumArtReady;
  }
  int clockMinute = timeReceived ? displayHour * 60 + displayMinute : -2;
  if (fresh || minute != clockMinute) {
    minute = clockMinute;
    char clock[8]; snprintf(clock, sizeof(clock), "%02d:%02d", displayHour % 24, displayMinute % 60);
    tft.fillRect(SCREEN_W - 40, 5, 34, 8, TFT_BLACK);
    drawTruncatedText(SCREEN_W - 40, 5, timeReceived ? clock : "--:--", 34, 0x9cf3, 1, TFT_BLACK);
  }
  String message = !wifiConnected ? "Offline" : standaloneSpotifyStatus;
  if (!message.length()) message = standaloneLyricsStatus;
  if (!message.length() && !standaloneLyricsSynced) message = "Unsynced lyrics";
  if (!message.length() && !currentTrack.length()) message = "Spotify";
  if (fresh || message != status) {
    status = message;
    tft.fillRect(8, 5, SCREEN_W - 56, 8, TFT_BLACK);
    drawTruncatedText(8, 5, message.c_str(), SCREEN_W - 56, 0x9cf3, 1, TFT_BLACK);
  }
  const String* incoming[] = {&currentLyrics, &currentLyricsLine2};
  if (!fresh && rawLines[0] != currentLyrics) {
    transitioning = buffered && isPlaying && lines[0].length() && currentLyrics.length();
    outgoing = transitioning ? lines[0] : String();
    outgoingOffset = offsets[0]; transitionAt = now;
  }
  bool contentChanged = fresh || synced != standaloneLyricsSynced || lineStart != companion_lyrics::timing().start;
  for (int i = 0; i < 2; ++i) {
    if (fresh || rawLines[i] != *incoming[i]) {
      rawLines[i] = *incoming[i];
      lines[i] = uiText(rawLines[i].c_str());
      widths[i] = lineWidth(lines[i], i == 0);
      contentChanged = true;
    }
  }
  if (contentChanged) changedAt = motionTime;
  lineStart = companion_lyrics::timing().start;
  bool changed = contentChanged || canvasRecovered;
  bool wordChanged = activeWord != companion_lyrics::timing().word || wordTimed != companion_lyrics::timing().wordTimed;
  wordTimed = companion_lyrics::timing().wordTimed;
  activeWord = companion_lyrics::timing().word; synced = standaloneLyricsSynced;
  const uint32_t fadeElapsed = uint32_t(now - transitionAt);
  const int fade = transitioning ? min(4, int(fadeElapsed / 25)) : 4;
  const bool fadeChanged = fade != paintedFade;
  paintedFade = fade;
  if (transitioning && fade >= 4) { transitioning = false; outgoing = String(); }
  for (int i = 0; i < 2; ++i) {
    int overflow = max(0, widths[i] - (width - 16));
    uint32_t phase = uint32_t(motionTime - changedAt);
    const uint32_t limit = uint32_t(overflow);
    const uint32_t cycle = phase % (2400UL + limit * 50UL);
    const uint32_t step = cycle > 1200UL ? (cycle - 1200UL) / 50UL : 0;
    int offset = overflow ? int(step < limit ? step : limit) : 0;
    if (i == 0 && overflow && wordTimed && activeWord >= 0) {
      if (changed || wordChanged || offsets[i] == -9999)
        offset = constrain(wordCenter(lines[i], activeWord) - (width - 16) / 2, 0, overflow);
      else offset = offsets[i];
    }
    // Under extreme SRAM pressure, keep stable text rather than erase/redraw flicker.
    if (!buffered && !(i == 0 && wordTimed && activeWord >= 0)) offset = 0;
    if (!isPlaying && !changed && offsets[i] != -9999) offset = offsets[i];
    if (!changed && !wordChanged && !(i == 0 && fadeChanged) && offsets[i] == offset) continue;
    offsets[i] = offset;
    // A bounded, non-blocking 100 ms fade; new lyrics appear after at most 50 ms.
    const bool fadingOld = i == 0 && transitioning && fadeElapsed < 50;
    const String& visible = fadingOld ? outgoing : lines[i];
    const int visibleOffset = fadingOld ? outgoingOffset : offset;
    const uint8_t alpha = i != 0 || !transitioning ? 255 : fadingOld ?
        uint8_t(128UL - fadeElapsed * 128UL / 50UL) : uint8_t(128UL + (fadeElapsed - 50UL) * 127UL / 50UL);
    if (buffered) {
      row.fillSprite(TFT_BLACK);
      paintLine(row, 0, 0, visible, i == 0, visibleOffset, alpha);
      row.pushSprite(left, top + i * rowHeight);
    } else {
      tft.setViewport(left, top + i * rowHeight, width, rowHeight, false);
      tft.fillRect(left, top + i * rowHeight, width, rowHeight, TFT_BLACK);
      paintLine(tft, left, top + i * rowHeight, visible, i == 0, visibleOffset, alpha);
      tft.resetViewport();
    }
  }
  if (fresh || playing != isPlaying) {
    playing = isPlaying;
    const int y = SCREEN_H - (compact ? 12 : 28);
    tft.fillRect(SCREEN_W / 2 - 42, y - 6, 84, 13, TFT_BLACK);
    drawIconPrev(SCREEN_W / 2 - 32, y, 0x9cf3);
    drawIconNext(SCREEN_W / 2 + 32, y, 0x9cf3);
    if (isPlaying) drawIconPause(SCREEN_W / 2, y, TFT_WHITE);
    else drawIconPlay(SCREEN_W / 2, y, TFT_WHITE);
  }
  int value = playDuration > 0 ? int(int64_t(SCREEN_W - 16) * constrain(playProgress, 0, playDuration) / playDuration) : 0;
  int total = max(0, playDuration / 1000);
  if (!compact && (fresh || seconds != playProgress / 1000 || durationSeconds != total)) {
    seconds = max(0, playProgress / 1000);
    durationSeconds = total;
    char elapsed[16], duration[16];
    snprintf(elapsed, sizeof(elapsed), "%d:%02d", seconds / 60, seconds % 60);
    snprintf(duration, sizeof(duration), "%d:%02d", total / 60, total % 60);
    tft.fillRect(8, SCREEN_H - 32, 56, 8, TFT_BLACK);
    tft.fillRect(SCREEN_W - 64, SCREEN_H - 32, 56, 8, TFT_BLACK);
    drawTruncatedText(8, SCREEN_H - 32, elapsed, 56, 0x9cf3, 1, TFT_BLACK);
    drawTruncatedText(SCREEN_W - 64, SCREEN_H - 32, duration, 56, 0x9cf3, 1, TFT_BLACK);
  }
  if (fresh || progress != value) {
    progress = value;
    tft.fillRect(8, SCREEN_H - 3, SCREEN_W - 16, 1, 0x3186);
    if (value) tft.drawFastHLine(8, SCREEN_H - 3, value, TFT_WHITE);
  }
}
inline void drawLyricsPage() { lyrics_page::generation = UINT32_MAX; redrawLyricsPage(); }
#endif
