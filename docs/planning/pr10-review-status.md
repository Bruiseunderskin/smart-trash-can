# PR #10 審查狀態（2026-11-02）

以下核對 PR #10 的程式與本機產生物；GitHub 上未解決的 review thread 不代表這些核對已獲審查者確認。

| Review concern | 現況和證據 |
|---|---|
| 下載模式缺少 metadata | CLI 現名 `--download-images`；help 和錯誤訊息要求預先取得 `classname.txt`、`train.csv`、`val.csv`，只擷取選定圖像，不承諾完整下載。 |
| 缺少來源追溯檔 | `write_provenance()` 產生 `labels.csv`、`mapping.csv`、`report.md`。 |
| 細類名稱無大類 prefix | 本機 ModelScope `classname.txt` 265 個名稱經 `map_major_class()` 全部匹配；這只覆蓋該 metadata，不代表任意資料集都支援。未知類別會停止。 |
| EI probe 嵌入符號 | 本次 IDF 6.0.2 build 的 `waste4_int8.tflite.S` 實際產生 `_binary_waste4_int8_tflite_start/end`，與宣告一致，且已成功 build、flash 和 Invoke。其他 IDF 版本仍須驗證。PR #9 自己的 probe 不因此自動修復。 |

#6 指定 MobileNet 在兩種 kernel 設定均未通過 AllocateTensors；使用 person-detect 的 A/B 數字不能代替指定模型。#7 的測試與匯出已記錄；#8 的 S31 量化主機模擬通過原定數字門檻，但資料來源重複及近重複限制仍在，不能當成獨立產品驗收。S31 實板、三秒整機、屏幕、斷網及十次循環仍待測。
