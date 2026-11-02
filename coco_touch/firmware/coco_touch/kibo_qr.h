// Minimal QR Code encoder for the Wi-Fi setup screen.
// Byte mode, error-correction level L, versions 1..6 (up to 134 bytes).
// No ESP-IDF dependency; verified on PC against zxing + the Python `qrcode`
// reference implementation (see sim/test_qr.py).
#pragma once
#include <stdint.h>

namespace kibo {
namespace qr {

constexpr int kMaxVersion = 6;
constexpr int kMaxSize = 17 + 4 * kMaxVersion;  // 41 modules

struct Code {
  int size = 0;                         // modules per side, 0 = encoding failed
  uint8_t modules[kMaxSize][kMaxSize];  // [y][x], 1 = dark
  bool dark(int x, int y) const { return modules[y][x] != 0; }
};

// Encodes `text` into `out`. Returns false if the text is too long.
// Not re-entrant (uses static scratch buffers to keep stacks small).
bool encode(const char* text, Code& out);

}  // namespace qr
}  // namespace kibo
