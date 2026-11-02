#include "kibo_screens.h"

#include <stdio.h>
#include <string.h>

#include "kibo_qr.h"

namespace kibo {

namespace {

// Draws a QR code as dark modules on a lit square (with quiet zone) at x,y.
// Returns the side length in pixels.
int drawQr(Canvas& c, const char* text, int x, int y, int maxSide) {
  static qr::Code code;
  static char cached[160] = "";
  if (strncmp(cached, text, sizeof(cached)) != 0 || code.size == 0) {
    strncpy(cached, text, sizeof(cached) - 1);
    cached[sizeof(cached) - 1] = 0;
    if (!qr::encode(text, code)) return 0;
  }
  int n = code.size;
  int scale = maxSide / (n + 2);  // at least 1 module of quiet zone each side
  if (scale < 1) scale = 1;
  int side = n * scale;
  int pad = (maxSide - side) / 2;
  c.setColor(1);
  c.rect(x, y, maxSide, maxSide);
  c.setColor(0);
  for (int my = 0; my < n; my++)
    for (int mx = 0; mx < n; mx++)
      if (code.dark(mx, my)) c.rect(x + pad + mx * scale, y + pad + my * scale, scale, scale);
  c.setColor(1);
  return maxSide;
}

void miniFace(Canvas& c, int x, int y, bool sad) {
  c.roundRect(x, y, 5, 7, 2);
  c.roundRect(x + 9, y, 5, 7, 2);
  if (sad) c.arcUp(x + 7, y + 12, 3, 2, 1);
  else c.arcDown(x + 7, y + 9, 3, 2, 1);
}

}  // namespace

void drawWifiSetup(Canvas& c, const char* apSsid, const char* url, bool phoneJoined, uint32_t t) {
  char payload[96];
  if (!phoneJoined) {
    // Standard Wi-Fi QR: phones offer to join the (open) setup hotspot.
    snprintf(payload, sizeof(payload), "WIFI:T:nopass;S:%s;;", apSsid);
  } else {
    snprintf(payload, sizeof(payload), "%s", url);
  }
  drawQr(c, payload, 0, 0, 64);

  int x = 68;
  if (!phoneJoined) {
    c.text(x, 0, "Scan me!", kFontMedium);
    c.text(x, 12, "1. join Wi-Fi", kFontSmall);
    c.textWrapped(x, 21, 60, 2, apSsid, kFontSmall);
    c.text(x, 38, "2. pick your", kFontSmall);
    c.text(x, 46, "   home Wi-Fi", kFontSmall);
  } else {
    c.text(x, 0, "Joined!", kFontMedium);
    c.text(x, 12, "Scan again or", kFontSmall);
    c.text(x, 20, "open:", kFontSmall);
    const char* host = strstr(url, "//");
    c.textWrapped(x, 30, 60, 2, host ? host + 2 : url, kFontSmall);
  }
  // little blinking eyes so it still looks like Kibo
  bool blink = (t % 3000) < 120;
  if (blink) {
    c.hline(x + 40, x + 46, 59);
    c.hline(x + 50, x + 56, 59);
  } else {
    c.roundRect(x + 40, 55, 7, 9, 2);
    c.roundRect(x + 50, 55, 7, 9, 2);
  }
}

void drawActivation(Canvas& c, const char* code, uint32_t t) {
  c.textCentered(64, 0, "Activate me at", kFontSmall);
  c.textCentered(64, 9, "xiaozhi.me", kFontMedium);
  // code in big digits, spaced
  int len = (int)strlen(code);
  if (len > 6) len = 6;
  int digitW = 0;
  char d[2] = {0, 0};
  for (int i = 0; i < len; i++) {
    d[0] = code[i];
    digitW += c.textWidth(d, kFontBig) + 2;
  }
  int x = 64 - digitW / 2;
  for (int i = 0; i < len; i++) {
    d[0] = code[i];
    x += c.text(x, 24, d, kFontBig) + 2;
  }
  if ((t / 600) % 2) c.hline(10, 117, 62);
}

void drawClock(Canvas& c, int hour, int minute, const char* date, uint32_t t) {
  char hh[3], mm[3];
  snprintf(hh, sizeof(hh), "%02d", hour);
  snprintf(mm, sizeof(mm), "%02d", minute);
  int wh = c.textWidth(hh, kFontBig), wm = c.textWidth(mm, kFontBig), wc = c.textWidth(":", kFontBig);
  int x = 64 - (wh + wc + wm + 4) / 2;
  x += c.text(x, 4, hh, kFontBig) + 2;
  if ((t % 1000) < 600) c.text(x, 2, ":", kFontBig);
  x += wc + 2;
  c.text(x, 4, mm, kFontBig);
  if (date && *date) c.textCentered(64, 46, date, kFontMedium);
  // sleepy eyes peeking at the bottom
  c.roundRect(52, 59, 8, 5, 2);
  c.roundRect(68, 59, 8, 5, 2);
}

void drawNotice(Canvas& c, const char* title, const char* message, bool error) {
  miniFace(c, 2, 1, error);
  c.textWrapped(22, 3, 106, 1, title && *title ? title : (error ? "Oops" : "Coco"), kFontMedium);
  c.hline(0, 127, 18);
  if (message) c.textWrapped(0, 22, 128, 5, message, kFontSmall, 1);
}

void drawSubtitle(Canvas& c, const char* text, uint32_t t) {
  if (!text || !*text) return;
  int w = c.textWidth(text, kFontSmall);
  c.setColor(0);
  c.rect(0, 55, kWidth, 9);
  c.setColor(1);
  int x;
  if (w <= kWidth - 4) {
    x = (kWidth - w) / 2;
  } else {
    // hold 1 s, scroll at ~60 px/s, hold 1.5 s at the end
    int travel = w - (kWidth - 4);
    int scrolled = t < 1000 ? 0 : (int)((t - 1000) * 60 / 1000);
    if (scrolled > travel) scrolled = travel;
    x = 2 - scrolled;
  }
  c.text(x, 56, text, kFontSmall);
}

void drawHoldMenu(Canvas& c, uint32_t held) {
  c.setColor(0);
  c.rect(0, 44, kWidth, 20);
  c.setColor(1);
  c.frame(0, 44, kWidth, 20);
  const char* label;
  uint32_t from, to;
  if (held < 5000) {
    label = "keep holding: nap";
    from = 2000;
    to = 5000;
  } else if (held < 10000) {
    label = "let go = nap / hold = Wi-Fi";
    from = 5000;
    to = 10000;
  } else {
    label = "let go to reset Wi-Fi";
    from = 0;
    to = 1;
  }
  c.textCentered(64, 46, label, kFontSmall);
  int pct = held >= to ? 100 : held <= from ? 0 : (int)((held - from) * 100 / (to - from));
  c.frame(6, 55, 116, 6);
  c.rect(7, 56, 114 * pct / 100, 4);
}

bool drawBoot(Canvas& c, uint32_t t, const char* version) {
  // 0-600 ms: line grows; 600-1100 ms: eyes open; then title
  if (t < 600) {
    int w = (int)(t * 26 / 600);
    c.rect(38 - w / 2, 27, w, 3);
    c.rect(90 - w / 2, 27, w, 3);
  } else {
    int h = t < 1100 ? 3 + (int)((t - 600) * 31 / 500) : 34;
    c.roundRectC(38, 28, 28, h, 9);
    c.roundRectC(90, 28, 28, h, 9);
  }
  if (t > 1100) {
    c.textCentered(64, 50, "COCO", kFontMedium);
    if (version) c.text(128 - c.textWidth(version, kFontSmall), 57, version, kFontSmall);
  }
  return t > 2200;
}

void drawBatteryIcon(Canvas& c, int x, int y, int percent, bool charging) {
  c.frame(x, y, 12, 7);
  c.rect(x + 12, y + 2, 1, 3);
  int bars = (percent + 24) / 25;
  if (bars > 4) bars = 4;
  for (int i = 0; i < bars; i++) c.rect(x + 2 + i * 2 + i / 2, y + 2, 2, 3);
  if (charging) {
    c.setColor(0);
    c.rect(x + 4, y + 1, 4, 5);
    c.setColor(1);
    c.line(x + 7, y + 1, x + 5, y + 3, 1);
    c.line(x + 5, y + 3, x + 7, y + 3, 1);
    c.line(x + 7, y + 3, x + 5, y + 5, 1);
  }
}

void drawOfflineIcon(Canvas& c, int x, int y) {
  // tiny wifi arcs with a slash
  c.arcUp(x + 5, y + 6, 5, 5, 1);
  c.arcUp(x + 5, y + 6, 2, 2, 1);
  c.line(x, y, x + 10, y + 7, 1);
}

}  // namespace kibo
