# 嵌入式 UI 研究：ESP32-S31-Korvo-1 怎麼做最方便（2026-11-03）

狀態：**研究稿**。作者：阿吉 the hermes。
問題來源：sillysben —「喺 ESP32 S31 上面點樣做介面會方便啲，官方好似有支持」。

---

## 0. 三層結論

| 層 | 問題 | 答案 |
|---|---|---|
| **顯示層** | 點亮 800×480，不想自己搞 RGB 時序 | **官方 BSP**：`espressif/esp32_s31_korvo_1` v1.1.0（target `esp32s31`，IDF ≥ 5.4），內含 LVGL、`esp_lvgl_port`、GT1151 觸控 |
| **製作層** | 由 Figma 到板 | **LVGL Pro + Figma 插件**（tokens／components／screens → XML → 純 LVGL C）；或維持現有 C 手寫 + SDL 預覽 |
| **設計層** | 畫面怎樣才算合格 | 四個硬指標：觸控 ≥ 85 px、主標 ≥ 48 px、對比 ≥ 4.5:1、每屏一個主行動 |

**最關鍵一條**：`esp32s31/bringup/main/t1_lcd.c` 內手寫的 `esp_lcd_rgb_panel_config_t`、時序、GPIO 對位，全部可由 BSP 取代；BSP 同時解決觸控與 LVGL 接駁。

---

## 1. 官方 BSP 詳情

| 項目 | 內容 |
|---|---|
| 元件 | `espressif/esp32_s31_korvo_1` · v1.1.0 |
| target | `esp32s31`（IDF ≥ 5.4） |
| 依賴 | `esp_lvgl_port ^2`（public）、LVGL 9、`esp_lcd_touch_gt1151`、`esp_codec_dev`、`esp_video`、`button`、`led_indicator` |
| 屏幕 | 800×480、RGB565、RGB 介面：PCLK 40／DE 43／HSYNC 44／VSYNC 45／DISP 38，DATA0 由 GPIO8 起 |
| 觸控 | GT1151（I²C：SDA 0／SCL 1） |
| 官方範例 | `display`、`display_lvgl_demos`、`display_lvgl_benchmark`、`display_camera_video`、`display_audio_photo`、`display_sdcard`、`display_usb_hid` |

### 1.1 接入步驟

```yaml
# main/idf_component.yml
dependencies:
  idf: ">=5.4"
  espressif/esp32_s31_korvo_1: "^1.1.0"
```

```c
#include "bsp/esp32_s31_korvo_1.h"

void app_main(void) {
    bsp_display_start();          // LVGL + 顯示 + 觸控
    bsp_display_backlight_on();
    bsp_display_lock(0);          // 之後所有 LVGL API 都要在鎖內
    lv_demo_widgets();
    bsp_display_unlock();
}
```

### 1.2 驗證次序

1. 先跑官方 `display_lvgl_demos`（不改自己程式，確認硬件無問題）。
2. 自家 bringup 叫 `bsp_display_start()` 出純色＋中文字（確認字型與無撕裂）。
3. 才把 `ui/lvgl` 接上去。

任何一步不過，就能定位是硬件、BSP 版本、還是自己的程式。

> 現時 `t1_lcd.c` 的註解自己寫着「TODO(verify)：時序必須對 ST7262E43 datasheet 覆核」。BSP 由 Espressif 對着實際板子維護，這類「量錯一個 porch 就黑屏」的風險直接消失。

---

## 2. 四條 UI 製作路線

| 路線 | 做法 | 成本 | 優點 | 代價 |
|---|---|---|---|---|
| **A · 現有 C 手寫 + SDL 預覽** | 直接寫 `stc_ui.c`；桌面用 SDL preview | 已投入 | 完全控制像素與記憶體；可入 CI | 每屏人手對位；設計改版要重寫 |
| **B · LVGL Pro + Figma** | Figma 設計 → 一鍵匯出 XML → 產 LVGL C | LVGL Pro 訂閱 | 真正一鍵；XML 可入 git／可 diff／可 CI | 要改 XML 工作流；綁 LVGL 9 |
| C · SquareLine Studio | 拖拽 → 匯出 C | 30 日試用；個人免費；商業訂閱 | 上手快、教學多 | 大量生成碼，與 `stc_ui.c` 打架 |
| D · EEZ Studio | 免費開源，LVGL 8／9 | 免費 | 零成本、可離線 | 社群細；複雜互動仍要寫 C |

**建議：A 為主，B 為升級路。** `ui/lvgl` 已有 SDL 預覽、CTest、生成式字型與 CI 檢查——「主機上跑、板上驗」的迴圈比任何設計工具值錢。但下一版 11 屏數量多，若要用 Figma 做稿再交人實作，B 最貼合，因為它輸出可審閱的文字檔而非黑盒。

### 2.1 LVGL Pro 的 Figma 流程

- Figma 裝「LVGL Flow for Figma」社群插件 → 一鍵匯出。
- 匯出：design tokens（顏色、字級、數值、樣式）→ `globals.xml`，加 components／screens 的 XML。
- LVGL Pro Editor 開 XML → 即時預覽 → 產出純 LVGL C（無額外 runtime）。
- XML 可入 git、可在 PR 內 diff、可進 CI。

---

## 3. 嵌入式 UI 硬指標（可由 Figma 直接套用）

4.3″ 800×480 實際尺寸 **93.7 × 56.2 mm**，即 **217 ppi**，**1 px ≈ 0.117 mm**。以下數字由換算得出。

| 指標 | 數值 | 依據 |
|---|---|---|
| 觸控目標 | **≥ 85 px（1 cm）**；按鈕建議 88–120 px 高 | NN/g 建議 1 cm；Apple HIG 44 pt、Material 48 dp 換算約 9–10 mm |
| 按鈕間距 | ≥ 16 px | 避免誤觸 |
| 主標（1.5 m 可讀） | ≥ 48–56 px | 56 px 在 1.5 m 視角約 15 arcmin |
| 內文（1.0 m 可讀） | ≥ 26–32 px | 26 px 在 1.5 m 只有 7 arcmin 總高 |
| 輔助字（主持用） | 22 px | 只在 0.5 m 內清楚；**不可**表達要遠看的資訊 |
| 對比 | 正文 ≥ 4.5:1 | WCAG AA；大字（≥24 px 粗體）可 3:1 |
| 語義色數量 | ≤ 5 | 四類別＋一個強調色 |
| 動畫 | 150–300 ms | 面板刷新慢，>400 ms 覺得遲鈍 |

### 3.1 現有配色的實算對比度

| 組合 | 對比 | 判定 |
|---|---:|---|
| #14342B 深綠 on #F5F3E9 奶油 | 12.13:1 | 極佳 |
| #2F63E8 可回收藍 on 奶油 | 4.64:1 | 合格（剛過） |
| #C6412F 有害紅 on 奶油 | 4.50:1 | 合格（臨界） |
| #2E9E5B 廚餘綠 on 奶油 | 3.07:1 | **不合格**（只可大字／色塊） |
| #E0912B 橙 on 奶油 | 2.29:1 | **不合格**（只可圖示／底色） |
| #7A868D 灰 on 奶油 | 3.36:1 | **不合格** |
| #DCE45A 青檸 on #14342B | 9.83:1 | 極佳 |

**修正（保留色相，加深文字版）**：廚餘文字 `#1B7A44`（4.82:1）、橙文字 `#9A5E14`（4.74:1）、灰文字 `#5A666C`（5.32:1）、有害文字 `#B23A2C`（5.34:1）。原亮色保留作色塊、圖示、頂欄底色。

---

## 4. Figma 交付清單

- Frame 800×480，一 frame 一屏；另開安全區圖層（四邊留 24 px）。
- Grid：8 px 基準；主分區 56／320／104。
- Text styles：Display 112、H1 56、H2 32、Body 26、Meta 22。
- Color styles：9 個 token（含四個深色文字版），命名與程式一致。
- Components：Topbar（3 變體）、Button（3 變體）、LidCell（開／關）、CategoryBadge、StepCard、Tile。
- 圖示：一套線稿，1.5–2 px 線寬，24／32 px 兩尺寸。
- 按鈕三態：default／pressed／disabled（板上無 hover）。
- **不要做**：陰影、毛玻璃、漸層、<22 px 字、hover——面板不會還原，且令 LVGL 每幀多畫圖層。

---

## 5. 落地次序

| 步 | 做甚麼 | 通過標準 |
|---|---|---|
| 1 | 接入官方 BSP，跑 `display_lvgl_demos` | 官方 demo 正常、觸控可用 |
| 2 | 自家 bringup 出純色＋中文字 | 無豆腐、無撕裂 |
| 3 | `ui/lvgl` 接上 BSP | 11 屏可逐屏切換 |
| 4 | Figma 按第 4 節出稿 | 字 ≥22 px、按鈕 ≥85 px、對比 ≥4.5:1 |
| 5 | 匯出或手寫 `stc_ui.c` | 設計稿與板上畫面 1:1 |

---

## 7. LVGL Pro 授權：Community 夠不夠用（2026-11-03 補充）

Community 方案（免費、無到期日、不限座位）實際包含：

| 項目 | Community |
|---|---|
| Editor 全部功能 | ✅ |
| **Figma Flow** | ✅ 完整可用 |
| Online project share | 只限公開 repo |
| **CLI** | ❌ 不可用（屬 Professional 功能，需授權 token） |
| **商業使用** | ❌ 只限個人使用與評估 |
| Product variants | 不限 |
| 支援 | 公開文件、AI assistant、Forum |

### 7.1 對本項目的結論

- **比賽用途完全夠。** 11 個畫面全部可以在 Community 版完成，Figma 匯出可用。
- **失去的是自動化**：沒有 CLI，就不能在 CI 內 `lvglpro generate / validate / screenshot`。
  替代做法：在設計機上手動 Generate C，把產出連 XML 一齊 commit。XML 可 diff，C 是產物。
- **授權風險點**：概念裡提過「賣給學校 ICT 科／NGO」。Community 只准個人使用與評估，
  一旦有商業銷售就要轉 Growth（per product 報價）。比賽本身非商業，現階段無問題，但要在文件裡寫清楚這個界線。
- **架構分工建議**：XML 管畫面（layout / 樣式 / 圖層），C 管狀態機與邏輯（`stc_ui.c` 內的 transition、計分、感測）。
  這樣即使日後不再用 LVGL Pro，畫面 XML 仍可手動翻譯，邏輯不動。

### 7.2 若日後要開 CLI

CLI 需要 token（`LVGLPRO_CLI_TOKEN`）。token 屬憑證，不得貼在對話或 commit 入 repo；
存放用 `hermes-vault add`，CI 內用 secret 注入。

---

## 6. 資料來源

1. esp-bsp 倉庫 — https://github.com/espressif/esp-bsp
2. ESP32-S31-Korvo-1 BSP v1.1.0 — https://components.espressif.com/components/espressif/esp32_s31_korvo_1
3. BSP 顯示標頭 — esp-bsp/bsp/esp32_s31_korvo_1/include/bsp/display.h
4. esp_lvgl_port v2.9.0 — https://components.espressif.com/components/espressif/esp_lvgl_port
5. LVGL Pro Figma 匯出 — https://lvgl.io/docs/pro/figma/exporting
6. lvgl_pro — https://github.com/lvgl/lvgl_pro
7. LVGL Flow for Figma — https://www.figma.com/community/plugin/1362005814860504095
8. SquareLine 授權 — https://squareline.io/pricing/licenses
9. EEZ Studio — https://www.envox.eu/studio/studio-introduction
10. NN/g 觸控目標 — https://www.nngroup.com/articles/touch-target-size
11. WCAG 2.5.8 — https://testparty.ai/blog/wcag-target-size-guide
12. ESP32-S3 RGB + LVGL PSRAM／bounce buffer — https://www.kadidisplay.com/blog-news/esp32-s3-rgb-lcd-lvgl-psram-buffers-and-tearing-control
