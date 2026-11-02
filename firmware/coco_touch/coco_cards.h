// Coco's info cards: weather, status and the long-press overlay.
//
// Like kibo_screens, no Arduino/ESP-IDF dependency: sim/ renders these on a PC.
#pragma once
#include <stdint.h>

#include "kibo_canvas.h"

namespace coco {

using kibo::Canvas;

enum class Sky : uint8_t { Unknown, Clear, PartlyCloudy, Cloudy, Fog, Drizzle, Rain, Snow, Thunder };

struct Weather {
  bool valid = false;
  float tempC = 0;
  float hiC = 0, loC = 0;
  int humidity = 0;
  Sky sky = Sky::Unknown;
  bool isDay = true;
};

// WMO weather interpretation code (Open-Meteo "weather_code") -> Sky.
Sky skyFromWmo(int code);
const char* skyName(Sky s);

// Parses an Open-Meteo /v1/forecast response requested with
// current=temperature_2m,relative_humidity_2m,weather_code,is_day and
// daily=temperature_2m_max,temperature_2m_min. Returns false if fields are missing.
bool parseOpenMeteo(const char* json, Weather& out);

// Animated weather icon, about 40x40 px around (cx, cy).
void drawWeatherIcon(Canvas& c, int cx, int cy, Sky sky, bool day, uint32_t t);

// Full weather card. `place` is a short label such as "Tokyo".
void drawWeather(Canvas& c, const Weather& w, const char* place, uint32_t t);

// Status card: battery, Wi-Fi and how old the weather is.
//   batteryMv <= 0: no battery measured
//   wifi: "ok 10:32", "off", "not set"...   weatherAgeMin < 0: never fetched
void drawStatus(Canvas& c, int batteryPct, int batteryMv, const char* wifi, int weatherAgeMin,
                const char* version);

// Long-press overlay at the bottom: 2-5 s "keep holding: sleep", 5 s+ "let go = sleep".
void drawHold(Canvas& c, uint32_t heldMs);

}  // namespace coco
