// Coco Voice - ESP32-C3 Super Mini pin map.
// Matches coco_voice_c3/images/coco_voice_c3_wiring.png. Change pins here only together with the wiring.
// Same GPIOs as Kibo's C3 build, with a 0.96" SSD1306 and no MPU6050.
#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

// ---- Audio: INMP441 mic + MAX98357A amp sharing BCLK/WS (one I2S port, duplex) ----
// Duplex I2S runs both directions on the same clock, so the rates must match.
#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 16000
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_6   // INMP441 SCK  + MAX98357A BCLK
#define AUDIO_I2S_GPIO_WS   GPIO_NUM_7   // INMP441 WS   + MAX98357A LRC
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_3   // INMP441 SD   (mic data in)
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_10  // MAX98357A DIN (speaker data out)

// ---- I2C bus: OLED (0x3C) ----
#define KIBO_I2C_SDA_PIN GPIO_NUM_4
#define KIBO_I2C_SCL_PIN GPIO_NUM_5

// ---- Display: 0.96" 128x64 SSD1306, 4-pin I2C ----
#define KIBO_OLED_SH1106 0
#define DISPLAY_I2C_ADDR 0x3C
#define DISPLAY_WIDTH    128
#define DISPLAY_HEIGHT   64
#define DISPLAY_MIRROR_X false  // set both to true if the OLED is mounted pin header down
#define DISPLAY_MIRROR_Y false

// ---- Touch, buttons, LED ----
#define KIBO_TOUCH_GPIO   GPIO_NUM_0   // TTP223 I/O, active HIGH, wakes from deep sleep
#define BOOT_BUTTON_GPIO  GPIO_NUM_9   // on-board BOOT button
#define BUILTIN_LED_GPIO  GPIO_NUM_8   // on-board blue LED (active LOW)
#define KIBO_LED_IS_WS2812 0

// ---- Amp shutdown: MAX98357A SD, mutes the amp in deep sleep ----
#define KIBO_AMP_SD_GPIO  GPIO_NUM_20

// ---- Battery: BAT_SW -> 1 MOhm -> GPIO1 -> 330 kOhm -> GND (100 nF across the 330k) ----
#define KIBO_BATTERY_ADC_CHANNEL    1  // ADC1_CH1 = GPIO1
#define KIBO_BATTERY_DIVIDER_TOP    1000000.0f
#define KIBO_BATTERY_DIVIDER_BOTTOM 330000.0f

// ---- No MPU6050 in Coco. The shared code probes 0x68, finds nothing and skips motion. ----
#define KIBO_IMU_ADDR       0x68
#define KIBO_IMU_AXIS_RIGHT 0
#define KIBO_IMU_AXIS_UP    2
#define KIBO_IMU_AXIS_OUT   1
#define KIBO_IMU_SIGN_RIGHT 1
#define KIBO_IMU_SIGN_UP    1
#define KIBO_IMU_SIGN_OUT   1

#endif  // _BOARD_CONFIG_H_
