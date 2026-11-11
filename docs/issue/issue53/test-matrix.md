# 測試矩陣與證據規格

讀者：測試撰寫者及審查者。何時讀：制定每個階段的自動測試和證據時。此文件是測試設計，非已執行結果。

| ID | 層級／輸入 | 期望 | 證據及阻塞 |
|---|---|---|---|
| T01 | label 四個合法 index | 映射 `{1,2,3,0}` | unit test；涵蓋邊界及未知 index。 |
| T02 | 缺少、重排、額外或未知 label | 初始化拒絕 | 含錯序 fixture 的 unit test；不可落入 other。 |
| T03 | 真實幀、記錄 raw hash | 輸出符合已核准 reference tensor hash | 必須有來源、轉換命令、hash、色彩／resize 規則；200 KB 衝突待決。 |
| T04 | 0、1、2 幀有效；3、4、5 幀有效 | 少於 3 回 IO；其餘按有效幀聚合 | mock clock 和注入錯誤；每條路徑釋放相機。 |
| T05 | scan timeout 發生於開啟、擷取、推理 | timeout 並清理 | mock clock；不聲稱硬件 timing。 |
| T06 | 聚合 confidence 低於、等於、高於 0.55 | 小於拒判；等於及高於依契約繼續 | mock logits 或 probabilities 明確標識；真模型結果另報。 |
| T07 | 模式所有狀態 × 所有輸入 | 依轉移表；非法輸入無副作用 | 表格驅動測試；BOOT 與最後回合接口矛盾先解決。 |
| T08 | 回合中長按 100 次；隨機 10000 步 | 不進 CHAT；不變量成立 | 固定 seed、保存失敗 seed 和 hook 計數。 |
| T09 | 五回合、答對、答錯、分數下限、重試、逾時 | 依 config 和 phase 轉移；唯遊戲改分數 | fake clock；區分顯示分與 raw score；180 秒結束時完成本回合。 |
| T10 | 遲到的影像／投放／ACK，round 已變 | 不影響新回合 | deterministic queue 測試；須先定義 round identity。 |
| T11 | UI 各狀態與錯誤、callback | payload 符合現有 UI 轉移；只發意圖 | host 接線測試及三張人手查看的 SDL render。 |
| T12 | 正常與不完整 CDC chunk、壞 CRC、垃圾、連續兩幀 | 正確重組、拒絕壞幀、可重同步 | 固定 wire vector；兩端共用已核准 byte order。 |
| T13 | ACK 遺失、重送、遲到、回繞、重連 | 同序同命令不重複執行，結果與 fault 清楚 | mock clock；reset/session 和未知實物狀態須先決定。 |
| T14 | 無節點、過期感測、queue 滿 | 可見 fault，按已決定的降級政策運作 | 不把 loopback 當實物開蓋；未知安全狀態不可默認成功。 |
| T15 | 八個真實類別案例及兩個真實低信心案例 | 逐例驗狀態、分數、命令 | 核實素材及可執行推理；不夠案例時明報 blocked。mock 不能代替真模型案例。 |
| T16 | 斷網實機、四桶、相機、觸控及計時 | 按 issue 的硬件驗收 | 原始 log、設備、版本、樣本、失敗次數、p50/p95；未測不填。 |

## 測試資料界線

建立獨立 manifest：資料 ID、來源、取得日期、授權、原始 hash、轉換步驟、用途及 model/runtime hash。開發集和最終 field-probe 分開；禁止以 field-probe 選閾值、校準或反覆挑選開發案例。若污染已發生，記錄並停止聲稱它是未接觸的獨立測試。

## 驗證層級

分別標記 unit/mock、host reference-model、host 真實目標模型（只在已證實可執行時）、target build、target 執行、實體效果。任何較低層級通過都不能推導較高層級通過。
