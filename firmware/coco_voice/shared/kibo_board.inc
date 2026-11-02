// Shared XiaoZhi board class for Kibo (ESP32-C3 and ESP32-S3 variants).
//
// XiaoZhi compiles only the *.cc files directly inside a board directory, so
// each variant (kibo/c3, kibo/s3) has one small .cc that includes its own
// config.h and then this file (a "unity build" of the shared Kibo sources).
// Expects these macros from config.h: AUDIO_*, DISPLAY_*, KIBO_*, BOOT_BUTTON_GPIO.

#include <driver/i2c_master.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>

#include "application.h"
#include "assets/lang_config.h"
#include "button.h"
#include "codecs/no_audio_codec.h"
#include "wifi_board.h"

#if KIBO_OLED_SH1106
#include <esp_lcd_panel_sh1106.h>
#endif

#if KIBO_LED_IS_WS2812
#include "led/single_led.h"
#else
#include "led/gpio_led.h"
#endif

// ---- shared Kibo sources (see comment above) ----
#include "kibo_canvas.cpp"
#include "kibo_qr.cpp"
#include "kibo_faces.cpp"
#include "kibo_screens.cpp"
#include "kibo_display.cpp"
#include "kibo_behavior.cpp"

class KiboBoard : public WifiBoard {
 private:
  i2c_master_bus_handle_t i2c_bus_ = nullptr;
  esp_lcd_panel_io_handle_t panel_io_ = nullptr;
  esp_lcd_panel_handle_t panel_ = nullptr;
  kibo::KiboDisplay* display_ = nullptr;
  kibo::KiboBehavior* behavior_ = nullptr;
  Button boot_button_;

  void InitializeI2c() {
    i2c_master_bus_config_t cfg = {};
    cfg.i2c_port = I2C_NUM_0;
    cfg.sda_io_num = KIBO_I2C_SDA_PIN;
    cfg.scl_io_num = KIBO_I2C_SCL_PIN;
    cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    cfg.glitch_ignore_cnt = 7;
    cfg.flags.enable_internal_pullup = 1;  // modules have 4.7k/10k pull-ups too
    ESP_ERROR_CHECK(i2c_new_master_bus(&cfg, &i2c_bus_));
  }

  void InitializeDisplay() {
    esp_lcd_panel_io_i2c_config_t io_config = {};
    io_config.dev_addr = DISPLAY_I2C_ADDR;
    io_config.scl_speed_hz = 400 * 1000;
    io_config.control_phase_bytes = 1;
    io_config.dc_bit_offset = 6;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus_, &io_config, &panel_io_));

    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = GPIO_NUM_NC;
    panel_config.bits_per_pixel = 1;
#if KIBO_OLED_SH1106
    esp_lcd_panel_sh1106_config_t vendor = {};
    panel_config.vendor_config = &vendor;
    esp_err_t err = esp_lcd_new_panel_sh1106(panel_io_, &panel_config, &panel_);
#else
    esp_lcd_panel_ssd1306_config_t vendor = {};
    vendor.height = DISPLAY_HEIGHT;
    panel_config.vendor_config = &vendor;
    esp_err_t err = esp_lcd_new_panel_ssd1306(panel_io_, &panel_config, &panel_);
#endif
    if (err == ESP_OK) {
      esp_lcd_panel_reset(panel_);
      err = esp_lcd_panel_init(panel_);
    }
    if (err != ESP_OK) {
      // No OLED answering on I2C: keep running (voice still works) without a screen.
      ESP_LOGE("KiboBoard", "OLED init failed (%s) - check SDA/SCL wiring and address 0x%02X",
               esp_err_to_name(err), DISPLAY_I2C_ADDR);
      panel_ = nullptr;
    } else {
      esp_lcd_panel_invert_color(panel_, false);
      esp_lcd_panel_mirror(panel_, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
      esp_lcd_panel_disp_on_off(panel_, true);
    }
    display_ = new kibo::KiboDisplay(panel_ ? panel_io_ : nullptr, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT);
  }

  void InitializeButtons() {
    // On-board BOOT button: click = talk; click while starting = Wi-Fi setup.
    boot_button_.OnClick([this]() {
      auto& app = Application::GetInstance();
      if (app.GetDeviceState() == kDeviceStateStarting) {
        EnterWifiConfigMode();
        return;
      }
      app.ToggleChatState();
    });
  }

  void InitializeKibo() {
    kibo::KiboPins pins;
    pins.touch = KIBO_TOUCH_GPIO;
    pins.amp_sd = KIBO_AMP_SD_GPIO;
    pins.battery_adc_channel = KIBO_BATTERY_ADC_CHANNEL;
    pins.divider_top_ohm = KIBO_BATTERY_DIVIDER_TOP;
    pins.divider_bottom_ohm = KIBO_BATTERY_DIVIDER_BOTTOM;
    pins.imu_addr = KIBO_IMU_ADDR;
    pins.axis_right = KIBO_IMU_AXIS_RIGHT;
    pins.axis_up = KIBO_IMU_AXIS_UP;
    pins.axis_out = KIBO_IMU_AXIS_OUT;
    pins.sign_right = KIBO_IMU_SIGN_RIGHT;
    pins.sign_up = KIBO_IMU_SIGN_UP;
    pins.sign_out = KIBO_IMU_SIGN_OUT;
    kibo::KiboCallbacks callbacks;
    callbacks.enter_wifi_config = [this]() { EnterWifiConfigMode(); };
    behavior_ = new kibo::KiboBehavior(display_, i2c_bus_, pins, std::move(callbacks));
    behavior_->Start();
  }

 public:
  KiboBoard() : boot_button_(BOOT_BUTTON_GPIO) {
    InitializeI2c();
    InitializeDisplay();
    InitializeButtons();
    InitializeKibo();
  }

  Led* GetLed() override {
#if KIBO_LED_IS_WS2812
    static SingleLed led(BUILTIN_LED_GPIO);
#else
    static GpioLed led(BUILTIN_LED_GPIO, 1 /* active LOW */);
#endif
    return &led;
  }

  AudioCodec* GetAudioCodec() override {
    // INMP441 + MAX98357A share BCLK/WS on one I2S port (full duplex).
    static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE, AUDIO_I2S_GPIO_BCLK,
                                          AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
    return &audio_codec;
  }

  Display* GetDisplay() override { return display_; }

  bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
    bool present = false;
    if (!behavior_ || !behavior_->GetBattery(level, present)) return false;
    charging = false;  // TP4056 status pins are not wired (its LEDs show charging)
    discharging = true;
    return true;
  }
};
