#include "coco_games.h"

#include <math.h>
#include <stdio.h>

namespace coco {

using kibo::hash32;
using kibo::isin;
using kibo::kFontMedium;
using kibo::kFontSmall;
using kibo::kHeight;
using kibo::kWidth;

namespace {

const char* const kNames[] = {"Dino Run", "Sky Hop", "Stacker", "Reflex"};

// Dino Run
constexpr int kGround = 56;   // runner's feet
constexpr int kRunX = 14;     // runner's left edge
constexpr int kRunW = 12;
constexpr float kJump = 215.0f;    // px/s
constexpr float kGravity = 790.0f; // px/s^2 -> ~29 px high, ~0.54 s in the air

// Sky Hop
constexpr int kHopX = 30;
constexpr int kPipeW = 10;
constexpr int kPipeGap = 28;
constexpr float kFlap = 122.0f;
constexpr float kHopGravity = 520.0f;

// Stacker
constexpr int kLayerH = 5;
constexpr int kBaseW = 44;

// Coco as a little round game character (also the Sky Hop bird).
void cocoSprite(Canvas& c, int x, int y, int w, int h, bool blink, int legs) {
  c.roundRect(x, y, w, h, 4);
  c.setColor(0);
  int eh = blink ? 1 : 3;
  c.rect(x + w / 2 - 1, y + 3, 2, eh);
  c.rect(x + w - 3, y + 3, 2, eh);
  c.setColor(1);
  if (legs == 1) {
    c.rect(x + 2, y + h, 2, 2);
    c.rect(x + w - 5, y + h, 2, 1);
  } else if (legs == 2) {
    c.rect(x + 2, y + h, 2, 1);
    c.rect(x + w - 5, y + h, 2, 2);
  }
}

void cactus(Canvas& c, int x, int top, int w, int h) {
  c.roundRect(x + w / 2 - 2, top, 4, h, 2);
  if (w >= 8) {  // arms
    c.rect(x, top + 3, 2, 5);
    c.rect(x, top + 7, w / 2, 2);
    c.rect(x + w - 2, top + 2, 2, 4);
    c.rect(x + w / 2, top + 5, w / 2, 2);
  }
}

void bird(Canvas& c, int x, int top, int w, uint32_t t) {
  int flap = (t / 150) % 2;
  c.ellipse(x + w / 2, top + 3, w / 2 - 1, 2);
  c.rect(x, top + 2, 2, 2);  // beak
  if (flap) c.triangle(x + 3, top + 2, x + w - 2, top + 2, x + w / 2 + 2, top - 4);
  else c.triangle(x + 3, top + 4, x + w - 2, top + 4, x + w / 2 + 2, top + 9);
}

void scoreText(Canvas& c, int score) {
  char s[12];
  snprintf(s, sizeof(s), "%d", score);
  c.text(kWidth - c.textWidth(s, kFontSmall) - 1, 0, s, kFontSmall);
}

}  // namespace

const char* gameName(Game g) { return (int)g < (int)Game::Count ? kNames[(int)g] : "?"; }
bool gameLowerIsBetter(Game g) { return g == Game::Reflex; }
bool gameHasAutopilot(Game g) { return g != Game::Reflex; }

uint32_t Arcade::rand() {
  seed_ = hash32(seed_ + 0x9e37);
  return seed_;
}

void Arcade::start(Game g, uint32_t now, uint32_t seed, bool autopilot) {
  game_ = g;
  auto_ = autopilot && gameHasAutopilot(g);
  over_ = false;
  valid_ = true;
  started_ = g == Game::Dino || auto_;  // Sky Hop and Stacker wait for the first tap
  seed_ = seed ? seed : 1;
  startAt_ = lastAt_ = now;
  score_ = 0;
  dist_ = 0;
  py_ = 0;
  vy_ = 0;
  gap_ = 40;
  for (auto& o : obs_) o.live = false;
  for (auto& p : pipes_) p.live = false;
  nLayers_ = 0;
  if (g == Game::Hop) {
    py_ = 30;
    for (int i = 0; i < 4; i++) pipes_[i] = {(float)(110 + i * 52), (int8_t)(18 + rand() % 28), false, true};
  } else if (g == Game::Stack) {
    started_ = true;
    layers_[0] = {(int16_t)(64 - kBaseW / 2), (int16_t)kBaseW};
    nLayers_ = 1;
    bw_ = kBaseW;
    bx_ = 0;
    bdir_ = 1;
    chopAt_ = perfectAt_ = 0;
    aimErr_ = 0;
  } else if (g == Game::Reflex) {
    started_ = true;
    goAt_ = now + 1500 + rand() % 2500;
  }
}

void Arcade::gameOver(uint32_t now) {
  if (over_) return;
  over_ = true;
  overAt_ = now;
}

void Arcade::press(uint32_t now) {
  if (over_ || auto_) return;
  switch (game_) {
    case Game::Dino:
      if (py_ >= 0) vy_ = -kJump;
      break;
    case Game::Hop:
      started_ = true;
      vy_ = -kFlap;
      break;
    case Game::Stack: dropBlock(now); break;
    case Game::Reflex:
      if (now < goAt_) {
        valid_ = false;  // too soon
        score_ = 0;
      } else {
        score_ = (int)(now - goAt_);
      }
      gameOver(now);
      break;
    default: break;
  }
}

void Arcade::update(uint32_t now) {
  float dt = (float)(now - lastAt_) / 1000.0f;
  if (dt > 0.06f) dt = 0.06f;  // after a stall, don't teleport
  lastAt_ = now;
  if (over_) {
    if (auto_ && now - overAt_ > 1800) start(game_, now, rand(), true);
    return;
  }
  switch (game_) {
    case Game::Dino: updateDino(dt, now); break;
    case Game::Hop: updateHop(dt, now); break;
    case Game::Stack: updateStack(dt, now); break;
    case Game::Reflex: updateReflex(now); break;
    default: break;
  }
}

// ------------------------------------------------------------ Dino Run

void Arcade::updateDino(float dt, uint32_t now) {
  float secs = (float)(now - startAt_) / 1000.0f;
  speed_ = 62.0f + secs * 2.6f;
  if (speed_ > 155.0f) speed_ = 155.0f;
  dist_ += speed_ * dt;
  score_ = (int)(dist_ / 8);

  // autopilot: jump so the top of the arc is over the middle of the next obstacle
  if (auto_ && py_ >= 0) {
    const float air = 2 * kJump / kGravity;
    for (const auto& o : obs_) {
      if (!o.live || o.x + o.w < kRunX) continue;
      float tc = (o.x + o.w / 2.0f - (kRunX + kRunW / 2.0f)) / speed_;
      if (tc > 0 && tc <= air / 2 + o.jitter) vy_ = -kJump;
      break;
    }
  }

  // jump physics (py_ <= 0 is height above the ground, negative = up)
  if (py_ < 0 || vy_ < 0) {
    vy_ += kGravity * dt;
    py_ += vy_ * dt;
    if (py_ >= 0) {
      py_ = 0;
      vy_ = 0;
    }
  }

  // obstacles
  gap_ -= speed_ * dt;
  if (gap_ <= 0) {
    for (auto& o : obs_) {
      if (o.live) continue;
      uint32_t r = rand();
      int kind = secs > 8 && r % 5 == 0 ? 1 : 0;
      o.live = true;
      o.x = 130;
      o.kind = kind;
      if (kind == 1) {  // bird at head height: jump it
        o.w = 11;
        o.h = 6;
        o.top = kGround - 15 + (int)(r >> 8) % 3;
      } else {
        static const int8_t kW[3] = {6, 8, 14}, kH[3] = {10, 14, 10};
        int k = (int)((r >> 8) % 3);
        o.w = kW[k];
        o.h = kH[k];
        o.top = kGround - o.h;
      }
      // autopilot: mostly good timing, sometimes a miss so it can lose too
      uint32_t j = (r >> 16) % 100;
      o.jitter = j < 4 ? 0.16f : (float)((int)j % 9 - 4) / 100.0f;
      break;
    }
    gap_ = 52.0f + (float)(rand() % 64) + speed_ * 0.38f;
  }
  int top = kGround - 12 + (int)py_;
  for (auto& o : obs_) {
    if (!o.live) continue;
    o.x -= speed_ * dt;
    if (o.x + o.w < 0) {
      o.live = false;
      continue;
    }
    // hit test with a little slack
    bool hx = o.x + 1 < kRunX + kRunW - 2 && o.x + o.w - 1 > kRunX + 2;
    bool hy = o.top + 1 < top + 12 && o.top + o.h > top + 2;
    if (hx && hy) gameOver(now);
  }
}

void Arcade::drawDino(Canvas& c, uint32_t now) const {
  // ground with scrolling pebbles
  c.hline(0, kWidth - 1, kGround);
  int off = (int)dist_ % 128;
  for (int i = 0; i < 9; i++) {
    int x = (i * 37 + 128 - off) % 128;
    c.pixel(x, kGround + 3 + (i % 3));
    if (i % 2) c.pixel(x + 1, kGround + 3 + (i % 3));
  }
  // clouds
  for (int i = 0; i < 2; i++) {
    int x = 140 - ((int)(dist_ / 4) + i * 70) % 160;
    c.ring(x, 12 + i * 8, 4, 1);
    c.ring(x + 5, 10 + i * 8, 5, 1);
    c.setColor(0);
    c.rect(x - 5, 13 + i * 8, 16, 4);
    c.setColor(1);
    c.hline(x - 4, x + 9, 13 + i * 8);
  }
  for (const auto& o : obs_) {
    if (!o.live) continue;
    if (o.kind == 1) bird(c, (int)o.x, o.top, o.w, now);
    else cactus(c, (int)o.x, o.top, o.w, o.h);
  }
  int top = kGround - 12 + (int)py_;
  int legs = py_ < 0 ? 0 : 1 + (int)(dist_ / 6) % 2;
  cocoSprite(c, kRunX, top, kRunW, 11, over_, legs);
  scoreText(c, score_);
}

// ------------------------------------------------------------ Sky Hop

void Arcade::updateHop(float dt, uint32_t now) {
  if (!started_) {
    py_ = 30 + isin((int)(now * 1024 / 800), 2);
    return;
  }
  speed_ = 40.0f + score_ * 1.2f;
  if (speed_ > 70.0f) speed_ = 70.0f;
  // autopilot: flap when sinking below the middle of the next gap
  if (auto_) {
    const Pipe* next = nullptr;  // pipes are recycled, so find the nearest one ahead
    for (const auto& p : pipes_)
      if (p.live && p.x + kPipeW >= kHopX && (!next || p.x < next->x)) next = &p;
    if (next && vy_ > 0 && py_ > next->gapY + 1 + aimErr_) vy_ = -kFlap;
    if (next && next->passed == false && next->x < kHopX - 4) aimErr_ = (float)((int)(rand() % 7) - 3);
  }
  vy_ += kHopGravity * dt;
  py_ += vy_ * dt;
  if (py_ < 0 || py_ > kHeight - 6) gameOver(now);
  float maxX = 0;
  for (const auto& p : pipes_) if (p.live && p.x > maxX) maxX = p.x;
  for (auto& p : pipes_) {
    p.x -= speed_ * dt;
    if (p.x + kPipeW < 0) {
      p = {maxX + 52, (int8_t)(16 + rand() % 32), false, true};
      continue;
    }
    if (!p.passed && p.x + kPipeW < kHopX) {
      p.passed = true;
      score_++;
    }
    bool hx = kHopX + 8 > p.x + 1 && kHopX + 1 < p.x + kPipeW - 1;
    bool hy = py_ + 1 < p.gapY - kPipeGap / 2 || py_ + 6 > p.gapY + kPipeGap / 2;
    if (hx && hy) gameOver(now);
  }
}

void Arcade::drawHop(Canvas& c, uint32_t now) const {
  for (const auto& p : pipes_) {
    if (!p.live) continue;
    int x = (int)p.x, gt = p.gapY - kPipeGap / 2, gb = p.gapY + kPipeGap / 2;
    c.rect(x, 0, kPipeW, gt);
    c.rect(x - 2, gt - 4, kPipeW + 4, 4);
    c.rect(x, gb, kPipeW, kHeight - gb);
    c.rect(x - 2, gb, kPipeW + 4, 4);
    c.setColor(0);
    c.vline(x + 2, 0, gt - 5);
    c.vline(x + 2, gb + 5, kHeight - 1);
    c.setColor(1);
  }
  int y = (int)py_;
  bool flap = vy_ < 0 && ((now / 80) % 2);
  c.setColor(0);
  c.rect(kHopX - 1, y - 1, 11, 9);
  c.setColor(1);
  cocoSprite(c, kHopX, y, 9, 7, over_, 0);
  c.line(kHopX - 2, y + 3, kHopX + 2, flap ? y : y + 5, 1);  // wing
  if (!started_) c.textCentered(64, 52, "tap to flap", kFontSmall);
  scoreText(c, score_);
}

// ------------------------------------------------------------ Stacker

void Arcade::dropBlock(uint32_t now) {
  const Layer& top = layers_[nLayers_ - 1];
  int x = (int)bx_;
  if (abs(x - top.x) <= 1) {  // perfect: snap, keep the width
    x = top.x;
    perfectAt_ = now;
  }
  int l = x > top.x ? x : top.x;
  int r = x + bw_ < top.x + top.w ? x + bw_ : top.x + top.w;
  if (r <= l) {
    chopX_ = x;
    chopW_ = bw_;
    chopLayer_ = nLayers_;
    chopAt_ = now;
    gameOver(now);
    return;
  }
  if (r - l < bw_) {  // the overhang falls off
    chopX_ = x < top.x ? x : r;
    chopW_ = bw_ - (r - l);
    chopLayer_ = nLayers_;
    chopAt_ = now;
  }
  if (nLayers_ < kMaxLayers) layers_[nLayers_++] = {(int16_t)l, (int16_t)(r - l)};
  else {  // keep the newest layers
    for (int i = 1; i < kMaxLayers; i++) layers_[i - 1] = layers_[i];
    layers_[kMaxLayers - 1] = {(int16_t)l, (int16_t)(r - l)};
  }
  score_++;
  bw_ = r - l;
  bdir_ = (score_ % 2) ? -1 : 1;
  bx_ = bdir_ > 0 ? -bw_ : kWidth;
  aimErr_ = (float)((int)(rand() % 9) - 4);  // autopilot aim for the next one
  if (rand() % 25 == 0) aimErr_ = 12;
}

void Arcade::updateStack(float dt, uint32_t now) {
  speed_ = 38.0f + score_ * 4.0f;
  if (speed_ > 120.0f) speed_ = 120.0f;
  bx_ += bdir_ * speed_ * dt;
  if (bx_ > kWidth - bw_ / 2) bdir_ = -1;
  if (bx_ < -bw_ / 2) bdir_ = 1;
  if (auto_) {
    const Layer& top = layers_[nLayers_ - 1];
    if (fabsf(bx_ - (top.x + aimErr_)) < speed_ * dt + 0.5f) dropBlock(now);
  }
}

void Arcade::drawStack(Canvas& c, uint32_t now) const {
  // camera: keep the moving block around y = 22
  int height = (nLayers_ + 1) * kLayerH;
  int cam = height > 42 ? height - 42 : 0;
  auto yOf = [&](int layer) { return kHeight - (layer + 1) * kLayerH + cam; };
  for (int i = 0; i < nLayers_; i++) {
    int y = yOf(i);
    if (y > kHeight || y < -kLayerH) continue;
    c.rect(layers_[i].x, y, layers_[i].w, kLayerH - 1);
  }
  if (!over_) c.rect((int)bx_, yOf(nLayers_), bw_, kLayerH - 1);
  if (chopAt_ && now - chopAt_ < 700) {  // falling piece
    int fall = (int)((now - chopAt_) * (now - chopAt_) / 4000);
    c.frame(chopX_, yOf(chopLayer_) + fall, chopW_, kLayerH - 1);
  }
  if (perfectAt_ && now - perfectAt_ < 700) c.textCentered(64, 12, "PERFECT!", kFontSmall);
  scoreText(c, score_);
}

// ------------------------------------------------------------ Reflex

void Arcade::updateReflex(uint32_t now) {
  if (now > goAt_ + 2000) {  // nobody tapped
    score_ = 2000;
    gameOver(now);
  }
}

void Arcade::drawReflex(Canvas& c, uint32_t now) const {
  if (now < goAt_) {
    // Coco stares, waiting
    int look = isin((int)(now * 1024 / 1500), 4);
    c.roundRectC(38 + look, 24, 22, 26, 7);
    c.roundRectC(90 + look, 24, 22, 26, 7);
    c.textCentered(64, 46, "wait for it...", kFontSmall);
    c.textCentered(64, 56, "tap when I say TAP!", kFontSmall);
  } else {
    c.rect(0, 0, kWidth, kHeight);
    c.setColor(0);
    c.textCentered(64, 20, "TAP!", kFontMedium);
    c.ring(38, 28, 10, 3);
    c.ring(90, 28, 10, 3);
    c.setColor(1);
  }
}

// ------------------------------------------------------------ drawing

void Arcade::drawOver(Canvas& c, uint32_t now, int best) const {
  c.setColor(0);
  c.rect(10, 8, 108, 50);
  c.setColor(1);
  c.frame(10, 8, 108, 50);
  char line[32];
  if (game_ == Game::Reflex) {
    c.textCentered(64, 11, valid_ ? (score_ >= 2000 ? "Too slow!" : "Reaction") : "Too soon!", kFontMedium);
    if (valid_ && score_ < 2000) {
      snprintf(line, sizeof(line), "%d ms", score_);
      c.textCentered(64, 24, line, kFontMedium);
    }
  } else {
    c.textCentered(64, 11, auto_ ? "Oops, I lost!" : "GAME OVER", kFontMedium);
    snprintf(line, sizeof(line), "Score %d", score_);
    c.textCentered(64, 24, line, kFontMedium);
  }
  bool newBest = valid_ && !auto_ && score_ > 0 &&
                 (best == 0 || (gameLowerIsBetter(game_) ? score_ < best && score_ < 2000 : score_ > best));
  if (newBest) {
    if ((now / 300) % 2) c.textCentered(64, 36, "NEW BEST!", kFontSmall);
  } else if (best > 0) {
    snprintf(line, sizeof(line), gameLowerIsBetter(game_) ? "Best %d ms" : "Best %d", best);
    c.textCentered(64, 36, line, kFontSmall);
  }
  c.textCentered(64, 47, auto_ ? "touch me to stop" : "tap: again  hold: exit", kFontSmall);
}

void Arcade::draw(Canvas& c, uint32_t now, int best) const {
  c.setColor(1);
  switch (game_) {
    case Game::Dino: drawDino(c, now); break;
    case Game::Hop: drawHop(c, now); break;
    case Game::Stack: drawStack(c, now); break;
    case Game::Reflex: drawReflex(c, now); break;
    default: break;
  }
  if (auto_ && !over_) {
    c.setColor(0);
    c.rect(0, 0, 44, 8);
    c.setColor(1);
    c.text(1, 0, "Coco plays", kFontSmall);
  }
  if (over_ && now - overAt_ > 350) drawOver(c, now, best);
}

}  // namespace coco
