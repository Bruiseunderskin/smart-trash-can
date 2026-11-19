# firmware/common — 跨板共用模組

讀者：韌體整合人員。何時讀：新增或修改 `firmware/common/` 內任何模組前。

此目錄放 S31 與 S3 共用、且可在 host 編譯測試的純 C 模組。所有模組不得直接呼叫硬件，硬件只經 `stc_frame_source.h` 與 `stc_lid.h` 兩個介面進入。

| 檔案 | 責任 |
|---|---|
| `stc_vision_map.c/.h` | 模型 index → 桶位（固定表，啟動時核對 `labels.txt`） |
| `stc_frame_source.h` | 幀來源介面（real／replay／stub） |
| `stc_preprocess.c` | 圖轉換與前處理（UYVY→RGB、center crop、resize） |
| `stc_mode.c` | 模式閘（遊戲／聊天） |
| `stc_game.c` | 回合、計分、學習頁（唯一可改分數） |
| `stc_vision_aggregate.c` | 五幀 softmax 聚合、top-two、agreement、abstain |
| `stc_replay_pack.c` | 內嵌 replay pack 邊界、CRC 與固定 playlist 解析 |
| `stc_hand_msg.c` | S31／S3 ESP-NOW 16-byte 訊息與 CRC |
| `stc_hand_link.c` | S31 command ACK、重試、DROP 去重與節點離線 |
| `stc_virtual_hand.c` | S3 虛擬手部 DROP pattern／delay／ACK 狀態機 |
| `stc_lid.h`、`stc_lid_loopback.c` | 蓋控制介面與回環實作 |
| `stc_time.h` | 可注入時鐘（測試用 mock） |
| `stc_lidmsg.h` | CDC 16 bytes 幀定義（兩板共用） |

測試：`test/`，用 CMake ＋ ctest 跑；每個新目錄都要有 `README.md`，否則 `tools/check_docs.py` 會失敗。
