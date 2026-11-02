// Coco Voice S3 - Seeed XIAO ESP32S3 (8 MB flash + 8 MB PSRAM) pin map, "Hey Coco".
// Matches coco_voice_s3/images/coco_voice_s3_wiring.png. Same modules as Coco Voice (C3);
// the GPIOs are Kibo's S3 pins, so XIAO pad names are in the comments.
// The 4 MB build (coco-s3-4mb) is for the ESP32-S3 Super Mini (HW-747): same GPIOs,
// 1.3" SH1106 OLED, RGB LED on GPIO48. Wiring: coco_voice_s3/images/coco_voice_s3_4mb_wiring.png.
#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

// ---- Audio: INMP441 + MAX98357A sharing BCLK/WS (one I2S port, duplex: rates must match) ----
#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 16000
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_7   // D8  - INMP441 SCK + MAX98357A BCLK
#define AUDIO_I2S_GPIO_WS   GPIO_NUM_8   // D9  - INMP441 WS  + MAX98357A LRC
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_9   // D10 - INMP441 SD
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_4   // D3  - MAX98357A DIN

// ---- I2C: OLED ----
#define KIBO_I2C_SDA_PIN GPIO_NUM_5      // D4
#define KIBO_I2C_SCL_PIN GPIO_NUM_6      // D5

// ---- Display: 0.96" SSD1306; the 4 MB build (coco-s3-4mb) drives a 1.3" SH1106 ----
#if CONFIG_ESPTOOLPY_FLASHSIZE_4MB
#define KIBO_OLED_SH1106 1
#else
#define KIBO_OLED_SH1106 0
#endif
#define DISPLAY_I2C_ADDR 0x3C
#define DISPLAY_WIDTH    128
#define DISPLAY_HEIGHT   64
#define DISPLAY_MIRROR_X false  // set both to true if the OLED is mounted pin header down
#define DISPLAY_MIRROR_Y false

// ---- Touch, buttons, LED ----
#define KIBO_TOUCH_GPIO   GPIO_NUM_1     // D0 - TTP223 I/O (RTC pin: deep-sleep wake)
#define BOOT_BUTTON_GPIO  GPIO_NUM_0     // XIAO BOOT button
#if CONFIG_ESPTOOLPY_FLASHSIZE_4MB
#define BUILTIN_LED_GPIO  GPIO_NUM_48    // S3 Super Mini (HW-747) RGB LED
#define KIBO_LED_IS_WS2812 1
#else
#define BUILTIN_LED_GPIO  GPIO_NUM_21    // XIAO user LED (active LOW)
#define KIBO_LED_IS_WS2812 0
#endif

// ---- Amp shutdown ----
#define KIBO_AMP_SD_GPIO  GPIO_NUM_43    // D6 - MAX98357A SD

// ---- Battery: BAT_SW -> 1M -> D1 (GPIO2) -> 330k -> GND, 100 nF across 330k ----
#define KIBO_BATTERY_ADC_CHANNEL    1    // ADC1_CH1 = GPIO2
#define KIBO_BATTERY_DIVIDER_TOP    1000000.0f
#define KIBO_BATTERY_DIVIDER_BOTTOM 330000.0f

// ---- No MPU6050 in Coco: the probe at 0x68 finds nothing and motion is skipped ----
#define KIBO_IMU_ADDR       0x68
#define KIBO_IMU_AXIS_RIGHT 0
#define KIBO_IMU_AXIS_UP    2
#define KIBO_IMU_AXIS_OUT   1
#define KIBO_IMU_SIGN_RIGHT 1
#define KIBO_IMU_SIGN_UP    1
#define KIBO_IMU_SIGN_OUT   1

#endif  // _BOARD_CONFIG_H_
