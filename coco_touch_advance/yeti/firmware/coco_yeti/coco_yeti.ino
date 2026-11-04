/*
  COCO YETI - Coco Touch Advance in a cute yeti case, with a motion sensor
  (MPU6050 / GY-521) and a vibration motor.

  Everything from Coco Touch Advance (50 faces, jokes, games, feeding, menu,
  settings), plus Coco now feels how it is moved:

    dropped            AAAH! (scared, buzzes) ... OUCH when it lands, phew when caught
    shaken             dizzy while you shake, angry if you keep going
    knock / slap       one sharp hit on the case = a slap (OUCH -> angry -> furious -> crying)
    spun around        dizzy spiral eyes
    tilted             the eyes slide to the low side
    upside down        scared, "put me back!"     on its side: confused
    on its back        sleepy, naps                face down 5 s: goodnight (deep sleep)
    picked up          wakes up from a nap; with INT wired it even wakes from deep sleep

  The vibration motor buzzes with taps, purrs while you pet it, giggles,
  thumps on slaps and landings and rumbles while it is shaken.
  Settings adds Vibration (Off / Soft / Strong), Motion (On / Off),
  Motion setup (teaches Coco how the sensor is mounted) and Motion test.

  Board: ESP32-C3 Super Mini ("ESP32C3 Dev Module", USB CDC On Boot Enabled,
  Partition Scheme "Huge APP (3MB No OTA/1MB SPIFFS)"). Pins: coco_config.h
  (Coco wiring, or YETI_PINOUT_LEROYD 1 for the Desk Companion wiring).
  Screen: OLED_SH1106 1 for the 1.3", 0 for the 0.96". No extra libraries.
*/

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_sleep.h>
#include <time.h>

#include "coco_cards.h"
#include "coco_config.h"
#include "coco_faces_x.h"
#include "coco_games.h"
#include "coco_jokes.h"
#include "coco_oled.h"
#include "coco_portal.h"
#include "coco_prefs.h"
#include "coco_settings.h"
#include "coco_ui.h"
#include "coco_imu.h"
#include "coco_motion.h"
#include "coco_vibe.h"
#include "kibo_screens.h"

using coco::Ex;
using coco::Game;
using kibo::Canvas;
using kibo::FaceCtx;
using kibo::hash32;

static const char* const kVersion = "v2.1-yeti";

// ------------------------------------------------------------------ timings
constexpr uint32_t kFrameMs = 40;          // 25 fps
constexpr uint32_t kBlinkMs = 120;         // blink between two faces
constexpr uint32_t kTapGapMs = 300;        // taps closer than this belong together (double-tap, slap)
constexpr uint32_t kLongPressMs = 800;     // petting / open a menu item
constexpr uint32_t kGameQuitMs = 1500;     // hold this long in a game to quit
constexpr uint32_t kHoldSleepMs = 5000;    // release after this -> deep sleep
constexpr uint32_t kHoldSetupMs = 10000;   // release after this -> Wi-Fi setup
constexpr uint32_t kMenuIdleMs = 15000;    // menus, cards, gallery: untouched this long -> face
constexpr uint32_t kNightNapMs = 60000;
constexpr uint32_t kDeepSleepMs = DEEP_SLEEP_AFTER_MIN * 60000UL;

// ------------------------------------------------------------ saved state
// Kept in RTC memory so deep sleep does not forget the weather or Coco's mood.
struct RtcSave {
  uint32_t magic;
  float temp, hi, lo;
  int16_t humidity;
  uint8_t sky, day;
  int64_t fetchedAt;  // epoch seconds, 0 = never
};
RTC_DATA_ATTR RtcSave rtcSave;
constexpr uint32_t kRtcMagic = 0xC0C0C0C0;

struct Pet {
  uint32_t magic;
  int16_t hunger, fun, energy;  // 0..100
  uint16_t slaps, pets;
};
RTC_DATA_ATTR Pet pet;
constexpr uint32_t kPetMagic = 0xC0C0BEEF;

RTC_NOINIT_ATTR uint32_t setupRequest;  // survives ESP.restart(): start in Wi-Fi setup
constexpr uint32_t kSetupMagic = 0x5E7A9C0C;

portMUX_TYPE netMux = portMUX_INITIALIZER_UNLOCKED;
coco::Weather weather;             // guarded by netMux
time_t weatherAt = 0;              // guarded by netMux
char wifiText[16] = "off";         // guarded by netMux
volatile bool wifiFailed = false;  // set by the net task, shown once as a notice
volatile bool forceSync = false;   // Settings > Update weather
TaskHandle_t netHandle = nullptr;

// ----------------------------------------------------------------- display
CocoOled oled;
Canvas canvas;
uint8_t lastSent[sizeof(Canvas::buf)];
bool haveSent = false;
coco::Arcade arcade;
CocoImu imu;
coco::MotionDetector motion;
CocoVibe vibe;

// -------------------------------------------------------------- modes
enum class Mode : uint8_t { Face, Menu, Card, Gallery, Game, MotionSetup };
enum class Card : uint8_t { Clock, Weather, Status, Stats, About, Motion, Count };
enum class MenuId : uint8_t { Main, Games, Feed, Settings, Reset };

Mode mode = Mode::Face;
uint32_t modeUntil = 0;  // menus, cards, gallery: back to the face after this
Card card = Card::Clock;
uint32_t cardSince = 0;

MenuId menuId = MenuId::Main;
int menuSel = 0;
bool menuTookPress = false;  // this press opened a menu item: its release is not petting

int galleryIdx = 0;
uint32_t gallerySince = 0;

bool gameFromMenu = false;
uint32_t autoGameUntil = 0;
bool gameScored = false;
int gameBestBefore = 0;  // best score when this game started (the game-over panel compares to it)

char toast[24] = "";
uint32_t toastUntil = 0;

// ------------------------------------------------------------- behaviour
uint32_t bootAt = 0;
bool booting = true;
bool setupMode = false;
uint32_t setupSince = 0;
uint32_t noticeUntil = 0;

Ex reactionEx = Ex::Neutral;
uint32_t reactionUntil = 0;
Ex followEx = Ex::Neutral;  // shown after the current reaction (e.g. Full after a big meal)
uint32_t followMs = 0;
bool holdingLove = false;

// motion
coco::MotionEvent lastMotionEvent = coco::MotionEvent::None;
uint32_t lastMotionAt = 0;
uint32_t shakeSince = 0;
bool shakeAngry = false;
float motionRate = 0;
int setupStep = 0;           // Motion setup: 0 upright, 1 on its back
uint32_t setupMeasureAt = 0; // measuring the still position
float setupSum[3] = {0}, setupUp[3] = {0};
int setupCount = 0;
bool pinned = false;  // a face picked in the gallery stays until Coco is tapped
Ex pinnedEx = Ex::Neutral;

int slapLevel = 0;
uint32_t lastSlapAt = 0, sulkUntil = 0;
int pokes = 0;
uint32_t lastPokeDecay = 0;

// Idle activities. kind: what is shown; cat: which mood likes it.
enum class Kind : uint8_t { Face, Clock, Weather, Game, Song, Joke };
enum Cat : uint8_t { kFood, kPlay, kFun, kCalm, kSleepy, kGrumpy, kInfo, kCatCount };
struct Activity {
  Kind kind;
  Ex ex;
  uint8_t cat, weight;
  uint16_t ms;
};
const Activity kActivities[] = {
    {Kind::Face, Ex::EatCookie, kFood, 2, 6500},   {Kind::Face, Ex::EatPizza, kFood, 2, 6500},
    {Kind::Face, Ex::EatNoodles, kFood, 2, 6000},  {Kind::Face, Ex::EatBurger, kFood, 2, 6500},
    {Kind::Face, Ex::EatIceCream, kFood, 2, 6500}, {Kind::Face, Ex::EatApple, kFood, 2, 6500},
    {Kind::Face, Ex::DrinkJuice, kFood, 2, 4200},  {Kind::Face, Ex::Playing, kPlay, 3, 6000},
    {Kind::Face, Ex::Music, kPlay, 2, 6000},       {Kind::Face, Ex::Gaming, kPlay, 2, 5200},
    {Kind::Game, Ex::Gaming, kPlay, 3, 20000},     {Kind::Face, Ex::Workout, kPlay, 1, 4200},
    {Kind::Face, Ex::Party, kFun, 1, 4000},        {Kind::Song, Ex::Singing, kFun, 3, 9000},
    {Kind::Face, Ex::Laughing, kFun, 1, 3600},     {Kind::Face, Ex::Wink, kFun, 1, 3000},
    {Kind::Face, Ex::StarEyes, kFun, 1, 3200},     {Kind::Face, Ex::Cool, kFun, 1, 4000},
    {Kind::Face, Ex::Cute, kFun, 1, 3600},         {Kind::Face, Ex::Love, kFun, 1, 3600},
    {Kind::Face, Ex::AnimeLove, kFun, 1, 3000},    {Kind::Face, Ex::Kissou, kFun, 1, 2500},
    {Kind::Face, Ex::MiniLove, kFun, 1, 2500},     {Kind::Face, Ex::Reading, kCalm, 2, 8000},
    {Kind::Face, Ex::Thinking, kCalm, 1, 4000},    {Kind::Face, Ex::Bored, kCalm, 1, 4000},
    {Kind::Face, Ex::Sneeze, kCalm, 1, 3200},      {Kind::Face, Ex::Confused, kCalm, 1, 4000},
    {Kind::Face, Ex::Surprised, kCalm, 1, 2000},   {Kind::Face, Ex::Yawn, kSleepy, 2, 2800},
    {Kind::Face, Ex::Sleepy, kSleepy, 2, 4000},    {Kind::Face, Ex::Pouting, kGrumpy, 2, 4000},
    {Kind::Face, Ex::Serious, kGrumpy, 2, 4000},   {Kind::Face, Ex::Angry, kGrumpy, 1, 2500},
    {Kind::Face, Ex::Fire, kGrumpy, 1, 2500},      {Kind::Face, Ex::Sad, kGrumpy, 1, 3000},
    {Kind::Joke, Ex::Laughing, kFun, 2, 0},        {Kind::Joke, Ex::Laughing, kCalm, 1, 0},
    {Kind::Clock, Ex::Neutral, kInfo, 3, 6000},    {Kind::Weather, Ex::Neutral, kInfo, 3, 6000},
};
constexpr int kActivityCount = sizeof(kActivities) / sizeof(kActivities[0]);

// How much each mood likes each kind of activity (food, play, fun, calm, sleepy, grumpy, info).
const uint8_t kMoodLikes[5][kCatCount] = {
    {2, 2, 3, 1, 1, 0, 2},  // Happy
    {2, 1, 1, 3, 1, 0, 2},  // Calm
    {2, 4, 2, 1, 0, 0, 1},  // Playful
    {1, 1, 1, 2, 4, 0, 1},  // Sleepy
    {2, 1, 0, 2, 1, 3, 1},  // Grumpy
};

// Songs Coco sings (traditional, public domain), shown as scrolling lyrics.
const char* const kSongs[] = {
    "Twinkle, twinkle, little star, how I wonder what you are!",
    "Row, row, row your boat, gently down the stream. Merrily, merrily, merrily, merrily, life is but a dream!",
    "Old MacDonald had a farm, E-I-E-I-O! And on that farm he had a cow, E-I-E-I-O!",
    "Mary had a little lamb, its fleece was white as snow. La la la!",
};
constexpr int kSongCount = sizeof(kSongs) / sizeof(kSongs[0]);

int activityIdx = -1;
uint32_t activitySince = 0, activityUntil = 0, nextActivityAt = 0;
int songIdx = 0;
int jokeIdx = -1;                 // the joke being told (also: don't tell it twice in a row)
uint32_t jokeSetupMs = 0, jokePunchMs = 0;
constexpr uint32_t kJokeLaughMs = 2500;

uint32_t lastInteraction = 0;
int lastClockHour = -1;
Ex lastFace = Ex::Count;
uint32_t faceSince = 0;
uint32_t rng = 0x434f434f;  // "COCO"
int wanderX = 0, wanderY = 0;
float gazeX = 0, gazeY = 0;
uint32_t nextWanderAt = 0;
bool napContrast = false;
uint32_t nextPetTick = 0;
int petTicks = 0;

// touch
bool wasDown = false;
uint32_t pressAt = 0;
bool longFired = false;
bool swallowPress = false;  // the press that woke Coco or stopped its game is not a tap
int tapCount = 0;
uint32_t lastTapAt = 0;

// battery
int batteryMv = 0;
int batteryPct = 100;
bool batteryPresent = false;
uint32_t nextBatteryAt = 0;
int criticalCount = 0;

// ------------------------------------------------------------------ helpers

uint32_t random32() {
  rng = hash32(rng + millis());
  return rng;
}

bool timeValid() { return time(nullptr) > 1700000000; }

bool localTm(struct tm& tm) {
  if (!timeValid()) return false;
  time_t t = time(nullptr);
  localtime_r(&t, &tm);
  return true;
}

int localHour() {
  struct tm tm;
  return localTm(tm) ? tm.tm_hour : -1;
}

bool isNight() {
  int h = localHour();
  if (h < 0) return false;
  return NIGHT_FROM_HOUR > NIGHT_TO_HOUR ? (h >= NIGHT_FROM_HOUR || h < NIGHT_TO_HOUR)
                                         : (h >= NIGHT_FROM_HOUR && h < NIGHT_TO_HOUR);
}

// Never = no naps at all
bool napEnabled() { return kNapMinutes[prefs.nap] != 0; }
uint32_t napAfter() {
  uint32_t ms = kNapMinutes[prefs.nap] * 60000UL;
  return isNight() && ms > kNightNapMs ? kNightNapMs : ms;
}
bool napping(uint32_t now) { return napEnabled() && now - lastInteraction > napAfter(); }

uint8_t contrastLevel() { return kBrightLevels[prefs.brightness]; }

Mood currentMood() {
  if ((Mood)prefs.mood != Mood::Random) return (Mood)prefs.mood;
  return (Mood)(hash32(millis() / 600000UL + 7) % 5);  // a new mood every 10 minutes
}

void showToast(const char* s, uint32_t ms = 1500) {
  strlcpy(toast, s, sizeof(toast));
  toastUntil = millis() + ms;
}

int clamp100(int v) { return v < 0 ? 0 : v > 100 ? 100 : v; }

void react(Ex e, uint32_t ms) {
  reactionEx = e;
  reactionUntil = millis() + ms;
  activityIdx = -1;
  followMs = 0;  // a new reaction cancels a queued one (callers queue theirs after this)
}

coco::Weather currentWeather(time_t* at = nullptr) {
  portENTER_CRITICAL(&netMux);
  coco::Weather w = weather;
  if (at) *at = weatherAt;
  portEXIT_CRITICAL(&netMux);
  return w;
}

const char* feeling() {
  if (slapLevel > 0 || millis() < sulkUntil) return "upset";
  if (pet.hunger > 70) return "hungry";
  if (pet.energy < 25) return "sleepy";
  if (pet.fun < 25) return "bored";
  static char m[12];
  strlcpy(m, kMoodNames[(int)currentMood()], sizeof(m));
  m[0] = tolower(m[0]);
  return m;
}

// --------------------------------------------------------------------- LED
void setLed(bool on) {
  if (PIN_LED < 0) return;
  static int8_t last = -1;
  if (last == (int8_t)on) return;
  last = on;
#if LED_IS_RGB
  rgbLedWrite(PIN_LED, 0, on ? LED_RGB_LEVEL : 0, 0);
#else
  digitalWrite(PIN_LED, on ? LOW : HIGH);
#endif
}

// -------------------------------------------------------------- deep sleep

void goToSleep() {
  uint32_t t0 = millis();
  while (millis() - t0 < 1500) {
    canvas.clear();
    FaceCtx ctx;
    ctx.t = millis() - t0;
    coco::drawEx(canvas, Ex::Sleeping, ctx);
    oled.show(canvas.buf);
    delay(kFrameMs);
  }
  oled.power(false);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  setLed(false);
  vibe.stop();
  while (digitalRead(PIN_TOUCH) == HIGH) delay(10);  // wake needs the pad released first
  delay(200);
  // The motion sensor's INT wakes Coco when it is picked up. Give it a moment
  // to settle (it was just put down) and make sure INT is low before sleeping.
  bool motionWake = PIN_IMU_INT >= 0 && imu.ok() && prefs.motionOn;
  if (motionWake) {
    delay(800);
    imu.armWakeOnMotion();
    for (int i = 0; i < 40 && digitalRead(PIN_IMU_INT) == HIGH; i++) {
      delay(50);
      imu.clearInterrupt();
    }
    if (digitalRead(PIN_IMU_INT) == HIGH) motionWake = false;  // INT stuck high: touch only
  }
#if YETI_PINOUT_LEROYD
  // GPIO7 can't wake the C3 from deep sleep: light sleep, then start fresh.
  gpio_wakeup_enable((gpio_num_t)PIN_TOUCH, GPIO_INTR_HIGH_LEVEL);
  if (motionWake) gpio_wakeup_enable((gpio_num_t)PIN_IMU_INT, GPIO_INTR_HIGH_LEVEL);
  esp_sleep_enable_gpio_wakeup();
  esp_light_sleep_start();
  ESP.restart();
#else
  uint64_t mask = 1ULL << PIN_TOUCH;
  if (motionWake) mask |= 1ULL << PIN_IMU_INT;
  esp_deep_sleep_enable_gpio_wakeup(mask, ESP_GPIO_WAKEUP_GPIO_HIGH);
  esp_deep_sleep_start();
#endif
}

// ------------------------------------------------------------------ battery

int percentFromMv(int mv) {
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
  if (PIN_BATTERY < 0) return;  // Desk Companion wiring: no battery divider
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += analogReadMilliVolts(PIN_BATTERY);
  int mv = (int)(sum / 8 * BATTERY_DIVIDER);
  batteryPresent = mv > 2500;
  if (!batteryPresent) return;
  batteryMv = batteryMv ? (batteryMv * 3 + mv) / 4 : mv;
  batteryPct = percentFromMv(batteryMv);
  criticalCount = batteryMv < 3300 ? criticalCount + 1 : 0;
  if (criticalCount >= 2) goToSleep();
}

// --------------------------------------------------------------- pet stats
// Once a minute: hunger grows, energy drains while awake and refills while
// napping, fun fades when nobody plays with Coco.
void updatePet(uint32_t now) {
  if (now < nextPetTick) return;
  nextPetTick = now + 60000;
  petTicks++;
  if (petTicks % 3 == 0) pet.hunger = clamp100(pet.hunger + 1);
  if (napping(now)) pet.energy = clamp100(pet.energy + 3);
  else if (petTicks % 3 == 0) pet.energy = clamp100(pet.energy - 1);
  if (now - lastInteraction > 300000 && petTicks % 8 == 0) pet.fun = clamp100(pet.fun - 1);
}

// --------------------------------------------------------------------- Wi-Fi

void setWifiText(const char* s) {
  portENTER_CRITICAL(&netMux);
  strlcpy(wifiText, s, sizeof(wifiText));
  portEXIT_CRITICAL(&netMux);
}

void applyTz(int utcOffsetSec) {
  char tz[24];
  coco::posixTz(utcOffsetSec, tz, sizeof(tz));
  if (settings.tz == tz) return;
  settings.tz = tz;
  setenv("TZ", tz, 1);
  tzset();
  saveTz(tz);
  Serial.printf("time zone: %s\n", tz);
}

String urlEncode(const char* s) {
  String o;
  for (; *s; s++) {
    char c = *s;
    if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.') {
      o += c;
    } else {
      char b[4];
      snprintf(b, sizeof(b), "%%%02X", (uint8_t)c);
      o += b;
    }
  }
  return o;
}

// HTTPS GET for OpenWeatherMap (the key is in the URL, so never plain HTTP and never printed).
int owmGet(const String& url, String& body) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(8000);
  if (!http.begin(client, url)) return -1;
  int code = http.GET();
  if (code == 200) body = http.getString();
  http.end();
  return code;
}

void geocode() {
  char place[32];
  portENTER_CRITICAL(&settingsMux);
  memcpy(place, settings.place, sizeof(place));
  portEXIT_CRITICAL(&settingsMux);
  if (settings.owmKey.length()) {
    String body;
    int code = owmGet("https://api.openweathermap.org/geo/1.0/direct?limit=1&q=" + urlEncode(place) +
                          "&appid=" + settings.owmKey,
                      body);
    float lat, lon;
    char name[32];
    if (code == 200 && coco::parseOwmGeocode(body.c_str(), lat, lon, name, sizeof(name))) {
      saveLocation(lat, lon, name[0] ? String(name) : String(place));
      Serial.printf("city (OpenWeatherMap): %s -> %.4f, %.4f\n", name, lat, lon);
      return;
    }
    Serial.printf("city (OpenWeatherMap): HTTP %d%s, trying open-meteo\n", code, code == 401 ? " bad API key" : "");
  }
  String url = "https://geocoding-api.open-meteo.com/v1/search?count=1&format=json&name=" + urlEncode(place);
  WiFiClientSecure client;
  client.setInsecure();  // public data, no secrets sent
  HTTPClient http;
  http.setTimeout(8000);
  if (!http.begin(client, url)) return;
  int code = http.GET();
  float lat, lon;
  char name[32];
  if (code == 200 && coco::parseGeocode(http.getString().c_str(), lat, lon, name, sizeof(name))) {
    saveLocation(lat, lon, name[0] ? String(name) : String(place));
    Serial.printf("city: %s -> %.4f, %.4f\n", name, lat, lon);
  } else {
    Serial.printf("city: '%s' not found (HTTP %d)\n", place, code);
  }
  http.end();
}

void storeWeather(const coco::Weather& w) {
  time_t now = time(nullptr);
  portENTER_CRITICAL(&netMux);
  weather = w;
  weatherAt = now;
  portEXIT_CRITICAL(&netMux);
  rtcSave = {kRtcMagic, w.tempC, w.hiC, w.loC, (int16_t)w.humidity, (uint8_t)w.sky, (uint8_t)w.isDay,
             (int64_t)now};
  if (w.hasOffset) applyTz(w.utcOffsetSec);
}

bool fetchOpenWeatherMap(float lat, float lon) {
  char q[96];
  snprintf(q, sizeof(q), "lat=%.4f&lon=%.4f&units=metric&appid=", (double)lat, (double)lon);
  String body;
  int code = owmGet(String("https://api.openweathermap.org/data/2.5/weather?") + q + settings.owmKey, body);
  coco::Weather w;
  bool ok = code == 200 && coco::parseOpenWeatherMap(body.c_str(), w);
  if (ok) storeWeather(w);
  Serial.printf("weather (OpenWeatherMap): HTTP %d %s\n", code,
                ok ? "ok" : code == 401 ? "bad API key, using open-meteo" : "failed, using open-meteo");
  return ok;
}

bool fetchWeather() {
  float lat, lon;
  portENTER_CRITICAL(&settingsMux);
  lat = settings.lat;
  lon = settings.lon;
  portEXIT_CRITICAL(&settingsMux);
  if (settings.owmKey.length() && fetchOpenWeatherMap(lat, lon)) return true;
  char url[256];
  snprintf(url, sizeof(url),
           "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,relative_humidity_2m,weather_code,is_day"
           "&daily=temperature_2m_max,temperature_2m_min&timezone=auto&forecast_days=1",
           (double)lat, (double)lon);
  HTTPClient http;
  http.setTimeout(8000);
  if (!http.begin(url)) return false;
  int code = http.GET();
  bool ok = false;
  if (code == 200) {
    String body = http.getString();
    coco::Weather w;
    if (coco::parseOpenMeteo(body.c_str(), w)) {
      storeWeather(w);
      ok = true;
    }
  }
  Serial.printf("weather (open-meteo): HTTP %d %s\n", code, ok ? "ok" : "failed");
  http.end();
  return ok;
}

void syncOnce() {
  setWifiText("connecting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(settings.ssid.c_str(), settings.pass.c_str());
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) delay(200);
  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("wifi: could not join '%s'\n", settings.ssid.c_str());
    setWifiText("failed");
    wifiFailed = true;
  } else {
    Serial.printf("wifi: connected, IP %s\n", WiFi.localIP().toString().c_str());
    configTzTime(settings.tz.c_str(), "pool.ntp.org", "time.google.com");
    if (settings.needGeocode) geocode();
    t0 = millis();
    while (!timeValid() && millis() - t0 < 8000) delay(100);
    fetchWeather();
    char s[16] = "ok";
    struct tm tm;
    if (localTm(tm)) strftime(s, sizeof(s), "ok %H:%M", &tm);
    setWifiText(s);
  }
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);  // saves ~60 mA between refreshes
}

// Wakes every WEATHER_REFRESH_MIN minutes, or at once when notified (Update
// weather, Wi-Fi switched back on). With Wi-Fi off in Settings it only waits.
void netTask(void*) {
  for (;;) {
    uint32_t refresh = WEATHER_REFRESH_MIN * 60UL;
    if (!prefs.wifiOn) {
      setWifiText("off");
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
      continue;
    }
    time_t at;
    currentWeather(&at);
    uint32_t age = at && timeValid() ? (uint32_t)(time(nullptr) - at) : UINT32_MAX;
    uint32_t waitS;
    if (forceSync || !timeValid() || age >= refresh) {
      forceSync = false;
      syncOnce();
      waitS = refresh;
    } else {
      waitS = refresh - age + 1;  // woke from deep sleep with fresh data
    }
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(waitS * 1000UL));
  }
}

void wakeNet() {
  if (netHandle) xTaskNotifyGive(netHandle);
}

// ---------------------------------------------------------------- modes

void noteInteraction(uint32_t now) {
  lastInteraction = now;
  if (napContrast) {
    oled.contrast(contrastLevel());
    napContrast = false;
  }
}

void toFace() { mode = Mode::Face; }

void openMenu(MenuId id, int sel, uint32_t now) {
  mode = Mode::Menu;
  menuId = id;
  menuSel = sel;
  modeUntil = now + kMenuIdleMs;
}

void showCard(Card c, uint32_t now) {
  mode = Mode::Card;
  card = c;
  cardSince = now;
  modeUntil = now + kMenuIdleMs;
}

void startGame(Game g, bool autopilot, bool fromMenu, uint32_t now) {
  mode = Mode::Game;
  gameFromMenu = fromMenu;
  gameScored = false;
  autoGameUntil = autopilot ? now + (fromMenu ? 180000UL : 20000UL) : 0;
  arcade.start(g, now, random32(), autopilot);
  gameBestBefore = prefs.best[(int)g];
  if (!autopilot) pet.fun = clamp100(pet.fun + 3);
}

void exitGame(uint32_t now) {
  if (gameFromMenu) {
    openMenu(MenuId::Games, arcade.autopilot() ? 4 : (int)arcade.game(), now);
  } else {
    toFace();
    react(Ex::Happy, 1500);
  }
}

void startSetupMode() {
  setupRequest = kSetupMagic;  // restart into setup, so the hotspot never fights the weather task
  canvas.clear();
  kibo::drawNotice(canvas, "Wi-Fi setup", "Starting my hotspot...", false);
  oled.show(canvas.buf);
  delay(300);
  ESP.restart();
}

// ---------------------------------------------------------------- menus

enum MainItem { mFaces, mGames, mFeed, mJoke, mStats, mClock, mWeather, mStatus, mSettings, mSleep, mBack, mCount };
const char* const kMainItems[] = {"Faces",   "Games",           "Feed me",  "Tell a joke", "My Coco", "Clock & date",
                                  "Weather", "Battery & Wi-Fi", "Settings", "Sleep",       "Back"};
constexpr int kMainCount = mCount;
static_assert(sizeof(kMainItems) / sizeof(kMainItems[0]) == mCount, "one label per main menu item");
const Ex kFoods[] = {Ex::EatCookie, Ex::EatPizza, Ex::EatNoodles, Ex::EatBurger,
                     Ex::EatIceCream, Ex::EatApple, Ex::DrinkJuice};
const char* const kFoodNames[] = {"Cookie", "Pizza", "Noodles", "Burger", "Ice cream", "Apple", "Juice"};
constexpr int kFoodCount = sizeof(kFoods) / sizeof(kFoods[0]);
enum SettingsItem { sBright, sMood, sShuffle, sJokes, sVibe, sMotion, sMotionSetup, sMotionTest, sNap, sClock, sFlip, sWifi, sSetup, sUpdate, sScores, sReset, sAbout,
                    sBack, sCount };
const char* const kSettingsItems[] = {"Brightness", "Mood", "Random faces", "Jokes", "Vibration", "Motion",
                                      "Motion setup", "Motion test", "Nap after", "Clock", "Flip screen",
                                      "Wi-Fi", "Wi-Fi setup (QR)", "Update weather", "Reset scores",
                                      "Factory reset", "About", "Back"};

int menuCount(MenuId id) {
  switch (id) {
    case MenuId::Main: return kMainCount;
    case MenuId::Games: return (int)Game::Count + 2;
    case MenuId::Feed: return kFoodCount + 1;
    case MenuId::Settings: return sCount;
    case MenuId::Reset: return 2;
  }
  return 1;
}

// Fills rows for the current menu. Values live in a static buffer.
int buildMenu(coco::MenuRow* rows, const char** title) {
  static char vals[(int)Game::Count][12];
  int n = menuCount(menuId);
  for (int i = 0; i < n; i++) rows[i] = {"", nullptr};
  switch (menuId) {
    case MenuId::Main:
      *title = "Menu";
      for (int i = 0; i < n; i++) rows[i].label = kMainItems[i];
      break;
    case MenuId::Games:
      *title = "Games";
      for (int g = 0; g < (int)Game::Count; g++) {
        rows[g].label = coco::gameName((Game)g);
        if (prefs.best[g]) {
          snprintf(vals[g], sizeof(vals[g]), coco::gameLowerIsBetter((Game)g) ? "%dms" : "%d", (int)prefs.best[g]);
          rows[g].value = vals[g];
        }
      }
      rows[(int)Game::Count].label = "Watch Coco play";
      rows[(int)Game::Count + 1].label = "Back";
      break;
    case MenuId::Feed:
      *title = "Feed me";
      for (int i = 0; i < kFoodCount; i++) rows[i].label = kFoodNames[i];
      rows[kFoodCount].label = "Back";
      break;
    case MenuId::Settings:
      *title = "Settings";
      for (int i = 0; i < n; i++) rows[i].label = kSettingsItems[i];
      rows[sBright].value = kBrightNames[prefs.brightness];
      rows[sMood].value = kMoodNames[prefs.mood];
      rows[sShuffle].value = kShuffleNames[prefs.shuffle];
      rows[sJokes].value = kJokeNames[prefs.jokes];
      rows[sVibe].value = kVibeNames[prefs.vibe];
      rows[sMotion].value = !imu.ok() ? "no sensor" : prefs.motionOn ? "On" : "Off";
      rows[sNap].value = kNapNames[prefs.nap];
      rows[sClock].value = prefs.clock24 ? "24 h" : "12 h";
      rows[sFlip].value = prefs.flip ? "On" : "Off";
      rows[sWifi].value = prefs.wifiOn ? "On" : "Off";
      break;
    case MenuId::Reset:
      *title = "Erase all?";
      rows[0].label = "No, keep everything";
      rows[1].label = "Yes, erase + restart";
      break;
  }
  return n;
}

void feed(int food, uint32_t now) {
  toFace();
  pinned = false;
  bool wasFull = pet.hunger < 20;
  Ex e = kFoods[food];
  uint32_t ms = e == Ex::EatNoodles ? 6000 : coco::exLoopMs(e) + 300;
  react(e, ms);
  followMs = wasFull ? 3000 : 1500;
  followEx = wasFull ? Ex::Full : Ex::Happy;
  pet.hunger = clamp100(pet.hunger - 35);
  pet.fun = clamp100(pet.fun + 4);
  pet.energy = clamp100(pet.energy + 5);
  nextActivityAt = now + ms + 20000;
}

void startActivity(int i, uint32_t now);

// Menu > Tell a joke: one joke right now (in the chosen language; "Off" only stops
// the jokes Coco tells by itself).
void tellJoke(uint32_t now) {
  toFace();
  pinned = false;
  reactionUntil = 0;
  sulkUntil = 0;
  for (int i = 0; i < kActivityCount; i++)
    if (kActivities[i].kind == Kind::Joke) {
      startActivity(i, now);
      break;
    }
}

// Hold in a menu: open the item, or change the setting (the menu stays open).
void selectMenuItem(uint32_t now) {
  modeUntil = now + kMenuIdleMs;
  int sel = menuSel;
  switch (menuId) {
    case MenuId::Main:
      switch (sel) {
        case mFaces:
          mode = Mode::Gallery;
          gallerySince = now;
          modeUntil = now + kMenuIdleMs;
          break;
        case mGames: openMenu(MenuId::Games, 0, now); break;
        case mFeed: openMenu(MenuId::Feed, 0, now); break;
        case mJoke: tellJoke(now); break;
        case mStats: showCard(Card::Stats, now); break;
        case mClock: showCard(Card::Clock, now); break;
        case mWeather: showCard(Card::Weather, now); break;
        case mStatus: showCard(Card::Status, now); break;
        case mSettings: openMenu(MenuId::Settings, 0, now); break;
        case mSleep: goToSleep(); break;
        default:
          toFace();
          react(Ex::Happy, 1500);
          break;
      }
      break;
    case MenuId::Games:
      if (sel < (int)Game::Count) startGame((Game)sel, false, true, now);
      else if (sel == (int)Game::Count) startGame((Game)(random32() % 3), true, true, now);
      else openMenu(MenuId::Main, mGames, now);
      break;
    case MenuId::Feed:
      if (sel < kFoodCount) feed(sel, now);
      else openMenu(MenuId::Main, mFeed, now);
      break;
    case MenuId::Settings:
      switch (sel) {
        case sBright:
          prefs.brightness = (prefs.brightness + 1) % kBrightCount;
          oled.contrast(contrastLevel());
          break;
        case sMood: prefs.mood = (prefs.mood + 1) % (int)Mood::Count; break;
        case sShuffle:
          prefs.shuffle = (prefs.shuffle + 1) % kShuffleCount;
          nextActivityAt = now + 5000;
          break;
        case sJokes: prefs.jokes = (prefs.jokes + 1) % kJokeSettingCount; break;
        case sVibe:
          prefs.vibe = (prefs.vibe + 1) % kVibeCount;
          vibe.setLevel(prefs.vibe);
          vibe.play(Buzz::Hello);
          break;
        case sMotion: prefs.motionOn = !prefs.motionOn; break;
        case sMotionSetup:
          if (!imu.ok()) {
            showToast("no motion sensor");
            return;
          }
          mode = Mode::MotionSetup;
          setupStep = 0;
          setupMeasureAt = 0;
          modeUntil = now + 60000;
          return;
        case sMotionTest: showCard(Card::Motion, now); return;
        case sNap: prefs.nap = (prefs.nap + 1) % kNapCount; break;
        case sClock: prefs.clock24 = !prefs.clock24; break;
        case sFlip:
          prefs.flip = !prefs.flip;
          oled.flip(prefs.flip);
          break;
        case sWifi:
          prefs.wifiOn = !prefs.wifiOn;
          if (prefs.wifiOn) wakeNet();
          break;
        case sSetup: startSetupMode(); return;
        case sUpdate:
          if (!prefs.wifiOn) showToast("Wi-Fi is off");
          else if (settings.ssid.length() == 0) showToast("set up Wi-Fi first");
          else {
            forceSync = true;
            wakeNet();
            showToast("updating...");
          }
          return;
        case sScores:
          resetScores();
          showToast("scores cleared");
          return;
        case sReset: openMenu(MenuId::Reset, 0, now); return;
        case sAbout: showCard(Card::About, now); return;
        default: openMenu(MenuId::Main, mSettings, now); return;
      }
      savePrefs();
      showToast("saved", 800);
      break;
    case MenuId::Reset:
      if (sel == 1) {
        canvas.clear();
        kibo::drawNotice(canvas, "Bye bye!", "Erasing everything. I start fresh with the Wi-Fi setup.", false);
        oled.show(canvas.buf);
        factoryReset();
        delay(1500);
        ESP.restart();
      }
      openMenu(MenuId::Settings, sReset, now);
      break;
  }
}

void menuBack(uint32_t now) {
  switch (menuId) {
    case MenuId::Main:
      toFace();
      react(Ex::Happy, 1500);
      break;
    case MenuId::Games: openMenu(MenuId::Main, mGames, now); break;
    case MenuId::Feed: openMenu(MenuId::Main, mFeed, now); break;
    case MenuId::Settings: openMenu(MenuId::Main, mSettings, now); break;
    case MenuId::Reset: openMenu(MenuId::Settings, sReset, now); break;
  }
}

// ---------------------------------------------------------------- gestures

Ex tapFace() {
  static const Ex kHappy[] = {Ex::Happy, Ex::Laughing, Ex::Wink, Ex::Cute, Ex::StarEyes, Ex::Shy, Ex::Surprised};
  static const Ex kCalm[] = {Ex::Happy, Ex::Cute, Ex::Shy, Ex::Wink, Ex::Kissou};
  static const Ex kPlayful[] = {Ex::Laughing, Ex::StarEyes, Ex::Party, Ex::Wink, Ex::Cool};
  static const Ex kSleepy[] = {Ex::Yawn, Ex::Sleepy, Ex::Surprised, Ex::Happy};
  static const Ex kGrumpy[] = {Ex::Pouting, Ex::Serious, Ex::Bored, Ex::Angry};
  uint32_t r = random32();
  switch (currentMood()) {
    case Mood::Calm: return kCalm[r % 5];
    case Mood::Playful: return kPlayful[r % 5];
    case Mood::Sleepy: return kSleepy[r % 4];
    case Mood::Grumpy: return kGrumpy[r % 4];
    default: return kHappy[r % 7];
  }
}

void onTap(uint32_t now) {
  if (setupMode) return;
  switch (mode) {
    case Mode::MotionSetup:
      if (!setupMeasureAt) {  // start measuring this position
        setupMeasureAt = now + 400;  // let the hand go first
        setupCount = 0;
        setupSum[0] = setupSum[1] = setupSum[2] = 0;
      }
      modeUntil = now + 60000;
      return;
    case Mode::Menu:
      menuSel = (menuSel + 1) % menuCount(menuId);
      modeUntil = now + kMenuIdleMs;
      return;
    case Mode::Gallery:
      galleryIdx = (galleryIdx + 1) % coco::kExCount;
      gallerySince = now;
      modeUntil = now + kMenuIdleMs;
      return;
    case Mode::Card:
      card = (Card)(((int)card + 1) % (int)Card::About);  // About only from Settings
      cardSince = now;
      modeUntil = now + kMenuIdleMs;
      return;
    default: break;
  }
  if (pinned) {
    pinned = false;
    showToast("free again");
  }
  pet.fun = clamp100(pet.fun + 1);
  pokes++;
  vibe.play(Buzz::Tap);
  if (now < sulkUntil) react(Ex::Pouting, 2000);  // still upset: pet me instead
  else if (pokes >= 6) react(Ex::Pouting, 3500);  // stop poking me!
  else react(tapFace(), 3000);
}

void onDoubleTap(uint32_t now) {
  if (setupMode) {
    if (settings.ssid.length()) ESP.restart();  // leave setup, keep the saved Wi-Fi
    return;
  }
  switch (mode) {
    case Mode::MotionSetup: openMenu(MenuId::Settings, sMotionSetup, now); break;
    case Mode::Menu: menuBack(now); break;
    case Mode::Gallery: openMenu(MenuId::Main, 0, now); break;
    case Mode::Card:
      openMenu(MenuId::Main, card == Card::About || card == Card::Motion ? mSettings : 0, now);
      break;
    default:
      openMenu(MenuId::Main, 0, now);
      break;
  }
}

// Three or more quick taps on the face = a slap. Every extra slap makes it worse.
void onSlap(uint32_t now) {
  if (setupMode) return;
  pinned = false;
  if (now - lastSlapAt > 10000) slapLevel = 0;
  lastSlapAt = now;
  slapLevel += currentMood() == Mood::Grumpy ? 2 : 1;
  if (slapLevel > 5) slapLevel = 5;
  pet.slaps++;
  pet.fun = clamp100(pet.fun - 8);
  uint32_t ms;
  vibe.play(slapLevel >= 3 ? Buzz::Alarm : Buzz::Thump);
  if (slapLevel <= 1) react(Ex::Slapped, ms = 2200);
  else if (slapLevel == 2) react(Ex::Angry, ms = 3000);
  else if (slapLevel <= 4) react(Ex::Furious, ms = 3500);
  else react(Ex::Crying, ms = 4500);
  followMs = 0;
  sulkUntil = slapLevel >= 2 ? now + ms + 9000 : 0;  // then it sulks until petted
}

void onLongPress(uint32_t now) {
  if (setupMode) return;
  switch (mode) {
    case Mode::Menu:
      menuTookPress = true;
      selectMenuItem(now);
      return;
    case Mode::Gallery:
      menuTookPress = true;
      pinned = true;
      pinnedEx = (Ex)galleryIdx;
      toFace();
      activityIdx = -1;
      reactionUntil = 0;
      showToast("tap me to roam", 2000);
      return;
    case Mode::Card:
      menuTookPress = true;
      openMenu(MenuId::Main, 0, now);
      return;
    case Mode::Game:
      if (arcade.over()) {
        menuTookPress = true;
        exitGame(now);
      }
      return;
    default: break;
  }
  // petting
  pet.pets++;
  pet.fun = clamp100(pet.fun + 3);
  bool makeUp = now < sulkUntil || slapLevel > 0;
  sulkUntil = 0;
  slapLevel = 0;
  pokes = 0;
  holdingLove = true;
  vibe.play(Buzz::Purr, true);
  react(makeUp ? Ex::Shy : Ex::Love, makeUp ? 1200 : 60000);
  if (makeUp) {
    followEx = Ex::Love;
    followMs = 60000;
  }
}

void onLongRelease(uint32_t now) {
  if (menuTookPress) {
    menuTookPress = false;
    return;
  }
  if (holdingLove) {
    holdingLove = false;
    vibe.stop();
    followMs = 0;
    reactionEx = Ex::Love;
    reactionUntil = now + 1500;  // keep smiling a moment
  }
}

void updateTouch(uint32_t now) {
  bool down = digitalRead(PIN_TOUCH) == HIGH;
  setLed(down);

  if (down && !wasDown) {
    pressAt = now;
    longFired = false;
    swallowPress = mode == Mode::Face && !setupMode && napping(now);
    if (swallowPress) react(Ex::Yawn, 2800);  // just woke up
    noteInteraction(now);
    if (mode == Mode::Game) {
      if (arcade.autopilot()) {  // touching stops Coco's own game
        swallowPress = true;
        exitGame(now);
      } else if (!arcade.over()) {
        arcade.press(now);
      }
    }
  }
  if (down) {
    uint32_t held = now - pressAt;
    if (!swallowPress && !longFired && held >= kLongPressMs) {
      longFired = true;
      onLongPress(now);
    }
    if (mode == Mode::Game && !swallowPress && !arcade.over() && held >= kGameQuitMs) {
      swallowPress = true;
      exitGame(now);
    }
    lastInteraction = now;
  }
  if (!down && wasDown) {
    uint32_t held = now - pressAt;
    bool onFace = mode == Mode::Face && !menuTookPress;
    if (onFace && held >= kHoldSetupMs) {
      startSetupMode();
    } else if (onFace && !setupMode && held >= kHoldSleepMs) {
      goToSleep();
    } else if (swallowPress) {
      // that touch only woke Coco up / stopped its game
    } else if (longFired) {
      onLongRelease(now);
    } else if (mode == Mode::Game) {
      if (arcade.over() && now - arcade.overSince() > 600) startGame(arcade.game(), false, gameFromMenu, now);
    } else {
      tapCount++;
      lastTapAt = now;
      if (tapCount >= 3 && mode == Mode::Face) onSlap(now);
    }
  }
  if (tapCount && !down && now - lastTapAt >= kTapGapMs) {
    int n = tapCount;
    tapCount = 0;
    if (n == 1) onTap(now);
    else if (n == 2 || mode != Mode::Face) onDoubleTap(now);  // 3+ on the face was a slap
  }
  wasDown = down;

  if (now - lastPokeDecay > 3000) {  // annoyance fades
    lastPokeDecay = now;
    if (pokes > 0) pokes--;
  }
}

// ---------------------------------------------------------------- drawing

void drawClockCard(uint32_t t) {
  struct tm tm;
  if (!localTm(tm)) {
    kibo::drawNotice(canvas, "Clock", "No time yet. Hold my head 10 s to set up Wi-Fi", false);
    return;
  }
  char date[24];
  strftime(date, sizeof(date), "%a %d %b", &tm);
  int h = tm.tm_hour;
  if (!prefs.clock24) {
    strlcat(date, h < 12 ? " AM" : " PM", sizeof(date));
    h = h % 12 ? h % 12 : 12;
  }
  kibo::drawClock(canvas, h, tm.tm_min, date, t);
}

void drawWeatherCard(uint32_t t) {
  char place[32];
  portENTER_CRITICAL(&settingsMux);
  memcpy(place, settings.place, sizeof(place));
  portEXIT_CRITICAL(&settingsMux);
  coco::drawWeather(canvas, currentWeather(), place, t);
}

void drawStatusCard() {
  time_t at;
  currentWeather(&at);
  int ageMin = at && timeValid() ? (int)((time(nullptr) - at) / 60) : -1;
  char wifi[16];
  portENTER_CRITICAL(&netMux);
  memcpy(wifi, wifiText, sizeof(wifi));
  portEXIT_CRITICAL(&netMux);
  if (settings.ssid.length() == 0) strcpy(wifi, "not set");
  else if (!prefs.wifiOn) strcpy(wifi, "off");
  coco::drawStatus(canvas, batteryPct, batteryPresent ? batteryMv : 0, wifi, ageMin, kVersion);
}

void menuClock(char* out, size_t n) {
  out[0] = 0;
  struct tm tm;
  if (!localTm(tm)) return;
  if (prefs.clock24) strftime(out, n, "%H:%M", &tm);
  else strftime(out, n, "%I:%M%p", &tm);
}

// The face Coco wears when nothing else is going on.
Ex restFace() {
  if (imu.ok() && prefs.motionOn) {
    switch (motion.orientation()) {
      case coco::Orientation::UpsideDown: return Ex::Scared;
      case coco::Orientation::OnBack: return Ex::Sleeping;  // bedtime
      case coco::Orientation::OnLeft:
      case coco::Orientation::OnRight: return Ex::Confused;
      default: break;
    }
  }
  if (pinned) return pinnedEx;
  if (pet.hunger > 85) return Ex::Hungry;
  if (pet.fun < 15) return Ex::Sad;
  switch (currentMood()) {
    case Mood::Sleepy: return Ex::Sleepy;
    case Mood::Grumpy: return Ex::Serious;
    default: return Ex::Neutral;
  }
}

void startActivity(int i, uint32_t now) {
  const Activity& a = kActivities[i];
  uint32_t gap = kShuffleMinMs[prefs.shuffle] + random32() % (kShuffleSpanMs[prefs.shuffle] + 1);
  if (a.kind == Kind::Game) {
    startGame((Game)(random32() % 3), true, false, now);
    nextActivityAt = now + a.ms + gap;
    return;
  }
  activityIdx = i;
  activitySince = now;
  activityUntil = now + a.ms;
  if (a.kind == Kind::Song) songIdx = (int)(random32() % kSongCount);
  if (a.kind == Kind::Joke) {  // setup, punchline, then a laugh
    coco::JokeLang lang = prefs.jokes >= 1 && prefs.jokes <= 3 ? (coco::JokeLang)(prefs.jokes - 1) : coco::JokeLang::Count;
    jokeIdx = coco::pickJoke(lang, random32(), jokeIdx);
    jokeSetupMs = coco::jokeReadMs(coco::kJokes[jokeIdx].setup);
    jokePunchMs = coco::jokeReadMs(coco::kJokes[jokeIdx].punch);
    activityUntil = now + jokeSetupMs + jokePunchMs + kJokeLaughMs;
    pet.fun = clamp100(pet.fun + 2);
  }
  nextActivityAt = activityUntil + gap;
  if (a.cat == kFood) pet.hunger = clamp100(pet.hunger - 10);  // a snack
  if (a.cat == kPlay) pet.energy = clamp100(pet.energy - 2);
}

void pickNextActivity(uint32_t now) {
  if (prefs.shuffle == 0 || pinned) return;
  Mood m = currentMood();
  if (m == Mood::Random) m = Mood::Happy;
  bool sleepy = pet.energy < 25 || isNight();
  coco::Weather w = currentWeather();
  int weights[kActivityCount + 4];
  int total = 0;
  for (int i = 0; i < kActivityCount; i++) {
    const Activity& a = kActivities[i];
    int wt = a.weight * kMoodLikes[(int)m][a.cat];
    if (a.cat == kSleepy && sleepy) wt *= 3;
    if (a.cat == kFood && pet.hunger > 60) wt *= 2;
    if (a.kind == Kind::Clock && !timeValid()) wt = 0;
    if (a.kind == Kind::Weather && !w.valid) wt = 0;
    if (a.kind == Kind::Joke && prefs.jokes == kJokesOff) wt = 0;
    weights[i] = wt;
    total += wt;
  }
  // situational extras
  struct Extra {
    Ex ex;
    int weight;
    uint16_t ms;
  };
  Extra extras[4] = {
      {Ex::Hungry, pet.hunger > 60 ? 8 : 0, 4500},
      {Ex::Cold, w.valid && w.tempC < 10 ? 4 : 0, 4000},
      {Ex::Hot, w.valid && w.tempC > 30 ? 4 : 0, 4000},
      {Ex::LowBattery, batteryPresent && batteryPct <= 15 ? 10 : 0, 4000},
  };
  for (int k = 0; k < 4; k++) {
    weights[kActivityCount + k] = extras[k].weight;
    total += extras[k].weight;
  }
  if (!total) return;
  int pick = (int)(random32() % (uint32_t)total);
  for (int i = 0; i < kActivityCount + 4; i++) {
    if (pick < weights[i]) {
      if (i < kActivityCount) {
        startActivity(i, now);
      } else {
        const Extra& e = extras[i - kActivityCount];
        react(e.ex, e.ms);
        nextActivityAt = now + e.ms + kShuffleMinMs[prefs.shuffle];
      }
      return;
    }
    pick -= weights[i];
  }
}

void renderFace(uint32_t now) {
#if HOURLY_CLOCK
  if (!pinned && !napping(now) && now >= reactionUntil) {
    struct tm tm;
    if (localTm(tm) && tm.tm_min == 0 && tm.tm_hour != lastClockHour) {
      lastClockHour = tm.tm_hour;
      for (int i = 0; i < kActivityCount; i++)
        if (kActivities[i].kind == Kind::Clock) startActivity(i, now);
    }
  }
#endif

  Ex face = restFace();
  bool asleep = napping(now);
  const char* lyric = nullptr;
  uint32_t lyricT = 0;
  if (now >= reactionUntil && followMs) {  // queued reaction
    react(followEx, followMs);
    followMs = 0;
  }
  if (now < reactionUntil) {
    face = reactionEx;
  } else if (now < sulkUntil) {
    face = Ex::Pouting;
  } else if (activityIdx >= 0 && now < activityUntil) {
    const Activity& a = kActivities[activityIdx];
    uint32_t t = now - activitySince;
    switch (a.kind) {
      case Kind::Clock: drawClockCard(t); return;
      case Kind::Weather: drawWeatherCard(t); return;
      case Kind::Joke:
        if (t < jokeSetupMs + jokePunchMs) {
          const coco::Joke& j = coco::kJokes[jokeIdx];
          bool punch = t >= jokeSetupMs;
          coco::drawJoke(canvas, coco::jokeLangName(j.lang), punch ? j.punch : j.setup, punch,
                         punch ? t - jokeSetupMs : t);
          return;
        }
        face = Ex::Laughing;  // ha ha!
        break;
      case Kind::Song:
        face = Ex::Singing;
        lyric = kSongs[songIdx];
        lyricT = t;
        break;
      default: face = a.ex; break;
    }
  } else {
    activityIdx = -1;
    if (asleep) {
      uint32_t slept = now - lastInteraction - napAfter();
      face = (slept / 30000) % 3 == 2 ? Ex::Dreaming : Ex::Sleeping;
    } else if (now >= nextActivityAt) {
      pickNextActivity(now);
      if (mode == Mode::Game) return;  // Coco started a game by itself
    }
  }

  if (asleep != napContrast) {  // dim while asleep
    oled.contrast(asleep ? min<uint8_t>(OLED_CONTRAST_NAP, contrastLevel()) : contrastLevel());
    napContrast = asleep;
  }

  if (now >= nextWanderAt) {  // idle gaze wandering
    uint32_t r = random32();
    bool centre = (r & 3) == 0;
    wanderX = centre ? 0 : (int)(r % 13) - 6;
    wanderY = centre ? 0 : (int)((r >> 8) % 7) - 3;
    nextWanderAt = now + 1500 + (r >> 16) % 2500;
  }
  bool wander = coco::exWanders(face);
  float targetX = wander ? wanderX : 0, targetY = wander ? wanderY : 0;
  if (imu.ok() && prefs.motionOn && motion.orientation() == coco::Orientation::Upright) {
    // tilted: the eyes slide to the low side (leaning right -> look right)
    float tx = -motion.tiltX(), tz = motion.tiltZ();
    if (fabsf(tx) > 0.12f || fabsf(tz) > 0.12f) {
      targetX = kibo::clampi((int)(tx * 28), -10, 10);
      targetY = kibo::clampi((int)(-tz * 20), -6, 6);
    }
  }
  gazeX += (targetX - gazeX) * 0.3f;
  gazeY += (targetY - gazeY) * 0.3f;

  if (face != lastFace) {
    lastFace = face;
    faceSince = now;
  }
  FaceCtx ctx;
  ctx.t = now - faceSince;
  ctx.lookX = (int)gazeX;
  ctx.lookY = (int)gazeY;
  if (ctx.t < kBlinkMs) kibo::drawBlink(canvas, ctx);
  else coco::drawEx(canvas, face, ctx);
  if (lyric) kibo::drawSubtitle(canvas, lyric, lyricT);

  uint32_t held = wasDown && !menuTookPress ? now - pressAt : 0;
  if (held >= 2000) coco::drawHold(canvas, held);
  if (batteryPresent && batteryPct <= 20) {
    canvas.setColor(0);
    canvas.rect(112, 0, 16, 9);
    canvas.setColor(1);
    kibo::drawBatteryIcon(canvas, 114, 1, batteryPct, false);
  }
}

// Runs the game, saves a new best once per game, and leaves Coco's own game
// after a while (or a forgotten game-over screen).
void updateGame(uint32_t now) {
  arcade.update(now);
  if (!arcade.over()) {
    gameScored = false;
  } else if (!gameScored) {
    gameScored = true;
    if (!arcade.autopilot()) vibe.play(Buzz::GameOver);
    int g = (int)arcade.game(), s = arcade.score();
    bool lower = coco::gameLowerIsBetter(arcade.game());
    bool better = prefs.best[g] == 0 || (lower ? s < prefs.best[g] : s > prefs.best[g]);
    if (!arcade.autopilot() && arcade.scoreCounts() && s > 0 && !(lower && s >= 2000) && better) {
      prefs.best[g] = s;
      saveBest(g);
      pet.fun = clamp100(pet.fun + 5);
    }
  }
  if (arcade.autopilot() && now > autoGameUntil) exitGame(now);
  else if (!arcade.autopilot() && arcade.over() && now - arcade.overSince() > 20000) exitGame(now);
}

// ---------------------------------------------------------------- motion

// Settings > Motion setup: average 0.6 s of stillness in each position, then
// build the sensor -> Coco matrix (rows right, up, out) and save it.
void motionSetupSample(const CocoImu::Sample& s, uint32_t now) {
  if (mode != Mode::MotionSetup || !setupMeasureAt || now < setupMeasureAt) return;
  setupSum[0] += s.rx;
  setupSum[1] += s.ry;
  setupSum[2] += s.rz;
  if (++setupCount < 60) return;
  float v[3] = {setupSum[0] / setupCount, setupSum[1] / setupCount, setupSum[2] / setupCount};
  float n = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  setupMeasureAt = 0;
  if (n < 0.8f || n > 1.2f) {  // it was moving
    showToast("hold me still");
    return;
  }
  for (float& x : v) x /= n;
  if (setupStep == 0) {
    memcpy(setupUp, v, sizeof(v));
    setupStep = 1;
    vibe.play(Buzz::Tap);
    return;
  }
  // out = second reading without its "up" part; right = up x out
  float d = v[0] * setupUp[0] + v[1] * setupUp[1] + v[2] * setupUp[2];
  float o[3] = {v[0] - d * setupUp[0], v[1] - d * setupUp[1], v[2] - d * setupUp[2]};
  float on = sqrtf(o[0] * o[0] + o[1] * o[1] + o[2] * o[2]);
  if (on < 0.7f) {  // not lying on its back
    showToast("lay me on my back");
    return;
  }
  for (float& x : o) x /= on;
  const float* u = setupUp;
  float r[3] = {u[1] * o[2] - u[2] * o[1], u[2] * o[0] - u[0] * o[2], u[0] * o[1] - u[1] * o[0]};
  float m[9] = {r[0], r[1], r[2], u[0], u[1], u[2], o[0], o[1], o[2]};
  memcpy(prefs.imuMatrix, m, sizeof(m));
  saveImuMatrix();
  imu.setMatrix(m);
  motion = coco::MotionDetector();
  vibe.play(Buzz::Hello);
  showToast("motion ready!", 2000);
  openMenu(MenuId::Settings, sMotionSetup, now);
}

// Turns a motion event into a reaction. Falls work everywhere (a dropped Coco
// comes back to its face); the rest only on the face, not during your game.
void onMotion(coco::MotionEvent e, uint32_t now) {
  using coco::MotionEvent;
  lastMotionEvent = e;
  lastMotionAt = now;
  Serial.printf("motion: %s (%s)\n", coco::motionEventName(e), coco::orientationName(motion.orientation()));
  if (!prefs.motionOn || setupMode || booting || mode == Mode::MotionSetup) return;
  bool playing = mode == Mode::Game && !arcade.autopilot();
  bool onFace = mode == Mode::Face;
  bool wasNapping = napping(now);
  switch (e) {
    case MotionEvent::FallStart:
      if (playing) return;
      toFace();
      pinned = false;
      react(Ex::Scared, 3000);
      showToast("AAAAH!", 1500);
      vibe.play(Buzz::Alarm);
      break;
    case MotionEvent::Landed:
      if (playing) return;
      toFace();
      react(Ex::Slapped, 2000);
      followEx = pet.fun < 40 ? Ex::Crying : Ex::Dizzy;
      followMs = 2500;
      showToast("OUCH!", 1500);
      vibe.play(Buzz::Thump);
      pet.fun = clamp100(pet.fun - 10);
      break;
    case MotionEvent::Caught:
      if (playing) return;
      toFace();
      react(Ex::Surprised, 1000);
      followEx = Ex::Love;
      followMs = 2500;
      showToast("phew! thanks", 1500);
      vibe.play(Buzz::Giggle);
      break;
    case MotionEvent::Knock:
      if (onFace && !wasNapping) onSlap(now);  // a real slap on the case
      else if (onFace) react(Ex::Surprised, 1500);
      break;
    case MotionEvent::ShakeStart:
      if (playing || !(onFace || mode == Mode::Game)) return;
      toFace();
      shakeSince = now;
      shakeAngry = false;
      react(Ex::Dizzy, 60000);
      vibe.play(Buzz::Rumble, true);
      break;
    case MotionEvent::ShakeEnd:
      if (!onFace) return;
      vibe.stop();
      react(Ex::Dizzy, 1800);
      followEx = shakeAngry ? Ex::Angry : Ex::Confused;
      followMs = 2500;
      if (shakeAngry) sulkUntil = now + 1800 + 2500 + 6000;
      break;
    case MotionEvent::Spin:
      if (!onFace) return;
      react(Ex::Dizzy, 3000);
      vibe.play(Buzz::Giggle);
      break;
    case MotionEvent::Moved:
      if (onFace && wasNapping) {
        react(Ex::Yawn, 2800);
        vibe.play(Buzz::Hello);
      }
      break;
    case MotionEvent::Orient:
      if (!onFace) return;
      switch (motion.orientation()) {
        case coco::Orientation::UpsideDown:
          showToast("put me back!", 2500);
          vibe.play(Buzz::Alarm);
          reactionUntil = 0;
          break;
        case coco::Orientation::Upright:
          react(Ex::Happy, 1500);
          vibe.play(Buzz::Tap);
          break;
        case coco::Orientation::OnBack:
          react(Ex::Yawn, 2800);  // then sleeps (rest face) until stood up
          break;
        case coco::Orientation::FaceDown: showToast("so dark...", 2000); break;
        default: reactionUntil = 0; break;
      }
      break;
    default: break;
  }
  // being handled counts as attention (but lying still on its back does not)
  if (e != MotionEvent::Orient || motion.orientation() == coco::Orientation::Upright) noteInteraction(now);
}

void updateMotion(uint32_t now) {
  if (!imu.ok()) return;
  CocoImu::Sample buf[24];
  int n = imu.read(buf, 24);
  for (int i = 0; i < n; i++) {
    const CocoImu::Sample& s = buf[i];
    motionSetupSample(s, now);
    motionRate = sqrtf(s.gx * s.gx + s.gy * s.gy + s.gz * s.gz);
    coco::MotionEvent e = motion.update(s.ax, s.ay, s.az, s.gx, s.gy, s.gz);
    if (e != coco::MotionEvent::None) onMotion(e, now);
  }
  if (!prefs.motionOn || mode != Mode::Face) return;
  // shaken for long: from dizzy to furious
  if (motion.shaking() && !shakeAngry && now - shakeSince > 3500) {
    shakeAngry = true;
    react(Ex::Furious, 60000);
    showToast("STOP IT!", 2000);
  }
  if (motion.shaking()) lastInteraction = now;
  // face down and still: goodnight
  if (FACE_DOWN_SLEEP_S && motion.orientation() == coco::Orientation::FaceDown &&
      motion.stillMs() > FACE_DOWN_SLEEP_S * 1000UL)
    goToSleep();
}

void render(uint32_t now) {
  canvas.clear();
  canvas.setColor(1);

  if (booting) {
    if (kibo::drawBoot(canvas, now - bootAt, kVersion)) booting = false;
    return;
  }
  if (setupMode) {
    kibo::drawWifiSetup(canvas, portal::apName.c_str(), portal::kUrl, portal::phoneJoined(), now - setupSince);
    uint32_t held = wasDown ? now - pressAt : 0;
    if (held >= 2000) coco::drawHold(canvas, held);
    return;
  }
  if (wifiFailed) {
    wifiFailed = false;
    if (mode == Mode::Face) noticeUntil = now + 8000;
  }
  if (now < noticeUntil && mode == Mode::Face) {
    char msg[64];
    snprintf(msg, sizeof(msg), "Can't join %s. Hold my head 10 s to fix it.", settings.ssid.c_str());
    kibo::drawNotice(canvas, "Wi-Fi", msg, true);
    return;
  }

  if ((mode == Mode::Menu || mode == Mode::Card || mode == Mode::Gallery || mode == Mode::MotionSetup) &&
      now > modeUntil)
    toFace();
  if (mode == Mode::Game) updateGame(now);  // may leave the game

  switch (mode) {
    case Mode::Menu: {
      coco::MenuRow rows[24];
      const char* title = "Menu";
      int n = buildMenu(rows, &title);
      char clock[10];
      menuClock(clock, sizeof(clock));
      coco::drawList(canvas, title, rows, n, menuSel, clock);
      break;
    }
    case Mode::Card: {
      uint32_t t = now - cardSince;
      switch (card) {
        case Card::Clock: drawClockCard(t); break;
        case Card::Weather: drawWeatherCard(t); break;
        case Card::Status: drawStatusCard(); break;
        case Card::Stats: coco::drawStats(canvas, 100 - pet.hunger, pet.fun, pet.energy, feeling(), pet.slaps, pet.pets); break;
        case Card::Motion:
          modeUntil = now + kMenuIdleMs;  // stays while you play with it
          coco::drawMotion(canvas, imu.ok(), imu.chipName(), -motion.tiltX(), motion.tiltZ(), motion.lastMagnitude(),
                           motionRate, coco::orientationName(motion.orientation()),
                           coco::motionEventName(lastMotionEvent),
                           lastMotionAt ? (int)((now - lastMotionAt) / 1000) : -1);
          break;
        default:
          coco::drawAbout(canvas, kVersion, BOARD_NAME, OLED_SH1106 ? "1.3in SH1106" : "0.96in SSD1306", coco::kExCount,
                          (int)(ESP.getFreeHeap() / 1024));
          break;
      }
      break;
    }
    case Mode::Gallery: {
      FaceCtx ctx;
      ctx.t = now - gallerySince;
      coco::drawEx(canvas, (Ex)galleryIdx, ctx);
      char label[32];
      snprintf(label, sizeof(label), "%d/%d %s", galleryIdx + 1, coco::kExCount, coco::exLabel((Ex)galleryIdx));
      coco::drawFooter(canvas, label);
      break;
    }
    case Mode::Game: break;  // drawn below
    case Mode::MotionSetup:
      coco::drawMotionSetup(canvas, setupMeasureAt && now >= setupMeasureAt ? 2 : setupStep,
                            setupMeasureAt && now >= setupMeasureAt ? now - setupMeasureAt : 0);
      break;
    default: renderFace(now); break;
  }
  if (mode == Mode::Game) arcade.draw(canvas, now, gameBestBefore);
  if (now < toastUntil) coco::drawToast(canvas, toast);
}

void flush() {
  if (haveSent && memcmp(lastSent, canvas.buf, sizeof(lastSent)) == 0) return;  // unchanged
  oled.show(canvas.buf);
  memcpy(lastSent, canvas.buf, sizeof(lastSent));
  haveSent = true;
}

// --------------------------------------------------------------- main

void setup() {
  setCpuFrequencyMhz(80);  // plenty for 25 fps, ~30% less current than 160 MHz
  Serial.begin(115200);
  pinMode(PIN_TOUCH, INPUT);
  if (PIN_LED >= 0) pinMode(PIN_LED, OUTPUT);
  setLed(false);
  if (PIN_BATTERY >= 0) analogSetPinAttenuation(PIN_BATTERY, ADC_11db);
  if (PIN_IMU_INT >= 0) pinMode(PIN_IMU_INT, INPUT);
  loadSettings();
  loadPrefs();
  setenv("TZ", settings.tz.c_str(), 1);
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
  if (pet.magic != kPetMagic) {
    pet = {kPetMagic, 30, 70, 80, 0, 0};
  } else if (fromSleep) {  // rested, and a bit hungrier
    pet.energy = max<int16_t>(pet.energy, 90);
    pet.hunger = clamp100(pet.hunger + 10);
  }

  if (!oled.begin(PIN_SDA, PIN_SCL, OLED_ADDR, prefs.flip, contrastLevel()))
    Serial.println("OLED not found - check SDA/SCL and address 0x3C");
  if (imu.begin()) {
    if (prefs.imuCalibrated) {
      imu.setMatrix(prefs.imuMatrix);
    } else {  // the mounting from coco_config.h
      float m[9] = {0};
      const int axes[3] = {IMU_RIGHT, IMU_UP, IMU_OUT};
      for (int r = 0; r < 3; r++) m[r * 3 + abs(axes[r]) - 1] = axes[r] > 0 ? 1.0f : -1.0f;
      imu.setMatrix(m);
    }
    Serial.printf("motion sensor: %s%s\n", imu.chipName(), prefs.imuCalibrated ? " (measured mounting)" : "");
  } else {
    Serial.println("motion sensor (MPU6050) not found - motion reactions off");
  }
  vibe.begin(PIN_VIBE);
  vibe.setLevel(prefs.vibe);
  vibe.play(Buzz::Hello);

  uint32_t now = millis();
  rng ^= esp_random();
  bootAt = now;
  lastInteraction = now;
  nextActivityAt = now + 20000;
  nextPetTick = now + 60000;
  booting = !fromSleep;
  if (fromSleep) react(Ex::Yawn, 2800);
  updateBattery(now);

  setupMode = setupRequest == kSetupMagic || settings.ssid.length() == 0;
  setupRequest = 0;
  if (setupMode) {
    booting = false;
    setupSince = now;
    portal::begin();
  } else {
    xTaskCreate(netTask, "coco_net", 8192, nullptr, 1, &netHandle);
  }
  Serial.printf("Coco Yeti %s on %s, %s, %d faces (%s%s)\n", kVersion, BOARD_NAME,
                OLED_SH1106 ? "SH1106" : "SSD1306", coco::kExCount, fromSleep ? "woke from sleep" : "power on",
                setupMode ? ", Wi-Fi setup" : "");
}

void loop() {
  static uint32_t last = millis();
  uint32_t now = millis();
  if (setupMode) {
    portal::loop();
    lastInteraction = now;  // no naps while setting up
    if (settings.ssid.length() && now - setupSince > 15 * 60000UL) ESP.restart();
  }
  if (!booting) updateTouch(now);
  updateMotion(now);
  vibe.update(millis());
  updateBattery(now);
  updatePet(now);
  if (kDeepSleepMs && mode == Mode::Face && napping(now) && now - lastInteraction > napAfter() + kDeepSleepMs)
    goToSleep();
  render(now);
  flush();
  uint32_t spent = millis() - last;
  delay(spent < kFrameMs ? kFrameMs - spent : 1);
  last = millis();
}
