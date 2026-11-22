#define COMPANION_EYES_RASTER
#include "eyes_thought_host.h"
#include "../arduino/CompanionOS_Main/eyes.h"
#include "../arduino/CompanionOS_Main/theme2_eyes.h"
#include "../arduino/CompanionOS_Main/theme3_eyes.h"
#include <iostream>

void releaseCanvases() {
  if (auto p=t2CanvasPtr()) { p->deleteSprite();delete p;t2CanvasSetNull(); }
  if (auto p=t3CanvasPtr()) { p->deleteSprite();delete p;t3CanvasSetNull(); }
  if (thoughtDirectCanvas) { delete thoughtDirectCanvas;thoughtDirectCanvas=nullptr; }
  releaseClassicEyeSprites();
  releaseEyeStrips();
}
void paint(int theme) {
  if(theme==0) { drawEyes();tickThoughtScheduler(&tft,false); }
  else if(theme==1) t2_drawEyesPage();
  else t3_drawEyesInternal();
}
void roboChecks(const std::string& output) {
  activeTheme=2; currentEmotion=EMO_NEUTRAL;
  companion_memory::networkBusy.store(false);
  companion_memory::testSnapshot={256*1024,200*1024};
  releaseCanvases(); hostMillis+=10000; t3_initEyes();
  t3_idle=t3_autoblink=false;
  thoughtSchedulerActive=true;
  strcpy(overrideThought,"A short visit, then back to the face.");
  t3_drawEyesInternal();
  const uint32_t shown=activeBubble.shownAt;
  assert(thoughtDisplayMs()==600000);
  for(int i=0;i<40;++i) { hostMillis+=33;t3_drawEyesInternal(); }
  // Robo apertures must be solid, not dark pupils surrounded by a white eye.
  for(int y=t3_eyeLy+t3_offsetY+t3_eyeLhCur/3;y<t3_eyeLy+t3_offsetY+t3_eyeLhCur*2/3;++y)
    for(int x=t3_eyeLx+t3_eyeLwCur/3;x<t3_eyeLx+t3_eyeLwCur*2/3;++x)
      assert(tft.pixels[y*SCREEN_W+x]==T3_EYE_COLOR);
  tft.clearCalls(); hostMillis+=33;t3_drawEyesInternal();
  assert(tft.pixelWrites==0); // Stable eyes and thought do not repaint at 30 FPS.
  strcpy(overrideThought,"A short visit, then back to the face.");
  activeBubble.active=false;hostMillis+=33;t3_drawEyesInternal();
  assert(activeBubble.shownAt==shown);
  const auto beforeExpiry=tft.pixels;
  releaseCanvases();
  companion_memory::testSnapshot={65536,38900};
  companion_memory::networkBusy.store(true);
  hostMillis=shown+thoughtDisplayMs()+THOUGHT_FADE_MS;
  t3_updateEyes();
  assert(!activeBubble.active);
  for(int y=UI_HEADER_H;y<UI_HEADER_H+THOUGHT_FACE_TOP;++y)
    for(int x=THOUGHT_BOX_X;x<THOUGHT_BOX_X+THOUGHT_BOX_W;++x)
      assert(tft.pixels[y*SCREEN_W+x]==COLOR_BG);
  for(int y=UI_HEADER_H+THOUGHT_FACE_TOP;y<SCREEN_H-UI_FOOTER_H;++y)
    for(int x=0;x<SCREEN_W;++x) assert(tft.pixels[y*SCREEN_W+x]==beforeExpiry[y*SCREEN_W+x]);
  strcpy(overrideThought,"A short visit, then back to the face.");
  hostMillis+=33;t3_updateEyes();assert(!activeBubble.active);
  companion_memory::networkBusy.store(false);
  thoughtSchedulerActive=false;
  int combinations=0;
  for(int low=0;low<2;++low) for(int variant=0;variant<T3_VARIANT_COUNT;++variant)
    for(int emotion=0;emotion<EMO_COUNT;++emotion) {
      releaseCanvases();hostMillis+=10000;
      companion_memory::testSnapshot=low ? companion_memory::Snapshot{65536,38900} : companion_memory::Snapshot{256*1024,200*1024};
      currentEmotion=static_cast<Emotion>(emotion);t3_applyVariant(variant);
      t3_idle=t3_autoblink=false;
      for(int frame=0;frame<32;++frame) {
        hostMillis+=33;t3_drawEyesInternal();
        assert(t3_frameKey[0]>=0 && t3_frameKey[0]+t3_frameKey[4]<=SCREEN_W);
        assert(t3_frameKey[1]>=UI_HEADER_H+THOUGHT_FACE_TOP);
        assert(t3_frameKey[1]+t3_frameKey[5]<=SCREEN_H-UI_FOOTER_H);
        if(!t3_cyclops) {
          assert(t3_frameKey[2]>=t3_frameKey[0]+t3_frameKey[4]+t3_spaceCur);
          assert(t3_frameKey[2]+t3_frameKey[6]<=SCREEN_W);
        }
      }
      if(!low && (variant==0 || emotion==EMO_NEUTRAL))
        tft.savePPM(output+"/robo-v"+std::to_string(variant)+"-m"+std::to_string(emotion)+".ppm");
      t3_blink(); bool closed=false;
      for(int frame=0;frame<40;++frame) {
        hostMillis+=33;t3_drawEyesInternal();
        if(t3_eyeLhCur<=1) closed=true;
      }
      assert(closed && t3_eyeLhNext==t3_eyeLhOpen && t3_eyeLhCur>=t3_eyeLhOpen);
      ++combinations;
    }
  std::cout<<"PASS Robo: "<<combinations<<" variant/mood/RAM combinations, blink recovery, TLS thought expiry and quiet frames\n";
}
int main(int argc,char** argv) {
  assert(argc==2);
  for(int theme=0;theme<3;++theme) {
    std::vector<uint16_t> buffered;
    for(int low=0;low<2;++low) {
      std::cout<<"Checking theme "<<theme+1<<", low RAM "<<low<<std::endl;
      releaseCanvases();hostMillis+=10000;
      companion_memory::testSnapshot=low ? companion_memory::Snapshot{65536,38900} : companion_memory::Snapshot{256*1024,200*1024};
      companion_memory::networkBusy.store(low);
      activeTheme=theme;currentEmotion=EMO_NEUTRAL;
      activeBubble.active=false;thoughtDirectVisible=false;thoughtSchedulerActive=false;
      if(theme==1) t2_initEyes();
      if(theme==2) t3_initEyes();
      hostMillis+=1000;
      t3_idle=t3_autoblink=false;
      tft.clearTrace(0);
      tft.fillRect(0,0,SCREEN_W,UI_HEADER_H,0x1234);
      tft.fillRect(0,SCREEN_H-UI_FOOTER_H,SCREEN_W,UI_FOOTER_H,0x4321);
      if(theme==0) drawStarfield();
      for(int settle=0;settle<32;++settle) paint(theme);
      int left=0,right=0;
      for(int y=0;y<SCREEN_H;++y) for(int x=0;x<SCREEN_W;++x) {
        uint16_t color=tft.pixels[y*SCREEN_W+x];
        if(y<UI_HEADER_H) assert(color==0x1234);
        else if(y>=SCREEN_H-UI_FOOTER_H) assert(color==0x4321);
        else if(y>=UI_HEADER_H+THOUGHT_FACE_TOP && color) { if(x<SCREEN_W/2) ++left;else ++right; }
      }
      assert(left>100 && right>100); // Clearing a rectangle is not a visible face.
      if(!low) buffered=tft.pixels;
      else if(buffered!=tft.pixels) {
        for(size_t i=0;i<buffered.size();++i) if(buffered[i]!=tft.pixels[i]) {
          std::cerr<<"First pixel difference at "<<i%SCREEN_W<<","<<i/SCREEN_W
                   <<": "<<buffered[i]<<" vs "<<tft.pixels[i]<<std::endl;break;
        }
        tft.savePPM(std::string(argv[1])+"/eyes-mismatch.ppm");
        assert(buffered==tft.pixels);
      }
      thoughtSchedulerActive=true;strcpy(activeBubble.text,"A thought above the right eye");
      activeBubble.active=true;activeBubble.shownAt=hostMillis-1000;activeBubble.fadeAlpha=255;
      activeBubble.fadingIn=activeBubble.fadingOut=false;invalidateThoughtBubble();
      paint(theme);
      const auto stable=tft.pixels;
      for(int frame=0;frame<15;++frame) {
        hostMillis+=33;
        tft.clearCalls();
        // Check the intermediate frame too, before a scheduler could repaint.
        if(theme==0) drawEyes(); else paint(theme);
        if (low && theme > 0) {
          const HostRect face{0,UI_HEADER_H+THOUGHT_FACE_TOP,SCREEN_W,
                              SCREEN_H-UI_FOOTER_H-UI_HEADER_H-THOUGHT_FACE_TOP};
          for (const auto& clear : tft.blackFills)
            assert(intersect(clear,face).empty()); // No visible erase before drawing.
          assert(eyeStripCanvas && eyeStripCanvas->created());
        }
        for(int y=THOUGHT_BOX_Y;y<THOUGHT_BOX_Y+THOUGHT_BOX_H+9;++y)
          for(int x=THOUGHT_BOX_X;x<THOUGHT_BOX_X+THOUGHT_BOX_W;++x)
            assert(tft.pixels[y*SCREEN_W+x]==stable[y*SCREEN_W+x]);
        if(theme==0) tickThoughtScheduler(&tft,false);
      }
      if(theme==0) for(int emotion=0;emotion<EMO_COUNT;++emotion) {
        setEmotion(static_cast<Emotion>(emotion));
        for(int y=THOUGHT_BOX_Y;y<THOUGHT_BOX_Y+THOUGHT_BOX_H+9;++y)
          for(int x=THOUGHT_BOX_X;x<THOUGHT_BOX_X+THOUGHT_BOX_W;++x)
            assert(tft.pixels[y*SCREEN_W+x]==stable[y*SCREEN_W+x]);
      }
      currentEmotion=EMO_NEUTRAL;paint(theme);
      // Switching to a short thought erases the previous wider/taller bubble.
      strcpy(activeBubble.text,"Hi");startThought(hostMillis);paint(theme);
      char wrapped[16][25];int count=wrapThoughtLines(wrapped);
      auto layout=thoughtPageLayout(wrapped,count,0);
      for(int y=THOUGHT_BOX_Y;y<THOUGHT_BOX_Y+THOUGHT_FACE_TOP;++y)
        for(int x=THOUGHT_BOX_X;x<layout.x;++x)
          assert(tft.pixels[y*SCREEN_W+x]==COLOR_BG);
      auto selectedFrame=tft.pixels;
      // Delayed updates from other themes cannot write over the selected face.
      if(theme!=0) { drawEyes();updateEyes(); }
      if(theme!=1) { t2_drawEyesPage();t2_updateEyes(); }
      if(theme!=2) { t3_drawEyesInternal();t3_updateEyes(); }
      assert(tft.pixels==selectedFrame);
      if(theme==1) {
        t2_leftEye.BlinkTransformation.Animation.Restart();
        t2_rightEye.BlinkTransformation.Animation.Restart();
        hostMillis+=80;paint(theme);
        int lidPixels=0;
        for(int y=UI_HEADER_H+THOUGHT_FACE_TOP;y<SCREEN_H-UI_FOOTER_H;++y)
          for(int x=0;x<SCREEN_W;++x) if(tft.pixels[y*SCREEN_W+x]) ++lidPixels;
        assert(lidPixels>0); // Closed lids must not round down to invisible eyes.
        hostMillis+=200;paint(theme);
      }
      tft.savePPM(std::string(argv[1])+"/eyes-theme"+std::to_string(theme+1)+(low?"-low-ram":"-buffered")+".ppm");
    }
  }
  for(int theme=0;theme<3;++theme) {
    activeTheme=theme;
    tft.fillRect(0,UI_HEADER_H,2,2,0x07E0);
    paint(theme);
    assert(tft.pixels[UI_HEADER_H*SCREEN_W]!=0x07E0);
  }
  roboChecks(argv[1]);
  releaseCanvases();
  std::cout<<"PASS raster eyes: 3 themes, identical low-RAM pixels, stable thoughts, protected header/footer ("<<SCREEN_W<<"x"<<SCREEN_H<<")\n";
}
