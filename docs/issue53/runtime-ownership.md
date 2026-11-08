# 執行時責任與失敗處理清單

讀者：整合者及 reviewer。何時讀：接線、取消、錯誤恢復或分配 target/host 後端之前。未決格子不可作為已批准行為。

| 能力 | 契約／來源 | 規劃責任 | 狀態及阻塞 |
|---|---|---|---|
| mode 與遊戲 | `contracts/stc_mode.h`, `contracts/stc_game.h` | I3；唯一 mode 寫入者／score 寫入者 | 模式事件缺口 D03；蓋行為 D01 |
| vision 與 inference | `contracts/stc_vision.h`, `contracts/model/preprocess.json` | I1/I2；每次掃描取得、處理、釋放 | `.espdl` host backend D05；target 接線仍須明確 |
| UI | `ui/lvgl/include/stc_ui.h` | I4 adapter、LVGL task 或鎖；呼叫者保留圖片記憶體 | enum 衝突 D02；payload 來源未全定 |
| lid／sensor | `contracts/stc_bin.h`，issue §6.4 | I5 loopback；I7 CDC、ToF | 去重及安全恢復 D06/D07；不能以 loopback 表示真實開蓋 |
| voice | `contracts/stc_voice.h` | 指派 PCM queue、模式切換及 stub 後端工作 | 實體 codec／speaker 不在本階段；階段與錯誤 policy 待定 |
| target camera | `firmware/esp32s31/bringup/main/t2_camera.c` | I6 單獨 bring-up | 不能當成 integrated `stc_frame_esp` 已實作 |

## 資源與故障逐項核對

對每步記錄呼叫 task、可否阻塞、deadline、持有資源、成功／失敗／取消時釋放、error code、模式、UI、重試及恢復條件。至少包括：相機打開／擷取／推理／超時、queue 滿及 stale round、模型契約錯誤、節點離線、壞 CRC、ACK 遺失、ToF 無效及 voice 失敗。現有 `stc_err_t` 沒有 `E_MODEL_LABELS` 專用 enum；公開錯誤映射與診斷文字須先決定，不應捏造現有符號。
