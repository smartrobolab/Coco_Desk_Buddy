// Coco Yeti: turns accelerometer + gyro samples into events Coco reacts to.
//
// Feed it one sample every 10 ms (100 Hz) in Coco's own frame:
//   x = to Coco's right, y = up, z = out of the screen (towards you)
//   acceleration in g (standing still upright: x 0, y +1, z 0), rotation in deg/s.
// The axis mapping from the sensor to this frame is in coco_config.h (IMU_MAP_*).
//
// Events:
//   FallStart   free fall (|a| < 0.4 g for 70 ms): Coco was dropped or thrown
//   Landed      hit something hard after a fall
//   Caught      the fall ended softly (caught in a hand)
//   Knock       one short sharp hit while it was still (a slap or a flick)
//   ShakeStart  shaken for 0.4 s                 ShakeEnd   calm again
//   Spin        turned fast (> 400 deg/s for 0.3 s)
//   Moved       picked up / moved after resting still for a while
//   Orient      lying position changed (see orientation())
//
// Pure logic, no Arduino: the PC test in the simulator runs it on recorded or
// synthetic motion.
#pragma once
#include <stdint.h>

namespace coco {

enum class MotionEvent : uint8_t { None, FallStart, Landed, Caught, Knock, ShakeStart, ShakeEnd, Spin, Moved, Orient };
enum class Orientation : uint8_t { Upright, UpsideDown, OnBack, FaceDown, OnLeft, OnRight };

const char* motionEventName(MotionEvent e);
const char* orientationName(Orientation o);

class MotionDetector {
 public:
  // One sample (10 ms apart). Returns the event it caused, or None. If two
  // happen in the same sample, the more important one wins (fall > knock > ...).
  MotionEvent update(float ax, float ay, float az, float gx, float gy, float gz);

  // Tilt from the slow gravity estimate, in g units (-1..1): >0 = leaning right / back.
  float tiltX() const { return gx_; }
  float tiltZ() const { return gz_; }
  float upness() const { return gy_; }  // +1 upright, -1 upside down
  float energy() const { return energy_; }  // how much it is being moved (g), ~0 when still
  float lastMagnitude() const { return mag_; }
  bool shaking() const { return shaking_; }
  bool falling() const { return falling_; }
  Orientation orientation() const { return orient_; }
  uint32_t stillMs() const { return stillMs_; }

 private:
  static constexpr uint32_t kDt = 10;  // ms per sample
  bool init_ = false;
  float gx_ = 0, gy_ = 1, gz_ = 0;  // low-passed gravity
  float energy_ = 0, mag_ = 1;
  // fall
  bool falling_ = false;
  uint32_t lowMs_ = 0, fallMs_ = 0;
  uint32_t afterFallMs_ = 0;  // time since the free fall ended (waiting for an impact)
  bool waitImpact_ = false;
  // knock
  uint32_t spikeMs_ = 0, sinceSpikeMs_ = 100000, calmBeforeMs_ = 0;
  bool spikePending_ = false;
  float spikePeak_ = 0;
  uint32_t knockCooldown_ = 0;
  // shake
  bool shaking_ = false;
  uint32_t shakeMs_ = 0, calmMs_ = 0;
  // spin
  uint32_t spinMs_ = 0;
  bool spun_ = false;
  // still / moved
  uint32_t stillMs_ = 0, movingMs_ = 0;
  // orientation
  Orientation orient_ = Orientation::Upright, candidate_ = Orientation::Upright;
  uint32_t candidateMs_ = 0;
};

}  // namespace coco
