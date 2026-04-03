# CompanionOS v4.0 — Resource Usage & Optimization Guide

This document provides a technical breakdown of how **CompanionOS** utilizes the ESP8266 hardware resources and outlines configurations to maximize performance.

---

## 📊 Resource Usage Breakdown

### 1. RAM (Static & Dynamic Memory)
The ESP8266 has **80 KB** of internal RAM. However, the WiFi stack and system overhead typically leave only **~35–45 KB** for user applications. CompanionOS is designed to push these limits:

| Component | RAM Usage | Detail |
| :--- | :--- | :--- |
| **Album Art Buffer** | 18.4 KB | Fixed `uint16_t[9216]` for 96x96 pixels (Shared with Gallery). |
| **Eye Animation Sprite** | ~22.0 KB | `TFT_eSprite` (~110x100 pixels) for tear-free rendering. |
| **Sleep (Zzz) Sprite** | 4.8 KB | `40x60` sprite for sleep animations. |
| **UDP RX Buffer** | 0.5 KB | Incoming packet buffer. |
| **JSON Work Area** | 1.0 KB | `DynamicJsonDocument` used for command parsing. |
| **WiFi & System** | ~32.0 KB | WiFi stack, TCP/UDP overhead, and core system. |
| **Total Estimated** | **~78.7 KB** | **CRITICAL:** Close to physical limit. |

> [!WARNING]
> **Fragmentation Risk**: Extensive use of `String` objects for lyrics and weather data can lead to heap fragmentation. The system may reset if RAM becomes too fragmented to allocate a new sprite.

### 2. CPU (Processing Power)
The ESP8266 runs at **80 MHz** by default but can be overclocked to **160 MHz**.

*   **Standard Usage**: WiFi handling and basic UI updates run smoothly at 80 MHz.
*   **Exotic Mode**: Features like Sin-wave Auroras, Particle Systems, and Kinetic Typography require **160 MHz** to maintain ~20 FPS.
*   **Math Intensity**: The "Spring-Physics" pupil logic and "Midpoint-Circle" ring renders use floating-point math, which is expensive on the ESP8266 (no dedicated FPU).

### 3. Flash Storage (Program Memory)
*   **Binary Size**: Approximately **500–650 KB** depending on enabled features.
*   **Libraries**: `TFT_eSPI`, `WiFiManager`, and `ArduinoJson` contribute significantly to the binary footprint.
*   **Storage**: 1MB Flash is enough, but 4MB is recommended for future OTA (Over-The-Air) updates.

---

## ⚡ Performance Optimizations

### 🚀 Hardware Settings & Productivity
To increase the "Productivity" (responsiveness) of CompanionOS, ensure the following in your Arduino IDE / PlatformIO settings:

1.  **CPU Frequency**: Set to **160 MHz**. This is essential for the "Exotic" visual effects and smooth eye movement.
2.  **Flash Frequency**: Set to **80 MHz** (if your module supports it) for faster UI asset loading.
3.  **Flash Mode**: Use **QIO** (Quad I/O) instead of DIO for a ~2x speedup in code execution from Flash.
4.  **LwIP Variant**: Use **v2 Higher Bandwidth** to improve UDP packet handling for Spotify/Art syncing.

### 🛠️ Code-Level Optimizations
CompanionOS already includes several advanced optimizations:
*   **Binary Packet Sniffing**: Uses `0xFE` prefix to send raw RGB565 data for album art, bypassing expensive Base64 decoding or String manipulation.
*   **Sprite Persistence**: Sprites are only reallocated if sizes change, reducing heap churn (`eyeSprAllocated` check).
*   **Shared Buffers**: The `galleryImage` pointer aliases the `albumArt` buffer, saving **18.4 KB** of RAM.
*   **Vector Rendering**: Icons and rings are drawn mathematically (e.g., `drawSmoothRing`) rather than using bitmaps, saving Flash.

---

## 🔍 Future Optimization Ideas

1.  **PSRAM Support**: If moving to an ESP32, PSRAM would allow for full-screen buffering (153 KB) for perfect transitions.
2.  **LittleFS for Assets**: Store heavy animations/icons in Flash (SPIFFS/LittleFS) to free up RAM.
3.  **DMA (Direct Memory Access)**: Use SPI DMA for the ST7789 display to offload CPU during screen updates (Note: ESP8266 DMA is limited/complex).
4.  **StaticJsonDocument**: Replace `DynamicJsonDocument` with static allocation where possible to prevent heap collisions.

---

## 🖼️ Faster Album Art Loading: Original vs Proposed

To achieve near-instant album art synchronization, we can optimize the binary packet processing and display communication.

### 📉 Current (Original) Logic
**File:** `network.h` (Lines 156–166)
The current loop flips endianness manually and draws small (2-row) chunks one by one.

```cpp
// ORIGINAL: Manual unpacking and small blocking writes
for (int i = 0; i < pixelsInChunk; i++) {
    int offset = 3 + (i * 2);
    // (L, H) -> RGB565 conversion happens on-device
    dest[i] = (unsigned char)udpBuffer[offset] | ((unsigned char)udpBuffer[offset+1] << 8); 
}

// Draw exactly 2 rows per packet
tft.pushImage(10, 25 + yStart, 96, maxRows, dest);
```

### 📈 Proposed (Enhanced) Logic
By offloading the byte-flipping to the PC (Python side) and increasing the chunk size, we reduce CPU cycles and packet overhead by ~70%.

```cpp
// PROPOSED: Bulk memory copy + DMA Transfer
// (Note: Requires Python to send Big-Endian bytes and increasing MTU)

// 1. Use memcpy if the Python script pre-flips bytes for the ESP
memcpy(dest, (unsigned char*)udpBuffer + 3, len - 3);

// 2. Increase packet size to ~600 pixels (1200 bytes) 
// reduces the number of tft.pushImage calls from 48 to ~15.

// 3. Hardware-accelerated SPI DMA (Optional but recommended)
// Allows the CPU to process the NEXT packet while the display is still drawing
tft.pushImageDMA(10, 25 + yStart, 96, maxRows, dest);
```

### ✨ Summary of Improvements
| Improvement | Rationale | Performance Gain |
| :--- | :--- | :--- |
| **Pre-flipped Bytes** | Moving the byte-swap loop from 80MHz ESP to 4GHz PC. | ~15ms saved per packet |
| **MTU Saturation** | Sending 1200 bytes/packet instead of 384. | ~60% reduction in overhead |
| **SPI Clock Increase** | Set `SPI_FREQUENCY=40000000` or `80000000` in `User_Setup.h`. | ~2x faster screen draw |
| **DMA Transfers** | Asynchronous data transfer (requires `ST7789_DRIVER`). | Zero CPU-blocking for display |

---
