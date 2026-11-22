#ifndef COMPANION_TEST_HOST_TFT_H
#define COMPANION_TEST_HOST_TFT_H

#include "host_arduino.h"
#include <fstream>
#include <stdexcept>
#include <vector>
#include <memory>

// Compile the installed tables directly; do not vendor a second set of metrics.
#define PROGMEM
namespace host_fonts {
#include <Fonts/glcdfont.c>
#include <Fonts/Font16.h>
#include <Fonts/Font32rle.h>
}
#undef PROGMEM

struct HostRect {
  int x, y, w, h;
  bool empty() const { return w <= 0 || h <= 0; }
  bool contains(int px, int py) const {
    return px >= x && py >= y && px < x + w && py < y + h;
  }
  bool contains(HostRect b) const {
    return b.empty() || (b.x >= x && b.y >= y && b.x + b.w <= x + w && b.y + b.h <= y + h);
  }
};
inline HostRect intersect(HostRect a, HostRect b) {
  const int x = max(a.x, b.x), y = max(a.y, b.y);
  return {x, y, max(0, min(a.x + a.w, b.x + b.w) - x),
                 max(0, min(a.y + a.h, b.y + b.h) - y)};
}
struct HostTextCall {
  std::string text;
  HostRect measured, clip;
  int font;
  uint16_t fg, bg;
};

class HostTFT {
  int width_, height_, datum_ = 0, size_ = 1, textFont_ = 1;
  int originX_ = 0, originY_ = 0, logicalW_, logicalH_;
  bool vpDatum_ = false;
  HostRect clip_;
  uint16_t fg_ = 0xFFFF, bg_ = 0xFFFF;
  int advance(unsigned char c, int f) const {
    if (f == 1) return 6;
    const unsigned char* table = f == 2 ? host_fonts::widtbl_f16 : host_fonts::widtbl_f32;
    return table[(c >= 32 && c < 128) ? c - 32 : 0];
  }
  void pixel(int x, int y, uint16_t c) {
    x += originX_; y += originY_;
    if (!clip_.contains(x, y)) return;
    pixels[static_cast<std::size_t>(y) * width_ + x] = c;
    ++pixelWrites;
    if (enforceWriteBounds && !writeBounds.contains(x, y)) ++escapedWrites;
  }
  void glyph(unsigned char c, int x, int y, int f) {
    const int w = advance(c, f), h = fontHeight(f) / size_;
    if (fg_ != bg_) fillRect(x, y, w * size_, h * size_, bg_);
    if (c < 32 || c >= 128) return; // Unsupported glyphs are not Unicode fonts.
    auto dot = [&](int dx, int dy) {
      for (int sy = 0; sy < size_; ++sy)
        for (int sx = 0; sx < size_; ++sx) pixel(x + dx * size_ + sx, y + dy * size_ + sy, fg_);
    };
    if (f == 1) {
      for (int col = 0; col < 5; ++col) {
        const unsigned char bits = host_fonts::font[c * 5 + col];
        for (int row = 0; row < 8; ++row) if (bits & (1 << row)) dot(col, row);
      }
    } else if (f == 2) {
      const unsigned char* data = host_fonts::chrtbl_f16[c - 32];
      const int stride = (w + 6) / 8;
      for (int row = 0; row < h; ++row)
        for (int col = 0; col < w; ++col)
          if (col < stride * 8 && (data[row * stride + col / 8] & (0x80 >> (col % 8)))) dot(col, row);
    } else {
      const unsigned char* data = host_fonts::chrtbl_f32[c - 32];
      int pos = 0;
      while (pos < w * h) {
        const unsigned char run = *data++;
        const int end = min(w * h, pos + (run & 127) + 1);
        while (pos < end) {
          if (run & 128) dot(pos % w, pos / w);
          ++pos;
        }
      }
    }
  }
 public:
  uint32_t bitmap_fg = 65535, bitmap_bg = 0;
  void setBitmapColor(uint16_t fg, uint16_t bg) { bitmap_fg=fg; bitmap_bg=bg; }
  uint8_t textsize = 1;
  uint32_t textcolor = 0xFFFF, textbgcolor = 0xFFFF;
  uint16_t textPadding = 0;
  std::vector<uint16_t> pixels;
  std::vector<HostTextCall> texts;
  std::size_t pixelWrites = 0, escapedWrites = 0, primitiveCalls = 0;
  std::size_t imagePushes = 0;
  std::vector<HostRect> blackFills;
  bool enforceWriteBounds = false;
  HostRect writeBounds{0, 0, 0, 0};
  HostTFT(int w, int h) : width_(w), height_(h), logicalW_(w), logicalH_(h),
      clip_{0, 0, w, h}, pixels(static_cast<std::size_t>(w) * h) {}
  int width() const { return width_; }
  int height() const { return height_; }
  HostRect clip() const { return clip_; }
  void clearTrace(uint16_t color = 0x0841) {
    std::fill(pixels.begin(), pixels.end(), color);
    texts.clear(); blackFills.clear(); pixelWrites = escapedWrites = primitiveCalls = imagePushes = 0;
    enforceWriteBounds = false;
    size_ = textsize = 1; datum_ = 0; textFont_ = 1;
    fg_ = bg_ = textcolor = textbgcolor = 0xFFFF; textPadding = 0;
    resetViewport();
  }
  void clearCalls() { texts.clear(); blackFills.clear(); pixelWrites = escapedWrites = primitiveCalls = imagePushes = 0; }
  void setTextPadding(int value) { textPadding = value; }
  uint16_t getTextPadding() const { return textPadding; }
  void setTextWrap(bool, bool = false) {}
  void setSwapBytes(bool) {}
  void pushImage(int x, int y, int w, int h, const uint16_t* data) {
    ++imagePushes;
    for (int row=0; row<h; ++row)
      for (int col=0; col<w; ++col) pixel(x+col,y+row,data[row*w+col]);
  }
  void pushSurface(const HostTFT& source, int x, int y, int sx, int sy, int w, int h) {
    const int dx=x+originX_-sx, dy=y+originY_-sy;
    for (const auto& original:source.texts) {
      HostTextCall call=original;
      call.measured.x+=dx; call.measured.y+=dy;
      call.clip.x+=dx; call.clip.y+=dy;
      call.clip=intersect(call.clip,clip_);
      texts.push_back(call);
    }
    if (!sx && !sy && w==source.width_ && h==source.height_) {
      pushImage(x,y,w,h,source.pixels.data());
    } else {
      for (int row=0; row<h; ++row)
        pushImage(x,y+row,w,1,source.pixels.data()+(sy+row)*source.width_+sx);
    }
  }
  void pushSurface(const HostTFT& source, int x, int y) {
    pushSurface(source,x,y,0,0,source.width_,source.height_);
  }
  void setTextColor(uint16_t fg) { fg_ = bg_ = textcolor = textbgcolor = fg; }
  void setTextColor(uint16_t fg, uint16_t bg) { fg_ = textcolor = fg; bg_ = textbgcolor = bg; }
  void setTextSize(int size) { size_ = textsize = max(1, size); }
  void setTextFont(int font) { textFont_ = font; }
  void setTextDatum(int datum) { datum_ = datum; }
  int getTextDatum() const { return datum_; }
  int fontHeight(int f = 1) const {
    if (f != 1 && f != 2 && f != 4) throw std::runtime_error("Unsupported font " + std::to_string(f));
    return (f == 1 ? 8 : f == 2 ? chr_hgt_f16 : chr_hgt_f32) * size_;
  }
  int textWidth(const char* s, int f = 1) const {
    fontHeight(f);
    if (!s) throw std::runtime_error("Null string passed to TFT sink");
    int w = 0;
    for (; *s; ++s) w += advance(static_cast<unsigned char>(*s), f);
    return w * size_;
  }
  int textWidth(const String& s, int f = 1) const { return textWidth(s.c_str(), f); }
  int drawString(const char* s, int x, int y, int f) {
    const int w = textWidth(s, f), h = fontHeight(f);
    if (datum_ % 3 == 1) x -= w / 2;
    if (datum_ % 3 == 2) x -= w;
    if (datum_ >= 3 && datum_ <= 5) y -= h / 2;
    if (datum_ >= 6 && datum_ <= 8) y -= h;
    texts.push_back({s, {x + originX_, y + originY_, w, h}, clip_, f, fg_, bg_});
    // UTF-8 drawing itself is intentionally only approximate. All normalized
    // firmware output is ASCII; raw high bytes are diagnosed by the tests.
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(s); *p; ++p) {
      glyph(*p, x, y, f); x += advance(*p, f) * size_;
    }
    return w;
  }
  int drawString(const char* s, int x, int y) { return drawString(s, x, y, textFont_); }
  int drawString(const String& s, int x, int y, int f = 1) { return drawString(s.c_str(), x, y, f); }
  int drawCentreString(const String& s, int x, int y, int f) {
    const int old = datum_; datum_ = 0;
    const int w = drawString(s, x - textWidth(s, f) / 2, y, f); datum_ = old; return w;
  }
  int drawRightString(const String& s, int x, int y, int f) {
    const int old = datum_; datum_ = 0;
    const int w = drawString(s, x - textWidth(s, f), y, f); datum_ = old; return w;
  }
  // Match TFT_eSPI's logical getters, including false-datum width = screen width.
  void setViewport(int x, int y, int w, int h, bool datum = true) {
    clip_ = intersect({0, 0, width_, height_}, {x, y, w, h});
    vpDatum_ = datum;
    originX_ = datum ? x : 0; originY_ = datum ? y : 0;
    logicalW_ = datum ? w : width_; logicalH_ = datum ? h : height_;
  }
  void resetViewport() { setViewport(0, 0, width_, height_, false); }
  int getViewportX() const { return originX_; }
  int getViewportY() const { return originY_; }
  int getViewportWidth() const { return logicalW_; }
  int getViewportHeight() const { return logicalH_; }
  bool getViewportDatum() const { return vpDatum_; }
  void drawPixel(int x, int y, uint16_t c) { ++primitiveCalls; pixel(x, y, c); }
  void fillRect(int x, int y, int w, int h, uint16_t c) {
    ++primitiveCalls;
    // Intersect first: malformed firmware dimensions cannot stall a test loop.
    HostRect r = intersect({x + originX_, y + originY_, w, h}, clip_);
    if (c == 0 && !r.empty()) blackFills.push_back(r);
    for (int yy = r.y; yy < r.y + r.h; ++yy)
      for (int xx = r.x; xx < r.x + r.w; ++xx) pixel(xx - originX_, yy - originY_, c);
  }
  void fillScreen(uint16_t c) { fillRect(0, 0, width_, height_, c); }
  void drawFastHLine(int x, int y, int w, uint16_t c) { fillRect(x, y, w, 1, c); }
  void drawFastVLine(int x, int y, int h, uint16_t c) { fillRect(x, y, 1, h, c); }
  void drawRect(int x, int y, int w, int h, uint16_t c) {
    if (w <= 0 || h <= 0) return;
    drawFastHLine(x, y, w, c); drawFastHLine(x, y + h - 1, w, c);
    drawFastVLine(x, y, h, c); drawFastVLine(x + w - 1, y, h, c);
  }
  void drawLine(int x0, int y0, int x1, int y1, uint16_t c) {
    ++primitiveCalls;
    const int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (int budget = 0; budget < 100000; ++budget) {
      pixel(x0, y0, c);
      if (x0 == x1 && y0 == y1) return;
      const int e = 2 * err;
      if (e >= dy) { err += dy; x0 += sx; }
      if (e <= dx) { err += dx; y0 += sy; }
    }
    throw std::runtime_error("Unreasonable line coordinates");
  }
  void fillCircle(int cx, int cy, int r, uint16_t c) {
    ++primitiveCalls;
    for (int y = -r; y <= r; ++y)
      for (int x = -r; x <= r; ++x) if (x*x + y*y <= r*r) pixel(cx+x, cy+y, c);
  }
  void drawCircle(int cx, int cy, int r, uint16_t c) {
    ++primitiveCalls;
    for (int y = -r; y <= r; ++y)
      for (int x = -r; x <= r; ++x)
        if (std::abs(x*x + y*y - r*r) <= r) pixel(cx+x, cy+y, c);
  }
  void fillRoundRect(int x, int y, int w, int h, int r, uint16_t c) {
    if (w <= 0 || h <= 0) return;
    r = constrain(r, 0, min(w, h) / 2);
    for (int row = 0; row < h; ++row) {
      int inset = 0;
      if (r && (row < r || row >= h-r)) {
        int dy = row < r ? r-row-1 : row-(h-r);
        inset = r - static_cast<int>(std::sqrt(r*r - dy*dy));
      }
      drawFastHLine(x+inset, y+row, w-2*inset, c);
    }
  }
  void drawRoundRect(int x, int y, int w, int h, int r, uint16_t c) {
    if (w <= 0 || h <= 0) return;
    r = constrain(r, 0, min(w, h) / 2);
    for (int row = 0; row < h; ++row) {
      int inset = 0;
      if (r && (row < r || row >= h-r)) {
        int dy = row < r ? r-row-1 : row-(h-r);
        inset = r - static_cast<int>(std::sqrt(r*r - dy*dy));
      }
      if (row == 0 || row == h-1) drawFastHLine(x+inset, y+row, w-2*inset, c);
      else { pixel(x+inset, y+row, c); pixel(x+w-inset-1, y+row, c); }
    }
  }
  void fillTriangle(int ax, int ay, int bx, int by, int cx, int cy, uint16_t c) {
    ++primitiveCalls;
    auto edge = [](int x1,int y1,int x2,int y2,int x,int y) { return (x-x1)*(y2-y1)-(y-y1)*(x2-x1); };
    for (int y = max(0, min(ay, min(by, cy))); y <= min(height_-1, max(ay, max(by, cy))); ++y)
      for (int x = max(0, min(ax, min(bx, cx))); x <= min(width_-1, max(ax, max(bx, cx))); ++x) {
        int a=edge(ax,ay,bx,by,x,y), b=edge(bx,by,cx,cy,x,y), d=edge(cx,cy,ax,ay,x,y);
        if ((a>=0 && b>=0 && d>=0) || (a<=0 && b<=0 && d<=0)) pixel(x,y,c);
      }
  }
  void savePPM(const std::string& path) const {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot write " + path);
    out << "P6\n" << width_ << " " << height_ << "\n255\n";
    for (uint16_t c : pixels) {
      const unsigned char rgb[] = {static_cast<unsigned char>(((c >> 11) & 31) * 255 / 31),
        static_cast<unsigned char>(((c >> 5) & 63) * 255 / 63), static_cast<unsigned char>((c & 31) * 255 / 31)};
      out.write(reinterpret_cast<const char*>(rgb), 3);
    }
  }
};

static unsigned hostFailSpriteAllocations=0;
static unsigned hostSpriteCreates=0, hostSpriteDeletes=0;
class TFT_eSprite {
  HostTFT* parent_;
  std::unique_ptr<HostTFT> surface_;
  int depth_ = 16;
  uint16_t ink(uint16_t c) const { return depth_==1 ? (c ? 65535 : 0) : c; }
 public:
  uint8_t textsize = 1;
  uint32_t textcolor = 0xFFFF, textbgcolor = 0xFFFF;
  explicit TFT_eSprite(HostTFT* parent) : parent_(parent) {}
  void setColorDepth(int depth) {
    if (depth!=16 && depth!=8 && depth!=1) throw std::runtime_error("Unsupported sprite depth");
    depth_=depth;
  }
  void* createSprite(int w,int h) {
    ++hostSpriteCreates;
    if (hostFailSpriteAllocations) { --hostFailSpriteAllocations; return nullptr; }
    surface_.reset(new HostTFT(w,h));
    return surface_->pixels.data();
  }
  void deleteSprite() { ++hostSpriteDeletes; surface_.reset(); }
  bool created() const { return bool(surface_); }
  void fillSprite(uint16_t color) { surface_->clearCalls(); surface_->fillScreen(ink(color)); }
  void fillRect(int x,int y,int w,int h,uint16_t c) { surface_->fillRect(x,y,w,h,ink(c)); }
  void setTextSize(int n) { textsize = n; surface_->setTextSize(n); }
  void setTextDatum(int n) { surface_->setTextDatum(n); }
  int getTextDatum() const { return surface_->getTextDatum(); }
  void setTextPadding(int n) { surface_->setTextPadding(n); }
  uint16_t getTextPadding() const { return surface_->getTextPadding(); }
  void setViewport(int x,int y,int w,int h,bool d=true) { surface_->setViewport(x,y,w,h,d); }
  int fontHeight(int f) const { return surface_->fontHeight(f); }
  void setTextColor(uint16_t fg,uint16_t bg) { textcolor=fg;textbgcolor=bg;surface_->setTextColor(fg,bg); }
  void setTextColor(uint16_t fg) { setTextColor(fg,fg); }
  int drawString(const String& text,int x,int y,int f) { return surface_->drawString(text,x,y,f); }
  int drawRightString(const String& text,int x,int y,int f) { return surface_->drawRightString(text,x,y,f); }
  void fillCircle(int x,int y,int r,uint16_t c) { surface_->fillCircle(x,y,r,c); }
  void drawFastHLine(int x,int y,int w,uint16_t c) { surface_->drawFastHLine(x,y,w,c); }
  void drawFastVLine(int x,int y,int h,uint16_t c) { surface_->drawFastVLine(x,y,h,c); }
  void drawCircle(int x,int y,int r,uint16_t c) { surface_->drawCircle(x,y,r,c); }
  void drawPixel(int x,int y,uint16_t c) { surface_->drawPixel(x,y,c); }
  void drawRoundRect(int x,int y,int w,int h,int r,uint16_t c) { surface_->drawRoundRect(x,y,w,h,r,c); }
  void fillRoundRect(int x,int y,int w,int h,int r,uint16_t c) { surface_->fillRoundRect(x,y,w,h,r,ink(c)); }
  void fillTriangle(int x,int y,int a,int b,int u,int v,uint16_t c) { surface_->fillTriangle(x,y,a,b,u,v,ink(c)); }
  void drawLine(int x,int y,int a,int b,uint16_t c) { surface_->drawLine(x,y,a,b,c); }
  void pushSprite(int x,int y) { parent_->pushSurface(*surface_,x,y); surface_->texts.clear(); }
  bool pushSprite(int x,int y,int sx,int sy,int w,int h) {
    parent_->pushSurface(*surface_,x,y,sx,sy,w,h);surface_->texts.clear();return true;
  }
};

#endif
