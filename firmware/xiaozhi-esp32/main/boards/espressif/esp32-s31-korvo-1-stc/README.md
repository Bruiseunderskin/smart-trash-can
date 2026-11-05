# ESP32-S31-Korvo-1 STC

本板型适配乐鑫 ESP32-S31-Korvo-1 开发板（V1.1），需要 ESP-IDF
`release/v6.1` 或更新版本（`esp32s31` target）。它是官方
`esp32-s31-korvo-1` 板的独立变体，用于 P1 的本地五回合游戏与受控 CHAT
语音问答。未修改官方板目录、`application.*`、`protocols/*` 或 MCP server。

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

## GAME / CHAT 模式

开机默认进入 GAME。此时 WiFi 与云端协议尚未初始化，日志明确记录
`STC_NETWORK_GATE state=GAME action=block cloud_connections=0`。GAME 只播放
`assets/` 内的本地 PCM，并使用 stub 视觉与投放事件完成五回合：

- 标签顺序由 `assets/stub-labels.json` 控制，默认 `[0,1,2,3,0]`。
- 每回合模拟 200 ms 视觉处理，3 s 后投放；答对 `+10`、答错 `-5`、
  10 s 未投放 `0`。
- 五回合后进入 SETTLEMENT；只有 SETTLEMENT 可以长按 SET 进入 CHAT。

CHAT 首次进入时才启动 WiFi、OTA 检查与 MQTT 协议；网络就绪后开始聆听。
CHAT 中收到 `PLAY` 会被拒绝。连续 60 s 没有新的协议交互或回复播放时，
自动关闭音频通道、停止 WiFi 并回到 GAME。再次进入 CHAT 会重新完成网络
激活，因此不会复用已停止的协议对象。

## 本地语音

`stc_voice` 使用单播放任务和最多 8 项的 FIFO；同一时刻只会有一句本地 PCM
播放。`stc_voice_stop()` 清空队列并打断当前句。`phrases.json` 将
`stc_line_t` 映射到：

```json
{
  "open": {"text": "請將卡片放入任何一個桶", "pcm": "stc_open.pcm"},
  "correct": {"text": "答對喇", "pcm": "stc_correct.pcm"},
  "wrong": {"text": "答錯喇", "pcm": "stc_wrong.pcm"},
  "timeout": {"text": "冇放入，下一回合", "pcm": "stc_timeout.pcm"},
  "retry": {"text": "睇唔清，再試一次", "pcm": "stc_retry.pcm"},
  "summary": {"text": "呢局完結", "pcm": "stc_summary.pcm"}
}
```

内置 PCM 为 mono / 16 kHz / 16-bit，使用 `edge-tts`
`zh-HK-HiuMaanNeural` 生成。重建命令：

```powershell
python tools/generate_stc_voice_assets.py `
  --phrases firmware/xiaozhi-esp32/main/boards/espressif/esp32-s31-korvo-1-stc/assets/phrases.json `
  --tts-voice zh-HK-HiuMaanNeural `
  --output-dir firmware/xiaozhi-esp32/main/boards/espressif/esp32-s31-korvo-1-stc/assets
```

生成时需要 `edge-tts` 和 `ffmpeg`（也可安装 `imageio-ffmpeg`）；运行时不需要
网络或 TTS 服务。也可用 `--source-dir` 转换同规格的录音 WAV。

## 按键、LED 与 UART 验收

- 长按 SET：仅 SETTLEMENT 进入 CHAT。
- MODE/BOOT：GAME 中从 IDLE 或 SETTLEMENT 启动新局。
- 板载 WS2812：白=拍摄，绿=等待/答对，红=答错，橙=逾时，青=结算，
  紫=CHAT。

UART0 波特率 115200，接收单行命令：

| 命令 | 作用 |
| --- | --- |
| `stc status` | 输出模式、阶段、回合、分数和本地语音队列状态 |
| `stc voice` | 顺序播放 10 句并验证打断/恢复 |
| `stc chat` | 仅结算时进入 CHAT，用于无人值守验收 |
| `stc play` | 尝试开始新局，用来验证 CHAT 模式互斥 |
| `stc game` | 停止 CHAT 并回到 GAME |

完整结果与原始串口证据见 [`VALIDATION.md`](./VALIDATION.md) 和
[`evidence/`](./evidence/)。

## 摄像头与 MCP

SC101IOT 仍会初始化，供游戏本机图像推理使用。默认
`CONFIG_STC_MCP_CAMERA` 关闭，此时 `GetCamera()` 返回 `nullptr`，server 不会
把 `self.camera.take_photo` 注册为 MCP 工具，CHAT 问答不会产生云端拍照
round trip。需要恢复云端看图 demo 时，可显式打开：

```text
CONFIG_STC_MCP_CAMERA=y
```

GAME 本地播放不依赖 `CONFIG_SEND_WAKE_WORD_DATA`；该选项仅保留给 CHAT
原生小智通路。

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
idf.py menuconfig   # Xiaozhi Assistant -> Board Type -> Espressif ESP32-S31-Korvo-1 STC game/chat
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

硬件验证建议覆盖：五回合 stub smoke、本地 PCM 首播/队列/打断、模式互斥、
GAME 零云端连接、CHAT 真人语音问答、60 s 自动退出、ADC 四键、BOOT 键、
RGB 灯、屏幕显示、触摸、拍照方向、Wi-Fi 配网与重连。完整结果和限制见
[`VALIDATION.md`](./VALIDATION.md)。

## 参考资料

- [ESP32-S31-Korvo-1 用户指南](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32s31/esp32-s31-korvo-1/user_guide.html)
- [原理图](https://dl.espressif.com/schematics/esp32-s31-korvo-1-schematics.pdf)
- [官方 factory demo（esp-dev-kits examples/esp32-s31-korvo）](https://github.com/espressif/esp-dev-kits/tree/master/examples/esp32-s31-korvo/examples/factory_demo)
