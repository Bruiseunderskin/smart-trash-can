# ESP32-S31 後續實板路線

目前完成的板端證據來自 ESP32-S3。S31 板在隊友手上，以下是待執行步驟，並非已通過的結果。

1. 記錄 S31 開發板確實型號、Flash／PSRAM、相機、4.3 吋屏和馬達驅動器。依板廠資料接線，不沿用 S3 引腳猜測。
2. 為 S31 使用獨立 ESP-IDF build/config；先以 Espressif S31 支援的 IDF 版本跑基本範例。官方入門文件列出 `idf.py set-target esp32s31`，此命令會清理並重建現有 build/config，應在獨立專案使用。來源：[ESP-IDF 6.1 S31 入門](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s31/get-started/linux-macos-start-project.html)。
3. 以現有 YOLO11n-cls ONNX 權重和訓練集 calibration，使用 ESP-PPQ `--target esp32s31` 匯出 S31 `.espdl`。本次候選為 1,796,864 B，SHA-256 `05361838193d2edf9d32ce616c92d926b08036d53090fa2d6f89a6605b8a168c`；模型不在 Git repo，須透過合適渠道交給隊友並核對 hash。不要使用 S3 量化產物代替。來源：[ESP-DL 量化教學](https://docs.espressif.com/projects/esp-dl/en/latest/tutorials/how_to_quantize_model.html)。
4. 先建立 ESP-IDF + ESP-DL 模型 probe，確認模型載入、輸入尺寸／量化、輸出類別順序、記憶體及模型自測；每次重複推論前重載 tensor。用四類已知實圖作 smoke test，再用獨立、按物件及拍攝場次分組的現場測試集量逐類準確率。記錄 CPU、PSRAM、build 設定和模型 hash。
5. 比較同一 S31 上至少 20 次模型 Invoke 的 mean、p50、p95 及峰值 RAM；記錄取像和前處理時間。這些數字不能替代拍照到蓋子全開的端到端計時。
6. 整合 ESP-IDF 本機狀態機、LVGL 4.3 吋畫面、舵機互鎖、離線分類和本機預錄粵語；有網絡時接 XiaoZhi 相容 ASR → LLM → TTS。驗證斷網時遊戲仍可操作，對話不可用時清楚提示。
7. 用真實裝置記錄拍照觸發到蓋子全開的分段和端到端時間，測足夠次數估計 p95 ≤ 3 秒；另跑至少連續十次完整拍攝、分類、開蓋及復位。十次循環與可靠的 p95 估計是不同驗收項。測試屏幕動畫、分數、學習內容和粵語聲音。

EI v1 `.tflite` 可作對照，但需確認所選 S31 IDF、TFLM component 和算子組合可編譯並在實板測試；EON v2 是生成 C++，不能當 `.tflite` 或 `.espdl` 直接載入。模型二進位、私人圖片及憑證不提交。
