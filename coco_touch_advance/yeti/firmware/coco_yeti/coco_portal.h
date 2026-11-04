// Wi-Fi setup from a phone: Coco opens its own hotspot "Coco-XXXX" and a
// captive page where you pick your Wi-Fi, type the password and your city.
// The OLED shows a QR code to join the hotspot, then one for the page.
#pragma once
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

#include "coco_settings.h"

namespace portal {

WebServer server(80);
DNSServer dns;
String apName;
String networksHtml;          // <option> list from the last scan
uint32_t restartAt = 0;       // set after saving
const char* const kUrl = "http://192.168.4.1";

String htmlEscape(const String& s) {
  String o;
  o.reserve(s.length() + 8);
  for (char c : s) {
    if (c == '&') o += "&amp;";
    else if (c == '<') o += "&lt;";
    else if (c == '>') o += "&gt;";
    else if (c == '"') o += "&quot;";
    else o += c;
  }
  return o;
}

void scan() {
  int n = WiFi.scanNetworks();
  // strongest first, one entry per network name
  int idx[40];
  int m = n < 40 ? (n > 0 ? n : 0) : 40;
  for (int i = 0; i < m; i++) idx[i] = i;
  for (int i = 0; i < m; i++)
    for (int j = i + 1; j < m; j++)
      if (WiFi.RSSI(idx[j]) > WiFi.RSSI(idx[i])) std::swap(idx[i], idx[j]);
  const char* const kBars[] = {"", "&#9675;", "&#9681;", "&#9679;", "&#9679;&#9679;"};
  networksHtml = "";
  String seen = "\n";
  for (int k = 0; k < m; k++) {
    int i = idx[k];
    String ssid = WiFi.SSID(i);
    if (ssid.length() == 0 || seen.indexOf("\n" + ssid + "\n") >= 0) continue;
    seen += ssid + "\n";
    int rssi = WiFi.RSSI(i);
    int bars = rssi > -60 ? 4 : rssi > -70 ? 3 : rssi > -80 ? 2 : 1;
    networksHtml += "<option value=\"" + htmlEscape(ssid) + "\"" + (ssid == settings.ssid ? " selected" : "") + ">" +
                    htmlEscape(ssid) + " " + kBars[bars] +
                    (WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? " (open)" : "") + "</option>";
  }
  WiFi.scanDelete();
}

const char kHead[] PROGMEM = R"HTML(<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Coco setup</title>
<style>
body{font-family:system-ui,sans-serif;background:#14181c;color:#eee;margin:0;padding:20px}
.card{max-width:420px;margin:auto;background:#1f262c;border:2px solid #c9962c;border-radius:16px;padding:20px}
h1{margin:0 0 4px;color:#e9c46a}.eyes{font-size:30px;letter-spacing:6px;color:#28e1eb}
label{display:block;margin:14px 0 4px;font-weight:600}
select,input{width:100%;box-sizing:border-box;padding:12px;font-size:16px;border-radius:10px;border:1px solid #556;background:#0f1418;color:#eee}
button{width:100%;margin-top:18px;padding:14px;font-size:17px;font-weight:700;border:0;border-radius:10px;background:#c9962c;color:#111}
small,a{color:#9ab}.row{display:flex;gap:8px;align-items:center}.row input{flex:1}
</style></head><body><div class="card"><div class="eyes">&#9646; &#9646;</div><h1>Hi, I'm Coco!</h1>
)HTML";

void handleRoot() {
  String h = FPSTR(kHead);
  h += "<small>Pick your Wi-Fi and tell me where you live (for the weather and the clock).</small>";
  h += "<form method='POST' action='/save'>";
  h += "<label>Wi-Fi network</label><select name='ssid' id='ssid' onchange=\"o.style.display=this.value=='__other'?'block':'none'\">";
  h += networksHtml;
  h += "<option value='__other'>Other (type the name)...</option></select>";
  h += "<input name='other' id='o' placeholder='network name' style='display:none;margin-top:8px'>";
  h += "<label>Password</label><div class='row'><input name='pass' id='p' type='password' placeholder='";
  h += settings.ssid.length() ? "empty = keep the saved one" : "Wi-Fi password";
  h += "'><input type='checkbox' style='width:auto' onclick=\"p.type=this.checked?'text':'password'\">show</div>";
  h += "<label>City</label><input name='city' value='" + htmlEscape(String(settings.place)) + "' placeholder='e.g. Tokyo'>";
  h += "<small>Time zone is set automatically from the city.</small>";
  // optional OpenWeatherMap key; the saved key is never sent back to the page
  h += "<label>OpenWeatherMap API key (optional)</label><input name='owm' autocomplete='off' placeholder='";
  h += settings.owmKey.length() ? "saved: empty = keep, - = remove" : "empty = free open-meteo weather";
  h += "'>";
  h += "<button>Save and connect</button></form>";
  h += "<p><a href='/scan'>&#8635; scan again</a></p></div></body></html>";
  server.send(200, "text/html", h);
}

void handleSave() {
  String ssid = server.arg("ssid");
  if (ssid == "__other") ssid = server.arg("other");
  ssid.trim();
  if (ssid.length() == 0) {
    server.send(400, "text/html", String(FPSTR(kHead)) + "<p>Please pick a network.</p><a href='/'>back</a></div></body></html>");
    return;
  }
  String city = server.arg("city");
  city.trim();
  String owm = server.arg("owm");
  owm.trim();
  saveWifi(ssid, server.arg("pass"), city, owm);
  server.send(200, "text/html", String(FPSTR(kHead)) + "<h2>Saved!</h2><p>Coco is joining <b>" + htmlEscape(ssid) +
                                    "</b> now. You can close this page and switch your phone back to your Wi-Fi.</p>"
                                    "<p><small>If Coco can't connect it says so on its screen: hold its head 10 s to come back here.</small></p></div></body></html>");
  restartAt = millis() + 1500;
}

void handleScan() {
  scan();
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

void handleCaptive() {
  // phones probe random URLs (generate_204, hotspot-detect...): send them to the page
  server.sendHeader("Location", String(kUrl) + "/", true);
  server.send(302, "text/plain", "");
}

void begin() {
  uint8_t mac[6];
  WiFi.mode(WIFI_STA);
  WiFi.macAddress(mac);
  char name[16];
  snprintf(name, sizeof(name), "Coco-%02X%02X", mac[4], mac[5]);
  apName = name;
  scan();  // before the hotspot starts, so the phone isn't kicked off by a channel scan
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(apName.c_str());  // open hotspot: the setup page is the only thing on it
  delay(100);
  dns.start(53, "*", WiFi.softAPIP());
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/scan", handleScan);
  server.onNotFound(handleCaptive);
  server.begin();
  Serial.printf("setup: join hotspot %s, open %s\n", name, kUrl);
}

void loop() {
  dns.processNextRequest();
  server.handleClient();
  if (restartAt && millis() > restartAt) ESP.restart();
}

bool phoneJoined() { return WiFi.softAPgetStationNum() > 0; }

}  // namespace portal
