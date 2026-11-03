#include "coco_cards.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace coco {

using kibo::clampi;
using kibo::isin;
using kibo::kFontBig;
using kibo::kFontMedium;
using kibo::kFontSmall;

Sky skyFromWmo(int code) {
  if (code < 0) return Sky::Unknown;
  if (code == 0) return Sky::Clear;
  if (code <= 2) return Sky::PartlyCloudy;
  if (code == 3) return Sky::Cloudy;
  if (code == 45 || code == 48) return Sky::Fog;
  if (code >= 51 && code <= 57) return Sky::Drizzle;
  if ((code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return Sky::Rain;
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) return Sky::Snow;
  if (code >= 95) return Sky::Thunder;
  return Sky::Unknown;
}

Sky skyFromOwm(int id) {
  // https://openweathermap.org/weather-conditions: 2xx storm, 3xx drizzle, 5xx rain,
  // 6xx snow, 7xx mist/fog/haze, 800 clear, 801-802 few/scattered clouds, 803-804 cloudy
  if (id >= 200 && id < 300) return Sky::Thunder;
  if (id >= 300 && id < 400) return Sky::Drizzle;
  if (id >= 500 && id < 600) return Sky::Rain;
  if (id >= 600 && id < 700) return Sky::Snow;
  if (id >= 700 && id < 800) return Sky::Fog;
  if (id == 800) return Sky::Clear;
  if (id == 801 || id == 802) return Sky::PartlyCloudy;
  if (id == 803 || id == 804) return Sky::Cloudy;
  return Sky::Unknown;
}

const char* skyName(Sky s) {
  switch (s) {
    case Sky::Clear: return "Clear";
    case Sky::PartlyCloudy: return "Few clouds";
    case Sky::Cloudy: return "Cloudy";
    case Sky::Fog: return "Foggy";
    case Sky::Drizzle: return "Drizzle";
    case Sky::Rain: return "Rain";
    case Sky::Snow: return "Snow";
    case Sky::Thunder: return "Storm";
    default: return "No data";
  }
}

namespace {

// Finds `"key":` after `from` and reads the number after it (skipping a '[').
bool number(const char* from, const char* key, float& out) {
  char pattern[48];
  snprintf(pattern, sizeof(pattern), "\"%s\":", key);
  const char* p = strstr(from, pattern);
  if (!p) return false;
  p += strlen(pattern);
  while (*p == ' ' || *p == '[') p++;
  char* end = nullptr;
  double v = strtod(p, &end);
  if (end == p) return false;
  out = (float)v;
  return true;
}

// ---- icon parts ---------------------------------------------------------

void cloud(Canvas& c, int cx, int cy, int s) {
  // s = 10 gives a ~30x17 px cloud; lit body with a dark outline so it can overlap a sun
  for (int pass = 0; pass < 2; pass++) {
    int g = pass == 0 ? 1 : 0;
    c.setColor(pass);
    c.circle(cx - s, cy + s / 3, s * 6 / 10 + g);
    c.circle(cx + s, cy + s / 3, s * 6 / 10 + g);
    c.circle(cx - s / 4, cy - s / 3, s * 9 / 10 + g);
    c.circle(cx + s * 6 / 10, cy - s / 10, s * 7 / 10 + g);
    c.roundRect(cx - s - s * 6 / 10 - g, cy, 2 * s + s * 12 / 10 + 2 * g, s * 6 / 10 + 1 + g, s / 4);
  }
  c.setColor(1);
}

void sun(Canvas& c, int cx, int cy, int r, uint32_t t) {
  c.circle(cx, cy, r);
  int spin = (int)(t * 1024 / 8000);  // one turn in 8 s
  for (int i = 0; i < 8; i++) {
    int ph = spin + i * 128;
    int pulse = isin((int)(t * 1024 / 900) + i * 256, 2);
    int r0 = r + 3, r1 = r + 7 + pulse;
    int x0 = cx + isin(ph + 256, r0), y0 = cy + isin(ph, r0);
    int x1 = cx + isin(ph + 256, r1), y1 = cy + isin(ph, r1);
    c.line(x0, y0, x1, y1, 2);
  }
}

void moon(Canvas& c, int cx, int cy, int r, uint32_t t) {
  c.circle(cx, cy, r);
  c.setColor(0);
  c.circle(cx + r / 2 + 1, cy - r / 3, r - 1);
  c.setColor(1);
  // twinkling stars
  static const int8_t kStars[3][2] = {{14, -10}, {18, 4}, {6, -16}};
  for (int i = 0; i < 3; i++) {
    uint32_t p = (t + i * 500) % 1500;
    int s = p < 750 ? (int)(p / 250) : (int)((1500 - p) / 250);
    int x = cx + kStars[i][0], y = cy + kStars[i][1];
    if (s > 0) {
      c.hline(x - s, x + s, y);
      c.vline(x, y - s, y + s);
    } else {
      c.pixel(x, y);
    }
  }
}

}  // namespace

bool parseOpenMeteo(const char* json, Weather& out) {
  if (!json) return false;
  const char* cur = strstr(json, "\"current\":");
  const char* daily = strstr(json, "\"daily\":");
  if (!cur) return false;
  Weather w;
  float code = -1, hum = 0, day = 1;
  if (!number(cur, "temperature_2m", w.tempC)) return false;
  if (!number(cur, "weather_code", code)) return false;
  number(cur, "relative_humidity_2m", hum);
  number(cur, "is_day", day);
  w.hiC = w.loC = w.tempC;
  if (daily) {
    number(daily, "temperature_2m_max", w.hiC);
    number(daily, "temperature_2m_min", w.loC);
  }
  w.humidity = (int)lroundf(hum);
  w.sky = skyFromWmo((int)code);
  w.isDay = day > 0.5f;
  float off = 0;
  w.hasOffset = number(json, "utc_offset_seconds", off);
  w.utcOffsetSec = (int)off;
  w.valid = true;
  out = w;
  return true;
}

bool parseOpenWeatherMap(const char* json, Weather& out) {
  if (!json) return false;
  const char* cond = strstr(json, "\"weather\":");
  const char* main = strstr(json, "\"main\":{");  // the object, not "main":"Clear"
  if (!cond || !main) return false;
  Weather w;
  float id = -1, hum = 0, off = 0;
  if (!number(cond, "id", id)) return false;
  if (!number(main, "temp", w.tempC)) return false;
  w.hiC = w.loC = w.tempC;
  number(main, "temp_max", w.hiC);
  number(main, "temp_min", w.loC);
  number(main, "humidity", hum);
  w.humidity = (int)lroundf(hum);
  w.sky = skyFromOwm((int)id);
  const char* icon = strstr(cond, "\"icon\":\"");  // e.g. "01d" (day) or "01n" (night)
  w.isDay = !(icon && icon[10] == 'n');
  w.hasOffset = number(json, "timezone", off);  // seconds east of UTC, DST included
  w.utcOffsetSec = (int)off;
  w.valid = true;
  out = w;
  return true;
}

bool parseOwmGeocode(const char* json, float& lat, float& lon, char* name, int nameLen) {
  if (!json || !strchr(json, '{')) return false;  // "[]": city not found
  if (!number(json, "lat", lat) || !number(json, "lon", lon)) return false;
  if (name && nameLen > 0) {
    name[0] = 0;
    const char* p = strstr(json, "\"name\":\"");  // the English name comes before local_names
    if (p) {
      p += 8;
      int n = 0;
      while (*p && *p != '"' && n < nameLen - 1) name[n++] = *p++;
      name[n] = 0;
    }
  }
  return true;
}

bool parseGeocode(const char* json, float& lat, float& lon, char* name, int nameLen) {
  if (!json) return false;
  const char* res = strstr(json, "\"results\":");
  if (!res || !number(res, "latitude", lat) || !number(res, "longitude", lon)) return false;
  if (name && nameLen > 0) {
    name[0] = 0;
    const char* p = strstr(res, "\"name\":\"");
    if (p) {
      p += 8;
      int n = 0;
      while (*p && *p != '"' && n < nameLen - 1) name[n++] = *p++;
      name[n] = 0;
    }
  }
  return true;
}

void posixTz(int sec, char* out, int outLen) {
  // POSIX counts the other way round: UTC+9 is "-9"
  int a = sec < 0 ? -sec : sec;
  int h = a / 3600, m = (a % 3600) / 60;
  snprintf(out, outLen, "<%c%02d%02d>%c%d:%02d", sec < 0 ? '-' : '+', h, m, sec < 0 ? '+' : '-', h, m);
}

void drawWeatherIcon(Canvas& c, int cx, int cy, Sky sky, bool day, uint32_t t) {
  int drift = isin((int)(t * 1024 / 3000), 2);  // clouds bob left/right
  switch (sky) {
    case Sky::Clear:
      if (day) sun(c, cx, cy, 8, t);
      else moon(c, cx - 3, cy + 2, 11, t);
      break;
    case Sky::PartlyCloudy:
      if (day) sun(c, cx + 6, cy - 7, 6, t);
      else moon(c, cx + 5, cy - 6, 8, t);
      cloud(c, cx - 3 + drift, cy + 5, 9);
      break;
    case Sky::Cloudy:
      cloud(c, cx + 5 - drift, cy - 6, 7);
      cloud(c, cx - 3 + drift, cy + 4, 10);
      break;
    case Sky::Fog:
      cloud(c, cx + drift, cy - 6, 9);
      for (int i = 0; i < 3; i++) {
        int off = isin((int)(t * 1024 / 2000) + i * 300, 4);
        c.roundRect(cx - 16 + off + (i & 1) * 4, cy + 8 + i * 5, 28, 2, 1);
      }
      break;
    case Sky::Drizzle:
    case Sky::Rain:
    case Sky::Thunder: {
      cloud(c, cx + drift, cy - 6, 10);
      int drops = sky == Sky::Drizzle ? 3 : 5;
      int len = sky == Sky::Drizzle ? 2 : 4;
      for (int i = 0; i < drops; i++) {
        uint32_t p = (t + i * 233) % 700;
        int x = cx - 12 + i * (24 / (drops - 1));
        int y = cy + 5 + (int)(p * 14 / 700);
        c.line(x, y, x - 2, y + len, 2);
      }
      if (sky == Sky::Thunder && (t % 1600) < 500) {
        c.setColor(0);
        c.triangle(cx - 1, cy + 2, cx + 7, cy + 2, cx - 4, cy + 13);
        c.setColor(1);
        c.triangle(cx, cy + 3, cx + 5, cy + 3, cx - 2, cy + 11);
        c.triangle(cx - 3, cy + 10, cx + 3, cy + 10, cx - 5, cy + 19);
      }
      break;
    }
    case Sky::Snow:
      cloud(c, cx + drift, cy - 6, 10);
      for (int i = 0; i < 4; i++) {
        uint32_t p = (t + i * 400) % 1600;
        int x = cx - 12 + i * 8 + isin((int)(p * 1024 / 800), 2);
        int y = cy + 6 + (int)(p * 14 / 1600);
        c.hline(x - 1, x + 1, y);
        c.vline(x, y - 1, y + 1);
      }
      break;
    default:
      c.textCentered(cx, cy - 5, "?", kFontMedium);
      c.frame(cx - 12, cy - 12, 24, 24);
      break;
  }
}

void drawWeather(Canvas& c, const Weather& w, const char* place, uint32_t t) {
  if (!w.valid) {
    drawWeatherIcon(c, 24, 26, Sky::Unknown, true, t);
    c.text(50, 14, "No weather", kFontMedium);
    c.text(50, 30, "Hold my head", kFontSmall);
    c.text(50, 39, "10 s: Wi-Fi setup", kFontSmall);
    return;
  }
  drawWeatherIcon(c, 22, 24, w.sky, w.isDay, t);

  // temperature: big digits, optional minus, degree ring + "C"
  int temp = (int)lroundf(w.tempC);
  char digits[12];
  snprintf(digits, sizeof(digits), "%d", abs(clampi(temp, -99, 99)));
  int x = 50;
  if (temp < 0) {
    c.rect(x, 14, 6, 3);
    x += 8;
  }
  x += c.text(x, 2, digits, kFontBig);
  c.ring(x + 4, 6, 3, 1);
  c.text(x + 9, 4, "C", kFontMedium);

  const char* name = skyName(w.sky);
  c.text(50, 30, name, kFontMedium);

  char line[32];
  snprintf(line, sizeof(line), "H%d L%d  %d%%", (int)lroundf(w.hiC), (int)lroundf(w.loC), w.humidity);
  c.text(50, 44, line, kFontSmall);
  if (place && *place) c.textCentered(22, 50, place, kFontSmall);
  c.hline(46, 127, 54);
  // a tiny Coco peeking from the bottom-right corner
  c.roundRect(104, 57, 7, 7, 2);
  c.roundRect(116, 57, 7, 7, 2);
}

void drawStatus(Canvas& c, int batteryPct, int batteryMv, const char* wifi, int weatherAgeMin,
                const char* version) {
  c.text(2, 0, "Coco", kFontMedium);
  if (version) c.text(128 - c.textWidth(version, kFontSmall) - 1, 2, version, kFontSmall);
  c.hline(0, 127, 12);

  // big battery
  int pct = clampi(batteryPct, 0, 100);
  c.frame(2, 17, 30, 14);
  c.rect(32, 21, 2, 6);
  if (batteryMv > 0) c.rect(4, 19, 26 * pct / 100, 10);

  char line[32];
  if (batteryMv > 0) {
    snprintf(line, sizeof(line), "%d%%", pct);
    c.text(40, 15, line, kFontMedium);
    snprintf(line, sizeof(line), "%d.%02d V", batteryMv / 1000, (batteryMv % 1000) / 10);
    c.text(40, 26, line, kFontSmall);
  } else {
    c.text(40, 19, "USB power", kFontMedium);
  }

  snprintf(line, sizeof(line), "Wi-Fi   %s", wifi ? wifi : "-");
  c.text(2, 38, line, kFontSmall);
  if (weatherAgeMin < 0) snprintf(line, sizeof(line), "Weather none yet");
  else if (weatherAgeMin < 120) snprintf(line, sizeof(line), "Weather %d min ago", weatherAgeMin);
  else snprintf(line, sizeof(line), "Weather %d h ago", weatherAgeMin / 60);
  c.text(2, 47, line, kFontSmall);
  c.text(2, 56, "tap: next  2x tap: menu", kFontSmall);
}

void drawMenu(Canvas& c, const char* const* items, int count, int selected, const char* clock) {
  c.text(2, 0, "Menu", kFontMedium);
  if (clock && clock[0]) c.text(128 - c.textWidth(clock, kFontSmall) - 1, 2, clock, kFontSmall);
  c.hline(0, 127, 12);
  // 5 rows of 10 px under the title; scroll so the selected row is always on screen
  constexpr int kRows = 5, kRowH = 10, kTop = 14;
  int first = clampi(selected - kRows / 2, 0, count > kRows ? count - kRows : 0);
  for (int r = 0; r < kRows && first + r < count; r++) {
    int i = first + r, y = kTop + r * kRowH;
    c.text(6, y + 1, items[i], kFontSmall);
    if (i == selected) c.invert(0, y, 124, kRowH);
  }
  // scroll bar when the list is longer than the screen
  if (count > kRows) {
    int h = kRows * kRowH, barH = h * kRows / count;
    c.vline(126, kTop, kTop + h - 1);
    c.rect(125, kTop + (h - barH) * first / (count - kRows), 3, barH);
  }
}

void drawHold(Canvas& c, uint32_t held) {
  c.setColor(0);
  c.rect(0, 44, kibo::kWidth, 20);
  c.setColor(1);
  c.frame(0, 44, kibo::kWidth, 20);
  const char* label;
  uint32_t from, to;
  if (held < 5000) {
    label = "keep holding: sleep";
    from = 2000;
    to = 5000;
  } else if (held < 10000) {
    label = "let go = sleep / hold = Wi-Fi";
    from = 5000;
    to = 10000;
  } else {
    label = "let go = Wi-Fi setup";
    from = 0;
    to = 1;
  }
  c.textCentered(64, 46, label, kFontSmall);
  int pct = held >= to ? 100 : held <= from ? 0 : (int)((held - from) * 100 / (to - from));
  c.frame(6, 55, 116, 6);
  c.rect(7, 56, 114 * pct / 100, 4);
}

}  // namespace coco
