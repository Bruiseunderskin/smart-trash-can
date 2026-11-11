# 實驗與測試證據範本

讀者：實作者及審查者。何時讀：運行 host、target 或實機測試前。複製一份填寫；空欄代表未知，不代表通過。

## 可重現資訊

- 測試 ID、日期、操作者、目的：
- git commit、dirty 狀態、submodule 版本：
- 指令、工作目錄、exit code、原始 log 路徑及 SHA-256：
- OS、Python／編譯器／ESP-IDF／runtime 版本及 target：
- 證據層級：mock／host reference graph／host 實際目標產物／target build／target 執行／實際物理效果：
- 模型路徑及 SHA-256、labels 與 preprocess hash、後端及版本：
- 真實／合成標記、資料集用途、manifest hash、來源授權、樣本數及類別：
- fixture 原始／壓縮 hash、尺寸、pixel format、stride、解壓及前處理命令：
- golden 來源、獨立生成方式、預期 tensor hash、輸出和容差：
- 板型、接線、供電及測量器材（如適用）：

## 結果

- 每案例 ID、truth、prediction、confidence、correct、錯誤或拒判：
- 測試總數、通過／失敗／跳過；失敗 log：
- 時間：時計來源、開始／結束定義、每次樣本、計算方式、p50／p95：
- 未測項、未解決假設、結果適用範圍：

不要以模擬或 replay 時間報告實板延遲；不把 mock 推理視為模型結果，不把 ACK 視為實際蓋到位。
