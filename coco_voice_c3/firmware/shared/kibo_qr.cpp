// QR encoder - follows ISO/IEC 18004 (structure after Project Nayuki's reference
// implementation), trimmed to byte mode / ECC L / versions 1..6, which have a
// single alignment pattern and no version-information blocks.
#include "kibo_qr.h"

#include <string.h>

#include <new>

namespace kibo {
namespace qr {
namespace {

struct VersionInfo {
  uint8_t dataCodewords;  // total data codewords (all blocks)
  uint8_t ecPerBlock;
  uint8_t blocks;
  uint8_t alignPos;  // centre of the bottom-right alignment pattern (0 = none)
};

// ECC level L, versions 1..6
const VersionInfo kVersions[kMaxVersion + 1] = {
    {0, 0, 0, 0},    {19, 7, 1, 0},     {34, 10, 1, 18}, {55, 15, 1, 22},
    {80, 20, 1, 26}, {108, 26, 1, 30}, {136, 18, 2, 34},
};

uint8_t gfMul(uint8_t x, uint8_t y) {
  int z = 0;
  for (int i = 7; i >= 0; i--) {
    z = (z << 1) ^ ((z >> 7) * 0x11D);
    z ^= ((y >> i) & 1) * x;
  }
  return (uint8_t)z;
}

void rsDivisor(int degree, uint8_t* out) {
  memset(out, 0, degree);
  out[degree - 1] = 1;
  uint8_t root = 1;
  for (int i = 0; i < degree; i++) {
    for (int j = 0; j < degree; j++) {
      out[j] = gfMul(out[j], root);
      if (j + 1 < degree) out[j] ^= out[j + 1];
    }
    root = gfMul(root, 0x02);
  }
}

void rsRemainder(const uint8_t* data, int len, const uint8_t* div, int degree, uint8_t* out) {
  memset(out, 0, degree);
  for (int i = 0; i < len; i++) {
    uint8_t factor = data[i] ^ out[0];
    memmove(out, out + 1, degree - 1);
    out[degree - 1] = 0;
    for (int j = 0; j < degree; j++) out[j] ^= gfMul(div[j], factor);
  }
}

class Builder {
 public:
  Builder(Code& c, int version) : c_(c), v_(kVersions[version]) {
    size_ = 17 + 4 * version;
    c_.size = size_;
    memset(c_.modules, 0, sizeof(c_.modules));
    memset(func_, 0, sizeof(func_));
  }

  void drawFunctionPatterns() {
    for (int i = 0; i < size_; i++) {
      setFunc(6, i, i % 2 == 0);
      setFunc(i, 6, i % 2 == 0);
    }
    drawFinder(3, 3);
    drawFinder(size_ - 4, 3);
    drawFinder(3, size_ - 4);
    if (v_.alignPos) drawAlignment(v_.alignPos, v_.alignPos);
    drawFormatBits(0);  // reserve area; real bits are drawn after masking
  }

  void drawCodewords(const uint8_t* data, int len) {
    int i = 0;
    for (int right = size_ - 1; right >= 1; right -= 2) {
      if (right == 6) right = 5;
      for (int vert = 0; vert < size_; vert++) {
        for (int j = 0; j < 2; j++) {
          int x = right - j;
          bool upward = ((right + 1) & 2) == 0;
          int y = upward ? size_ - 1 - vert : vert;
          if (!func_[y][x] && i < len * 8) {
            c_.modules[y][x] = (data[i >> 3] >> (7 - (i & 7))) & 1;
            i++;
          }
        }
      }
    }
  }

  void applyMask(int mask) {
    for (int y = 0; y < size_; y++) {
      for (int x = 0; x < size_; x++) {
        if (func_[y][x]) continue;
        bool invert;
        switch (mask) {
          case 0: invert = (x + y) % 2 == 0; break;
          case 1: invert = y % 2 == 0; break;
          case 2: invert = x % 3 == 0; break;
          case 3: invert = (x + y) % 3 == 0; break;
          case 4: invert = (x / 3 + y / 2) % 2 == 0; break;
          case 5: invert = x * y % 2 + x * y % 3 == 0; break;
          case 6: invert = (x * y % 2 + x * y % 3) % 2 == 0; break;
          default: invert = ((x + y) % 2 + x * y % 3) % 2 == 0; break;
        }
        if (invert) c_.modules[y][x] ^= 1;
      }
    }
  }

  void drawFormatBits(int mask) {
    int data = (1 << 3) | mask;  // ECC L format bits = 01
    int rem = data;
    for (int i = 0; i < 10; i++) rem = (rem << 1) ^ ((rem >> 9) * 0x537);
    int bits = ((data << 10) | rem) ^ 0x5412;
    auto bit = [&](int i) { return ((bits >> i) & 1) != 0; };

    for (int i = 0; i <= 5; i++) setFunc(8, i, bit(i));
    setFunc(8, 7, bit(6));
    setFunc(8, 8, bit(7));
    setFunc(7, 8, bit(8));
    for (int i = 9; i < 15; i++) setFunc(14 - i, 8, bit(i));

    for (int i = 0; i < 8; i++) setFunc(size_ - 1 - i, 8, bit(i));
    for (int i = 8; i < 15; i++) setFunc(8, size_ - 15 + i, bit(i));
    setFunc(8, size_ - 8, true);  // dark module
  }

  long penalty() const {
    long result = 0;
    // Rule 1: runs of 5+ same-colour modules in rows and columns
    for (int pass = 0; pass < 2; pass++) {
      for (int a = 0; a < size_; a++) {
        int run = 0, prev = -1;
        for (int b = 0; b < size_; b++) {
          int m = pass == 0 ? c_.modules[a][b] : c_.modules[b][a];
          if (m == prev) {
            run++;
            if (run == 5) result += 3;
            else if (run > 5) result++;
          } else {
            prev = m;
            run = 1;
          }
        }
      }
    }
    // Rule 2: 2x2 blocks of one colour
    for (int y = 0; y < size_ - 1; y++)
      for (int x = 0; x < size_ - 1; x++) {
        int m = c_.modules[y][x];
        if (m == c_.modules[y][x + 1] && m == c_.modules[y + 1][x] && m == c_.modules[y + 1][x + 1]) result += 3;
      }
    // Rule 3: finder-like 1:1:3:1:1 patterns next to 4 light modules
    static const uint8_t p1[11] = {1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0};
    static const uint8_t p2[11] = {0, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1};
    for (int pass = 0; pass < 2; pass++)
      for (int a = 0; a < size_; a++)
        for (int b = 0; b + 11 <= size_; b++) {
          bool m1 = true, m2 = true;
          for (int k = 0; k < 11; k++) {
            int m = pass == 0 ? c_.modules[a][b + k] : c_.modules[b + k][a];
            if (m != p1[k]) m1 = false;
            if (m != p2[k]) m2 = false;
          }
          if (m1) result += 40;
          if (m2) result += 40;
        }
    // Rule 4: dark/light balance
    long dark = 0, total = (long)size_ * size_;
    for (int y = 0; y < size_; y++)
      for (int x = 0; x < size_; x++) dark += c_.modules[y][x];
    long diff = dark * 20 - total * 10;
    if (diff < 0) diff = -diff;
    result += ((diff + total - 1) / total - 1) * 10;
    return result;
  }

 private:
  void setFunc(int x, int y, bool dark) {
    c_.modules[y][x] = dark ? 1 : 0;
    func_[y][x] = 1;
  }
  void drawFinder(int cx, int cy) {
    for (int dy = -4; dy <= 4; dy++)
      for (int dx = -4; dx <= 4; dx++) {
        int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
        int dist = ax > ay ? ax : ay;
        int x = cx + dx, y = cy + dy;
        if (x >= 0 && x < size_ && y >= 0 && y < size_) setFunc(x, y, dist != 2 && dist != 4);
      }
  }
  void drawAlignment(int cx, int cy) {
    for (int dy = -2; dy <= 2; dy++)
      for (int dx = -2; dx <= 2; dx++) {
        int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
        setFunc(cx + dx, cy + dy, (ax > ay ? ax : ay) != 1);
      }
  }

  Code& c_;
  const VersionInfo& v_;
  int size_;
  uint8_t func_[kMaxSize][kMaxSize];
};

}  // namespace

bool encode(const char* text, Code& out) {
  out.size = 0;
  int len = (int)strlen(text);
  int version = 0;
  for (int v = 1; v <= kMaxVersion; v++) {
    if (4 + 8 + 8 * len <= kVersions[v].dataCodewords * 8) {
      version = v;
      break;
    }
  }
  if (!version) return false;
  const VersionInfo& vi = kVersions[version];

  // --- data bit stream ---
  static uint8_t data[136];
  memset(data, 0, sizeof(data));
  int bitLen = 0;
  auto append = [&](uint32_t val, int n) {
    for (int i = n - 1; i >= 0; i--, bitLen++)
      if ((val >> i) & 1) data[bitLen >> 3] |= 0x80 >> (bitLen & 7);
  };
  int capBits = vi.dataCodewords * 8;
  append(0x4, 4);  // byte mode
  append((uint32_t)len, 8);
  for (int i = 0; i < len; i++) append((uint8_t)text[i], 8);
  append(0, capBits - bitLen < 4 ? capBits - bitLen : 4);  // terminator
  append(0, (8 - bitLen % 8) % 8);
  for (uint8_t pad = 0xEC; bitLen < capBits; pad ^= 0xEC ^ 0x11) append(pad, 8);

  // --- error correction + interleaving (equal-length blocks for L, v1..6) ---
  static uint8_t ecc[2][26];
  static uint8_t all[172];
  uint8_t div[26];
  rsDivisor(vi.ecPerBlock, div);
  int blockLen = vi.dataCodewords / vi.blocks;
  for (int b = 0; b < vi.blocks; b++) rsRemainder(data + b * blockLen, blockLen, div, vi.ecPerBlock, ecc[b]);
  int n = 0;
  for (int i = 0; i < blockLen; i++)
    for (int b = 0; b < vi.blocks; b++) all[n++] = data[b * blockLen + i];
  for (int i = 0; i < vi.ecPerBlock; i++)
    for (int b = 0; b < vi.blocks; b++) all[n++] = ecc[b][i];

  // --- modules --- (static storage keeps ~1.7 KB off the caller's stack)
  alignas(Builder) static uint8_t storage[sizeof(Builder)];
  Builder& bld = *new (storage) Builder(out, version);
  bld.drawFunctionPatterns();
  bld.drawCodewords(all, n);

  int bestMask = 0;
  long bestPenalty = -1;
  for (int m = 0; m < 8; m++) {
    bld.applyMask(m);
    bld.drawFormatBits(m);
    long p = bld.penalty();
    if (bestPenalty < 0 || p < bestPenalty) {
      bestPenalty = p;
      bestMask = m;
    }
    bld.applyMask(m);  // XOR again = undo
  }
  bld.applyMask(bestMask);
  bld.drawFormatBits(bestMask);
  return true;
}

}  // namespace qr
}  // namespace kibo
