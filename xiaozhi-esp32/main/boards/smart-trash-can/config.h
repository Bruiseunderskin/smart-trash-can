#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// 如果使用 Duplex I2S 模式，请注释下面一行
#define AUDIO_I2S_METHOD_SIMPLEX

#ifdef AUDIO_I2S_METHOD_SIMPLEX

#define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_4
#define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_5
#define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_6
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_7
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_15
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_16

#else

#define AUDIO_I2S_GPIO_WS GPIO_NUM_4
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_5
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_6
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_7

#endif


#define BUILTIN_LED_GPIO        GPIO_NUM_48
#define BOOT_BUTTON_GPIO        GPIO_NUM_0
#define TOUCH_BUTTON_GPIO       GPIO_NUM_47
#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_40
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_39

#define DISPLAY_SDA_PIN GPIO_NUM_41
#define DISPLAY_SCL_PIN GPIO_NUM_42
#define DISPLAY_WIDTH   128

#if CONFIG_OLED_SSD1306_128X32
#define DISPLAY_HEIGHT  32
#elif CONFIG_OLED_SSD1306_128X64
#define DISPLAY_HEIGHT  64
#elif CONFIG_OLED_SH1106_128X64
#define DISPLAY_HEIGHT  64
#define SH1106
#else
#error "OLED display type is not selected"
#endif

#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y true


// ---------------------------------------------------------------------------
// 智能分类垃圾桶：4 路舵机桶盖
//
// 分类顺序固定为：0=可回收物 1=厨余垃圾 2=有害垃圾 3=其他垃圾
// 角度约定：0 度 = 关盖，90 度 = 开盖（与 arduino/MG90_LidServo_ESP32S3 示例一致）
// 舵机电源请使用独立 5V 供电，并与 ESP32-S3 共地。
// ---------------------------------------------------------------------------
#define TRASH_CAN_BIN_COUNT      4

#define SERVO_RECYCLABLE_GPIO    GPIO_NUM_9   // 可回收物
#define SERVO_KITCHEN_GPIO       GPIO_NUM_10  // 厨余垃圾
#define SERVO_HAZARDOUS_GPIO     GPIO_NUM_11  // 有害垃圾
#define SERVO_OTHER_GPIO         GPIO_NUM_12  // 其他垃圾

#define SERVO_CLOSE_DEG          0
#define SERVO_OPEN_DEG           90

// 开盖后自动关闭的延时（毫秒）
#define LID_AUTO_CLOSE_MS        8000

#endif // _BOARD_CONFIG_H_
