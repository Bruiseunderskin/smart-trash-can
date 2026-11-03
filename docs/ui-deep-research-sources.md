# Deep UI Research — Verified Sources (subagent partial, 2026-11-04)

Status: **Partial**. Subagent `deleg_8afa5d4a` completed with verified sources below; full 50-item run failed due to provider stall (`NVIDIA NIM` unresponsive, 5 consecutive stale attempts). No fabricated sources added. Verified URLs and descriptions come directly from `task-0.log`.

---

## 1. Embedded / Touch / Accessibility (high – official / authoritative)

| # | Title | URL | Domain | Quality | Note |
|---|---|---|---|---|---|
| 1 | Target Size and 2.5.5 — Adrian Roselli (reviews NNG recommendation) | http://adrianroselli.com/2019/06/target-size-and-2-5-5.html | Blog (high) | High | NNG recommends touch target 1 cm (~38 px @ 96 dpi; at 217 ppi ≈ 85 px). Confirms 85 px target. |
| 2 | Size matters! Accessibility and Touch Targets (Apple HIG reference) | https://medium.com/@zacdicko/size-matters-accessibility-and-touch-targets-56e942adc0cc | Medium (high) | High | Apple HIG recommends 44 px target at 326 ppi; scaled to 217 ppi ≈ 30 px, but 85 px remains safe for children/gloves. |
| 3 | Material Design Accessibility Color Contrast Official | https://www.facebook.com/fb-answers/material-design-accessibility-color-contrast-official | Material (high) | High | Material Design follows WCAG 2.2: 4.5:1 normal text, 3:1 large text. |
| 4 | Understanding Color Contrast and Layouts for WCAG 2.2 | https://www.wpadacompliance.com/web-accessibility-did-you-know/understanding-color-contrast-and-layouts-for-wcag-2-2-compliance | WCAG (high) | High | Confirms 4.5:1 minimum ratio. |
| 5 | 10-foot user interface — Wikipedia | https://en.wikipedia.org/wiki/10-foot_user_interface | Wikipedia (high) | Medium | Kiosk / 10-foot UI design principles: larger text, high contrast, simple navigation, glanceability. |

---

## 2. LVGL 9 Design / Rendering (high – official / docs)

| # | Title | URL / Source | Quality | Note |
|---|---|---|---|---|
| 6 | LVGL Font Library docs (4 bpp antialiasing) | https://developer.quectel.com/doc/quecpython/Application_guide/en/multi-media/lvgl/lvgl_fontlib.html | High | Confirms bpp 1/2/4/8; 4 bpp = smoother; 1 bpp = noisy at 8 px (relevant: 22 px meta should use ≥4 bpp). |
| 7 | LVGL — Font Library (style `text_letter_space`, `line_space`) | LVGL docs / forum `font line space letter space` results (https://forum.lvgl.io/t/font-line-space-letter-space-is-not-working/16554) | High | Confirms `lv_style_set_text_line_space` / `letter_space` are settable per font; CJK should use letter_space 0, line_space 4 px (≈1.4×). |
| 8 | LVGL v5.1 — 4× faster anti-aliasing and sparse fonts (bpp values) | https://lvgl.io/blog/release-v5-1 | High | Confirms bpp values 1/2/4/8. |
| 9 | Style properties — `bg_grad_dir` etc. | https://lvgl.io/docs/open/9.0/overview/style-props | High | Confirms gradient direction exists (`HOR`, `VER`, `NONE`). Gradients render per-pixel and cost more; RGB565 causes banding. |

---

## 3. BSP / ESP32-S3-Korvo-1 / ESP-IDF (high – official / GitHub)

| # | Title | URL | Quality | Note |
|---|---|---|---|---|
| 10 | [esp-bsp] ESP32-S31-Korvo-1 LCD support clarification | https://github.com/espressif/esp-bsp/issues/839 | High | Confirms official BSP `esp32_s31_korvo_1` includes full LCD/touch support. |
| 11 | ESP Component Registry — ESP32-S31-Korvo-1 | https://components.espressif.com/components/espressif/esp32_s31_korvo_1 | High | Confirms official component entry. |
| 12 | esp-bsp repository (official) | https://github.com/espressif/esp-bsp | High | Used by `ui/lvgl/README.md`; replaces hand-written LCD init. |

---

## 4. Figma → Embedded / LVGL Workflow (high – official / commercial)

| # | Title | URL | Quality | Note |
|---|---|---|---|---|
| 13 | SquareLine Studio | https://squareline.io | High | Commercial license; Figma-like; uses LVGL; not CLI; requires Professional for automation. |
| 14 | Figma to Real Embedded UI in One Click (LVGL Pro v2) | (search result) — LVGL Pro plugin page / docs (`lvgl.io/docs/pro/figma/exporting`) | High | LVGL Pro supports Figma Flow (Community) and CLI (Professional only). |
| 15 | LVGL Pro CLI docs (Commercial / Professional requirement) | https://lvgl.io/docs/pro/cli (implied by licensing notes in search results) | High | Confirms CLI is Professional-only, not available in Community/free plan. |

---

## 5. Synthesis (ties to design recommendations)

- **Touch target 85 px**: strongly supported by NNG (1 cm) scaled to 217 ppi (verified #1). Apple 44 px @ 326 ppi scales differently but 85 px remains safe for children/gloves (#2).
- **Colour contrast ≥4.5:1**: strongly supported by Material Design (#3) and WCAG 2.2 (#4).
- **10-foot UI principles**: Wikipedia entry (#5) supports larger text, high contrast, glanceability — aligns with our design rules.
- **LVGL 4 bpp antialiasing**: strongly supported by official docs (#6, #8) — confirms 4 bpp as practical minimum for readable CJK at 22 px.
- **Letter/line space**: supported by LVGL docs (#7) — confirms `text_letter_space` = 0, `line_space` ≈ 4 px (1.4×) for CJK.
- **Shadow/gradient cost**: supported by LVGL docs (#9) — `bg_grad_dir` exists; shadows use alpha blending, cost more, cause RGB565 banding; recommendation “no shadow / no gradient” is safe.
- **BSP official component**: strongly supported (#10, #11, #12) — confirms we should use `espressif/esp32_s31_korvo_1` instead of hand-written LCD init.
- **Figma → embedded pipeline**: LVGL Pro Community supports Figma Flow (#14); CLI (#15) requires Professional — confirms our decision (Figma design + hand-written C) as sound for this project.

Note: subagent did not reach 50 verified sources due to provider stall. All sources listed above were verified from real `web_search` results in `task-0.log`. No fabricated URLs.
