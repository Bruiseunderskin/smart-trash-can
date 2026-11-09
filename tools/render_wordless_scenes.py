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
import shutil
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
    parser.add_argument("--bg-dir", type=Path, default=None,
                        help="folder of real photos used as blurred backgrounds")
    parser.add_argument("--extra-train", type=Path, default=None,
                        help="ImageFolder-style dir of real object photos added to train")
    parser.add_argument("--extra-repeat", type=int, default=1,
                        help="how many times each extra training image is repeated")
    parser.add_argument("--balance-train-per-class", type=int, default=0,
                        help="when >0, render this many training scenes per class instead of per card")
    parser.add_argument("--balance-val-per-class", type=int, default=0,
                        help="when >0, render this many validation scenes per class instead of per card")
    parser.add_argument("--scene-split", type=float, default=0.0,
                        help="when >0, render every card and split its scenes by this fraction instead of the manifest split")
    parser.add_argument("--size", type=int, default=160)
    parser.add_argument("--seed", type=int, default=20261109)
    args = parser.parse_args()

    with args.manifest.open() as f:
        rows = list(csv.DictReader(f))

    bg_paths: list[Path] = []
    if args.bg_dir:
        bg_paths = sorted(p for p in args.bg_dir.rglob("*") if p.suffix.lower() in (".jpg", ".jpeg", ".png"))
        if not bg_paths:
            raise SystemExit(f"no background images under {args.bg_dir}")

    counts = {"train": 0, "val": 0}

    # class-balanced budget: cards per (class, split) decide how many scenes each card gets
    class_budget = {}
    if args.balance_train_per_class or args.balance_val_per_class:
        for row in rows:
            class_budget.setdefault((row["class"], row["split"]), 0)
            class_budget[(row["class"], row["split"])] += 1

    def scenes_for(row):
        target = args.balance_train_per_class if row["split"] == "train" else args.balance_val_per_class
        if not target:
            return args.train_per_card if row["split"] == "train" else args.val_per_card
        n_cards = class_budget[(row["class"], row["split"])]
        return max(1, round(target / n_cards))
    missing = []
    for row in rows:
        stem = row["card_id"]
        card_path = args.cards / f"{stem}.png"
        if not card_path.exists():
            card_path = args.cards / f"{stem.replace('-', '')}.png"
            stem = stem.replace("-", "")
        if not card_path.exists():
            missing.append(stem)
            continue
        card = Image.open(card_path).convert("RGBA")
        scene_split = args.scene_split
        per_card = args.train_per_card if scene_split > 0 else scenes_for(row)
        (args.out / "train" / row["class"]).mkdir(parents=True, exist_ok=True)
        (args.out / "val" / row["class"]).mkdir(parents=True, exist_ok=True)
        for i in range(per_card):
            rng = random.Random(f"{args.seed}:{row['card_id']}:{i}")
            scene = render(card, args.size, rng, bg_paths or None)
            if scene_split > 0:
                split = "val" if rng.random() < scene_split else "train"
            else:
                split = row["split"]
            scene.save(args.out / split / row["class"] / f"{stem}_{i:03d}.jpg", quality=rng.randint(82, 94))
            counts[split] += 1

    if args.extra_train:
        for class_dir in sorted(p for p in args.extra_train.iterdir() if p.is_dir()):
            label = class_dir.name
            if label not in {row["class"] for row in rows}:
                continue
            dest = args.out / "train" / label
            dest.mkdir(parents=True, exist_ok=True)
            for path in sorted(class_dir.iterdir()):
                if path.suffix.lower() not in (".jpg", ".jpeg", ".png"):
                    continue
                for rep in range(max(1, args.extra_repeat)):
                    shutil.copy2(path, dest / f"physical_{path.stem}_{rep}{path.suffix.lower()}")
                    counts["train"] += 1

    if missing:
        raise SystemExit(f"missing card art for: {sorted(set(missing))}")
    print(f"rendered {counts} scenes into {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
