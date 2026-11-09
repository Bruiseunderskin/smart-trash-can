#!/usr/bin/env python3
"""Render scenes from the padded wordless cards, honouring the card-identity split.

Cards come from `cards.csv` (card_id, class, split). The split is by card identity,
so the same artwork never appears in both train and validation.

    python3 tools/render_wordless_scenes.py \
        --cards data/mixed-card-object/wordless-v1/cards-padded \
        --manifest data/mixed-card-object/wordless-v1/cards.csv \
        --out /tmp/wordless-dataset --train-per-card 60 --val-per-card 10
"""

from __future__ import annotations

import argparse
import csv
import random
from pathlib import Path

from PIL import Image

from render_card_scenes import render


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cards", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--train-per-card", type=int, default=60)
    parser.add_argument("--val-per-card", type=int, default=10)
    parser.add_argument("--size", type=int, default=160)
    parser.add_argument("--seed", type=int, default=20261109)
    args = parser.parse_args()

    with args.manifest.open() as f:
        rows = list(csv.DictReader(f))

    counts = {"train": 0, "val": 0}
    missing = []
    for row in rows:
        stem = row["card_id"].replace("-", "")
        card_path = args.cards / f"{stem}.png"
        if not card_path.exists():
            missing.append(stem)
            continue
        per_card = args.train_per_card if row["split"] == "train" else args.val_per_card
        card = Image.open(card_path).convert("RGBA")
        dest_dir = args.out / row["split"] / row["class"]
        dest_dir.mkdir(parents=True, exist_ok=True)
        for i in range(per_card):
            rng = random.Random(f"{args.seed}:{row['card_id']}:{i}")
            scene = render(card, args.size, rng)
            scene.save(dest_dir / f"{stem}_{i:03d}.jpg", quality=rng.randint(82, 94))
            counts[row["split"]] += 1

    if missing:
        raise SystemExit(f"missing card art for: {sorted(set(missing))}")
    print(f"rendered {counts} scenes into {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
