/**
 * ESP8266 Video Stream Receiver — Serial 2Mbps + ILI9341 TFT
 * ============================================================
 * Receives JPEG frames over USB Serial at 2,000,000 baud,
 * decodes with TJpgDec, renders to ILI9341 via SPI.
 *
 * Required Libraries (Arduino Library Manager):
 *   - TFT_eSPI  (Bodmer)
 *   - TJpg_Decoder (bundled with TFT_eSPI)
 *
 * Board  : NodeMCU 1.0 / Wemos D1 Mini / Generic ESP8266
 * CPU    : Tools → CPU Frequency → 160 MHz  (required for 20+ fps)
 *
 * Pin mapping (boot-safe):
 *   TFT SCK  → D5 / GPIO14
 *   TFT MOSI → D7 / GPIO13
 *   TFT MISO → D6 / GPIO12
 *   TFT CS   → D2 / GPIO4
 *   TFT DC   → D1 / GPIO5   ← safe (not a boot pin)
 *   TFT RST  → D0 / GPIO16  ← safe (not a boot pin)
 *   TFT LED  → 3.3V
 *
 * Update User_Setup.h:
 *   #define TFT_DC  5
 *   #define TFT_RST 16
 *
 * Frame wire format (sent by pc_sender.py):
 *   [0]     0xFF  ┐
 *   [1]     0xAA  │ 4-byte start marker
 *   [2]     0xFF  │
 *   [3]     0xAA  ┘
 *   [4-7]   uint32 little-endian — JPEG byte count
 *   [8]     uint8  — XOR checksum of bytes 4-7
 *   [9 …]   JPEG payload (variable length)
 */

#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>

// ─── Baud rate ─────────────────────────────────────────────────────────────────
#define SERIAL_BAUD 2000000
// ───────────────────────────────────────────────────────────────────────────────

// ─── Frame buffer ──────────────────────────────────────────────────────────────
// 12 KB fits a quality-35 JPEG at 240×320 with headroom.
// Raise to 16384 if you use quality > 50.
#define MAX_JPEG_SIZE 12288
static uint8_t jpegBuf[MAX_JPEG_SIZE];
// ───────────────────────────────────────────────────────────────────────────────

// ─── Start marker ──────────────────────────────────────────────────────────────
static const uint8_t MARKER[4] = { 0xFF, 0xAA, 0xFF, 0xAA };
// ───────────────────────────────────────────────────────────────────────────────

TFT_eSPI tft = TFT_eSPI();

// FPS tracking
uint32_t frameCount   = 0;
uint32_t fpsTimer     = 0;
uint8_t  displayedFps = 0;


// ─── TJpgDec callback ─────────────────────────────────────────────────────────
bool tftJpegOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    if (y >= tft.height()) return false;
    tft.pushImage(x, y, w, h, bitmap);
    return true;
}


// ─── Read exactly N bytes from Serial with timeout ────────────────────────────
// Returns true if all bytes arrived within the timeout window.
bool serialReadBytes(uint8_t* buf, uint32_t len, uint32_t timeoutMs = 500) {
    uint32_t remaining = len;
    uint32_t deadline  = millis() + timeoutMs;
    uint8_t* ptr       = buf;

    while (remaining > 0) {
        int avail = Serial.available();
        if (avail > 0) {
            uint32_t toRead = min((uint32_t)avail, remaining);
            Serial.readBytes(ptr, toRead);
            ptr       += toRead;
            remaining -= toRead;
        } else {
            if (millis() > deadline) return false;  // timed out
            yield();  // feed watchdog while waiting
        }
    }
    return true;
}


// ─── Scan incoming serial stream for the 4-byte start marker ─────────────────
// Reads one byte at a time and slides a 4-byte window until it matches.
// Returns false if nothing arrives within timeoutMs.
bool waitForMarker(uint32_t timeoutMs = 2000) {
    uint8_t  window[4] = { 0, 0, 0, 0 };
    uint32_t deadline  = millis() + timeoutMs;

    while (millis() < deadline) {
        if (Serial.available() > 0) {
            // Shift window left, read one new byte at the end
            window[0] = window[1];
            window[1] = window[2];
            window[2] = window[3];
            window[3] = (uint8_t)Serial.read();

            if (window[0] == MARKER[0] &&
                window[1] == MARKER[1] &&
                window[2] == MARKER[2] &&
                window[3] == MARKER[3]) {
                return true;
            }
        } else {
            yield();
        }
    }
    return false;
}


// ─── Render one JPEG frame ────────────────────────────────────────────────────
void renderFrame(uint32_t jpegLen) {
    uint32_t t0  = millis();
    JRESULT  res = TJpgDec.drawJpg(0, 0, jpegBuf, jpegLen);
    uint32_t ms  = millis() - t0;

    if (res != JDR_OK) {
        // Don't print here — Serial is our data channel, not debug channel.
        // Bad frames are silently dropped; the next frame overwrites the display.
        return;
    }

    frameCount++;

    uint32_t now = millis();
    if (now - fpsTimer >= 1000) {
        displayedFps = frameCount;
        frameCount   = 0;
        fpsTimer     = now;

        // FPS overlay — top-left corner, 1 draw call
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(String(displayedFps) + "fps", 2, 2, 1);
    }
}


// ─── Setup ────────────────────────────────────────────────────────────────────
void setup() {
    // IMPORTANT: Serial is the data channel here, not a debug console.
    // Do NOT add Serial.print() calls — they corrupt the frame stream.
    Serial.begin(SERIAL_BAUD);
    Serial.setTimeout(500);

    // TFT init
    tft.init();
    tft.setRotation(0);          // Portrait. Use 1 for landscape (swap W/H in sender too)
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Waiting for", tft.width() / 2, tft.height() / 2 - 16, 2);
    tft.drawString("video stream...", tft.width() / 2, tft.height() / 2 + 16, 2);

    // TJpgDec init
    TJpgDec.setJpgScale(1);
    TJpgDec.setSwapBytes(true);
    TJpgDec.setCallback(tftJpegOutput);

    fpsTimer = millis();
}


// ─── Loop ─────────────────────────────────────────────────────────────────────
void loop() {
    // 1. Wait for the 4-byte start marker
    if (!waitForMarker(2000)) {
        yield();
        return;  // Nothing coming — keep waiting
    }

    // 2. Read 5-byte header: 4 bytes length + 1 byte checksum
    uint8_t header[5];
    if (!serialReadBytes(header, 5, 200)) return;  // timeout → discard

    // 3. Verify checksum (XOR of the 4 length bytes)
    uint8_t xorCheck = header[0] ^ header[1] ^ header[2] ^ header[3];
    if (xorCheck != header[4]) return;  // corrupted header → discard

    // 4. Decode length (little-endian uint32)
    uint32_t jpegLen = (uint32_t)header[0]
                     | ((uint32_t)header[1] << 8)
                     | ((uint32_t)header[2] << 16)
                     | ((uint32_t)header[3] << 24);

    if (jpegLen == 0 || jpegLen > MAX_JPEG_SIZE) return;  // sanity check

    // 5. Read JPEG payload
    // Timeout scales with frame size — at 2Mbps, 12KB takes ~50ms
    uint32_t timeoutMs = 100 + (jpegLen / 200);
    if (!serialReadBytes(jpegBuf, jpegLen, timeoutMs)) return;  // incomplete → discard

    // 6. Decode and render
    renderFrame(jpegLen);

    yield();  // Feed watchdog
}
