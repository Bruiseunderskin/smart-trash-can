# Native SDL preview backend

The portable script is a safety wrapper around a conventional native LVGL simulator. It does not bundle LVGL, SDL, a compiler, or a project build system.

## Requirements

- Python 3.9 or newer
- CMake and a configured build directory
- A working SDL2 development/runtime installation
- An LVGL project that exposes the supported SDL initialization pattern

## What the script changes

The script creates a backup under `.lvgl-preview/backups/` and changes only:

- one `sdl_hal_init(width, height)` call;
- one `lv_display_set_rotation(...)` call, inserting or updating it next to the display initialization;
- one `#define LV_COLOR_DEPTH ...` line.

It refuses to continue when any target is missing or appears more than once. It never uses `shell=True`, `eval`, `source`, `curl | sh`, `git checkout`, or a project-provided run wrapper.

## Output state

Runtime state is written to `.lvgl-preview/`, which should normally be ignored by the project:

```text
.lvgl-preview/
├── backups/<run-id>/
├── logs/<run-id>.log
├── ledger.jsonl
├── render-eval.json
├── preview-evaluation.md
└── state.json
```

Do not commit logs or state if they contain project-specific paths. The scripts do not collect secrets or send telemetry.

The runner records source/configuration and executable hashes. `--replace` archives the prior run without signalling its PID. Only the current invocation’s own unreaped Popen child can be terminated after runtime failure. New runs proceed to evidence evaluation without a human-decision gate.
