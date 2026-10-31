# Native render evaluation

`preview.py` proves a build and startup, not UI correctness. `evaluate_render.py` combines native screenshots with independently recorded native checks and an agent visual inspection. It never labels an evaluation as human approval.

## Input manifest

Paths are relative to the native project root and cannot escape it. Use the `display` object from `.lvgl-preview/state.json`. List every required state and explicit variants such as long counters or all result categories.

```json
{
  "display": {"width": 800, "height": 480, "rotation": 0, "color_depth": 16},
  "required_states": ["idle", "result"],
  "required_frames": ["idle", "result", "result-max"],
  "frames": [
    {"id":"idle", "state":"idle", "path":"render/idle.png", "sha256":"actual file digest", "evidence_kind":"framebuffer"}
  ],
  "checks": {
    "build": {"evidence":"render/native-checks.json", "sha256":"actual file digest"},
    "layout": {"evidence":"render/native-checks.json", "sha256":"actual file digest"},
    "glyphs": {"evidence":"render/native-checks.json", "sha256":"actual file digest"},
    "contrast_touch": {"evidence":"render/design-checks.json", "sha256":"actual file digest"},
    "interaction": {"evidence":"render/native-checks.json", "sha256":"actual file digest"},
    "visual_review": {"evidence":"render/visual-review.json", "sha256":"actual file digest"}
  }
}
```

The example omits result frames for brevity; required but missing frames fail coverage.

Each JSON evidence file contains a `checks` object. The native check report also records `executable_sha256` matching the runner. For example:

```json
{
  "executable_sha256":"current native executable digest",
  "checks": {
    "layout":{"status":"PASS", "details":"actual LVGL object/text bounds checked"},
    "glyphs":{"status":"PASS", "details":"all rendered label codepoints resolved without placeholders"}
  }
}
```

Supply `build` and `interaction` results in that report too. `contrast_touch` belongs to the design/accessibility report. Use the actual measurements; neither a detector nor this evaluator establishes WCAG conformance.

The visual review file records `checks.visual_review.status`, the reviewer type (such as `agent_visual_inspection`), observations/limitations, and `frame_hashes` mapping every frame ID to its reviewed digest. It is authored after inspecting images, not generated as PASS by a screenshot script. A prior review can be reused only for unchanged frame hashes, or an exact `frame_pixel_hashes` match over full RGBA pixels. Pixel equivalence allows PNG compression differences across operating systems; it does not tolerate rendering differences.

## What is measured

- Expected dimensions and required state/variant coverage.
- Capture and evidence SHA-256 consistency; native executable and preview-configuration consistency.
- Empty, nearly uniform and fully transparent capture detection using Pillow. These heuristics are not semantic visual judgment.
- Native check results for layout, glyphs, interaction and build; separate contrast/touch evidence.
- Visual inspection tied to the exact captures.

An RGB/RGBA PNG is an encoding of a framebuffer readback. Its channel depth is not proof of the renderer's `LV_COLOR_DEPTH`, physical RGB/BGR order or electrical behavior; the logical render profile is recorded separately.

## Outputs and decisions

The evaluator writes `.lvgl-preview/render-eval.json` and sets `evaluation` to PASS, FAIL or INSUFFICIENT_EVIDENCE. PASS sets `completion_status=completed`; no pending user decision is created. Evaluation never signals a persisted preview PID. The window may remain open for inspection without delaying completion. Failed build/runtime still restores backups; an evaluation failure preserves captures and code for diagnosis.

Explicit user approval/rejection is an optional separate annotation. A user-requested change supersedes a preview without inventing either approval or rejection.
