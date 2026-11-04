// Coco Touch Advance: menu and info screens. No Arduino dependency (sim/ can draw them).
#pragma once
#include <stdint.h>

#include "kibo_canvas.h"

namespace coco {

using kibo::Canvas;

struct MenuRow {
  const char* label;
  const char* value;  // shown right-aligned (settings), or nullptr
};

// Title bar (with `clock` on the right, may be empty) and a scrolling list;
// the selected row is drawn inverted, values are right-aligned.
void drawList(Canvas& c, const char* title, const MenuRow* rows, int count, int selected, const char* clock);

// "My Coco": food, fun and energy bars, how Coco feels, slaps today.
void drawStats(Canvas& c, int food, int fun, int energy, const char* feeling, int slaps, int pets);

// About: version, board, screen, number of faces, free memory.
void drawAbout(Canvas& c, const char* version, const char* board, const char* screen, int faces, int freeKb);

// Joke: Coco's little eyes and `title` ("Coding joke") on top, the text word-wrapped
// below (up to 6 lines). punch = the punchline: happy eyes.
void drawJoke(Canvas& c, const char* title, const char* text, bool punch, uint32_t t);

// A dark strip at the bottom of the face with a centred label.
void drawFooter(Canvas& c, const char* text);

// Small toast in the top-right corner (over the menu clock) ("saved", "pinned").
void drawToast(Canvas& c, const char* text);

}  // namespace coco
