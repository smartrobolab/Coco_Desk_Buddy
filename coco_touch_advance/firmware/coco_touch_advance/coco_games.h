// Coco Touch Advance: one-button mini games on the 128x64 OLED.
//
//   Dino Run   tap = jump over cacti and birds; it speeds up
//   Sky Hop    tap = flap through the gaps
//   Stacker    tap = drop the sliding block; only the overlap stays
//   Reflex     wait for "TAP!", then tap as fast as you can (ms, lower is better)
//
// Dino Run, Sky Hop and Stacker also have an autopilot, so Coco can play by itself.
// No Arduino dependency: the PC simulator can run them.
#pragma once
#include <stdint.h>

#include "kibo_canvas.h"

namespace coco {

using kibo::Canvas;

enum class Game : uint8_t { Dino, Hop, Stack, Reflex, Count };

const char* gameName(Game g);
bool gameLowerIsBetter(Game g);  // Reflex: the best score is the smallest
bool gameHasAutopilot(Game g);

class Arcade {
 public:
  void start(Game g, uint32_t now, uint32_t seed, bool autopilot);
  void press(uint32_t now);  // the pad was touched (rising edge)
  void update(uint32_t now);  // once per frame; the autopilot restarts by itself after a game over
  // best: the saved best score (0 = none yet), shown on the game-over panel.
  void draw(Canvas& c, uint32_t now, int best) const;

  Game game() const { return game_; }
  bool over() const { return over_; }
  bool autopilot() const { return auto_; }
  int score() const { return score_; }
  bool scoreCounts() const { return over_ && valid_; }  // false: Reflex "too soon"
  uint32_t overSince() const { return overAt_; }

 private:
  struct Obstacle {
    float x;
    int8_t w, h, top;
    uint8_t kind;  // 0 cactus, 1 bird
    bool live;
    float jitter;  // autopilot timing error for this one
  };
  struct Pipe {
    float x;
    int8_t gapY;
    bool passed, live;
  };
  struct Layer {
    int16_t x, w;
  };

  uint32_t rand();
  void gameOver(uint32_t now);
  void updateDino(float dt, uint32_t now);
  void updateHop(float dt, uint32_t now);
  void updateStack(float dt, uint32_t now);
  void updateReflex(uint32_t now);
  void dropBlock(uint32_t now);
  void drawDino(Canvas& c, uint32_t now) const;
  void drawHop(Canvas& c, uint32_t now) const;
  void drawStack(Canvas& c, uint32_t now) const;
  void drawReflex(Canvas& c, uint32_t now) const;
  void drawOver(Canvas& c, uint32_t now, int best) const;

  Game game_ = Game::Dino;
  bool auto_ = false, over_ = false, started_ = false, valid_ = true;
  uint32_t seed_ = 1, startAt_ = 0, lastAt_ = 0, overAt_ = 0;
  int score_ = 0;
  float dist_ = 0, speed_ = 0;

  // Dino + Hop: the player
  float py_ = 0, vy_ = 0;
  Obstacle obs_[5] = {};
  float gap_ = 0;
  Pipe pipes_[4] = {};

  // Stacker
  static constexpr int kMaxLayers = 48;
  Layer layers_[kMaxLayers] = {};
  int nLayers_ = 0;
  float bx_ = 0, bdir_ = 1;
  int bw_ = 0;
  int chopX_ = 0, chopW_ = 0, chopLayer_ = 0;
  uint32_t chopAt_ = 0, perfectAt_ = 0;
  float aimErr_ = 0;

  // Reflex
  uint32_t goAt_ = 0;
};

}  // namespace coco
