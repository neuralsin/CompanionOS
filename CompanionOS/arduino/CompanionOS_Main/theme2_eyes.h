#ifndef THEME2_EYES_H
#define THEME2_EYES_H

/*
 * ═══════════════════════════════════════════════════════════
 *  THEME 2 EYES — Ported from esp32-eyes-main (Playful Technology)
 *  Rewritten for TFT_eSPI on 320×240 color TFT (ESP8266)
 *
 *  Original: U8G2 128×64 OLED monochrome → white filled shapes
 *  This port: TFT_eSPI 320×240 → white shapes on black bg
 *
 *  Scale factors: X = 2.5, Y = 3.5 (approx)
 *  All classes inlined to avoid Arduino multi-file linking issues.
 * ═══════════════════════════════════════════════════════════
 */

#include "globals.h"
#include <math.h>

// ═══════════════════════════════════════════════════════════
// T2 EYE CONFIG STRUCT (renamed to avoid collision)
// ═══════════════════════════════════════════════════════════

struct T2EyeConfig {
  int16_t OffsetX;
  int16_t OffsetY;
  int16_t Height;
  int16_t Width;
  float   Slope_Top;
  float   Slope_Bottom;
  int16_t Radius_Top;
  int16_t Radius_Bottom;
  int16_t Inverse_Radius_Top;
  int16_t Inverse_Radius_Bottom;
  int16_t Inverse_Offset_Top;
  int16_t Inverse_Offset_Bottom;
};

// ═══════════════════════════════════════════════════════════
// T2 PRESETS (scaled from 40px base → 100px for 320×240)
// Scale: all dimensions × 2.5
// ═══════════════════════════════════════════════════════════

static const T2EyeConfig T2P_Normal        = { 0, 0, 100, 100, 0,    0,    20, 20, 0, 0, 0, 0 };
static const T2EyeConfig T2P_Happy         = { 0, 0,  25, 100, 0,    0,    25,  0, 0, 0, 0, 0 };
static const T2EyeConfig T2P_Glee          = { 0, 0,  20, 100, 0,    0,    20,  0, 0,12, 0, 0 };
static const T2EyeConfig T2P_Sad           = { 0, 0,  38, 100,-0.5,  0,     2, 25, 0, 0, 0, 0 };
static const T2EyeConfig T2P_Worried       = { 0, 0,  62, 100,-0.1,  0,    15, 25, 0, 0, 0, 0 };
static const T2EyeConfig T2P_Worried_A     = { 0, 0,  88, 100,-0.2,  0,    15, 25, 0, 0, 0, 0 };
static const T2EyeConfig T2P_Focused       = { 0, 0,  35, 100, 0.2,  0,     8,  2, 0, 0, 0, 0 };
static const T2EyeConfig T2P_Annoyed       = { 0, 0,  30, 100, 0,    0,     0, 25, 0, 0, 0, 0 };
static const T2EyeConfig T2P_Annoyed_A     = { 0, 0,  12, 100, 0,    0,     0, 10, 0, 0, 0, 0 };
static const T2EyeConfig T2P_Surprised     = {-5, 0, 112, 112, 0,    0,    40, 40, 0, 0, 0, 0 };
static const T2EyeConfig T2P_Skeptic       = { 0, 0, 100, 100, 0,    0,    25, 25, 0, 0, 0, 0 };
static const T2EyeConfig T2P_Skeptic_A     = { 0,-15, 65, 100, 0.3,  0,     2, 25, 0, 0, 0, 0 };
static const T2EyeConfig T2P_Frustrated    = { 8,-12, 30, 100, 0,    0,     0, 25, 0, 0, 0, 0 };
static const T2EyeConfig T2P_Unimpressed   = { 8,  0, 30, 100, 0,    0,     2, 25, 0, 0, 0, 0 };
static const T2EyeConfig T2P_Unimpressed_A = { 8, -8, 55, 100, 0,    0,     2, 40, 0, 0, 0, 0 };
static const T2EyeConfig T2P_Sleepy        = { 0,-5,  35, 100,-0.5, -0.5,   8,  8, 0, 0, 0, 0 };
static const T2EyeConfig T2P_Sleepy_A      = { 0,-5,  20, 100,-0.5, -0.5,   8,  8, 0, 0, 0, 0 };
static const T2EyeConfig T2P_Suspicious    = { 0,  0, 55, 100, 0,    0,    20,  8, 0, 0, 0, 0 };
static const T2EyeConfig T2P_Suspicious_A  = { 0, -8, 40, 100, 0.2,  0,    15,  8, 0, 0, 0, 0 };
static const T2EyeConfig T2P_Squint        = {-25,-8, 88,  88, 0,    0,    20, 20, 0, 0, 0, 0 };
static const T2EyeConfig T2P_Squint_A      = { 12, 0, 50,  50, 0,    0,    12, 12, 0, 0, 0, 0 };
static const T2EyeConfig T2P_Angry         = {-8, 0,  50, 100, 0.3,  0,     5, 30, 0, 0, 0, 0 };
static const T2EyeConfig T2P_Furious       = {-5, 0,  75, 100, 0.4,  0,     5, 20, 0, 0, 0, 0 };
static const T2EyeConfig T2P_Scared        = {-8, 0, 100, 100,-0.1,  0,    30, 20, 0, 0, 0, 0 };
static const T2EyeConfig T2P_Awe           = { 5, 0,  88, 112,-0.1,  0.1,  30, 30, 0, 0, 0, 0 };

// ═══════════════════════════════════════════════════════════
// T2 ANIMATION BASE (simplified from Animations.h)
// ═══════════════════════════════════════════════════════════

class T2Anim {
public:
  unsigned long Interval;
  unsigned long StarTime;

  T2Anim(unsigned long interval = 500) : Interval(interval), StarTime(millis()) {}
  void Restart() { StarTime = millis(); }
  unsigned long GetElapsed() { return (unsigned long)(millis() - StarTime); }

  float GetValue() {
    unsigned long e = GetElapsed();
    if (e >= Interval) return 1.0f;
    return (float)e / (float)Interval;
  }
};

// Trapezium pulse for variations
class T2PulseAnim {
public:
  unsigned long _t0, _t1, _t2, _t3, _t4;
  unsigned long Interval;
  unsigned long StarTime;

  T2PulseAnim() : _t0(200),_t1(200),_t2(200),_t3(200),_t4(0), Interval(800), StarTime(millis()) {}
  void Restart() { StarTime = millis(); }
  unsigned long GetElapsed() { return (unsigned long)(millis() - StarTime); }

  void SetTriangle(uint16_t t, uint16_t delay) {
    _t0 = 0; _t1 = t/2; _t2 = 0; _t3 = _t1; _t4 = delay;
    Interval = _t0+_t1+_t2+_t3+_t4;
  }

  float GetValue() {
    if (Interval == 0) return 0.0f;
    unsigned long elapsed = GetElapsed() % Interval;
    if (elapsed < _t0) return 0.0f;
    if (elapsed < _t0+_t1) return (float)(elapsed-_t0)/(float)_t1;
    if (elapsed < _t0+_t1+_t2) return 1.0f;
    if (elapsed < _t0+_t1+_t2+_t3) return 1.0f-(float)(elapsed-_t2-_t1-_t0)/(float)_t3;
    return 0.0f;
  }
};

// ═══════════════════════════════════════════════════════════
// T2 ASYNC TIMER
// ═══════════════════════════════════════════════════════════

class T2Timer {
public:
  unsigned long _interval;
  unsigned long _start;
  T2Timer(unsigned long interval = 3500) : _interval(interval), _start(millis()) {}
  void Start() { _start = millis(); }
  void Reset() { _start = millis(); }
  void SetIntervalMillis(unsigned long ms) { _interval = ms; }
  void Update() {} // placeholder for API compat
  bool IsExpired() { return (millis() - _start) >= _interval; }
};

// ═══════════════════════════════════════════════════════════
// T2 EYE DRAWER (ported from EyeDrawer.h — u8g2 → tft)
// ═══════════════════════════════════════════════════════════

enum T2Corner { T2_TR, T2_TL, T2_BL, T2_BR };

class T2EyeDrawer {
public:
  static TFT_eSprite* t2Spr;

  static void Draw(int16_t centerX, int16_t centerY, T2EyeConfig *cfg, uint16_t fg = TFT_WHITE) {
    if (!t2Spr) {
      t2Spr = new TFT_eSprite(&tft);
      t2Spr->setColorDepth(1);
      t2Spr->createSprite(140, 140);
    }
    t2Spr->setBitmapColor(fg, COLOR_BG);
    t2Spr->fillSprite(0);

    // Sprite local center
    int16_t scx = 70;
    int16_t scy = 70;

    int32_t dyt = cfg->Height * cfg->Slope_Top / 2.0;
    int32_t dyb = cfg->Height * cfg->Slope_Bottom / 2.0;
    auto totalH = cfg->Height + dyt - dyb;

    int16_t rT = cfg->Radius_Top;
    int16_t rB = cfg->Radius_Bottom;
    if (rB > 0 && rT > 0 && totalH - 1 < rB + rT) {
      int16_t crt = (float)rT * (totalH-1) / (rB+rT);
      int16_t crb = (float)rB * (totalH-1) / (rB+rT);
      rT = crt; rB = crb;
    }

    int32_t TLy = scy - cfg->Height/2 + rT - dyt;
    int32_t TLx = scx - cfg->Width/2  + rT;
    int32_t TRy = scy - cfg->Height/2 + rT + dyt;
    int32_t TRx = scx + cfg->Width/2  - rT;
    int32_t BLy = scy + cfg->Height/2 - rB - dyb;
    int32_t BLx = scx - cfg->Width/2  + rB;
    int32_t BRy = scy + cfg->Height/2 - rB + dyb;
    int32_t BRx = scx + cfg->Width/2  - rB;

    int32_t minCx = min(TLx, BLx);
    int32_t maxCx = max(TRx, BRx);
    int32_t minCy = min(TLy, TRy);
    int32_t maxCy = max(BLy, BRy);

    // Center fill
    FillRect(minCx, minCy, maxCx, maxCy, 1);
    // Extend to meet rounded corners
    FillRect(TRx, TRy, BRx + rB, BRy, 1);
    FillRect(TLx - rT, TLy, BLx, BLy, 1);
    FillRect(TLx, TLy - rT, TRx, TRy, 1);
    FillRect(BLx, BLy, BRx, BRy + rB, 1);

    // Slanted edges (top)
    if (cfg->Slope_Top > 0) {
      FillTri(TLx, TLy-rT, TRx, TRy-rT, 0);
      FillTri(TRx, TRy-rT, TLx, TLy-rT, 1);
    } else if (cfg->Slope_Top < 0) {
      FillTri(TRx, TRy-rT, TLx, TLy-rT, 0);
      FillTri(TLx, TLy-rT, TRx, TRy-rT, 1);
    }
    // Slanted edges (bottom)
    if (cfg->Slope_Bottom > 0) {
      FillTri(BRx+rB, BRy+rB, BLx-rB, BLy+rB, 0);
      FillTri(BLx-rB, BLy+rB, BRx+rB, BRy+rB, 1);
    } else if (cfg->Slope_Bottom < 0) {
      FillTri(BLx-rB, BLy+rB, BRx+rB, BRy+rB, 0);
      FillTri(BRx+rB, BRy+rB, BLx-rB, BLy+rB, 1);
    }

    // Rounded corners
    if (rT > 0) {
      FillCorner(T2_TL, TLx, TLy, rT, rT, 1);
      FillCorner(T2_TR, TRx, TRy, rT, rT, 1);
    }
    if (rB > 0) {
      FillCorner(T2_BL, BLx, BLy, rB, rB, 1);
      FillCorner(T2_BR, BRx, BRy, rB, rB, 1);
    }

    t2Spr->pushSprite(centerX + cfg->OffsetX - scx, centerY + cfg->OffsetY - scy);
  }

  static void FillRect(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t color) {
    int32_t l = min(x0,x1), r = max(x0,x1);
    int32_t t = min(y0,y1), b = max(y0,y1);
    if (r-l > 0 && b-t > 0) t2Spr->fillRect(l, t, r-l, b-t, color);
  }

  static void FillTri(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t color) {
    t2Spr->fillTriangle(x0, y0, x1, y1, x1, y0, color);
  }

  static void FillCorner(T2Corner corner, int16_t x0, int16_t y0, int32_t rx, int32_t ry, uint16_t color) {
    if (rx < 2 || ry < 2) return;
    int32_t rx2 = rx*rx, ry2 = ry*ry;
    int32_t fx2 = 4*rx2, fy2 = 4*ry2;
    int32_t x, y, s;

    if (corner == T2_TR) {
      for (x=0, y=ry, s=2*ry2+rx2*(1-2*ry); ry2*x<=rx2*y; x++) {
        t2Spr->drawFastHLine(x0, y0-y, x, color);
        if (s>=0) { s+=fx2*(1-y); y--; } s+=ry2*((4*x)+6);
      }
      for (x=rx, y=0, s=2*rx2+ry2*(1-2*rx); rx2*y<=ry2*x; y++) {
        t2Spr->drawFastHLine(x0, y0-y, x, color);
        if (s>=0) { s+=fy2*(1-x); x--; } s+=rx2*((4*y)+6);
      }
    } else if (corner == T2_BR) {
      for (x=0, y=ry, s=2*ry2+rx2*(1-2*ry); ry2*x<=rx2*y; x++) {
        t2Spr->drawFastHLine(x0, y0+y-1, x, color);
        if (s>=0) { s+=fx2*(1-y); y--; } s+=ry2*((4*x)+6);
      }
      for (x=rx, y=0, s=2*rx2+ry2*(1-2*rx); rx2*y<=ry2*x; y++) {
        t2Spr->drawFastHLine(x0, y0+y-1, x, color);
        if (s>=0) { s+=fy2*(1-x); x--; } s+=rx2*((4*y)+6);
      }
    } else if (corner == T2_TL) {
      for (x=0, y=ry, s=2*ry2+rx2*(1-2*ry); ry2*x<=rx2*y; x++) {
        t2Spr->drawFastHLine(x0-x, y0-y, x, color);
        if (s>=0) { s+=fx2*(1-y); y--; } s+=ry2*((4*x)+6);
      }
      for (x=rx, y=0, s=2*rx2+ry2*(1-2*rx); rx2*y<=ry2*x; y++) {
        t2Spr->drawFastHLine(x0-x, y0-y, x, color);
        if (s>=0) { s+=fy2*(1-x); x--; } s+=rx2*((4*y)+6);
      }
    } else if (corner == T2_BL) {
      for (x=0, y=ry, s=2*ry2+rx2*(1-2*ry); ry2*x<=rx2*y; x++) {
        t2Spr->drawFastHLine(x0-x, y0+y-1, x, color);
        if (s>=0) { s+=fx2*(1-y); y--; } s+=ry2*((4*x)+6);
      }
      for (x=rx, y=0, s=2*rx2+ry2*(1-2*rx); rx2*y<=ry2*x; y++) {
        t2Spr->drawFastHLine(x0-x, y0+y, x, color);
        if (s>=0) { s+=fy2*(1-x); x--; } s+=rx2*((4*y)+6);
      }
    }
  }
};
TFT_eSprite* T2EyeDrawer::t2Spr = nullptr;

// ═══════════════════════════════════════════════════════════
// T2 EYE TRANSITION ENGINE (interpolates between presets)
// ═══════════════════════════════════════════════════════════

struct T2Transformation {
  int16_t MoveX = 0;
  int16_t MoveY = 0;
  float ScaleX = 1.0;
  float ScaleY = 1.0;
};

class T2EyeTransition {
public:
  T2EyeConfig *Origin;
  T2EyeConfig Destin;
  T2Anim Animation;
  T2EyeTransition() : Origin(nullptr), Animation(500) {}

  void Update() {
    if (!Origin) return;
    float t = Animation.GetValue();
    Apply(t);
  }
  void Apply(float t) {
    if (!Origin) return;
    Origin->OffsetX = Origin->OffsetX*(1.0-t) + Destin.OffsetX*t;
    Origin->OffsetY = Origin->OffsetY*(1.0-t) + Destin.OffsetY*t;
    Origin->Height  = Origin->Height*(1.0-t)  + Destin.Height*t;
    Origin->Width   = Origin->Width*(1.0-t)   + Destin.Width*t;
    Origin->Slope_Top = Origin->Slope_Top*(1.0-t) + Destin.Slope_Top*t;
    Origin->Slope_Bottom = Origin->Slope_Bottom*(1.0-t) + Destin.Slope_Bottom*t;
    Origin->Radius_Top = Origin->Radius_Top*(1.0-t) + Destin.Radius_Top*t;
    Origin->Radius_Bottom = Origin->Radius_Bottom*(1.0-t) + Destin.Radius_Bottom*t;
  }
};

class T2EyeTransformation {
public:
  T2EyeConfig *Input;
  T2EyeConfig Output;
  T2Anim Animation;
  T2Transformation Origin, Destin, Current;

  T2EyeTransformation() : Input(nullptr), Animation(200) {
    Current.ScaleX = 1.0; Current.ScaleY = 1.0;
    Origin.ScaleX = 1.0; Origin.ScaleY = 1.0;
    Destin.ScaleX = 1.0; Destin.ScaleY = 1.0;
  }

  void SetDestin(T2Transformation tr) {
    Origin = Current;
    Destin = tr;
  }

  void Update() {
    float t = Animation.GetValue();
    Current.MoveX = (Destin.MoveX - Origin.MoveX)*t + Origin.MoveX;
    Current.MoveY = (Destin.MoveY - Origin.MoveY)*t + Origin.MoveY;
    Current.ScaleX = (Destin.ScaleX - Origin.ScaleX)*t + Origin.ScaleX;
    Current.ScaleY = (Destin.ScaleY - Origin.ScaleY)*t + Origin.ScaleY;
    Apply();
  }

  void Apply() {
    if (!Input) return;
    Output.OffsetX = Input->OffsetX + Current.MoveX;
    Output.OffsetY = Input->OffsetY - Current.MoveY;
    Output.Width   = Input->Width * Current.ScaleX;
    Output.Height  = Input->Height * Current.ScaleY;
    Output.Slope_Top = Input->Slope_Top;
    Output.Slope_Bottom = Input->Slope_Bottom;
    Output.Radius_Top = Input->Radius_Top;
    Output.Radius_Bottom = Input->Radius_Bottom;
  }
};

// Variation: subtle breathing animation on eye dimensions
class T2EyeVariation {
public:
  T2EyeConfig *Input;
  T2EyeConfig Output;
  T2EyeConfig Values; // delta values to oscillate
  T2PulseAnim Animation;

  T2EyeVariation() : Input(nullptr) { memset(&Values, 0, sizeof(Values)); }
  void Clear() { memset(&Values, 0, sizeof(Values)); }

  void Update() {
    float t = Animation.GetValue();
    if (Animation.GetElapsed() > Animation.Interval) t = 0.0;
    if (!Input) return;
    Output.OffsetX = Input->OffsetX + Values.OffsetX * t;
    Output.OffsetY = Input->OffsetY + Values.OffsetY * t;
    Output.Width   = Input->Width   + Values.Width * t;
    Output.Height  = Input->Height  + Values.Height * t;
    Output.Slope_Top = Input->Slope_Top;
    Output.Slope_Bottom = Input->Slope_Bottom;
    Output.Radius_Top = Input->Radius_Top;
    Output.Radius_Bottom = Input->Radius_Bottom;
  }
};

// Blink: squashes eye to flat line
class T2EyeBlink {
public:
  T2EyeConfig *Input;
  T2EyeConfig Output;
  T2Anim Animation; // 40+100+40 ms total
  int16_t BlinkWidth = 100;
  int16_t BlinkHeight = 2;

  T2EyeBlink() : Input(nullptr), Animation(180) {}

  void Update() {
    float t = Animation.GetValue();
    if (Animation.GetElapsed() > Animation.Interval) t = 0.0;
    Apply(t*t);
  }
  void Apply(float t) {
    if (!Input) return;
    Output.OffsetX = Input->OffsetX;
    Output.OffsetY = Input->OffsetY;
    Output.Width   = (BlinkWidth - Input->Width)*t + Input->Width;
    Output.Height  = (BlinkHeight - Input->Height)*t + Input->Height;
    Output.Slope_Top = Input->Slope_Top*(1.0-t);
    Output.Slope_Bottom = Input->Slope_Bottom*(1.0-t);
    Output.Radius_Top = Input->Radius_Top*(1.0-t);
    Output.Radius_Bottom = Input->Radius_Bottom*(1.0-t);
  }
};

// ═══════════════════════════════════════════════════════════
// T2 EYE — Single eye with full pipeline
// ═══════════════════════════════════════════════════════════

class T2Eye {
public:
  uint16_t CenterX, CenterY;
  bool IsMirrored;
  T2EyeConfig Config;
  T2EyeConfig *FinalConfig;

  T2EyeTransition Transition;
  T2EyeTransformation Transformation;
  T2EyeVariation Variation1, Variation2;
  T2EyeBlink BlinkTransformation;

  T2Eye() : CenterX(0), CenterY(0), IsMirrored(false) {
    Config = T2P_Normal;
    ChainOperators();
    Variation1.Animation._t0 = 200; Variation1.Animation._t1 = 200;
    Variation1.Animation._t2 = 200; Variation1.Animation._t3 = 200;
    Variation1.Animation._t4 = 0;   Variation1.Animation.Interval = 800;
    Variation2.Animation._t0 = 0;   Variation2.Animation._t1 = 200;
    Variation2.Animation._t2 = 200; Variation2.Animation._t3 = 200;
    Variation2.Animation._t4 = 200; Variation2.Animation.Interval = 800;
  }

  void ChainOperators() {
    Transition.Origin = &Config;
    Transformation.Input = &Config;
    Variation1.Input = &(Transformation.Output);
    Variation2.Input = &(Variation1.Output);
    BlinkTransformation.Input = &(Variation2.Output);
    FinalConfig = &(BlinkTransformation.Output);
  }

  void Update() {
    Transition.Update();
    Transformation.Update();
    Variation1.Update();
    Variation2.Update();
    BlinkTransformation.Update();
  }

  void Draw() {
    Update();
    T2EyeDrawer::Draw(CenterX, CenterY, FinalConfig);
  }

  void ApplyPreset(const T2EyeConfig &preset) {
    Config.OffsetX = IsMirrored ? -preset.OffsetX : preset.OffsetX;
    Config.OffsetY = -preset.OffsetY;
    Config.Height  = preset.Height;
    Config.Width   = preset.Width;
    Config.Slope_Top = IsMirrored ? preset.Slope_Top : -preset.Slope_Top;
    Config.Slope_Bottom = IsMirrored ? preset.Slope_Bottom : -preset.Slope_Bottom;
    Config.Radius_Top = preset.Radius_Top;
    Config.Radius_Bottom = preset.Radius_Bottom;
    Transition.Animation.Restart();
  }

  void TransitionTo(const T2EyeConfig &preset) {
    Transition.Destin.OffsetX = IsMirrored ? -preset.OffsetX : preset.OffsetX;
    Transition.Destin.OffsetY = -preset.OffsetY;
    Transition.Destin.Height  = preset.Height;
    Transition.Destin.Width   = preset.Width;
    Transition.Destin.Slope_Top = IsMirrored ? preset.Slope_Top : -preset.Slope_Top;
    Transition.Destin.Slope_Bottom = IsMirrored ? preset.Slope_Bottom : -preset.Slope_Bottom;
    Transition.Destin.Radius_Top = preset.Radius_Top;
    Transition.Destin.Radius_Bottom = preset.Radius_Bottom;
    Transition.Animation.Restart();
  }
};

// ═══════════════════════════════════════════════════════════
// T2 FACE — The whole face with two eyes + behavior
// ═══════════════════════════════════════════════════════════

// Layout for 320×240 screen (eyes centered in content area below status bar)
#define T2_SCREEN_CX   160
#define T2_SCREEN_CY   130  // Slightly above center to leave room for status
#define T2_EYE_SIZE    100
#define T2_EYE_GAP     10   // Inter-eye distance

T2Eye t2_leftEye;
T2Eye t2_rightEye;
T2Timer t2_blinkTimer(4000);
T2Timer t2_lookTimer(4000);
T2Timer t2_behaviorTimer(8000);
bool t2_randomBlink = true;
bool t2_randomLook = true;
bool t2_randomBehavior = true;
bool t2_initialized = false;

// Current T2 expression index for cycling
int t2_expressionIdx = 0;

// ── Look At ──
void t2_lookAt(float x, float y) {
  T2Transformation tr;
  int16_t moveX = -25 * x * 2.5; // scaled
  int16_t moveY = 20 * y * 3.5;
  float scaleY_x = 1.0 - x * 0.2;
  float scaleY_y = 1.0 - (y > 0 ? y : -y) * 0.4;

  tr.MoveX = moveX; tr.MoveY = moveY;
  tr.ScaleX = 1.0; tr.ScaleY = scaleY_x * scaleY_y;
  t2_rightEye.Transformation.SetDestin(tr);
  t2_rightEye.Transformation.Animation.Restart();

  scaleY_x = 1.0 + x * 0.2;
  tr.ScaleY = scaleY_x * scaleY_y;
  t2_leftEye.Transformation.SetDestin(tr);
  t2_leftEye.Transformation.Animation.Restart();
}

// ── Clear Variations ──
void t2_clearVariations() {
  t2_rightEye.Variation1.Clear();
  t2_rightEye.Variation2.Clear();
  t2_leftEye.Variation1.Clear();
  t2_leftEye.Variation2.Clear();
  t2_rightEye.Variation1.Animation.Restart();
  t2_leftEye.Variation1.Animation.Restart();
}

// ── Expression transitions (all 18 emotions) ──
void t2_goTo_Normal() {
  t2_clearVariations();
  t2_rightEye.Variation1.Values.Height = 8;
  t2_rightEye.Variation2.Values.Width = 2;
  t2_leftEye.Variation1.Values.Height = 5;
  t2_leftEye.Variation2.Values.Width = 5;
  t2_rightEye.Variation1.Animation.SetTriangle(1000, 0);
  t2_leftEye.Variation1.Animation.SetTriangle(1000, 0);
  t2_rightEye.TransitionTo(T2P_Normal);
  t2_leftEye.TransitionTo(T2P_Normal);
}

void t2_goTo_Happy() {
  t2_clearVariations();
  t2_rightEye.TransitionTo(T2P_Happy);
  t2_leftEye.TransitionTo(T2P_Happy);
}

void t2_goTo_Glee() {
  t2_clearVariations();
  t2_rightEye.Variation1.Values.OffsetY = 12;
  t2_leftEye.Variation1.Values.OffsetY = 12;
  t2_rightEye.Variation1.Animation.SetTriangle(300, 0);
  t2_leftEye.Variation1.Animation.SetTriangle(300, 0);
  t2_rightEye.TransitionTo(T2P_Glee);
  t2_leftEye.TransitionTo(T2P_Glee);
}

void t2_goTo_Sad() {
  t2_clearVariations();
  t2_rightEye.TransitionTo(T2P_Sad);
  t2_leftEye.TransitionTo(T2P_Sad);
}

void t2_goTo_Angry() {
  t2_clearVariations();
  t2_rightEye.Variation1.Values.OffsetY = 5;
  t2_leftEye.Variation1.Values.OffsetY = 5;
  t2_rightEye.Variation1.Animation.SetTriangle(300, 0);
  t2_leftEye.Variation1.Animation.SetTriangle(300, 0);
  t2_rightEye.TransitionTo(T2P_Angry);
  t2_leftEye.TransitionTo(T2P_Angry);
}

void t2_goTo_Surprised() {
  t2_clearVariations();
  t2_rightEye.TransitionTo(T2P_Surprised);
  t2_leftEye.TransitionTo(T2P_Surprised);
}

void t2_goTo_Sleepy() {
  t2_clearVariations();
  t2_rightEye.TransitionTo(T2P_Sleepy);
  t2_leftEye.TransitionTo(T2P_Sleepy_A);
}

void t2_goTo_Awe() {
  t2_clearVariations();
  t2_rightEye.TransitionTo(T2P_Awe);
  t2_leftEye.TransitionTo(T2P_Awe);
}

void t2_goTo_Furious() {
  t2_clearVariations();
  t2_rightEye.TransitionTo(T2P_Furious);
  t2_leftEye.TransitionTo(T2P_Furious);
}

void t2_goTo_Scared() {
  t2_clearVariations();
  t2_rightEye.TransitionTo(T2P_Scared);
  t2_leftEye.TransitionTo(T2P_Scared);
}

void t2_goTo_Worried() {
  t2_clearVariations();
  t2_rightEye.TransitionTo(T2P_Worried);
  t2_leftEye.TransitionTo(T2P_Worried_A);
}

void t2_goTo_Focused() {
  t2_clearVariations();
  t2_rightEye.TransitionTo(T2P_Focused);
  t2_leftEye.TransitionTo(T2P_Focused);
}

void t2_goTo_Annoyed() {
  t2_clearVariations();
  t2_rightEye.TransitionTo(T2P_Annoyed);
  t2_leftEye.TransitionTo(T2P_Annoyed_A);
}

void t2_goTo_Skeptic() {
  t2_clearVariations();
  t2_rightEye.TransitionTo(T2P_Skeptic);
  t2_leftEye.TransitionTo(T2P_Skeptic_A);
}

void t2_goTo_Frustrated() {
  t2_clearVariations();
  t2_rightEye.TransitionTo(T2P_Frustrated);
  t2_leftEye.TransitionTo(T2P_Frustrated);
}

void t2_goTo_Unimpressed() {
  t2_clearVariations();
  t2_rightEye.TransitionTo(T2P_Unimpressed);
  t2_leftEye.TransitionTo(T2P_Unimpressed_A);
}

void t2_goTo_Suspicious() {
  t2_clearVariations();
  t2_rightEye.TransitionTo(T2P_Suspicious);
  t2_leftEye.TransitionTo(T2P_Suspicious_A);
}

void t2_goTo_Squint() {
  t2_clearVariations();
  t2_leftEye.Variation1.Values.OffsetX = 15;
  t2_leftEye.Variation2.Values.OffsetY = 15;
  t2_rightEye.TransitionTo(T2P_Squint);
  t2_leftEye.TransitionTo(T2P_Squint_A);
}

// ═══════════════════════════════════════════════════════════
// PUBLIC API — called from pages.h / CompanionOS_Main.ino
// ═══════════════════════════════════════════════════════════

void t2_initEyes() {
  t2_leftEye.IsMirrored = true;
  t2_leftEye.ChainOperators();
  t2_rightEye.ChainOperators();
  t2_leftEye.ApplyPreset(T2P_Normal);
  t2_rightEye.ApplyPreset(T2P_Normal);
  t2_blinkTimer.Start();
  t2_lookTimer.Start();
  t2_behaviorTimer.Start();
  t2_initialized = true;
}

void t2_setEmotion(Emotion emo) {
  currentEmotion = emo;
  switch(emo) {
    case EMO_HAPPY:     t2_goTo_Happy(); break;
    case EMO_SAD:       t2_goTo_Sad(); break;
    case EMO_EXCITED:   t2_goTo_Surprised(); break;
    case EMO_LOVE:      t2_goTo_Glee(); break;
    case EMO_SLEEPY:    t2_goTo_Sleepy(); break;
    case EMO_ANGRY:     t2_goTo_Angry(); break;
    case EMO_SURPRISED: t2_goTo_Awe(); break;
    default:            t2_goTo_Normal(); break;
  }
  if (currentState == STATE_EYES) {
    tft.fillRect(0, 16, SCREEN_W, SCREEN_H - 16, COLOR_BG);
  }
}

// ── Cycle all 18 expressions for taps ──
void t2_nextExpression() {
  t2_expressionIdx = (t2_expressionIdx + 1) % 18;
  switch (t2_expressionIdx) {
    case 0: t2_goTo_Normal(); break;
    case 1: t2_goTo_Happy(); break;
    case 2: t2_goTo_Glee(); break;
    case 3: t2_goTo_Sad(); break;
    case 4: t2_goTo_Worried(); break;
    case 5: t2_goTo_Focused(); break;
    case 6: t2_goTo_Annoyed(); break;
    case 7: t2_goTo_Surprised(); break;
    case 8: t2_goTo_Skeptic(); break;
    case 9: t2_goTo_Frustrated(); break;
    case 10: t2_goTo_Unimpressed(); break;
    case 11: t2_goTo_Sleepy(); break;
    case 12: t2_goTo_Suspicious(); break;
    case 13: t2_goTo_Squint(); break;
    case 14: t2_goTo_Angry(); break;
    case 15: t2_goTo_Furious(); break;
    case 16: t2_goTo_Scared(); break;
    case 17: t2_goTo_Awe(); break;
  }
  if (currentState == STATE_EYES) {
    tft.fillRect(0, 16, SCREEN_W, SCREEN_H - 16, COLOR_BG);
  }
}

void t2_drawEyesPage() {
  if (!t2_initialized) t2_initEyes();
  tft.fillRect(0, 16, SCREEN_W, SCREEN_H - 16, COLOR_BG);

  t2_leftEye.CenterX  = T2_SCREEN_CX - T2_EYE_SIZE/2 - T2_EYE_GAP;
  t2_leftEye.CenterY  = T2_SCREEN_CY;
  t2_rightEye.CenterX = T2_SCREEN_CX + T2_EYE_SIZE/2 + T2_EYE_GAP;
  t2_rightEye.CenterY = T2_SCREEN_CY;

  t2_leftEye.Draw();
  t2_rightEye.Draw();
}

void t2_updateEyes() {
  if (currentState != STATE_EYES) return;
  if (!t2_initialized) t2_initEyes();

  // Random look
  if (t2_randomLook && t2_lookTimer.IsExpired()) {
    t2_lookTimer.Reset();
    float x = (float)random(-50, 50) / 100.0;
    float y = (float)random(-50, 50) / 100.0;
    t2_lookAt(x, y);
  }

  // Random blink
  if (t2_randomBlink && t2_blinkTimer.IsExpired()) {
    t2_leftEye.BlinkTransformation.Animation.Restart();
    t2_rightEye.BlinkTransformation.Animation.Restart();
    t2_blinkTimer.Reset();
  }

  // Clear and redraw (now handled by double buffering)
  // tft.fillRect(0, 16, SCREEN_W, SCREEN_H - 16, COLOR_BG);

  t2_leftEye.CenterX  = T2_SCREEN_CX - T2_EYE_SIZE/2 - T2_EYE_GAP;
  t2_leftEye.CenterY  = T2_SCREEN_CY;
  t2_rightEye.CenterX = T2_SCREEN_CX + T2_EYE_SIZE/2 + T2_EYE_GAP;
  t2_rightEye.CenterY = T2_SCREEN_CY;

  t2_leftEye.Draw();
  t2_rightEye.Draw();
}

#endif
