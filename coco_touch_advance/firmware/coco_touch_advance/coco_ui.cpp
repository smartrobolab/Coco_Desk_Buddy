#include "coco_ui.h"

#include <stdio.h>

namespace coco {

using kibo::clampi;
using kibo::kFontMedium;
using kibo::kFontSmall;
using kibo::kWidth;

void drawList(Canvas& c, const char* title, const MenuRow* rows, int count, int selected, const char* clock) {
  c.text(2, 0, title, kFontMedium);
  if (clock && clock[0]) c.text(kWidth - c.textWidth(clock, kFontSmall) - 1, 2, clock, kFontSmall);
  c.hline(0, 127, 12);
  constexpr int kRows = 5, kRowH = 10, kTop = 14;
  int first = clampi(selected - kRows / 2, 0, count > kRows ? count - kRows : 0);
  bool bar = count > kRows;
  int right = bar ? 122 : 126;
  for (int r = 0; r < kRows && first + r < count; r++) {
    int i = first + r, y = kTop + r * kRowH;
    c.text(4, y + 1, rows[i].label, kFontSmall);
    if (rows[i].value && rows[i].value[0]) {
      int w = c.textWidth(rows[i].value, kFontSmall);
      c.text(right - w, y + 1, rows[i].value, kFontSmall);
    }
    if (i == selected) c.invert(0, y, bar ? 124 : 128, kRowH);
  }
  if (bar) {
    int h = kRows * kRowH, barH = h * kRows / count;
    if (barH < 4) barH = 4;
    c.vline(126, kTop, kTop + h - 1);
    c.rect(125, kTop + (h - barH) * first / (count - kRows), 3, barH);
  }
}

static void statBar(Canvas& c, int y, const char* name, int value) {
  c.text(2, y, name, kFontSmall);
  c.frame(40, y, 64, 7);
  c.rect(41, y + 1, 62 * clampi(value, 0, 100) / 100, 5);
  char v[6];
  snprintf(v, sizeof(v), "%d", clampi(value, 0, 100));
  c.text(127 - c.textWidth(v, kFontSmall), y, v, kFontSmall);
}

void drawStats(Canvas& c, int food, int fun, int energy, const char* feeling, int slaps, int pets) {
  c.text(2, 0, "My Coco", kFontMedium);
  c.hline(0, 127, 12);
  statBar(c, 15, "Food", food);
  statBar(c, 25, "Fun", fun);
  statBar(c, 35, "Energy", energy);
  char line[40];
  snprintf(line, sizeof(line), "I feel %s", feeling);
  c.text(2, 46, line, kFontSmall);
  snprintf(line, sizeof(line), "pets %d  slaps %d", pets, slaps);
  c.text(2, 56, line, kFontSmall);
}

void drawAbout(Canvas& c, const char* version, const char* board, const char* screen, int faces, int freeKb) {
  c.text(2, 0, "About", kFontMedium);
  c.hline(0, 127, 12);
  char line[40];
  snprintf(line, sizeof(line), "Coco Touch Advance %s", version);
  c.text(2, 15, line, kFontSmall);
  c.text(2, 25, board, kFontSmall);
  snprintf(line, sizeof(line), "screen %s", screen);
  c.text(2, 35, line, kFontSmall);
  snprintf(line, sizeof(line), "%d faces, 4 games", faces);
  c.text(2, 45, line, kFontSmall);
  snprintf(line, sizeof(line), "%d KB free memory", freeKb);
  c.text(2, 55, line, kFontSmall);
}

void drawFooter(Canvas& c, const char* text) {
  c.setColor(0);
  c.rect(0, 55, kWidth, 9);
  c.setColor(1);
  c.textCentered(64, 56, text, kFontSmall);
}

void drawToast(Canvas& c, const char* text) {
  int w = c.textWidth(text, kFontSmall) + 4;
  int x = kWidth - w - 1;
  c.setColor(0);
  c.rect(x, 0, w + 1, 11);
  c.setColor(1);
  c.frame(x, 0, w + 1, 11);
  c.text(x + 2, 2, text, kFontSmall);
}

}  // namespace coco
