# ESP32-S31-Korvo-1 STC

本板型适配乐鑫 ESP32-S31-Korvo-1 开发板（V1.1），需要 ESP-IDF `release/v6.1`
或更新版本（`esp32s31` target）。它是官方
`esp32-s31-korvo-1` 板的独立变体，用于验证不经麦克风/ASR 的文字提问路径。

## 支持的板载硬件

- ESP32-S31-WROOM-3 模组（16MB Flash + 16MB 八线 PSRAM，Wi-Fi 6 / BT 5.4）
- ES8389 双声道音频编解码（I2C 地址 0x20，从 BCLK 恢复时钟，use_mclk=false）
- 双模拟麦克风阵列（双通道输入）+ 双 NS4150B 功放（立体声输出）
- RGB 接口 LCD 子板（ST7262E43，800x480，16MHz PCLK，背光常开）
- GT1151 触摸（与 codec 共享 I2C 总线）
- SC101IOT 摄像头（DVP 8bit，640x480 YUV422，SCCB 共享 I2C 总线）
- WS2812 RGB 状态灯（GPIO37）
- BOOT 键（GPIO61）+ 4 个 ADC 按键（GPIO42，电阻分压）
- microSD 卡槽、USB-A OTG（暂未启用）

## 动态文字接口

文字请求按到达顺序串行执行。上一条请求正在播放时，新请求会排队；设备回到
idle 后再发送，避免两条 TTS 重叠。队列最多保存 8 条，超出时立即记录
`reason=queue_full`。**长按 SET 键**使用 `phrases.json` 的
`question` 项发起 Q 请求，短按 SET 仍进入 Wi-Fi 配网。

### UART0 命令

通过 USB 串口以 UTF-8 发送一行命令，常用波特率为 115200：

```text
ask 呢張卡係咩垃圾
say 請放入廚餘桶
phrase right
ask
say
```

| 命令 | 模式 | 路径 |
| --- | --- | --- |
| `ask <text>` | Q | 文字 → LLM → TTS |
| `say <text>` | S | 受限 Q 回退，不是真正的 TTS-only |
| `phrase <key>` | 诊断 | 读取 `phrases.json` 并输出命中值；不联网 |

官方 server 的 `listen/detect.text` 只接受短文字。实测在总长度超过约 16 个
Unicode 字符时返回 `Detect is only for wake words, do not send long texts`。
因此 Q 自动增加 `40字：` 四字符前缀，原文字最多 12 字符；S 自动增加
`照讀：` 三字符前缀，原文字最多 13 字符。超限请求立即记录
`reason=server_text_limit`，不会占用 20 秒网络超时。

### C API

```c
#include "stc_chat.h"

stc_chat_send_text("廚餘應該放入邊個桶");
stc_chat_speak_text("答對喇");

const char *phrase = stc_phrase_get("right", "答對喇");
```

接口在 `stc_chat.h` 中以 `extern "C"` 声明，可在 C 游戏代码中调用。传入
`NULL` 或空白字符串会立即以 `reason=empty_text` 拒绝；队列容量受控，不在
音频任务中执行网络操作。

### 台詞表

`assets/phrases.json` 由构建脚本打入 `assets` 分区：

```json
{
  "open": "請將卡片放入任何一個桶",
  "right": "答對喇",
  "wrong": "答錯喇",
  "question": "呢張卡係咩垃圾",
  "timeout": "用手勢示意佢再試一次"
}
```

`stc_phrase_get(key, fallback)` 在 key 缺失或 assets 不可用时返回调用方给出的
fallback，不会返回空悬指针。动态文字会做 JSON 转义，引号、反斜杠和控制字符
不会破坏 `listen/detect` payload。

## 摄像头与 MCP

SC101IOT 仍会初始化，供游戏本机图像推理使用。默认
`CONFIG_STC_MCP_CAMERA` 关闭，此时 `GetCamera()` 返回 `nullptr`，server 不会
把 `self.camera.take_photo` 注册为 MCP 工具，文字提问不会产生云端拍照
round trip。需要恢复云端看图 demo 时，可显式打开：

```text
CONFIG_STC_MCP_CAMERA=y
```

文字路径依赖 `CONFIG_SEND_WAKE_WORD_DATA=y`。官方 board、`application.*` 和
`protocols/*` 均未修改。

## ADC 按键方案

采用官方 factory demo BSP 的方案（移植自 esp-dev-kits）：

- **S31 SAR ADC 软件校准**（`esp32_s31_adc_calibration.c`）：通过内部校准
  寄存器计算 17 位权重，使无硬件校准方案的 ESP32-S31 获得准确电压读数；
- **中心电压取分压理论值**：SET=1870mV，MODE=1340mV，VOL-=820mV，VOL+=380mV；
- **按键窗口取相邻键中心的中点**自动划分，无需手工调参。

> 注意：S31 的 SAR ADC 原始值是反相的（电压越高 raw 越小），未经校准的
> 直读公式会得出相反的键序——键位判断必须以校准后的电压为准。

## 音频引脚

| 信号 | GPIO |
| --- | ---: |
| I2C SDA（codec + 触摸 + 摄像头 SCCB 共享） | 0 |
| I2C SCL | 1 |
| I2S MCLK（仅驱动该脚，codec 不使用） | 2 |
| I2S BCLK / SCLK | 3 |
| I2S WS / LRCK | 4 |
| I2S DOUT（接 codec DSIN，播放） | 5 |
| I2S DIN（接 codec ASDOUT，录音） | 6 |
| NS4150B 功放使能（两路共用） | 7 |

> 原理图/文档中的 I2S 网络名是 codec 视角，与 SoC 视角相反，移植时注意。

## 摄像头引脚

| 信号 | GPIO |
| --- | ---: |
| D0 ~ D7 | 46 ~ 53 |
| PCLK | 54 |
| XCLK（20MHz） | 55 |
| VSYNC | 56 |
| HREF | 57 |
| RESET / PWDN | 未连接 |

## 构建

```sh
export IDF_CCACHE_ENABLE=1
idf.py set-target esp32s31
idf.py menuconfig   # Xiaozhi Assistant -> Board Type -> Espressif ESP32-S31-Korvo-1 STC text query test
idf.py build flash monitor
```

本变体只启用 SC101IOT 驱动，并明确关闭 OV3660，避免对错误传感器地址做探测。
若手工配置 sdkconfig，请确认：

```text
CONFIG_SEND_WAKE_WORD_DATA=y
# CONFIG_STC_MCP_CAMERA is not set
CONFIG_CAMERA_SC101IOT=y
# CONFIG_CAMERA_OV3660 is not set
CONFIG_FLASH_DEFAULT_ASSETS=y
```

硬件验证建议覆盖：动态 Q/S、状态机串行队列、原有语音通路、麦克风采集、
扬声器播放（立体声）、唤醒词、ADC 四键、BOOT 键、RGB 灯、屏幕显示、触摸、
拍照方向、Wi-Fi 配网与重连。完整样本、失败边界和限制见
[`VALIDATION.md`](./VALIDATION.md)。

## 参考资料

- [ESP32-S31-Korvo-1 用户指南](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32s31/esp32-s31-korvo-1/user_guide.html)
- [原理图](https://dl.espressif.com/schematics/esp32-s31-korvo-1-schematics.pdf)
- [官方 factory demo（esp-dev-kits examples/esp32-s31-korvo）](https://github.com/espressif/esp-dev-kits/tree/master/examples/esp32-s31-korvo/examples/factory_demo)
