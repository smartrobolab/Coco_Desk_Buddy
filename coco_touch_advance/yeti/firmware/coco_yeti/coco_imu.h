// Coco Yeti: MPU6050 (GY-521) accelerometer + gyro on the OLED's I2C bus.
// Also works with the MPU6500 / MPU9250 / MPU9255 (same registers for this).
//
// The sensor samples at 100 Hz into its own FIFO; Coco reads everything that
// piled up once per frame (~4 samples), so no sample is missed even though the
// screen only updates 25 times a second. Samples come out in Coco's frame
// (x right, y up, z out of the screen) using a 3x3 matrix: the default from
// coco_config.h (IMU_RIGHT / IMU_UP / IMU_OUT), or the one measured by
// Settings > Motion setup.
//
// Before deep sleep, armWakeOnMotion() puts the sensor into a low-power cycle
// mode (~70 uA) that raises INT when Coco is moved; INT wakes the ESP32-C3.
#pragma once
#include <Arduino.h>
#include <Wire.h>

class CocoImu {
 public:
  struct Sample {
    float ax, ay, az;  // g, Coco frame
    float gx, gy, gz;  // deg/s, Coco frame
    float rx, ry, rz;  // raw accel in sensor axes (for the motion setup)
  };

  bool begin() {
    for (uint8_t a : {(uint8_t)0x68, (uint8_t)0x69}) {
      Wire.beginTransmission(a);
      if (Wire.endTransmission() == 0) {
        addr_ = a;
        break;
      }
    }
    if (!addr_) return false;
    who_ = read8(0x75);
    if (who_ != 0x68 && who_ != 0x70 && who_ != 0x71 && who_ != 0x73 && who_ != 0x72) {
      Serial.printf("imu: unknown WHO_AM_I 0x%02X at 0x%02X, trying anyway\n", who_, addr_);
    }
    write8(0x6B, 0x80);  // reset
    delay(100);
    write8(0x6B, 0x01);  // wake, clock = gyro X PLL
    write8(0x6C, 0x00);  // all axes on
    write8(0x1A, 0x03);  // DLPF ~44 Hz (needed for the 1 kHz base rate)
    write8(0x19, 9);     // 1 kHz / (1 + 9) = 100 Hz
    write8(0x1B, 0x10);  // gyro +-1000 deg/s (32.8 LSB per deg/s)
    write8(0x1C, 0x10);  // accel +-8 g (4096 LSB per g): falls and knocks go well above 2 g
    if (who_ != 0x68) write8(0x1D, 0x03);  // MPU65xx: accel DLPF ~44 Hz (reserved on the 6050)
    write8(0x37, 0x30);  // INT active high, push-pull, latched, cleared by any read
    write8(0x38, 0x00);  // no interrupts while awake
    write8(0x23, 0x78);  // FIFO: accel xyz + gyro xyz = 12 bytes per sample
    resetFifo();
    ok_ = true;
    return true;
  }

  bool ok() const { return ok_; }
  uint8_t whoAmI() const { return who_; }
  const char* chipName() const {
    switch (who_) {
      case 0x68: return "MPU6050";
      case 0x70: return "MPU6500";
      case 0x71: return "MPU9250";
      case 0x73: return "MPU9255";
      default: return "MPU?";
    }
  }

  // Coco frame = M * sensor. Rows: right, up, out (unit vectors in sensor axes).
  void setMatrix(const float m[9]) { memcpy(m_, m, sizeof(m_)); }
  const float* matrix() const { return m_; }

  // Reads all samples waiting in the FIFO (at most `max`). Returns how many.
  int read(Sample* out, int max) {
    if (!ok_) return 0;
    uint8_t st = read8(0x3A);
    uint8_t c[2];
    if (!readN(0x72, c, 2)) return 0;
    int count = (c[0] << 8) | c[1];
    if ((st & 0x10) || count >= 1020) {  // overflowed (Coco was busy): start over
      resetFifo();
      return 0;
    }
    int n = count / 12;
    if (n > max) n = max;
    for (int i = 0; i < n; i++) {
      uint8_t b[12];
      if (!readN(0x74, b, 12)) return i;
      float r[6];
      for (int k = 0; k < 6; k++) r[k] = (float)(int16_t)((b[k * 2] << 8) | b[k * 2 + 1]);
      for (int k = 0; k < 3; k++) r[k] /= 4096.0f;      // g
      for (int k = 3; k < 6; k++) r[k] /= 32.8f;        // deg/s
      Sample& s = out[i];
      s.rx = r[0];
      s.ry = r[1];
      s.rz = r[2];
      s.ax = m_[0] * r[0] + m_[1] * r[1] + m_[2] * r[2];
      s.ay = m_[3] * r[0] + m_[4] * r[1] + m_[5] * r[2];
      s.az = m_[6] * r[0] + m_[7] * r[1] + m_[8] * r[2];
      s.gx = m_[0] * r[3] + m_[1] * r[4] + m_[2] * r[5];
      s.gy = m_[3] * r[3] + m_[4] * r[4] + m_[5] * r[5];
      s.gz = m_[6] * r[3] + m_[7] * r[4] + m_[8] * r[5];
    }
    return n;
  }

  // Low-power accelerometer cycling with a motion interrupt on INT.
  void armWakeOnMotion() {
    if (!ok_) return;
    write8(0x38, 0x00);
    write8(0x23, 0x00);  // FIFO off
    write8(0x6A, 0x00);
    if (who_ == 0x68) {  // MPU6050: motion detection, then cycle mode at 5 Hz
      write8(0x1C, 0x01);  // +-2 g, high-pass 5 Hz (motion detection uses the filtered data)
      write8(0x1F, 6);     // threshold ~ 6 x 2 mg... small, a nudge wakes Coco
      write8(0x20, 1);     // duration 1 ms
      write8(0x69, 0x15);  // MOT_DETECT_CTRL: accel on delay + counters
      write8(0x38, 0x40);  // MOT_EN
      delay(10);
      write8(0x6C, 0x47);  // LP wake 5 Hz, gyros in standby
      write8(0x6B, 0x28);  // CYCLE, temp sensor off
    } else {  // MPU65xx: wake-on-motion
      write8(0x6B, 0x00);
      write8(0x6C, 0x07);  // gyros off
      write8(0x1D, 0x09);  // accel DLPF 184 Hz
      write8(0x38, 0x40);  // WOM_EN
      write8(0x69, 0xC0);  // ACCEL_INTEL_EN + compare to the previous sample
      write8(0x1F, 12);    // threshold 12 x 4 mg
      write8(0x1E, 0x04);  // low-power accel output rate ~4 Hz
      write8(0x6B, 0x20);  // CYCLE
    }
    read8(0x3A);  // clear a pending interrupt
  }

  void clearInterrupt() {
    if (ok_) read8(0x3A);
  }

 private:
  void resetFifo() {
    write8(0x6A, 0x04);  // FIFO reset
    delay(1);
    write8(0x6A, 0x40);  // FIFO on
  }
  void write8(uint8_t reg, uint8_t v) {
    Wire.beginTransmission(addr_);
    Wire.write(reg);
    Wire.write(v);
    Wire.endTransmission();
  }
  uint8_t read8(uint8_t reg) {
    uint8_t v = 0;
    readN(reg, &v, 1);
    return v;
  }
  bool readN(uint8_t reg, uint8_t* buf, int n) {
    Wire.beginTransmission(addr_);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)addr_, n) != n) return false;
    for (int i = 0; i < n; i++) buf[i] = Wire.read();
    return true;
  }

  uint8_t addr_ = 0, who_ = 0;
  bool ok_ = false;
  float m_[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
};
