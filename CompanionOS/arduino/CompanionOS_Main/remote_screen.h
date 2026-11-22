#ifndef COMPANION_REMOTE_SCREEN_H
#define COMPANION_REMOTE_SCREEN_H

// Read the panel a scanline at a time: no persistent/full-screen RAM buffer.
static void sendRemoteScreen() {
#ifdef COMPANION_BOOT_HEALTH_H
  if (boot_health::recovery) { sysWebServer.send(503, "text/plain", "Recovery mode"); return; }
#endif
#if defined(TFT_MISO) && TFT_MISO >= 0
  if (companion_memory::networkBusy.load(std::memory_order_relaxed)) {
    sysWebServer.send(503, "text/plain", "Display capture paused during TLS"); return;
  }
  const int vx=tft.getViewportX(), vy=tft.getViewportY();
  const int vw=tft.getViewportWidth(), vh=tft.getViewportHeight();
  const bool datum=tft.getViewportDatum();
  tft.resetViewport();
  uint16_t original=tft.readPixel(0, 0);
  const uint16_t probe = original ^ 0xffff;
  tft.drawPixel(0, 0, probe);
  const bool readable=tft.readPixel(0, 0)==probe;
  tft.drawPixel(0, 0, original);
  tft.setViewport(vx, vy, vw, vh, datum);
  if (!readable) {
    sysWebServer.send(501, "text/plain", "TFT readback unavailable; check display MISO wiring"); return;
  }
  constexpr uint32_t stride=(SCREEN_W*3+3)&~3;
  constexpr uint32_t length=54+stride*SCREEN_H;
  uint8_t header[54]={};
  auto put32=[&](int at, uint32_t value) { for(int i=0;i<4;++i) header[at+i]=value>>(8*i); };
  header[0]='B'; header[1]='M'; put32(2,length); put32(10,54); put32(14,40);
  put32(18,SCREEN_W); put32(22,uint32_t(-int32_t(SCREEN_H)));
  header[26]=1; header[28]=24; put32(34,stride*SCREEN_H);
  sysWebServer.sendHeader("Cache-Control", "no-store");
  sysWebServer.setContentLength(length);
  sysWebServer.send(200, "image/bmp", "");
  auto client=sysWebServer.client();
  client.setTimeout(1000);
  if(client.write(header,sizeof(header))!=sizeof(header)) { client.stop(); return; }
  uint8_t row[stride]={};
  uint32_t started=millis();
  for(int y=0;y<SCREEN_H;++y) {
    if(!client.connected() || millis()-started>2000) { client.stop(); break; }
    tft.readRectRGB(0,y,SCREEN_W,1,row);
    for(int x=0;x<SCREEN_W;++x) { uint8_t red=row[x*3]; row[x*3]=row[x*3+2]; row[x*3+2]=red; }
    if(client.write(row,stride)!=stride) { client.stop(); break; }
    yield();
  }
#else
  sysWebServer.send(501, "text/plain", "TFT MISO is not configured");
#endif
}

static void registerRemoteScreenRoutes() {
#ifdef COMPANION_BOOT_HEALTH_H
  sysWebServer.on("/api/recovery", HTTP_POST, []() {
    sysWebServer.send(200, "text/plain", "Restarting");
    boot_health::retry();
  });
#endif
  sysWebServer.on("/api/status", HTTP_GET, []() {
    StaticJsonDocument<384> doc;
    doc["protocol"]=1; doc["page"]=int(currentState); doc["theme"]=activeTheme;
    doc["manual_emotion"]=manualEmotionOverride; doc["ldr_enabled"]=ldrEnabled;
    doc["emotion"]=int(currentEmotion);
    doc["width"]=SCREEN_W; doc["height"]=SCREEN_H;
#ifdef COMPANION_BOOT_HEALTH_H
    doc["recovery"] = boot_health::recovery;
    #ifdef ESP32
    doc["reset_reason"] = int(esp_reset_reason());
    doc["boot_stage"] = boot_health::record.stage;
    #endif
#endif
    String body; serializeJson(doc,body);
    sysWebServer.sendHeader("Cache-Control", "no-store");
    sysWebServer.send(200,"application/json",body);
  });
  sysWebServer.on("/api/screen.bmp", HTTP_GET, sendRemoteScreen);
}
#endif
