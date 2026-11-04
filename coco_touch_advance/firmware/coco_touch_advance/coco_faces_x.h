// Coco Touch Advance: every face Coco can show, in one list.
//
// The Kibo face engine (kibo_faces.cpp, shared with the other Coco versions and
// left untouched) gives the first set; coco_faces_x.cpp adds the rest: more
// eating faces, slap / anger reactions, moods, hobbies and weather faces.
// Like the face engine, no Arduino dependency, so the PC simulator can draw them.
#pragma once
#include <stdint.h>

#include "kibo_canvas.h"
#include "kibo_faces.h"

namespace coco {

using kibo::Canvas;
using kibo::FaceCtx;

// Order = order in the face gallery.
enum class Ex : uint8_t {
  // happy
  Neutral, Happy, Love, Cute, Laughing, Wink, Shy, StarEyes, Cool, Party, Kissou, MiniLove, AnimeLove,
  // food
  EatCookie, EatPizza, EatNoodles, EatBurger, EatIceCream, EatApple, DrinkJuice, Hungry, Full,
  // sleepy
  Sleepy, Yawn, Sleeping, Dreaming,
  // upset
  Slapped, Angry, Furious, Pouting, Sad, Crying, Scared, Fire,
  // other moods
  Surprised, Confused, Dizzy, Bored, Serious, Thinking, Sneeze,
  // hobbies
  Playing, Music, Singing, Gaming, Workout, Reading,
  // weather + battery
  Cold, Hot, LowBattery,
  Count
};

constexpr int kExCount = (int)Ex::Count;

void drawEx(Canvas& c, Ex e, const FaceCtx& ctx);
const char* exLabel(Ex e);      // "Pizza", "Star eyes", ...
bool exWanders(Ex e);           // the idle gaze may wander on this face
uint32_t exLoopMs(Ex e);        // one animation loop (for activities that should end on a loop)

}  // namespace coco
