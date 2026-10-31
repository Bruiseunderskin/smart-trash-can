# Native UI verification — 2026-11-01 (Hong Kong)

Scope: UI source component and desktop preview; 800×480, rotation 0, RGB565, LVGL 9.5.0. Completion is based on the native render evaluation; human feedback is optional.

| Check | Evidence | Result |
|---|---|---|
| Native build | CMake/Ninja Debug; component and preview compile with -Wall -Wextra -Werror | PASS |
| Data/state/layout/actions | CTest / `main --self-test`: 49 scenarios; nine states, four results/lessons, every distinct candidate pair, invalid fields/NaN/transition, click-event navigation, completion, duplicate retry, uint32 maximum, parent deletion, decoded-image cache eviction on destroy, 150 repeated cycles | PASS |
| Figma retry regression | Reproduced overlapping nested SVG; replaced with true 48×48 imported source-SVG components, then inspected corrected Figma screenshot and native `result_retry.png` | PASS |
| Text contrast | `design-checks.json`: 75 text/background checks ≥4.5:1 | PASS |
| Touch target geometry | 13 targets ≥44×44 pixels | PASS |
| Native framebuffers | Nine states, all four result/lesson variants, maximum counter screenshots; software SDL RenderReadPixels at 800×480 | PASS |
| Font coverage | Generated corpus 223 CJK characters; required source glyphs verified; camera fallback missing characters corrected | PASS for fixed corpus |
| Motion | One 260 ms opacity animation; reduced-motion removes it | PASS |
| Generated-input sync | `manifest.json`, `design-checks.json`; pinned font/icon tools | PASS |
| Deterministic regeneration | `regeneration-check.json`: all generated files identical | PASS |
| Issue sync | `design/issue-3-sync.json`; nine-state issue update read back | PASS |
| Runtime gate | `preview/.lvgl-preview/state.json`; process live after runner startup | PASS; evaluation owns completion |
| Preview skill package validator | Standalone skill validation and 22 evaluator tests | PASS |
| Physical display/touch/camera/AI/servos | No physical tests or firmware flash | NOT_TESTED |
| ESP32-S31 target build and memory headroom | No ESP-IDF target compilation of this new component | NOT_TESTED |
| Real backend correlation | Controller must filter stale round_id before calling UI; no SSE/job scheduler in this component | NOT_TESTED |
| Educational efficacy/children usability | Product reference review and static content only; no child testing | NOT_TESTED |

Astra through Poe provided a scoped architecture review. Material boundaries were tightened: controller-owned navigation, single-use error acknowledgment, duplicate-start suppression, documented callback/image lifetimes, inert parent-deletion handling, and full counter bounds. Consultation is not independent on-device verification.

Known design/render differences: RGB565 quantizes colors; Figma and LVGL text rasterizers differ. LVGL uses the existing fonts as 4bpp subsets and line spacing derived from the Figma 1.4 line-height. Native keyboard focus adds an outline. All screenshots use fixture score 120/streak 4 except explicitly named maximum-counter cases; the interactive initial score is 0.

No stale-work protection is claimed for an unconnected backend. The production controller must reject obsolete round IDs before UI application. Navigation callbacks emit intents only; Start does not fabricate sensor results in the reusable component. Native preview's separate demo controller is explicitly labeled DEMO.

No mandatory user-decision gate remains. Explicit feedback can optionally restore the runner backup, separately from evaluation.

## Post-completion learning and spacing revision

- User requested extension learning only for finished students. Homepage has one Start action; learning is reached through result_ok → summary → category_guide. Learning returns home rather than starting a challenge. Returning home removes extension access.
- 49 native scenarios pass, including blocked early guide access, summary-to-guide navigation, lesson-to-home, guide-to-summary, and no direct restart from lessons.
- 27 geometry/navigation assertions cover all bottom rows, 24 px icon/text gaps, padded score cards, and absence of early learning entry points.
- Shared spacing: outer horizontal 24 px; normal cards 24 px; compact cards 16 px; icon/text gap 24 px; text stack gap 8 px; bottom controls/instruction rows 64 px.
- The earlier pending preview was archived as superseded by the user's edit request. No OK/NO-GO was inferred. This historical pending state is superseded by evidence-based render evaluation.

## Evidence-based render completion

The portable/installed lvgl-preview skill evaluates 17 native state/category/boundary framebuffers. Required evidence covers build, actual LVGL object/text containment, actual glyph resolution, contrast/touch geometry, interaction/state/lifecycle tests, and separate visual inspection tied to capture hashes. PNG re-encoding is accepted only when complete RGBA pixels remain identical.

The evaluator rejects blank captures, stale executable/configuration/evidence, missing states/variants and missing review. Missing proof is INSUFFICIENT_EVIDENCE. PASS requires no user OK/NO-GO and does not mean hardware readiness. The authoritative result is render-eval.json.

Linux CI baseline: reviewed all 17 captures from run 41219979378. Cross-platform comparison showed RGB channel differences of at most 1 with unchanged geometry. Linux uses its separately reviewed exact pixel hashes and committed PNG baselines; acceptance does not use a pixel-difference tolerance.
