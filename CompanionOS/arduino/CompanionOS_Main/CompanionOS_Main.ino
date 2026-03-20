/*
 * ═══════════════════════════════════════════════════════════
 *   COMPANION OS - Starter Version
 *   
 *   Working system with:
 *   - Emotional eyes (8 core emotions)
 *   - Touch controls
 *   - WiFi/UDP communication
 *   - Basic Spotify display
 *   
 *   Build on this to add more features!
 * ═══════════════════════════════════════════════════════════
 */

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

// ═══════════════════════════════════════════════════════════
// CONFIGURATION - EDIT THESE!
// ═══════════════════════════════════════════════════════════

// WiFi Settings
const char* WIFI_SSID = "YOUR_WIFI_NAME";        // <-- CHANGE THIS
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD"; // <-- CHANGE THIS

// Network Settings
const char* PC_IP = "192.168.1.100";  // <-- Your PC's IP address
const int UDP_PORT_RX = 8888;         // Receive on this port
const int UDP_PORT_TX = 8889;         // Send to this port

// Pin Definitions (match your wiring)
#define TOUCH_CS 15      // D8 - Touch screen chip select
#define TOUCH_LEFT 0     // D3 - Left capacitive sensor
#define TOUCH_RIGHT 2    // D4 - Right capacitive sensor
#define MIC_PIN A0       // A0 - Microphone

// ═══════════════════════════════════════════════════════════
// OBJECTS
// ═══════════════════════════════════════════════════════════

TFT_eSPI tft = TFT_eSPI();
XPT2046_Touchscreen ts(TOUCH_CS);
WiFiUDP udp;

// ═══════════════════════════════════════════════════════════
// GLOBAL VARIABLES
// ═══════════════════════════════════════════════════════════

// Display constants
#define SCREEN_W 240
#define SCREEN_H 320

// Colors
#define COLOR_BG 0x0000      // Black
#define COLOR_EYE 0x07FF     // Cyan
#define COLOR_PUPIL 0x0000   // Black  
#define COLOR_HIGHLIGHT 0xFFFF // White

// Eye positions
#define EYE_Y 160
#define LEFT_EYE_X 80
#define RIGHT_EYE_X 160
#define EYE_W 60
#define EYE_H 80

// Current state
enum Emotion {
  EMO_HAPPY,
  EMO_SAD,
  EMO_EXCITED,
  EMO_LOVE,
  EMO_SLEEPY,
  EMO_ANGRY,
  EMO_SURPRISED,
  EMO_NEUTRAL
};

Emotion currentEmotion = EMO_HAPPY;
unsigned long lastBlink = 0;
bool isBlinking = false;
int blinkPhase = 0;

// Network buffer
char udpBuffer[512];

// ═══════════════════════════════════════════════════════════
// SETUP
// ═══════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  Serial.println(F("\n\n╔════════════════════════════════════════╗"));
  Serial.println(F("║   COMPANION OS - Starter v1.0          ║"));
  Serial.println(F("╚════════════════════════════════════════╝\n"));
  
  // Initialize display
  Serial.print(F("Display... "));
  tft.init();
  tft.setRotation(0);  // Portrait
  tft.fillScreen(COLOR_BG);
  Serial.println(F("OK"));
  
  // Initialize touch
  Serial.print(F("Touch... "));
  ts.begin();
  ts.setRotation(0);
  Serial.println(F("OK"));
  
  // Initialize sensors
  pinMode(TOUCH_LEFT, INPUT);
  pinMode(TOUCH_RIGHT, INPUT);
  pinMode(MIC_PIN, INPUT);
  Serial.println(F("Sensors... OK"));
  
  // Connect WiFi
  Serial.print(F("WiFi... "));
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(F("."));
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F(" OK"));
    Serial.print(F("IP: "));
    Serial.println(WiFi.localIP());
    udp.begin(UDP_PORT_RX);
  } else {
    Serial.println(F(" FAILED"));
  }
  
  // Draw initial eyes
  drawEyes();
  
  Serial.println(F("\n✓ Ready!\n"));
}

// ═══════════════════════════════════════════════════════════
// MAIN LOOP
// ═══════════════════════════════════════════════════════════

void loop() {
  handleNetwork();
  handleTouch();
  updateEyes();
  delay(50);  // ~20 FPS
}

// ═══════════════════════════════════════════════════════════
// NETWORK FUNCTIONS
// ═══════════════════════════════════════════════════════════

void handleNetwork() {
  int packetSize = udp.parsePacket();
  if (packetSize) {
    int len = udp.read(udpBuffer, sizeof(udpBuffer) - 1);
    udpBuffer[len] = 0;
    String msg = String(udpBuffer);
    
    // Parse commands
    if (msg.startsWith("EMOTION:")) {
      String emo = msg.substring(8);
      if (emo == "HAPPY") setEmotion(EMO_HAPPY);
      else if (emo == "SAD") setEmotion(EMO_SAD);
      else if (emo == "EXCITED") setEmotion(EMO_EXCITED);
      else if (emo == "LOVE") setEmotion(EMO_LOVE);
      else if (emo == "SLEEPY") setEmotion(EMO_SLEEPY);
      else if (emo == "ANGRY") setEmotion(EMO_ANGRY);
      else if (emo == "SURPRISED") setEmotion(EMO_SURPRISED);
    }
    else if (msg.startsWith("TRACK:")) {
      // Display track info (basic version)
      displayTrack(msg.substring(6));
    }
    
    Serial.print(F("Received: "));
    Serial.println(msg);
  }
}

void sendCommand(String cmd) {
  udp.beginPacket(PC_IP, UDP_PORT_TX);
  udp.write(cmd.c_str());
  udp.endPacket();
  Serial.print(F("Sent: "));
  Serial.println(cmd);
}

// ═══════════════════════════════════════════════════════════
// TOUCH FUNCTIONS
// ═══════════════════════════════════════════════════════════

void handleTouch() {
  static unsigned long lastTouch = 0;
  
  // Physical touch sensors
  if (millis() - lastTouch > 500) {  // Debounce
    if (digitalRead(TOUCH_LEFT) == HIGH) {
      sendCommand("PREV");
      setEmotion(EMO_EXCITED);
      lastTouch = millis();
    }
    if (digitalRead(TOUCH_RIGHT) == HIGH) {
      sendCommand("NEXT");
      setEmotion(EMO_HAPPY);
      lastTouch = millis();
    }
  }
  
  // Screen touch
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    int x = map(p.x, 200, 3800, 0, SCREEN_W);
    int y = map(p.y, 200, 3800, 0, SCREEN_H);
    
    // Simple button at bottom
    if (y > 280 && y < 310) {
      if (x < 120) {
        sendCommand("PLAY_PAUSE");
      } else {
        setEmotion((Emotion)((currentEmotion + 1) % 8));
      }
    }
  }
}

// ═══════════════════════════════════════════════════════════
// EYE RENDERING
// ═══════════════════════════════════════════════════════════

void drawEyes() {
  tft.fillScreen(COLOR_BG);
  
  // Draw based on current emotion
  switch (currentEmotion) {
    case EMO_HAPPY:
      drawHappyEyes();
      break;
    case EMO_SAD:
      drawSadEyes();
      break;
    case EMO_EXCITED:
      drawExcitedEyes();
      break;
    case EMO_LOVE:
      drawLoveEyes();
      break;
    case EMO_SLEEPY:
      drawSleepyEyes();
      break;
    case EMO_ANGRY:
      drawAngryEyes();
      break;
    case EMO_SURPRISED:
      drawSurprisedEyes();
      break;
    default:
      drawNeutralEyes();
  }
  
  // Draw control buttons
  tft.fillRoundRect(10, 285, 100, 30, 5, TFT_BLUE);
  tft.setTextColor(TFT_WHITE);
  tft.drawCentreString("Play", 60, 292, 2);
  
  tft.fillRoundRect(130, 285, 100, 30, 5, TFT_GREEN);
  tft.drawCentreString("Emotion", 180, 292, 2);
}

void drawHappyEyes() {
  // Left eye - curved smile shape
  tft.fillEllipse(LEFT_EYE_X, EYE_Y, EYE_W/2, EYE_H/2, COLOR_EYE);
  tft.fillEllipse(LEFT_EYE_X, EYE_Y - 10, EYE_W/3, EYE_H/3, COLOR_PUPIL);
  tft.fillCircle(LEFT_EYE_X + 5, EYE_Y - 15, 3, COLOR_HIGHLIGHT);
  
  // Right eye
  tft.fillEllipse(RIGHT_EYE_X, EYE_Y, EYE_W/2, EYE_H/2, COLOR_EYE);
  tft.fillEllipse(RIGHT_EYE_X, EYE_Y - 10, EYE_W/3, EYE_H/3, COLOR_PUPIL);
  tft.fillCircle(RIGHT_EYE_X + 5, EYE_Y - 15, 3, COLOR_HIGHLIGHT);
  
  // Label
  tft.setTextColor(COLOR_EYE);
  tft.drawCentreString("Happy", SCREEN_W/2, 50, 4);
}

void drawSadEyes() {
  // Droopy eyes
  tft.fillEllipse(LEFT_EYE_X, EYE_Y + 10, EYE_W/2, EYE_H/2, COLOR_EYE);
  tft.fillEllipse(LEFT_EYE_X, EYE_Y + 15, EYE_W/3, EYE_H/3, COLOR_PUPIL);
  
  tft.fillEllipse(RIGHT_EYE_X, EYE_Y + 10, EYE_W/2, EYE_H/2, COLOR_EYE);
  tft.fillEllipse(RIGHT_EYE_X, EYE_Y + 15, EYE_W/3, EYE_H/3, COLOR_PUPIL);
  
  // Tears
  tft.fillCircle(LEFT_EYE_X, EYE_Y + 50, 4, COLOR_EYE);
  tft.fillCircle(RIGHT_EYE_X, EYE_Y + 50, 4, COLOR_EYE);
  
  tft.setTextColor(COLOR_EYE);
  tft.drawCentreString("Sad", SCREEN_W/2, 50, 4);
}

void drawExcitedEyes() {
  // Wide open eyes
  tft.fillEllipse(LEFT_EYE_X, EYE_Y, EYE_W/2 + 10, EYE_H/2 + 10, COLOR_EYE);
  tft.fillEllipse(LEFT_EYE_X, EYE_Y, EYE_W/3 + 5, EYE_H/3 + 5, COLOR_PUPIL);
  
  tft.fillEllipse(RIGHT_EYE_X, EYE_Y, EYE_W/2 + 10, EYE_H/2 + 10, COLOR_EYE);
  tft.fillEllipse(RIGHT_EYE_X, EYE_Y, EYE_W/3 + 5, EYE_H/3 + 5, COLOR_PUPIL);
  
  // Sparkles
  for (int i = 0; i < 5; i++) {
    int x = random(20, SCREEN_W - 20);
    int y = random(20, EYE_Y - 40);
    tft.fillStar(x, y, 3, COLOR_EYE);
  }
  
  tft.setTextColor(COLOR_EYE);
  tft.drawCentreString("Excited!", SCREEN_W/2, 50, 4);
}

void drawLoveEyes() {
  // Heart-shaped eyes
  tft.fillHeart(LEFT_EYE_X, EYE_Y, 25, TFT_PINK);
  tft.fillHeart(RIGHT_EYE_X, EYE_Y, 25, TFT_PINK);
  
  tft.setTextColor(TFT_PINK);
  tft.drawCentreString("Love", SCREEN_W/2, 50, 4);
}

void drawSleepyEyes() {
  // Half-closed horizontal lines
  tft.fillRect(LEFT_EYE_X - EYE_W/2, EYE_Y, EYE_W, 10, COLOR_EYE);
  tft.fillRect(RIGHT_EYE_X - EYE_W/2, EYE_Y, EYE_W, 10, COLOR_EYE);
  
  // Z's for sleeping
  tft.setTextColor(COLOR_EYE);
  tft.drawString("Z", LEFT_EYE_X + 40, EYE_Y - 40, 4);
  tft.drawString("Z", LEFT_EYE_X + 50, EYE_Y - 60, 2);
  
  tft.drawCentreString("Sleepy", SCREEN_W/2, 50, 4);
}

void drawAngryEyes() {
  // Angled eyes
  tft.fillTriangle(
    LEFT_EYE_X - EYE_W/2, EYE_Y - 20,
    LEFT_EYE_X + EYE_W/2, EYE_Y,
    LEFT_EYE_X - EYE_W/2, EYE_Y + 20,
    TFT_RED
  );
  
  tft.fillTriangle(
    RIGHT_EYE_X + EYE_W/2, EYE_Y - 20,
    RIGHT_EYE_X - EYE_W/2, EYE_Y,
    RIGHT_EYE_X + EYE_W/2, EYE_Y + 20,
    TFT_RED
  );
  
  tft.setTextColor(TFT_RED);
  tft.drawCentreString("Angry!", SCREEN_W/2, 50, 4);
}

void drawSurprisedEyes() {
  // Very wide circular eyes
  tft.fillCircle(LEFT_EYE_X, EYE_Y, EYE_W/2 + 5, COLOR_EYE);
  tft.fillCircle(LEFT_EYE_X, EYE_Y, 8, COLOR_PUPIL);
  
  tft.fillCircle(RIGHT_EYE_X, EYE_Y, EYE_W/2 + 5, COLOR_EYE);
  tft.fillCircle(RIGHT_EYE_X, EYE_Y, 8, COLOR_PUPIL);
  
  tft.setTextColor(COLOR_EYE);
  tft.drawCentreString("Surprised!", SCREEN_W/2, 50, 4);
}

void drawNeutralEyes() {
  // Standard eyes
  tft.fillEllipse(LEFT_EYE_X, EYE_Y, EYE_W/2, EYE_H/2, COLOR_EYE);
  tft.fillEllipse(LEFT_EYE_X, EYE_Y, EYE_W/3, EYE_H/3, COLOR_PUPIL);
  
  tft.fillEllipse(RIGHT_EYE_X, EYE_Y, EYE_W/2, EYE_H/2, COLOR_EYE);
  tft.fillEllipse(RIGHT_EYE_X, EYE_Y, EYE_W/3, EYE_H/3, COLOR_PUPIL);
  
  tft.setTextColor(COLOR_EYE);
  tft.drawCentreString("Neutral", SCREEN_W/2, 50, 4);
}

void setEmotion(Emotion newEmotion) {
  if (newEmotion != currentEmotion) {
    currentEmotion = newEmotion;
    drawEyes();
  }
}

void updateEyes() {
  // Blinking animation
  unsigned long now = millis();
  if (now - lastBlink > 3000) {  // Blink every 3 seconds
    if (!isBlinking) {
      isBlinking = true;
      blinkPhase = 0;
      lastBlink = now;
    }
  }
  
  if (isBlinking) {
    blinkPhase++;
    if (blinkPhase == 1) {
      // Close eyes
      tft.fillRect(LEFT_EYE_X - EYE_W/2, EYE_Y - EYE_H/2, EYE_W, EYE_H, COLOR_BG);
      tft.fillRect(RIGHT_EYE_X - EYE_W/2, EYE_Y - EYE_H/2, EYE_W, EYE_H, COLOR_BG);
      tft.fillRect(LEFT_EYE_X - EYE_W/2, EYE_Y, EYE_W, 3, COLOR_EYE);
      tft.fillRect(RIGHT_EYE_X - EYE_W/2, EYE_Y, EYE_W, 3, COLOR_EYE);
    } else if (blinkPhase == 3) {
      // Reopen eyes
      drawEyes();
      isBlinking = false;
    }
  }
}

// ═══════════════════════════════════════════════════════════
// SPOTIFY DISPLAY (BASIC)
// ═══════════════════════════════════════════════════════════

void displayTrack(String info) {
  // Simple track info display at bottom
  tft.fillRect(0, 250, SCREEN_W, 30, COLOR_BG);
  tft.setTextColor(TFT_GREEN);
  tft.drawCentreString(info, SCREEN_W/2, 255, 2);
}

// ═══════════════════════════════════════════════════════════
// HELPER FUNCTIONS
// ═══════════════════════════════════════════════════════════

// Draw a simple star (for excited emotion)
void TFT_eSPI::fillStar(int x, int y, int r, uint16_t color) {
  for (int i = 0; i < 5; i++) {
    int angle = i * 144;
    int x1 = x + r * cos(angle * PI / 180);
    int y1 = y + r * sin(angle * PI / 180);
    drawLine(x, y, x1, y1, color);
  }
}

// Draw a simple heart
void TFT_eSPI::fillHeart(int x, int y, int size, uint16_t color) {
  fillCircle(x - size/2, y, size/2, color);
  fillCircle(x + size/2, y, size/2, color);
  fillTriangle(
    x - size, y,
    x + size, y,
    x, y + size,
    color
  );
}
