---
name: lvgl-preview
description: Build, render and evaluate a native LVGL embedded UI at a requested size, rotation and color depth. Check real framebuffer captures, layout, glyphs and interaction before hardware bring-up or handing off code.
license: MIT
---

# LVGL Preview

Use native desktop composition evidence. A passing preview does not prove physical hardware, touch controllers, display buses, timing, wiring, backlight or power behavior.

## Detect and build

Identify the project root, LVGL major version, build system, SDL backend, logical dimensions, rotation and color depth. Read [project detection](references/project-detection.md) and inspect build/display initialization first. A user's request to build/verify the UI authorizes the ordinary build and launch; explain an unfamiliar build command before running it, without adding a separate approval gate.

```sh
python3 scripts/preview.py --project-root /path/to/project \
  --width W --height H --rotation R --color-depth D --dry-run
python3 scripts/preview.py --project-root /path/to/project \
  --width W --height H --rotation R --color-depth D
```

The dry run names the exact files and executable. The runner backs up the source/configuration it changes, builds, checks runtime liveness, and records hashes. Use `--replace` to archive the previous run and preview an authorized revision. It does not signal a persisted PID or record human acceptance. On build/runtime failure it restores its exact backups. See [backend integration](references/preview-backend.md).

## Render and evaluate

Read [render evaluation](references/render-evaluation.md) for the manifest and evidence schema. Reuse the project's native screenshot, layout, font and interaction checks; do not substitute a browser mockup. Install `requirements.txt` in the evaluation Python environment if Pillow is unavailable.

1. Capture every required state and category/long-content variant from the actual LVGL executable. Prefer its framebuffer export; label window captures separately. For black accelerated readbacks, retry with `SDL_RENDER_DRIVER=software` before changing the UI.
2. Run native layout/text-boundary and glyph-coverage checks, contrast/touch-target checks, and the relevant interaction/state tests. Inspect the rendered frames for overlap, clipping, missing glyphs, stale content, hierarchy and spacing. Pixel statistics alone cannot judge those qualities.
3. Record the checks and visual inspection against the current executable/capture hashes. Do not mark visual review PASS without inspecting the images. Missing capabilities are `INSUFFICIENT_EVIDENCE`, not an invented pass.
4. Evaluate the collected evidence:

```sh
python3 scripts/evaluate_render.py --project-root /path/to/project \
  --manifest render/render-manifest.json
```

Evaluation does not depend on closing the window. It never signals a PID stored by a previous invocation; window lifecycle is separate from render acceptance.

- `PASS` (exit 0): complete the scoped UI/native handoff; no mandatory human confirmation.
- `FAIL` (exit 1): fix the demonstrated rendering/check failures and rerun affected checks.
- `INSUFFICIENT_EVIDENCE` (exit 2): state exactly which proof is unavailable; continue other authorized work. Do not claim the missing behavior passed.

Human feedback is optional unless the user explicitly requests approval before proceeding. Keep it separate from evaluation. The legacy `record_decision.py` can record explicitly supplied feedback and restore backups after an explicit rejection; it is not a workflow gate. Never infer feedback from silence.

## Handoff

Report the build/executable, profile, state/variant coverage, evaluation result, evidence paths and remaining hardware limits. Preserve a reviewable source/configuration change and rollback record. Publication/push still requires the user's authorization; evaluation does not grant it.

Run `scripts/validate_skill.py` and `python -m unittest discover -s tests` before claiming this skill package is valid. Use [troubleshooting](references/troubleshooting.md) for failures. Stop repeating the same unsuccessful repair after three attempts; report the concrete blocker rather than inventing a user-approval requirement.
