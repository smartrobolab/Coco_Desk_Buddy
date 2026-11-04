#include "kibo_canvas.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace kibo {

void Canvas::clear() { memset(buf, 0, sizeof(buf)); }

void Canvas::pixel(int x, int y) {
  if ((unsigned)x >= (unsigned)kWidth || (unsigned)y >= (unsigned)kHeight) return;
  uint8_t mask = (uint8_t)(1u << (y & 7));
  uint8_t& b = buf[(y >> 3) * kWidth + x];
  if (color_) b |= mask;
  else b &= (uint8_t)~mask;
}

bool Canvas::get(int x, int y) const {
  if ((unsigned)x >= (unsigned)kWidth || (unsigned)y >= (unsigned)kHeight) return false;
  return (buf[(y >> 3) * kWidth + x] >> (y & 7)) & 1;
}

void Canvas::hline(int x0, int x1, int y) {
  if ((unsigned)y >= (unsigned)kHeight) return;
  if (x0 > x1) {
    int t = x0;
    x0 = x1;
    x1 = t;
  }
  if (x1 < 0 || x0 >= kWidth) return;
  if (x0 < 0) x0 = 0;
  if (x1 >= kWidth) x1 = kWidth - 1;
  uint8_t mask = (uint8_t)(1u << (y & 7));
  uint8_t* row = &buf[(y >> 3) * kWidth];
  if (color_) {
    for (int x = x0; x <= x1; x++) row[x] |= mask;
  } else {
    for (int x = x0; x <= x1; x++) row[x] &= (uint8_t)~mask;
  }
}

void Canvas::vline(int x, int y0, int y1) {
  if (y0 > y1) {
    int t = y0;
    y0 = y1;
    y1 = t;
  }
  for (int y = y0; y <= y1; y++) pixel(x, y);
}

void Canvas::rect(int x, int y, int w, int h) {
  for (int yy = y; yy < y + h; yy++) hline(x, x + w - 1, yy);
}

void Canvas::frame(int x, int y, int w, int h) {
  hline(x, x + w - 1, y);
  hline(x, x + w - 1, y + h - 1);
  vline(x, y, y + h - 1);
  vline(x + w - 1, y, y + h - 1);
}

void Canvas::roundRect(int x, int y, int w, int h, int r) {
  if (w <= 0 || h <= 0) return;
  if (r * 2 > w) r = w / 2;
  if (r * 2 > h) r = h / 2;
  for (int i = 0; i < h; i++) {
    int inset = 0;
    int d = 0;
    if (i < r) d = r - i;
    else if (i >= h - r) d = i - (h - r - 1);
    if (d > 0) {
      float dy = (float)d - 0.5f;
      float dx = sqrtf((float)(r * r) - dy * dy);
      inset = r - (int)(dx + 0.5f);
    }
    hline(x + inset, x + w - 1 - inset, y + i);
  }
}

void Canvas::roundRectC(int cx, int cy, int w, int h, int r) { roundRect(cx - w / 2, cy - h / 2, w, h, r); }

void Canvas::ellipse(int cx, int cy, int rx, int ry) {
  if (rx <= 0 || ry <= 0) {
    if (rx >= 0 && ry >= 0) pixel(cx, cy);
    return;
  }
  for (int dy = -ry; dy <= ry; dy++) {
    float f = 1.0f - ((float)dy * dy) / ((float)ry * ry + 0.5f);
    if (f < 0) continue;
    int half = (int)(rx * sqrtf(f) + 0.5f);
    hline(cx - half, cx + half, cy + dy);
  }
}

void Canvas::ring(int cx, int cy, int r, int t) {
  int ri = r - t;
  for (int dy = -r; dy <= r; dy++) {
    int outer = (int)(sqrtf((float)(r * r - dy * dy)) + 0.5f);
    if (ri <= 0 || dy * dy >= ri * ri) {
      hline(cx - outer, cx + outer, cy + dy);
    } else {
      int inner = (int)(sqrtf((float)(ri * ri - dy * dy)) + 0.5f);
      hline(cx - outer, cx - inner, cy + dy);
      hline(cx + inner, cx + outer, cy + dy);
    }
  }
}

void Canvas::triangle(int x0, int y0, int x1, int y1, int x2, int y2) {
  int t;
  if (y1 < y0) { t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }
  if (y2 < y0) { t = y0; y0 = y2; y2 = t; t = x0; x0 = x2; x2 = t; }
  if (y2 < y1) { t = y1; y1 = y2; y2 = t; t = x1; x1 = x2; x2 = t; }
  if (y2 == y0) {
    int mn = x0 < x1 ? x0 : x1, mx = x0 > x1 ? x0 : x1;
    if (x2 < mn) mn = x2;
    if (x2 > mx) mx = x2;
    hline(mn, mx, y0);
    return;
  }
  for (int y = y0; y <= y2; y++) {
    float xa = x0 + (float)(x2 - x0) * (y - y0) / (float)(y2 - y0);
    float xb;
    if (y < y1) xb = x0 + (float)(x1 - x0) * (y - y0) / (float)(y1 - y0);
    else xb = (y2 == y1) ? (float)x1 : x1 + (float)(x2 - x1) * (y - y1) / (float)(y2 - y1);
    hline((int)lroundf(xa), (int)lroundf(xb), y);
  }
}

void Canvas::line(int x0, int y0, int x1, int y1, int t) {
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    if (t <= 1) pixel(x0, y0);
    else rect(x0 - t / 2, y0 - t / 2, t, t);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

void Canvas::heart(int cx, int cy, int s) {
  if (s < 2) {
    pixel(cx, cy);
    return;
  }
  int r = (s + 1) / 2;
  int top = cy - r / 2;
  circle(cx - r + 1, top, r);
  circle(cx + r - 1, top, r);
  triangle(cx - s, top + 1, cx + s, top + 1, cx, cy + s);
}

static void halfRing(Canvas& c, int cx, int cy, int rx, int ry, int t, bool upper) {
  int rxi = rx - t, ryi = ry - t;
  for (int k = 0; k <= ry; k++) {
    int dy = upper ? -k : k;
    float fo = 1.0f - (float)(k * k) / (float)(ry * ry);
    int outer = (int)(rx * sqrtf(fo < 0 ? 0 : fo) + 0.5f);
    if (ryi <= 0 || k >= ryi) {
      c.hline(cx - outer, cx + outer, cy + dy);
    } else {
      float fi = 1.0f - (float)(k * k) / (float)(ryi * ryi);
      int inner = (int)(rxi * sqrtf(fi < 0 ? 0 : fi) + 0.5f);
      c.hline(cx - outer, cx - inner, cy + dy);
      c.hline(cx + inner, cx + outer, cy + dy);
    }
  }
}

void Canvas::arcUp(int cx, int cy, int rx, int ry, int t) { halfRing(*this, cx, cy, rx, ry, t, true); }
void Canvas::arcDown(int cx, int cy, int rx, int ry, int t) { halfRing(*this, cx, cy, rx, ry, t, false); }

void Canvas::invert(int x, int y, int w, int h) {
  for (int yy = y; yy < y + h; yy++)
    for (int xx = x; xx < x + w; xx++) {
      if ((unsigned)xx >= (unsigned)kWidth || (unsigned)yy >= (unsigned)kHeight) continue;
      buf[(yy >> 3) * kWidth + xx] ^= (uint8_t)(1u << (yy & 7));
    }
}

static int glyphIndex(const Font& f, char ch) {
  int i = (uint8_t)ch - f.first;
  if (i < 0 || i >= f.count) i = '?' - f.first;
  if (i < 0 || i >= f.count) i = 0;
  return i;
}

int Canvas::text(int x, int y, const char* s, const Font& f) {
  int x0 = x;
  for (; *s; s++) {
    if ((uint8_t)*s >= 0x80) {
      // UTF-8 (e.g. Chinese) is not in our bitmap fonts; skip continuation bytes.
      if (((uint8_t)*s & 0xC0) != 0x80) {
        frame(x, y + 1, 4, f.height - 2);
        x += 5;
      }
      continue;
    }
    int gi = glyphIndex(f, *s);
    int w = f.widths[gi];
    const uint8_t* g = f.bitmap + gi * f.height * f.bytesPerRow;
    for (int row = 0; row < f.height; row++) {
      for (int col = 0; col < w; col++) {
        if (g[row * f.bytesPerRow + (col >> 3)] & (0x80 >> (col & 7))) pixel(x + col, y + row);
      }
    }
    x += w;
  }
  return x - x0;
}

int Canvas::textWidth(const char* s, const Font& f) const {
  int w = 0;
  for (; *s; s++) {
    if ((uint8_t)*s >= 0x80) {
      if (((uint8_t)*s & 0xC0) != 0x80) w += 5;
      continue;
    }
    w += f.widths[glyphIndex(f, *s)];
  }
  return w;
}

void Canvas::textCentered(int cx, int y, const char* s, const Font& f) { text(cx - textWidth(s, f) / 2, y, s, f); }

int Canvas::textWrapped(int x, int y, int maxWidth, int maxLines, const char* s, const Font& f, int lineGap) {
  char line[64];
  int lines = 0;
  while (*s && lines < maxLines) {
    while (*s == ' ') s++;
    int len = 0, lastSpace = -1, width = 0;
    while (s[len] && s[len] != '\n' && len < (int)sizeof(line) - 1) {
      char tmp[2] = {s[len], 0};
      int cw = textWidth(tmp, f);
      if (width + cw > maxWidth) break;
      if (s[len] == ' ') lastSpace = len;
      width += cw;
      len++;
    }
    int take = len;
    if (s[len] && s[len] != '\n' && lastSpace > 0) take = lastSpace;
    if (take == 0) take = len > 0 ? len : 1;
    memcpy(line, s, take);
    line[take] = 0;
    text(x, y + lines * (f.height + lineGap), line, f);
    lines++;
    s += take;
    if (*s == '\n') s++;
  }
  return lines;
}

uint32_t hash32(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352dU;
  x ^= x >> 15;
  x *= 0x846ca68bU;
  x ^= x >> 16;
  return x;
}

int isin(int phase, int amp) { return (int)lroundf(sinf((float)(phase & 1023) * (6.2831853f / 1024.0f)) * (float)amp); }

int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

}  // namespace kibo
