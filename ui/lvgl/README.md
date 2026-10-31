# 分類小隊 · ESP32-S31 4.3 吋 LVGL UI

800×480 橫向、rotation 0、RGB565、LVGL 9.5。九種狀態、四類圖鑑、分類小百科、完成總分頁。

[Figma 可編輯設計](https://www.figma.com/design/g5J7a5DhDuX1UYucR6jitn) · [需求 issue #3](https://github.com/Bruiseunderskin/smart-trash-can/issues/3)

## 編譯桌面預覽

從 repository root 執行：

```sh
cmake -S ui/lvgl/preview -B ui/lvgl/preview/build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build ui/lvgl/preview/build --parallel 8
ctest --test-dir ui/lvgl/preview/build --output-on-failure
SDL_RENDER_DRIVER=software ui/lvgl/preview/bin/main
```

需要 CMake、C/C++ compiler、SDL2。預設讀取本 repo 已安裝的 `xiaozhi-esp32/managed_components/lvgl__lvgl`（9.5.0）。若未安裝，使用現有 LVGL 9.5 checkout，並在 configure 加 `-DLVGL_DIR=/absolute/path/to/lvgl`。不會自動下載、替換或修改 LVGL。

- `1`–`6`：原有待機、拍攝、辨識、結果、再試、錯誤。
- `7`：先建立完成示範，再開四類圖鑑；點分類卡進入該類小百科。
- `8`：完成示範後的可回收物小百科；`9`：完成／總分頁。
- `M`：切換 reduced motion；`R`：錯誤人手重試。
- 點「開始挑戰」會執行**示範流程**：拍攝 → 辨識 → 四類輪流結果。資料是固定 fixture，沒有實際相機、AI 或舵機操作。
- 首頁只提供「開始挑戰」。點結果頁「完成」查看總分；只有完成頁的「繼續學分類」可進入圖鑑。
- 小百科提供「返回圖鑑」及「返回主頁」，圖鑑提供「返回成績」及「返回主頁」。延伸學習頁不能直接重開挑戰；回主頁後才可開始新一輪。
- Tab／Enter 可操作按鈕，鍵盤焦點有深色外框。

## 自動評估 native 渲染

本 repo 附有更新版 `tools/skills/lvgl-preview`。同一版本亦已安裝到本機技能目錄。它以實際渲染證據完成交付，不再要求用戶回覆 OK／NO-GO。

```sh
python3 tools/skills/lvgl-preview/scripts/preview.py \
  --project-root "$PWD/ui/lvgl/preview" --width 800 --height 480 \
  --rotation 0 --color-depth 16 --dry-run
python3 tools/skills/lvgl-preview/scripts/preview.py \
  --project-root "$PWD/ui/lvgl/preview" --width 800 --height 480 \
  --rotation 0 --color-depth 16 --replace
ui/lvgl/tools/.venv/bin/python ui/lvgl/tools/prepare_eval.py
ui/lvgl/tools/.venv/bin/python tools/skills/lvgl-preview/scripts/evaluate_render.py \
  --project-root "$PWD/ui/lvgl/preview" --manifest render/render-manifest.json
```

`prepare_eval.py` 產生 fresh 原生測試、實際 glyph／文字邊界檢查、17 個 framebuffer 及 manifest。視覺檢視在截圖後獨立記錄於 `render/visual-review.json`，不能由截圖腳本自動填 PASS。已記錄的檢視只可重用完全相同的檔案或完整 RGBA 像素。Linux CI 明確使用 `--visual-review render/visual-review-linux.json`；已檢視的 Linux 基準圖保存在 `render/baselines/linux/`。它與 macOS 畫面的差異每個 RGB channel 最多為 1，屬色彩展開差異。兩個平台各自要求完整像素 hash 相同，不使用寬鬆差異門檻。

結果為 `PASS`、`FAIL` 或 `INSUFFICIENT_EVIDENCE`。PASS 直接完成 native UI 評估；視窗可繼續顯示，不影響完成狀態。工具不會向舊紀錄中的 PID 發送終止訊號。build／runtime 失敗仍會還原備份。人手回饋屬可選的獨立記錄，不是交付閘門。

[完整技能與評估格式](../../tools/skills/lvgl-preview/SKILL.md)。GitHub Actions 會使用固定 LVGL 9.5.0 source，在 Linux 重建、執行測試及比較實際渲染證據。通過不能推定硬體通過。

## 接入 ESP-IDF／現有 Korvo 顯示

`CMakeLists.txt` 可作為 ESP-IDF component。頂層 project 在 `include(project.cmake)` 之前加入：

```cmake
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../ui/lvgl")
```

路徑須相對該 firmware project 調整。依賴使用現有的 `lvgl` component，沒有第二份 LVGL、SDL、字型 TTF 或新顯示驅動。

在已有的 800×480 LVGL display 初始化後，用 `lvgl_port_lock()` 包住以下操作；parent 需是無 padding 的 800×480 專用根容器：

```c
#include "stc_ui.h"

lv_obj_t *root = lv_obj_create(lv_screen_active());
lv_obj_remove_style_all(root);
lv_obj_set_size(root, 800, 480);
stc_ui_t *ui = stc_ui_create(root, on_retry, controller_context);
stc_ui_set_action_callback(ui, on_action);
```

不要把 UI 加到現有小智聊天畫面的有 padding 容器，也不要同時讓兩套 UI 管理同一個 active screen。此交付沒有改動／燒錄 `xiaozhi-esp32` 板型；實體整合點須由 owning display/controller 選擇。

控制程式依序套用 payload：

```c
stc_payload_t p = {.state=STC_CAPTURING, .score=120, .streak=4};
stc_ui_apply(ui, &p);
p.state=STC_CLASSIFYING;
stc_ui_apply(ui, &p);
p.state=STC_RESULT_OK;
p.category_id=0;
p.confidence=0.93f;
p.lid_open=false; /* true 只能來自真實蓋狀態 */
stc_ui_apply(ui, &p);
```

完成後學習轉移固定為 `result_ok → summary → category_guide → learning`；`learning → idle` 返回主頁。`idle` 不能直接進入圖鑑／小百科／完成頁。

`labels.h` 唯一定義類別順序：0 可回收物／1 廚餘／2 有害／3 其他。`result_retry` 需要兩個不同有效 ID 及各自有限的 0–1 機率。`learning` 需要有效 ID。

## 狀態／控制程式責任

- 所有 API 只能在 LVGL task 或同一把 `lvgl_port_lock` 下呼叫。
- 操作 callback 只送意圖。控制程式確認後呼叫 `apply` 或 `show_guide`／`show_learning`／`show_summary`；UI 不樂觀切頁。
- 控制程式在接收資料的邊界核對 `round_id`、目前工作及事件次序，丟棄舊推理結果後才套用。純 UI component 不解碼 SSE，也不提供工作排程器。尚未接上的 live backend 不在 native 驗證範圍。
- Start 在控制程式接受新狀態前去重。控制程式拒絕開始時應送出明確 error，不能靜默留在 idle。
- 每次 error 清除重試確認。按重試只確認本次 error，callback 每次錯誤最多一次；控制程式確認裝置可用後才送 idle。無 callback 時只記錄確認，不自動恢復。
- Callback 可同步 `apply` 或更新 images；在 callback 返回前不要 destroy UI。若採用 queue，複製 payload 及 error_code，不可保留 stack 字串。
- 先 `stc_ui_destroy` 再刪 parent。若 parent 已被刪，handle 會變 inert；仍需 destroy 一次以釋放 context。
- 圖片 descriptor 與 backing pixels 由 caller 擁有，直到替換、清除或 destroy。相機請採 double buffer，持鎖更新，不可與繪製同時寫 buffer。重用 descriptor 時 setter 會丟棄對應 image cache。
- `cutout=NULL` 使用 original；兩者皆無顯示佔位。結果頁保留獨立縮圖與類別圖示。
- 總分／連對只讀取 controller 值；不把 AI 信心值當成學習準確率，也不自行加分或宣稱答對。
- 一般分數 80 px；超過五位數用 32 px 完整顯示。超長連對數用 26 px，不截斷。完成頁完整顯示兩個 uint32 計數。

## 統一間距

`design/spacing-policy.json` 與 Figma 的 `space/*` variables 定義：左右留白 24 px、一般卡片內距 24 px、緊湊卡片內距 16 px、圖示／文字間距 24 px、段落／格線間隔 16 px、文字堆疊間隔 8 px。底部按鈕及提示列皆為 64 px 高。兩張分數卡使用相同的 24 px 內距。

`validate_design.py` 會檢查所有狀態的底列、分類卡圖文距離、分數卡留白及延伸學習入口。這些幾何檢查需配合實際 framebuffer 截圖。

## 設計同步／資產重用

`design/tokens.json` 及 `design/layout.json` 由 Figma 實際讀回。`design/export_figma.cjs` 是 MCP 匯出腳本。更新 Figma 後重匯出 tokens/layout，再生成 C。

```sh
npm ci --prefix ui/lvgl/tools --ignore-scripts --no-audit --no-fund
uv venv ui/lvgl/tools/.venv
uv pip install --python ui/lvgl/tools/.venv/bin/python -r ui/lvgl/tools/requirements.txt
ui/lvgl/tools/.venv/bin/python ui/lvgl/tools/generate.py
python3 ui/lvgl/tools/validate_design.py
```

使用官方 `lv_font_conv` 1.5.3、FontTools 4.68.0、Pillow 13.0.0、resvg 2.6.2。來源 SHA-256 記錄於 `generated/manifest.json`。完整 TTF 僅供生成，不編入 firmware；字型子集及 48／96 px icon 是 C 陣列。單一 C 原始檔的文字大小不等於板上二進位資產大小。

- 類別／camera／trophy：重用 repo `output/infographic-research/.../assets/tabler` 或其 upstream outline；MIT 授權在 `design/assets/icon/LICENSE-Tabler.txt`。
- 字型：重用本機已有 Noto Sans TC／Inter，OFL 授權在 `design/assets/fonts/`。
- Figma 按鈕：Simple Design System 的 Button variant；在 Figma 保留 instances，LVGL 使用原生 button 行為。
- 教學提示與參考產品來源在 `design/content.json`、`design/issue-3.md`。

## 驗證與界線

`preview/render/` 保存實際 LVGL framebuffer 截圖、逐狀態及四類預覽、對比檢查報告。原生預覽不證明 ESP32 編譯、RGB timing、touch mapping、相機 buffer、推理、舵機、安全開蓋、flash/RAM headroom 或小朋友學習成效。

Figma 與 LVGL 有已知 raster 差異：文字採同一字型的 4bpp 子集，字面與基線會受 FreeType／LVGL rasterizer 影響；原生鍵盤焦點額外顯示外框。數據 120／4 是預覽 fixture；啟動預覽的初始分數為 0。

## 下載交付包

[`dist/smart-trash-can-lvgl-800x480.zip`](dist/smart-trash-can-lvgl-800x480.zip) 包含受版本控制的 UI、設計、授權、截圖及 portable skill。可在 repository root 執行 `python3 ui/lvgl/tools/package_release.py` 重建，不會打包 build、venv、node_modules 或本機 process state。

原規格的 1080p／Sketch／實機整合由 [後續 issue #4](https://github.com/Bruiseunderskin/smart-trash-can/issues/4) 跟進。
