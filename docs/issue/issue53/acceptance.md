# 驗收與證據清單

讀者：實作者、審查者、硬件持有人。何時讀：每個階段交付和上板測試時。

## Host 和編譯

- [ ] 執行 `python3 tools/check_docs.py`、`python3 tools/check_mode_gate.py`、`python3 -m unittest discover -s tools/tests -v`，保存結果；任何失敗停止並報告。
- [ ] 執行各階段針對性 CTest，留命令、commit 和結果；驗映射、拒判、遊戲所有權、模式閘、前處理、CDC 及重試。
- [ ] 逐張記錄 replay 的資料來源、truth、prediction、confidence 和 correct；不把獨立測試集用於調參。
- [ ] 保存 SDL 三張畫面並人手檢查；CTest 通過不能取代視覺審查。
- [ ] 記錄 S31 和 S3 的 build、工具鏈及配置；核對節點 Wi-Fi 關閉。
- [ ] 檢查契約 diff、CHANGELOG、無網絡的 GAME 路徑及未完成事項。

## 實機持有人

- [ ] 記錄板型、韌體 commit、模型 hash、供電與接線。
- [ ] 留下相機 RESULT 原始 log、有效幀及 fps；檢查屏幕和觸控。
- [ ] 記錄 CDC 枚舉、四路 ToF、每個蓋實際動作；發送命令或 ACK 不證明到位。
- [ ] 斷網連續測試 10 次，包含失敗紀錄；保存各階段原始時間 JSON，計算 p50/p95。
- [ ] 記錄供電與峰值實測；舵機使用獨立電源並共地。未測不報安全裕度。

每條證據附日期、設備、版本、樣本及原始 log 路徑；尚未執行標「未測」。
