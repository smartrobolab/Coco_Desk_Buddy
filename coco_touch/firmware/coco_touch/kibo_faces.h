// Kibo's procedurally drawn OLED faces.
//
// Every face is drawn from simple shapes each frame, so faces can blink, look
// around (gaze follows the IMU tilt) and animate without bitmap assets.
// The first 12 faces follow the Desk Companion expression set
// (https://leroyd.com/): 11 in the tap rotation, plus "Love" on long press.
#pragma once
#include <stdint.h>

#include "kibo_canvas.h"

namespace kibo {

enum class Face : uint8_t {
  // --- tap rotation (Desk Companion set) ---
  Neutral,    // animated, 6 frames - default idle face
  Serious,    // animated, 6 frames
  Sleepy,     // static
  Crying,     // static
  Sad,        // static
  Cute,       // static
  Kissou,     // static
  MiniLove,   // animated, 2 frames
  AnimeLove,  // animated, 8 frames
  Fire,       // animated, 3 frames
  Angry,      // animated, 3 frames
  // --- long press ---
  Love,  // static
  // --- reactions / voice states ---
  Happy,
  Surprised,
  Dizzy,
  Listening,
  Thinking,
  Talking,
  // --- idle activities ---
  Sleeping,
  Eating,
  Playing,
  Music,
  Singing,
  Yawn,
  LowBattery,
  Count
};

constexpr int kTapFaceCount = 11;
extern const Face kTapFaces[kTapFaceCount];

struct FaceCtx {
  uint32_t t = 0;     // ms since this face started (drives animation)
  int lookX = 0;      // gaze offset in pixels (-10..10), e.g. from tilt
  int lookY = 0;      // (-6..6)
  int level = -1;     // 0..100 voice level for Listening/Talking; -1 = animate by itself
  bool talking = false;  // overlay a moving mouth (used while speaking)
};

void drawFace(Canvas& c, Face f, const FaceCtx& ctx);
void drawBlink(Canvas& c, const FaceCtx& ctx);  // transition frame between faces

const char* faceName(Face f);               // "neutral", "anime_love", ...
bool faceFromName(const char* name, Face& out);
int faceFrameCount(Face f);                 // 1 = static, N = animated frames
uint32_t faceLoopMs(Face f);                // length of one full animation loop
bool faceHasMouth(Face f);                  // false -> a talking mouth can be overlaid

// Maps XiaoZhi / LLM emotion strings ("happy", "loving", "shocked"...) to a face.
Face faceForEmotion(const char* emotion);

}  // namespace kibo
