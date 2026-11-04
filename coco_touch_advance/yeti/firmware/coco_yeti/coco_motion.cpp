#include "coco_motion.h"

#include <math.h>

namespace coco {

const char* motionEventName(MotionEvent e) {
  static const char* const k[] = {"none", "fall", "landed", "caught", "knock", "shake", "shake end", "spin", "moved", "orient"};
  return k[(int)e];
}

const char* orientationName(Orientation o) {
  static const char* const k[] = {"upright", "upside down", "on its back", "face down", "on its left", "on its right"};
  return k[(int)o];
}

namespace {
constexpr float kFreeFallG = 0.40f;      // |a| below this = falling
constexpr uint32_t kFreeFallMs = 70;     // for this long
constexpr float kImpactG = 1.9f;         // |a| above this after a fall = landed
constexpr uint32_t kImpactWindowMs = 600;
constexpr float kSpikeG = 0.85f;         // deviation from gravity for a knock
constexpr uint32_t kSpikeMaxMs = 50;     // a knock is short
constexpr uint32_t kCalmBeforeMs = 150;  // ...and comes out of stillness
constexpr float kCalmEnergy = 0.12f;
constexpr float kShakeEnergy = 0.55f;
constexpr uint32_t kShakeStartMs = 400;
constexpr uint32_t kShakeEndMs = 300;
constexpr float kSpinDps = 400.0f;
constexpr uint32_t kSpinMs = 300;
constexpr uint32_t kRestedMs = 3000;     // still this long, then movement = "Moved"
constexpr uint32_t kOrientMs = 500;
}  // namespace

MotionEvent MotionDetector::update(float ax, float ay, float az, float gx, float gy, float gz) {
  if (!init_) {
    gx_ = ax;
    gy_ = ay;
    gz_ = az;
    init_ = true;
  }
  float mag = sqrtf(ax * ax + ay * ay + az * az);
  mag_ = mag;
  float rate = sqrtf(gx * gx + gy * gy + gz * gz);
  // Gravity estimate. When |a| is close to 1 g the reading is (almost) only
  // gravity, even while Coco is slowly turned, so it follows quickly; when
  // |a| is far from 1 g (shaken, hit, falling) it barely moves.
  float off = fabsf(mag - 1.0f);
  float k = off < 0.12f ? (rate < 250.0f ? 0.3f : 0.05f) : off < 0.4f ? 0.02f : 0.0f;
  gx_ += (ax - gx_) * k;
  gy_ += (ay - gy_) * k;
  gz_ += (az - gz_) * k;
  // deviation from the gravity estimate = how hard it is being moved
  float dx = ax - gx_, dy = ay - gy_, dz = az - gz_;
  float dev = sqrtf(dx * dx + dy * dy + dz * dz);
  energy_ += (dev - energy_) * 0.08f;
  bool calmNow = energy_ < kCalmEnergy;

  MotionEvent ev = MotionEvent::None;
  auto raise = [&](MotionEvent e) {
    // priority: fall/landed/caught > knock > shake > spin > orient > moved
    auto rank = [](MotionEvent m) {
      switch (m) {
        case MotionEvent::FallStart: case MotionEvent::Landed: case MotionEvent::Caught: return 6;
        case MotionEvent::Knock: return 5;
        case MotionEvent::ShakeStart: case MotionEvent::ShakeEnd: return 4;
        case MotionEvent::Spin: return 3;
        case MotionEvent::Orient: return 2;
        case MotionEvent::Moved: return 1;
        default: return 0;
      }
    };
    if (rank(e) > rank(ev)) ev = e;
  };

  // ---- free fall, then landed or caught
  if (mag < kFreeFallG) {
    lowMs_ += kDt;
    if (!falling_ && lowMs_ >= kFreeFallMs) {
      falling_ = true;
      fallMs_ = lowMs_;
      raise(MotionEvent::FallStart);
    }
  } else {
    if (falling_) {  // the free fall just ended
      falling_ = false;
      waitImpact_ = true;
      afterFallMs_ = 0;
    }
    lowMs_ = 0;
  }
  if (falling_) fallMs_ += kDt;
  if (waitImpact_) {
    afterFallMs_ += kDt;
    if (mag > kImpactG) {
      waitImpact_ = false;
      knockCooldown_ = 800;  // the bounce is not a knock
      spikePending_ = false;
      raise(MotionEvent::Landed);
    } else if (afterFallMs_ >= kImpactWindowMs) {
      waitImpact_ = false;
      raise(MotionEvent::Caught);
    }
  }

  // ---- knock: a short spike out of stillness, followed by calm
  if (knockCooldown_) knockCooldown_ = knockCooldown_ > kDt ? knockCooldown_ - kDt : 0;
  if (calmNow && dev < 0.3f) calmBeforeMs_ += kDt;
  if (!falling_ && !waitImpact_ && !shaking_) {
    if (dev > kSpikeG) {
      if (!spikePending_ && calmBeforeMs_ >= kCalmBeforeMs && !knockCooldown_) {
        spikePending_ = true;
        spikeMs_ = 0;
        spikePeak_ = 0;
      }
      if (spikePending_) {
        spikeMs_ += kDt;
        if (dev > spikePeak_) spikePeak_ = dev;
        if (spikeMs_ > kSpikeMaxMs) spikePending_ = false;  // too long: a push or a shake
      }
      sinceSpikeMs_ = 0;
    } else if (spikePending_) {
      sinceSpikeMs_ += kDt;
      if (dev > 0.45f && sinceSpikeMs_ > 30) spikePending_ = false;  // keeps moving: not a knock
      if (spikePending_ && sinceSpikeMs_ >= 100) {
        spikePending_ = false;
        knockCooldown_ = 180;
        raise(MotionEvent::Knock);
      }
    }
  }
  if (!(calmNow && dev < 0.3f)) calmBeforeMs_ = dev > kSpikeG ? calmBeforeMs_ : 0;

  // ---- shake
  if (!falling_) {
    if (!shaking_) {
      shakeMs_ = energy_ > kShakeEnergy ? shakeMs_ + kDt : 0;
      if (shakeMs_ >= kShakeStartMs) {
        shaking_ = true;
        calmMs_ = 0;
        spikePending_ = false;
        raise(MotionEvent::ShakeStart);
      }
    } else {
      calmMs_ = energy_ < 0.2f ? calmMs_ + kDt : 0;
      if (calmMs_ >= kShakeEndMs) {
        shaking_ = false;
        shakeMs_ = 0;
        knockCooldown_ = 300;
        raise(MotionEvent::ShakeEnd);
      }
    }
  }

  // ---- spin
  if (rate > kSpinDps) {
    spinMs_ += kDt;
    if (!spun_ && spinMs_ >= kSpinMs) {
      spun_ = true;
      raise(MotionEvent::Spin);
    }
  } else {
    spinMs_ = 0;
    spun_ = false;
  }

  // ---- resting / moved
  if (energy_ < 0.05f && rate < 15.0f) {
    stillMs_ += kDt;
    movingMs_ = 0;
  } else {
    movingMs_ += kDt;
    if (stillMs_ >= kRestedMs && movingMs_ >= 80) {
      stillMs_ = 0;
      raise(MotionEvent::Moved);
    }
    if (movingMs_ >= 80) stillMs_ = 0;
  }

  // ---- orientation, from the gravity estimate, only while fairly calm
  Orientation o = orient_;
  float ux = gx_, uy = gy_, uz = gz_;
  if (uy > 0.6f) o = Orientation::Upright;
  else if (uy < -0.6f) o = Orientation::UpsideDown;
  else if (uz > 0.7f) o = Orientation::OnBack;  // screen pointing at the ceiling
  else if (uz < -0.7f) o = Orientation::FaceDown;
  else if (ux > 0.7f) o = Orientation::OnLeft;  // the accelerometer reads "up": left side down -> up = +x
  else if (ux < -0.7f) o = Orientation::OnRight;
  if (o != candidate_) {
    candidate_ = o;
    candidateMs_ = 0;
  } else if (o != orient_ && energy_ < 0.3f) {
    candidateMs_ += kDt;
    if (candidateMs_ >= kOrientMs) {
      orient_ = o;
      raise(MotionEvent::Orient);
    }
  }
  return ev;
}

}  // namespace coco
