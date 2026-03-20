/**
 * ESP8266 Video Stream Receiver — WiFi UDP + ILI9341 TFT
 * =======================================================
 * Receives JPEG frames over WiFi UDP, decodes with TJpgDec,
 * renders to ILI9341 TFT via SPI.
 *
 * Required Libraries (Arduino Library Manager):
 *   - TFT_eSPI  (Bodmer)
 *   - TJpg_Decoder (bundled with TFT_eSPI)
 *
 * Board  : Tools → Board → NodeMCU 1.0 (ESP-12E Module)
 *          ^^^ THIS IS CRITICAL — Generic ESP8266 breaks WiFi init
 * CPU    : Tools → CPU Frequency → 160 MHz  (mandatory for 20+ fps)
 *
 * Pin mapping (boot-safe — no boot pins used):
 *   TFT SCK  → D5 / GPIO14
 *   TFT MOSI → D7 / GPIO13
 *   TFT MISO → D6 / GPIO12
 *   TFT CS   → D2 / GPIO4
 *   TFT DC   → D1 / GPIO5   ← safe, not a boot pin
 *   TFT RST  → D0 / GPIO16  ← safe, not a boot pin
 *   TFT LED  → 3.3V
 *
 * User_Setup.h must have:
 *   #define TFT_DC  5
 *   #define TFT_RST 16
 *   #define SPI_FREQUENCY 40000000
 *
 * UDP packet format (sent by pc_sender.py):
 *   [0-1]  uint16 BE  — frame_id
 *   [2]    uint8      — chunk_id (0-indexed)
 *   [3]    uint8      — num_chunks (total for this frame)
 *   [4-7]  uint32 BE  — total JPEG bytes in this frame
 *   [8]    uint8      — XOR checksum of bytes 0-7
 *   [9+]   JPEG payload chunk
 */

#include <ESP8266WiFi.h>
#include <WiFiUDP.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>

// ─── WiFi credentials ──────────────────────────────────────────────────────────
const char* WIFI_SSID     = "YOUR_WIFI_SSID";      // ← change this
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";  // ← change this
// ───────────────────────────────────────────────────────────────────────────────

// ─── UDP config ────────────────────────────────────────────────────────────────
const uint16_t UDP_PORT    = 5005;
const uint16_t CHUNK_SIZE  = 1024;
const uint8_t  HEADER_SIZE = 9;
// ───────────────────────────────────────────────────────────────────────────────

// ─── Frame buffer ──────────────────────────────────────────────────────────────
// 12 KB handles quality-35 JPEG at 240×320 comfortably.
// Raise to 16384 if you increase JPEG quality above 50.
#define MAX_JPEG_SIZE 12288
static uint8_t  frameBuffer[MAX_JPEG_SIZE];
static uint32_t frameLen       = 0;
static uint32_t bytesReceived  = 0;
static uint8_t  chunksExpected = 0;
static uint8_t  chunksReceived = 0;
static uint16_t currentFrameId = 0xFFFF;
// ───────────────────────────────────────────────────────────────────────────────

// ─── UDP receive buffer ────────────────────────────────────────────────────────
#define UDP_BUF_SIZE (CHUNK_SIZE + HEADER_SIZE + 8)
static uint8_t udpBuf[UDP_BUF_SIZE];
// ───────────────────────────────────────────────────────────────────────────────

TFT_eSPI tft = TFT_eSPI();
WiFiUDP  udp;

uint32_t frameCount   = 0;
uint32_t fpsTimer     = 0;
uint8_t  displayedFps = 0;


// ─── TJpgDec callback ─────────────────────────────────────────────────────────
bool tftJpegOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    if (y >= tft.height()) return false;
    tft.pushImage(x, y, w, h, bitmap);
    return true;
}


// ─── Header parser ────────────────────────────────────────────────────────────
bool parseHeader(const uint8_t* buf,
                 uint16_t& frameId, uint8_t& chunkId,
                 uint8_t& numChunks, uint32_t& totalLen) {
    uint8_t xorCheck = 0;
    for (int i = 0; i < 8; i++) xorCheck ^= buf[i];
    if (xorCheck != buf[8]) return false;

    frameId   = ((uint16_t)buf[0] << 8) | buf[1];
    chunkId   =  buf[2];
    numChunks =  buf[3];
    totalLen  = ((uint32_t)buf[4] << 24) |
                ((uint32_t)buf[5] << 16) |
                ((uint32_t)buf[6] <<  8) |
                 (uint32_t)buf[7];
    return true;
}


// ─── Frame renderer ───────────────────────────────────────────────────────────
void renderFrame() {
    TJpgDec.setSwapBytes(true);

    uint32_t t0  = millis();
    JRESULT  res = TJpgDec.drawJpg(0, 0, frameBuffer, bytesReceived);
    uint32_t ms  = millis() - t0;

    if (res != JDR_OK) {
        Serial.printf("[WARN] JPEG decode error %d\n", res);
        return;
    }

    frameCount++;

    uint32_t now = millis();
    if (now - fpsTimer >= 1000) {
        displayedFps = frameCount;
        frameCount   = 0;
        fpsTimer     = now;

        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(String(displayedFps) + "fps", 2, 2, 1);

        Serial.printf("[INFO] FPS: %d  |  Decode: %u ms  |  JPEG: %u B\n",
                      displayedFps, ms, bytesReceived);
    }
}


// ─── UDP packet processor ─────────────────────────────────────────────────────
void processPacket(const uint8_t* packet, int packetSize) {
    if (packetSize <= HEADER_SIZE) return;

    uint16_t frameId;
    uint8_t  chunkId, numChunks;
    uint32_t totalLen;

    if (!parseHeader(packet, frameId, chunkId, numChunks, totalLen)) return;

    int            dataLen = packetSize - HEADER_SIZE;
    const uint8_t* data    = packet + HEADER_SIZE;

    if (totalLen > MAX_JPEG_SIZE || totalLen == 0) return;
    if (numChunks == 0) return;

    // New frame — reset accumulator
    if (frameId != currentFrameId) {
        currentFrameId = frameId;
        bytesReceived  = 0;
        chunksReceived = 0;
        chunksExpected = numChunks;
        frameLen       = totalLen;
    }

    uint32_t writeOffset = (uint32_t)chunkId * CHUNK_SIZE;
    if (writeOffset + dataLen > MAX_JPEG_SIZE) return;

    memcpy(frameBuffer + writeOffset, data, dataLen);
    bytesReceived += dataLen;
    chunksReceived++;

    if (chunksReceived >= chunksExpected && bytesReceived >= frameLen) {
        renderFrame();
        bytesReceived  = 0;
        chunksReceived = 0;
    }
}


// ─── WiFi connect ─────────────────────────────────────────────────────────────
void connectWiFi() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Connecting to WiFi...", tft.width() / 2, tft.height() / 2, 2);

    // persistent(false) prevents flash wear and config corruption
    WiFi.persistent(false);
    WiFi.disconnect(true);
    delay(200);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    WiFi.setSleepMode(WIFI_NONE_SLEEP);  // Disable modem sleep → lowest latency

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        yield();
        delay(300);
        Serial.print(".");

        if (millis() - start > 20000) {
            Serial.println("\n[ERROR] WiFi timeout. Restarting...");
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.drawString("WiFi timeout!", tft.width() / 2, tft.height() / 2, 2);
            delay(2000);
            ESP.restart();
        }
    }

    Serial.printf("\n[INFO] Connected! IP: %s\n", WiFi.localIP().toString().c_str());

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("Connected!", tft.width() / 2, tft.height() / 2 - 40, 2);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString(WiFi.localIP().toString(), tft.width() / 2, tft.height() / 2, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Waiting for stream...", tft.width() / 2, tft.height() / 2 + 40, 2);
    delay(1200);
}


// ─── Setup ────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("\n[INFO] ESP8266 Video Stream Receiver booting...");

    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Booting...", tft.width() / 2, tft.height() / 2, 2);

    TJpgDec.setJpgScale(1);
    TJpgDec.setSwapBytes(true);
    TJpgDec.setCallback(tftJpegOutput);

    connectWiFi();

    udp.begin(UDP_PORT);
    Serial.printf("[INFO] UDP listening on port %d\n", UDP_PORT);

    fpsTimer = millis();
}


// ─── Loop ─────────────────────────────────────────────────────────────────────
void loop() {
    // Drain all pending UDP packets before rendering
    int packetSize;
    while ((packetSize = udp.parsePacket()) > 0) {
        if (packetSize <= (int)UDP_BUF_SIZE) {
            int n = udp.read(udpBuf, UDP_BUF_SIZE);
            if (n > 0) processPacket(udpBuf, n);
        } else {
            udp.flush();
        }
    }

    // Feed the watchdog and let the WiFi stack run its background tasks
    yield();
}
