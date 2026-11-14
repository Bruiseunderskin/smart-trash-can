#!/usr/bin/env python3
"""Render the `unknown` class: scenes with no card at all.

Adds `unknown/` to an existing ImageFolder dataset (the card scenes stay as they
are), plus a separate holdout folder used only to measure how often the model
still accepts an empty scene.

Surfaces are procedural: the repo's photo pool is all garbage close-ups, so a
cropped real photo cannot represent an empty desk. The holdout therefore differs
from train/val by RNG stream and surface parameters, not by photo file — there is
no photo surface to hold out.

    python3 tools/render_none_scenes.py \
        --dataset runs/unknown-v1/dataset --train 1200 --val 24 --holdout 300
"""

from __future__ import annotations

import argparse
import csv
import json
import random
from pathlib import Path

from render_card_scenes import render_empty

UNKNOWN = "unknown"


def write_scenes(out_dir: Path, count: int, size: int, seed: str, manifest: list[dict]) -> int:
    target = out_dir / UNKNOWN
    target.mkdir(parents=True, exist_ok=True)
    for i in range(count):
        rng = random.Random(f"{seed}:{i}")
        image = render_empty(size, rng)
        name = f"unknown_{i:04d}.jpg"
        image.save(target / name, quality=rng.randint(40, 95))
        manifest.append({"scene": str(target / name), "seed": seed, "index": i})
    return count


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dataset", type=Path, required=True, help="ImageFolder root holding train/ and val/")
    parser.add_argument("--holdout-out", type=Path, default=None, help="default: <dataset>/holdout")
    parser.add_argument("--size", type=int, default=160)
    parser.add_argument("--seed", type=int, default=20261114)
    parser.add_argument("--train", type=int, default=1200)
    parser.add_argument("--val", type=int, default=24)
    parser.add_argument("--holdout", type=int, default=300)
    args = parser.parse_args()

    holdout_out = args.holdout_out or (args.dataset / "holdout")

    manifest: list[dict] = []
    counts = {}
    counts["train"] = write_scenes(args.dataset / "train", args.train, args.size,
                                   f"{args.seed}:train", manifest)
    counts["val"] = write_scenes(args.dataset / "val", args.val, args.size,
                                 f"{args.seed}:val", manifest)
    counts["holdout"] = write_scenes(holdout_out, args.holdout, args.size,
                                     f"{args.seed}:holdout", manifest)

    (args.dataset / "unknown-manifest.json").write_text(json.dumps({
        "class": UNKNOWN,
        "counts": counts,
        "holdout_dir": str(holdout_out),
        "surfaces": "procedural (desk, concrete, wood, bin plastic, fabric, wall, metal, tile); "
                    "no photo surfaces, because the repo photo pool holds no empty scene",
        "holdout_disjoint_from": "separate RNG stream and surface parameters; no shared photo files are involved",
        "transforms": "render_empty(): tilt, shear, 0.35-1.60 brightness (10% dim), colour cast, "
                      "shadow ramp, 0-2 glare hotspots, blur, motion blur, noise sigma 0-12, "
                      "JPEG q40-95, 10% covered lens",
        "scenes": manifest,
    }, indent=2) + "\n")
    print(f"rendered {counts} unknown scenes with procedural surfaces")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
