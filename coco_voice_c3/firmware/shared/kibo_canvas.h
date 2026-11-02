// 128x64 monochrome canvas with clipped drawing primitives.
//
// The buffer uses the native SSD1306/SH1106 page layout (8 pages of 128 bytes,
// bit 0 = top pixel of the page), so it can be sent to the panel as-is.
// No ESP-IDF dependency: the PC simulator in sim/ compiles this file too.
#pragma once
#include <stdint.h>

#include "kibo_font.h"

namespace kibo {

constexpr int kWidth = 128;
constexpr int kHeight = 64;

class Canvas {
 public:
  uint8_t buf[kWidth * kHeight / 8];

  void clear();
  void setColor(int c) { color_ = c ? 1 : 0; }  // 1 = lit, 0 = erase
  int color() const { return color_; }

  void pixel(int x, int y);
  bool get(int x, int y) const;
  void hline(int x0, int x1, int y);
  void vline(int x, int y0, int y1);
  void rect(int x, int y, int w, int h);
  void frame(int x, int y, int w, int h);
  void roundRect(int x, int y, int w, int h, int r);
  void roundRectC(int cx, int cy, int w, int h, int r);  // centred
  void ellipse(int cx, int cy, int rx, int ry);
  void circle(int cx, int cy, int r) { ellipse(cx, cy, r, r); }
  void ring(int cx, int cy, int r, int thickness);
  void triangle(int x0, int y0, int x1, int y1, int x2, int y2);
  void line(int x0, int y0, int x1, int y1, int thickness = 1);
  void heart(int cx, int cy, int size);  // size ~ half width
  // Half elliptical rings: arcUp = "^" (happy eye), arcDown = "u" (smile / closed eye)
  void arcUp(int cx, int cy, int rx, int ry, int thickness);
  void arcDown(int cx, int cy, int rx, int ry, int thickness);
  void invert(int x, int y, int w, int h);

  // Text: y is the top of the glyph cell. Returns the drawn width.
  int text(int x, int y, const char* s, const Font& f);
  int textWidth(const char* s, const Font& f) const;
  void textCentered(int cx, int y, const char* s, const Font& f);
  // Word-wraps into lines of maxWidth pixels; returns number of lines drawn.
  int textWrapped(int x, int y, int maxWidth, int maxLines, const char* s, const Font& f, int lineGap = 1);

 private:
  int color_ = 1;
};

uint32_t hash32(uint32_t x);
int isin(int phase1024, int amplitude);  // sine, phase in 1/1024 turns
int clampi(int v, int lo, int hi);

}  // namespace kibo
