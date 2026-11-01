# tflite_probe —「呢個模型跑幾快」測試

唔接相機、唔接舵機、唔寫 UI。只回答一個問題：

> 這塊板跑這個 int8 模型，一次要幾多 ms？

先在 **ESP32-S3-CAM** 上跑，兩日後 S31 到手再跑同一份程式對比。

## 一、放模型

```bash
mkdir -p main/models
cp /path/to/waste4_int8.tflite main/models/waste4_int8.tflite   # 檔名不可改
```

模型要求：

- 輸入 **int8**（full integer quantization，帶 representative dataset）
- 輸入尺寸建議 **160×160 或 224×224**
- 檔案 < 3 MB（S3-CAM 多數 4 MB flash 或 8 MB，仍未計 app）

如果手上未有自己的模型，可以先用任何 ImageNet 級 int8 分類模型（如 MobileNetV2 int8）做熱身測試，量到的 ms 只作參考。

## 二、編譯燒錄（S3-CAM）

```bash
idf.py set-target esp32s3
idf.py menuconfig     # 確認 PSRAM: SPI RAM config → Enable；Component config → ESP TFLite-Micro → Use ESP-NN = y
idf.py -p /dev/ttyUSB0 build flash monitor
```

> PSRAM 一定要開，否則 4 MB arena 會 alloc 失敗。若板子只有 2 MB PSRAM，把 `kArenaSize` 改成 2 MB 再試。

## 三、讀結果

monitor 會印：

```
I probe: model bytes: 1234567
I probe: arena: 4194304 bytes in PSRAM
I probe: arena used: 812345 bytes
I probe: input: type=9 dims=4 shape=[1,160,160,3]
I probe: RESULT runs=20 mean=123.45ms p50=120.11ms p95=131.02ms min=118.9ms max=140.2ms top1=0
```

**要記錄的四個數**

| 項目 | 點解要記 |
|---|---|
| `arena used` | 決定 S31 要預留幾多 PSRAM |
| `mean` / `p95` | 答辯用 p95，唔好只用平均 |
| `input shape` | 確認同訓練時一致（160 或 224） |
| `top1` | 全 0 輸入下冇意義，只證明輸出層讀得到 |

## 四、A/B 對比（重要）

同一塊板、同一個模型，只改一個開關：

```
Component config → ESP TFLite-Micro → Use ESP-NN optimized kernels  →  n
```

再跑一次。兩個數的差距 = esp-nn 幫你省了幾多，亦即是「唔用加速會慢幾多」的直接證據。這個對比可以直接放進報告，評審問「你點知效能」時就是答案。

## 五、S31 到手後

```bash
idf.py set-target esp32s31
idf.py build flash monitor
```

同一份 `probe.cc`、同一個模型，只換 target。如果 `esp32s31` 編譯不過（esp-tflite-micro 未明確支援），錯誤訊息本身就是結論——那就走 ESP-DL 路線。

## 六、這個測試證明 / 不證明什麼

**證明**
- 模型格式（int8 tflite）在這塊板跑得通
- 真實推理延遲（mean / p95）
- esp-nn 加速的實際收益（A/B 對比）
- PSRAM 需求

**不證明**
- 相機 pipeline 通不通（要 Phase 2 加 camera）
- 分類準確率（這個 probe 只餵全 0 輸入）
- S31 一定支援（要等 S31 編譯結果）

## 七、下一步

Phase 2：加 OV2640 取景 → RGB888 → int8 量化 → Invoke → 印 top-1 與信心值。
Phase 3：換成 4 類模型，用測試集 50 張逐張對比 PC 與板上的預測是否一致。
