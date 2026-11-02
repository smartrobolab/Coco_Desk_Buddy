#include "kibo_faces.h"

#include <math.h>
#include <string.h>

namespace kibo {

const Face kTapFaces[kTapFaceCount] = {
    Face::Neutral, Face::Serious, Face::Sleepy,    Face::Crying, Face::Sad,   Face::Cute,
    Face::Kissou,  Face::MiniLove, Face::AnimeLove, Face::Fire,   Face::Angry,
};

namespace {

// Eye centres
constexpr int LX = 38;
constexpr int RX = 90;
constexpr int EY = 28;

// Picks the current frame of an animation from per-frame durations (ms).
int frameAt(uint32_t t, const uint16_t* durs, int n) {
  uint32_t total = 0;
  for (int i = 0; i < n; i++) total += durs[i];
  uint32_t p = total ? t % total : 0;
  for (int i = 0; i < n; i++) {
    if (p < durs[i]) return i;
    p -= durs[i];
  }
  return n - 1;
}

// Rounded-rect eye. `open` 0..100 squashes it vertically (blink); values above
// 100 stretch it (overshoot when the eye pops open). Squash & stretch: a closing
// eye gets a little wider, so blinks feel soft and bouncy instead of mechanical.
void eye(Canvas& c, int cx, int cy, int w, int h, int r, int open = 100) {
  int hh = h * open / 100;
  int ww = open < 100 ? w + (100 - open) * w / 400 : w - (open - 100) * w / 300;
  if (hh < 3) {
    c.roundRect(cx - ww / 2, cy - 1, ww, 3, 1);
    return;
  }
  c.roundRectC(cx, cy, ww, hh, r);
}

// Soft dithered blush (checkerboard ellipse) - reads as "pink cheeks" on 1-bit OLED.
void blushSpot(Canvas& c, int cx, int cy, int rx, int ry) {
  for (int dy = -ry; dy <= ry; dy++)
    for (int dx = -rx; dx <= rx; dx++)
      if (dx * dx * ry * ry + dy * dy * rx * rx <= rx * rx * ry * ry && ((cx + dx + cy + dy) & 1) == 0)
        c.pixel(cx + dx, cy + dy);
}

// 4-point twinkle star; size 0..4
void sparkle(Canvas& c, int x, int y, int size) {
  if (size <= 0) return;
  c.hline(x - size, x + size, y);
  c.vline(x, y - size, y + size);
  if (size >= 3) {
    c.pixel(x - 1, y - 1);
    c.pixel(x + 1, y - 1);
    c.pixel(x - 1, y + 1);
    c.pixel(x + 1, y + 1);
  }
}

// Rounded tear / sweat drop with its tip at the top.
void drop(Canvas& c, int x, int y, int r) {
  c.circle(x, y, r);
  c.triangle(x - r + 1, y - 1, x + r - 1, y - 1, x, y - r * 2 - 1);
}

// ---- hands -------------------------------------------------------------
// Hands are 8x8 pixel-art sprites drawn at 2x (16x16 px) with a dark outline,
// on little arms that come up from Kibo's body below the screen. Sprites are
// drawn for the LEFT hand; mirror = true gives the right hand.
enum class Pose { Open, Grip };

const char* const kOpenHand[8] = {  // round cartoon mitten with a thumb
    "..XXX...",
    ".XXXXX..",
    ".XXXXX.X",
    ".XXXXXXX",
    ".XXXXXX.",
    ".XXXXX..",
    "..XXXX..",
    "...XX...",
};
const char* const kGripHand[8] = {  // "C" grip holding the edge of something on the right
    "..XXX...",
    ".XXXXX..",
    "XXXX....",
    "XXX.....",
    "XXX.....",
    "XXXX....",
    ".XXXXX..",
    "..XXX...",
};

void sprite(Canvas& c, const char* const* rows, int x, int y, bool mirror, int scale = 2) {
  // x,y = centre of the sprite on screen
  int ox = x - 4 * scale, oy = y - 4 * scale;
  for (int pass = 0; pass < 2; pass++) {  // pass 0: dark outline, pass 1: lit pixels
    c.setColor(pass);
    for (int r = 0; r < 8; r++)
      for (int k = 0; k < 8; k++) {
        if (rows[r][mirror ? 7 - k : k] != 'X') continue;
        int px = ox + k * scale, py = oy + r * scale;
        if (pass == 0) c.rect(px - 1, py - 1, scale + 2, scale + 2);
        else c.rect(px, py, scale, scale);
      }
  }
  c.setColor(1);
}

void arm(Canvas& c, int x0, int y0, int x1, int y1) {
  // chubby stub arm: start at most ~16 px away from the hand
  int dx = x0 - x1, dy = y0 - y1;
  int len = (int)sqrtf((float)(dx * dx + dy * dy));
  if (len > 16) {
    x0 = x1 + dx * 16 / len;
    y0 = y1 + dy * 16 / len;
  }
  c.setColor(0);
  c.line(x0, y0, x1, y1, 7);
  c.setColor(1);
  c.line(x0, y0, x1, y1, 5);
}

// Hand at (x,y) on an arm from (bx,by). left = Kibo's left hand (screen left).
void hand(Canvas& c, int x, int y, bool left, Pose pose, int bx, int by) {
  arm(c, bx, by, x, y + 6);
  const char* const* rows = pose == Pose::Open ? kOpenHand : kGripHand;
  sprite(c, rows, x, y, !left);
}

// Twinkle size for sparkle i at time t (0..3, staggered)
int twinkle(uint32_t t, int i, uint32_t period = 1200) {
  uint32_t p = (t + i * 397) % period;
  int s = (int)(p * 8 / period);  // 0..7
  return s < 4 ? s : 7 - s;
}

// Anime "shiny" eye: lit eye, big dark pupil that follows the gaze, two lit highlights.
void shinyEye(Canvas& c, int cx, int cy, int w, int h, int gx, int gy) {
  c.roundRectC(cx, cy, w, h, w / 2 - 1);
  c.setColor(0);
  int px = cx + clampi(gx / 2, -3, 3), py = cy + 3 + clampi(gy / 2, -2, 2);
  c.ellipse(px, py, w / 2 - 6, h / 2 - 7);
  c.setColor(1);
  c.circle(px - 3, py - 4, 3);
  c.circle(px + 3, py + 3, 1);
}

// Erases a slanted "eyelid" over the top of an eye: depthL/depthR are how far
// the lid reaches down at the eye's left / right edge.
void lid(Canvas& c, int cx, int cy, int w, int h, int depthL, int depthR) {
  int x0 = cx - w / 2 - 2, x1 = cx + w / 2 + 2, top = cy - h / 2 - 2;
  c.setColor(0);
  c.triangle(x0, top, x1, top, x0, top + 2 + depthL);
  c.triangle(x1, top, x1, top + 2 + depthR, x0, top + 2 + depthL);
  c.setColor(1);
}

// Erases the lower part of an eye (smiling / squinting cheeks).
void cheek(Canvas& c, int cx, int cy, int w, int h, int amount) {
  c.setColor(0);
  c.ellipse(cx, cy + h / 2 + w / 2 - amount, w / 2 + 4, w / 2);
  c.setColor(1);
}

void happyEyes(Canvas& c, int dy = 0, int rx = 13, int ry = 12, int t = 5) {
  c.arcUp(LX, EY + 6 + dy, rx, ry, t);
  c.arcUp(RX, EY + 6 + dy, rx, ry, t);
}

void blush(Canvas& c, int y = EY + 18) {
  blushSpot(c, LX - 8, y + 2, 7, 3);
  blushSpot(c, RX + 8, y + 2, 7, 3);
}

void smile(Canvas& c, int cx, int cy, int rx = 8, int ry = 4) { c.arcDown(cx, cy, rx, ry, 2); }
void frown(Canvas& c, int cx, int cy, int rx = 8, int ry = 4) { c.arcUp(cx, cy, rx, ry, 2); }

void talkMouth(Canvas& c, const FaceCtx& x, int cy = 51) {
  int lv = x.level >= 0 ? x.level : 50 + isin((int)(x.t * 1024 / 260), 45) + isin((int)(x.t * 1024 / 170), 20);
  lv = clampi(lv, 0, 100);
  int ry = 1 + lv * 6 / 100;
  c.setColor(0);
  c.rect(54, cy - 8, 21, 15);
  c.setColor(1);
  c.ellipse(64, cy, 7 - ry / 3, ry);
}

void zzz(Canvas& c, uint32_t t, int baseX, int baseY) {
  for (int i = 0; i < 3; i++) {
    uint32_t p = (t + i * 1000) % 3000;
    int x = baseX + (int)(p / 120);
    int y = baseY - (int)(p / 110);
    c.text(x, y, p < 1500 ? "z" : "Z", p < 1000 ? kFontSmall : kFontMedium);
  }
}

void note(Canvas& c, int x, int y) {
  c.ellipse(x, y, 3, 2);
  c.vline(x + 3, y - 9, y);
  c.line(x + 3, y - 9, x + 6, y - 6, 1);
}

void spiral(Canvas& c, int cx, int cy, int rmax, float rot) {
  const float turns = 3.0f;
  const int steps = 60;
  int px = cx, py = cy;
  for (int i = 1; i <= steps; i++) {
    float a = (float)i / steps * turns * 6.2831853f;
    float r = (float)rmax * i / steps;
    int x = cx + (int)lroundf(cosf(a + rot) * r);
    int y = cy + (int)lroundf(sinf(a + rot) * r);
    c.line(px, py, x, y, 2);
    px = x;
    py = y;
  }
}

void heartEyes(Canvas& c, int size, int dy = 0) {
  c.heart(LX, EY + dy, size);
  c.heart(RX, EY + dy, size);
}

// ------------------------------------------------------------------ faces

void neutral(Canvas& c, const FaceCtx& x) {
  // calm, alive: rest, glance left, glance right, squash-blink, pop open
  static const uint16_t d[6] = {2200, 800, 800, 60, 100, 90};
  static const int8_t look[6] = {0, -7, 7, 0, 0, 0};
  static const uint8_t open[6] = {100, 100, 100, 40, 6, 110};
  int f = frameAt(x.t, d, 6);
  int breathe = isin((int)(x.t * 1024 / 3200), 1);
  int gx = x.lookX + look[f], gy = x.lookY - (f == 5 ? 1 : 0);
  // the eye on the side Kibo looks at is a bit closer -> slightly bigger
  int pl = clampi(-gx / 3, -2, 2), pr = clampi(gx / 3, -2, 2);
  eye(c, LX + gx, EY + gy, 28 + pl, 34 + breathe + pl, 9, open[f]);
  eye(c, RX + gx, EY + gy, 28 + pr, 34 + breathe + pr, 9, open[f]);
}

void serious(Canvas& c, const FaceCtx& x) {
  static const uint16_t d[6] = {900, 500, 900, 500, 120, 600};
  static const int8_t look[6] = {-7, 0, 7, 0, 0, 0};
  static const uint8_t open[6] = {100, 100, 100, 100, 35, 100};
  int f = frameAt(x.t, d, 6);
  int gx = x.lookX / 2 + look[f], gy = 3 + x.lookY / 2;
  eye(c, LX + gx, EY + gy, 32, 22, 5, open[f]);
  eye(c, RX + gx, EY + gy, 32, 22, 5, open[f]);
  lid(c, LX + gx, EY + gy, 32, 22, 2, 7);  // inner corners lower = focused
  lid(c, RX + gx, EY + gy, 32, 22, 7, 2);
  c.line(LX - 16 + gx, EY - 12, LX + 12 + gx, EY - 8, 3);  // brows
  c.line(RX - 12 + gx, EY - 8, RX + 16 + gx, EY - 12, 3);
}

void sleepy(Canvas& c, const FaceCtx& x) {
  // heavy-lidded: only the rounded bottom of each eye shows, lids droop outward
  eye(c, LX, EY + 4, 30, 30, 10);
  eye(c, RX, EY + 4, 30, 30, 10);
  c.setColor(0);
  c.rect(0, 0, kWidth, EY + 5);
  c.setColor(1);
  lid(c, LX, EY + 9, 30, 4, 5, 0);
  lid(c, RX, EY + 9, 30, 4, 0, 5);
  c.text(104, 6, "z", kFontSmall);
  c.text(112, 0, "Z", kFontMedium);
  // rubbing a sleepy eye
  int rub = isin((int)(x.t * 1024 / 500), 2);
  hand(c, RX + 15 + rub, EY + 16, false, Pose::Open, RX + 24, 74);
}

void sadEyes(Canvas& c, int gx, int gy, int h) {
  eye(c, LX + gx, EY + 4 + gy, 28, h, 9);
  eye(c, RX + gx, EY + 4 + gy, 28, h, 9);
  lid(c, LX + gx, EY + 4 + gy, 28, h, 11, 1);  // outer corners low, inner raised
  lid(c, RX + gx, EY + 4 + gy, 28, h, 1, 11);
}

void crying(Canvas& c, const FaceCtx&) {
  // squeezed-shut eyes with two waterfalls of tears
  c.arcUp(LX, EY + 4, 14, 7, 3);
  c.arcUp(RX, EY + 4, 14, 7, 3);
  c.line(LX - 15, EY - 6, LX - 4, EY - 10, 2);  // worried brows
  c.line(RX + 4, EY - 10, RX + 15, EY - 6, 2);
  c.roundRect(LX - 3, EY + 6, 6, 64 - (EY + 6), 3);
  c.roundRect(RX - 3, EY + 6, 6, 64 - (EY + 6), 3);
  c.circle(LX - 10, EY + 16, 2);
  c.circle(RX + 10, EY + 20, 2);
  frown(c, 64, 54, 8, 5);
}

void sad(Canvas& c, const FaceCtx& x) {
  sadEyes(c, x.lookX / 2, x.lookY / 2, 26);
  frown(c, 64, 56, 8, 4);
  // one tear wells up at the inner corner, then rolls down (2.4 s loop)
  uint32_t p = x.t % 2400;
  int tx = RX - 11 + x.lookX / 2;
  if (p < 1200) drop(c, tx, EY + 19, 1 + (int)(p * 2 / 1200));
  else if (p < 2100) drop(c, tx, EY + 19 + (int)((p - 1200) * 26 / 900), 3);
}

void cute(Canvas& c, const FaceCtx& x) {
  shinyEye(c, LX, EY - 1, 28, 32, x.lookX, x.lookY);
  shinyEye(c, RX, EY - 1, 28, 32, x.lookX, x.lookY);
  blush(c, EY + 20);
  c.arcDown(60, 50, 4, 3, 2);  // "w" cat mouth
  c.arcDown(68, 50, 4, 3, 2);
  sparkle(c, 10, 10, twinkle(x.t, 0));
  sparkle(c, 117, 14, twinkle(x.t, 1));
}

void kissou(Canvas& c, const FaceCtx&) {
  c.arcUp(LX, EY + 6, 12, 9, 4);  // closed happy eyes
  c.arcUp(RX, EY + 6, 12, 9, 4);
  blush(c, EY + 18);
  // puckered "3" lips
  c.ring(64, 45, 5, 2);
  c.ring(64, 54, 5, 2);
  c.setColor(0);
  c.rect(57, 38, 7, 22);
  c.setColor(1);
  c.heart(82, 44, 4);
}

void miniLove(Canvas& c, const FaceCtx& x) {
  static const uint16_t d[2] = {420, 420};
  int f = frameAt(x.t, d, 2);
  heartEyes(c, f ? 14 : 11, 0);
}

void animeLove(Canvas& c, const FaceCtx& x) {
  static const uint16_t d[8] = {140, 140, 140, 140, 140, 140, 140, 140};
  int f = frameAt(x.t, d, 8);
  static const int8_t pulse[8] = {0, 1, 2, 3, 2, 1, 0, -1};
  heartEyes(c, 13 + pulse[f], -2);
  blush(c, EY + 18);
  smile(c, 64, 50, 7, 4);
  static const int8_t hx[4] = {8, 58, 70, 120};  // columns between/outside the eyes
  for (int i = 0; i < 4; i++) {
    int y = 66 - ((f * 8 + i * 17) % 70);
    c.heart(hx[i] + (i % 2 ? -(f % 3) : f % 3), y, i % 2 ? 3 : 4);
  }
}

void flames(Canvas& c, int cx, int baseY, int f) {
  static const int8_t heights[3][3] = {{12, 18, 10}, {16, 13, 15}, {10, 20, 13}};
  for (int i = 0; i < 3; i++) {
    int x = cx - 10 + i * 10;
    int h = heights[f][i];
    c.triangle(x - 6, baseY, x + 6, baseY, x + (i - 1) * 2, baseY - h);
  }
  c.setColor(0);
  for (int i = 0; i < 3; i++) {
    int x = cx - 10 + i * 10;
    int h = heights[(f + 1) % 3][i] / 2;
    c.triangle(x - 2, baseY, x + 2, baseY, x, baseY - h);
  }
  c.setColor(1);
}

void fire(Canvas& c, const FaceCtx& x) {
  static const uint16_t d[3] = {120, 120, 120};
  int f = frameAt(x.t, d, 3);
  int gx = x.lookX / 2;
  eye(c, LX + gx, EY + 10, 30, 22, 6);
  eye(c, RX + gx, EY + 10, 30, 22, 6);
  lid(c, LX + gx, EY + 10, 30, 22, 1, 9);
  lid(c, RX + gx, EY + 10, 30, 22, 9, 1);
  flames(c, LX + gx, EY - 3, f);
  flames(c, RX + gx, EY - 3, (f + 1) % 3);
}

void angerMark(Canvas& c, int cx, int cy) {
  c.line(cx - 6, cy - 2, cx - 2, cy - 2, 2);
  c.line(cx - 2, cy - 6, cx - 2, cy - 2, 2);
  c.line(cx + 2, cy - 6, cx + 2, cy - 2, 2);
  c.line(cx + 2, cy - 2, cx + 6, cy - 2, 2);
  c.line(cx - 6, cy + 2, cx - 2, cy + 2, 2);
  c.line(cx - 2, cy + 2, cx - 2, cy + 6, 2);
  c.line(cx + 2, cy + 2, cx + 6, cy + 2, 2);
  c.line(cx + 2, cy + 2, cx + 2, cy + 6, 2);
}

void angry(Canvas& c, const FaceCtx& x) {
  static const uint16_t d[3] = {90, 90, 90};
  static const int8_t shake[3] = {-2, 2, 0};
  int f = frameAt(x.t, d, 3);
  int gx = shake[f] + x.lookX / 3;
  eye(c, LX + gx, EY + 6, 30, 26, 7);
  eye(c, RX + gx, EY + 6, 30, 26, 7);
  lid(c, LX + gx, EY + 6, 30, 26, 1, 13);  // strong V
  lid(c, RX + gx, EY + 6, 30, 26, 13, 1);
  angerMark(c, 116, 9);
  // steam puffing out of the head
  for (int i = 0; i < 2; i++) {
    uint32_t p = (x.t + i * 450) % 900;
    int r = 1 + (int)(p * 3 / 900);
    c.ring(10 + (int)(p / 60), 12 - (int)(p / 90), r, 1);
  }
  // gritted mouth
  int mx = 54 + gx;
  for (int i = 0; i < 5; i++) c.line(mx + i * 4, 56 + (i % 2) * 3, mx + i * 4 + 4, 56 + ((i + 1) % 2) * 3, 1);
}

void love(Canvas& c, const FaceCtx& x) {
  happyEyes(c, 4, 12, 10, 4);
  blush(c, EY + 19);
  smile(c, 64, 50, 9, 5);
  uint32_t b = x.t % 1000;  // ba-dump heartbeat
  int beat = b < 100 ? 2 : b < 200 ? 0 : b < 300 ? 1 : 0;
  c.heart(64, 12, 8 + beat);
  int sway = isin((int)(x.t * 1024 / 1200), 1);
  hand(c, LX - 18, EY + 20 + sway, true, Pose::Open, LX - 22, 70);    // hands on cheeks, kyaa~
  hand(c, RX + 18, EY + 20 - sway, false, Pose::Open, RX + 22, 70);
  sparkle(c, 44, 6, twinkle(x.t, 0));
  sparkle(c, 86, 8, twinkle(x.t, 2));
}

void happy(Canvas& c, const FaceCtx& x) {
  // bouncy ^ ^ with a little squash at the bottom of each hop
  int hop = -abs(isin((int)(x.t * 1024 / 640), 4));
  int squash = hop > -1 ? 1 : 0;
  happyEyes(c, hop, 14 + squash, 13 - squash, 6);
  blush(c, EY + 19 + hop);
  sparkle(c, 30, 6 + hop, twinkle(x.t, 0, 900));
  sparkle(c, 98, 5 + hop, twinkle(x.t, 2, 900));
}

void surprised(Canvas& c, const FaceCtx& x) {
  // pops open bigger, then settles
  int grow = x.t < 120 ? (int)(x.t * 6 / 120) : x.t < 300 ? 6 - (int)((x.t - 120) * 6 / 180) : 0;
  shinyEye(c, LX, EY - 2 - grow / 2, 30 + grow, 32 + grow, x.lookX, x.lookY);
  shinyEye(c, RX, EY - 2 - grow / 2, 30 + grow, 32 + grow, x.lookX, x.lookY);
  c.ring(64, 56, 4 + grow / 3, 2);
  if (x.t < 900) {  // "!" pop
    c.rect(118, 4, 3, 9);
    c.rect(118, 15, 3, 3);
  }
}

void dizzy(Canvas& c, const FaceCtx& x) {
  float rot = (float)x.t * 0.012f;
  spiral(c, LX, EY, 14, rot);
  spiral(c, RX, EY, 14, rot);
  for (int i = 0; i < 3; i++) {  // little stars circling the head
    int a = (int)(x.t * 1024 / 1400) + i * 341;
    sparkle(c, 64 + isin(a + 256, 54), 6 + isin(a, 4), 2 + (i == 1));
  }
  int px = 50, py = 56;
  for (int i = 1; i <= 28; i++) {  // wobbly mouth
    int xx = 50 + i;
    int yy = 56 + isin((int)(i * 1024 / 9 + x.t), 2);
    c.line(px, py, xx, yy, 1);
    px = xx;
    py = yy;
  }
}

void bars(Canvas& c, int x0, int dir, int level, uint32_t t) {
  for (int i = 0; i < 3; i++) {
    int lv = level >= 0 ? level : 40;
    int wob = isin((int)(t * 1024 / (230 + i * 70)) + i * 300, 30);
    int h = clampi(4 + (lv + wob) * (24 - i * 4) / 100, 3, 28);
    c.roundRect(x0 + dir * i * 6, 32 - h / 2, 3, h, 1);
  }
}

void listening(Canvas& c, const FaceCtx& x) {
  static const uint16_t d[4] = {2600, 70, 90, 70};
  static const uint8_t open[4] = {100, 45, 10, 45};
  int f = frameAt(x.t, d, 4);
  int gx = x.lookX / 2, gy = -2 + x.lookY / 2;
  int tilt = x.t < 400 ? (int)(x.t * 3 / 400) : 3;  // head tilts while listening
  eye(c, LX + gx, EY + gy - tilt, 28, 36, 10, open[f]);
  eye(c, RX + gx, EY + gy + tilt, 28, 36, 10, open[f]);
  bars(c, 3, 1, x.level, x.t);
  // hand cupped behind the "ear": I'm listening!
  hand(c, 117, 30 + tilt, false, Pose::Grip, 112, 74);
}

void thinking(Canvas& c, const FaceCtx& x) {
  int gx = 6, gy = -5;
  eye(c, LX + gx, EY + gy, 26, 28, 8);
  eye(c, RX + gx, EY + gy, 26, 28, 8);
  cheek(c, LX + gx, EY + gy, 26, 28, 6);
  cheek(c, RX + gx, EY + gy, 26, 28, 6);
  int n = (int)((x.t / 300) % 4);
  for (int i = 0; i < 3; i++) {
    if (i < n) c.circle(90 + i * 9, 56, 2);
  }
  hand(c, 52, 53, true, Pose::Open, 42, 76);  // hand on chin, hmm...
  int bob = isin((int)(x.t * 1024 / 1000), 2);
  c.text(116, 2 + bob, "?", kFontMedium);
}

void talking(Canvas& c, const FaceCtx& x) {
  static const uint16_t d[4] = {2400, 60, 90, 60};
  static const uint8_t open[4] = {100, 45, 10, 45};
  int f = frameAt(x.t, d, 4);
  int bob = isin((int)(x.t * 1024 / 900), 1);
  eye(c, LX + x.lookX / 2, EY - 3 + bob, 28, 30, 9, open[f]);
  eye(c, RX + x.lookX / 2, EY - 3 + bob, 28, 30, 9, open[f]);
  talkMouth(c, x);
}

void sleeping(Canvas& c, const FaceCtx& x) {
  int b = isin((int)(x.t * 1024 / 4000), 1);
  c.arcDown(LX, EY + 8 + b, 13, 6, 3);
  c.arcDown(RX, EY + 8 + b, 13, 6, 3);
  blush(c, EY + 15 + b);
  // snot bubble grows with each breath, then pops
  uint32_t p = x.t % 4000;
  if (p < 3600) {
    int r = 2 + (int)(p * 7 / 3600);
    c.ring(62, 48 - r / 2, r, 1);
    c.pixel(61 - r / 2, 47 - r);  // shine
  } else if (p < 3800) {
    for (int i = 0; i < 6; i++) {  // pop!
      int a = i * 170;
      c.pixel(62 + isin(a + 256, 7), 43 + isin(a, 6));
    }
  }
  zzz(c, x.t, 100, 26);
}

void cookie(Canvas& c, int cx, int cy, int bites) {
  c.circle(cx, cy, 9);
  c.setColor(0);
  c.pixel(cx - 3, cy - 2);
  c.pixel(cx + 2, cy - 4);
  c.pixel(cx + 3, cy + 2);
  c.pixel(cx - 1, cy + 3);
  c.pixel(cx, cy - 1);
  for (int b = 0; b < bites; b++) c.circle(cx - 9 + b * 6, cy - 3 + (b % 2) * 5, 5);
  c.setColor(1);
}

void eating(Canvas& c, const FaceCtx& x) {
  // 0-1.5 s: both hands lift a cookie up; 1.5-4.5 s: three big bites;
  // 4.5-6 s: happy chewing and patting the tummy.
  uint32_t t = x.t % 6000;
  if (t < 4500) {
    int cookieX = 86, cookieY = 50;
    int bites = 0;
    bool mouthOpen = false;
    if (t < 1500) {
      cookieY = 74 - (int)(t * 24 / 1500);  // rises from below the screen
    } else {
      uint32_t bt = t - 1500;
      bites = (int)(bt / 1000);
      uint32_t inBite = bt % 1000;
      mouthOpen = inBite < 380;
      if (inBite >= 380) bites++;
      if (mouthOpen) cookieX = 77;  // hands push the cookie to the mouth
    }
    int gx = clampi((cookieX - 64) / 4, -6, 6);
    int gy = cookieY > 56 ? 3 : 1;  // looking down at the snack
    eye(c, LX + gx, EY - 6 + gy, 24, 24, 8);
    eye(c, RX + gx, EY - 6 + gy, 24, 24, 8);
    if (mouthOpen) c.ellipse(60, 51, 7, 6);
    else c.hline(54, 66, 51);
    if (bites < 3) {
      cookie(c, cookieX, cookieY, bites);
      hand(c, cookieX - 14, cookieY + 1, true, Pose::Grip, cookieX - 20, 74);
      hand(c, cookieX + 14, cookieY + 1, false, Pose::Grip, cookieX + 20, 74);
    }
    if (t > 1500) {  // crumbs
      for (int i = 0; i < 4; i++) {
        uint32_t h = hash32((t / 1000) * 7 + i);
        c.pixel(62 + (int)(h % 14), 54 + (int)((t % 1000) / 90) + (int)(h % 3));
      }
    }
  } else {
    happyEyes(c, -4, 13, 11, 5);
    int chew = (int)((t / 180) % 2);
    c.ellipse(64, 50, 6, chew ? 3 : 1);
    c.arcDown(52 - chew, 48, 4 + chew, 5, 1);  // puffed hamster cheeks
    c.arcDown(76 + chew, 48, 4 + chew, 5, 1);
    blush(c, EY + 14);
    int pat = (int)((t / 250) % 2) * 3;  // pat pat
    hand(c, 40, 59 - pat, true, Pose::Open, 32, 74);  // pat pat on the tummy
    hand(c, 88, 59 - (3 - pat), false, Pose::Open, 96, 74);
  }
}

void playing(Canvas& c, const FaceCtx& x) {
  // Kibo bats a ball up with both hands; eyes follow it.
  uint32_t period = 2400;
  uint32_t p = x.t % (period * 2);
  int bx = p < period ? 16 + (int)(p * 96 / period) : 112 - (int)((p - period) * 96 / period);
  float ph = (float)(x.t % 600) / 600.0f;  // 4 bounces per crossing
  float up = fabsf(sinf(ph * 3.14159f));
  int by = 47 - (int)(up * 40.0f);
  int gx = clampi((bx - 64) / 6, -9, 9), gy = clampi((by - 30) / 5, -6, 6);
  bool hit = up < 0.18f;
  if (hit) {
    happyEyes(c, -4 + gy / 2, 11, 10, 5);
  } else {
    eye(c, LX + gx, EY - 4 + gy, 22, 24, 8);
    eye(c, RX + gx, EY - 4 + gy, 22, 24, 8);
  }
  // hands under the ball, popping up on each hit
  int hy = hit ? 54 : 58;
  hand(c, bx - 10, hy, true, Pose::Open, bx - 16, 74);
  hand(c, bx + 10, hy, false, Pose::Open, bx + 16, 74);
  c.setColor(0);
  c.circle(bx, by, 6);
  c.setColor(1);
  c.circle(bx, by, 4);
  c.setColor(0);
  c.pixel(bx - 1, by - 2);  // ball shine
  c.setColor(1);
}

void music(Canvas& c, const FaceCtx& x) {
  int bob = isin((int)(x.t * 1024 / 600), 2);
  happyEyes(c, bob, 12, 10, 4);
  c.ring(64, 52 + bob, 4, 2);
  // hands wave left/right to the beat
  int wave = isin((int)(x.t * 1024 / 600), 7);
  hand(c, 12 + wave / 2, 44 - wave, true, Pose::Open, 22, 72);
  hand(c, 116 + wave / 2, 44 + wave, false, Pose::Open, 106, 72);
  for (int i = 0; i < 3; i++) {
    uint32_t p = (x.t + i * 700) % 2100;
    int y = 52 - (int)(p * 44 / 2100);
    int nx = (i % 2 ? 110 : 12) + isin((int)p, 4);
    if (y > 8) note(c, nx, y);
  }
}

void mic(Canvas& c, int x, int y) {
  // little handheld microphone: mesh head + handle
  c.setColor(0);
  c.circle(x, y, 6);
  c.setColor(1);
  c.circle(x, y, 5);
  c.setColor(0);
  for (int i = -3; i <= 3; i += 2)
    for (int j = -3; j <= 3; j += 2) c.pixel(x + i, y + j);
  c.setColor(1);
  c.line(x + 2, y + 5, x + 7, y + 15, 3);
}

void singing(Canvas& c, const FaceCtx& x) {
  // Kibo sings into a microphone: sways to the beat, mouth opens on every "la",
  // free hand waves, notes rise from the mouth between the eyes.
  int sway = isin((int)(x.t * 1024 / 1200), 3);
  uint32_t syl = x.t % 300;
  int open = syl < 180 ? (syl < 90 ? (int)syl * 6 / 90 : 6 - (int)(syl - 90) * 4 / 90) : 1;
  bool high = ((x.t / 300) % 4) == 3;  // every 4th note is the big one
  int dy = high ? -2 : 0;
  happyEyes(c, -6 + dy, 12, 9 + (high ? 2 : 0), 4);
  blush(c, EY + 11 + dy);
  int mx = 58 + sway;
  c.ellipse(mx, 46, 4 + open / 3, 1 + open);  // singing mouth
  for (int i = 0; i < 2; i++) {  // notes rising between the eyes
    uint32_t p = (x.t + i * 900) % 1800;
    int ny = 36 - (int)(p * 34 / 1800);
    int nx = 62 + isin((int)(p * 1024 / 900), 4);
    if (ny > 4) note(c, nx, ny);
  }
  if (high) {
    sparkle(c, 8, 8, 3);
    sparkle(c, 120, 6, 3);
  }
  mic(c, mx + 15, 46);
  hand(c, mx + 22, 57, false, Pose::Grip, mx + 30, 76);  // right hand grips the mic handle
  int wave = isin((int)(x.t * 1024 / 600), 5);
  hand(c, 14 + sway, 40 - wave, true, Pose::Open, 24, 74);  // left hand waves
}

void yawn(Canvas& c, const FaceCtx& x) {
  uint32_t t = x.t % 2800;
  int open = t < 400 ? (int)(t * 100 / 400) : t < 2000 ? 100 : t < 2400 ? (int)((2400 - t) * 100 / 400) : 0;
  c.arcUp(LX, EY + 4, 13, 6 + (100 - open) / 25, 3);
  c.arcUp(RX, EY + 4, 13, 6 + (100 - open) / 25, 3);
  if (open > 5) c.ellipse(64, 48, 6 + open / 40, 2 + open * 10 / 100);
  else c.hline(58, 70, 48);
}

void lowBattery(Canvas& c, const FaceCtx& x) {
  eye(c, LX + x.lookX / 3, EY + 4, 28, 22, 8);
  eye(c, RX + x.lookX / 3, EY + 4, 28, 22, 8);
  c.setColor(0);
  c.rect(0, 0, kWidth, EY - 1);
  c.setColor(1);
  c.frame(50, 46, 24, 12);
  c.rect(74, 49, 2, 6);
  if ((x.t / 500) % 2) c.rect(52, 48, 4, 8);
  // nervous sweat drop sliding down
  drop(c, RX + 17, EY + (int)((x.t % 2000) * 10 / 2000), 2);
}

struct FaceInfo {
  const char* name;
  void (*draw)(Canvas&, const FaceCtx&);
  uint8_t frames;
  uint16_t loopMs;
  bool mouth;
};

const FaceInfo kInfo[(int)Face::Count] = {
    {"neutral", neutral, 6, 4050, false},
    {"serious", serious, 6, 3520, false},
    {"sleepy", sleepy, 1, 1000, false},
    {"crying", crying, 1, 0, true},
    {"sad", sad, 1, 2400, true},
    {"cute", cute, 1, 1200, true},
    {"kissou", kissou, 1, 0, true},
    {"mini_love", miniLove, 2, 840, false},
    {"anime_love", animeLove, 8, 1120, true},
    {"fire", fire, 3, 360, false},
    {"angry", angry, 3, 270, true},
    {"love", love, 1, 1200, true},
    {"happy", happy, 2, 640, false},
    {"surprised", surprised, 1, 1000, true},
    {"dizzy", dizzy, 0, 1600, true},
    {"listening", listening, 0, 2830, false},
    {"thinking", thinking, 0, 2000, false},
    {"talking", talking, 0, 2610, true},
    {"sleeping", sleeping, 0, 4000, false},
    {"eating", eating, 0, 6000, true},
    {"playing", playing, 0, 4800, false},
    {"music", music, 0, 2100, true},
    {"singing", singing, 0, 2400, true},
    {"yawn", yawn, 0, 2800, true},
    {"low_battery", lowBattery, 0, 2000, true},
};

}  // namespace

void drawFace(Canvas& c, Face f, const FaceCtx& ctx) {
  int i = (int)f;
  if (i < 0 || i >= (int)Face::Count) i = 0;
  c.setColor(1);
  kInfo[i].draw(c, ctx);
  if (ctx.talking && !kInfo[i].mouth) talkMouth(c, ctx);
}

void drawBlink(Canvas& c, const FaceCtx& ctx) {
  // closed eyes, used for a quick blink when switching between faces
  c.setColor(1);
  c.roundRect(LX + ctx.lookX - 17, EY + ctx.lookY - 1, 34, 4, 2);
  c.roundRect(RX + ctx.lookX - 17, EY + ctx.lookY - 1, 34, 4, 2);
}

const char* faceName(Face f) {
  int i = (int)f;
  return (i >= 0 && i < (int)Face::Count) ? kInfo[i].name : "neutral";
}

bool faceFromName(const char* name, Face& out) {
  for (int i = 0; i < (int)Face::Count; i++) {
    if (strcmp(name, kInfo[i].name) == 0) {
      out = (Face)i;
      return true;
    }
  }
  return false;
}

int faceFrameCount(Face f) { return kInfo[(int)f].frames; }

uint32_t faceLoopMs(Face f) {
  // Computed from the frame tables for the frame-based faces.
  return kInfo[(int)f].loopMs;
}

bool faceHasMouth(Face f) { return kInfo[(int)f].mouth; }

Face faceForEmotion(const char* e) {
  struct Map {
    const char* emotion;
    Face face;
  };
  static const Map kMap[] = {
      {"neutral", Face::Neutral},     {"happy", Face::Happy},         {"laughing", Face::Happy},
      {"funny", Face::Cute},          {"sad", Face::Sad},             {"angry", Face::Angry},
      {"crying", Face::Crying},       {"loving", Face::Love},         {"love", Face::Love},
      {"embarrassed", Face::Cute},    {"surprised", Face::Surprised}, {"shocked", Face::Surprised},
      {"thinking", Face::Thinking},   {"winking", Face::Kissou},      {"cool", Face::Serious},
      {"relaxed", Face::Happy},       {"delicious", Face::Cute},      {"kissy", Face::Kissou},
      {"confident", Face::Fire},      {"sleepy", Face::Sleepy},       {"silly", Face::Dizzy},
      {"confused", Face::Dizzy},
  };
  if (!e) return Face::Neutral;
  for (const auto& m : kMap)
    if (strcmp(e, m.emotion) == 0) return m.face;
  Face f;
  if (faceFromName(e, f)) return f;
  return Face::Neutral;
}

}  // namespace kibo
