---
name: embedded-ui-visual-quality
description: Use when improving the visual quality of an embedded LVGL UI (screen design, layout, typography, colour, motion, Figma-to-LVGL workflow, ESP-BSP, embedded design rules). Includes proposal for how to optimize the smart-trash-can UI and the decision record for toolchain (Figma/hand-written C) and design rules.
version: 1.0.0
author: Ah-Jat the hermes
license: MIT
compatibility: Requires filesystem; references docs/ in the smart-trash-can repo; uses LVGL 9, esp-bsp, ESP-IDF 5.4+, optional Figma / LVGL Pro.
metadata:
  hermes:
    tags: [embedded, ui, lvgl, design, figma, esp32-s31-korvo-1]
    related_skills: [loop-skill-improver, research, prototype]
---

# Embedded UI Visual Quality

Use this when the user asks "點樣令個 UI 睇落去更好" for an embedded LVGL screen (e.g. the smart-trash-can 4.3" 800×480 display).

## Activation conditions
- Project uses LVGL 9 (confirmed: `stc_ui.c` / `ui/lvgl/`).
- Hardware is ESP32-S3-Korvo-1 (`esp32s31`) or another RGB LCD board with BSP support.
- There is an existing visual artifact to improve (Figma file, mockups, or running UI).

## Core rules (apply before any redesign)

1. Read `docs/ui-redesign-spec.md` first — it defines the current 11-state flow and design tokens (`#14342B`, `#F5F3E9`, `#DCE45A`, `#2F63E8`, etc.).
2. Read `docs/ui-visual-quality.md` — it gives 12 concrete improvement rules (touch targets ≥85 px, text ≥22 px meta / ≥32 px body, colour contrast ≥4.5:1, no shadows/gradients, one action per screen, etc.).
3. Read `docs/embedded-ui-research.md` — it gives the official BSP, Figma→LVGL pipeline, and design-system measurements (217 ppi, 1 px = 0.117 mm, 88 px button = 10.3 mm).

## Proposal embedded in PR `docs/ui-optimization`

The proposal answers: **how should we optimize the smart-trash-can UI?**
- Change #1: state/await_bin (new) — the lid-close vote interaction must be visible.
- Change #2: hidden AI result — AI predicts, player picks, verdict reveals (not before).
- Change #3: design-system enforcement — 4-zone layout (56/320/104), 5-level typography (Display/H1/H2/Body/Meta), colour roles, 8 px grid, 24 px safe margin.
- Change #4: no hover, no shadows, no gradients, no <22 px meta text, motion <300 ms.
- Change #5: Figma and code stay in sync via tokens (`stc_tokens.h`); C is source of truth, Figma is design source.

See `docs/ui-optimization-proposal.md` for the full proposal (intended for PR `docs/ui-optimization`).

## References

- `docs/ui-redesign-spec.md` (UI flow redesign spec)
- `docs/ui-visual-quality.md` (visual quality rules and before/after patterns)
- `docs/embedded-ui-research.md` (BSP, Figma→LVGL pipeline, touch/typography metrics)
- `docs/ui-flow-redesign.md` (11-state flow with payload changes)
- `docs/edu-game-concept.md` (game rules, M1/M2/M3, D2 card decision)
- `docs/lid-detect-research.md` (hardware sensor selection, 3.3V condition)
- `docs/ROADMAP.md` (calendar, milestones, cut rules)
- Project: `github.com/Bruiseunderskin/smart-trash-can`
- BSP: `https://components.espressif.com/components/espressif/esp32_s31_korvo_1`
- esp-bsp: `https://github.com/espressif/esp-bsp`
- esp_lvgl_port v2.9.0 docs + feature list
- LVGL Pro / Figma Flow docs (`lvgl.io/docs/pro/figma/exporting`)
- LVGL Pro CLI docs (`lvglpro` requires Professional license; not available in Community)

## Toolchain decision (2026-11-03)

- **BSP**: use `espressif/esp32_s31_korvo_1` v1.1.0 (official), not hand-written LCD init.
- **Design**: Figma (free) for mockups; no Figma Dev Mode required.
- **UI authoring**: hand-written C (`stc_ui.c` + SDL preview) remains primary; LVGL Pro Community (free, Figma Flow available) can be adopted for design-to-code flow but does not replace the code-first loop. CLI (CI automation) requires Professional license.
- **Fonts**: generated `stc_font_*` bitmap set (4 bpp) is the source of truth; add 22 px meta size.
- **Images**: 17-image field probe (`docs/issue12/field-probe/`) is locked; own-site card dataset must be session-split (see `docs/edu-game-concept.md` §3.5 / `docs/lid-detect-research.md` §7).
