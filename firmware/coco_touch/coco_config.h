// Coco Touch - your settings. Edit this file, then upload.
#pragma once

// ---------------------------------------------------------------- Wi-Fi
// Leave WIFI_SSID empty ("") to run offline: faces and touch still work,
// the clock and weather cards just say they have no data.
#define WIFI_SSID     ""
#define WIFI_PASSWORD ""

// ------------------------------------------------------- time + weather
// POSIX time zone string. Examples:
//   Japan "JST-9"   India "IST-5:30"   UK "GMT0BST,M3.5.0/1,M10.5.0"
//   US Eastern "EST5EDT,M3.2.0,M11.1.0"   Central Europe "CET-1CEST,M3.5.0,M10.5.0/3"
#define TZ_INFO "JST-9"

// Weather location (decimal degrees) and the short name shown on the card.
// Weather comes from open-meteo.com (free, no API key).
#define WEATHER_LAT   35.6812
#define WEATHER_LON   139.7671
#define WEATHER_PLACE "Tokyo"
#define WEATHER_REFRESH_MIN 30   // Wi-Fi switches on only for each refresh

// ------------------------------------------------------------- behaviour
#define NAP_AFTER_MIN        5    // no touch for this long -> Coco falls asleep (Zzz)
#define DEEP_SLEEP_AFTER_MIN 30   // asleep this long -> screen off, deep sleep (touch wakes). 0 = never
#define NIGHT_FROM_HOUR      23   // at night Coco naps after 1 minute instead
#define NIGHT_TO_HOUR        7
#define HOURLY_CLOCK         1    // show the clock for a few seconds at every full hour

// ----------------------------------------------------------------- pins
// ESP32-C3 Super Mini. Matches docs/images/coco_touch_wiring.png.
#define PIN_SDA     4    // OLED SDA
#define PIN_SCL     5    // OLED SCL
#define PIN_TOUCH   0    // TTP223 I/O (HIGH = touched). Must be GPIO0-5 to wake from deep sleep.
#define PIN_BATTERY 1    // battery divider middle: BAT+ -> 1M -> GPIO1 -> 330k -> GND (+100nF)
#define PIN_LED     8    // on-board blue LED (active LOW)

#define BATTERY_DIVIDER ((1000000.0f + 330000.0f) / 330000.0f)

// --------------------------------------------------------------- display
#define OLED_ADDR       0x3C
#define OLED_ROTATE_180 0    // 1 if the OLED is mounted with its pin header at the bottom
#define OLED_CONTRAST   0xCF
#define OLED_CONTRAST_NAP 0x08
