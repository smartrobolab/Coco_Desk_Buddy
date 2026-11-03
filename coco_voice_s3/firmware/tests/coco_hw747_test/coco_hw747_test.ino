// Coco Voice S3 (HW-747) - hardware test
//
// Tests every part of the Coco Voice S3 HW-747 build, one at a time, before the real firmware
// goes on. Same pins as coco_voice_s3/images/coco_voice_s3_hw747_esp32s3_wiring.png.
//
// Arduino IDE: board "ESP32S3 Dev Module", USB CDC On Boot: Enabled, Flash Size: 4MB,
// PSRAM: QSPI PSRAM. Library: U8g2. Serial Monitor at 115200 baud, then type a number:
//
//   1  RGB LED (GPIO48)          red, green, blue, off
//   2  OLED (I2C GPIO5 / GPIO6)  I2C scan, then a test picture on the 1.3" SH1106
//   3  touch (GPIO1)             shows TOUCHED / free for 10 s
//   4  battery (GPIO2)           330k / 330k divider: pin voltage, battery voltage and %
//   5  speaker (MAX98357A)       three beeps: 440, 880, 1320 Hz
//   6  microphone (INMP441)      sound level bar for 10 s: talk or clap
//   7  all of the above
//
// arduino-cli: arduino-cli compile -b esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=4M,PSRAM=enabled .

#include <Wire.h>
#include <U8g2lib.h>
#include <ESP_I2S.h>

#define PIN_TOUCH    1
#define PIN_BATTERY  2
#define PIN_AMP_DIN  4
#define PIN_SDA      5
#define PIN_SCL      6
#define PIN_BCLK     7
#define PIN_WS       8
#define PIN_MIC_SD   9
#define PIN_AMP_SD   43
#define PIN_RGB      48

const float DIVIDER_TOP = 330000.0;     // BAT_SW -> GPIO2
const float DIVIDER_BOTTOM = 330000.0;  // GPIO2 -> GND
const int SAMPLE_RATE = 16000;

U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE, PIN_SCL, PIN_SDA);
I2SClass i2s;
bool oledFound = false;

void showMenu() {
  Serial.println();
  Serial.println("Coco Voice S3 (HW-747) hardware test - type a number:");
  Serial.println("  1 RGB LED   2 OLED   3 touch   4 battery   5 speaker   6 microphone   7 all");
}

void oledText(const char* line1, const char* line2) {
  if (!oledFound) return;
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x12_tf);
  oled.drawStr(0, 12, "Coco HW-747 test");
  oled.drawStr(0, 36, line1);
  oled.drawStr(0, 52, line2);
  oled.sendBuffer();
}

// ---------------------------------------------------------------- 1 RGB LED
void testLed() {
  Serial.println("\n[1] RGB LED on GPIO48: red, green, blue, off");
  const char* names[] = {"red", "green", "blue"};
  const uint8_t colours[3][3] = {{40, 0, 0}, {0, 40, 0}, {0, 0, 40}};
  for (int i = 0; i < 3; i++) {
    Serial.printf("    %s\n", names[i]);
    rgbLedWrite(PIN_RGB, colours[i][0], colours[i][1], colours[i][2]);
    delay(700);
  }
  rgbLedWrite(PIN_RGB, 0, 0, 0);
  Serial.println("    PASS if you saw red, green and blue. Some boards swap red and green: that is fine.");
}

// ---------------------------------------------------------------- 2 OLED
void testOled() {
  Serial.println("\n[2] OLED: I2C scan on SDA GPIO5, SCL GPIO6");
  Wire.begin(PIN_SDA, PIN_SCL);
  int found = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.printf("    device at 0x%02X%s\n", a, a == 0x3C || a == 0x3D ? "  <- OLED" : "");
      found++;
    }
  }
  if (!found) {
    Serial.println("    FAIL: nothing found. Check VCC (3V3), GND, and that SDA/SCL are not swapped.");
    oledFound = false;
    return;
  }
  oled.begin();
  oledFound = true;
  oled.clearBuffer();
  oled.drawFrame(0, 0, 128, 64);
  oled.drawRBox(24, 18, 22, 28, 6);  // Coco's eyes
  oled.drawRBox(82, 18, 22, 28, 6);
  oled.setFont(u8g2_font_6x12_tf);
  oled.drawStr(30, 60, "OLED OK 128x64");
  oled.sendBuffer();
  Serial.println("    PASS if the screen shows a frame, two eyes and \"OLED OK\".");
  Serial.println("    Snow / shifted picture: it is an SSD1306, not an SH1106 (change the U8G2_ line).");
}

// ---------------------------------------------------------------- 3 touch
void testTouch() {
  Serial.println("\n[3] TTP223 touch on GPIO1: touch Coco's head for the next 10 s");
  pinMode(PIN_TOUCH, INPUT);
  int last = -1, touches = 0;
  unsigned long end = millis() + 10000;
  while (millis() < end) {
    int now = digitalRead(PIN_TOUCH);
    if (now != last) {
      Serial.println(now ? "    TOUCHED" : "    free");
      oledText("touch:", now ? "TOUCHED" : "free");
      if (now) touches++;
      last = now;
    }
    delay(20);
  }
  if (touches) Serial.printf("    PASS: %d touch(es) seen\n", touches);
  else Serial.println("    FAIL: never HIGH. Module LED lights on touch? Then check the I/O wire to GPIO1.");
}

// ---------------------------------------------------------------- 4 battery
int percentFromVolts(float v) {
  const float kV[] = {3.30, 3.50, 3.60, 3.70, 3.80, 3.90, 4.00, 4.10, 4.20};
  const int kP[] = {0, 5, 15, 30, 50, 65, 80, 90, 100};
  if (v <= kV[0]) return 0;
  for (int i = 1; i < 9; i++)
    if (v < kV[i]) return kP[i - 1] + (int)((v - kV[i - 1]) / (kV[i] - kV[i - 1]) * (kP[i] - kP[i - 1]));
  return 100;
}

void testBattery() {
  Serial.println("\n[4] battery sense on GPIO2 (330k / 330k divider)");
  analogSetPinAttenuation(PIN_BATTERY, ADC_11db);
  long sum = 0;
  for (int i = 0; i < 32; i++) sum += analogReadMilliVolts(PIN_BATTERY);
  float vPin = sum / 32.0 / 1000.0;
  float vBat = vPin * (DIVIDER_TOP + DIVIDER_BOTTOM) / DIVIDER_BOTTOM;
  Serial.printf("    GPIO2 = %.3f V  ->  battery = %.2f V\n", vPin, vBat);
  char line[24];
  snprintf(line, sizeof line, "%.2f V", vBat);
  oledText("battery:", line);
  if (vBat < 2.5) {
    Serial.println("    No cell (or switch OFF). With the cell in and switch ON this must be 3.0-4.2 V.");
  } else if (vBat > 4.35) {
    Serial.println("    FAIL: too high. Is the top resistor really 330k (orange-orange-yellow)?");
  } else {
    Serial.printf("    PASS: %d %%. Compare with your multimeter on the cell: within 0.1 V is good.\n",
                  percentFromVolts(vBat));
  }
}

// ---------------------------------------------------------------- 5 speaker
void testSpeaker() {
  Serial.println("\n[5] speaker: MAX98357A DIN GPIO4, BCLK GPIO7, LRC GPIO8, SD GPIO43");
  pinMode(PIN_AMP_SD, OUTPUT);
  digitalWrite(PIN_AMP_SD, HIGH);  // amp on (left channel)
  i2s.setPins(PIN_BCLK, PIN_WS, PIN_AMP_DIN, -1);
  if (!i2s.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
    Serial.println("    FAIL: I2S did not start");
    return;
  }
  oledText("speaker:", "beep beep beep");
  static int16_t buf[2 * 256];
  for (int f : {440, 880, 1320}) {
    Serial.printf("    %d Hz\n", f);
    float phase = 0, step = 2 * PI * f / SAMPLE_RATE;
    for (int n = 0; n < SAMPLE_RATE / 2 / 256; n++) {  // 0.5 s
      for (int i = 0; i < 256; i++) {
        int16_t s = (int16_t)(6000 * sinf(phase));  // about 20 % volume
        buf[2 * i] = s;
        buf[2 * i + 1] = s;
        phase += step;
        if (phase > 2 * PI) phase -= 2 * PI;
      }
      i2s.write((uint8_t*)buf, sizeof buf);
    }
    memset(buf, 0, sizeof buf);
    for (int n = 0; n < 8; n++) i2s.write((uint8_t*)buf, sizeof buf);  // short pause
  }
  i2s.end();
  digitalWrite(PIN_AMP_SD, LOW);  // amp off
  Serial.println("    PASS if you heard three rising beeps.");
  Serial.println("    Silent: VIN on BAT_SW (not 3V3 only), SD HIGH, speaker on OUT+/OUT-, DIN/BCLK/LRC wires.");
}

// ---------------------------------------------------------------- 6 microphone
void testMic() {
  Serial.println("\n[6] microphone: INMP441 SD GPIO9, SCK GPIO7, WS GPIO8, L/R -> GND. Talk or clap for 10 s");
  i2s.setPins(PIN_BCLK, PIN_WS, -1, PIN_MIC_SD);
  if (!i2s.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO)) {
    Serial.println("    FAIL: I2S did not start");
    return;
  }
  static int32_t buf[2 * 512];
  int32_t quiet = INT32_MAX, loud = 0;
  bool allZero = true;
  unsigned long end = millis() + 10000;
  while (millis() < end) {
    size_t got = i2s.readBytes((char*)buf, sizeof buf) / sizeof(int32_t);
    if (got < 2) continue;
    // left slot (L/R = GND), 24-bit sample in the top bits; remove the DC offset
    double mean = 0;
    for (size_t i = 0; i < got; i += 2) mean += buf[i] >> 8;
    mean /= got / 2;
    double sq = 0;
    for (size_t i = 0; i < got; i += 2) {
      if (buf[i] != 0) allZero = false;
      double v = (buf[i] >> 8) - mean;
      sq += v * v;
    }
    int32_t rms = (int32_t)sqrt(sq / (got / 2));
    quiet = min(quiet, rms);
    loud = max(loud, rms);
    int bars = constrain((int)(20 * log10(1 + rms) - 40), 0, 40);
    char bar[41];
    memset(bar, '#', bars);
    bar[bars] = 0;
    Serial.printf("    level %7ld |%s\n", (long)rms, bar);
    char line[24];
    snprintf(line, sizeof line, "level %ld", (long)rms);
    oledText("microphone:", line);
    delay(150);
  }
  i2s.end();
  if (allZero) {
    Serial.println("    FAIL: only zeros. Check VDD (3V3), SD -> GPIO9, SCK -> GPIO7, WS -> GPIO8, L/R -> GND.");
  } else if (loud < quiet * 4) {
    Serial.printf("    CHECK: level hardly changed (%ld .. %ld). Is the sound hole open and facing out?\n",
                  (long)quiet, (long)loud);
  } else {
    Serial.printf("    PASS: quiet %ld, loud %ld\n", (long)quiet, (long)loud);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1500);  // time for the USB serial port to open
  pinMode(PIN_AMP_SD, OUTPUT);
  digitalWrite(PIN_AMP_SD, LOW);
  pinMode(PIN_TOUCH, INPUT);
  testOled();
  showMenu();
}

void loop() {
  if (!Serial.available()) return;
  char c = Serial.read();
  switch (c) {
    case '1': testLed(); break;
    case '2': testOled(); break;
    case '3': testTouch(); break;
    case '4': testBattery(); break;
    case '5': testSpeaker(); break;
    case '6': testMic(); break;
    case '7':
      testLed();
      testOled();
      testTouch();
      testBattery();
      testSpeaker();
      testMic();
      break;
    default: return;  // newlines etc.
  }
  showMenu();
}
