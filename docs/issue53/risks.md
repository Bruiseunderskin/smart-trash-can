# 衝突及待決記錄

讀者：實作者、owner、硬件持有人。何時讀：依賴有關數值或介面之前。以下為分析與建議，並非覆蓋 issue 的新決定。

| 項目 | 衝突／證據 | 建議及阻塞範圍 |
|---|---|---|
| 原始幀大小 | 640×480 UYVY 每像素 2 bytes，單幀 614,400 bytes；issue 又要求所有 fixtures ≤200 KB。 | 使用可重現的無損壓縮真實幀並校驗解壓 hash，或調整上限；先取得 owner 對規格變更的決定。阻塞 I1 golden fixture。 |
| CDC 幀長 | 正文 6 個單 byte 欄位 + 8 bytes ToF + 2 bytes CRC = 16 bytes；早期 comment 說 14。 | 按較新正文的 16 bytes 設計，明確 endian、CRC 測試向量；保留 comment 作歷史。 |
| 電流推論 | issue 稱 355 mA 超過 500 mA，算術不成立；估算亦不等於量測。 | 不依此宣稱安全；按實際供電、峰值、線材和容限量測。Wi-Fi 關閉仍是既定要求。阻塞硬件安全結論。 |
| 推理後端 | 指定 S31 ESP-DL 產物與 host replay、softmax golden；issue 未證明 host 能執行同一產物。 | 確認支援的後端及模型等價性，記錄模型 hash、前處理、量化和容差。不可用假輸出冒充推理；阻塞依賴真推理的 I1/I5。 |
| 拒判樣本 | 固定閾值及兩個低信心案例未提供來源；field-probe 不可用於閾值選擇。 | 固定閾值後在獨立資料上報告；人工構造分數只測邏輯並標為 mock，不當模型表現。 |
| 重試副作用 | ACK 遺失可能令相同命令重複執行；seq 回繞及重連亦需定義。 | 定義重複命令去重、ACK 行為與重連 epoch；先在 mock 測試。不要聲稱實蓋到位。 |
| 啟動錯誤 | issue 同時要求每步失敗進 FAULT，及節點失敗退 loopback 不阻開機。 | 區分致命錯誤與可降級 fault flag，明確 UI 和開蓋禁用策略，再實作。 |
| 實際螢幕和供電 | issue D5 與現有 AGENTS 文字不同；供電估算未附板級量測。 | I0 按決定更新專案文字，實際型號、連接和供電仍須由持有人核對。 |
| 來源分支描述 | issue 一處容許按檔參考 #41，另一處禁止搬檔。 | 以 main 為基準重新實作，歷史分支僅供理解；衝突由 owner 定序。 |

## 獨立意見：Poe GPT-6-Astra

2026-11-08 請 Astra 依公開 issue 審視上述衝突；其明言沒有檢查 repo、執行測試或驗證硬件。以下為建議，須按實際代碼核對，並不構成 owner 決定：

- 壓縮不保證真實相片可壓到 200 KB；先量實際素材。可定義上限為 committed compressed bytes，並驗解壓後長度和 hash。
- CDC 是 byte stream，parser 需處理分段、合併、垃圾和 CRC 錯誤後重同步；16 bytes 的前 14 bytes 是 CRC 覆蓋範圍，後 2 bytes 是 CRC。以固定 wire vector 測試序列化。
- 355 mA 若是 S3 單獨峰值，再加其餘 170 mA 才是 525 mA；此解釋未核實，須界定電流量測邊界和 inrush。
- 前處理 golden 在 I1 要求、實作在 I2：先取得獨立 reference 及 fixture，再在 I2 驗實作；不得用同一實作產生與驗證期望值。
- 以平均後 burst confidence 判定拒判；`< 0.55` 意味等於 0.55 不拒判。拒判 policy mock 與真正模型 replay 分開。
- 重試同一 seq 和內容；節點收到重複命令重發 ACK 而不重執行，並定義 reset、回繞、遲到 ACK 和重新連線。ACK 遺失時實際動作未知，不能推斷蓋未開。
- 清楚安排 voice stub/本地 PCM、實際推理和 target frame source 接線的階段；獨立相機 bring-up 本身不證明整條路徑。
- 定義 ToF stale、去抖、校準、重複投放和每回合一次計分；定義過期視覺結果和蓋命令如何取消。
- 定義非致命節點故障時的可見提示及操作限制；loopback 成功不可當成實物投放或成功開蓋。
