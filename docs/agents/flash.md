# 燒錄與驗收（給隊友）

讀者：拿板子要即刻燒來試的人。
何時讀：拿到 S31 Korvo-1、要驗證這一版 UI 與綠豆接駁時。

## 1. 燒 S31 遊戲機

```sh
cd firmware/xiaozhi-esp32
idf.py set-target esp32s31
idf.py build flash monitor
```

`idf.py` 用 ESP-IDF v6.1（CI 用同一版）。第一次 build 會拉 managed components，需要網絡。

## 2. 開機應該見到

1. 頂欄「分類小隊｜準備好」，中間 56 px 標題、左邊相機卡、右邊「目前分數」。
2. 按「開始挑戰」→ 掃描頁。
3. 掃描頁**冇掣**：頂部綠色條 3 秒縮短，自動開始掃描。
4. 結果頁：112 px 類別名、頂欄「連對 N 次」＋「完成」、相機縮圖、類別圖示、分數、提示、底部「等待開蓋」pill。
5. 蓋閂上後頂部綠條倒數，完結自動入下一回合。
6. 總分頁按「問綠豆」→ 切去 xiaozhi 語音（離線時頂欄顯示「暫時離線」）。

## 3. 已知未驗

- 自動掃描未有實機驗證：controller 只在 `STC_PH_PROMPT` 接受 `STC_ACTION_SCAN`；冇卡時會走模型拒判，畫面停喺重試。
- 綠豆 prompt 只在文件層（`docs/agents/greenbean-system-prompt.md`），xiaozhi 的 LLM 端要自己貼入 system message。
- 四類 glyph 是 96 px 資產；`bin_choice` 用 80 px 顯示。

## 4. 出事時

| 現象 | 查 |
|---|---|
| 開機黑屏 | `idf.py monitor` 看 `stc_ui_port` / `StcShellUi` log |
| 字變豆腐格 | 字型子集缺字；重跑 `tools/lvglpro_preview/gen_category_icons.py` 以外，字型要用 lv_font_conv 重出（見 CHANGELOG 2026-11-21 條目） |
| 問綠豆冇反應 | 確認 build 咗 xiaozhi 板 `esp32-s31-korvo-1-stc`；`stc_shell_ui_request_chat()` 只在此板存在 |
| CI 紅 | `gh pr checks 74`，第二欄唔係 `pass` 即紅 |

## 5. ESP-IDF 6.1 與 VS Code 版本切換

`esp32s31` 仍是 preview target，只有 ESP-IDF 6.1 支援。本機安裝位置與設定如下（macOS Apple Silicon）：

```sh
git clone --depth 1 --branch v6.1 https://github.com/espressif/esp-idf.git \
    ~/.espressif/v6.1/esp-idf
cd ~/.espressif/v6.1/esp-idf
env -u IDF_PYTHON_ENV_PATH -u IDF_PATH SSL_CERT_FILE=/etc/ssl/cert.pem ./install.sh esp32s31
```

建置時一定要 `--preview`，否則 `set-target` 會拒絕：

```sh
source ~/.espressif/v6.1/esp-idf/export.sh
idf.py --preview set-target esp32s31
idf.py --preview build
```

VS Code（ESP-IDF 擴充）在 `settings.json` 指定這三個鍵就會用 6.1：

```json
"idf.espIdfPath": "/Users/wed/.espressif/v6.1/esp-idf",
"idf.toolsPath": "/Users/wed/.espressif",
"idf.pythonInstallPath": "/Users/wed/.espressif/python_env/idf6.1_py3.14_env/bin/python3"
```

EIM 清單（`~/.espressif/idf-env.json`）要有一筆 `version: "6.1"`、`targets: ["esp32s31"]` 的紀錄，否則擴充的選擇器見不到它。
切回 6.0：把上面三個鍵改回 `~/.espressif/v6.0.2/esp-idf` 與 `python_env/idf6.0_py3.14_env/bin/python3`。

## 6. 在 esp-emu 模擬 S31（唔用板）

[espressif/esp-emulator](https://github.com/espressif/esp-emulator) 支援 ESP32-S31，可以驗證 flash 影像真的跑得起。

```sh
curl -fsSL https://raw.githubusercontent.com/espressif/esp-emulator/main/install.sh | sh
cd firmware/esp32s31/game
idf.py --preview build
idf.py --preview merge-bin -o /tmp/s31_merged.bin
esp-emu --chip esp32s31 --firmware /tmp/s31_merged.bin --psram-size 16M --timeout 60s
```

### GT1151 觸控模擬（令佢過到觸控 init）

BSP 要 GT1151（I2C 地址 0x14、16-bit register）讀到合法 product id 才繼續。用 emulator 的 regfile 模擬：

```sh
esp-emu --chip esp32s31 --firmware /tmp/s31_merged.bin --psram-size 16M \
  --i2c-dev "1:0x14,width=2,fill=0,reg=0x8140=0x31,reg=0x8141=0x31,reg=0x8142=0x35,reg=0x8143=0x31,reg=0x814A=0x00" \
  --exit-on "800x480 UI ready" --timeout 60s
```

**2026-11-21 實測**（`s31_merged.bin` 8,209,328 bytes）：

- ROM → bootloader → 16MB flash／16MB PSRAM → `app_main`。
- `gt1151: IC version: GT1151_…` → `Display initialized with resolution 800x480` → `stc_ui_port: 800x480 UI ready`，emulator 以 exit code 0 結束。
- 下一個 blocker：`ES8389: Open fail` → BSP `assert(dev)` → reboot。ES8389 在 0x20；`--i2c-dev` 的 regfile（width 1 或 2、fill 0 或 0xFF）都試過，仍有 135 次讀／寫失敗，唔夠模擬佢嘅 open 序列。
- 用一個**本機診斷版**（暫時跳過 speaker 初始化，不入 repo）再跑：`SELFTEST` 40 張卡在模擬的 S31 PIE 上跑完，`correct=39 total=40`（與實機已知 B-19 miss 一致）、`ESPNOW controller ready`、`VISION_TASK=READY`；`/dev/video2` 開不到（模擬冇相機）。即係模型推論同 ESP-NOW 初始化在 emu 都真的執行過。

`--psram-size 32M` 會卡在 `MSPI Timing: Enter psram timing tuning`；16M 可過。
