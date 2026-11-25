#!/usr/bin/env python3
"""Render every ui/lvglpro screen and, optionally, diff it against baselines.

    tools/lvglpro_preview/render.py                      # render into build/png
    tools/lvglpro_preview/render.py --compare baselines  # fail when a screen moved
    tools/lvglpro_preview/render.py --update baselines   # rewrite the baselines

A baseline failure means a pixel changed; inspect build/png/<state>.png and
build/png/diff_<state>.png before updating the baseline.
"""
from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageChops

HERE = Path(__file__).resolve().parent
PREVIEW = HERE / "build" / ("preview.exe" if sys.platform == "win32" else "preview")
OUT = HERE / "build" / "png"

# state -> milliseconds to advance before capturing (the countdown moves)
STATES = [
    ("idle", 0),
    ("intro", 0),
    ("scan_ready", 0),
    ("scan_logout_confirm", 1300),
    ("capturing", 0),
    ("classifying", 0),
    ("result_ok", 0),
    ("result_ok_2s", 2000),
    ("result_ok_lid", 0),
    ("result_retry", 0),
    ("bin_choice", 0),
    ("error", 0),
    ("learning", 0),
    ("summary", 0),
    ("ask_ready", 0),
    ("ask_listening", 0),
    ("ask_thinking", 0),
    ("ask_answering", 0),
    ("ask_answering_long", 0),
    ("ask_offline", 0),
    ("ask_asr_retry", 0),
    ("ask_countdown", 0),
    ("ask_no_answer", 0),
    ("ask_goodbye", 0),
    ("ask_connecting", 0),
    ("ask_provisioning", 0),
]

W, H = 800, 480
DIFF_LIMIT = 0.02  # 2% of pixels may differ (font rasterisation)


def render(state: str, ticks: int) -> Image.Image:
    raw = OUT / f"{state}.raw"
    proc = subprocess.run(
        [str(PREVIEW), state, str(ticks), str(raw)],
        capture_output=True, text=True, check=False,
    )
    if proc.returncode != 0:
        sys.exit(f"{state}: preview failed\n{proc.stdout}{proc.stderr}")
    if "layout=FAIL" in proc.stdout:
        sys.exit(f"{state}: stc_ui_check_layout failed\n{proc.stdout}")
    data = raw.read_bytes()
    if len(data) != W * H * 2:
        sys.exit(f"{state}: expected {W * H * 2} bytes, got {len(data)}")
    image = Image.frombytes("RGB", (W, H), data, "raw", "BGR;16")
    image.save(OUT / f"{state}.png")
    raw.unlink()
    return image


def compare(state: str, image: Image.Image, baseline_dir: Path) -> float:
    ref_path = baseline_dir / f"{state}.png"
    if not ref_path.exists():
        sys.exit(f"{state}: baseline {ref_path} is missing (run --update)")
    ref = Image.open(ref_path).convert("RGB")
    diff = ImageChops.difference(image, ref)
    raw_diff = diff.tobytes()
    changed = sum(
        1
        for offset in range(0, len(raw_diff), 3)
        if raw_diff[offset] or raw_diff[offset + 1] or raw_diff[offset + 2]
    )
    ratio = changed / (W * H)
    if changed:
        diff.point(lambda v: min(255, v * 4)).save(OUT / f"diff_{state}.png")
    return ratio


def main() -> int:
    parser = argparse.ArgumentParser()
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--compare", metavar="DIR", type=Path)
    group.add_argument("--update", metavar="DIR", type=Path)
    args = parser.parse_args()

    if not PREVIEW.exists():
        sys.exit(f"{PREVIEW} is missing; run tools/lvglpro_preview/build.sh first")
    OUT.mkdir(parents=True, exist_ok=True)

    failures = []
    for state, ticks in STATES:
        image = render(state, ticks)
        if args.update:
            args.update.mkdir(parents=True, exist_ok=True)
            image.save(args.update / f"{state}.png")
            print(f"{state:16s} baseline updated")
        elif args.compare:
            ratio = compare(state, image, args.compare)
            mark = "ok" if ratio <= DIFF_LIMIT else "FAIL"
            print(f"{state:16s} {ratio * 100:6.2f}%  {mark}")
            if ratio > DIFF_LIMIT:
                failures.append(state)
        else:
            print(f"{state:16s} rendered")

    if failures:
        print(f"\n{len(failures)} screen(s) moved: {', '.join(failures)}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
