# 測試 fixtures

讀者：改前處理或測試的人。何時讀：更新 golden vector 前。

- `frame_640x480.uyvy`：640×480 UYVY，由 `data/mixed-card-object/wordless-v1/test-scenes/A-03_000.jpg` 轉出（BT.601 有限範圍）
- `expected_rgb160.sha256`：經 UYVY→RGB→中央正方形→bilinear 160 後的 tensor 的 SHA-256

更新方式：`ctest -R test_preprocess_golden` 首次執行會寫 hash；只有在**故意**修改前處理時才可用
`test_preprocess_golden --update`，並在 PR 說明原因。
