// ═══════════════════════════════════════════════════════════
// COMPANION OS v7.0 — DR. HACK: WEB DASHBOARD
// Creates AP + web interface for remote tool control
// Adapted from ESP32-TOOLS-PRO for 160×128 ST7735R
// ═══════════════════════════════════════════════════════════
#ifndef DH_WEB_DASHBOARD_H
#define DH_WEB_DASHBOARD_H

#ifdef ESP32

#include "globals.h"
#include "ui_components.h"
#include <WiFi.h>
#include <WebServer.h>

// ═══════════════════════════════════════════════════════════
// HTML DASHBOARD PAGE
// ═══════════════════════════════════════════════════════════

static const char DH_DASH_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>CompanionOS Dashboard</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:'Segoe UI',system-ui,sans-serif;background:#0a0a1a;color:#e0e0ff;
min-height:100vh}
.header{background:linear-gradient(135deg,#1a1a3e,#0a0a2a);padding:20px;
border-bottom:1px solid #333}
h1{color:#05ff;font-size:20px}
.sub{color:#888;font-size:12px;margin-top:4px}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(200px,1fr));
gap:16px;padding:20px}
.card{background:linear-gradient(135deg,#1a1a3e,#12122a);border:1px solid #333;
border-radius:12px;padding:16px;transition:all 0.2s}
.card:hover{border-color:#05ff;transform:translateY(-2px);
box-shadow:0 4px 20px rgba(0,255,255,0.1)}
.card h3{color:#05ff;font-size:14px;margin-bottom:8px}
.stat{color:#aaa;font-size:12px;line-height:1.8}
.stat span{color:#fff;float:right}
.actions{padding:20px}
.btn{display:inline-block;padding:10px 20px;margin:6px;border:1px solid #333;
border-radius:8px;background:#1a1a3e;color:#05ff;text-decoration:none;
font-size:13px;cursor:pointer;transition:all 0.2s}
.btn:hover{background:#05ff;color:#000}
.log{background:#0a0a1a;border:1px solid #333;border-radius:8px;
padding:12px;margin:20px;font-family:monospace;font-size:11px;
max-height:200px;overflow-y:auto;color:#0f0}
</style>
</head>
<body>
<div class="header">
<h1>CompanionOS Dashboard</h1>
<div class="sub">Dr. Hack Remote Control</div>
</div>
<div class="grid">
<div class="card">
<h3>System</h3>
<div class="stat">Uptime: <span id="uptime">--</span></div>
<div class="stat">Free Heap: <span id="heap">--</span></div>
<div class="stat">CPU Freq: <span id="cpu">--</span></div>
<div class="stat">Flash: <span id="flash">--</span></div>
</div>
<div class="card">
<h3>WiFi</h3>
<div class="stat">AP Clients: <span id="clients">--</span></div>
<div class="stat">AP IP: <span>192.168.4.1</span></div>
<div class="stat">MAC: <span id="mac">--</span></div>
</div>
<div class="card">
<h3>Hardware</h3>
<div class="stat">IR TX: <span>GPIO26</span></div>
<div class="stat">IR RX: <span>GPIO34</span></div>
<div class="stat">CC1101 CSN: <span>GPIO21</span></div>
<div class="stat">nRF24 #1: <span>CE32/CS33</span></div>
</div>
</div>
<div class="actions">
<h3 style="color:#05ff;margin-bottom:12px">Remote Buttons</h3>
<a class="btn" href="/btn?id=left">◀ LEFT</a>
<a class="btn" href="/btn?id=select">● SELECT</a>
<a class="btn" href="/btn?id=right">▶ RIGHT</a>
<a class="btn" href="/btn?id=hold">■ HOLD SELECT</a>
</div>
<div class="actions">
<h3 style="color:#05ff;margin-bottom:12px">Quick Actions</h3>
<a class="btn" href="/scan">WiFi Scan</a>
<a class="btn" href="/status">Refresh Status</a>
</div>
<div class="log" id="log">System ready...</div>
<script>
function refresh(){
fetch('/api/status').then(r=>r.json()).then(d=>{
document.getElementById('uptime').textContent=d.uptime+'s';
document.getElementById('heap').textContent=d.heap+'KB';
document.getElementById('cpu').textContent=d.cpu+'MHz';
document.getElementById('flash').textContent=d.flash+'KB';
document.getElementById('clients').textContent=d.clients;
document.getElementById('mac').textContent=d.mac;
}).catch(()=>{});
}
refresh();
setInterval(refresh,3000);
</script>
</body>
</html>
)rawliteral";

// ═══════════════════════════════════════════════════════════
// STATE
// ═══════════════════════════════════════════════════════════

static WebServer* dhDashServer = nullptr;
static bool dhDashRunning = false;
static int dhDashBtnQueue = -1;  // 0=left, 1=select, 2=right, 3=hold

// ═══════════════════════════════════════════════════════════
// HANDLERS
// ═══════════════════════════════════════════════════════════

static void dhDashHandleRoot() {
  dhDashServer->send(200, "text/html", DH_DASH_HTML);
}

static void dhDashHandleStatus() {
  char json[256];
  snprintf(json, sizeof(json),
    "{\"uptime\":%lu,\"heap\":%d,\"cpu\":%d,\"flash\":%d,\"clients\":%d,\"mac\":\"%s\"}",
    millis() / 1000,
    ESP.getFreeHeap() / 1024,
    ESP.getCpuFreqMHz(),
    ESP.getFlashChipSize() / 1024,
    WiFi.softAPgetStationNum(),
    WiFi.macAddress().c_str()
  );
  dhDashServer->send(200, "application/json", json);
}

static void dhDashHandleBtn() {
  String id = dhDashServer->arg("id");
  if (id == "left") dhDashBtnQueue = 0;
  else if (id == "select") dhDashBtnQueue = 1;
  else if (id == "right") dhDashBtnQueue = 2;
  else if (id == "hold") dhDashBtnQueue = 3;
  dhDashServer->sendHeader("Location", "/", true);
  dhDashServer->send(302, "text/plain", "");
}

static void dhDashHandleScan() {
  String html = "<html><head><meta charset='UTF-8'><title>WiFi Scan</title>";
  html += "<style>body{background:#0a0a1a;color:#e0e0ff;font-family:sans-serif;padding:20px}";
  html += "table{border-collapse:collapse;width:100%}th,td{border:1px solid #333;padding:8px;text-align:left}";
  html += "th{background:#1a1a3e;color:#05ff}</style></head><body>";
  html += "<h2>WiFi Scan Results</h2><table><tr><th>SSID</th><th>RSSI</th><th>CH</th><th>Auth</th></tr>";

  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++) {
    html += "<tr><td>" + WiFi.SSID(i) + "</td><td>" + String(WiFi.RSSI(i)) + "dBm</td>";
    html += "<td>" + String(WiFi.channel(i)) + "</td><td>" + String(WiFi.encryptionType(i)) + "</td></tr>";
  }
  WiFi.scanDelete();
  html += "</table><br><a href='/' style='color:#05ff'>Back</a></body></html>";
  dhDashServer->send(200, "text/html", html);
}

// ═══════════════════════════════════════════════════════════
// DISPLAY
// ═══════════════════════════════════════════════════════════

static void dhDashDrawStatus() {
  tft.fillScreen(CLR_BG);
  drawGradientCard(0, 0, SCR_W, SCALE_Y(14), CLR_PRIMARY, darkenColor(CLR_PRIMARY, 50), 0);
  tft.setTextColor(CLR_TEXT_HI);
  tft.drawString("WEB DASHBOARD", SCALE_X(4), SCALE_Y(2), 1);

  int y = SCALE_Y(20);
  tft.setTextColor(CLR_SUCCESS);
  tft.drawString("[ACTIVE]", SCALE_X(4), y, 1); y += SCALE_Y(14);

  tft.setTextColor(CLR_TEXT_MED);
  tft.drawString("AP: CompanionOS-Hack", SCALE_X(4), y, 1); y += SCALE_Y(12);
  tft.drawString("IP: 192.168.4.1", SCALE_X(4), y, 1); y += SCALE_Y(14);

  char buf[32];
  int clients = WiFi.softAPgetStationNum();
  sprintf(buf, "Clients: %d", clients);
  tft.setTextColor(clients > 0 ? CLR_SUCCESS : CLR_TEXT_MED);
  tft.drawString(buf, SCALE_X(4), y, 1); y += SCALE_Y(14);

  sprintf(buf, "Heap: %dKB", ESP.getFreeHeap() / 1024);
  tft.setTextColor(CLR_TEXT_LO); tft.drawString(buf, SCALE_X(4), y, 1); y += SCALE_Y(12);

  sprintf(buf, "Up: %lus", millis() / 1000);
  tft.drawString(buf, SCALE_X(4), y, 1);

  // Activity bar
  int barW = random(10, SCR_W - SCALE_X(8));
  tft.fillRect(SCALE_X(4), SCR_H - SCALE_Y(22), SCR_W - SCALE_X(8), SCALE_Y(3), CLR_SURFACE);
  tft.fillRect(SCALE_X(4), SCR_H - SCALE_Y(22), barW, SCALE_Y(3), CLR_PRIMARY);

  tft.setTextColor(CLR_TEXT_LO);
  tft.drawString("HOLD SEL: stop", SCALE_X(4), SCR_H - SCALE_Y(10), 1);
}

// ═══════════════════════════════════════════════════════════
// MAIN
// ═══════════════════════════════════════════════════════════

static void dhRunWebDashboard() {
  // Setup AP
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("CompanionOS-Hack", "companion123");
  delay(200);

  // Web server
  dhDashServer = new WebServer(80);
  dhDashServer->on("/", HTTP_GET, dhDashHandleRoot);
  dhDashServer->on("/api/status", HTTP_GET, dhDashHandleStatus);
  dhDashServer->on("/btn", HTTP_GET, dhDashHandleBtn);
  dhDashServer->on("/scan", HTTP_GET, dhDashHandleScan);
  dhDashServer->on("/status", HTTP_GET, dhDashHandleRoot);
  dhDashServer->begin();
  dhDashRunning = true;
  dhDashBtnQueue = -1;

  unsigned long lastDraw = 0;
  unsigned long holdStart = 0; bool holding = false;

  while (true) {
    dhDashServer->handleClient();

    if (millis() - lastDraw > 500) {
      dhDashDrawStatus();
      lastDraw = millis();
    }

    if (digitalRead(BTN_SELECT) == LOW) {
      if (!holding) { holdStart = millis(); holding = true; }
      if (millis() - holdStart > 800) break;
    } else { holding = false; }

    yield();
    delay(5);
  }

  // Cleanup
  dhDashServer->stop();
  delete dhDashServer; dhDashServer = nullptr;
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  dhDashRunning = false;
  delay(100);
}

#endif // ESP32
#endif // DH_WEB_DASHBOARD_H
