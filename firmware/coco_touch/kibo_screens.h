// Non-face screens: Wi-Fi setup QR, activation code, clock, notices, overlays.
#pragma once
#include <stdint.h>

#include "kibo_canvas.h"

namespace kibo {

// Wi-Fi provisioning. Step 0: QR that joins the phone to Kibo's hotspot.
// Step 1 (phone joined): QR with the setup page URL.
void drawWifiSetup(Canvas& c, const char* apSsid, const char* url, bool phoneJoined, uint32_t t);

// XiaoZhi activation: shows the 6-digit code to enter at xiaozhi.me.
void drawActivation(Canvas& c, const char* code, uint32_t t);

// Big clock. `date` e.g. "Fri 02 Oct"; colon blinks with t.
void drawClock(Canvas& c, int hour, int minute, const char* date, uint32_t t);

// Title + wrapped message, with a small sad/neutral face icon on the left.
void drawNotice(Canvas& c, const char* title, const char* message, bool error);

// Bottom-line marquee (speech subtitles). Draws over the face on a black band.
void drawSubtitle(Canvas& c, const char* text, uint32_t t);

// Long-press menu progress: 3-5 s "nap", 5-10 s "Wi-Fi reset".
void drawHoldMenu(Canvas& c, uint32_t heldMs);

// Boot animation: eyes open from a line, "KIBO" title. Returns true when done.
bool drawBoot(Canvas& c, uint32_t t, const char* version);

// Small status icons (top-right corner).
void drawBatteryIcon(Canvas& c, int x, int y, int percent, bool charging);
void drawOfflineIcon(Canvas& c, int x, int y);

}  // namespace kibo
