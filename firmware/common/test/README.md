# firmware/common/test — host 單元測試

讀者：跑測試與改模組的人。何時讀：加測試或看 CI 紅燈前。

```bash
cmake -S firmware/common/test -B build/common-test -DCMAKE_BUILD_TYPE=Debug
cmake --build build/common-test -j
ctest --test-dir build/common-test --output-on-failure
```

只依賴 libc 與 gcc；不需要 LVGL、ESP-IDF 或硬件。2026-11-11 在本分支重跑為 18/18 passed。

| 測試 | 驗什麼 |
|---|---|
| `test_contract_ui` | `contracts/stc_ui.h` 的列舉次序與 payload 佈局 |
| `test_classes_order` | `classes.json` 次序 == `models/wordless-unknown/labels.txt`，且 bin 值正確 |
| `test_label_map` | 模型 index 固定映射到桶位 `1,2,3,0,4`，未知／錯誤標籤拒絕 |
| `test_preprocess_golden` | UYVY→RGB、中央裁切、160×160 前處理 golden hash |
| `test_mode_gate` | GAME／CHAT 模式閘與閒置退出 |
| `test_game`、`test_game_spec` | 五回合、計分、超時、拒判、重複事件、舊回合與 180 秒邊界 |
| `test_e2e` | 十個單回合正常／拒判案例 |
| `test_app_wiring` | snapshot／view → UI learning payload |
| `test_lid`、`test_lid_retry`、`test_node_offline` | 舊 CDC 蓋介面與可靠性回歸 |
| `test_hand_protocol` | 16-byte hand v2 佈局與 CRC |
| `test_hand_link` | ACK callback、完整回合關聯、重試、離線與 DROP 去重 |
| `test_game_controller` | 假視覺／假手部／假語音／假 UI 的完整五回合 |
| `test_replay_pack` | replay pack 邊界與 CRC |
| `test_vision_aggregate` | 五幀平均、top-two、agreement、abstain |
| `test_virtual_hand` | S3 ACK、虛擬 DROP、重送、去重與 pattern |

新增行為測試先以 RED 確認失敗，再修改 production code；本次 ACK 接線和回合 ID 測試已依此流程驗證。
