// KiboBehavior - Kibo's body: touch gestures, MPU6050 motion reactions,
// battery measurement, little sound effects, deep sleep and the MCP tools
// that let the AI (xiaozhi.me agent) change Kibo's face.
#pragma once

#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_oneshot.h>

#include <atomic>
#include <functional>
#include <memory>

#include "button.h"
#include "kibo_display.h"

namespace kibo {

struct KiboPins {
  gpio_num_t touch = GPIO_NUM_NC;  // TTP223 OUT (active HIGH); also deep-sleep wake
  gpio_num_t amp_sd = GPIO_NUM_NC; // optional MAX98357A SD pin (HIGH = on)
  int battery_adc_channel = -1;    // ADC1 channel of the battery divider, -1 = none
  float divider_top_ohm = 1000000.0f;
  float divider_bottom_ohm = 330000.0f;
  uint8_t imu_addr = 0x68;
  // How the MPU6050 is mounted: which accelerometer axis (0=X,1=Y,2=Z)
  // points to Kibo's right, up, and out of the screen; sign flips with -1.
  int axis_right = 0, axis_up = 2, axis_out = 1;
  int sign_right = 1, sign_up = 1, sign_out = 1;
};

struct KiboCallbacks {
  std::function<void()> enter_wifi_config;  // board: WifiBoard::EnterWifiConfigMode
};

class KiboBehavior {
 public:
  KiboBehavior(KiboDisplay* display, i2c_master_bus_handle_t bus, const KiboPins& pins, KiboCallbacks callbacks);
  void Start();
  bool GetBattery(int& percent, bool& present) const;
  void EnterDeepSleep();

 private:
  enum class Sfx { Giggle, Happy, Purr, Sad, Angry, Surprised, Dizzy, Yawn, Nom, Kiss, Boop, Sing };

  void InitTouch();
  void InitImu();
  void InitBattery();
  void InitTools();
  static void TaskEntry(void* arg);
  void Task();
  void UpdateMotion(uint32_t now);
  void UpdateBattery(uint32_t now);

  void OnTap();
  void OnDoubleTap();
  void OnLongPress();
  void OnRelease();
  void PlaySfx(Sfx s);
  void PlaySfxForFace(Face f);
  bool IsIdle() const;
  bool Ready() const;  // XiaoZhi app initialised

  bool ImuRead(float g[3]);
  bool ImuWrite(uint8_t reg, uint8_t value);

  KiboDisplay* display_;
  i2c_master_bus_handle_t bus_;
  KiboPins pins_;
  KiboCallbacks callbacks_;
  std::unique_ptr<Button> touch_;

  // touch state
  std::atomic<uint32_t> press_down_at_{0};
  std::atomic<bool> pressed_{false};
  std::atomic<bool> long_press_active_{false};
  int tap_index_ = 0;
  int pokes_ = 0;               // annoyance meter for rapid tapping
  uint32_t last_poke_decay_ = 0;

  // IMU
  i2c_master_dev_handle_t imu_ = nullptr;
  float grav_[3] = {0, 1, 0};   // low-passed right/up/out in g
  uint32_t still_since_ = 0;
  uint32_t shake_window_start_ = 0;
  int shake_peaks_ = 0;
  uint32_t last_peak_ = 0;
  uint32_t cooldown_until_ = 0;
  uint32_t upside_since_ = 0;
  bool upside_down_ = false;
  std::atomic<bool> shaking_{false};

  // battery
  adc_oneshot_unit_handle_t adc_ = nullptr;
  adc_cali_handle_t cali_ = nullptr;
  std::atomic<int> battery_percent_{-1};
  std::atomic<int> battery_mv_{0};
  uint32_t next_battery_at_ = 0;
  int critical_count_ = 0;
};

}  // namespace kibo
