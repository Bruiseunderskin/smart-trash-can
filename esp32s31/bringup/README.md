# ESP32-S31-Korvo-1 Bring-up 实测报告

> 测试日期：2026-11-02
>
> 严格结论：**M1 仍未完成。** 用户已接受实际 SC101IOT，T2 也已补齐板上
> UYVY→JPEG 串口导出；但 T1、T3、T4、T5 仍缺少 issue 要求的照片、WAV
> 或人工听感证据。不能把“外设已初始化”或“自动检查通过”写成四项全通。

本报告分别记录“自动/链路检查结果”和“issue #11 严格验收结论”，避免把
缺少现场证据的项目误报为 PASS。

## 1. 硬件与工具链

| 项目 | 本次实测值 |
| --- | --- |
| 开发板 | ESP32-S31-Korvo-1 V1.1 |
| SoC | ESP32-S31 revision v0.0 |
| Flash / PSRAM | 16 MB / 16 MB，PSRAM 200 MHz |
| MAC | `1c:29:04:d0:58:a0` |
| 烧录与日志口 | `COM5`，115200 baud |
| 摄像头实测 | SC101IOT，PID `0xda4a`；1280x720 UYVY |
| LCD | 800x480 RGB565；GT1151 驱动识别到 GT1158 |
| Audio codec | ES8389 + NS4150B |
| microSD | 未插入；三次挂载均为 `ESP_ERR_TIMEOUT (0x107)` |
| ESP-IDF | v6.1，commit `fff9895c82d7` |
| IDF 路径 | `C:\esp\v6.1\esp-idf` |
| 工具路径 | `C:\Espressif\tools` |
| Python 环境 | `C:\Espressif\tools\python_env\idf6.1_py3.13_env` |
| 官方 BSP | `esp32_s31_korvo`，revision `5c06bf1` |

测试期间 `COM9` 上已有另一套 ESP32-S3 固件的 monitor。所有 S31 烧录和
monitor 操作都显式指定 `COM5`，未停止或重启 `COM9` 的既有进程。

## 2. 官方 factory_demo 门槛

先按 issue #11 的要求验证官方 `factory_demo`。运行日志确认：

- RGB LCD 800x480 初始化成功，GT1151 触摸初始化成功。
- 摄像头实际识别为 SC101IOT，PID `0xda4a`，不是 OV3660。
- DVP 首帧成功，格式为 640x480 UYVY。
- ES8389 音频链路以 48 kHz stereo 打开。
- USB HID、按键、WS2812 初始化，最终打印 `Factory demo is ready`。

证据：[factory_demo/monitor.log](artifacts/factory_demo/monitor.log)。
该结果证明官方 BSP 和板卡基础链路可用。后续产品决定已接受实际 SC101IOT，
因此它不再作为继续 #12 的阻断项。

## 3. 结果总表

| 测试 | 自动/链路结果 | issue #11 严格结论 | 关键实测数据 | 未满足项 |
| --- | --- | --- | --- | --- |
| T1 LCD | PASS（仅 timing） | **未完成验收** | 最大填屏 12.148 ms | 无五色、线条和文字的现场照片 |
| T2 Camera | PASS（链路） | **未完成验收** | 15.18 fps，30/30 帧有效；已串口导出 1280x720 JPEG | 当前现场图只有 1 张；无 SD |
| T3 Mic | FAIL | **FAIL** | 静音 RMS 254.39，讲话 RMS 227.51，比值 0.89 | 静音和讲话阈值失败；无 SD，未生成 WAV |
| T4 Speaker | PASS（目标频率与音量） | **未完成验收** | 最大回录峰值 14,888；440 Hz 两档增幅 18.70/21.05 dB；1 kHz 两档增幅 14.77/18.91 dB | 无 speaker 回录 WAV；人工听感和“无爆音”听感未验收 |
| T5 Loopback | FAIL（因 WAV 条件） | **未完成验收** | underrun 0，overrun 0，read/write error 0；输入 RMS 146.94，峰值 3005 | 无 SD，未生成 WAV；啸叫和原声听感未人工验收 |

**最终判定：尚未完成。** SC101IOT 已获接受，串口 JPEG 导出也已跑通；
其余证据缺口仍不能按 issue 规则宣称 PASS。

## 4. 分项结果

### T1 LCD

程序依次全屏显示红、绿、蓝、白、黑各 2 秒，随后绘制 8 条等间距垂直线
以及中英混合文字。实测填屏耗时：

| 颜色 | 耗时 |
| --- | ---: |
| 红 | 12.148 ms |
| 绿 | 9.541 ms |
| 蓝 | 9.534 ms |
| 白 | 9.554 ms |
| 黑 | 9.556 ms |

最大值 12.148 ms，低于 issue 的 40 ms 门槛；自动日志为：

```text
T1_AUTO_CHECK=PASS timing=pass visual=REQUIRES_EXTERNAL_PHOTO
```

现场照片缺失，因此颜色、无撕裂、无偏移和文字清晰度仍不能验收。

证据：[T1 monitor.log](artifacts/t1/monitor.log)。

### T2 Camera

独立测试请求 1280x720 JPEG，实际枚举结果为：

```text
T2_FORMAT width=1280 height=720 fourcc=UYVY
T2_FORMAT_MISMATCH expected=OV3660/JPEG actual=UYVY
```

连续采集 30 帧，30 帧均通过亮度范围与方差检查；后续串口采集实测
15.18 fps，每帧 1,843,200 字节。当前程序使用 `esp_new_jpeg` 将 UYVY
编码为 JPEG，并通过 COM5 带长度和 CRC32 下载；已成功取得一张
1280x720、66,921 bytes 的真实相机图片。无 SD 不再是图像导出的阻断项，
但 issue #11 的 3 张交付仍未补齐。

官方 factory_demo 和设备识别日志均指出实际传感器为 SC101IOT。该差异
已由用户接受，不再作为硬失败。

证据：[T2 monitor.log](artifacts/t2/monitor.log)。

### T3 Mic

麦克风以 16 kHz、16-bit、mono 打开，每段采集 48,000 samples。实测：

| 阶段 | RMS | Peak | Clipped |
| --- | ---: | ---: | ---: |
| 静音 | 254.39 | 1840 | 0 |
| 讲话 | 227.51 | 893 | 0 |

静音 RMS 未低于 200，讲话/静音比值仅 0.89，未达到至少 3 倍的要求。
本次执行时没有可用的人工讲话激励证据，且无 microSD，无法保存和回听
3 秒 WAV，所以不能把“麦克风能够读数据”当作 T3 PASS。

证据：[T3 monitor.log](artifacts/t3/monitor.log)。

### T4 Speaker

T4 播放 440 Hz、1 kHz 正弦音，每次用板载麦克风回录，并对目标频点和
对照频点做 Goertzel 测量；之后播放 20 Hz–16 kHz 扫频。三档音量下目标
频率分量均单调增长：

| 频率 | 30% target dB | 60% target dB | 100% target dB |
| --- | ---: | ---: | ---: |
| 440 Hz | 23.70 | 42.41 | 63.46 |
| 1 kHz | 40.46 | 55.22 | 74.14 |

程序内部要求每次音量提升至少 3 dB，440 Hz 与 1 kHz 均通过；回录最大
peak 为 14,888，未检测到削波样本。扫频完整结束。自动结果：

```text
T4_AUTO_CHECK=PASS audio_beauty=REQUIRES_EXTERNAL_LISTEN
```

第一次 T4 运行使用宽带 RMS 判断音量，环境噪声掩盖了 30%→60% 的目标
信号变化。最终测试改为直接测量对应音频频率分量，符合 issue 的
“量到对应频率的峰”要求；两次原始日志都保留，便于复核。当前仍缺
speaker 回录 WAV 和人工听感，因此严格交付尚未完成。

证据：[T4 最终 monitor.log](artifacts/t4/monitor.log)；
[T4 初次宽带 RMS 日志](artifacts/t4/monitor.rms-check.log)。

### T5 Loopback

mic 输入为 16 kHz mono，speaker 输出为 16 kHz stereo；20 ms chunk，
预置 10 个 chunk，即约 200 ms 延迟，运行 10 秒。实测：

```text
T5_STATS underruns=0 overruns=0 read_errors=0 write_errors=0 rms=146.94 peak=3005
T5_AUTO_CHECK=FAIL buffers=pass io=pass wav=fail feedback=REQUIRES_EXTERNAL_LISTEN
```

缓冲和同步读写路径通过，自动总检查因缺少 WAV 而失败。由于没有 SD，
不能回听原始声是否一致，也无法人工确认没有回授啸叫。

证据：[T5 monitor.log](artifacts/t5/monitor.log)。

## 5. 独立构建与烧录

五个工程均位于本目录下，彼此没有共享可执行文件；公共代码只有官方 BSP
和音频统计/WAV 小工具。每个工程的 `build.log` 都包含
`Project build complete`，烧录日志均连接 ESP32-S31 并使用 `COM5`。

T3 的构建日志保留了 IDF 6.1 将 `const` sample-info 指针传给
`esp_codec_dev_open()` 后的失败，以及修复后的成功结果。T5 的
`build.failed.log` 保留了首次缺少 `esp_ringbuf` 组件声明的问题；
补齐 `main/CMakeLists.txt` 后，最终 [build.log](artifacts/t5/build.log)
成功生成固件。

### 本机复现命令

ESP-IDF v6.1 安装后可直接使用以下 PowerShell 环境。`esp32s31` 在
v6.1 中仍是 preview target，因此需要使用 `--preview`：

```powershell
$env:PATH='C:\Users\jack\scoop\apps\python313\current;C:\Users\jack\scoop\apps\python313\current\Scripts;'+$env:PATH
$env:IDF_TOOLS_PATH='C:\Espressif\tools'
& 'C:\esp\v6.1\esp-idf\export.ps1'
```

进入任一测试目录后构建和烧录：

```powershell
python "$env:IDF_PATH\tools\idf.py" --preview set-target esp32s31
python "$env:IDF_PATH\tools\idf.py" --preview -p COM5 build flash
python "$env:IDF_PATH\tools\idf.py" --preview -p COM5 monitor
```

各工程的 `sdkconfig.defaults` 已包含目标、16 MB Flash、Oct PSRAM、
320 MHz CPU 和外设开关；`sdkconfig`、`build/` 和 `managed_components/`
是可再生成的本地文件，不应手工伪造测试结果。

## 6. 当前缺少的交付证据

- LCD 五色、8 条线和文字的现场照片。
- 3 张可由电脑打开的真实现场 JPEG；已通过串口完成 1 张，还需 2 张。
- mic 静音/讲话 3 秒 WAV，以及真实讲话时 RMS 上升至少 3 倍的日志。
- speaker 三档音量回录 WAV。
- loopback 原声/输出 WAV，以及现场无啸叫、声音一致的人工确认。
- 接线全貌照片。

当前唯一现场 JPG 位于 `docs/issue12/live-camera/`，内容为天花板和窗帘，
仅证明相机链路，不代替 issue 要求的垃圾现场图。仓库仍无本次测试生成的
WAV 产物。

## 7. M1 决策

按 issue #11 和 `docs/ROADMAP.md`：

1. SC101IOT/UYVY 已获接受，T2 串口 JPEG 链路已验证，不再构成硬失败。
2. 缺照片、WAV、外部听感及剩余现场 JPEG 的项目不能补写成 PASS。
3. 因此 M1 仍未完成，不能据此宣称四外设全通。

后续按真实人工操作补齐 T1–T5 所需的照片、剩余 JPEG、WAV 和听感证据。
只有全部硬门槛和交付证据都满足后，才应更新 ROADMAP 为全通。

本次未修改 `xiaozhi-esp32/`，也未向 GitHub issue、commit 或 ROADMAP
写入结果。
