// Coco Touch Advance: the settings you change from the on-screen menu, and the
// game high scores. Saved in flash (NVS namespace "cocoadv"); Wi-Fi, password,
// city and API key stay in "coco" (coco_settings.h), set from the phone page.
#pragma once
#include <Arduino.h>
#include <Preferences.h>

#include "coco_config.h"
#include "coco_games.h"

// ---- choices shown in Settings (index = saved value)
const char* const kBrightNames[] = {"Low", "Medium", "High", "Max"};
const uint8_t kBrightLevels[] = {0x10, 0x50, 0xCF, 0xFF};
constexpr int kBrightCount = 4;

enum class Mood : uint8_t { Happy, Calm, Playful, Sleepy, Grumpy, Random, Count };
const char* const kMoodNames[] = {"Happy", "Calm", "Playful", "Sleepy", "Grumpy", "Random"};

// how often Coco does something by itself (eats, plays, sings...)
const char* const kShuffleNames[] = {"Off", "Rare", "Normal", "Often"};
const uint32_t kShuffleMinMs[] = {0, 60000, 20000, 6000};
const uint32_t kShuffleSpanMs[] = {0, 60000, 25000, 9000};
constexpr int kShuffleCount = 4;

// which jokes Coco tells by itself (Menu > Tell a joke always tells one)
const char* const kJokeNames[] = {"All", "Coding", "Hindi", "Marathi", "Off"};
constexpr int kJokeSettingCount = 5;
constexpr int kJokesOff = 4;

const char* const kVibeNames[] = {"Off", "Soft", "Strong"};
constexpr int kVibeCount = 3;

const char* const kNapNames[] = {"1 min", "2 min", "5 min", "10 min", "30 min", "Never"};
const uint16_t kNapMinutes[] = {1, 2, 5, 10, 30, 0};
constexpr int kNapCount = 6;

struct AdvPrefs {
  uint8_t brightness = DEFAULT_BRIGHTNESS;
  uint8_t mood = DEFAULT_MOOD;
  uint8_t shuffle = DEFAULT_SHUFFLE;
  uint8_t nap = DEFAULT_NAP;
  uint8_t jokes = DEFAULT_JOKES;
  uint8_t vibe = DEFAULT_VIBE;
  bool motionOn = true;
  bool imuCalibrated = false;
  float imuMatrix[9] = {0};  // rows right, up, out in sensor axes (Settings > Motion setup)
  bool clock24 = DEFAULT_CLOCK_24H;
  bool flip = OLED_ROTATE_180;
  bool wifiOn = true;
  int32_t best[(int)coco::Game::Count] = {0};
};

AdvPrefs prefs;

void loadPrefs() {
  Preferences p;
  p.begin("cocoadv", true);
  prefs.brightness = min<uint8_t>(p.getUChar("bright", DEFAULT_BRIGHTNESS), kBrightCount - 1);
  prefs.mood = min<uint8_t>(p.getUChar("mood", DEFAULT_MOOD), (uint8_t)Mood::Count - 1);
  prefs.shuffle = min<uint8_t>(p.getUChar("shuffle", DEFAULT_SHUFFLE), kShuffleCount - 1);
  prefs.nap = min<uint8_t>(p.getUChar("nap", DEFAULT_NAP), kNapCount - 1);
  prefs.jokes = min<uint8_t>(p.getUChar("jokes", DEFAULT_JOKES), kJokeSettingCount - 1);
  prefs.vibe = min<uint8_t>(p.getUChar("vibe", DEFAULT_VIBE), kVibeCount - 1);
  prefs.motionOn = p.getBool("motion", true);
  prefs.imuCalibrated = p.getBytes("imum", prefs.imuMatrix, sizeof(prefs.imuMatrix)) == sizeof(prefs.imuMatrix);
  prefs.clock24 = p.getBool("c24", DEFAULT_CLOCK_24H);
  prefs.flip = p.getBool("flip", OLED_ROTATE_180);
  prefs.wifiOn = p.getBool("wifi", true);
  for (int i = 0; i < (int)coco::Game::Count; i++) {
    char key[6] = {'b', 'e', 's', 't', (char)('0' + i), 0};
    prefs.best[i] = p.getInt(key, 0);
  }
  p.end();
}

void savePrefs() {
  Preferences p;
  p.begin("cocoadv", false);
  p.putUChar("bright", prefs.brightness);
  p.putUChar("mood", prefs.mood);
  p.putUChar("shuffle", prefs.shuffle);
  p.putUChar("nap", prefs.nap);
  p.putUChar("jokes", prefs.jokes);
  p.putUChar("vibe", prefs.vibe);
  p.putBool("motion", prefs.motionOn);
  p.putBool("c24", prefs.clock24);
  p.putBool("flip", prefs.flip);
  p.putBool("wifi", prefs.wifiOn);
  p.end();
}

void saveBest(int game) {
  Preferences p;
  p.begin("cocoadv", false);
  char key[6] = {'b', 'e', 's', 't', (char)('0' + game), 0};
  p.putInt(key, prefs.best[game]);
  p.end();
}

void saveImuMatrix() {
  Preferences p;
  p.begin("cocoadv", false);
  p.putBytes("imum", prefs.imuMatrix, sizeof(prefs.imuMatrix));
  p.end();
  prefs.imuCalibrated = true;
}

void resetScores() {
  for (int i = 0; i < (int)coco::Game::Count; i++) {
    prefs.best[i] = 0;
    saveBest(i);
  }
}

// Erases Wi-Fi, city, API key, settings and scores. Coco restarts into the phone setup.
void factoryReset() {
  Preferences p;
  p.begin("cocoadv", false);
  p.clear();
  p.end();
  p.begin("coco", false);
  p.clear();
  p.end();
}
