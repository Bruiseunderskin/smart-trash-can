# ESP32-S31-Korvo-1 STC

本板型适配乐鑫 ESP32-S31-Korvo-1 开发板（V1.1），需要 ESP-IDF
`release/v6.1` 或更新版本（`esp32s31` target）。它是官方
`esp32-s31-korvo-1` 板的独立变体，用外层 GAME/CHAT 状态机隔离垃圾游戏与
小智语音问答。未修改官方板目录、`application.*`、`protocols/*` 或 MCP
server；小智内部 `kDeviceState` 也保持不变。

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
`STC_NETWORK_GATE state=GAME action=block cloud_connections=0`。GAME 页面
当前是空白占位，不运行任何游戏逻辑、不收语音、不显示小智。

外层状态机为 `GAME -> CHAT_STARTING -> CHAT_READY -> GAME`，与上游设备状态
正交：

- `GAME`：关闭唤醒、断开音频通道、停止 WiFi；空白游戏页启用。
- `CHAT_STARTING`：启动 WiFi/OTA/协议，等待上游真正到达 idle。
- `CHAT_READY`：启动一次原生小智聆听；显示项目自建的 CHAT 页面。
- CHAT 连续 60 s 无协议交互或 TTS 播放时自动回 GAME。

进入 CHAT 时先等待网络激活完成，之后才调用 `stc_voice_start_dialog()`，避免
协议尚未初始化就提前进入 Listening。再次进入 CHAT 会重新完成激活。

## 自建 UI 与替换点

屏幕底部有两个常驻触控按钮 `GAME` 和 `CHAT`。按钮创建在 LVGL top layer，
内容区和按钮区使用独立的 386 px / 94 px 高度，不再覆盖小智字幕。

- CHAT 页面完全替代小智原生显示，提供：
  - 当前用户 STT 或小智 TTS 大字幕，使用动态中文字体、1 px 描边和
    约 1.08 倍视觉放大；
  - Wi-Fi/云端连接状态；
  - 小智内部 `DeviceState` 中英文状态，例如
    `聆听中 · listening`、`回复中 · speaking`；
  - 底部常驻 `GAME` / `CHAT` 触控按钮。
- `stc_chat_display.cc/.h`：只负责把 `SetChatMessage()` 等 XiaoZhi 事件
  转给自建 UI，不创建原生小智页面。
- `stc_shell_ui.cc`：CHAT/GAME 页面布局、字幕、状态和模式栏。后续更换
  视觉设计时只改这一个文件，保留公开的更新函数即可。
- `stc_game_placeholder.cc`：游戏页占位内容，后续球队 UI 只需替换此文件。
- `stc_app_fsm.c/.h`：不依赖 LVGL 或小智的外层状态机，已加主机测试。

按钮回调只设置“请求模式”，不会在 LVGL 任务内直接操作协议；实际启停由
`stc_shell` 主任务执行。

## 本地语音

GAME 语音资源仍保留给后续游戏逻辑使用。`stc_voice` 使用单播放任务和最多
8 项的 FIFO；同一时刻只会有一句本地 PCM 播放。`stc_voice_stop()` 清空队列
并打断当前句。当前空白游戏页不会自动触发这些台词。`phrases.json` 将
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

## 按键、LED 与 UART

- 屏幕 `GAME` / `CHAT`：主要模式切换入口。
- 长按 SET：请求 CHAT；MODE/BOOT：请求 GAME，作为硬件备用入口。
- 板载 WS2812：蓝=GAME，青=CHAT 连接中，紫=CHAT 可用。

UART0 波特率 115200，接收单行命令：

| 命令 | 作用 |
| --- | --- |
| `stc status` | 输出外层状态、小智模式、底层 DeviceState、网络和语音队列 |
| `stc screen` | 采样两个 RGB framebuffer，输出平均颜色与白色比例，用于诊断白屏 |
| `stc voice` | 顺序播放 10 句并验证打断/恢复 |
| `stc chat` | 请求 CHAT，用于无人值守验收 |
| `stc play` | 游戏占位动作；当前只记录 `result=placeholder` |
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

硬件验证建议覆盖：两个触控按钮、GAME 空白页遮挡、CHAT 原生语音问答、
GAME 零云端连接、60 s 自动退出、再次进入 CHAT、ADC 四键、BOOT 键、
RGB 灯、屏幕显示、拍照方向、Wi-Fi 配网与重连。完整结果和限制见
[`VALIDATION.md`](./VALIDATION.md)。

## 参考资料

- [ESP32-S31-Korvo-1 用户指南](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32s31/esp32-s31-korvo-1/user_guide.html)
- [原理图](https://dl.espressif.com/schematics/esp32-s31-korvo-1-schematics.pdf)
- [官方 factory demo（esp-dev-kits examples/esp32-s31-korvo）](https://github.com/espressif/esp-dev-kits/tree/master/examples/esp32-s31-korvo/examples/factory_demo)
