#!/usr/bin/env python3
"""Merge rendered scene folders into an ImageFolder train/val tree.

    python3 tools/split_scenes.py --out /tmp/dataset --val-fraction 0.2 \
        /tmp/stage-a /tmp/stage-b
"""

from __future__ import annotations

import argparse
import random
import shutil
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sources", nargs="+", type=Path)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--val-fraction", type=float, default=0.2)
    parser.add_argument("--seed", type=int, default=42)
    args = parser.parse_args()

    labels = sorted({p.name for source in args.sources for p in source.iterdir() if p.is_dir()})
    rng = random.Random(args.seed)
    totals = {}
    for label in labels:
        files: list[Path] = []
        for source in args.sources:
            files.extend(sorted((source / label).glob("*.jpg")))
        rng.shuffle(files)
        cut = int(len(files) * (1 - args.val_fraction))
        for i, path in enumerate(files):
            part = "train" if i < cut else "val"
            dest = args.out / part / label / path.name
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, dest)
        totals[label] = {"train": cut, "val": len(files) - cut}

    print(totals)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
