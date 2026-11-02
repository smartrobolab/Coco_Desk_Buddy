// PC renderer for Coco's faces and cards - writes raw 1024-byte frames to stdout.
//   render face <name> <fps> <duration_ms>
//   render card <boot|clock|weather-<sky>|weather-night|status|hold|noweather> <fps> <duration_ms>
//   render test        (Open-Meteo parser self-test)
// sim/make_screens.py turns the frames into PNG / GIF previews.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "coco_cards.h"
#include "kibo_faces.h"
#include "kibo_screens.h"

using namespace kibo;

static void emit(const Canvas& c) { fwrite(c.buf, 1, sizeof(c.buf), stdout); }

static int selfTest() {
  // trimmed real response from api.open-meteo.com
  const char* json =
      "{\"latitude\":35.7,\"longitude\":139.75,\"current_units\":{\"time\":\"iso8601\",\"temperature_2m\":\"°C\","
      "\"relative_humidity_2m\":\"%\",\"weather_code\":\"wmo code\",\"is_day\":\"\"},\"current\":{\"time\":"
      "\"2026-11-02T10:00\",\"interval\":900,\"temperature_2m\":-3.4,\"relative_humidity_2m\":62,\"weather_code\":61,"
      "\"is_day\":0},\"daily_units\":{\"time\":\"iso8601\",\"temperature_2m_max\":\"°C\"},\"daily\":{\"time\":"
      "[\"2026-11-02\"],\"temperature_2m_max\":[18.6],\"temperature_2m_min\":[9.2]}}";
  coco::Weather w;
  int fails = 0;
  auto check = [&](bool ok, const char* what) {
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
  };
  check(coco::parseOpenMeteo(json, w), "parses");
  check(w.tempC < -3.3f && w.tempC > -3.5f, "temperature -3.4");
  check(w.hiC > 18.5f && w.hiC < 18.7f, "max 18.6");
  check(w.loC > 9.1f && w.loC < 9.3f, "min 9.2");
  check(w.humidity == 62, "humidity 62");
  check(w.sky == coco::Sky::Rain, "code 61 = rain");
  check(!w.isDay, "night");
  check(!coco::parseOpenMeteo("{\"error\":true,\"reason\":\"bad\"}", w), "error body rejected");
  check(coco::skyFromWmo(0) == coco::Sky::Clear && coco::skyFromWmo(95) == coco::Sky::Thunder, "wmo mapping");
  return fails ? 1 : 0;
}

static coco::Sky skyFromName(const char* s) {
  static const char* const names[] = {"unknown", "clear", "partly", "cloudy", "fog", "drizzle", "rain", "snow", "thunder"};
  for (int i = 0; i < 9; i++)
    if (strcmp(s, names[i]) == 0) return (coco::Sky)i;
  return coco::Sky::Unknown;
}

int main(int argc, char** argv) {
  if (argc >= 2 && strcmp(argv[1], "test") == 0) return selfTest();
  if (argc < 5) {
    fprintf(stderr, "usage: render face|card <name> <fps> <duration_ms>\n");
    return 2;
  }
  const char* kind = argv[1];
  const char* name = argv[2];
  int fps = atoi(argv[3]);
  int dur = atoi(argv[4]);
  int frames = dur * fps / 1000;
  if (frames < 1) frames = 1;
  static Canvas c;
  for (int i = 0; i < frames; i++) {
    uint32_t t = (uint32_t)(i * 1000 / fps);
    c.clear();
    c.setColor(1);
    if (strcmp(kind, "face") == 0) {
      Face f;
      if (!faceFromName(name, f)) {
        fprintf(stderr, "unknown face %s\n", name);
        return 1;
      }
      FaceCtx ctx;
      ctx.t = t;
      drawFace(c, f, ctx);
    } else if (strcmp(name, "boot") == 0) {
      drawBoot(c, t, "v1.0");
    } else if (strcmp(name, "clock") == 0) {
      drawClock(c, 10, 42, "Mon 02 Nov", t);
    } else if (strncmp(name, "weather-", 8) == 0) {
      coco::Weather w;
      w.valid = true;
      w.tempC = 18.4f;
      w.hiC = 21.0f;
      w.loC = 12.2f;
      w.humidity = 58;
      bool night = strcmp(name + 8, "night") == 0;
      w.sky = night ? coco::Sky::Clear : skyFromName(name + 8);
      w.isDay = !night;
      if (w.sky == coco::Sky::Snow) w.tempC = -2.0f, w.hiC = 1, w.loC = -5;
      coco::drawWeather(c, w, "Tokyo", t);
    } else if (strcmp(name, "noweather") == 0) {
      coco::drawWeather(c, coco::Weather(), "Tokyo", t);
    } else if (strcmp(name, "status") == 0) {
      coco::drawStatus(c, 82, 4012, "ok 10:30", 12, "v1.0");
    } else if (strcmp(name, "hold") == 0) {
      FaceCtx ctx;
      ctx.t = 0;
      drawFace(c, Face::Love, ctx);
      coco::drawHold(c, 2000 + t);
    } else {
      fprintf(stderr, "unknown card %s\n", name);
      return 1;
    }
    emit(c);
  }
  return 0;
}
