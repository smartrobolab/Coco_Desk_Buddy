/*
  COCO TOUCH - a tiny brass desk buddy (ESP32-C3 Super Mini + 0.96" OLED + TTP223)

  Coco lives on the OLED as a pair of animated eyes (Kibo's face engine:
  kibo_faces.cpp). Touch the pad on its head:

    tap            next face (Neutral, Serious, Sleepy, Crying, Sad, Cute, Kissou,
                   Mini Love, Anime Love, Fire, Angry)
    double-tap     info cards: Clock -> Weather -> Status (tap = next card,
                   double-tap = back to the face)
    long press     Love (petting) while you hold
    hold 5 s       deep sleep, screen off (touch to wake)
    poke a lot     Angry (6+ quick taps), then Crying (10+)

  When you leave it alone it eats cookies, plays with a ball, dances, sings,
  yawns and shows the clock / weather. After NAP_AFTER_MIN it falls asleep,
  after DEEP_SLEEP_AFTER_MIN more it powers down until you touch it.

  Wi-Fi is on only for a few seconds every WEATHER_REFRESH_MIN minutes to sync
  the time (NTP) and fetch the weather (open-meteo.com, no API key).

  Settings: coco_config.h.   Wiring: docs/images/coco_touch_wiring.png
  Board package: "esp32" by Espressif 3.x, board "ESP32C3 Dev Module",
  USB CDC On Boot "Enabled". No extra libraries.
*/

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <time.h>

#include "coco_cards.h"
#include "coco_config.h"
#include "coco_oled.h"
#include "kibo_faces.h"
#include "kibo_screens.h"

using namespace kibo;

static const char* const kVersion = "v1.0";

// ------------------------------------------------------------------ timings
constexpr uint32_t kFrameMs = 40;          // 25 fps
constexpr uint32_t kBlinkMs = 120;         // blink between two faces
constexpr uint32_t kDoubleTapMs = 300;     // second tap within this -> double-tap
constexpr uint32_t kLongPressMs = 800;     // -> Love
constexpr uint32_t kHoldSleepMs = 5000;    // release after this -> deep sleep
constexpr uint32_t kTapFaceMs = 6000;      // how long a tapped face stays
constexpr uint32_t kCardIdleMs = 12000;    // cards go back to the face after this
constexpr uint32_t kNapMs = NAP_AFTER_MIN * 60000UL;
constexpr uint32_t kNightNapMs = 60000;
constexpr uint32_t kDeepSleepMs = DEEP_SLEEP_AFTER_MIN * 60000UL;

// ------------------------------------------------------------ shared state
// Kept in RTC memory so deep sleep does not forget the weather.
struct RtcSave {
  uint32_t magic;
  float temp, hi, lo;
  int16_t humidity;
  uint8_t sky, day;
  int64_t fetchedAt;  // epoch seconds, 0 = never
};
RTC_DATA_ATTR RtcSave rtcSave;
constexpr uint32_t kRtcMagic = 0xC0C0C0C0;

portMUX_TYPE netMux = portMUX_INITIALIZER_UNLOCKED;
coco::Weather weather;          // guarded by netMux
time_t weatherAt = 0;           // guarded by netMux
char wifiText[16] = "off";      // guarded by netMux

// ----------------------------------------------------------------- display
CocoOled oled;
Canvas canvas;
uint8_t lastSent[sizeof(Canvas::buf)];
bool haveSent = false;

// -------------------------------------------------------------- behaviour
enum class Activity : uint8_t { None, Eating, Playing, Music, Sing, Yawn, Clock, Weather, LowBattery };
struct ActivityInfo {
  uint32_t ms;
  uint8_t weight;
};
const ActivityInfo kActivities[] = {
    {0, 0},     // None
    {7000, 3},  // Eating
    {6000, 3},  // Playing
    {6000, 2},  // Music
    {5000, 1},  // Sing
    {3000, 1},  // Yawn
    {6000, 3},  // Clock
    {6000, 3},  // Weather
    {4000, 0},  // LowBattery (weight set when the battery is low)
};
constexpr int kActivityCount = sizeof(kActivities) / sizeof(kActivities[0]);

enum class Card : uint8_t { Clock, Weather, Status, Count };

uint32_t bootAt = 0;
bool booting = true;
bool showingCards = false;
Card card = Card::Clock;
uint32_t cardSince = 0, cardUntil = 0;

Face reactionFace = Face::Neutral;
uint32_t reactionUntil = 0;
bool holdingLove = false;

Activity activity = Activity::None;
uint32_t activitySince = 0, activityUntil = 0, nextActivityAt = 0;

uint32_t lastInteraction = 0;
int tapIndex = 0;
int pokes = 0;
uint32_t lastPokeDecay = 0;
int lastClockHour = -1;

Face lastFace = Face::Count;
uint32_t faceSince = 0;
uint32_t rng = 0x434f434f;  // "COCO"
int wanderX = 0, wanderY = 0;
float gazeX = 0, gazeY = 0;
uint32_t nextWanderAt = 0;
bool napContrast = false;

// touch
bool wasDown = false;
uint32_t pressAt = 0;
bool longFired = false;
bool swallowPress = false;  // the press that woke Coco from a nap is not a tap
bool tapPending = false;
uint32_t lastTapAt = 0;

// battery
int batteryMv = 0;
int batteryPct = 100;
bool batteryPresent = false;
uint32_t nextBatteryAt = 0;
int criticalCount = 0;

// ------------------------------------------------------------------ helpers

bool timeValid() { return time(nullptr) > 1700000000; }

int localHour() {
  if (!timeValid()) return -1;
  time_t t = time(nullptr);
  struct tm tm;
  localtime_r(&t, &tm);
  return tm.tm_hour;
}

bool isNight() {
  int h = localHour();
  if (h < 0) return false;
  return NIGHT_FROM_HOUR > NIGHT_TO_HOUR ? (h >= NIGHT_FROM_HOUR || h < NIGHT_TO_HOUR)
                                         : (h >= NIGHT_FROM_HOUR && h < NIGHT_TO_HOUR);
}

uint32_t napAfter() { return isNight() ? kNightNapMs : kNapMs; }
bool napping(uint32_t now) { return now - lastInteraction > napAfter(); }

void react(Face f, uint32_t ms) {
  uint32_t now = millis();
  reactionFace = f;
  reactionUntil = now + ms;
  activity = Activity::None;
}

void startActivity(Activity a, uint32_t now) {
  activity = a;
  activitySince = now;
  activityUntil = now + kActivities[(int)a].ms;
  nextActivityAt = activityUntil + 25000 + hash32(rng) % 35000;  // 25-60 s later
}

coco::Weather currentWeather(time_t* at = nullptr) {
  portENTER_CRITICAL(&netMux);
  coco::Weather w = weather;
  if (at) *at = weatherAt;
  portEXIT_CRITICAL(&netMux);
  return w;
}

// -------------------------------------------------------------- deep sleep

void goToSleep() {
  // goodnight face, then screen off
  uint32_t t0 = millis();
  while (millis() - t0 < 1500) {
    canvas.clear();
    canvas.setColor(1);
    FaceCtx ctx;
    ctx.t = millis() - t0;
    drawFace(canvas, Face::Sleeping, ctx);
    oled.show(canvas.buf);
    delay(kFrameMs);
  }
  oled.power(false);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  digitalWrite(PIN_LED, HIGH);
  // waking needs the pad released first, otherwise Coco wakes up at once
  while (digitalRead(PIN_TOUCH) == HIGH) delay(10);
  delay(200);
  esp_deep_sleep_enable_gpio_wakeup(1ULL << PIN_TOUCH, ESP_GPIO_WAKEUP_GPIO_HIGH);
  esp_deep_sleep_start();
}

// ------------------------------------------------------------------ battery

int percentFromMv(int mv) {
  // resting Li-ion curve
  static const int kMv[] = {3300, 3500, 3600, 3700, 3750, 3800, 3870, 3950, 4050, 4150};
  static const int kPct[] = {0, 5, 10, 20, 30, 40, 55, 70, 85, 100};
  if (mv <= kMv[0]) return 0;
  for (int i = 1; i < 10; i++) {
    if (mv < kMv[i]) return kPct[i - 1] + (mv - kMv[i - 1]) * (kPct[i] - kPct[i - 1]) / (kMv[i] - kMv[i - 1]);
  }
  return 100;
}

void updateBattery(uint32_t now) {
  if (now < nextBatteryAt) return;
  nextBatteryAt = now + 10000;
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += analogReadMilliVolts(PIN_BATTERY);
  int mv = (int)(sum / 8 * BATTERY_DIVIDER);
  batteryPresent = mv > 2500;  // below that: no divider fitted / USB only
  if (!batteryPresent) return;
  batteryMv = batteryMv ? (batteryMv * 3 + mv) / 4 : mv;  // smooth
  batteryPct = percentFromMv(batteryMv);
  // protect the cell: two readings under 3.3 V in a row -> sleep
  criticalCount = batteryMv < 3300 ? criticalCount + 1 : 0;
  if (criticalCount >= 2) goToSleep();
}

// --------------------------------------------------------------------- Wi-Fi

void setWifiText(const char* s) {
  portENTER_CRITICAL(&netMux);
  strncpy(wifiText, s, sizeof(wifiText) - 1);
  wifiText[sizeof(wifiText) - 1] = 0;
  portEXIT_CRITICAL(&netMux);
}

bool fetchWeather() {
  char url[256];
  snprintf(url, sizeof(url),
           "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,relative_humidity_2m,weather_code,is_day"
           "&daily=temperature_2m_max,temperature_2m_min&timezone=auto&forecast_days=1",
           (double)WEATHER_LAT, (double)WEATHER_LON);
  HTTPClient http;
  http.setTimeout(8000);
  if (!http.begin(url)) return false;
  int code = http.GET();
  bool ok = false;
  if (code == 200) {
    String body = http.getString();
    coco::Weather w;
    if (coco::parseOpenMeteo(body.c_str(), w)) {
      time_t now = time(nullptr);
      portENTER_CRITICAL(&netMux);
      weather = w;
      weatherAt = now;
      portEXIT_CRITICAL(&netMux);
      rtcSave = {kRtcMagic, w.tempC, w.hiC, w.loC, (int16_t)w.humidity, (uint8_t)w.sky, (uint8_t)w.isDay,
                 (int64_t)now};
      ok = true;
    }
  }
  Serial.printf("weather: HTTP %d %s\n", code, ok ? "ok" : "failed");
  http.end();
  return ok;
}

void syncOnce() {
  setWifiText("connecting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) delay(200);
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("wifi: could not connect");
    setWifiText("failed");
  } else {
    Serial.printf("wifi: connected, IP %s\n", WiFi.localIP().toString().c_str());
    configTzTime(TZ_INFO, "pool.ntp.org", "time.google.com");
    t0 = millis();
    while (!timeValid() && millis() - t0 < 8000) delay(100);
    fetchWeather();
    char s[16] = "ok";
    if (timeValid()) {
      time_t t = time(nullptr);
      struct tm tm;
      localtime_r(&t, &tm);
      strftime(s, sizeof(s), "ok %H:%M", &tm);
    }
    setWifiText(s);
  }
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);  // saves ~60 mA between refreshes
}

void netTask(void*) {
  for (;;) {
    time_t at;
    currentWeather(&at);
    uint32_t refresh = WEATHER_REFRESH_MIN * 60UL;
    uint32_t age = at && timeValid() ? (uint32_t)(time(nullptr) - at) : UINT32_MAX;
    if (!timeValid() || age >= refresh) {
      syncOnce();
      vTaskDelay(pdMS_TO_TICKS(refresh * 1000UL));
    } else {
      vTaskDelay(pdMS_TO_TICKS((refresh - age) * 1000UL + 1000));  // woke from deep sleep with fresh data
    }
  }
}

// ---------------------------------------------------------------- gestures

void noteInteraction(uint32_t now) {
  lastInteraction = now;
  if (napContrast) {
    oled.contrast(OLED_CONTRAST);
    napContrast = false;
  }
}

void onTap(uint32_t now) {
  if (showingCards) {
    card = (Card)(((int)card + 1) % (int)Card::Count);
    cardSince = now;
    cardUntil = now + kCardIdleMs;
    return;
  }
  pokes++;
  if (pokes >= 10) {
    react(Face::Crying, 4000);
  } else if (pokes >= 6) {
    react(Face::Angry, 3500);
  } else {
    tapIndex = (tapIndex + 1) % kTapFaceCount;
    react(kTapFaces[tapIndex], kTapFaceMs);
  }
}

void onDoubleTap(uint32_t now) {
  if (showingCards) {
    showingCards = false;
    react(Face::Happy, 1500);
  } else {
    showingCards = true;
    card = Card::Clock;
    cardSince = now;
    cardUntil = now + kCardIdleMs;
  }
}

void onLongPress() {
  showingCards = false;
  holdingLove = true;
  react(Face::Love, 60000);
}

void onLongRelease() {
  holdingLove = false;
  reactionUntil = millis() + 1500;  // keep smiling a moment
}

void updateTouch(uint32_t now) {
  bool down = digitalRead(PIN_TOUCH) == HIGH;
  digitalWrite(PIN_LED, down ? LOW : HIGH);

  if (down && !wasDown) {
    pressAt = now;
    longFired = false;
    swallowPress = napping(now);
    if (swallowPress) react(Face::Yawn, 2800);  // just woke up
    noteInteraction(now);
  }
  if (down) {
    uint32_t held = now - pressAt;
    if (!swallowPress && !longFired && held >= kLongPressMs) {
      longFired = true;
      onLongPress();
    }
    lastInteraction = now;
  }
  if (!down && wasDown) {
    uint32_t held = now - pressAt;
    if (held >= kHoldSleepMs) {
      goToSleep();
    } else if (swallowPress) {
      // nothing: that touch only woke Coco up
    } else if (longFired) {
      onLongRelease();
    } else if (tapPending && now - lastTapAt < kDoubleTapMs) {
      tapPending = false;
      onDoubleTap(now);
    } else {
      tapPending = true;
      lastTapAt = now;
    }
  }
  if (tapPending && !down && now - lastTapAt >= kDoubleTapMs) {
    tapPending = false;
    onTap(now);
  }
  wasDown = down;

  // annoyance fades: one poke every 3 s
  if (now - lastPokeDecay > 3000) {
    lastPokeDecay = now;
    if (pokes > 0) pokes--;
  }
}

// ---------------------------------------------------------------- drawing

void drawClockCard(uint32_t t) {
  if (!timeValid()) {
    drawNotice(canvas, "Clock", "No time yet. Set Wi-Fi in coco_config.h", false);
    return;
  }
  time_t now = time(nullptr);
  struct tm tm;
  localtime_r(&now, &tm);
  char date[24];
  strftime(date, sizeof(date), "%a %d %b", &tm);
  drawClock(canvas, tm.tm_hour, tm.tm_min, date, t);
}

void drawWeatherCard(uint32_t t) { coco::drawWeather(canvas, currentWeather(), WEATHER_PLACE, t); }

void drawStatusCard() {
  time_t at;
  currentWeather(&at);
  int ageMin = at && timeValid() ? (int)((time(nullptr) - at) / 60) : -1;
  char wifi[16];
  portENTER_CRITICAL(&netMux);
  memcpy(wifi, wifiText, sizeof(wifi));
  portEXIT_CRITICAL(&netMux);
  if (strlen(WIFI_SSID) == 0) strcpy(wifi, "not set");
  coco::drawStatus(canvas, batteryPct, batteryPresent ? batteryMv : 0, wifi, ageMin, kVersion);
}

void pickNextActivity(uint32_t now) {
  bool lowBattery = batteryPresent && batteryPct <= 15;
  int weights[kActivityCount] = {0};
  int total = 0;
  for (int i = 1; i < kActivityCount; i++) {
    Activity a = (Activity)i;
    int w = kActivities[i].weight;
    if (a == Activity::Clock && !timeValid()) w = 0;
    if (a == Activity::Weather && !currentWeather().valid) w = 0;
    if (a == Activity::LowBattery && lowBattery) w = 6;
    weights[i] = w;
    total += w;
  }
  rng = hash32(rng + now);
  int pick = total ? (int)(rng % (uint32_t)total) : 0;
  for (int i = 1; i < kActivityCount && total; i++) {
    if (pick < weights[i]) {
      startActivity((Activity)i, now);
      return;
    }
    pick -= weights[i];
  }
}

void render(uint32_t now) {
  canvas.clear();
  canvas.setColor(1);

  if (booting) {
    if (drawBoot(canvas, now - bootAt, kVersion)) booting = false;
    return;
  }

  if (showingCards) {
    if (now > cardUntil) {
      showingCards = false;
    } else {
      uint32_t t = now - cardSince;
      if (card == Card::Clock) drawClockCard(t);
      else if (card == Card::Weather) drawWeatherCard(t);
      else drawStatusCard();
      return;
    }
  }

  // hourly clock
#if HOURLY_CLOCK
  if (timeValid() && !napping(now)) {
    time_t tt = time(nullptr);
    struct tm tm;
    localtime_r(&tt, &tm);
    if (tm.tm_min == 0 && tm.tm_hour != lastClockHour && now >= reactionUntil) {
      lastClockHour = tm.tm_hour;
      startActivity(Activity::Clock, now);
    }
  }
#endif

  Face face = Face::Neutral;
  bool asleep = napping(now);
  if (now < reactionUntil) {
    face = reactionFace;
  } else if (activity != Activity::None && now < activityUntil) {
    uint32_t t = now - activitySince;
    switch (activity) {
      case Activity::Eating: face = Face::Eating; break;
      case Activity::Playing: face = Face::Playing; break;
      case Activity::Music: face = Face::Music; break;
      case Activity::Sing: face = Face::Singing; break;
      case Activity::Yawn: face = Face::Yawn; break;
      case Activity::LowBattery: face = Face::LowBattery; break;
      case Activity::Clock: drawClockCard(t); return;
      case Activity::Weather: drawWeatherCard(t); return;
      default: break;
    }
  } else {
    activity = Activity::None;
    if (asleep) face = Face::Sleeping;
    else if (now >= nextActivityAt) pickNextActivity(now);
  }

  // dim while asleep
  if (asleep != napContrast) {
    oled.contrast(asleep ? OLED_CONTRAST_NAP : OLED_CONTRAST);
    napContrast = asleep;
  }

  // idle gaze wandering
  if (now >= nextWanderAt) {
    rng = hash32(rng ^ now);
    bool centre = (rng & 3) == 0;
    wanderX = centre ? 0 : (int)(rng % 13) - 6;
    wanderY = centre ? 0 : (int)((rng >> 8) % 7) - 3;
    nextWanderAt = now + 1500 + (rng >> 16) % 2500;
  }
  bool wander = face == Face::Neutral || face == Face::Serious || face == Face::Cute;
  gazeX += ((wander ? wanderX : 0) - gazeX) * 0.3f;
  gazeY += ((wander ? wanderY : 0) - gazeY) * 0.3f;

  if (face != lastFace) {
    lastFace = face;
    faceSince = now;
  }
  FaceCtx ctx;
  ctx.t = now - faceSince;
  ctx.lookX = (int)gazeX;
  ctx.lookY = (int)gazeY;
  if (ctx.t < kBlinkMs) drawBlink(canvas, ctx);
  else drawFace(canvas, face, ctx);

  // overlays
  uint32_t held = wasDown ? now - pressAt : 0;
  if (held >= 2000) coco::drawHold(canvas, held);
  if (batteryPresent && batteryPct <= 20) {
    canvas.setColor(0);
    canvas.rect(112, 0, 16, 9);
    canvas.setColor(1);
    drawBatteryIcon(canvas, 114, 1, batteryPct, false);
  }
}

void flush() {
  if (haveSent && memcmp(lastSent, canvas.buf, sizeof(lastSent)) == 0) return;  // unchanged
  oled.show(canvas.buf);
  memcpy(lastSent, canvas.buf, sizeof(lastSent));
  haveSent = true;
}

// --------------------------------------------------------------- main

void setup() {
  setCpuFrequencyMhz(80);  // plenty for 25 fps faces, ~30% less current than 160 MHz
  Serial.begin(115200);
  pinMode(PIN_TOUCH, INPUT);  // TTP223 drives the line itself
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);
  analogSetPinAttenuation(PIN_BATTERY, ADC_11db);
  setenv("TZ", TZ_INFO, 1);  // local time also right after a deep sleep
  tzset();

  bool fromSleep = esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED;
  if (rtcSave.magic == kRtcMagic) {
    weather.valid = true;
    weather.tempC = rtcSave.temp;
    weather.hiC = rtcSave.hi;
    weather.loC = rtcSave.lo;
    weather.humidity = rtcSave.humidity;
    weather.sky = (coco::Sky)rtcSave.sky;
    weather.isDay = rtcSave.day;
    weatherAt = (time_t)rtcSave.fetchedAt;
  }

  if (!oled.begin(PIN_SDA, PIN_SCL, OLED_ADDR, OLED_ROTATE_180, OLED_CONTRAST))
    Serial.println("OLED not found - check SDA/SCL and address 0x3C");

  uint32_t now = millis();
  bootAt = now;
  lastInteraction = now;
  nextActivityAt = now + 30000;
  booting = !fromSleep;
  if (fromSleep) react(Face::Yawn, 2800);
  updateBattery(now);

  if (strlen(WIFI_SSID) > 0) xTaskCreate(netTask, "coco_net", 8192, nullptr, 1, nullptr);
  Serial.printf("Coco Touch %s ready (%s)\n", kVersion, fromSleep ? "woke from sleep" : "power on");
}

void loop() {
  static uint32_t last = millis();
  uint32_t now = millis();
  if (!booting) updateTouch(now);
  updateBattery(now);
  if (kDeepSleepMs && napping(now) && now - lastInteraction > napAfter() + kDeepSleepMs) goToSleep();
  render(now);
  flush();
  uint32_t spent = millis() - last;
  delay(spent < kFrameMs ? kFrameMs - spent : 1);
  last = millis();
}
