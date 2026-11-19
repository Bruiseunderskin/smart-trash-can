#!/usr/bin/env python3
"""Build the S1 evaluation matrix: every printed card x six capture conditions.

The training renderer randomises everything at once, which is right for training
and useless for diagnosis: when a card fails you cannot say under which condition
it failed. This script renders each card once under a nominal condition and then
derives the other conditions by deterministic transforms, so every cell of the
matrix is attributable.

Conditions (issue #67):

    nominal    as rendered
    glare      1-2 soft highlights, peak +40% brightness
    low_light  brightness x0.35, gaussian noise sigma 8
    far        card shrunk to 25-35% of frame, then resampled back up
    angle      rotate +-30 deg
    motion     directional blur, kernel 7-15 px

Output: `<out>/manifest.csv` with card_id,class,condition,path,seed plus one PNG
per cell. Images are reproducible from the manifest seed and are not committed.

Requires the training venv (numpy, Pillow): see models/wordless-padded/README.md.

Usage:
    python3 tools/render_card_matrix.py \
        --cards models/wordless-padded/cards \
        --cards-csv models/wordless-padded/cards.csv \
        --out /tmp/card-matrix --per-condition 20
"""

from __future__ import annotations

import argparse
import csv
import random
from pathlib import Path

from PIL import Image, ImageChops, ImageEnhance, ImageFilter

import render_card_scenes as base

CONDITIONS = ("nominal", "glare", "low_light", "far", "angle", "motion")
FILL = (128, 128, 128)


def _radial(size: int) -> Image.Image:
    """White-centre radial gradient used as a glare sprite."""
    return Image.radial_gradient("L").resize((size, size), Image.Resampling.BICUBIC)


def add_glare(img: Image.Image, rng: random.Random) -> Image.Image:
    """Screen one or two soft highlights, the way pearl paper catches a lamp."""
    width, height = img.size
    out = img
    for _ in range(rng.randint(1, 2)):
        radius = rng.randint(max(8, width // 8), max(12, width // 3))
        sprite = _radial(radius * 2)
        mask = sprite.point(lambda v: int(v * 0.40))
        highlight = Image.new("RGB", sprite.size, (255, 255, 255))
        cx = rng.randint(0, max(0, width - sprite.width))
        cy = rng.randint(0, max(0, height - sprite.height))
        layer = Image.new("RGB", out.size, (0, 0, 0))
        layer.paste(highlight, (cx, cy), mask)
        out = ImageChops.add(out, layer)
    return out


def low_light(img: Image.Image, rng: random.Random) -> Image.Image:
    dark = ImageEnhance.Brightness(img).enhance(0.35)
    noise = Image.effect_noise(dark.size, 8).convert("RGB")
    return Image.blend(dark, noise, 0.10)


def far(img: Image.Image, rng: random.Random) -> Image.Image:
    """Simulate a card at 20-25 cm: smaller in frame, then resampled to input size."""
    width, height = img.size
    ratio = rng.uniform(0.25, 0.35)
    small = img.resize((max(8, int(width * ratio)), max(8, int(height * ratio))), Image.Resampling.BICUBIC)
    canvas = Image.new("RGB", (width, height), FILL)
    canvas.paste(small, ((width - small.width) // 2, (height - small.height) // 2))
    return canvas.filter(ImageFilter.GaussianBlur(1.2))


def angle(img: Image.Image, rng: random.Random) -> Image.Image:
    degrees = rng.choice([-30, -20, -10, 10, 20, 30])
    return img.rotate(degrees, resample=Image.Resampling.BICUBIC, expand=False, fillcolor=FILL)


def motion(img: Image.Image, rng: random.Random) -> Image.Image:
    length = rng.randint(7, 15)
    dx, dy = rng.choice([(1, 0), (0, 1), (1, 1)])
    acc = None
    count = 0
    for step in range(0, length, 2):
        shifted = img.transform(
            img.size,
            Image.AFFINE,
            (1, 0, -dx * step, 0, 1, -dy * step),
            resample=Image.Resampling.BILINEAR,
        )
        acc = shifted if acc is None else ImageChops.add(acc, shifted, scale=2.0)
        count += 1
    return acc if acc is not None else img


TRANSFORMS = {
    "glare": add_glare,
    "low_light": low_light,
    "far": far,
    "angle": angle,
    "motion": motion,
}


def load_classes(cards_csv: Path) -> dict[str, str]:
    classes: dict[str, str] = {}
    with cards_csv.open(newline="", encoding="utf-8") as handle:
        for line in csv.DictReader(handle):
            if line.get("kind") == "printed":
                classes[line["card_id"]] = line["class"]
    return classes


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cards", type=Path, required=True, help="directory of card PNGs")
    parser.add_argument("--cards-csv", type=Path, required=True, help="cards.csv with card_id,class,kind")
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--per-condition", type=int, default=20)
    parser.add_argument("--size", type=int, default=160)
    parser.add_argument("--seed", type=int, default=20261119)
    parser.add_argument("--conditions", default=",".join(CONDITIONS))
    parser.add_argument("--backgrounds", type=Path, help="optional photo directory for backgrounds")
    args = parser.parse_args()

    wanted = [c.strip() for c in args.conditions.split(",") if c.strip()]
    unknown = [c for c in wanted if c not in CONDITIONS]
    if unknown:
        raise SystemExit(f"unknown condition(s): {', '.join(unknown)}")

    classes = load_classes(args.cards_csv)
    bg_paths = None
    if args.backgrounds:
        bg_paths = sorted(
            p for p in args.backgrounds.glob("**/*")
            if p.suffix.lower() in {".jpg", ".jpeg", ".png"}
        )

    args.out.mkdir(parents=True, exist_ok=True)
    manifest = args.out / "manifest.csv"
    written = 0

    with manifest.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(["card_id", "class", "condition", "path", "seed"])

        for card_path in sorted(args.cards.glob("*.png")):
            card_id = card_path.stem
            label = classes.get(card_id)
            if label is None:
                continue
            with Image.open(card_path) as opened:
                card = opened.convert("RGB")

            for index in range(args.per_condition):
                seed = args.seed + index * 7919 + sum(ord(c) for c in card_id)
                nominal = base.render(card, args.size, random.Random(seed), bg_paths)

                for condition in wanted:
                    if condition == "nominal":
                        image = nominal
                    else:
                        image = TRANSFORMS[condition](nominal.copy(), random.Random(seed ^ 0x5EED))
                    target = args.out / condition / f"{card_id}-{index:02d}.png"
                    target.parent.mkdir(parents=True, exist_ok=True)
                    image.save(target)
                    writer.writerow(
                        [card_id, label, condition, str(target.relative_to(args.out)), seed]
                    )
                    written += 1

    print(f"wrote {written} images across {len(wanted)} conditions -> {manifest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
