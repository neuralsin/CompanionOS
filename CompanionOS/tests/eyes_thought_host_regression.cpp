#include "eyes_thought_host.h"
#include <iostream>

// Report failures in CI instead of opening the Windows CRT assertion dialog.
#undef assert
#define assert(expr) do { if (!(expr)) { std::cerr << "FAIL line " << __LINE__ << ": " << #expr << std::endl; std::exit(1); } } while (false)

#ifdef NDEBUG
#error "Regression assertions must not be disabled"
#endif

// Include the real production bodies; only their hardware environment is fake.
#include "../arduino/CompanionOS_Main/eyes.h"
#include "../arduino/CompanionOS_Main/thought_engine.h"
#include "../arduino/CompanionOS_Main/theme2_eyes.h"
#include "../arduino/CompanionOS_Main/theme3_eyes.h"

void push(const char* s) {
  std::strcpy(overrideThought,s);
  activeBubble.active=false; // companion_net transport behavior
  tickThoughtScheduler(&tft,false);
}
int main(){
  hostMillis=1000;push("persistent thought");
  auto shown=activeBubble.shownAt;
  assert(activeBubble.active && activeBubble.fadeAlpha==255);
  hostMillis=2000;push("persistent thought");
  assert(activeBubble.shownAt==shown);
  hostMillis=shown+THOUGHT_DISPLAY_MS;tickThoughtScheduler(&tft,false);
  assert(activeBubble.fadingOut && activeBubble.fadeAlpha==255);
  hostMillis+=500;tickThoughtScheduler(&tft,false);
  assert(activeBubble.fadeAlpha==127);
  auto a=activeBubble.fadeAlpha;tickThoughtScheduler(&tft,false);
  assert(activeBubble.fadeAlpha==a);
  hostMillis+=500;tickThoughtScheduler(&tft,false);
  assert(!activeBubble.active && activeBubble.fadeAlpha==0 && !activeBubble.fadingOut);
  push("persistent thought");assert(!activeBubble.active);

  // Wrapping consumes all 79 bytes, including long words, and pages all lines.
  std::string longText(79,'x');
  std::strcpy(activeBubble.text,longText.c_str());
  char lines[16][25];int n=wrapThoughtLines(lines);
  std::string rejoined;
  for(int i=0;i<n;i++){assert(std::strlen(lines[i])*6u<=THOUGHT_BOX_W-6);rejoined+=lines[i];}
  assert(rejoined==longText && n>3);
  std::strcpy(activeBubble.text,"Hi");
  n=wrapThoughtLines(lines);
  auto shortLayout=thoughtPageLayout(lines,n,0);
  assert(shortLayout.width==36 && shortLayout.height==12);
  assert(shortLayout.x+shortLayout.width/2==SCREEN_W*3/4);
  std::strcpy(activeBubble.text,longText.c_str());
  n=wrapThoughtLines(lines);
  auto longLayout=thoughtPageLayout(lines,n,0);
  assert(longLayout.width>shortLayout.width && longLayout.height==28);
  assert(longLayout.x>=6 && longLayout.height+9<=THOUGHT_FACE_TOP);
  std::strcpy(activeBubble.text,"one two three four five six seven eight nine ten eleven twelve thirteen fourteen");
  n=wrapThoughtLines(lines);rejoined="";
  for(int i=0;i<n;i++)for(char* p=lines[i];*p;p++)if(*p!=' ')rejoined+=*p;
  std::string expected;for(char* p=activeBubble.text;*p;p++)if(*p!=' ')expected+=*p;
  assert(rejoined==expected);
  for(int length=0;length<80;length++) {
    for(int pattern=0;pattern<9;pattern++) {
      std::string input;
      for(int i=0;i<length;i++)input+=(i%(pattern+2)==0?' ':char('a'+i%26));
      std::strcpy(activeBubble.text,input.c_str());
      n=wrapThoughtLines(lines);rejoined="";expected="";
      for(int i=0;i<n;i++) {
        assert(std::strlen(lines[i])*6u<=THOUGHT_BOX_W-6);
        for(char* p=lines[i];*p;p++)if(*p!=' ')rejoined+=*p;
      }
      for(char c:input)if(c!=' ')expected+=c;
      assert(rejoined==expected);
    }
  }
  std::strcpy(activeBubble.text,longText.c_str());startThought(hostMillis);
  tickThoughtScheduler(&tft,false);
  assert(thoughtDirectPage==0);
  hostMillis+=THOUGHT_PAGE_MS;tickThoughtScheduler(&tft,false);
  assert(thoughtDirectPage==1);

  // The face no longer erases the bubble on each blink or pupil movement.
  auto paints=thoughtDirectCanvas->pushes;
  tickThoughtScheduler(&tft,false);assert(thoughtDirectCanvas->pushes==paints);
  drawEyes();tickThoughtScheduler(&tft,false);assert(thoughtDirectCanvas->pushes==paints);
  // Composited themes cannot double-paint or consume another override in main.
  activeTheme=1;setThoughtSpriteCompositing(true);
  std::strcpy(overrideThought,"next frame");
  int ops=tft.ops;
  tickThoughtScheduler(&tft,false);
  assert(tft.ops==ops && overrideThought[0]);
  TFT_eSprite frame(&tft);frame.createSprite(SCREEN_W,SCREEN_H-16);
  tickThoughtScheduler(&frame,true);assert(!overrideThought[0]);
  frame.setTextDatum(4);frame.setTextSize(2);frame.setTextPadding(123);
  frame.setTextColor(1234,5678);tickThoughtScheduler(&frame,true);
  assert(frame.getTextDatum()==4 && frame.textsize==2 && frame.getTextPadding()==123);
  assert(frame.textcolor==1234 && frame.textbgcolor==5678);
  thoughtSchedulerActive=false;tickThoughtScheduler(&frame,true);
  assert(!activeBubble.active && !activeBubble.fadeAlpha && !activeBubble.fadingOut);
  thoughtSchedulerActive=true;

  hostMillis=0xfffffff0u;nextThoughtTime=hostMillis+100;activeBubble.active=false;
  tickThoughtScheduler(&frame,true);assert(!activeBubble.active);
  hostMillis+=100;tickThoughtScheduler(&frame,true);assert(activeBubble.active);
  activeBubble.shownAt=0xffff0000u;hostMillis=(uint32_t)activeBubble.shownAt+THOUGHT_DISPLAY_MS+500;
  updateThoughtLifetime(hostMillis);assert(activeBubble.fadeAlpha==127);
  hostMillis+=500;updateThoughtLifetime(hostMillis);assert(!activeBubble.active);

  // Eye sprite dimensions/origin stay constant across blink/emotion changes.
  activeTheme=0;releaseClassicEyeSprites();currentBlinkMod=1;drawNeutralEyes();
  int alloc=allocations,oldW=eyeSpr.w,oldH=eyeSpr.h;
  currentBlinkMod=0;drawSurprisedEyes();currentBlinkMod=1;drawSurprisedEyes();
  assert(allocations==alloc && eyeSpr.w==oldW && eyeSpr.h==oldH);
  // Allocation failures never mark an invalid sprite usable.
  releaseClassicEyeSprites();fail16=true;drawNeutralEyes();
  assert(eyeSprAllocated && eyeSpr.depth==8);
  releaseClassicEyeSprites();fail8=true;drawNeutralEyes();assert(!eyeSprAllocated);
  fail16=fail8=false;
  alloc=allocations;
  hostMillis+=1999;drawNeutralEyes();assert(allocations==alloc && !eyeSprAllocated);
  hostMillis+=1;drawNeutralEyes();assert(eyeSprAllocated);
  releaseClassicEyeSprites();
  companion_memory::testSnapshot={65536,40960};
  drawNeutralEyes();assert(!eyeSprAllocated);
  companion_memory::testSnapshot={256*1024,200*1024};
  hostMillis+=2000;drawNeutralEyes();assert(eyeSprAllocated);

  T2Eye eye;eye.ApplyPreset(T2P_Normal);eye.TransitionTo(T2P_Happy);
  hostMillis+=250;eye.Transition.Update();int half=eye.Config.Height;
  eye.Transition.Update();assert(eye.Config.Height==half && half==62);
  hostMillis+=250;eye.Transition.Update();assert(eye.Config.Height==25);
  eye.BlinkTransformation.Input=&eye.Config;
  eye.BlinkTransformation.Animation.Restart();
  hostMillis+=40;eye.BlinkTransformation.Update();assert(eye.BlinkTransformation.Output.Height==2);
  hostMillis+=140;eye.BlinkTransformation.Update();assert(eye.BlinkTransformation.Output.Height==25);

  activeTheme=2;t3_initEyes();t3_applyVariant(0);t3_idle=false;t3_autoblink=false;t3_curious=false;
  t3_eyeLhCur=t3_eyeLhNext=30;t3_eyeRhCur=t3_eyeRhNext=30;
  t3_eyeLx=t3_eyeLxNext=20;t3_eyeLy=t3_eyeLyNext=40;
  t3_hFlicker=true;t3_vFlicker=true;
  for(int i=0;i<20;i++){hostMillis+=50;t3_drawEyesInternal();}
  assert(t3_eyeLx==20 && t3_eyeLy==40);
  assert(t3_tween(29,30)==30 && t3_tween(30,29)==29);
  t3_setEmotion(EMO_SLEEPY);t3_blink();
  for(int i=0;i<20;i++)t3_drawEyesInternal();
  assert(t3_eyeLhNext==t3_scale(4) && t3_eyeLhCur==t3_scale(4));
  t3_setEmotion(EMO_LOVE);assert(t3_vFlicker);
  t3_setEmotion(EMO_NEUTRAL);assert(!t3_vFlicker && !t3_hFlicker);
  t3_applyVariant(4);assert(t3_getConstraintX()==SCREEN_W-t3_scale(40));
  activeTheme=1;t2_initEyes();t2_drawEyesPage();assert(t2CanvasPtr()->created());
  activeTheme=2;t3_initEyes();assert(!t2CanvasPtr() && t2_initialized);
  t3_drawEyesInternal();assert(t3CanvasPtr()->created());
  activeTheme=0;drawEyes();assert(!t2CanvasPtr() && !t3CanvasPtr());
  agentOverlayActive=true;
  assert(eyeRenderBottom()==SCREEN_H-UI_FOOTER_H-28);
  {EyeRenderViewport vp;assert(tft.clipBounds.y==16 && tft.clipBounds.h==eyeRenderBottom()-16);}
  assert(tft.getViewportY()==0 && tft.getViewportHeight()==SCREEN_H);
  flashNotifActive=true;assert(eyeRenderBottom()==SCREEN_H-28);
  flashNotifActive=agentOverlayActive=false;
  ldrEnabled=true;timeReceived=false;ldrValue=99;assert(getAutoEmotion()==EMO_SLEEPY);
  ldrValue=100;assert(getAutoEmotion()==EMO_NEUTRAL);
  ldrEnabled=false;
  hostMillis += 6000;
  hostAdcValue = 500;
  updateEyes();
  assert(ldrValue == 100); // Disabled sensor must not consume floating ADC data.
  ldrEnabled=true;
  hostMillis += 6000;
  updateEyes();
#ifdef ESP32
  assert(ldrValue == 125); // Real updateEyes() must match main's ADC >> 2.
#else
  assert(ldrValue == 500);
#endif
  ldrEnabled=false; timeReceived=false;
  manualEmotionOverride=false; petMoodLevel=0;
  lastInteractionTime=0; yawnStart=0; hostMillis=400000;
  tickIdleExpression(); assert(isYawning && currentEmotion==EMO_SLEEPY);
  hostMillis+=3000; tickIdleExpression();
  assert(!isYawning && currentEmotion==EMO_NEUTRAL);
  hostMillis+=1; tickIdleExpression(); assert(!isYawning);
  hostMillis+=300000; tickIdleExpression(); assert(isYawning);
  setEmotion(EMO_HAPPY); assert(!isYawning && manualEmotionOverride);
  hostMillis+=3600000; tickIdleExpression(); assert(currentEmotion==EMO_HAPPY);
  resumeAutoEmotion(); assert(!manualEmotionOverride && currentEmotion==EMO_NEUTRAL);
  applyAmbientEmotion(EMO_SLEEPY); wakeForManualAppearance();
  assert(manualEmotionOverride && currentEmotion==EMO_NEUTRAL);
  setEmotion(EMO_SLEEPY); wakeForManualAppearance();
  assert(currentEmotion==EMO_SLEEPY); // Explicitly requested sleep is not cancelled.

  // Real render paths must leave shared chrome and active overlays alone.
  auto checkProtectedWrites = [] {
    for (const auto& write : tft.writes) {
      assert(write.y >= UI_HEADER_H);
      assert(write.y + write.h <= eyeRenderBottom());
    }
  };
  agentOverlayActive = true;
  currentEmotion = EMO_NEUTRAL;
  tft.writes.clear(); drawEyes(); checkProtectedWrites();
  activeTheme = 1;
  t2_drawEyesPage();
  tft.writes.clear(); t2_drawEyesPage(); checkProtectedWrites();
  assert(t2CanvasPtr()->lastPushH == eyeRenderBottom() - UI_HEADER_H);
  activeTheme = 2; t3_initEyes();
  tft.writes.clear(); t3_drawEyesInternal(); checkProtectedWrites();
  assert(t3CanvasPtr()->lastPushH == eyeRenderBottom() - UI_HEADER_H - THOUGHT_FACE_TOP);
  flashNotifActive = true;
  tft.writes.clear(); t3_drawEyesInternal(); checkProtectedWrites();
  flashNotifActive = agentOverlayActive = false;
  tft.writes.clear(); t3_drawEyesInternal(); checkProtectedWrites();
  assert(t3CanvasPtr()->lastPushH == SCREEN_H - UI_HEADER_H - UI_FOOTER_H - THOUGHT_FACE_TOP);

  TFT_eSprite* failedCanvas=nullptr;
  hostMillis+=3000;fail16=fail8=true;
  assert(!prepareEyeCanvas(failedCanvas,SCREEN_W,SCREEN_H-16));
  alloc=allocations;
  assert(!prepareEyeCanvas(failedCanvas,SCREEN_W,SCREEN_H-16));
  assert(allocations==alloc);
  hostMillis+=2000;fail16=fail8=false;
  assert(prepareEyeCanvas(failedCanvas,SCREEN_W,SCREEN_H-16));
  delete failedCanvas;
  activeTheme = 0;
  drawEyes();
  releaseClassicEyeSprites();
  // Low SRAM/active HTTPS must not paint Classic inside either selected theme.
  companion_memory::testSnapshot={65536,38900};
  companion_memory::networkBusy.store(true);
  if(t2CanvasPtr()) { auto p=t2CanvasPtr();p->deleteSprite();delete p;t2CanvasSetNull(); }
  if(t3CanvasPtr()) { auto p=t3CanvasPtr();p->deleteSprite();delete p;t3CanvasSetNull(); }
  hostMillis+=3000;
  activeTheme=1;t2_initEyes();
  pupilRenderedX=12345;
  tft.writes.clear();t2_drawEyesPage();
  assert(pupilRenderedX==12345 && !eyeSprAllocated && !tft.writes.empty());
  assert(t2_initialized && (!t2CanvasPtr() || !t2CanvasPtr()->created()));
  int stableHeight=t2_leftEye.Config.Height;
  t2_drawEyesPage();assert(t2_leftEye.Config.Height==stableHeight);
  int priorOps=tft.ops;drawEyes();updateEyes();assert(tft.ops==priorOps);
  thoughtSchedulerActive=false;tickThoughtScheduler(&tft,false);
  priorOps=tft.ops;hostMillis+=33;t2_updateEyes();
  assert(tft.ops==priorOps); // No erase/redraw cycles while TLS owns the heap.
  setEmotion(EMO_ANGRY);assert(currentEmotion==EMO_ANGRY && pupilRenderedX==12345);
  activeTheme=2;t3_initEyes();
  tft.writes.clear();t3_drawEyesInternal();
  assert(pupilRenderedX==12345 && !eyeSprAllocated && !tft.writes.empty());
  assert(!t3CanvasPtr() || !t3CanvasPtr()->created());
  setEmotion(EMO_SLEEPY);assert(t3_tired && currentEmotion==EMO_SLEEPY && pupilRenderedX==12345);
  priorOps=tft.ops;t2_drawEyesPage();t2_updateEyes();assert(tft.ops==priorOps);
  priorOps=tft.ops;hostMillis+=33;t3_updateEyes();assert(tft.ops==priorOps);
  activeTheme=0;priorOps=tft.ops;t3_drawEyesInternal();t3_updateEyes();assert(tft.ops==priorOps);
  companion_memory::networkBusy.store(false);
  thoughtSchedulerActive=true;
  companion_memory::testSnapshot={256*1024,200*1024};
  hostMillis+=3000;
  activeTheme=1;t2_drawEyesPage();assert(t2CanvasPtr()->created() && pupilRenderedX==12345);
  activeTheme=2;t3_drawEyesInternal();assert(t3CanvasPtr()->created() && pupilRenderedX==12345);
  // The thought tail must point to the right-hand eye in both display sizes.
  TFT_eSPI probe;
  std::strcpy(activeBubble.text,"right-side thought");startThought(hostMillis);
  n=wrapThoughtLines(lines);paintThoughtPage(&probe,THOUGHT_BOX_X,THOUGHT_BOX_Y,lines,n,0);
  assert(!probe.triangles.empty() && probe.triangles.back().x > SCREEN_W/2);
  assert(probe.triangles.back().x+probe.triangles.back().w < SCREEN_W);
  // A composed bubble still needs erasing if TLS releases its owning frame.
  activeTheme=1;setThoughtSpriteCompositing(true);
  std::strcpy(activeBubble.text,"Composed thought");startThought(hostMillis);
  tickThoughtScheduler(&frame,true);assert(thoughtCompositedVisible);
  setThoughtSpriteCompositing(false);assert(thoughtDirectVisible);
  hostMillis+=thoughtDisplayMs()+THOUGHT_FADE_MS;
  tickThoughtScheduler(&tft,false);assert(!thoughtDirectVisible && !activeBubble.active);
  // The ten-minute lifetime and duplicate suppression survive millis rollover.
  activeTheme=2;hostMillis=0xffffe000U;
  push("Robo rollover thought");shown=activeBubble.shownAt;
  hostMillis+=thoughtDisplayMs();tickThoughtScheduler(&tft,false);
  assert(activeBubble.fadingOut && activeBubble.fadeAlpha==255);
  hostMillis+=1000;tickThoughtScheduler(&tft,false);assert(!activeBubble.active);
  push("Robo rollover thought");assert(!activeBubble.active && activeBubble.shownAt==shown);
  if(t3CanvasPtr()) { auto p=t3CanvasPtr();delete p;t3CanvasSetNull(); }
  companion_memory::testSnapshot={96*1024,40*1024};hostMillis+=3000;
  assert(t3_prepareCanvas() && t3CanvasPtr()->depth==1);
  assert(!companion_memory::permitsVisual(companion_memory::snapshot(),size_t(SCREEN_W)*T3_EYE_ZONE_H*2));
  currentEmotion=EMO_NEUTRAL;t3_applyVariant(0);
  for(int emotion=0;emotion<EMO_COUNT;++emotion) {
    t3_setEmotion(static_cast<Emotion>(emotion));
    const int center=t3_eyeLxNext+(t3_eyeLwNext+t3_spaceNext+t3_eyeRwNext)/2;
    assert(abs(center-SCREEN_W/2)<=1);
  }
  companion_memory::testSnapshot={256*1024,200*1024};
  // Time-scaled motion remains monotonic and a full blink finishes promptly.
  for (int dt : {8, 16, 33, 66}) {
    t3_motionMs = dt;
    assert(t3_tween(10, 80) > 10 && t3_tween(10, 80) <= 80);
    assert(t3_tween(80, 10) < 80 && t3_tween(80, 10) >= 10);
  }
  activeTheme=2; currentEmotion=EMO_NEUTRAL; t3_applyVariant(0);
  t3_idle=t3_autoblink=false;
  for (int i=0;i<30;++i) { hostMillis+=33;t3_drawEyesInternal(); }
  t3_blink(); bool blinkClosed=false, blinkRecovered=false;
  for (int i=0;i<10;++i) {
    hostMillis+=33;t3_drawEyesInternal();
    if(t3_eyeLhCur==1) blinkClosed=true;
    if(blinkClosed && t3_eyeLhCur==t3_eyeLhOpen) blinkRecovered=true;
  }
  assert(blinkClosed && blinkRecovered);
  activeTheme=0;drawEyes();releaseClassicEyeSprites();
  if (thoughtDirectCanvas) {
    delete thoughtDirectCanvas;
    thoughtDirectCanvas = nullptr;
  }
  std::cout<<"PASS: lifetime, duplicate pushes, pagination, invalidation, compositing, rollover, allocation fallback, geometry, transitions, blink and shake ("<<SCREEN_W<<"x"<<SCREEN_H<<", "
#ifdef ESP32
           <<"ESP32 ADC"
#else
           <<"ESP8266 ADC"
#endif
           <<")\n";
}
