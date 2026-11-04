// Coco Yeti - defaults, pins, motion sensor and screen (Coco Touch Advance + MPU6050 + vibration motor).
//
// You don't need to edit anything to get going: Wi-Fi, password and city are
// set from your phone (first boot, or hold Coco's head 10 s) and saved in flash.
// The values below are only used until something has been saved.
#pragma once
#include <sdkconfig.h>  // CONFIG_IDF_TARGET_* (which chip this build is for)

// ---------------------------------------------------------------- Wi-Fi
// Optional: pre-fill a network here instead of using the phone setup.
#define WIFI_SSID     ""
#define WIFI_PASSWORD ""

// ------------------------------------------------------- time + weather
// The time zone is learned from the weather location (daylight saving included).
// TZ_INFO is only used before the first weather update. Examples:
//   Japan "JST-9"   India "IST-5:30"   UK "GMT0BST,M3.5.0/1,M10.5.0"
#define TZ_INFO "JST-9"

// Default weather location (decimal degrees) and its name on the card.
// The phone setup page replaces it with the city you type.
// Weather comes from OpenWeatherMap when an API key is set (on the phone setup
// page, or OWM_API_KEY below), otherwise from open-meteo.com (free, no key).
// Coco also falls back to open-meteo whenever OpenWeatherMap does not answer.
#define WEATHER_LAT   35.6812
#define WEATHER_LON   139.7671
#define WEATHER_PLACE "Tokyo"
#define WEATHER_REFRESH_MIN 30   // Wi-Fi switches on only for each refresh

// OpenWeatherMap API key (https://home.openweathermap.org/api_keys). Don't type
// it here if you share this code: put `#define OWM_API_KEY "your key"` in a file
// coco_secrets.h next to this one (git ignores it), or enter it on the phone
// setup page. A key typed on the setup page wins over this default.
#if !defined(COCO_NO_SECRETS) && __has_include("coco_secrets.h")  // build.sh sets COCO_NO_SECRETS for public files
#include "coco_secrets.h"
#endif
#ifndef OWM_API_KEY
#define OWM_API_KEY ""
#endif

// ------------------------------------------------------------- behaviour
#define DEEP_SLEEP_AFTER_MIN 30   // asleep this long -> screen off, deep sleep (touch wakes). 0 = never
#define NIGHT_FROM_HOUR      23   // at night Coco naps after 1 minute instead
#define NIGHT_TO_HOUR        7
#define HOURLY_CLOCK         1    // show the clock for a few seconds at every full hour

// Defaults for the on-screen Settings menu (each can be changed on Coco itself
// and is then saved; these are only used before anything was saved):
#define DEFAULT_BRIGHTNESS   2    // 0 Low, 1 Medium, 2 High, 3 Max
#define DEFAULT_MOOD         0    // 0 Happy, 1 Calm, 2 Playful, 3 Sleepy, 4 Grumpy, 5 Random
#define DEFAULT_SHUFFLE      2    // random faces: 0 Off, 1 Rare, 2 Normal, 3 Often
#define DEFAULT_NAP          2    // nap after: 0 1 min, 1 2 min, 2 5 min, 3 10 min, 4 30 min, 5 never
#define DEFAULT_CLOCK_24H    1    // 0 = 12-hour clock
#define DEFAULT_JOKES        0    // jokes Coco tells: 0 All, 1 Coding, 2 Hindi, 3 Marathi, 4 Off
#define DEFAULT_VIBE         2    // vibration: 0 Off, 1 Soft, 2 Strong

// ----------------------------------------------------------------- board
// Coco Yeti is built for the ESP32-C3 Super Mini (Tools > Board: "ESP32C3 Dev Module").
// Two wirings are supported:
//   YETI_PINOUT_LEROYD 0 (default): Coco's wiring. Touch on GPIO0 and the motion
//       sensor's INT on GPIO3 can wake Coco from deep sleep (weeks on a battery).
//   YETI_PINOUT_LEROYD 1: the Desk Companion wiring from leroyd.com/manual.html
//       (OLED SDA 8 / SCL 9, touch 7, vibration 10). GPIO7 can't wake the C3
//       from deep sleep, so "sleep" is a light sleep there, and there is no
//       battery measurement. INT (optional) still goes to GPIO3.
// arduino-cli: -DYETI_PINOUT_LEROYD=1 (../build.sh builds both).
#if !CONFIG_IDF_TARGET_ESP32C3
#error "Coco Yeti firmware is for the ESP32-C3 Super Mini (board: ESP32C3 Dev Module)"
#endif
#ifndef YETI_PINOUT_LEROYD
#define YETI_PINOUT_LEROYD 0
#endif

#define BOARD_NAME  "ESP32-C3 Yeti"
#if YETI_PINOUT_LEROYD
#define PIN_SDA     8    // OLED + MPU6050 SDA
#define PIN_SCL     9    // OLED + MPU6050 SCL
#define PIN_TOUCH   7    // touch sensor OUT (HIGH = touched); wakes from light sleep only
#define PIN_BATTERY -1   // no battery divider in this wiring
#define PIN_LED     -1   // GPIO8 is SDA here
#define PIN_VIBE    10   // vibration motor module IN/SIG (HIGH = on)
#define PIN_IMU_INT 3    // MPU6050 INT (optional, -1 if not wired)
#else
#define PIN_SDA     4    // OLED + MPU6050 SDA
#define PIN_SCL     5    // OLED + MPU6050 SCL
#define PIN_TOUCH   0    // TTP223 I/O (HIGH = touched). Must be GPIO0-5 to wake from deep sleep.
#define PIN_BATTERY 1    // battery divider middle: BAT+ -> 1M -> GPIO1 -> 330k -> GND (+100nF)
#define PIN_LED     8    // on-board blue LED (active LOW)
#define PIN_VIBE    10   // vibration motor module IN/SIG (HIGH = on), same pin as the Desk Companion
#define PIN_IMU_INT 3    // MPU6050 INT: wakes Coco from deep sleep when it is picked up. -1 = not wired
#endif
#define BATTERY_DIVIDER ((1000000.0f + 330000.0f) / 330000.0f)

// ------------------------------------------------------- motion sensor
// How the MPU6050 sits in the case, as sensor axes: 1 = X, 2 = Y, 3 = Z, minus = reversed.
// Coco's frame: RIGHT = to its right, UP = up, OUT = out of the screen towards you.
// Yeti case: the GY-521 stands on the chassis fin behind the screen, its long side
// left-right, chip facing down (towards the feet). If tilting looks wrong, run
// Settings > Motion setup once (it measures this and saves it): falls, shakes
// and knocks work either way.
#define IMU_RIGHT  1
#define IMU_UP     -3
#define IMU_OUT    2
#define FACE_DOWN_SLEEP_S 5   // lying on its face this long -> goodnight (deep sleep). 0 = never

// --------------------------------------------------------------- display
// Screen: 0 = 0.96" SSD1306, 1 = 1.3" SH1106 (both 128x64 I2C).
// Arduino IDE: change the 0 below. arduino-cli: pass -DOLED_SH1106=1 (see ../build.sh).
#ifndef OLED_SH1106
#define OLED_SH1106 0   // 1 = 1.3" SH1106
#endif
#define OLED_ADDR       0x3C
#define OLED_ROTATE_180 0    // 1 if the OLED is mounted with its pin header at the bottom
#define OLED_CONTRAST   0xCF
#define OLED_CONTRAST_NAP 0x08
