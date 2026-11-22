#define COMPANION_HOST_REAL_UI
#include "host_globals.h"
#include "host_page_guards.h"

// TFT_eSPI exports this bitmap name globally on hardware, too.
using host_fonts::font;

// These are the implementation under test, not host copies of the algorithms.
#include "../arduino/CompanionOS_Main/ui_components.h"
#include "../arduino/CompanionOS_Main/page_clock_dashboard.h"
#include "../arduino/CompanionOS_Main/page_retro_watch.h"
#include "../arduino/CompanionOS_Main/page_gaming.h"
#include "../arduino/CompanionOS_Main/page_social.h"
#include "../arduino/CompanionOS_Main/page_productivity.h"
#include "../arduino/CompanionOS_Main/page_network.h"
#include "../arduino/CompanionOS_Main/page_stocks.h"
#include "../arduino/CompanionOS_Main/pages.h"
#include "../arduino/CompanionOS_Main/boot_screen.h"

#include <functional>
#include <iostream>
#include <limits>

namespace {
int failures=0, cases=0, assertions=0, caseFailures=0;
std::string currentCase, filter, renderDir;
void check(bool ok, const std::string& why) {
  ++assertions;
  if (ok) return;
  ++failures;
  if (++caseFailures <= 8) std::cerr << "FAIL " << currentCase << ": " << why << '\n';
}
void test(const std::string& name, const std::function<void()>& body) {
  if (!filter.empty() && name.find(filter) == std::string::npos) return;
  ++cases; caseFailures=0; currentCase=name;
  tft.clearTrace();
  try { body(); }
  catch (const std::exception& e) { check(false, std::string("exception: ") + e.what()); }
  if (caseFailures > 8) std::cerr << "  ... " << caseFailures-8 << " more failures\n";
  if (!caseFailures) std::cout << "PASS " << name << '\n';
}
std::string rect(HostRect r) {
  return "("+std::to_string(r.x)+","+std::to_string(r.y)+","+
    std::to_string(r.w)+","+std::to_string(r.h)+")";
}
bool printable(const std::string& s, bool newline=false) {
  for (unsigned char c : s) if ((c < 32 || c > 126) && !(newline && c=='\n')) return false;
  return true;
}
void checkTextBounds(HostRect bounds, bool allowClip=false) {
  for (const auto& call : tft.texts) {
    const auto r = allowClip ? intersect(call.measured, call.clip) : call.measured;
    check(bounds.contains(r), "text '" + call.text.substr(0, 40) + "' font " +
      std::to_string(call.font) + " measured " + rect(r) + " outside " + rect(bounds));
    check(printable(call.text), "non-ASCII/control bytes reached bitmap text sink");
  }
}
void checkNoTextCollisions() {
  for (std::size_t i=0; i<tft.texts.size(); ++i) {
    const auto& a=tft.texts[i];
    if (a.text.empty()) continue;
    for (std::size_t j=i+1; j<tft.texts.size(); ++j) {
      const auto& b=tft.texts[j];
      if (b.text.empty()) continue;
      const auto overlap=intersect(a.measured,b.measured);
      check(overlap.empty(), "text collision '"+a.text.substr(0,24)+"' "+rect(a.measured)+
        " / '"+b.text.substr(0,24)+"' "+rect(b.measured));
    }
  }
}
template<std::size_t N> void put(char (&dest)[N], const std::string& source) {
  std::snprintf(dest,N,"%s",source.c_str());
}
std::string repeat(const std::string& s, int n) {
  std::string out;
  for (int i=0; i<n; ++i) out+=s;
  return out;
}
const std::string utf8="Caf\xC3\xA9 \xE2\x80\x94 \xE2\x80\x9C" "d\xC3\xA9j\xC3\xA0" "\xE2\x80\x9D \xF0\x9F\x8E\xAE";

void helperTests() {
  test("boot/startup-gates",[] {
    using companion_startup::Gate;
    for (uint32_t start : {0U, uint32_t(UINT32_MAX-2000)}) {
      Gate g;
      check(!g.services(start+10000)&&!g.apis(start+10000),"no services before a first frame");
      g.firstFrame(start);
      g.observeLink(true,start+1500);
      check(!g.services(start+3499)&&g.services(start+3500),"web delay is respected");
      check(!g.apis(start+6499)&&g.apis(start+6500),"API delay is respected");
      g.observeLink(false,start+7000);
      check(!g.services(start+9000)&&!g.apis(start+9000),"drop closes gates");
      g.observeLink(true,start+9000);
      g.observeLink(true,start+10000);
      check(!g.services(start+10499)&&g.services(start+10500),"link must stabilize; repeated polls do not reset timer");
      check(!g.apis(start+11999)&&g.apis(start+12000),"API reconnect gets extra settling time");
    }
  });
  test("boot/incremental-animation",[] {
    drawBootFrame(0,true);
    checkTextBounds({0,0,SCREEN_W,SCREEN_H});checkNoTextCollisions();
    const auto initial=tft.pixels;
    int last=0;
    for (uint32_t ms=16;ms<=1008;ms+=16) {
      tft.clearCalls();drawBootFrame(ms,false);
      check(tft.texts.empty()&&tft.pixelWrites<=size_t(SCREEN_W*2),"animation never erases the title or screen");
      int count=std::count(tft.pixels.begin(),tft.pixels.end(),CLR_PRIMARY);
      check(count>=last,"progress cannot move backwards");last=count;
    }
    for(int y=0;y<SCREEN_H;++y) if(y<SCREEN_H*2/3 || y>=SCREEN_H*2/3+2)
      for(int x=0;x<SCREEN_W;++x) check(tft.pixels[y*SCREEN_W+x]==initial[y*SCREEN_W+x],"only progress pixels change");
    drawBootFrame(0,true);check(tft.pixels==initial,"animation can restart cleanly");
  });
  test("sink/font-metrics-and-raster",[] {
    check(tft.textWidth("Wi.",1)==18,"GLCD uses six-pixel advance");
    check(tft.textWidth("Wi.",2)==19,"font 2 uses installed proportional width table");
    check(tft.textWidth("Wi.",4)==36,"font 4 uses installed proportional width table");
    check(tft.fontHeight(1)==8 && tft.fontHeight(2)==16 && tft.fontHeight(4)==26,"installed font heights");
    for (int font : {1,2,4}) {
      tft.clearTrace(0);
      tft.setTextColor(TFT_WHITE,TFT_BLACK);
      tft.drawString("Wi.",5,5,font);
      const auto ink=std::count(tft.pixels.begin(),tft.pixels.end(),TFT_WHITE);
      check(ink>10,"installed font bitmap produces nonblank glyph pixels");
      tft.setViewport(10,10,12,12,false);
      tft.enforceWriteBounds=true; tft.writeBounds={10,10,12,12};
      tft.drawString("WWWW",0,10,font);
      check(tft.escapedWrites==0,"sink clips text pixels to viewport");
    }
    tft.clearTrace();
    tft.drawString("WWWW",SCREEN_W-5,3,1);
    check(!HostRect{0,0,SCREEN_W,SCREEN_H}.contains(tft.texts.back().measured),
      "sink retains unclipped text bounds so screen clipping cannot hide a regression");
  });
  test("helpers/uiText-transformations",[] {
    struct Mapping { const char* in; const char* out; };
    const Mapping mappings[]={
      {nullptr,""},{"",""},{"plain ASCII 123","plain ASCII 123"},
      {"a\tb\rc\nd","a b c\nd"},{"a\xC2\xA0" "b","a b"},
      {"\xE2\x80\x98" "x\xE2\x80\x99","'x'"},
      {"\xE2\x80\x9C" "x\xE2\x80\x9D","\"x\""},
      {"a\xE2\x80\x93" "b\xE2\x80\x94" "c","a-b-c"},
      {"wait\xE2\x80\xA6","wait..."},
      {"\xC3\x80\xC3\xA0\xC3\x88\xC3\xA8\xC3\x87\xC3\xA7","AaEeCc"},
      {"Caf\xC3\xA9","Cafe"},
      {"\xE2\x80\xA2\xE2\x99\xAA\xE2\x99\xAB","   "},
      {"\xE6\xBC\xA2\xF0\x9F\x8E\xAE","??"}
    };
    for (const auto& m:mappings) check(uiText(m.in)==m.out,std::string("transformation expected '")+m.out+"'");
    const std::string malformed[]={"\xC3","\xE2\x80","\xF0\x9F\x8E","\x80","\xFF","\xE2" "A"};
    for (const auto& s:malformed) {
      const String out=uiText(s.c_str());
      check(printable(out.c_str(),true),"malformed UTF-8 tail must produce only printable fallback bytes");
      check(out.length()>0,"malformed input must not disappear entirely");
    }
    const String huge=uiText(repeat(utf8,300).c_str());
    check(huge.length()<=512 && huge.length()>0,"normalization caps very long input");
  });
  test("helpers/truncate-width-matrix",[] {
    const std::string values[]={"","i","WWW",repeat("W",300),utf8,"one\ntwo",repeat("long word ",80)};
    for (int font:{1,2,4}) for (const auto& value:values) for (int w=-2;w<=90;++w) {
      tft.clearCalls();
      const String fitted=fitUiText(value.c_str(),w,font);
      check(printable(fitted.c_str()),"fitted text is printable ASCII");
      check(w>0 ? tft.textWidth(fitted,font)<=w : fitted.length()==0,"fit width="+std::to_string(w));
      drawTruncatedText(10,10,value.c_str(),w,TFT_WHITE,font,0x1234);
      checkTextBounds({10,10,max(0,w),tft.fontHeight(font)});
      for (const auto& c:tft.texts) check(c.bg==0x1234,"explicit truncation background is preserved");
      if (w<=0) check(tft.pixelWrites==0 && tft.texts.empty(),"nonpositive truncate box must do nothing");
    }
  });
  test("helpers/tiny-empty-and-screen-edge-boxes",[] {
    for (int font:{1,2,4}) {
      const int dot=tft.textWidth(".",font);
      for (int w=0;w<=dot*3+1;++w) {
        tft.clearCalls();
        drawTruncatedText(10,10,"A deliberately long value",w,TFT_WHITE,font);
        checkTextBounds({10,10,w,tft.fontHeight(font)});
        if (w<dot) check(tft.pixelWrites==0,"a box narrower than one dot cannot draw text");
      }
      tft.clearCalls();
      drawTruncatedText(SCREEN_W-7,10,"WWWW",100,TFT_WHITE,font);
      checkTextBounds({SCREEN_W-7,10,7,tft.fontHeight(font)});
      tft.clearCalls();
      drawTruncatedText(SCREEN_W,10,"WWWW",20,TFT_WHITE,font);
      check(tft.texts.empty() && tft.pixelWrites==0,"off-right-edge box has no available width");
      tft.clearCalls();
      drawTruncatedText(10,10,nullptr,20,TFT_WHITE,font);
      drawTruncatedText(10,10,"",20,TFT_WHITE,font);
      check(tft.pixelWrites==0,"null and empty truncation paint no glyphs");
    }
    check(fitUiText("ABC",18,1)=="ABC","exact fit does not gain ellipsis");
    check(fitUiText("WWWW",17,1)=="..","tiny box uses a partial ellipsis");
  });
  test("helpers/wrapped-width-height-matrix",[] {
    const std::string values[]={"","   ","\n","one\ntwo\nthree",repeat("W",100),
      "short words plus a muchlongerunbreakableword",repeat(utf8,20),"a\n\nb", "trailing spaces   "};
    for (int font:{1,2,4}) for (const auto& value:values)
      for (int w:{-1,0,1,5,6,17,30,75}) for (int h:{-1,0,1,8,10,18,28,56}) {
        tft.clearCalls();
        const int n=drawWrappedText(10,10,value.c_str(),w,h,TFT_WHITE,font,0x2345);
        check(n>=0 && n<=max(0,h)/(tft.fontHeight(font)+2),"wrapped row count respects available height");
        check(static_cast<std::size_t>(n)==tft.texts.size(),"returned rows match real draw calls");
        checkTextBounds({10,10,max(0,w),max(0,h)});
        for (const auto& c:tft.texts) check(c.bg==0x2345,"wrapped background forwarded");
        if (w<=0 || h<tft.fontHeight(font)+2)
          check(tft.pixelWrites==0,"tiny/empty wrapped box paints nothing");
      }
  });
  test("helpers/zero-and-one-row-gradients",[] {
    for (int w:{-1,0,1,20}) for (int h:{-1,0}) {
      tft.clearCalls();
      drawGradientCard(10,10,w,h,0xFFFF,0,8);
      check(tft.pixelWrites==0 && tft.primitiveCalls==0,"empty gradient submits no primitives");
    }
    tft.clearTrace(0);
    tft.enforceWriteBounds=true; tft.writeBounds={10,10,20,1};
    drawGradientCard(10,10,20,1,0xFFFF,0,8);
    check(tft.pixelWrites==20,"one-row gradient fills exactly its width");
    check(tft.escapedWrites==0,"one-row gradient stays inside its box");
    check(tft.pixels[10*SCREEN_W+10]==0xFFFF,"one-row gradient uses top color without division by zero");
  });
  test("helpers/marquee-clip-and-restore",[] {
    for (int font:{1,2,4}) for (unsigned now:{0u,1199u,1300u,4000u,100000u,0xFFFFFFF0u}) {
      tft.clearTrace(0x4567); hostMillis=now;
      tft.setViewport(4,5,SCREEN_W-8,SCREEN_H-10,true);
      const HostRect before=tft.clip();
      unsigned long epoch=0;
      tft.enforceWriteBounds=true; tft.writeBounds={24,35,40,tft.fontHeight(font)};
      check(drawMarqueeText(20,30,(repeat(utf8,8)).c_str(),40,TFT_WHITE,font,&epoch),"long marquee reports scrolling");
      check(tft.pixelWrites>0 && tft.escapedWrites==0,"marquee raster is confined to its viewport");
      checkTextBounds(tft.writeBounds,true);
      check(tft.getViewportDatum() && tft.getViewportX()==4 && tft.getViewportY()==5 &&
        tft.getViewportWidth()==SCREEN_W-8 && tft.getViewportHeight()==SCREEN_H-10 &&
        tft.clip().x==before.x && tft.clip().w==before.w,"marquee restores prior viewport and datum");
    }
    tft.clearTrace(); unsigned long epoch=0;
    check(!drawMarqueeText(10,10,"Hi",30,TFT_WHITE,1,&epoch),"fitting marquee is not scrolling");
    tft.clearCalls();
    check(!drawMarqueeText(10,10,"long",0,TFT_WHITE,1,&epoch),"zero-width marquee returns false");
    check(!drawMarqueeText(10,10,"long",30,TFT_WHITE,1,nullptr),"null epoch is safe");
    check(tft.pixelWrites==0 && tft.texts.empty(),"invalid marquee arguments draw nothing");
  });
  test("helpers/marquee-absolute-viewport-restoration",[] {
    tft.setViewport(7,9,80,60,false);
    const HostRect before=tft.clip(); unsigned long epoch=0;
    drawMarqueeText(20,30,"long long long long",40,TFT_WHITE,1,&epoch);
    const HostRect after=tft.clip();
    check(rect(before)==rect(after) && !tft.getViewportDatum(),
      "false-datum clip must also be restored; logical getters alone lose clip: before="+rect(before)+" after="+rect(after));
  });
  test("helpers/marquee-timing-and-tiny-boxes",[] {
    const char* value="WWWWWWWWWWWWWWWWWWWW";
    const int distance=tft.textWidth(value,1)-40;
    unsigned long epoch=100;
    const unsigned moments[]={100,1300,1335,static_cast<unsigned>(1300+distance*35),
      static_cast<unsigned>(2500+distance*35),static_cast<unsigned>(2500+distance*70)};
    for (unsigned now:moments) {
      tft.clearTrace(); hostMillis=now;
      drawMarqueeText(20,30,value,40,TFT_WHITE,1,&epoch);
      check(tft.texts.size()==1,"marquee issues exactly one text draw");
      const int offset=20-tft.texts.back().measured.x;
      check(offset>=0 && offset<=distance,"bounce never scrolls past either text end");
      if (now==100 || now==1300 || now==static_cast<unsigned>(2500+distance*70))
        check(offset==0,"marquee start pause and full-cycle reset");
      if (now==static_cast<unsigned>(1300+distance*35)) check(offset==distance,"marquee reaches far text end");
    }
    for (int width:{-1,0,1,2,5,6}) {
      tft.clearTrace(); hostMillis=5000;
      tft.enforceWriteBounds=true; tft.writeBounds={20,30,max(0,width),8};
      drawMarqueeText(20,30,value,width,TFT_WHITE,1,&epoch);
      check(tft.escapedWrites==0,"tiny marquee viewport does not leak pixels");
      if (width<=0) check(tft.pixelWrites==0,"nonpositive marquee box draws nothing");
    }
    tft.clearTrace(); hostMillis=32;
    epoch=0xFFFFFFF0UL;
    drawMarqueeText(20,30,value,40,TFT_WHITE,1,&epoch);
    check(tft.texts.back().measured.x==20,"32-bit millis rollover retains starting pause");
  });
  test("helpers/marquee-sprite-reuse-and-allocation-fallback",[] {
    unsigned long epoch=0;
    hostMillis=3000;
    drawMarqueeText(10,20,"Long scrolling title here",47,TFT_WHITE,1,&epoch,0x1234);
    const unsigned created=hostSpriteCreates;
    tft.clearCalls();
    drawMarqueeText(10,20,"Long scrolling title here",47,TFT_WHITE,1,&epoch,0x1234);
    check(hostSpriteCreates==created,"same dimensions reuse the allocated sprite");
    check(tft.imagePushes==1,"marquee output uses a sprite transfer");
    check(tft.texts.back().bg==0x1234,"sprite text honors explicit background");
    const unsigned deleted=hostSpriteDeletes;
    drawMarqueeText(10,20,"Long scrolling title here",48,TFT_WHITE,1,&epoch);
    check(hostSpriteCreates==created+1 && hostSpriteDeletes==deleted+1,"resize deletes and recreates strip");
    hostFailSpriteAllocations=2; // Both the RGB565 and reduced-memory attempts fail.
    tft.clearCalls();
    tft.setViewport(7,9,80,60,false);
    drawMarqueeText(10,20,"Long scrolling title here",49,TFT_WHITE,1,&epoch,0x1234);
    check(tft.imagePushes==0 && tft.pixelWrites>0,"allocation failure renders fitted text directly");
    check(rect(tft.clip())==rect({7,9,80,60}),"allocation fallback preserves absolute viewport");
    checkTextBounds({10,20,49,8});
    hostMillis+=2000; tft.clearCalls();
    drawMarqueeText(10,20,"Long scrolling title here",49,TFT_WHITE,1,&epoch,0x1234);
    check(tft.imagePushes==1,"same-size marquee recovers buffering after temporary memory pressure");
    check(rect(tft.clip())==rect({7,9,80,60}),"allocation recovery preserves absolute viewport");
    tft.clearTrace();
    drawMarqueeText(SCREEN_W-5,20,"Long",70,TFT_WHITE,1,&epoch);
    checkTextBounds({SCREEN_W-5,20,5,8},true);
  });
}

void seed(int mode) {
  const bool populated=mode!=0;
  const std::string text=mode==2 ? repeat(utf8,8) : repeat("W",100);
  currentTrack=populated ? text.c_str() : ""; currentArtist=currentTrack;
  currentLyrics=populated ? repeat("A long lyric with spaces ",30).c_str() : "";
  currentLyricsLine2=populated ? text.c_str() : "";
  currentWiFiSSID=populated ? text.c_str() : "";
  wifiConnected=musicPlaying=isPlaying=timeReceived=gameActive=taskActive=populated;
  WiFi.connected=populated; WiFi.ssid=currentWiFiSSID; WiFi.rssi=-65;
  displayHour=populated ? 23 : 0; displayMinute=populated ? 59 : 0; displaySecond=populated ? 58 : 0;
  hostMillis=123456789;
  playProgress=populated ? 5432000 : 0; playDuration=populated ? 7200000 : 0;
  auto set=[&](auto& dest) { put(dest,populated ? text : ""); };
  set(gameTitle); set(sessionTime); set(gameStatus);
  achievePct=populated ? 100 : 0; friendsOnline=populated ? 255 : 0;
  for (int i=0;i<3;++i) { set(recentGame[i]); recentPlaytime[i]=populated ? 65535 : 0; }
  set(socialUser); set(socialApp); set(socialBody); set(socialTime);
  socialLikes=socialComments=populated ? 65535 : 0;
  set(taskCurrent); set(taskCurrentTime); set(taskNext1); set(taskNext1Time); set(taskNext2); set(taskNext2Time);
  taskProgressPct=populated ? 100 : 0;
  set(stockSymbol); set(stockPrice); set(stockDelta); set(stockPctChg);
  for (int i=0;i<3;++i) { set(wlSymbol[i]); set(wlPrice[i]); set(wlDelta[i]); }
  stockHistoryLen=populated ? 40 : 0;
  for (int i=0;i<40;++i) stockHistory[i]=static_cast<int16_t>(1000+i*10);
}
void pageTests() {
  test("memory/current-and-adjacent-custom-image",[] {
    currentState=STATE_EYES;
    check(prepareCustomEyeImage(),"current eye tile can allocate its image buffer");
    uint16_t* image=customEyeImg;
    customEyeReady=true;
    currentState=STATE_SPOTIFY; releaseDistantPageBuffers();
    check(customEyeImg==image && customEyeReady,"immediate neighbor may retain the image");
    currentState=STATE_POMODORO; releaseDistantPageBuffers();
    check(!customEyeImg && !customEyeReady && !customEyeActive,"distant tile releases custom image memory");
    check(!prepareCustomEyeImage(),"off-screen image traffic cannot allocate a distant tile buffer");
  });
  struct Page { const char* name; AppState state; void (*reset)(); void (*draw)(); void (*partial)(); };
  const Page pages[]={
    {"clock",STATE_CLOCK_DASHBOARD,resetClockDashboardDrawState,drawClockDashboardPage,redrawClockDashboardPartial},
    {"retro",STATE_RETRO_WATCH,resetRetroWatchDrawState,drawRetroWatchPage,redrawRetroWatchPartial},
    {"gaming",STATE_GAMING,resetGamingDrawState,drawGamingPage,redrawGamingPartial},
    {"social",STATE_SOCIAL,resetSocialDrawState,drawSocialPage,redrawSocialPartial},
    {"productivity",STATE_PRODUCTIVITY,resetProductivityDrawState,drawProductivityPage,redrawProductivityPartial},
    {"network",STATE_NETWORK,resetNetworkDrawState,drawNetworkPage,redrawNetworkPartial},
    {"stocks",STATE_STOCKS,resetStockDrawState,drawStocksPage,redrawStocksPartial}
  };
  for (const auto& page:pages) for (int mode:{0,1,2}) {
    const std::string label=std::string("pages/")+page.name+(mode==0 ? "-empty" : mode==1 ? "-long" : "-utf8");
    test(label,[&] {
      seed(mode); currentState=page.state; page.reset(); hostPageIndicatorCalls=0;
      page.draw();
      check(tft.pixelWrites>0 && !tft.texts.empty(),"real page rendered nonblank output");
      checkTextBounds({0,0,SCREEN_W,SCREEN_H-UI_FOOTER_H});
      checkNoTextCollisions();
      if (!renderDir.empty()) tft.savePPM(renderDir+"/"+page.name+(mode==0 ? "-empty" : mode==1 ? "-long" : "-utf8")+".ppm");

      // Invoke a real partial refresh after changing every relevant cache key.
      tft.clearCalls();
      displaySecond=(displaySecond+1)%60; displayMinute=(displayMinute+1)%60;
      currentTrack=currentTrack+"!"; currentLyrics=currentLyrics+"!";
      gameTitle[0]='I'; socialBody[0]='I'; taskCurrent[0]='I'; stockPrice[0]='I';
      WiFi.rssi-=5; hostMillis+=1000; albumArtReady=true;
      page.partial();
      checkTextBounds({0,0,SCREEN_W,SCREEN_H-UI_FOOTER_H});
      // Partial paths can intentionally redraw an area twice. Do not treat that
      // temporal overdraw as a simultaneous layout collision.
      const std::size_t calls=tft.texts.size();
      currentState=STATE_EYES; page.partial();
      check(tft.texts.size()==calls,"inactive page partial refresh performs no text draws");
    });
  }
}

std::vector<uint16_t> crop(HostRect r) {
  std::vector<uint16_t> out;
  for (int y=r.y;y<r.y+r.h;++y)
    for (int x=r.x;x<r.x+r.w;++x) out.push_back(tft.pixels[y*SCREEN_W+x]);
  return out;
}
bool hasTextAt(int x,int y,const String& expected) {
  for (const auto& call:tft.texts)
    if (call.measured.x==x && call.measured.y==y && call.text==expected.c_str()) return true;
  return false;
}
void saveFrame(const std::string& name) {
  if (!renderDir.empty()) tft.savePPM(renderDir+"/"+name+".ppm");
}
void orchestrationTests() {
  for (int theme:{0,1,2}) test(std::string("spotify/quiet-polling-")+std::to_string(theme),[&] {
    seed(0); activeTheme=theme; currentState=STATE_SPOTIFY;
    currentTrack="Test track"; currentArtist="Test artist";
    currentLyrics=currentLyricsLine2=standaloneSpotifyStatus=standaloneLyricsStatus="";
    isPlaying=true; playDuration=240000; playProgress=1000;
    renderCurrentPage();
    for (const auto& call:tft.texts)
      check(call.text!="Now playing" && call.text!="Updating","routine status never displaces lyrics");
    tft.clearCalls();
    for (int frame=0;frame<100;++frame) { hostMillis+=50; tickSpotifyVisuals(); }
    check(tft.pixelWrites==0,"unchanged short-title playback does not repaint at frame rate");
    currentLyrics="A real timed line"; currentLyricsLine2="The following line";
    if (theme==1) t2_redrawSpotifyPartial(); else redrawSpotifyPartial();
    check(tft.pixelWrites>0,"a changed lyric actually paints");
    checkTextBounds({0,0,SCREEN_W,SCREEN_H});
    checkNoTextCollisions();
    saveFrame(std::string("spotify-lyrics-theme")+std::to_string(theme+1));
    tft.clearCalls(); standaloneLyricsSynced=false;
    if (theme==1) t2_redrawSpotifyPartial(); else redrawSpotifyPartial();
    bool textMode=false;
    for (const auto& call:tft.texts) if (call.text=="TEXT") textMode=true;
    check(textMode,"unsynced text is labeled, never passed off as timed lyrics");
    checkTextBounds({0,0,SCREEN_W,SCREEN_H});
    checkNoTextCollisions();
    saveFrame(std::string("spotify-text-theme")+std::to_string(theme+1));
    standaloneLyricsSynced=true;
    standaloneLyricsStatus="Lyrics unavailable"; currentLyrics=currentLyricsLine2="";
    if (theme==1) t2_redrawSpotifyPartial(); else redrawSpotifyPartial();
    checkTextBounds({0,0,SCREEN_W,SCREEN_H});
    saveFrame(std::string("spotify-unavailable-theme")+std::to_string(theme+1));
    standaloneLyricsStatus="";
  });
  for (int theme:{0,1}) for (int mode:{0,1,2}) {
    const std::string name=std::string("spotify-")+(theme ? "theme2" : "classic")+
      (mode==0 ? "-empty" : mode==1 ? "-long" : "-utf8");
    test("integration/"+name,[&] {
      seed(mode); activeTheme=theme; albumArtReady=false;
      hostMillis+=static_cast<uint32_t>(cases)*10000;
      standaloneSpotifyStatus="";
      currentState=STATE_SPOTIFY;
      renderCurrentPage();
      const int tx=theme ? 74 : 4, ty=theme ? 18 : 85, width=SCREEN_W-tx-4;
      const String title=currentTrack.length() ? currentTrack : "Nothing playing";
      const String artist=currentArtist.length() ? currentArtist : "Spotify";
      check(hasTextAt(tx,ty,fitUiText(title.c_str(),width,1)),"title paints on first frame without waiting for a packet");
      check(hasTextAt(tx,theme ? 30 : 95,fitUiText(artist.c_str(),width,1)),"artist paints on first frame");
      checkTextBounds({0,0,SCREEN_W,SCREEN_H});
      checkNoTextCollisions();
      const HostRect control=theme ? HostRect{74+(SCREEN_W-78)/2-8,59,17,15} : HostRect{SCREEN_W/2-8,105,17,15};
      const auto before=crop(control);
      check(std::count(before.begin(),before.end(),theme ? T2S_ACCENT : CLR_TEXT_HI)>10,
        "first frame contains actual playback control pixels");
      saveFrame(name);
      const auto initial=crop({0,UI_HEADER_H,SCREEN_W,SCREEN_H-UI_HEADER_H});
      setPage(STATE_NOTES);
      tft.clearCalls();
      setPage(STATE_SPOTIFY);
      check(initial==crop({0,UI_HEADER_H,SCREEN_W,SCREEN_H-UI_HEADER_H}),
        "same-data page reentry repaints complete Spotify content identically");
      check(hasTextAt(tx,ty,fitUiText(title.c_str(),width,1)),"title is issued again after reentry");
      isPlaying=!isPlaying; tft.clearCalls();
      if (theme) t2_redrawSpotifyPartial(); else redrawSpotifyPartial();
      check(before!=crop(control),"play/pause state change redraws real control geometry");
      checkTextBounds({0,0,SCREEN_W,SCREEN_H});
      currentTrack=repeat(utf8,30).c_str();
      hostMillis+=5000; tft.clearCalls();
      tickSpotifyVisuals();
      checkTextBounds({0,0,SCREEN_W,SCREEN_H},true);
      check(tft.imagePushes>=1,"visual tick draws title through sprite marquee");
    });
  }
  for (int theme:{0,1}) test(std::string("integration/spotify-art-transaction-")+std::to_string(theme),[&] {
    seed(1); activeTheme=theme; currentState=STATE_SPOTIFY; albumArtReady=false;
    currentTrack="Cover regression"; currentArtist="Native harness";
    currentLyrics="Known RGB565 thumbnail"; currentLyricsLine2="Same-title art updates";
    renderCurrentPage();
    const uint32_t revision=albumArtRevision;
    const uint16_t colors[]={0xF800,0x07FF,0xFFE0,0xF81F};
    check(prepareIncomingArt(),"artwork storage is allocated only for a transfer near its page");
    for (int y=0;y<64;++y) for (int x=0;x<64;++x)
      albumArtIncoming[y*64+x]=(x%16<2 || y%16<2) ? 0xFFFF : colors[(x/32)+2*(y/32)];
    const std::vector<uint16_t> firstCover(albumArtIncoming,albumArtIncoming+ALBUM_ART_PIXELS);
    albumArtRowsComplete=63; receivingArt=true;
    completeAlbumArt();
    check(albumArtRevision==revision && !albumArtReady,"incomplete art does not replace displayed image");
    albumArtRowsComplete=64; tft.clearCalls();
    completeAlbumArt();
    check(albumArtRevision==revision+1 && albumArtReady && !receivingArt,"completed art publishes one new revision");
    check(tft.imagePushes>0 && crop({4,18,64,64})==firstCover,"first completed cover transfers every RGB565 pixel at the correct orientation");
    const std::string imageName=theme ? "spotify-theme2-cover" : "spotify-classic-cover";
    saveFrame(imageName);
    check(prepareIncomingArt(),"next cover reuses the previous thumbnail storage");
    for (int i=0;i<ALBUM_ART_PIXELS;++i) albumArtIncoming[i]=static_cast<uint16_t>(albumArtIncoming[i]^0xFFFF);
    const std::vector<uint16_t> secondCover(albumArtIncoming,albumArtIncoming+ALBUM_ART_PIXELS);
    tft.clearCalls(); completeAlbumArt();
    check(tft.imagePushes>0 && crop({4,18,64,64})==secondCover,"same-title newer art revision is not skipped by cache");
    saveFrame(imageName+"-updated");
    setPage(STATE_POMODORO); tft.clearCalls(); setPage(STATE_SPOTIFY);
    check(tft.imagePushes>0 && crop({4,18,64,64})==secondCover,"cached cover is fully repainted on unchanged-data reentry");
    setPage(STATE_NOTES);
    check(!albumArt && !albumArtIncoming && !albumArtReady,"distant tiles do not retain Spotify image buffers");
    checkTextBounds({0,0,SCREEN_W,SCREEN_H});
  });
  for (AppState state:{STATE_NOTIFICATIONS,STATE_NOTES,STATE_SETTINGS}) for (int mode:{0,1,2}) {
    const std::string name=std::string(state==STATE_NOTIFICATIONS ? "notifications" : state==STATE_NOTES ? "notes" : "settings")+
      (mode==0 ? "-empty" : mode==1 ? "-long" : "-utf8");
    test("integration/"+name,[&] {
      seed(mode); activeTheme=mode; currentState=state;
      for (int i=0;i<4;++i) currentNotes[i]=mode ? repeat(mode==2 ? utf8 : "Long note text ",40).c_str() : "";
      notifTotal=mode ? 32767 : 0;
      for (int i=0;i<3;++i) {
        notifApps[i]=mode ? repeat(mode==2 ? utf8 : "APP",40).c_str() : "";
        notifTitles[i]=mode ? repeat(mode==2 ? utf8 : "Long message ",40).c_str() : "";
        notifTimes[i]=mode ? "23:59 very long timestamp" : "";
      }
      renderCurrentPage();
      check(tft.texts.size()>2,"real pages.h function paints content on first entry");
      checkTextBounds({0,0,SCREEN_W,SCREEN_H-UI_FOOTER_H},state==STATE_SETTINGS);
      checkNoTextCollisions(); saveFrame(name);
      const auto first=tft.pixels;
      setPage(STATE_GAMING); tft.clearCalls(); setPage(state);
      check(first==tft.pixels,"unchanged data repaints identical full page after reentry");
      if (state==STATE_SETTINGS) {
        for (int scroll:{1,40,10000,-1}) {
          settingsScrollY=scroll; tft.clearCalls(); redrawSettingsPartial();
          check(settingsScrollY>=0 && settingsScrollY<=max(0,228-(SCREEN_H-UI_HEADER_H-UI_FOOTER_H)),"settings scroll is clamped");
          checkTextBounds({0,UI_HEADER_H,SCREEN_W,SCREEN_H-UI_HEADER_H-UI_FOOTER_H},true);
          check(rect(tft.clip())==rect({0,0,SCREEN_W,SCREEN_H}),"settings restores default viewport after clipped scrolling");
        }
      }
    });
  }
  test("integration/pomodoro-same-data-reentry",[] {
    seed(0); currentState=STATE_POMODORO; pomoRemaining=1500; pomoActive=false;
    renderCurrentPage();
    const auto first=tft.pixels;
    setPage(STATE_NOTES); tft.clearCalls(); setPage(STATE_POMODORO);
    check(first==tft.pixels,"page-generation invalidates cached timer text on reentry");
    check(tft.texts.size()>2,"timer content is not blank with unchanged time");
  });
}

void weatherTests() {
  test("weather/icon-native-pixel-bounds",[] {
    for (int size:{44,46,86,88}) for (int kind=0;kind<8;++kind) {
      tft.clearTrace(); tft.enforceWriteBounds=true; tft.writeBounds={10,20,size,size};
      drawWeatherIcon(10,20,size,kind);
      check(tft.pixelWrites>10,"weather icon is a visible native graphic");
      check(tft.escapedWrites==0,"native icon remains in its assigned box, kind="+std::to_string(kind));
    }
    check(weatherIconKind(1000)==weatherIconKind(800),"clear icon supports both weather sources");
    check(weatherIconKind(1183)==weatherIconKind(500),"rain icon supports both weather sources");
    check(weatherIconKind(1213)==weatherIconKind(600),"snow icon supports both weather sources");
    check(weatherIconKind(1087)==weatherIconKind(200),"storm icon supports both weather sources");
  });
  auto forecast=[] {
    seed(1); currentState=STATE_WEATHER;
    standaloneWeatherStatus="";
    weatherCity="Delhi"; weatherCondition="Partly cloudy"; weatherCode=1003;
    weatherTemp=32; weatherFeels=37; weatherHumidity=74; weatherWind=12;
    weatherHigh=35; weatherLow=26; weatherSunrise="06:04 AM"; weatherSunset="06:20 PM";
  };
  struct Condition { int code; const char* name; };
  const Condition conditions[]={{1000,"Clear"},{1003,"Partly cloudy"},{1006,"Overcast"},
    {1183,"Rain showers"},{1087,"Thunderstorm"},{1213,"Snow"},{1135,"Fog"},{0,"Unknown condition"},
    {800,"Clear"},{801,"Partly cloudy"},{803,"Overcast"},{500,"Rain"},{200,"Thunderstorm"},{600,"Snow"},{741,"Fog"}};
  for (const auto& condition:conditions) test("weather/forecast-"+std::to_string(condition.code),[&] {
    forecast(); weatherCode=condition.code; weatherCondition=condition.name;
    renderCurrentPage();
    checkTextBounds({0,0,SCREEN_W,SCREEN_H-UI_FOOTER_H}); checkNoTextCollisions();
    check(hasTextAt(SCREEN_W<=200 ? 60 : 120,SCREEN_W<=200 ? 33 : 52,"32 C"),"prominent temperature is the actual reading");
    check(hasTextAt(6,86,"Feels 37 C") || hasTextAt(12,169,"37 C"),"actual feels-like temperature is shown");
    check(hasTextAt(86,86,"RH 74%") || hasTextAt(110,169,"74%"),"actual humidity is shown");
    check(hasTextAt(6,98,"12 km/h") || hasTextAt(208,169,"12 km/h"),"actual wind speed is shown");
    check(hasTextAt(86,98,"H35 L26") || hasTextAt(12,212,"35 / 26 C"),"actual high and low are shown");
    check(hasTextAt(20,110,"06:04 AM") || hasTextAt(110,212,"06:04 AM"),"actual sunrise is shown");
    check(hasTextAt(100,110,"06:20 PM") || hasTextAt(208,212,"06:20 PM"),"actual sunset is shown");
    const auto frame=tft.pixels;
    saveFrame("weather-"+std::to_string(condition.code));
    setPage(STATE_NOTES); tft.clearCalls(); setPage(STATE_WEATHER);
    check(frame==tft.pixels,"unchanged weather repaints identically on reentry");
    currentState=STATE_EYES; tft.clearCalls(); redrawWeatherPartial();
    check(tft.pixelWrites==0 && tft.texts.empty(),"inactive weather update paints nothing");
  });
  for (int state=0;state<5;++state) test("weather/empty-status-"+std::to_string(state),[&] {
    forecast(); weatherCondition=state==4 ? "   " : "";
    wifiConnected=state!=0;
    standaloneWeatherStatus=state==0 ? "Updating forecast" : state==1 ? "Weather request failed" :
      state==2 ? repeat(utf8,50).c_str() : "";
    renderCurrentPage();
    checkTextBounds({0,0,SCREEN_W,SCREEN_H-UI_FOOTER_H}); checkNoTextCollisions();
    for (const auto& call:tft.texts) {
      check(call.text.find("Updating")==std::string::npos,"empty/offline page cannot indefinitely claim an update");
      check(call.text!="32 C","empty forecast must not display default numeric readings");
    }
    saveFrame("weather-empty-"+std::to_string(state));
  });
  for (int state=0;state<4;++state) test("weather/bounded-and-cached-"+std::to_string(state),[&] {
    forecast();
    if (state<=1) {
      weatherCity=repeat(state ? utf8 : "Very long city ",50).c_str();
      weatherCondition=repeat(state ? utf8 : "A long weather condition ",50).c_str();
      weatherSunrise=repeat(utf8,30).c_str(); weatherSunset=weatherSunrise;
      weatherTemp=state ? -128 : 2147483647;
      weatherFeels=-2147483647; weatherWind=2147483647;
      weatherHumidity=2147483647; weatherHigh=2147483647; weatherLow=-2147483647;
    } else {
      wifiConnected=state!=2;
      standaloneWeatherStatus="Request timed out";
    }
    renderCurrentPage();
    checkTextBounds({0,0,SCREEN_W,SCREEN_H-UI_FOOTER_H}); checkNoTextCollisions();
    if (state>=2) {
      check(hasTextAt(SCREEN_W<=200 ? 60 : 120,SCREEN_W<=200 ? 33 : 52,"32 C"),"cached forecast remains visible after connection failure");
      bool statusFound=false;
      for (const auto& call:tft.texts) if (call.text==(state==2 ? "Offline" : "Saved")) statusFound=true;
      check(statusFound,"cached forecast carries a small offline/stale status");
    }
    saveFrame("weather-cached-"+std::to_string(state));
  });
}

void lyricsTests() {
  for (int theme=0; theme<3; ++theme) for (int mode=0; mode<4; ++mode)
    test("lyrics/layout-"+std::to_string(theme)+"-"+std::to_string(mode),[&] {
      seed(0); activeTheme=theme; currentState=STATE_LYRICS;
      releaseLyricsRenderingBuffers(); hostFailSpriteAllocations=mode==3 ? 1 : 0;
      isPlaying=mode!=0; wifiConnected=mode!=0;
      currentTrack=mode==2 ? repeat(utf8,30).c_str() : "Quiet satellite";
      currentArtist="Studio session";
      currentLyrics=mode==0 ? "" : mode==2 ? repeat("Superlongword ",40).c_str() : "Hold the light";
      currentLyricsLine2=mode==0 ? "" : "Close to me";
      standaloneLyricsStatus=mode==0 ? "Instrumental" : "";
      standaloneSpotifyStatus=""; standaloneLyricsSynced=true;
      companion_lyrics::timing()=companion_lyrics::Timing();
      if (!albumArt) albumArt=static_cast<uint16_t*>(malloc(ALBUM_ART_BYTES));
      for (int y=0;y<64;++y) for (int x=0;x<64;++x)
        albumArt[y*64+x]=x<32 ? (y<32 ? 0x07ff : 0xf800) : (y<32 ? 0xf81f : 0xffe0);
      albumArtReady=true; ++albumArtRevision;
      playDuration=120000; playProgress=40000;
      renderCurrentPage();
      checkTextBounds({0,0,SCREEN_W,SCREEN_H},true);
      if (mode!=2) checkNoTextCollisions();
      check(lyrics_page::artSide>(SCREEN_W<240 ? 32 : 80),"cover is larger than the previous layout");
      check(tft.fontHeight(lyrics_page::currentFont)>tft.fontHeight(lyrics_page::nextFont),"current lyric uses larger type");
      if(SCREEN_W>=240) check(8+lyrics_page::artSide<lyrics_page::left,"cover and lyrics have a clear gutter");
      for(int x=lyrics_page::left;x<lyrics_page::left+lyrics_page::width;++x)
        check(tft.pixels[(lyrics_page::top+lyrics_page::rowHeight-1)*SCREEN_W+x]==TFT_BLACK,"no constellation marks beneath words");
      saveFrame("lyrics-"+std::to_string(theme)+"-"+std::to_string(mode));
      tft.clearCalls(); hostMillis+=50; redrawLyricsPage();
      check(tft.pixelWrites<size_t(SCREEN_W*SCREEN_H/2),"no full-screen erase on animation tick");
      tft.clearCalls(); drawStatusBar();
      check(tft.pixelWrites==0,"shared status bar cannot overwrite immersive lyrics");
      // Animated rows cannot overwrite artwork or metadata.
      for (int f=0;f<80;++f) {
        tft.clearCalls(); hostMillis+=50;
        tft.enforceWriteBounds=true;
        tft.writeBounds={lyrics_page::left,lyrics_page::top,lyrics_page::width,lyrics_page::rowHeight*2};
        redrawLyricsPage();
        check(tft.escapedWrites==0,"animation stays in two-row lyric region");
        checkTextBounds({lyrics_page::left,lyrics_page::top,lyrics_page::width,lyrics_page::rowHeight*2},true);
      }
      tft.enforceWriteBounds=false;
      isPlaying=false; hostMillis+=50; redrawLyricsPage();
      tft.clearCalls(); hostMillis+=50; redrawLyricsPage();
      check(tft.pixelWrites==0,"paused lyric display remains quiet");
      setPage(STATE_SPOTIFY);
      check(!lyrics_page::buffered,"leaving releases lyric canvas");
      hostFailSpriteAllocations=0;
    });
  test("lyrics/short-line-fade",[] {
    seed(0); currentState=STATE_LYRICS; isPlaying=true;
    currentLyrics="Stay"; currentLyricsLine2="Sing";
    renderCurrentPage();
    currentLyrics="Sing"; currentLyricsLine2="Again";
    hostMillis+=50; redrawLyricsPage();
    check(lyrics_page::transitioning && lyrics_page::outgoing=="Stay","previous line starts a bounded fade");
    hostMillis+=25; redrawLyricsPage();
    check(lyrics_page::paintedFade==1,"fade advances without blocking the loop");
    hostMillis+=25; tft.clearCalls(); redrawLyricsPage();
    bool newLine=false;
    for(const auto& call:tft.texts) if(call.text=="Sing") newLine=true;
    check(newLine,"new current line is visible within 50ms");
    hostMillis+=50; redrawLyricsPage();
    check(!lyrics_page::transitioning && !lyrics_page::outgoing.length(),"fade completes and releases old text after 100ms");
    hostMillis+=50; tft.clearCalls(); redrawLyricsPage();
    check(tft.pixelWrites==0,"short lyrics remain stable after the fade");
    saveFrame("lyrics-transition-finished");
  });
  test("lyrics/paused-scroll-and-duration-refresh",[] {
    seed(0); currentState=STATE_LYRICS; isPlaying=true;
    currentLyrics=repeat("A long lyric line ",12).c_str(); currentLyricsLine2="Next";
    companion_lyrics::timing()=companion_lyrics::Timing();
    playProgress=10000; playDuration=120000;
    renderCurrentPage();
    for(int i=0;i<50;++i) { hostMillis+=50; redrawLyricsPage(); }
    int stoppedOffset=lyrics_page::offsets[0];
    check(stoppedOffset>0,"long lyric has started scrolling");
    isPlaying=false; hostMillis+=50; redrawLyricsPage();
    uint32_t stoppedMotion=lyrics_page::motionTime;
    for(int i=0;i<40;++i) { hostMillis+=50; redrawLyricsPage(); }
    check(lyrics_page::motionTime==stoppedMotion,"pause freezes motion clock");
    isPlaying=true; hostMillis+=50; redrawLyricsPage();
    check(lyrics_page::offsets[0]==stoppedOffset,"resume does not jump past paused lyric text");
    playDuration=180000; hostMillis+=50; tft.clearCalls(); redrawLyricsPage();
    if(SCREEN_W>=240) check(lyrics_page::durationSeconds==180,"duration changes repaint even at same playback second");
    companion_lyrics::timing().wordTimed=true;
    companion_lyrics::timing().word=10; hostMillis+=50; redrawLyricsPage();
    int center=lyrics_page::wordCenter(lyrics_page::lines[0],10)+8-lyrics_page::offsets[0];
    check(center>=0 && center<lyrics_page::width,"real timestamped active word remains on-screen");
    releaseLyricsRenderingBuffers();
    lyrics_page::lastAllocation=hostMillis;
    companion_lyrics::timing().word=12; hostMillis+=50; redrawLyricsPage();
    center=lyrics_page::wordCenter(lyrics_page::lines[0],12)+8-lyrics_page::offsets[0];
    check(!lyrics_page::buffered,"low-memory fallback does not require a sprite");
    check(center>=0 && center<lyrics_page::width,"low-memory fallback keeps the sung word on-screen");
    setPage(STATE_EYES);
    check(!lyrics_page::rawLines[0].length() && !lyrics_page::lines[0].length(),"leaving frees lyric text layout caches");
  });
  test("lyrics/navigation",[] {
    seed(0); currentState=STATE_EYES;
    companion_lyrics::navigation()=companion_lyrics::Navigation();
    currentTrack="Track"; playDuration=120000; playProgress=2000; isPlaying=true;
    tickLyricsNavigation(); check(currentState==STATE_LYRICS,"playing opens tile three");
    changePage(1); tickLyricsNavigation();
    check(currentState==STATE_POMODORO,"manual next wins over auto entry");
    currentTrack="Next track"; tickLyricsNavigation();
    check(currentState==STATE_POMODORO,"track changes do not steal manual navigation");
    isPlaying=false; tickLyricsNavigation(); hostMillis+=5000; tickLyricsNavigation();
    check(currentState==STATE_POMODORO,"stop cannot displace a manually selected tile");
    isPlaying=true; tickLyricsNavigation(); check(currentState==STATE_LYRICS,"new playback session rearms");
    isPlaying=false; tickLyricsNavigation(); hostMillis+=4999; tickLyricsNavigation();
    check(currentState==STATE_LYRICS,"stop grace lasts five seconds");
    hostMillis+=1; tickLyricsNavigation(); check(currentState==STATE_EYES,"confirmed stop returns home");
  });
}

void headerTests() {
  test("header/idle-sun-and-moon",[] {
    for (int hour:{8,15,19,23}) {
      seed(0); currentState=STATE_EYES; timeReceived=true; displayHour=hour;
      ++uiPageGeneration; drawStatusBar();
      checkTextBounds({0,0,SCREEN_W,UI_HEADER_H}); checkNoTextCollisions();
      check(std::count(tft.pixels.begin(),tft.pixels.end(),hour==23 ? CLR_PRIMARY : CLR_WARNING)>0,"day/night graphic uses actual hour");
      saveFrame("header-idle-"+std::to_string(hour));
      tft.clearTrace();
    }
  });
  test("header/music-alternation-and-real-progress",[] {
    seed(1); currentState=STATE_EYES;
    currentTrack=repeat("Track ",30).c_str(); currentArtist=repeat(utf8,20).c_str();
    playDuration=10000; playProgress=2500;
    for (unsigned now:{0u,6000u,12000u}) {
      tft.clearTrace(); hostMillis=now; ++uiPageGeneration; drawStatusBar();
      const String expected=(now/6000)%2 ? currentArtist : currentTrack;
      check(hasTextAt(17,4,fitUiText(expected.c_str(),SCREEN_W-64,1)),"six-second alternation uses the actual title/artist");
      checkTextBounds({0,0,SCREEN_W,UI_HEADER_H}); checkNoTextCollisions();
      const int actual=std::count(tft.pixels.begin()+(UI_HEADER_H-1)*SCREEN_W,tft.pixels.begin()+UI_HEADER_H*SCREEN_W,CLR_SUCCESS);
      check(actual==(SCREEN_W-8)/4,"header underline is actual quarter-track progress");
      saveFrame("header-music-"+std::to_string(now));
    }
    playProgress=20000; tft.clearCalls(); drawStatusBar();
    check(std::count(tft.pixels.begin()+(UI_HEADER_H-1)*SCREEN_W,tft.pixels.begin()+UI_HEADER_H*SCREEN_W,CLR_SUCCESS)==SCREEN_W-8,"header progress clamps at track duration");
  });
}

void advancedTileTests() {
  for(int mode=0;mode<6;++mode) test("focus/state-"+std::to_string(mode),[&] {
    seed(0); currentState=STATE_POMODORO;
    pomoTotal=1500;pomoRemaining=mode==0?1500:mode==3?0:750;
    pomoActive=mode==1;pomoIsBreak=mode==2;pomoSessions=2;
    if(mode==4) { pomoTotal=pomoRemaining=86400;pomoSessions=2147483647; }
    if(mode==5) { pomoTotal=0;pomoRemaining=-2147483647; }
    renderCurrentPage();checkTextBounds({0,0,SCREEN_W,SCREEN_H-UI_FOOTER_H});checkNoTextCollisions();
    if(mode==1||mode==2) {
      uint16_t color=pomoIsBreak?TFT_GREEN:0xDF26;
      check(std::count(tft.pixels.begin(),tft.pixels.end(),color)>20,"running and paused timers retain actual elapsed progress");
    }
    saveFrame("focus-"+std::to_string(mode));
  });
  for(int page=0;page<4;++page) for(int mode=0;mode<3;++mode)
    test("pc-dashboard/"+std::to_string(page)+"/"+std::to_string(mode),[&] {
      seed(0); currentState=STATE_PC_DASH; hostMillis+=10000; pcd_subPage=page;
      pcd_historyCount=0;
      processPcDashPacket(R"({"cpu":42,"ram":68,"down":12.5,"up":1.5,"uptime":987654,"freq":4200,"dread":8,"dwrite":4,"proc":200,"rx":25,"tx":7,"latency":24,"diskfree":40,"top_cpu_pct":12,"top_cpu":"Renderer","top_ram_mb":2400,"top_ram":"Editor","gpu":72,"vram":64,"vram_used":4096,"vram_total":8192,"gpu_temp":65,"laptop_temp":70})");
      if(mode==0) pcd_hasPacket=false;
      if(mode==2) processPcDashPacket(R"({"cpu":null,"gpu_temp":149,"freq":99999,"proc":2147483647,"rx":1000000000,"latency":3600000,"diskfree":0,"top_cpu":"An exceptionally long process name that must be clipped","top_ram":"Another exceptionally long process name","top_ram_mb":1000000000})");
      resetPcDashDrawState(); tft.clearCalls(); renderCurrentPage();
      checkTextBounds({0,0,SCREEN_W,SCREEN_H});checkNoTextCollisions();
      saveFrame("pc-dashboard-"+std::to_string(page)+"-"+std::to_string(mode));
      tft.clearCalls();hostMillis+=250;tickPcDash();
      check(tft.pixelWrites==0,"unchanged telemetry does not redraw");
      if(mode) {
        hostMillis+=PCD_TIMEOUT_MS;tickPcDash();
        check(pcd_offline&&pcd_offlineShown,"expired telemetry shows offline");
        checkTextBounds({0,0,SCREEN_W,SCREEN_H});
      }
    });
  for(int mode=0;mode<5;++mode) test("ruview/state-"+std::to_string(mode),[&] {
    seed(0); currentState=STATE_RUVIEW;
    rvOccupied=mode==1||mode==2;rvMotion=mode==2;rvCalibrating=mode==3;
    rvConfidence=mode==4 ? 99999999.0f : 72.0f;
    rvVariance=mode==4 ? 99999999.5f : 0.015f;
    rvPPS=mode==4 ? 99999999.0f : 20.0f;rvRSSI=-62;rvSubcarriers=128;
    put(rvZoneLabel,mode==4 ? repeat(utf8,9) : "Office");
    put(rvStatus,mode==0 ? "Offline" : mode==3 ? "Calibrating" : mode==4 ? repeat("A",40) : "Present");
    renderCurrentPage();checkTextBounds({0,0,SCREEN_W,SCREEN_H});checkNoTextCollisions();
    saveFrame("ruview-"+std::to_string(mode));
    tft.clearCalls();hostMillis+=1000;redrawRuviewPartial();
    tft.clearCalls();hostMillis+=1000;redrawRuviewPartial();
    check(tft.pixelWrites==0,"unchanged sensing does not redraw at frame rate");
  });
  test("ruview/stale-samples",[&] {
    currentState=STATE_RUVIEW;hostMillis+=20000;noteRuviewPacket();
    rvOccupied=rvMotion=true;rvPPS=20;rvConfidence=95;put(rvStatus,"Motion");
    drawRuviewPageFull();tft.clearCalls();hostMillis+=RV_TIMEOUT_MS;redrawRuviewPartial();
    check(!rvOccupied&&!rvMotion&&rvPPS==0&&rvConfidence==0,"expired sensor data never reports live occupancy");
    check(std::string(rvStatus)=="CSI offline","lost feed reports a real timeout");
  });
}
}

void plannerTests() {
  for (bool classes : {false,true}) for (bool populated : {false,true}) {
    test(std::string("planner/")+(classes?"classes":"events")+(populated?"-full":"-empty"),[&] {
      using namespace companion_planner;
      plan=Plan(); cursor=0;
      if(populated) {
        plan.eventCount=12;plan.classCount=16;
        for(int i=0;i<12;++i) {
          plan.events[i].id=i+1;plan.events[i].at=uint32_t(time(nullptr))+i*3600;
          strcpy(plan.events[i].title,"Calculus final exam - integration module");
        }
        for(int i=0;i<16;++i) {
          plan.classes[i].id=i+1;plan.classes[i].day=i%7;plan.classes[i].start=540;
          plan.classes[i].duration=60;
          strcpy(plan.classes[i].title,"Physics lab - practical assessment");
          strcpy(plan.classes[i].place,"Building C laboratory 04");
        }
      }
      currentState=classes?STATE_TIMETABLE:STATE_CALENDAR;
      renderCurrentPage();checkTextBounds({0,0,SCREEN_W,SCREEN_H});checkNoTextCollisions();
      saveFrame(classes?"planner-classes":"planner-events");
      if(populated) {
        for(int i=0;i<(classes?16:12);++i) {tft.clearCalls();input(1);checkTextBounds({0,0,SCREEN_W,SCREEN_H});checkNoTextCollisions();}
        check(cursor==0,"planner wraps after last entry");
      }
    });
  }
}

int main(int argc,char** argv) {
  std::cout.setf(std::ios::unitbuf);
  for (int i=1;i<argc;++i) {
    const std::string arg=argv[i];
    if (arg=="--filter" && i+1<argc) filter=argv[++i];
    else if (arg=="--render" && i+1<argc) renderDir=argv[++i];
    else { std::cerr << "Usage: ui_host_regression [--filter substring] [--render existing-directory]\n"; return 2; }
  }
  std::cout << "Native real-header UI regression " << SCREEN_W << "x" << SCREEN_H << '\n';
  helperTests(); pageTests(); orchestrationTests(); weatherTests(); headerTests(); advancedTileTests(); lyricsTests(); plannerTests();
  std::cout << cases << " cases, " << assertions << " assertions, " << failures << " failures\n";
  if (!cases) { std::cerr << "Filter matched no tests\n"; return 2; }
  return failures ? 1 : 0;
}
