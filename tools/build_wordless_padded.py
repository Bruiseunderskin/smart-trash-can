#!/usr/bin/env python3
"""Assemble the wordless-padded source of truth: cards, sprites, scenes.

Inputs, and nothing else:

  * `models/wordless-padded/cards.csv` — 38 printed card identities plus the 120
    auxiliary identities used to teach unseen designs, with class and fold;
  * `docs/cards/300dpi/PNG` — the white-background art that is actually printed;
  * `models/wordless-padded/real-photos/` — licensed real photos, produced by
    `tools/fetch_real_photos.py`, used as blurred backgrounds and as a small
    extra training set.

Every card becomes one sprite: the printed face cut out of its page and put on
pearl stock, the material the cards are actually laser cut from. Scenes are
rendered per card, `--train-per-card` each, then split by scene.

    python3 tools/build_wordless_padded.py --force
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import shutil
import subprocess
import sys
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
from rebuild_wordless_cards import cutout, pearl_paper  # noqa: E402

CLASSES = ("food", "hazardous", "other", "recyclable")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def reset(path: Path) -> None:
    if path.exists():
        shutil.rmtree(path)
    path.mkdir(parents=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path("models/wordless-padded"))
    parser.add_argument("--train-per-card", type=int, default=200)
    parser.add_argument("--scene-split", type=float, default=0.15)
    parser.add_argument("--real-repeat", type=int, default=1)
    parser.add_argument("--size", type=int, default=160)
    parser.add_argument("--seed", type=int, default=20261111)
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()

    root = args.root
    cards_csv = root / "cards.csv"
    real_root = root / "real-photos"
    if not cards_csv.exists():
        raise SystemExit(f"missing curated card list: {cards_csv}")
    if not (real_root / "real-photos.csv").exists():
        raise SystemExit(f"missing licensed photo list: {real_root / 'real-photos.csv'}; run tools/fetch_real_photos.py")

    rows = list(csv.DictReader(cards_csv.open()))
    classes = sorted({row["class"] for row in rows})
    if not set(CLASSES) <= set(classes):
        raise SystemExit(f"expected the four deployment classes, found {classes}")

    cards_dir = root / "cards"
    padded_dir = root / "cards-padded"
    data_dir = root / "dataset"
    for target in (cards_dir, padded_dir):
        if target.exists() and not args.force:
            raise SystemExit(f"{target} exists; pass --force to rebuild it")
    cards_dir.mkdir(parents=True, exist_ok=True)  # printed faces are re-copied; auxiliary art lives here
    reset(padded_dir)
    if data_dir.exists():
        reset(data_dir)
    else:
        data_dir.mkdir(parents=True)

    # 1. card art: printed faces are copied and verified, auxiliary faces live here already
    for row in rows:
        target = cards_dir / f"{row['card_id']}.png"
        source = row["source"]
        if source.startswith("docs/"):
            origin = Path(source)
            if not origin.exists():
                raise SystemExit(f"missing printed card art: {origin}")
            if sha256(origin) != row["sha256"]:
                raise SystemExit(f"hash mismatch for {row['card_id']}: {origin}")
            shutil.copy2(origin, target)
        elif not target.exists():
            raise SystemExit(f"missing auxiliary card art: {target}")
        if sha256(target) != row["sha256"]:
            raise SystemExit(f"card art changed on disk: {target}")
        sprite = pearl_paper(cutout(target), int(row["sha256"][:8], 16))
        sprite.save(padded_dir / f"{row['card_id']}.png", "PNG")

    strays = {path.stem for path in cards_dir.glob("*.png")} - {row["card_id"] for row in rows}
    if strays:
        raise SystemExit(f"card art not listed in cards.csv: {sorted(strays)}")

    # 2. scenes: every card gets the same budget, split per scene; real photos join train
    render = [
        sys.executable,
        str(Path(__file__).resolve().parent / "render_wordless_scenes.py"),
        "--cards", str(padded_dir),
        "--manifest", str(cards_csv),
        "--out", str(data_dir),
        "--scene-split", str(args.scene_split),
        "--train-per-card", str(args.train_per_card),
        "--extra-train", str(real_root),
        "--extra-repeat", str(args.real_repeat),
        "--bg-dir", str(real_root),
        "--size", str(args.size),
        "--seed", str(args.seed),
        "--manifest-out", str(root / "dataset.csv"),
    ]
    subprocess.run(render, check=True)

    counts = {
        split: {cls: len(list((data_dir / split / cls).glob("*.jpg"))) for cls in classes}
        for split in ("train", "val")
    }
    build = {
        "cards": len(rows),
        "printed": sum(1 for row in rows if row["kind"] == "printed"),
        "auxiliary": sum(1 for row in rows if row["kind"] == "auxiliary"),
        "classes": classes,
        "card_class_counts": {cls: sum(1 for row in rows if row["class"] == cls) for cls in classes},
        "real_photos": {
            cls: len(list((real_root / cls).glob("*.jpg"))) for cls in classes if (real_root / cls).exists()
        },
        "real_repeat": args.real_repeat,
        "card_size": [709, 709],
        "material": "pearl paper (translucent ink, glitter stock, broad sheen) baked into the sprite",
        "scene_size": args.size,
        "train_per_card": args.train_per_card,
        "scene_split": args.scene_split,
        "seed": args.seed,
        "counts": counts,
        "command": " ".join(render[1:]),
    }
    (root / "build.json").write_text(json.dumps(build, ensure_ascii=False, indent=2) + "\n")
    (root / "labels.txt").write_text("\n".join(classes) + "\n")
    print(json.dumps(build, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
