#include "kibo_behavior.h"

#include <esp_log.h>
#include <esp_sleep.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cmath>
#include <cstdio>
#include <string_view>

#include "application.h"
#include "kibo_time.h"
#include "device_state.h"
#include "kibo_sounds.h"
#include "mcp_server.h"

static const char* const kTagBehavior = "KiboBehavior";

namespace kibo {

namespace {

constexpr uint32_t kLongPressMs = 800;     // -> Love face + purr
constexpr uint32_t kHoldNapMs = 5000;      // release after 5 s -> deep sleep ("nap")
constexpr uint32_t kHoldWifiMs = 10000;    // release after 10 s -> Wi-Fi setup mode
constexpr uint32_t kTapFaceMs = 6000;      // how long a tapped face stays

int PercentFromVolts(float v) {
  // Resting single-cell Li-ion curve (rough - not a fuel gauge).
  static const float kV[] = {3.30f, 3.50f, 3.60f, 3.70f, 3.80f, 3.90f, 4.00f, 4.10f, 4.20f};
  static const int kP[] = {0, 5, 15, 30, 50, 65, 80, 90, 100};
  if (v <= kV[0]) return 0;
  for (int i = 1; i < 9; i++) {
    if (v < kV[i]) return kP[i - 1] + (int)((v - kV[i - 1]) / (kV[i] - kV[i - 1]) * (kP[i] - kP[i - 1]));
  }
  return 100;
}

template <size_t N>
std::string_view Clip(const uint8_t (&data)[N]) {
  return std::string_view(reinterpret_cast<const char*>(data), N);
}

}  // namespace

KiboBehavior::KiboBehavior(KiboDisplay* display, i2c_master_bus_handle_t bus, const KiboPins& pins,
                           KiboCallbacks callbacks)
    : display_(display), bus_(bus), pins_(pins), callbacks_(std::move(callbacks)) {}

void KiboBehavior::Start() {
  if (pins_.amp_sd != GPIO_NUM_NC) {
    gpio_hold_dis(pins_.amp_sd);
    gpio_set_direction(pins_.amp_sd, GPIO_MODE_OUTPUT);
    gpio_set_level(pins_.amp_sd, 1);  // amplifier on
  }
  InitTouch();
  InitImu();
  InitBattery();
  InitTools();
  display_->SetActivityCallback([this](Activity a) {
    switch (a) {
      case Activity::Eating: PlaySfx(Sfx::Nom); break;
      case Activity::Sing: PlaySfx(Sfx::Sing); break;
      case Activity::Music: PlaySfx(Sfx::Happy); break;
      case Activity::Yawn: PlaySfx(Sfx::Yawn); break;
      case Activity::Playing: PlaySfx(Sfx::Giggle); break;
      default: break;
    }
  });
  xTaskCreate(TaskEntry, "kibo_body", 4096, this, 3, nullptr);
}

bool KiboBehavior::IsIdle() const {
  return Ready() && Application::GetInstance().GetDeviceState() == kDeviceStateIdle;
}

bool KiboBehavior::Ready() const { return display_->IsSetupUICalled(); }

// ------------------------------------------------------------------ sounds

void KiboBehavior::PlaySfx(Sfx s) {
  if (!IsIdle()) return;  // never talk over the conversation / mic
  auto& app = Application::GetInstance();
  switch (s) {
    case Sfx::Giggle: app.PlaySound(Clip(kSfx_giggle)); break;
    case Sfx::Happy: app.PlaySound(Clip(kSfx_happy)); break;
    case Sfx::Purr: app.PlaySound(Clip(kSfx_purr)); break;
    case Sfx::Sad: app.PlaySound(Clip(kSfx_sad)); break;
    case Sfx::Angry: app.PlaySound(Clip(kSfx_angry)); break;
    case Sfx::Surprised: app.PlaySound(Clip(kSfx_surprised)); break;
    case Sfx::Dizzy: app.PlaySound(Clip(kSfx_dizzy)); break;
    case Sfx::Yawn: app.PlaySound(Clip(kSfx_yawn)); break;
    case Sfx::Nom: app.PlaySound(Clip(kSfx_nom)); break;
    case Sfx::Kiss: app.PlaySound(Clip(kSfx_kiss)); break;
    case Sfx::Boop: app.PlaySound(Clip(kSfx_boop)); break;
    case Sfx::Sing: app.PlaySound(Clip(kSfx_sing)); break;
  }
}

void KiboBehavior::PlaySfxForFace(Face f) {
  switch (f) {
    case Face::Sad:
    case Face::Crying: PlaySfx(Sfx::Sad); break;
    case Face::Angry:
    case Face::Fire: PlaySfx(Sfx::Angry); break;
    case Face::Kissou: PlaySfx(Sfx::Kiss); break;
    case Face::MiniLove:
    case Face::AnimeLove:
    case Face::Love: PlaySfx(Sfx::Purr); break;
    case Face::Sleepy: PlaySfx(Sfx::Yawn); break;
    case Face::Serious: PlaySfx(Sfx::Boop); break;
    default: PlaySfx(Sfx::Giggle); break;
  }
}

// ------------------------------------------------------------------ touch

void KiboBehavior::InitTouch() {
  if (pins_.touch == GPIO_NUM_NC) return;
  touch_ = std::make_unique<Button>(pins_.touch, true /* TTP223 is active HIGH */, kLongPressMs, 0, false);
  touch_->OnPressDown([this]() {
    press_down_at_ = NowMs();
    pressed_ = true;
    display_->NoteInteraction();
  });
  touch_->OnPressUp([this]() { OnRelease(); });
  touch_->OnClick([this]() { OnTap(); });
  touch_->OnDoubleClick([this]() { OnDoubleTap(); });
  touch_->OnLongPress([this]() { OnLongPress(); });
}

void KiboBehavior::OnTap() {
  if (!Ready()) return;
  auto& app = Application::GetInstance();
  auto state = app.GetDeviceState();
  if (state == kDeviceStateSpeaking || state == kDeviceStateListening) {
    app.ToggleChatState();  // tap while talking = stop / interrupt
    return;
  }
  if (state != kDeviceStateIdle) return;

  // Annoyance meter: poke Kibo too often and it gets angry, then cries.
  uint32_t now = NowMs();
  pokes_++;
  if (pokes_ >= 10) {
    display_->React(Face::Crying, 5000);
    PlaySfx(Sfx::Sad);
    pokes_ = 4;
    return;
  }
  if (pokes_ >= 6) {
    display_->React(Face::Angry, 3500);
    PlaySfx(Sfx::Angry);
    return;
  }
  (void)now;
  tap_index_ = (tap_index_ + 1) % kTapFaceCount;
  Face f = kTapFaces[tap_index_];
  display_->React(f, kTapFaceMs);
  PlaySfxForFace(f);
}

void KiboBehavior::OnDoubleTap() {
  if (!Ready()) return;
  // Double tap = talk to Kibo (same as saying the wake word).
  auto& app = Application::GetInstance();
  auto state = app.GetDeviceState();
  if (state == kDeviceStateIdle || state == kDeviceStateListening || state == kDeviceStateSpeaking) {
    app.ToggleChatState();
  }
}

void KiboBehavior::OnLongPress() {
  long_press_active_ = true;
  if (IsIdle()) {
    display_->React(Face::Love, 4000);  // being petted
    PlaySfx(Sfx::Purr);
  }
}

void KiboBehavior::OnRelease() {
  uint32_t held = NowMs() - press_down_at_.load();
  pressed_ = false;
  display_->SetHoldMs(0);
  if (!long_press_active_.exchange(false)) return;
  if (held >= kHoldWifiMs) {
    ESP_LOGI(kTagBehavior, "Hold %lu ms -> Wi-Fi setup", (unsigned long)held);
    if (callbacks_.enter_wifi_config) callbacks_.enter_wifi_config();
  } else if (held >= kHoldNapMs) {
    ESP_LOGI(kTagBehavior, "Hold %lu ms -> deep sleep", (unsigned long)held);
    Application::GetInstance().Schedule([this]() { EnterDeepSleep(); });
  }
}

// ------------------------------------------------------------------ IMU (MPU6050)

bool KiboBehavior::ImuWrite(uint8_t reg, uint8_t value) {
  uint8_t b[2] = {reg, value};
  return imu_ && i2c_master_transmit(imu_, b, 2, 50) == ESP_OK;
}

bool KiboBehavior::ImuRead(float g[3]) {
  uint8_t reg = 0x3B, b[6];
  if (!imu_ || i2c_master_transmit_receive(imu_, &reg, 1, b, 6, 50) != ESP_OK) return false;
  float raw[3];
  for (int i = 0; i < 3; i++) raw[i] = (float)(int16_t)((b[2 * i] << 8) | b[2 * i + 1]) / 8192.0f;  // +-4 g
  g[0] = raw[pins_.axis_right] * pins_.sign_right;
  g[1] = raw[pins_.axis_up] * pins_.sign_up;
  g[2] = raw[pins_.axis_out] * pins_.sign_out;
  return true;
}

void KiboBehavior::InitImu() {
  if (!bus_ || i2c_master_probe(bus_, pins_.imu_addr, 100) != ESP_OK) {
    ESP_LOGW(kTagBehavior, "MPU6050 not found at 0x%02X - motion reactions disabled", pins_.imu_addr);
    return;
  }
  i2c_device_config_t cfg = {};
  cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  cfg.device_address = pins_.imu_addr;
  cfg.scl_speed_hz = 400000;
  if (i2c_master_bus_add_device(bus_, &cfg, &imu_) != ESP_OK) {
    imu_ = nullptr;
    return;
  }
  uint8_t reg = 0x75, who = 0;
  i2c_master_transmit_receive(imu_, &reg, 1, &who, 1, 50);
  ESP_LOGI(kTagBehavior, "MPU6050 WHO_AM_I=0x%02X", who);  // 0x68 genuine; clones report 0x70/0x72/0x98
  ImuWrite(0x6B, 0x00);  // wake up
  vTaskDelay(pdMS_TO_TICKS(20));
  ImuWrite(0x1A, 0x03);  // DLPF ~44 Hz
  ImuWrite(0x1B, 0x08);  // gyro +-500 dps
  ImuWrite(0x1C, 0x08);  // accel +-4 g
}

void KiboBehavior::UpdateMotion(uint32_t now) {
  float a[3];
  if (!ImuRead(a)) return;
  for (int i = 0; i < 3; i++) grav_[i] += (a[i] - grav_[i]) * 0.15f;
  float mag = sqrtf(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
  float dev = fabsf(mag - 1.0f);

  // Eyes follow the tilt.
  display_->SetGaze((int)(grav_[0] * 14.0f), (int)(-grav_[2] * 10.0f));

  bool idle = IsIdle();
  // Shake: several strong peaks within 1.5 s.
  if (dev > 0.9f && now - last_peak_ > 100) {
    if (now - shake_window_start_ > 1500) {
      shake_window_start_ = now;
      shake_peaks_ = 0;
    }
    shake_peaks_++;
    last_peak_ = now;
  }
  if (shake_peaks_ >= 4 && now > cooldown_until_) {
    shake_peaks_ = 0;
    shaking_ = true;
    cooldown_until_ = now + 3000;
    display_->NoteInteraction();
    if (idle) {
      display_->React(Face::Dizzy, 3000);
      PlaySfx(Sfx::Dizzy);
    }
    pokes_ += 2;
  } else if (now > cooldown_until_) {
    shaking_ = false;
  }

  // Upside down for 1 s -> cry until turned back.
  if (grav_[1] < -0.55f) {
    if (!upside_since_) upside_since_ = now;
    if (!upside_down_ && now - upside_since_ > 1000) {
      upside_down_ = true;
      display_->NoteInteraction();
      if (idle) {
        display_->React(Face::Crying, 60000);
        PlaySfx(Sfx::Sad);
      }
    }
  } else {
    upside_since_ = 0;
    if (upside_down_ && grav_[1] > 0.5f) {
      upside_down_ = false;
      if (idle) {
        display_->React(Face::Happy, 2000);
        PlaySfx(Sfx::Happy);
      }
    }
  }

  // Bump / picked up after being still for a while -> surprised.
  if (dev < 0.08f) {
    if (!still_since_) still_since_ = now;
  } else if (dev > 0.35f && still_since_ && now - still_since_ > 2000 && now > cooldown_until_) {
    still_since_ = 0;
    cooldown_until_ = now + 2000;
    display_->NoteInteraction();
    if (idle) {
      display_->React(Face::Surprised, 1200);
      PlaySfx(Sfx::Surprised);
    }
  } else if (dev > 0.15f) {
    still_since_ = 0;
  }
}

// ------------------------------------------------------------------ battery

void KiboBehavior::InitBattery() {
  if (pins_.battery_adc_channel < 0) return;
  adc_oneshot_unit_init_cfg_t ucfg = {};
  ucfg.unit_id = ADC_UNIT_1;
  if (adc_oneshot_new_unit(&ucfg, &adc_) != ESP_OK) {
    adc_ = nullptr;
    return;
  }
  adc_oneshot_chan_cfg_t ccfg = {};
  ccfg.atten = ADC_ATTEN_DB_12;
  ccfg.bitwidth = ADC_BITWIDTH_DEFAULT;
  adc_oneshot_config_channel(adc_, (adc_channel_t)pins_.battery_adc_channel, &ccfg);
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
  adc_cali_curve_fitting_config_t cal = {};
  cal.unit_id = ADC_UNIT_1;
  cal.chan = (adc_channel_t)pins_.battery_adc_channel;
  cal.atten = ADC_ATTEN_DB_12;
  cal.bitwidth = ADC_BITWIDTH_DEFAULT;
  if (adc_cali_create_scheme_curve_fitting(&cal, &cali_) != ESP_OK) cali_ = nullptr;
#endif
}

void KiboBehavior::UpdateBattery(uint32_t now) {
  if (!adc_ || now < next_battery_at_) return;
  next_battery_at_ = now + 10000;
  int sum = 0, n = 0;
  for (int i = 0; i < 16; i++) {
    int raw = 0, mv = 0;
    if (adc_oneshot_read(adc_, (adc_channel_t)pins_.battery_adc_channel, &raw) != ESP_OK) continue;
    if (cali_ && adc_cali_raw_to_voltage(cali_, raw, &mv) == ESP_OK) sum += mv;
    else sum += raw * 3100 / 4095;
    n++;
  }
  if (!n) return;
  float vadc = (float)sum / n / 1000.0f;
  float vbat = vadc * (pins_.divider_top_ohm + pins_.divider_bottom_ohm) / pins_.divider_bottom_ohm;
  battery_mv_ = (int)(vbat * 1000);
  bool present = vbat > 2.5f;  // below this: USB bench power without a cell
  int pct = present ? PercentFromVolts(vbat) : -1;
  battery_percent_ = pct;
  display_->SetBattery(pct < 0 ? 100 : pct, present);

  // Protect the cell: sustained < 3.25 V -> sleep (TP4056 protection cuts much lower).
  if (present && vbat < 3.25f) {
    if (++critical_count_ >= 3) {
      ESP_LOGW(kTagBehavior, "Battery critical (%.2f V) - going to sleep", vbat);
      display_->React(Face::LowBattery, 4000, true);
      vTaskDelay(pdMS_TO_TICKS(4000));
      Application::GetInstance().Schedule([this]() { EnterDeepSleep(); });
    }
  } else {
    critical_count_ = 0;
  }
}

bool KiboBehavior::GetBattery(int& percent, bool& present) const {
  int p = battery_percent_.load();
  present = p >= 0;
  percent = present ? p : 0;
  return adc_ != nullptr && present;
}

// ------------------------------------------------------------------ task

void KiboBehavior::TaskEntry(void* arg) { static_cast<KiboBehavior*>(arg)->Task(); }

void KiboBehavior::Task() {
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    uint32_t now = NowMs();
    if (!Ready()) {
      vTaskDelayUntil(&last, pdMS_TO_TICKS(100));
      continue;
    }
    if (pressed_) {
      uint32_t held = now - press_down_at_.load();
      display_->SetHoldMs(long_press_active_ ? held : 0);
    }
    if (now - last_poke_decay_ > 1500) {  // annoyance cools down
      last_poke_decay_ = now;
      if (pokes_ > 0) pokes_--;
    }
    if (imu_) UpdateMotion(now);
    UpdateBattery(now);
    vTaskDelayUntil(&last, pdMS_TO_TICKS(20));  // 50 Hz
  }
}

// ------------------------------------------------------------------ deep sleep

void KiboBehavior::EnterDeepSleep() {
  ESP_LOGI(kTagBehavior, "Entering deep sleep - touch to wake");
  display_->PowerOff();
  ImuWrite(0x6B, 0x40);  // MPU6050 sleep
  if (pins_.amp_sd != GPIO_NUM_NC) {
    gpio_set_level(pins_.amp_sd, 0);  // amplifier off
    gpio_hold_en(pins_.amp_sd);
    gpio_deep_sleep_hold_en();
  }
  esp_wifi_stop();
  // Wait for the finger to leave the pad, otherwise we would wake immediately.
  for (int i = 0; i < 300 && gpio_get_level(pins_.touch); i++) vTaskDelay(pdMS_TO_TICKS(10));
  vTaskDelay(pdMS_TO_TICKS(200));
#if SOC_GPIO_SUPPORT_DEEPSLEEP_WAKEUP  // ESP32-C3
  esp_sleep_enable_gpio_wakeup_on_hp_periph_powerdown(1ULL << pins_.touch, ESP_GPIO_WAKEUP_GPIO_HIGH);
#else  // ESP32-S3
  esp_sleep_enable_ext1_wakeup_io(1ULL << pins_.touch, ESP_EXT1_WAKEUP_ANY_HIGH);
#endif
  esp_deep_sleep_start();
}

// ------------------------------------------------------------------ MCP tools

void KiboBehavior::InitTools() {
  auto& mcp = McpServer::GetInstance();
  mcp.AddTool("self.coco.show_face",
              "Show one of Coco's facial expressions on its OLED screen for a few seconds. Use it when the user "
              "asks to see a face, or to act out a feeling. face: one of neutral, serious, sleepy, crying, sad, cute, "
              "kissou, mini_love, anime_love, fire, angry, love, happy, surprised, dizzy, thinking, singing.",
              PropertyList({Property("face", kPropertyTypeString)}),
              [this](const PropertyList& props) -> ToolResult {
                std::string name = props["face"].value<std::string>();
                Face f;
                if (!faceFromName(name.c_str(), f)) return std::unexpected("unknown face: " + name);
                display_->React(f, 5000, true);
                return true;
              });
  mcp.AddTool("self.coco.do_activity",
              "Make Coco do a little activity on its screen after this reply: eat (a cookie), play (with a ball), "
              "music (dance), sing (sing a la-la song into a microphone), yawn, clock (show the time), or sleep "
              "(take a nap).",
              PropertyList({Property("activity", kPropertyTypeString)}),
              [this](const PropertyList& props) -> ToolResult {
                std::string name = props["activity"].value<std::string>();
                Activity a;
                if (!activityFromName(name.c_str(), a)) return std::unexpected("unknown activity: " + name);
                display_->StartActivity(a);
                return true;
              });
  mcp.AddTool("self.coco.get_body_state",
              "Get Coco's physical state: battery level, and whether it is being held upside down or shaken.",
              PropertyList(), [this](const PropertyList&) -> ToolResult {
                int pct = battery_percent_.load();
                char json[160];
                snprintf(json, sizeof(json),
                         "{\"battery_percent\":%d,\"battery_mv\":%d,\"upside_down\":%s,\"being_shaken\":%s,"
                         "\"motion_sensor\":%s}",
                         pct, battery_mv_.load(), upside_down_ ? "true" : "false", shaking_ ? "true" : "false",
                         imu_ ? "true" : "false");
                return std::string(json);
              });
}

}  // namespace kibo
