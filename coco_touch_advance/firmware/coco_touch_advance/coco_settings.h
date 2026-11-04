// Coco's saved settings (Wi-Fi, city, time zone) in flash (NVS "coco").
// Set from the phone setup page (coco_portal.h); coco_config.h only gives the
// defaults used before anything was saved.
#pragma once
#include <Arduino.h>
#include <Preferences.h>

#include "coco_config.h"

struct CocoSettings {
  String ssid, pass;
  char place[32] = "";  // city as typed / as found by the geocoder (guarded by settingsMux)
  float lat = WEATHER_LAT, lon = WEATHER_LON;
  bool needGeocode = false;  // `place` was typed but not looked up yet
  String tz;           // POSIX TZ, learned from the weather location
  String owmKey;       // OpenWeatherMap API key; empty = use open-meteo (no key)
};

CocoSettings settings;
portMUX_TYPE settingsMux = portMUX_INITIALIZER_UNLOCKED;  // tz/place/lat/lon change from the net task

void loadSettings() {
  Preferences p;
  p.begin("coco", true);  // read-only; a missing namespace just returns the defaults
  settings.ssid = p.getString("ssid", WIFI_SSID);
  settings.pass = p.getString("pass", WIFI_PASSWORD);
  strlcpy(settings.place, p.getString("place", WEATHER_PLACE).c_str(), sizeof(settings.place));
  settings.lat = p.getFloat("lat", WEATHER_LAT);
  settings.lon = p.getFloat("lon", WEATHER_LON);
  settings.needGeocode = p.getBool("geo", false);
  settings.tz = p.getString("tz", TZ_INFO);
  settings.owmKey = p.getString("owm", OWM_API_KEY);
  p.end();
}

// From the setup page. An empty password for the already saved network keeps the old one.
// owmKey: empty = keep the saved key, "-" = remove it (back to open-meteo), else the new key.
void saveWifi(const String& ssid, const String& pass, const String& city, const String& owmKey) {
  Preferences p;
  p.begin("coco", false);
  bool samePlace = city.length() == 0 || city.equalsIgnoreCase(String(settings.place));
  String keepPass = (pass.length() == 0 && ssid == settings.ssid) ? settings.pass : pass;
  p.putString("ssid", ssid);
  p.putString("pass", keepPass);
  if (owmKey == "-") p.remove("owm");
  else if (owmKey.length()) p.putString("owm", owmKey);
  if (!samePlace) {
    p.putString("place", city);
    p.putBool("geo", true);  // look the city up once Wi-Fi works
  }
  p.end();
}

void saveLocation(float lat, float lon, const String& name) {
  Preferences p;
  p.begin("coco", false);
  p.putFloat("lat", lat);
  p.putFloat("lon", lon);
  p.putString("place", name);
  p.putBool("geo", false);
  p.end();
  portENTER_CRITICAL(&settingsMux);
  settings.lat = lat;
  settings.lon = lon;
  settings.needGeocode = false;
  strlcpy(settings.place, name.c_str(), sizeof(settings.place));
  portEXIT_CRITICAL(&settingsMux);
}

void saveTz(const String& tz) {
  Preferences p;
  p.begin("coco", false);
  p.putString("tz", tz);
  p.end();
}

void forgetWifi() {
  Preferences p;
  p.begin("coco", false);
  p.remove("ssid");
  p.remove("pass");
  p.end();
}
