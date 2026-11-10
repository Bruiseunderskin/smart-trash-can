#!/usr/bin/env python3
"""Render training scenes from the padded classification cards.

The original simulated-card generator was never committed, so this is a small
reimplementation with the two properties that matter for the classifier:

  * the card sits inside the frame with background visible on all four sides,
    which is what the padded artwork is for;
  * viewpoint, scale, light and blur vary, so the model learns the card rather
    than one fixed crop.

Class comes from the file name (`card-04-food.png`, `b-07-hazardous.png`).
Back, cover and sheet images are skipped.

Usage:
    python3 tools/render_card_scenes.py docs/images/cards/set-a-padded /tmp/scenes \
        --per-class 5 --size 160 --seed 7
"""

from __future__ import annotations

import argparse
import random
from pathlib import Path

from PIL import Image, ImageDraw, ImageEnhance, ImageFilter

LABELS = ("recyclable", "food", "hazardous", "other")


def label_from_name(name: str) -> str | None:
    stem = Path(name).stem
    for label in LABELS:
        if stem.endswith(f"-{label}") or f"-{label}-" in stem:
            return label
    return None


def photo_background(size: int, rng: random.Random, paths: list[Path]) -> Image.Image:
    """Random crop of a real photo, blurred and darkened, used as the table surface."""
    path = rng.choice(paths)
    with Image.open(path) as source:
        img = source.convert("RGB")
        w, h = img.size
        side = min(w, h)
        left = rng.randint(0, max(0, w - side))
        top = rng.randint(0, max(0, h - side))
        img = img.crop((left, top, left + side, top + side)).resize((size, size), Image.LANCZOS)
    img = img.filter(ImageFilter.GaussianBlur(rng.uniform(2.0, 5.0)))
    img = ImageEnhance.Brightness(img).enhance(rng.uniform(0.7, 1.1))
    return ImageEnhance.Color(img).enhance(rng.uniform(0.6, 1.0))


def make_background(size: int, rng: random.Random, bg_paths: list[Path] | None = None) -> Image.Image:
    if bg_paths:
        return photo_background(size, rng, bg_paths)
    base = rng.choice([(232, 228, 219), (214, 208, 198), (200, 197, 190), (176, 170, 162), (235, 233, 228)])
    img = Image.new("RGB", (size, size), base)
    draw = ImageDraw.Draw(img)
    # soft plank / surface bands
    for _ in range(rng.randint(2, 5)):
        y = rng.randint(0, size)
        h = rng.randint(4, max(6, size // 6))
        shade = tuple(max(0, min(255, c + rng.randint(-18, 18))) for c in base)
        draw.rectangle([0, y, size, y + h], fill=shade)
    img = img.filter(ImageFilter.GaussianBlur(rng.uniform(0.4, 1.6)))

    # grain
    px = img.load()
    strength = rng.uniform(3, 10)
    for _ in range(size * size // 3):
        x, y = rng.randrange(size), rng.randrange(size)
        r, g, b = px[x, y]
        d = int(rng.gauss(0, strength))
        px[x, y] = (max(0, min(255, r + d)), max(0, min(255, g + d)), max(0, min(255, b + d)))
    return img


def render(card: Image.Image, size: int, rng: random.Random, bg_paths: list[Path] | None = None) -> Image.Image:
    scene = make_background(size, rng, bg_paths)

    target = int(size * rng.uniform(0.52, 0.78))
    c = card.copy()
    c.thumbnail((target, target), Image.LANCZOS)
    c = c.rotate(rng.uniform(-22, 22), expand=True, resample=Image.BICUBIC)

    # slight perspective via horizontal shear
    shear = rng.uniform(-0.10, 0.10)
    c = c.transform(
        (c.width, c.height),
        Image.AFFINE,
        (1, shear, -shear * c.height / 2, 0, 1, 0),
        resample=Image.BICUBIC,
    )

    x = rng.randint(-(size // 20), max(0, size - c.width + size // 20))
    y = rng.randint(-(size // 20), max(0, size - c.height + size // 20))

    # shadow
    alpha = c.split()[-1]
    shadow = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    shadow.paste((0, 0, 0, rng.randint(40, 90)), (x + rng.randint(2, 7), y + rng.randint(2, 7)), alpha)
    shadow = shadow.filter(ImageFilter.GaussianBlur(rng.uniform(1.5, 4.0)))
    scene = Image.alpha_composite(scene.convert("RGBA"), shadow)

    scene.paste(c, (x, y), c)

    scene = scene.convert("RGB")
    scene = ImageEnhance.Brightness(scene).enhance(rng.uniform(0.86, 1.10))
    scene = ImageEnhance.Contrast(scene).enhance(rng.uniform(0.88, 1.12))

    # specular glare: a bright elliptical hotspot, as when a lamp or window lands on the card
    if rng.random() < 0.45:
        glare = Image.new("L", (size, size), 0)
        gd = ImageDraw.Draw(glare)
        cx, cy = rng.randint(-size // 4, size + size // 4), rng.randint(-size // 4, size + size // 4)
        rx, ry = rng.randint(size // 8, size // 2), rng.randint(size // 8, size // 2)
        gd.ellipse([cx - rx, cy - ry, cx + rx, cy + ry], fill=rng.randint(70, 150))
        glare = glare.filter(ImageFilter.GaussianBlur(size // 12))
        scene = Image.composite(Image.new("RGB", (size, size), (255, 255, 255)), scene, glare)

    # colour cast from indoor lighting
    if rng.random() < 0.5:
        gains = [rng.uniform(0.88, 1.08) for _ in range(3)]
        bands = [scene.getchannel(c).point(lambda v, g=g: min(255, int(v * g))) for c in range(3)]
        scene = Image.merge("RGB", bands)

    # motion blur, approximated by blurring a rotated copy
    if rng.random() < 0.3:
        angle = rng.uniform(-25, 25)
        blurred = scene.rotate(angle, resample=Image.BICUBIC).filter(
            ImageFilter.GaussianBlur(rng.uniform(1.0, 2.6))
        ).rotate(-angle, resample=Image.BICUBIC)
        scene = Image.blend(scene, blurred, rng.uniform(0.45, 0.8))

    # partial occlusion, as when a hand or a neighbouring card covers an edge
    if rng.random() < 0.2:
        layer = Image.new("RGBA", (size, size), (0, 0, 0, 0))
        od = ImageDraw.Draw(layer)
        w = rng.randint(size // 8, size // 3)
        h = rng.randint(size // 8, size // 3)
        ox = rng.choice([0, size - w]) if rng.random() < 0.6 else rng.randint(0, size - w)
        oy = rng.choice([0, size - h]) if rng.random() < 0.6 else rng.randint(0, size - h)
        od.rectangle([ox, oy, ox + w, oy + h], fill=(40, 40, 40, rng.randint(120, 220)))
        layer = layer.filter(ImageFilter.GaussianBlur(size // 60))
        scene = Image.alpha_composite(scene.convert("RGBA"), layer).convert("RGB")

    scene = scene.filter(ImageFilter.GaussianBlur(rng.uniform(0.0, 1.1)))
    return scene


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("card_dir", type=Path)
    parser.add_argument("out_dir", type=Path)
    parser.add_argument("--per-class", type=int, default=4)
    parser.add_argument("--size", type=int, default=160)
    parser.add_argument("--seed", type=int, default=7)
    args = parser.parse_args()

    cards: dict[str, list[Path]] = {label: [] for label in LABELS}
    for path in sorted(args.card_dir.glob("*.png")):
        label = label_from_name(path.stem)
        if label:
            cards[label].append(path)
    if not any(cards.values()):
        raise SystemExit(f"no labelled cards in {args.card_dir}")

    total = 0
    for label, paths in cards.items():
        if not paths:
            continue
        out = args.out_dir / label
        out.mkdir(parents=True, exist_ok=True)
        for i in range(args.per_class):
            rng = random.Random(f"{args.seed}-{label}-{i}")
            card_path = paths[i % len(paths)]
            img = render(Image.open(card_path).convert("RGBA"), args.size, rng)
            name = f"{label}-{i:04d}-{card_path.stem}.jpg"
            img.save(out / name, quality=rng.randint(82, 94))
            total += 1
    print(f"rendered {total} scenes into {args.out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
