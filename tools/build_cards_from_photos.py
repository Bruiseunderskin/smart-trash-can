#!/usr/bin/env python3
"""Turn real object photos into extra card faces.

Each photo becomes one new card identity: the subject is cut out, given a white
sticker outline so stray background halo is hidden, and centred on a white card.
The result matches the existing 709x709 card format, so the renderer and trainer
need no special case.

    python3 tools/build_cards_from_photos.py \
        --photos /config/work/garbo/data/standardized_384 \
        --out data/mixed-card-object/wordless-v1/cards \
        --per-class 30 --prefix C --start 1

Writes the new PNGs and prints one CSV row per card on stdout.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import io
import random
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

DEFAULT_MAP = {
    "biological": "food",
    "battery": "hazardous",
    "cardboard": "recyclable",
    "glass": "recyclable",
    "metal": "recyclable",
    "paper": "recyclable",
    "plastic": "recyclable",
    "clothes": "other",
    "shoes": "other",
    "trash": "other",
}


def cutout(image: Image.Image, session) -> Image.Image:
    from rembg import remove

    return remove(image, session=session, post_process_mask=True)


def good_cutout(rgba: Image.Image) -> bool:
    alpha = np.array(rgba.getchannel("A"))
    coverage = (alpha > 30).mean()
    if not 0.08 <= coverage <= 0.92:
        return False
    ys, xs = np.where(alpha > 30)
    if len(xs) == 0:
        return False
    w, h = xs.max() - xs.min() + 1, ys.max() - ys.min() + 1
    if w < 60 or h < 60:
        return False
    aspect = max(w, h) / min(w, h)
    return aspect <= 4.0


def build_card(rgba: Image.Image, size: int, fill: float, outline: int, rng: random.Random) -> Image.Image:
    alpha = rgba.getchannel("A")
    bbox = alpha.point(lambda p: 255 if p > 24 else 0).getbbox()
    if not bbox:
        raise ValueError("empty cutout")
    subject = rgba.crop(bbox)

    target = int(size * fill)
    scale = target / max(subject.size)
    subject = subject.resize(
        (max(1, int(subject.width * scale)), max(1, int(subject.height * scale))), Image.LANCZOS
    )

    # white die-cut outline: dilate the mask, fill white, sit the subject on top
    pad = outline * 3
    mask = subject.getchannel("A")
    grow = mask.filter(ImageFilter.MaxFilter(outline * 2 + 1))
    padded_mask = Image.new("L", (subject.width + pad * 2, subject.height + pad * 2), 0)
    padded_mask.paste(grow, (pad, pad))
    white = Image.new("RGBA", padded_mask.size, (255, 255, 255, 0))
    white.putalpha(padded_mask)
    outline_layer = white

    card = Image.new("RGBA", (size, size), (255, 255, 255, 255))
    x = (size - subject.width) // 2 + rng.randint(-4, 4)
    y = (size - subject.height) // 2 + rng.randint(-4, 4)
    card.alpha_composite(outline_layer, (x - pad, y - pad))
    card.alpha_composite(subject, (x, y))

    # faint rounded border, matching the printable card look
    draw = ImageDraw.Draw(card)
    inset = max(2, size // 200)
    draw.rounded_rectangle(
        (inset, inset, size - inset - 1, size - inset - 1),
        radius=int(size * 0.045),
        outline=(214, 214, 214, 255),
        width=max(2, size // 300),
    )
    return card.convert("RGB")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--photos", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--per-class", type=int, default=30)
    parser.add_argument("--size", type=int, default=709)
    parser.add_argument("--fill", type=float, default=0.72)
    parser.add_argument("--outline", type=int, default=8)
    parser.add_argument("--prefix", default="C")
    parser.add_argument("--start", type=int, default=1)
    parser.add_argument("--seed", type=int, default=20261110)
    parser.add_argument("--model", default="u2netp")
    args = parser.parse_args()

    from rembg import new_session

    rng = random.Random(args.seed)
    session = new_session(args.model)
    args.out.mkdir(parents=True, exist_ok=True)

    by_class: dict[str, list[Path]] = {}
    for class_dir in sorted(p for p in args.photos.iterdir() if p.is_dir()):
        label = DEFAULT_MAP.get(class_dir.name.lower())
        if not label:
            print(f"skip unknown class dir {class_dir.name}", file=sys.stderr)
            continue
        files = sorted(p for p in class_dir.iterdir() if p.suffix.lower() in (".jpg", ".jpeg", ".png"))
        rng.shuffle(files)
        by_class.setdefault(label, []).extend(files)

    index = args.start
    writer = csv.writer(sys.stdout)
    writer.writerow(["card_id", "class", "split", "sha256", "retouch", "source"])
    made = 0
    for label, files in sorted(by_class.items()):
        produced = 0
        for path in files:
            if produced >= args.per_class:
                break
            try:
                with Image.open(path) as raw:
                    image = raw.convert("RGB")
                rgba = cutout(image, session)
                if not good_cutout(rgba):
                    continue
                card = build_card(rgba, args.size, args.fill, args.outline, rng)
            except Exception as exc:  # noqa: BLE001 - a single bad photo must not stop the batch
                print(f"skip {path.name}: {exc}", file=sys.stderr)
                continue
            card_id = f"{args.prefix}-{index:02d}"
            index += 1
            dest = args.out / f"{card_id}.png"
            card.save(dest)
            digest = hashlib.sha256(dest.read_bytes()).hexdigest()
            writer.writerow([card_id, label, "", digest, "cutout", str(path)])
            produced += 1
            made += 1
        print(f"{label}: {produced} cards", file=sys.stderr)

    print(f"done, {made} cards", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
