# Field probe set（獨立現場探測集）

> 狀態：歷史資料，保留原路徑及下列用途限制；未用於本次新模型訓練、校準或模型選擇。RealWaste 亦曾用於混合模型訓練，不能宣稱該模型的來源域獨立。

17 張圖，四個香港分類桶。**用途只有一個：測試現有模型在「非 garbage265 風格」圖片上的表現。**

## 鐵律

- ❌ 不參與訓練、校準、閾值掃描、模型選擇
- ❌ 不因為在這裡表現差就去調 threshold
- ❌ 不用來宣稱「現場準確率」（每類只有 2–8 張，統計上無意義）
- ✅ 用來回答一個問題：**同一個模型，換一個資料來源，會跌幾多？**

結果要寫成 per-image 表格（truth / prediction / confidence / 對錯），唔准只報一個總數。

## 為甚麼要這一批

issue #12 的 SC101IOT 現場測試只有 4 張，而且全部由同一部相機、同一輪拍攝。4 張可以否決「現在可以量產」，但不足以說明錯在哪裡。

這批圖的價值是：**來源與 garbage265 完全不同**，而且有真實地面、真實光線、真實陰影：

| 來源 | 內容 | 授權 | 數量 |
|---|---|---|---|
| [RealWaste](https://archive.ics.uci.edu/dataset/908/realwaste)（UCI #908，鏡像 `shahzaibvohra/realwaste`） | 垃圾接收站實拍的單件廢物，524×524 | CC-BY-4.0 | 14 |
| [`viola77data/recycling-dataset`](https://huggingface.co/datasets/viola77data/recycling-dataset) | 電池類 | Apache-2.0 | 3 |

引用：Single, S. & Cook, D. (2023). *RealWaste: A novel real-world waste dataset*. UCI Machine Learning Repository.

## 分佈

| 桶 | 張數 |
|---|---:|
| 可回收物 | 8 |
| 廚餘 | 4 |
| 有害垃圾 | 3 |
| 其他垃圾 | 2 |

分佈不均，**唔准用 macro-F1 比較**，只准逐張睇。

## 標註注意

1. **RealWaste 的 `Miscellaneous Trash` 唔等於香港的「其他垃圾」。** 它指該回收站不收的東西，當中有金屬。原圖 `miscellaneous-trash_91.jpg`（金屬條）已剔除，因為它應屬可回收物。
2. `miscellaneous-trash_33.jpg`（乾果殼）標為「其他垃圾」有爭議，可視為廚餘。判錯不算模型錯，分析時要標出來。
3. `vegetation_4.jpg` 是整條青瓜、`food-organics_9.jpg` 是完整蔬果 —— 完整食物在香港屬廚餘，但與「廚餘」訓練類（多為殘渣）不同分佈。
4. 電池圖來自另一個資料集，底色與 RealWaste 不同（白底／產品照風格）。**這三張同時是「studio 風格」對照組** —— 如果模型只在電池那三張高分，就證明它靠背景而非物件。

## 檔案

`manifest.json` 逐張列出 `bin_hk`（香港桶）、`source_class`（原始資料集類別）、`sha256_16`、備註。
圖片按桶分目錄存放，檔名保留原始編號以便追溯。
