// Minimal SSD1306 128x64 I2C driver. kibo::Canvas already keeps its pixels in
// the SSD1306 page layout, so a frame is sent as-is with no library needed.
#pragma once
#include <Arduino.h>
#include <Wire.h>

class CocoOled {
 public:
  bool begin(int sda, int scl, uint8_t addr, bool rotate180, uint8_t contrast) {
    addr_ = addr;
    Wire.begin(sda, scl, 400000);
    Wire.beginTransmission(addr_);
    if (Wire.endTransmission() != 0) return false;  // nothing answering on I2C
    const uint8_t init[] = {
        0xAE,                     // display off
        0xD5, 0x80,               // clock divide
        0xA8, 0x3F,               // multiplex 64
        0xD3, 0x00,               // display offset 0
        0x40,                     // start line 0
        0x8D, 0x14,               // charge pump on
        0x20, 0x00,               // horizontal addressing
        (uint8_t)(rotate180 ? 0xA0 : 0xA1),  // segment remap
        (uint8_t)(rotate180 ? 0xC0 : 0xC8),  // COM scan direction
        0xDA, 0x12,               // COM pins
        0x81, contrast,           // contrast
        0xD9, 0xF1,               // pre-charge
        0xDB, 0x40,               // VCOM detect
        0xA4,                     // show RAM
        0xA6,                     // normal (not inverted)
        0xAF,                     // display on
    };
    for (uint8_t c : init) command(c);
    ok_ = true;
    return true;
  }

  bool ok() const { return ok_; }
  void contrast(uint8_t v) {
    command(0x81);
    command(v);
  }
  void power(bool on) { command(on ? 0xAF : 0xAE); }

  // buf: 1024 bytes, 8 pages of 128 columns (bit 0 = top pixel of the page).
  void show(const uint8_t* buf) {
    if (!ok_) return;
    const uint8_t window[] = {0x21, 0, 127, 0x22, 0, 7};
    for (uint8_t c : window) command(c);
    for (int i = 0; i < 1024; i += 32) {
      Wire.beginTransmission(addr_);
      Wire.write(0x40);  // data
      Wire.write(buf + i, 32);
      Wire.endTransmission();
    }
  }

 private:
  void command(uint8_t c) {
    Wire.beginTransmission(addr_);
    Wire.write(0x00);  // command
    Wire.write(c);
    Wire.endTransmission();
  }

  uint8_t addr_ = 0x3C;
  bool ok_ = false;
};
