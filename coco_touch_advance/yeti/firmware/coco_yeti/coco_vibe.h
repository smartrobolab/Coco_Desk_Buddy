// Coco Yeti: vibration motor module (coin motor + transistor, signal pin HIGH = on).
// Short patterns play in the background; update() is called every frame.
#pragma once
#include <Arduino.h>

enum class Buzz : uint8_t { Tap, Purr, Giggle, Thump, Alarm, Rumble, Hello, GameOver, Count };

class CocoVibe {
 public:
  void begin(int pin) {
    pin_ = pin;
    if (pin_ < 0) return;
    pinMode(pin_, OUTPUT);
    analogWriteFrequency(pin_, 2000);
    analogWrite(pin_, 0);
  }

  // 0 = off, 1 = soft, 2 = strong
  void setLevel(uint8_t level) {
    level_ = level;
    if (!level_) stop();
  }

  void play(Buzz b, bool repeat = false) {
    if (pin_ < 0 || !level_) return;
    pat_ = kPatterns[(int)b];
    repeat_ = repeat;
    step_ = 0;
    stepAt_ = millis();
    set(pat_[0] > 0);
  }

  bool playing() const { return pat_ != nullptr; }
  bool repeating(Buzz b) const { return pat_ == kPatterns[(int)b] && repeat_; }

  void stop() {
    pat_ = nullptr;
    set(false);
  }

  void update(uint32_t now) {
    if (!pat_) return;
    while (pat_ && now - stepAt_ >= pat_[step_]) {
      stepAt_ += pat_[step_];
      step_++;
      if (pat_[step_] == 0) {  // end of the pattern
        if (!repeat_) {
          stop();
          return;
        }
        step_ = 0;
      }
      set(step_ % 2 == 0);  // even steps vibrate, odd steps pause
    }
  }

 private:
  void set(bool on) {
    if (pin_ < 0) return;
    analogWrite(pin_, on ? (level_ >= 2 ? 255 : 150) : 0);
  }

  // on, off, on, off, ... ms; 0 ends. (Coco's frame is 40 ms, so steps are >= 40 ms.)
  static constexpr uint16_t kTap[] = {45, 0};
  static constexpr uint16_t kPurr[] = {40, 160, 40, 400, 0};
  static constexpr uint16_t kGiggle[] = {50, 60, 50, 60, 50, 0};
  static constexpr uint16_t kThump[] = {160, 0};
  static constexpr uint16_t kAlarm[] = {80, 50, 80, 50, 80, 50, 200, 0};
  static constexpr uint16_t kRumble[] = {120, 60, 0};
  static constexpr uint16_t kHello[] = {50, 80, 120, 0};
  static constexpr uint16_t kGameOver[] = {250, 0};
  static constexpr const uint16_t* kPatterns[(int)Buzz::Count] = {kTap,   kPurr,   kGiggle, kThump,
                                                                  kAlarm, kRumble, kHello,  kGameOver};

  int pin_ = -1;
  uint8_t level_ = 2;
  const uint16_t* pat_ = nullptr;
  bool repeat_ = false;
  uint8_t step_ = 0;
  uint32_t stepAt_ = 0;
};
