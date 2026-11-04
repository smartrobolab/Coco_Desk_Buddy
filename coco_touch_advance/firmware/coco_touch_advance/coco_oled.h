// Minimal 128x64 I2C OLED driver for the 0.96" SSD1306 and the 1.3" SH1106.
// kibo::Canvas already keeps its pixels in the controllers' page layout, so a
// frame is sent as-is with no library needed. Pick the screen with OLED_SH1106
// in coco_config.h.
//
// SH1106 vs SSD1306:
//   - the SH1106 has 132 columns of RAM and the 128 visible ones start at
//     column 2 (the same in both mirror settings, the glass sits in the middle)
//   - it has no horizontal addressing (0x20/0x21/0x22 don't exist), so a frame
//     is written page by page: set page 0xB0+n, column 2, then 128 bytes
//   - its DC-DC converter is switched on with 0xAD,0x8B instead of the
//     SSD1306 charge pump 0x8D,0x14
//   Contrast, display on/off, segment remap and COM scan are the same.
#pragma once
#include <Arduino.h>
#include <Wire.h>

#include "coco_config.h"

class CocoOled {
 public:
  bool begin(int sda, int scl, uint8_t addr, bool rotate180, uint8_t contrast) {
    addr_ = addr;
    Wire.begin(sda, scl, 400000);
    Wire.beginTransmission(addr_);
    if (Wire.endTransmission() != 0) return false;  // nothing answering on I2C
#if OLED_SH1106
    const uint8_t init[] = {
        0xAE,                     // display off
        0xD5, 0x80,               // clock divide
        0xA8, 0x3F,               // multiplex 64
        0xD3, 0x00,               // display offset 0
        0x40,                     // start line 0
        0xAD, 0x8B,               // DC-DC on (SH1106 has no 0x8D charge pump)
        (uint8_t)(rotate180 ? 0xA0 : 0xA1),  // segment remap
        (uint8_t)(rotate180 ? 0xC0 : 0xC8),  // COM scan direction
        0xDA, 0x12,               // COM pins
        0x81, contrast,           // contrast
        0xD9, 0xF1,               // pre-charge
        0xDB, 0x40,               // VCOM deselect level
        0xA4,                     // show RAM
        0xA6,                     // normal (not inverted)
        0xAF,                     // display on
    };
#else
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
#endif
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
  // Turns the picture upside down (or back) without a restart. Same commands on both controllers.
  void flip(bool rotate180) {
    command(rotate180 ? 0xA0 : 0xA1);  // segment remap
    command(rotate180 ? 0xC0 : 0xC8);  // COM scan direction
  }

  // buf: 1024 bytes, 8 pages of 128 columns (bit 0 = top pixel of the page).
  void show(const uint8_t* buf) {
    if (!ok_) return;
#if OLED_SH1106
    // no auto-wrap to the next page: address each page, starting at column 2
    for (int page = 0; page < 8; page++) {
      Wire.beginTransmission(addr_);
      Wire.write(0x00);               // commands
      Wire.write((uint8_t)(0xB0 + page));  // page address
      Wire.write(0x02);               // column low nibble = 2
      Wire.write(0x10);               // column high nibble = 0
      Wire.endTransmission();
      const uint8_t* row = buf + page * 128;
      for (int i = 0; i < 128; i += 32) {
        Wire.beginTransmission(addr_);
        Wire.write(0x40);  // data
        Wire.write(row + i, 32);
        Wire.endTransmission();
      }
    }
#else
    const uint8_t window[] = {0x21, 0, 127, 0x22, 0, 7};
    for (uint8_t c : window) command(c);
    for (int i = 0; i < 1024; i += 32) {
      Wire.beginTransmission(addr_);
      Wire.write(0x40);  // data
      Wire.write(buf + i, 32);
      Wire.endTransmission();
    }
#endif
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
