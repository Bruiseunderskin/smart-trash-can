/*
 * ESP32-S3 I2S 麦克风录音与回放测试（Arduino）
 *
 * 引脚与 xiaozhi-esp32 的 smart-trash-can / bread-compact-wifi 板型一致：
 *   麦克风 WS / LRCK -> GPIO4
 *   麦克风 SCK / BCLK -> GPIO5
 *   麦克风 SD / DIN   -> GPIO6
 *
 *   功放 BCLK         -> GPIO15
 *   功放 LRCK / WS    -> GPIO16
 *   功放 DIN          -> GPIO7
 *
 * 假设使用的是 INMP441、ICS-43434 等标准数字 I2S 麦克风。
 * 上电后自动录音 5 秒并回放；串口命令：
 *   r = 录音并回放
 *   p = 重放上次录音
 *   s = 再显示上次录音统计
 *   h = 显示帮助
 */

#include <Arduino.h>
#include <driver/i2s_std.h>
#include <esp_heap_caps.h>
#include <math.h>

// ---------------- 可调参数 ----------------
static constexpr gpio_num_t MIC_WS_PIN = GPIO_NUM_4;
static constexpr gpio_num_t MIC_BCLK_PIN = GPIO_NUM_5;
static constexpr gpio_num_t MIC_DATA_PIN = GPIO_NUM_6;

static constexpr gpio_num_t SPEAKER_BCLK_PIN = GPIO_NUM_15;
static constexpr gpio_num_t SPEAKER_WS_PIN = GPIO_NUM_16;
static constexpr gpio_num_t SPEAKER_DATA_PIN = GPIO_NUM_7;

static constexpr bool ENABLE_PLAYBACK = true;
static constexpr bool MIC_USE_LEFT_CHANNEL = true;

static constexpr uint32_t SAMPLE_RATE_HZ = 16000;
static constexpr uint32_t RECORD_SECONDS = 5;
static constexpr size_t RECORD_FRAMES = SAMPLE_RATE_HZ * RECORD_SECONDS;
static constexpr size_t READ_CHUNK_FRAMES = 256;

// xiaozhi-esp32 的 NoAudioCodec 将 32-bit I2S 数据右移 12 位后作为 int16 PCM。
static constexpr int32_t PCM_SHIFT_BITS = 12;

// 32 位 I2S 槽中，int16 PCM 需要放大到高 16 位。
static constexpr int32_t PLAYBACK_GAIN_Q16 = 49152;  // 75%，可改为 65536 得到 100%

static constexpr i2s_port_t MIC_I2S_PORT = I2S_NUM_1;
static constexpr i2s_port_t SPEAKER_I2S_PORT = I2S_NUM_0;
// ---------------- 可调参数结束 ----------------

struct AudioStats {
  uint32_t sampleCount = 0;
  int64_t sampleSum = 0;
  int64_t squareSum = 0;
  int16_t minimum = INT16_MAX;
  int16_t maximum = INT16_MIN;
  uint32_t clippedSamples = 0;
};

i2s_chan_handle_t micRx = nullptr;
i2s_chan_handle_t speakerTx = nullptr;
int16_t *recordBuffer = nullptr;
size_t recordedFrames = 0;
bool microphoneReady = false;
bool speakerReady = false;
bool recordBufferInPsram = false;
AudioStats lastStats;

static int16_t rawToPcm(int32_t raw) {
  const int32_t shifted = raw >> PCM_SHIFT_BITS;
  if (shifted > INT16_MAX) {
    return INT16_MAX;
  }
  if (shifted < INT16_MIN) {
    return INT16_MIN;
  }
  return static_cast<int16_t>(shifted);
}

static void addStats(AudioStats &stats, int16_t sample) {
  ++stats.sampleCount;
  stats.sampleSum += sample;
  stats.squareSum += static_cast<int64_t>(sample) * sample;

  if (sample < stats.minimum) {
    stats.minimum = sample;
  }
  if (sample > stats.maximum) {
    stats.maximum = sample;
  }
  if (sample >= INT16_MAX - 16 || sample <= INT16_MIN + 16) {
    ++stats.clippedSamples;
  }
}

static double rmsOf(const AudioStats &stats) {
  if (stats.sampleCount == 0) {
    return 0.0;
  }
  return sqrt(static_cast<double>(stats.squareSum) / stats.sampleCount);
}

static double acRmsOf(const AudioStats &stats) {
  if (stats.sampleCount == 0) {
    return 0.0;
  }
  const double mean = static_cast<double>(stats.sampleSum) / stats.sampleCount;
  const double meanSquare = static_cast<double>(stats.squareSum) / stats.sampleCount;
  const double variance = meanSquare - mean * mean;
  return variance > 0.0 ? sqrt(variance) : 0.0;
}

static bool initMicrophone() {
  i2s_chan_config_t channelConfig =
      I2S_CHANNEL_DEFAULT_CONFIG(MIC_I2S_PORT, I2S_ROLE_MASTER);
  channelConfig.dma_desc_num = 6;
  channelConfig.dma_frame_num = READ_CHUNK_FRAMES;
  channelConfig.auto_clear_after_cb = true;

  esp_err_t err = i2s_new_channel(&channelConfig, nullptr, &micRx);
  if (err != ESP_OK) {
    Serial.printf("创建麦克风 I2S 通道失败: %s\n", esp_err_to_name(err));
    return false;
  }

  i2s_std_config_t stdConfig = {};
  stdConfig.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE_HZ);
  stdConfig.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
      I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO);

  // INMP441 / ICS-43434 的 L/R 接 GND 时输出在左声道。
  stdConfig.slot_cfg.slot_mask =
      MIC_USE_LEFT_CHANNEL ? I2S_STD_SLOT_LEFT : I2S_STD_SLOT_RIGHT;
  stdConfig.slot_cfg.ws_width = I2S_DATA_BIT_WIDTH_32BIT;
  stdConfig.slot_cfg.ws_pol = false;
  stdConfig.slot_cfg.bit_shift = true;

#if defined(SOC_I2S_HW_VERSION_2) && SOC_I2S_HW_VERSION_2
  stdConfig.slot_cfg.left_align = true;
  stdConfig.slot_cfg.big_endian = false;
  stdConfig.slot_cfg.bit_order_lsb = false;
#endif

  stdConfig.gpio_cfg.mclk = I2S_GPIO_UNUSED;
  stdConfig.gpio_cfg.bclk = MIC_BCLK_PIN;
  stdConfig.gpio_cfg.ws = MIC_WS_PIN;
  stdConfig.gpio_cfg.dout = I2S_GPIO_UNUSED;
  stdConfig.gpio_cfg.din = MIC_DATA_PIN;
  stdConfig.gpio_cfg.invert_flags.mclk_inv = false;
  stdConfig.gpio_cfg.invert_flags.bclk_inv = false;
  stdConfig.gpio_cfg.invert_flags.ws_inv = false;

  err = i2s_channel_init_std_mode(micRx, &stdConfig);
  if (err == ESP_OK) {
    err = i2s_channel_enable(micRx);
  }
  if (err != ESP_OK) {
    Serial.printf("初始化麦克风 I2S 失败: %s\n", esp_err_to_name(err));
    i2s_del_channel(micRx);
    micRx = nullptr;
    return false;
  }

  Serial.printf("麦克风 I2S 就绪: WS=%d, BCLK=%d, DIN=%d, %lu Hz\n",
                static_cast<int>(MIC_WS_PIN),
                static_cast<int>(MIC_BCLK_PIN),
                static_cast<int>(MIC_DATA_PIN),
                static_cast<unsigned long>(SAMPLE_RATE_HZ));
  return true;
}

static bool initSpeaker() {
  i2s_chan_config_t channelConfig =
      I2S_CHANNEL_DEFAULT_CONFIG(SPEAKER_I2S_PORT, I2S_ROLE_MASTER);
  channelConfig.dma_desc_num = 6;
  channelConfig.dma_frame_num = READ_CHUNK_FRAMES;
  channelConfig.auto_clear_after_cb = true;

  esp_err_t err = i2s_new_channel(&channelConfig, &speakerTx, nullptr);
  if (err != ESP_OK) {
    Serial.printf("创建功放 I2S 通道失败（麦克风测试仍可继续）: %s\n",
                  esp_err_to_name(err));
    return false;
  }

  i2s_std_config_t stdConfig = {};
  stdConfig.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE_HZ);
  stdConfig.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
      I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO);
  stdConfig.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
  stdConfig.slot_cfg.ws_width = I2S_DATA_BIT_WIDTH_32BIT;
  stdConfig.slot_cfg.ws_pol = false;
  stdConfig.slot_cfg.bit_shift = true;

#if defined(SOC_I2S_HW_VERSION_2) && SOC_I2S_HW_VERSION_2
  stdConfig.slot_cfg.left_align = true;
  stdConfig.slot_cfg.big_endian = false;
  stdConfig.slot_cfg.bit_order_lsb = false;
#endif

  stdConfig.gpio_cfg.mclk = I2S_GPIO_UNUSED;
  stdConfig.gpio_cfg.bclk = SPEAKER_BCLK_PIN;
  stdConfig.gpio_cfg.ws = SPEAKER_WS_PIN;
  stdConfig.gpio_cfg.dout = SPEAKER_DATA_PIN;
  stdConfig.gpio_cfg.din = I2S_GPIO_UNUSED;
  stdConfig.gpio_cfg.invert_flags.mclk_inv = false;
  stdConfig.gpio_cfg.invert_flags.bclk_inv = false;
  stdConfig.gpio_cfg.invert_flags.ws_inv = false;

  err = i2s_channel_init_std_mode(speakerTx, &stdConfig);
  if (err == ESP_OK) {
    err = i2s_channel_enable(speakerTx);
  }
  if (err != ESP_OK) {
    Serial.printf("初始化功放 I2S 失败（麦克风测试仍可继续）: %s\n",
                  esp_err_to_name(err));
    i2s_del_channel(speakerTx);
    speakerTx = nullptr;
    return false;
  }

  Serial.printf("回放 I2S 就绪: BCLK=%d, LRCK=%d, DIN=%d\n",
                static_cast<int>(SPEAKER_BCLK_PIN),
                static_cast<int>(SPEAKER_WS_PIN),
                static_cast<int>(SPEAKER_DATA_PIN));
  return true;
}

static bool allocateRecordBuffer() {
  const size_t bytes = RECORD_FRAMES * sizeof(int16_t);
  recordBuffer = static_cast<int16_t *>(
      heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  recordBufferInPsram = recordBuffer != nullptr;

  if (recordBuffer == nullptr) {
    recordBuffer = static_cast<int16_t *>(
        heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  }

  if (recordBuffer == nullptr) {
    Serial.printf("无法分配 %lu 字节录音缓冲\n", static_cast<unsigned long>(bytes));
    return false;
  }

  Serial.printf("录音缓冲: %lu 字节（%s）\n",
                static_cast<unsigned long>(bytes),
                recordBufferInPsram ? "PSRAM" : "内部 RAM");
  return true;
}

static void printStats(const AudioStats &stats) {
  if (stats.sampleCount == 0) {
    Serial.println("没有可统计的采样。");
    return;
  }

  const double mean = static_cast<double>(stats.sampleSum) / stats.sampleCount;
  const double rms = rmsOf(stats);
  const double acRms = acRmsOf(stats);
  const int peakToPeak =
      static_cast<int>(stats.maximum) - static_cast<int>(stats.minimum);
  const double dbfs =
      acRms > 0.0 ? 20.0 * log10(acRms / 32768.0) : -120.0;
  const double clippedPercent =
      100.0 * stats.clippedSamples / stats.sampleCount;

  Serial.println();
  Serial.println("---------------- 录音统计 ----------------");
  Serial.printf("采样数: %lu\n", static_cast<unsigned long>(stats.sampleCount));
  Serial.printf("最小值 / 最大值: %d / %d\n", stats.minimum, stats.maximum);
  Serial.printf("峰峰值: %d\n", peakToPeak);
  Serial.printf("直流偏置: %.1f\n", mean);
  Serial.printf("RMS: %.1f（AC RMS %.1f，约 %.1f dBFS）\n",
                rms, acRms, dbfs);
  Serial.printf("接近满幅的采样: %.2f%%\n", clippedPercent);
  Serial.print("诊断: ");

  if (peakToPeak <= 2 || acRms < 1.0) {
    Serial.println("数据几乎不变化，DIN 可能断线、没有时钟或麦克风损坏。");
  } else if (stats.maximum >= INT16_MAX - 16 &&
             stats.minimum <= INT16_MIN + 16) {
    Serial.println("数据在正负满幅之间跳动，优先检查通道/格式、时钟和接线。");
  } else if (clippedPercent > 1.0) {
    Serial.println("明显削波，麦克风可能离声源太近或增益/格式不匹配。");
  } else if (acRms < 20.0) {
    Serial.println("有数据变化，但电平极低；请靠近麦克风正常说话后重测。");
  } else if (acRms < 80.0) {
    Serial.println("电平偏弱；请检查麦克风孔、供电和焊接后用正常音量重测。");
  } else if (acRms > 5000.0) {
    Serial.println("电平过高，可能靠近声源或存在异常自激。");
  } else {
    Serial.println("检测到有效声音电平，线路基本正常；请听回放确认清晰度。");
  }
  Serial.println("注意：统计只能判断电气/格式问题，不能单独证明振膜未受损。");
  Serial.println("------------------------------------------");
  Serial.println();
}

static bool recordAudio() {
  if (!microphoneReady || recordBuffer == nullptr) {
    Serial.println("麦克风未初始化。");
    return false;
  }

  int32_t rawChunk[READ_CHUNK_FRAMES];
  AudioStats stats;
  recordedFrames = 0;

  Serial.println("准备录音，请保持安静约 0.3 秒，然后开始说话...");
  delay(300);

  // 丢弃启动后的首段数据，避免 DMA 中残留的旧样本影响统计。
  size_t warmupRemaining = SAMPLE_RATE_HZ / 5;
  while (warmupRemaining > 0) {
    const size_t framesToRead =
        min(warmupRemaining, static_cast<size_t>(READ_CHUNK_FRAMES));
    size_t bytesRead = 0;
    const esp_err_t err = i2s_channel_read(
        micRx, rawChunk, framesToRead * sizeof(int32_t), &bytesRead, 1000);
    if (err != ESP_OK || bytesRead == 0) {
      Serial.printf("麦克风读取失败: %s\n", esp_err_to_name(err));
      return false;
    }
    warmupRemaining -= bytesRead / sizeof(int32_t);
  }

  Serial.printf("开始录音 %lu 秒...\n", static_cast<unsigned long>(RECORD_SECONDS));
  uint32_t lastProgressMs = millis();

  while (recordedFrames < RECORD_FRAMES) {
    const size_t remaining = RECORD_FRAMES - recordedFrames;
    const size_t framesToRead =
        min(remaining, static_cast<size_t>(READ_CHUNK_FRAMES));
    size_t bytesRead = 0;
    const esp_err_t err = i2s_channel_read(
        micRx, rawChunk, framesToRead * sizeof(int32_t), &bytesRead, 1000);

    if (err != ESP_OK || bytesRead == 0) {
      Serial.printf("录音中断，I2S 读取失败: %s\n", esp_err_to_name(err));
      return false;
    }

    const size_t framesRead = bytesRead / sizeof(int32_t);
    int blockPeak = 0;
    double blockSquareSum = 0.0;

    for (size_t i = 0; i < framesRead; ++i) {
      const int16_t sample = rawToPcm(rawChunk[i]);
      recordBuffer[recordedFrames + i] = sample;
      addStats(stats, sample);

      const int magnitude = abs(static_cast<int>(sample));
      if (magnitude > blockPeak) {
        blockPeak = magnitude;
      }
      blockSquareSum += static_cast<double>(sample) * sample;
    }
    recordedFrames += framesRead;

    if (millis() - lastProgressMs >= 250) {
      const double blockRms = sqrt(blockSquareSum / framesRead);
      const int bars =
          constrain(static_cast<int>((blockRms / 1000.0) * 32.0), 0, 32);
      Serial.printf("[%4lu%%] ", static_cast<unsigned long>(
                                   100UL * recordedFrames / RECORD_FRAMES));
      for (int i = 0; i < 32; ++i) {
        Serial.print(i < bars ? '#' : '.');
      }
      Serial.printf("  峰值=%d RMS=%.0f\n", blockPeak, blockRms);
      lastProgressMs = millis();
    }
  }

  lastStats = stats;
  printStats(lastStats);
  return true;
}

static bool writeAll(const int32_t *data, size_t bytes) {
  size_t totalWritten = 0;
  while (totalWritten < bytes) {
    size_t bytesWritten = 0;
    const esp_err_t err = i2s_channel_write(
        speakerTx, reinterpret_cast<const uint8_t *>(data) + totalWritten,
        bytes - totalWritten, &bytesWritten, 1000);
    if (err != ESP_OK || bytesWritten == 0) {
      Serial.printf("功放写入失败: %s\n", esp_err_to_name(err));
      return false;
    }
    totalWritten += bytesWritten;
  }
  return true;
}

static bool playLastRecording() {
  if (!ENABLE_PLAYBACK) {
    Serial.println("ENABLE_PLAYBACK=false，跳过回放。");
    return false;
  }
  if (!speakerReady || speakerTx == nullptr) {
    Serial.println("功放 I2S 未就绪，跳过回放；串口统计仍可用于诊断。");
    return false;
  }
  if (recordedFrames == 0 || recordBuffer == nullptr) {
    Serial.println("还没有录音可回放。");
    return false;
  }

  Serial.println("开始回放...");
  int32_t outputChunk[READ_CHUNK_FRAMES];
  size_t offset = 0;

  while (offset < recordedFrames) {
    const size_t frames = min(recordedFrames - offset,
                              static_cast<size_t>(READ_CHUNK_FRAMES));
    for (size_t i = 0; i < frames; ++i) {
      outputChunk[i] =
          static_cast<int32_t>(recordBuffer[offset + i]) * PLAYBACK_GAIN_Q16;
    }
    if (!writeAll(outputChunk, frames * sizeof(int32_t))) {
      return false;
    }
    offset += frames;
  }

  // 写少量静音，避免功放下电时产生尾音。
  for (size_t i = 0; i < READ_CHUNK_FRAMES; ++i) {
    outputChunk[i] = 0;
  }
  for (int i = 0; i < 4; ++i) {
    writeAll(outputChunk, sizeof(outputChunk));
  }

  Serial.println("回放结束。能清楚听到刚才的声音，麦克风和音频链路基本正常。");
  return true;
}

static void runFullTest() {
  if (recordAudio()) {
    playLastRecording();
  }
  Serial.println("发送 r 重新录音，p 重放，s 显示统计，h 显示帮助。");
}

static void printHelp() {
  Serial.println();
  Serial.println("命令: r=录音并回放  p=重放  s=统计  h=帮助");
  Serial.println("录音时将麦克风靠近嘴部 10~20 cm，先说话，再停顿 1 秒。");
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println();
  Serial.println("==========================================");
  Serial.println(" ESP32-S3 麦克风录音 / 回放测试");
  Serial.println("==========================================");

  microphoneReady = initMicrophone();
  if (ENABLE_PLAYBACK) {
    speakerReady = initSpeaker();
  }

  if (!allocateRecordBuffer()) {
    Serial.println("程序停止：录音缓冲区分配失败。");
    return;
  }

  if (microphoneReady) {
    Serial.println("提示：先安静约 0.3 秒，随后正常说话，最后停顿约 1 秒。");
  }
}

void loop() {
  static bool firstRun = true;
  if (firstRun) {
    firstRun = false;
    if (microphoneReady) {
      runFullTest();
    }
  }

  if (!Serial.available()) {
    delay(5);
    return;
  }

  const int incoming = Serial.read();
  if (incoming == '\r' || incoming == '\n' || incoming < 0) {
    return;
  }

  switch (tolower(incoming)) {
    case 'r':
      runFullTest();
      break;
    case 'p':
      playLastRecording();
      break;
    case 's':
      printStats(lastStats);
      break;
    case 'h':
      printHelp();
      break;
    default:
      Serial.println("未知命令，发送 h 查看帮助。");
      break;
  }
}
