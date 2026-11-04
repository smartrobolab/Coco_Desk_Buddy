#include "coco_faces_x.h"

#include <math.h>
#include <stdlib.h>

namespace coco {

using kibo::clampi;
using kibo::Face;
using kibo::hash32;
using kibo::isin;
using kibo::kFontMedium;
using kibo::kFontSmall;
using kibo::kWidth;

namespace {

// Same eye positions as the Kibo engine, so switching faces lines up.
constexpr int LX = 38;
constexpr int RX = 90;
constexpr int EY = 28;

// ------------------------------------------------------------ helpers
// (small copies of the Kibo engine helpers, which are private to kibo_faces.cpp)

void eye(Canvas& c, int cx, int cy, int w, int h, int r, int open = 100) {
  int hh = h * open / 100;
  int ww = open < 100 ? w + (100 - open) * w / 400 : w;
  if (hh < 3) {
    c.roundRect(cx - ww / 2, cy - 1, ww, 3, 1);
    return;
  }
  c.roundRectC(cx, cy, ww, hh, r);
}

void lid(Canvas& c, int cx, int cy, int w, int h, int depthL, int depthR) {
  int x0 = cx - w / 2 - 2, x1 = cx + w / 2 + 2, top = cy - h / 2 - 2;
  c.setColor(0);
  c.triangle(x0, top, x1, top, x0, top + 2 + depthL);
  c.triangle(x1, top, x1, top + 2 + depthR, x0, top + 2 + depthL);
  c.setColor(1);
}

void blushSpot(Canvas& c, int cx, int cy, int rx, int ry) {
  for (int dy = -ry; dy <= ry; dy++)
    for (int dx = -rx; dx <= rx; dx++)
      if (dx * dx * ry * ry + dy * dy * rx * rx <= rx * rx * ry * ry && ((cx + dx + cy + dy) & 1) == 0)
        c.pixel(cx + dx, cy + dy);
}

void blush(Canvas& c, int y = EY + 18, int rx = 7) {
  blushSpot(c, LX - 8, y + 2, rx, 3);
  blushSpot(c, RX + 8, y + 2, rx, 3);
}

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

int twinkle(uint32_t t, int i, uint32_t period = 1200) {
  uint32_t p = (t + i * 397) % period;
  int s = (int)(p * 8 / period);
  return s < 4 ? s : 7 - s;
}

void drop(Canvas& c, int x, int y, int r) {
  c.circle(x, y, r);
  c.triangle(x - r + 1, y - 1, x + r - 1, y - 1, x, y - r * 2 - 1);
}

void happyEyes(Canvas& c, int dy = 0, int rx = 13, int ry = 12, int t = 5) {
  c.arcUp(LX, EY + 6 + dy, rx, ry, t);
  c.arcUp(RX, EY + 6 + dy, rx, ry, t);
}

void smile(Canvas& c, int cx, int cy, int rx = 8, int ry = 4) { c.arcDown(cx, cy, rx, ry, 2); }

// "D" mouth: lower half of an ellipse, filled
void bigMouth(Canvas& c, int cx, int cy, int rx, int ry) {
  c.ellipse(cx, cy, rx, ry);
  c.setColor(0);
  c.rect(cx - rx - 1, cy - ry - 1, rx * 2 + 3, ry + 1);
  c.setColor(1);
  c.hline(cx - rx, cx + rx, cy);
}

// ">" and "<" squeezed eyes
void squeezeEyes(Canvas& c, int dx = 0, int dy = 0) {
  c.line(LX - 10 + dx, EY - 7 + dy, LX + 8 + dx, EY + dy, 3);
  c.line(LX + 8 + dx, EY + dy, LX - 10 + dx, EY + 7 + dy, 3);
  c.line(RX + 10 + dx, EY - 7 + dy, RX - 8 + dx, EY + dy, 3);
  c.line(RX - 8 + dx, EY + dy, RX + 10 + dx, EY + 7 + dy, 3);
}

void star(Canvas& c, int cx, int cy, int r) {
  int px[10], py[10];
  for (int i = 0; i < 10; i++) {
    int rr = (i % 2) ? r * 45 / 100 : r;
    int a = i * 1024 / 10 - 256;  // first point straight up
    px[i] = cx + isin(a + 256, rr);
    py[i] = cy + isin(a, rr);
  }
  for (int i = 0; i < 10; i++) c.triangle(cx, cy, px[i], py[i], px[(i + 1) % 10], py[(i + 1) % 10]);
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

// Text with a dark outline, readable on top of the face.
void label(Canvas& c, int x, int y, const char* s, const kibo::Font& f) {
  c.setColor(0);
  for (int dx = -1; dx <= 1; dx++)
    for (int dy = -1; dy <= 1; dy++)
      if (dx || dy) c.text(x + dx, y + dy, s, f);
  c.setColor(1);
  c.text(x, y, s, f);
}

// ---- hands (same pixel art as the Kibo engine)
enum class Pose { Open, Grip };
const char* const kOpenHand[8] = {"..XXX...", ".XXXXX..", ".XXXXX.X", ".XXXXXXX",
                                  ".XXXXXX.", ".XXXXX..", "..XXXX..", "...XX..."};
const char* const kGripHand[8] = {"..XXX...", ".XXXXX..", "XXXX....", "XXX.....",
                                  "XXX.....", "XXXX....", ".XXXXX..", "..XXX..."};

void sprite(Canvas& c, const char* const* rows, int x, int y, bool mirror, int scale = 2) {
  int ox = x - 4 * scale, oy = y - 4 * scale;
  for (int pass = 0; pass < 2; pass++) {
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

void hand(Canvas& c, int x, int y, bool left, Pose pose, int bx, int by) {
  arm(c, bx, by, x, y + 6);
  sprite(c, pose == Pose::Open ? kOpenHand : kGripHand, x, y, !left);
}

// ------------------------------------------------------------ food

using FoodFn = void (*)(Canvas&, int cx, int cy, int bites);

void bites(Canvas& c, int cx, int cy, int n) {
  c.setColor(0);
  for (int b = 0; b < n; b++) c.circle(cx - 9 + b * 6, cy - 3 + (b % 2) * 5, 5);
  c.setColor(1);
}

void cookieFood(Canvas& c, int cx, int cy, int n) {
  c.circle(cx, cy, 9);
  c.setColor(0);
  c.pixel(cx - 3, cy - 2);
  c.pixel(cx + 2, cy - 4);
  c.pixel(cx + 3, cy + 2);
  c.pixel(cx - 1, cy + 3);
  c.setColor(1);
  bites(c, cx, cy, n);
}

void pizzaFood(Canvas& c, int cx, int cy, int n) {
  c.triangle(cx - 10, cy - 7, cx + 10, cy - 7, cx, cy + 11);
  c.roundRect(cx - 11, cy - 10, 23, 5, 2);  // crust
  c.setColor(0);
  c.hline(cx - 9, cx + 9, cy - 6);
  c.circle(cx - 3, cy - 2, 2);  // pepperoni
  c.circle(cx + 4, cy - 1, 2);
  c.circle(cx, cy + 4, 1);
  c.setColor(1);
  bites(c, cx, cy + 4, n);
}

void burgerFood(Canvas& c, int cx, int cy, int n) {
  c.roundRect(cx - 10, cy - 9, 21, 7, 3);  // top bun
  c.setColor(0);
  c.pixel(cx - 5, cy - 7);
  c.pixel(cx, cy - 8);
  c.pixel(cx + 5, cy - 7);
  c.setColor(1);
  for (int i = 0; i < 7; i++) c.line(cx - 10 + i * 3, cy - 1, cx - 9 + i * 3, cy + 1, 1);  // lettuce
  c.rect(cx - 10, cy + 2, 21, 3);                                                          // patty
  c.roundRect(cx - 9, cy + 6, 19, 4, 2);                                                   // bottom bun
  bites(c, cx, cy, n);
}

void iceCreamFood(Canvas& c, int cx, int cy, int n) {
  c.triangle(cx - 7, cy, cx + 7, cy, cx, cy + 14);  // cone
  c.setColor(0);
  c.line(cx - 4, cy + 1, cx + 2, cy + 8, 1);
  c.line(cx + 4, cy + 1, cx - 2, cy + 8, 1);
  c.setColor(1);
  int r = 8 - n * 2;  // licked smaller
  if (r > 2) {
    c.circle(cx, cy - r + 2, r);
    c.setColor(0);
    c.pixel(cx - r / 2, cy - r);  // shine
    c.setColor(1);
  }
}

void appleFood(Canvas& c, int cx, int cy, int n) {
  c.circle(cx - 3, cy, 7);
  c.circle(cx + 3, cy, 7);
  c.setColor(0);
  c.rect(cx - 1, cy - 8, 2, 3);  // dent at the top
  c.pixel(cx - 5, cy - 3);
  c.setColor(1);
  c.line(cx, cy - 6, cx + 2, cy - 11, 1);  // stem
  c.ellipse(cx + 5, cy - 9, 3, 1);         // leaf
  bites(c, cx, cy, n);
}

// Lift food up, three bites, then happy chewing (6 s loop). Same choreography as
// the Kibo engine's cookie, with any food.
void eatWith(Canvas& c, const FaceCtx& x, FoodFn food) {
  uint32_t t = x.t % 6000;
  if (t < 4500) {
    int fx = 86, fy = 50, nb = 0;
    bool mouthOpen = false;
    if (t < 1500) {
      fy = 74 - (int)(t * 24 / 1500);
    } else {
      uint32_t bt = t - 1500;
      nb = (int)(bt / 1000);
      uint32_t inBite = bt % 1000;
      mouthOpen = inBite < 380;
      if (inBite >= 380) nb++;
      if (mouthOpen) fx = 77;
    }
    int gx = clampi((fx - 64) / 4, -6, 6), gy = fy > 56 ? 3 : 1;
    eye(c, LX + gx, EY - 6 + gy, 24, 24, 8);
    eye(c, RX + gx, EY - 6 + gy, 24, 24, 8);
    if (mouthOpen) c.ellipse(60, 51, 7, 6);
    else c.hline(54, 66, 51);
    if (nb < 3) {
      food(c, fx, fy, nb);
      hand(c, fx - 14, fy + 1, true, Pose::Grip, fx - 20, 74);
      hand(c, fx + 14, fy + 1, false, Pose::Grip, fx + 20, 74);
    }
    if (t > 1500)
      for (int i = 0; i < 4; i++) {  // crumbs
        uint32_t h = hash32((t / 1000) * 7 + i);
        c.pixel(62 + (int)(h % 14), 54 + (int)((t % 1000) / 90) + (int)(h % 3));
      }
  } else {
    happyEyes(c, -4, 13, 11, 5);
    int chew = (int)((t / 180) % 2);
    c.ellipse(64, 50, 6, chew ? 3 : 1);
    c.arcDown(52 - chew, 48, 4 + chew, 5, 1);
    c.arcDown(76 + chew, 48, 4 + chew, 5, 1);
    blush(c, EY + 14);
    int pat = (int)((t / 250) % 2) * 3;
    hand(c, 40, 59 - pat, true, Pose::Open, 32, 74);
    hand(c, 88, 59 - (3 - pat), false, Pose::Open, 96, 74);
    if ((t / 400) % 2) c.text(100, 4, "yum", kFontSmall);
  }
}

void eatPizza(Canvas& c, const FaceCtx& x) { eatWith(c, x, pizzaFood); }
void eatBurger(Canvas& c, const FaceCtx& x) { eatWith(c, x, burgerFood); }
void eatIceCream(Canvas& c, const FaceCtx& x) { eatWith(c, x, iceCreamFood); }
void eatApple(Canvas& c, const FaceCtx& x) { eatWith(c, x, appleFood); }

void eatNoodles(Canvas& c, const FaceCtx& x) {
  // bowl in front, chopsticks lift a noodle, slurp!
  uint32_t t = x.t % 3000;
  bool slurp = t > 1200 && t < 2400;
  if (slurp) {
    c.arcUp(LX - 4, EY - 2, 12, 7, 3);  // eyes shut, enjoying
    c.arcUp(RX - 10, EY - 2, 12, 7, 3);
  } else {
    eye(c, LX - 4, EY - 8, 22, 22, 8);
    eye(c, RX - 10, EY - 8, 22, 22, 8);
  }
  // bowl
  c.ellipse(64, 54, 26, 12);
  c.setColor(0);
  c.rect(30, 40, 70, 14);
  c.ellipse(64, 54, 23, 3);  // soup surface
  c.setColor(1);
  c.hline(38, 90, 54);
  c.rect(56, 62, 17, 2);  // foot of the bowl
  // chopsticks lift up to the mouth, then back
  int lift = t < 1200 ? (int)(t * 14 / 1200) : t < 2400 ? 14 : 14 - (int)((t - 2400) * 14 / 600);
  int tipY = 52 - lift;
  c.line(100, 40 - lift / 2, 70, tipY, 2);
  c.line(104, 44 - lift / 2, 74, tipY + 2, 2);
  hand(c, 108, 44 - lift / 2, false, Pose::Grip, 118, 74);
  // wavy noodles from the bowl to the chopsticks (and into the mouth while slurping)
  int top = slurp ? 46 - (int)((t - 1200) % 300) / 100 : tipY;
  for (int k = 0; k < 2; k++) {
    int px = 66 + k * 4, py = 54;
    for (int y = 54; y >= top; y -= 2) {
      int xx = 66 + k * 4 + isin((int)(y * 1024 / 8 + t / 2 + k * 300), 2);
      c.line(px, py, xx, y, 1);
      px = xx;
      py = y;
    }
  }
  if (slurp) {
    c.ring(66, 45, 3, 1);
    if ((t / 150) % 2) c.text(100, 4, "slurp", kFontSmall);
  } else {
    c.hline(58, 70, 45);
  }
  // steam
  for (int i = 0; i < 3; i++) {
    uint32_t p = (t + i * 500) % 1500;
    int sy = 40 - (int)(p * 14 / 1500);
    int sx = 44 + i * 6 + isin((int)(p * 2), 2);
    if (p < 1200) c.pixel(sx, sy), c.pixel(sx + 1, sy - 1);
  }
}

void drinkJuice(Canvas& c, const FaceCtx& x) {
  uint32_t t = x.t % 4000;
  int level = 14 - (int)(t * 13 / 3200);  // juice goes down while sipping
  if (level < 1) level = 1;
  bool sipping = t < 3200;
  if (sipping) {
    eye(c, LX + 3, EY - 4, 24, 26, 8);
    eye(c, RX + 3, EY - 4, 24, 26, 8);
    blush(c, EY + 12);
  } else {
    happyEyes(c, -4, 12, 10, 4);
    c.text(100, 4, "ahh", kFontSmall);
  }
  // cup with a straw to the mouth
  c.frame(70, 44, 18, 20);
  c.rect(71, 64 - level, 16, level);
  c.line(78, 44, 70, 34, 2);  // straw
  c.line(70, 34, 66, 46, 2);
  if (sipping) {
    c.ring(64, 47, 2, 1);
    for (int i = 0; i < 2; i++) {  // bubbles
      uint32_t p = (t + i * 350) % 700;
      c.setColor(0);
      c.pixel(75 + i * 6, 62 - (int)(p * level / 700));
      c.setColor(1);
    }
  } else {
    smile(c, 60, 46, 6, 3);
  }
  hand(c, 92, 54, false, Pose::Grip, 102, 74);
}

void hungry(Canvas& c, const FaceCtx& x) {
  // looking up at a dream of food, drooling, tummy rumbles
  const int hl = LX - 10, hr = RX - 20;
  eye(c, hl, EY - 2, 24, 30, 9);
  eye(c, hr, EY - 2, 24, 30, 9);
  c.setColor(0);
  c.circle(hl + 5, EY - 9, 4);  // big highlights: shiny, wanting eyes
  c.circle(hr + 5, EY - 9, 4);
  c.setColor(1);
  c.circle(hl + 5, EY - 9, 2);
  c.circle(hr + 5, EY - 9, 2);
  c.ellipse(50, 51, 5, 3);
  uint32_t p = x.t % 2000;  // drool
  drop(c, 56, 55 + (int)(p * 6 / 2000), 1 + (int)(p / 1000));
  // thought bubble with food
  c.circle(94, 40, 1);
  c.circle(100, 33, 2);
  c.ring(112, 16, 12, 1);
  int which = (int)((x.t / 1500) % 3);
  if (which == 0) cookieFood(c, 112, 16, 0);
  else if (which == 1) pizzaFood(c, 112, 15, 0);
  else appleFood(c, 112, 17, 0);
  if ((x.t / 300) % 4 == 0) label(c, 98, 54, "grrr", kFontSmall);
}

void full(Canvas& c, const FaceCtx& x) {
  uint32_t t = x.t % 3000;
  bool burp = t > 1800 && t < 2400;
  int sh = burp ? isin((int)(t * 10), 2) : 0;
  if (burp) {
    c.arcUp(LX + sh, EY + 2, 12, 7, 3);
    c.arcUp(RX + sh, EY + 2, 12, 7, 3);
    c.ellipse(64 + sh, 48, 7, 5);
    label(c, 4, 2, "BURP!", kFontMedium);
  } else {
    happyEyes(c, -2, 12, 10, 4);
    smile(c, 64, 46, 8, 4);
  }
  blush(c, EY + 14);
  // big round tummy with hands patting
  c.arcDown(64, 58, 26, 8, 2);
  int pat = (int)((t / 300) % 2) * 2;
  hand(c, 36, 56 - pat, true, Pose::Open, 28, 74);
  hand(c, 92, 56 - (2 - pat), false, Pose::Open, 100, 74);
}

// ------------------------------------------------------------ upset

void slapped(Canvas& c, const FaceCtx& x) {
  // knocked sideways, bounces back; hand print on the cheek; OUCH!
  int amp = x.t < 900 ? 8 - (int)(x.t * 8 / 900) : 0;
  int sh = isin((int)(x.t * 1024 / 140), amp);
  squeezeEyes(c, sh, -2);
  c.line(LX - 14 + sh, EY - 14, LX + 6 + sh, EY - 11, 2);  // brows
  c.line(RX - 6 + sh, EY - 11, RX + 14 + sh, EY - 14, 2);
  // wobbly mouth
  int px = 52 + sh, py = 52;
  for (int i = 2; i <= 24; i += 2) {
    int xx = 52 + i + sh, yy = 52 + ((i / 2) % 2 ? 2 : -1);
    c.line(px, py, xx, yy, 1);
    px = xx;
    py = yy;
  }
  // red hand print on the right cheek (dithered)
  blushSpot(c, RX + 14 + sh, EY + 20, 9, 6);
  for (int f = 0; f < 4; f++) c.line(RX + 7 + f * 5 + sh, EY + 12, RX + 9 + f * 5 + sh, EY + 6, 1);
  // impact stars
  for (int i = 0; i < 3; i++) sparkle(c, 112 - i * 8, 6 + i * 4, twinkle(x.t, i, 500) + 1);
  if (x.t < 1600 || (x.t / 250) % 2) label(c, 4, 2, "OUCH!", kFontMedium);
}

void furious(Canvas& c, const FaceCtx& x) {
  int sh = isin((int)(x.t * 1024 / 90), 3);
  int gx = sh;
  // flames behind the head
  for (int i = 0; i < 6; i++) {
    int h = 6 + (int)(hash32((x.t / 100) * 13 + i) % 9);
    int fx = 8 + i * 22;
    c.triangle(fx - 7, 14, fx + 7, 14, fx + (i % 2 ? 2 : -2), 14 - h);
    c.setColor(0);
    c.triangle(fx - 3, 14, fx + 3, 14, fx, 14 - h / 2);
    c.setColor(1);
  }
  eye(c, LX + gx, EY + 8, 32, 24, 6);
  eye(c, RX + gx, EY + 8, 32, 24, 6);
  lid(c, LX + gx, EY + 8, 32, 24, 0, 16);
  lid(c, RX + gx, EY + 8, 32, 24, 16, 0);
  c.setColor(0);  // tiny angry pupils
  c.circle(LX + 4 + gx, EY + 13, 3);
  c.circle(RX - 4 + gx, EY + 13, 3);
  c.setColor(1);
  // wide gritted teeth
  c.frame(46 + gx, 49, 37, 11);
  for (int i = 1; i < 6; i++) c.vline(46 + gx + i * 6, 49, 59);
  c.hline(46 + gx, 82 + gx, 54);
  // steam from both sides
  for (int i = 0; i < 2; i++) {
    uint32_t p = (x.t + i * 300) % 600;
    int r = 1 + (int)(p * 4 / 600);
    c.ring(6 - (int)(p / 120), 40 - (int)(p / 50), r, 1);
    c.ring(122 + (int)(p / 120), 40 - (int)(p / 50), r, 1);
  }
  angerMark(c, 64, 10);
}

void pouting(Canvas& c, const FaceCtx& x) {
  // looks away, puffed cheeks, arms crossed: hmph!
  int look = 9;
  eye(c, LX + look, EY + 2, 26, 20, 7);
  eye(c, RX + look, EY + 2, 26, 20, 7);
  lid(c, LX + look, EY + 2, 26, 20, 3, 8);
  lid(c, RX + look, EY + 2, 26, 20, 8, 3);
  // puffed cheeks + tiny pursed mouth
  int puff = isin((int)(x.t * 1024 / 1600), 1);
  c.ring(52, 48, 6 + puff, 1);
  c.ring(76, 48, 6 + puff, 1);
  c.setColor(0);
  c.rect(53, 40, 22, 16);
  c.setColor(1);
  c.line(60, 49, 68, 47, 2);
  // crossed arms
  hand(c, 50, 60, true, Pose::Grip, 24, 74);
  hand(c, 78, 60, false, Pose::Grip, 104, 74);
  if ((x.t % 3000) < 1500) label(c, 2, 2, "hmph!", kFontMedium);
}

void scared(Canvas& c, const FaceCtx& x) {
  int jx = (int)(hash32(x.t / 50) % 3) - 1;  // trembling
  int jy = (int)(hash32(x.t / 50 + 7) % 3) - 1;
  for (int s = 0; s < 2; s++) {
    int cx = (s ? RX : LX) + jx;
    c.ring(cx, EY + jy, 13, 2);
    c.circle(cx, EY + jy, 3);  // tiny pupils
  }
  c.line(LX - 12 + jx, EY - 18, LX + 8 + jx, EY - 22, 2);  // raised brows
  c.line(RX - 8 + jx, EY - 22, RX + 12 + jx, EY - 18, 2);
  int px = 48, py = 54;
  for (int i = 2; i <= 32; i += 2) {  // wavy mouth
    int xx = 48 + i + jx, yy = 54 + ((i / 2) % 2 ? 2 : -1) + jy;
    c.line(px, py, xx, yy, 1);
    px = xx;
    py = yy;
  }
  uint32_t p = x.t % 1800;  // sweat drops slide down
  drop(c, 10, 16 + (int)(p * 16 / 1800), 2);
  drop(c, 118, 22 + (int)(((p + 900) % 1800) * 16 / 1800), 2);
}

// ------------------------------------------------------------ happy

void laughing(Canvas& c, const FaceCtx& x) {
  int hop = isin((int)(x.t * 1024 / 300), 2);
  squeezeEyes(c, 0, -4 + hop);
  bigMouth(c, 64, 42 + hop, 14, 14);
  c.setColor(0);
  c.ellipse(64, 52 + hop, 6, 3);  // tongue
  c.setColor(1);
  // tears of joy
  uint32_t p = x.t % 900;
  drop(c, LX - 18, EY + 2 + (int)(p * 8 / 900), 2);
  drop(c, RX + 18, EY + 2 + (int)(p * 8 / 900), 2);
  const char* ha = ((x.t / 300) % 2) ? "HA" : "ha";
  c.text(2, 2 + hop, ha, kFontMedium);
  c.text(108, 4 - hop, ha, kFontMedium);
}

void wink(Canvas& c, const FaceCtx& x) {
  uint32_t t = x.t % 3000;
  bool winking = t > 600;
  eye(c, LX + x.lookX / 3, EY - 2, 28, 34, 9);
  if (winking) {
    c.arcUp(RX, EY + 4, 13, 9, 4);
    sparkle(c, RX + 20, EY - 12, twinkle(x.t, 0, 600) + 1);
  } else {
    eye(c, RX + x.lookX / 3, EY - 2, 28, 34, 9);
  }
  smile(c, 64, 46, 9, 5);
  if (winking) {  // tongue poking out
    c.ellipse(68, 53, 4, 4);
    c.setColor(0);
    c.vline(68, 51, 56);
    c.setColor(1);
  }
}

void shy(Canvas& c, const FaceCtx& x) {
  int gx = -6, gy = 4;  // looking down to the side
  eye(c, LX + gx, EY + gy, 22, 24, 8);
  eye(c, RX + gx, EY + gy, 22, 24, 8);
  lid(c, LX + gx, EY + gy, 22, 24, 6, 6);
  lid(c, RX + gx, EY + gy, 22, 24, 6, 6);
  blush(c, EY + 15, 9);
  for (int s = 0; s < 2; s++) {  // ///  lines on the cheeks
    int bx = s ? RX + 2 : LX - 16;
    for (int i = 0; i < 3; i++) c.line(bx + i * 5, EY + 21, bx + i * 5 + 3, EY + 16, 1);
  }
  // small wavy mouth
  c.line(58, 52, 61, 50, 1);
  c.line(61, 50, 64, 52, 1);
  c.line(64, 52, 67, 50, 1);
  // fiddling fingers
  int f = isin((int)(x.t * 1024 / 700), 2);
  hand(c, 56 + f, 60, true, Pose::Open, 44, 76);
  hand(c, 72 - f, 60, false, Pose::Open, 84, 76);
}

void starEyes(Canvas& c, const FaceCtx& x) {
  int pulse = isin((int)(x.t * 1024 / 500), 2);
  star(c, LX, EY - 2, 15 + pulse);
  star(c, RX, EY - 2, 15 + pulse);
  bigMouth(c, 64, 46, 9, 9);
  for (int i = 0; i < 4; i++) {
    static const int8_t sx[4] = {8, 120, 14, 114};
    static const int8_t sy[4] = {8, 10, 54, 52};
    sparkle(c, sx[i], sy[i], twinkle(x.t, i, 800));
  }
}

void cool(Canvas& c, const FaceCtx& x) {
  // sunglasses with a sweeping glint, smirk
  uint32_t t = x.t % 4000;
  int slide = t > 2800 && t < 3600 ? (t < 3000 ? (int)(t - 2800) / 25 : t < 3400 ? 8 : 8 - (int)(t - 3400) / 25) : 0;
  if (slide > 0) {  // peeking over the glasses
    eye(c, LX, EY - 10, 20, 10, 4);
    eye(c, RX, EY - 10, 20, 10, 4);
  }
  int gy = EY - 6 + slide;
  c.roundRect(LX - 17, gy, 34, 18, 5);
  c.roundRect(RX - 17, gy, 34, 18, 5);
  c.rect(LX + 16, gy + 3, RX - LX - 32, 3);  // bridge
  c.line(LX - 17, gy + 3, 2, gy - 1, 2);     // arms
  c.line(RX + 17, gy + 3, 125, gy - 1, 2);
  int g = (int)((x.t % 2000) * 60 / 2000);  // glint sweeps across
  c.setColor(0);
  for (int s = 0; s < 2; s++) {
    int bx = (s ? RX : LX) - 17 + g - 14;
    c.line(bx, gy + 15, bx + 8, gy + 2, 2);
  }
  c.setColor(1);
  c.line(56, 50, 72, 47, 2);  // smirk
  c.pixel(73, 46);
}

void party(Canvas& c, const FaceCtx& x) {
  int bob = isin((int)(x.t * 1024 / 500), 2);
  happyEyes(c, 2 + bob, 12, 10, 4);
  bigMouth(c, 64, 46 + bob, 8, 7);
  blush(c, EY + 18 + bob);
  // party hat (tilted) on top
  c.triangle(54, 15 + bob, 74, 13 + bob, 66, 0 + bob);
  c.setColor(0);
  c.line(58, 10 + bob, 71, 8 + bob, 1);
  c.line(61, 5 + bob, 69, 4 + bob, 1);
  c.setColor(1);
  c.circle(66, 1 + bob, 2);  // pompom
  // confetti falling
  for (int i = 0; i < 10; i++) {
    uint32_t h = hash32(i * 31 + 5);
    int cx = (int)(h % 128);
    int cy = (int)((x.t / 25 + h % 64) % 70) - 4;
    if (cx > 50 && cx < 80 && cy < 18) continue;
    if (i % 2) c.rect(cx, cy, 2, 2);
    else c.line(cx, cy, cx + 2, cy + 1, 1);
  }
}

// ------------------------------------------------------------ sleepy

void dreaming(Canvas& c, const FaceCtx& x) {
  int b = isin((int)(x.t * 1024 / 4000), 1);
  c.arcDown(LX - 10, EY + 12 + b, 12, 5, 3);
  c.arcDown(RX - 22, EY + 12 + b, 12, 5, 3);
  smile(c, 48, 48 + b, 5, 2);
  // dream bubble: heart, cookie or a star
  c.circle(80, 46, 2);
  c.circle(88, 38, 3);
  c.ellipse(108, 18, 18, 14);
  c.setColor(0);
  c.ellipse(108, 18, 16, 12);
  c.setColor(1);
  int which = (int)((x.t / 2500) % 3);
  if (which == 0) c.heart(108, 18, 7 + isin((int)(x.t * 1024 / 800), 1));
  else if (which == 1) cookieFood(c, 108, 18, 0);
  else star(c, 108, 18, 9);
  for (int i = 0; i < 2; i++) {
    uint32_t p = (x.t + i * 1200) % 2400;
    c.text(18 + (int)(p / 200), 14 - (int)(p / 220), "z", kFontSmall);
  }
}

// ------------------------------------------------------------ other moods

void confused(Canvas& c, const FaceCtx& x) {
  int tilt = isin((int)(x.t * 1024 / 2400), 3);
  eye(c, LX, EY - 2 - tilt, 30, 32, 10);        // big eye
  eye(c, RX, EY + 2 + tilt, 20, 12, 5);         // squinted eye
  c.line(RX - 12, EY - 12 + tilt, RX + 10, EY - 8 + tilt, 2);  // one brow down
  c.line(LX - 12, EY - 22 - tilt, LX + 10, EY - 24 - tilt, 2); // one brow up
  int px = 52, py = 52;  // wavy mouth
  for (int i = 1; i <= 24; i++) {
    int xx = 52 + i, yy = 52 + isin(i * 1024 / 12, 2);
    c.line(px, py, xx, yy, 1);
    px = xx;
    py = yy;
  }
  for (int i = 0; i < 3; i++) {  // question marks drift up
    uint32_t p = (x.t + i * 700) % 2100;
    int qx = 112 + isin((int)(p / 2) + i * 300, 4) - i * 6;
    int qy = 48 - (int)(p * 44 / 2100);
    if (qy > -4) c.text(qx, qy, "?", i == 1 ? kFontMedium : kFontSmall);
  }
}

void bored(Canvas& c, const FaceCtx& x) {
  int drift = isin((int)(x.t * 1024 / 5000), 6);
  eye(c, LX + drift, EY + 6, 28, 16, 6);
  eye(c, RX + drift, EY + 6, 28, 16, 6);
  c.setColor(0);
  c.rect(0, 0, kWidth, EY + 2);  // flat heavy lids
  c.setColor(1);
  c.hline(LX - 15, LX + 15, EY + 1);
  c.hline(RX - 15, RX + 15, EY + 1);
  c.hline(56, 74, 52);  // flat mouth
  uint32_t p = x.t % 4000;
  if (p > 2500) {  // sigh puff
    int r = 1 + (int)((p - 2500) * 4 / 1500);
    c.ring(82 + (int)((p - 2500) / 100), 50 - (int)((p - 2500) / 200), r, 1);
  }
  int dots = (int)((x.t / 600) % 4);
  for (int i = 0; i < dots; i++) c.rect(100 + i * 6, 8, 2, 2);
}

void sneeze(Canvas& c, const FaceCtx& x) {
  uint32_t t = x.t % 3200;
  if (t < 1600) {  // ah... ah...
    int sq = (int)(t * 8 / 1600);
    eye(c, LX, EY - 2, 26, 28 - sq * 2, 9);
    eye(c, RX, EY - 2, 26, 28 - sq * 2, 9);
    c.line(LX - 12, EY - 16 + sq / 2, LX + 10, EY - 20, 2);
    c.line(RX - 10, EY - 20, RX + 12, EY - 16 + sq / 2, 2);
    c.ellipse(64, 50, 3 + sq / 3, 2 + sq / 2);
    c.text(4, 2, t < 800 ? "ah..." : "AH...", kFontSmall);
  } else if (t < 2200) {  // ACHOO!
    int sh = isin((int)(t * 1024 / 80), 3);
    squeezeEyes(c, sh, 0);
    c.ellipse(64 + sh, 50, 6, 5);
    for (int i = 0; i < 12; i++) {  // spray
      uint32_t h = hash32(i * 17 + 3);
      int d = (int)((t - 1600) / 20);
      c.pixel(64 + (int)(h % 40) - 20 + sh, 56 + (int)((h >> 8) % 6) + d / 4);
    }
    label(c, 70, 2, "ACHOO!", kFontMedium);
  } else {  // sniff
    eye(c, LX, EY, 26, 26, 9);
    eye(c, RX, EY, 26, 26, 9);
    blush(c, EY + 16);
    c.hline(58, 70, 50);
    drop(c, 64, 46, 1);
    c.text(100, 4, "sniff", kFontSmall);
  }
}

// ------------------------------------------------------------ hobbies

void gaming(Canvas& c, const FaceCtx& x) {
  uint32_t t = x.t % 5000;
  bool win = t > 4000;
  if (win) {
    happyEyes(c, -6, 12, 10, 4);
    label(c, 96, 2, "WIN!", kFontMedium);
  } else {
    eye(c, LX, EY - 6, 26, 18, 6);
    eye(c, RX, EY - 6, 26, 18, 6);
    lid(c, LX, EY - 6, 26, 18, 3, 3);
    lid(c, RX, EY - 6, 26, 18, 3, 3);
    c.setColor(0);
    c.rect(LX - 4, EY - 4, 8, 6);  // pupils looking down at the screen
    c.rect(RX - 4, EY - 4, 8, 6);
    c.setColor(1);
  }
  // handheld console
  c.roundRect(40, 42, 48, 22, 4);
  c.setColor(0);
  c.rect(52, 45, 24, 14);  // screen
  c.rect(44, 51, 6, 2);    // d-pad
  c.rect(46, 49, 2, 6);
  c.circle(80, 50, 1);
  c.circle(84, 54, 1);
  c.setColor(1);
  // tiny game on the screen: a dot jumping over a block
  int bx = 74 - (int)((t / 40) % 22);
  c.rect(bx, 55, 2, 3);
  int jy = (bx > 56 && bx < 62) ? 51 : 56;
  c.rect(56, jy, 2, 2);
  // thumbs tapping
  int tap = (int)((t / 160) % 2);
  hand(c, 36, 54 + tap, true, Pose::Grip, 24, 76);
  hand(c, 92, 54 + (1 - tap), false, Pose::Grip, 104, 76);
}

void workout(Canvas& c, const FaceCtx& x) {
  uint32_t t = x.t % 1400;
  int lift = t < 700 ? (int)(t * 12 / 700) : 12 - (int)((t - 700) * 12 / 700);
  eye(c, LX, EY - 8, 28, 20, 7);
  eye(c, RX, EY - 8, 28, 20, 7);
  lid(c, LX, EY - 8, 28, 20, 2, 8);
  lid(c, RX, EY - 8, 28, 20, 8, 2);
  c.frame(54, 34, 20, 7);  // gritted effort
  c.vline(60, 34, 40);
  c.vline(66, 34, 40);
  // dumbbell lifted up and down in front of the tummy: bar + plates
  int by = 58 - lift;
  c.rect(20, by - 1, 88, 3);
  c.roundRect(12, by - 6, 7, 13, 2);
  c.roundRect(109, by - 6, 7, 13, 2);
  hand(c, 30, by, true, Pose::Grip, 26, 76);
  hand(c, 98, by, false, Pose::Grip, 102, 76);
  drop(c, RX + 20, EY - 14 + (int)((x.t % 1000) / 100), 2);
  char reps[8];
  int n = (int)((x.t / 1400) % 10) + 1;
  reps[0] = '0' + (n / 10);
  reps[1] = '0' + (n % 10);
  reps[2] = 0;
  c.text(2, 2, n < 10 ? reps + 1 : reps, kFontMedium);
}

void reading(Canvas& c, const FaceCtx& x) {
  uint32_t t = x.t % 4000;
  bool flip = t > 3600;
  int line = (int)(t / 900);           // which line
  int across = (int)((t % 900) * 16 / 900) - 8;  // eyes sweep along it
  eye(c, LX + across, EY - 6 + line, 22, 20, 7);
  eye(c, RX + across, EY - 6 + line, 22, 20, 7);
  // open book
  c.triangle(30, 44, 64, 48, 30, 64);
  c.triangle(30, 64, 64, 48, 64, 64);
  c.triangle(98, 44, 64, 48, 98, 64);
  c.triangle(98, 64, 64, 48, 64, 64);
  c.setColor(0);
  c.vline(64, 48, 63);
  for (int i = 0; i < 4; i++) {
    c.line(34, 49 + i * 4, 60, 51 + i * 4, 1);
    if (!flip) c.line(68, 51 + i * 4, 94, 49 + i * 4, 1);
  }
  c.setColor(1);
  if (flip) c.line(64, 48, 64 + (int)((4000 - t) * 30 / 400), 40, 2);  // page turning
  hand(c, 26, 56, true, Pose::Grip, 16, 76);
  hand(c, 102, 56, false, Pose::Grip, 112, 76);
}

// ------------------------------------------------------------ weather

void cold(Canvas& c, const FaceCtx& x) {
  int sh = ((x.t / 60) % 2) ? 1 : -1;  // shivering
  eye(c, LX + sh, EY - 2, 26, 24, 8);
  eye(c, RX + sh, EY - 2, 26, 24, 8);
  lid(c, LX + sh, EY - 2, 26, 24, 8, 2);
  lid(c, RX + sh, EY - 2, 26, 24, 2, 8);
  // chattering teeth
  int open = (int)((x.t / 90) % 2) * 2;
  c.frame(52 + sh, 44, 25, 7 + open);
  for (int i = 1; i < 4; i++) c.vline(52 + sh + i * 6, 44, 50 + open);
  // scarf
  c.rect(20, 56, 88, 6);
  c.setColor(0);
  for (int i = 0; i < 11; i++) c.vline(24 + i * 8, 57, 60);
  c.setColor(1);
  c.rect(84, 56, 6, 8);
  // snowflakes
  for (int i = 0; i < 7; i++) {
    uint32_t h = hash32(i * 13 + 1);
    int fx = (int)(h % 128), fy = (int)((x.t / 40 + h % 64) % 56);
    if (fx > 20 && fx < 108 && fy > 8 && fy < 48) continue;
    c.pixel(fx, fy);
    c.pixel(fx - 1, fy);
    c.pixel(fx + 1, fy);
    c.pixel(fx, fy - 1);
    c.pixel(fx, fy + 1);
  }
}

void hot(Canvas& c, const FaceCtx& x) {
  eye(c, LX, EY + 4, 26, 20, 7);
  eye(c, RX, EY + 4, 26, 20, 7);
  lid(c, LX, EY + 4, 26, 20, 8, 3);
  lid(c, RX, EY + 4, 26, 20, 3, 8);
  // panting, tongue out
  int pant = (int)((x.t / 250) % 2);
  c.ellipse(64, 48, 7, 4 + pant);
  c.setColor(0);
  c.ellipse(64, 47, 5, 2 + pant);
  c.setColor(1);
  c.roundRect(61, 50, 7, 7 + pant * 2, 3);
  // sun in the corner
  c.circle(116, 8, 5);
  for (int i = 0; i < 8; i++) {
    int a = i * 128 + (int)(x.t / 8);
    c.line(116 + isin(a + 256, 8), 8 + isin(a, 8), 116 + isin(a + 256, 11), 8 + isin(a, 11), 1);
  }
  drop(c, LX - 18, EY - 6 + (int)((x.t % 1400) / 100), 2);
  // hand fanning
  int fan = isin((int)(x.t * 1024 / 300), 5);
  hand(c, 16, 44 + fan, true, Pose::Open, 12, 76);
}

// ------------------------------------------------------------ catalog

struct ExInfo {
  const char* label;
  int8_t kibo;  // >= 0: draw this Kibo face
  void (*draw)(Canvas&, const FaceCtx&);
  uint16_t loopMs;
  bool wanders;
};

#define K(f) (int8_t)(Face::f), nullptr
#define X(fn) -1, fn
const ExInfo kEx[kExCount] = {
    {"Neutral", K(Neutral), 4050, true},
    {"Happy", K(Happy), 640, false},
    {"Love", K(Love), 1200, false},
    {"Cute", K(Cute), 1200, true},
    {"Laughing", X(laughing), 1800, false},
    {"Wink", X(wink), 3000, false},
    {"Shy", X(shy), 1400, false},
    {"Star eyes", X(starEyes), 1600, false},
    {"Cool", X(cool), 4000, false},
    {"Party", X(party), 2000, false},
    {"Kissou", K(Kissou), 1000, false},
    {"Mini love", K(MiniLove), 840, false},
    {"Anime love", K(AnimeLove), 1120, false},
    {"Cookie", K(Eating), 6000, false},
    {"Pizza", X(eatPizza), 6000, false},
    {"Noodles", X(eatNoodles), 3000, false},
    {"Burger", X(eatBurger), 6000, false},
    {"Ice cream", X(eatIceCream), 6000, false},
    {"Apple", X(eatApple), 6000, false},
    {"Juice", X(drinkJuice), 4000, false},
    {"Hungry", X(hungry), 4500, false},
    {"Full", X(full), 3000, false},
    {"Sleepy", K(Sleepy), 1000, false},
    {"Yawn", K(Yawn), 2800, false},
    {"Sleeping", K(Sleeping), 4000, false},
    {"Dreaming", X(dreaming), 7500, false},
    {"Slapped", X(slapped), 2000, false},
    {"Angry", K(Angry), 1000, false},
    {"Furious", X(furious), 1000, false},
    {"Pouting", X(pouting), 3000, false},
    {"Sad", K(Sad), 2400, false},
    {"Crying", K(Crying), 1000, false},
    {"Scared", X(scared), 1800, false},
    {"Fire", K(Fire), 360, false},
    {"Surprised", K(Surprised), 1000, false},
    {"Confused", X(confused), 2400, false},
    {"Dizzy", K(Dizzy), 1600, false},
    {"Bored", X(bored), 4000, true},
    {"Serious", K(Serious), 3520, true},
    {"Thinking", K(Thinking), 2000, false},
    {"Sneeze", X(sneeze), 3200, false},
    {"Playing", K(Playing), 4800, false},
    {"Dancing", K(Music), 2100, false},
    {"Singing", K(Singing), 2400, false},
    {"Gaming", X(gaming), 5000, false},
    {"Workout", X(workout), 1400, false},
    {"Reading", X(reading), 4000, false},
    {"Cold", X(cold), 1000, false},
    {"Hot", X(hot), 1000, false},
    {"Low battery", K(LowBattery), 2000, false},
};
#undef K
#undef X

}  // namespace

void drawEx(Canvas& c, Ex e, const FaceCtx& ctx) {
  int i = (int)e;
  if (i < 0 || i >= kExCount) i = 0;
  c.setColor(1);
  if (kEx[i].kibo >= 0) kibo::drawFace(c, (Face)kEx[i].kibo, ctx);
  else kEx[i].draw(c, ctx);
  c.setColor(1);
}

const char* exLabel(Ex e) { return (int)e < kExCount ? kEx[(int)e].label : "?"; }
bool exWanders(Ex e) { return (int)e < kExCount && kEx[(int)e].wanders; }
uint32_t exLoopMs(Ex e) { return (int)e < kExCount ? kEx[(int)e].loopMs : 1000; }

}  // namespace coco
