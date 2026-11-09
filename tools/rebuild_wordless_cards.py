#!/usr/bin/env python3
"""Rebuild the wordless card faces with a padded frame and an enlarged motif.

Input is the card art that ships with the wordless candidate; those files still
carry the old white border and a label band. This tool

  1. drops a bottom label band when it is a flat strip,
  2. trims the white margin to the motif's bounding box,
  3. enlarges the motif to fill the card,
  4. wraps it in the padded frame (white card inside a rounded green border).

Usage:
    python3 tools/rebuild_wordless_cards.py <src-dir> <out-dir> [--motif 0.78]
"""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageStat

FRAME_COLOUR = (63, 127, 79, 255)
OUTER_MARGIN = 0.025
FRAME = 0.095
INNER_MARGIN = 0.015
OUTER_RADIUS = 0.130
INNER_RADIUS = 0.080


def trim_label_band(img: Image.Image) -> Image.Image:
    """Drop the old name band across the bottom, even when it carries text.

    The band is a wide, uniformly tinted strip; text on top of it only changes a
    minority of pixels, so the row median stays close to the band colour while
    the row coverage is high.
    """
    w, h = img.size
    px = img.convert("RGB").load()

    def row_is_band(y: int) -> bool:
        samples = [px[x, y] for x in range(0, w, 2)]
        med = tuple(sorted(c[i] for c in samples)[len(samples) // 2] for i in range(3))
        if min(255 - v for v in med) < 6:  # near-white row
            return False
        tinted = sum(1 for c in samples if max(abs(c[i] - med[i]) for i in range(3)) < 12)
        return tinted / len(samples) > 0.6

    start = h
    for y in range(h - 1, int(h * 0.55), -1):
        if row_is_band(y):
            start = y
        elif start != h:
            break
    if start == h or (h - start) < int(h * 0.06):
        return img
    return img.crop((0, 0, w, max(1, start - 1)))


def motif_bbox(img: Image.Image, threshold: int = 18) -> tuple[int, int, int, int]:
    white = Image.new("RGB", img.size, (255, 255, 255))
    diff = ImageChops.difference(img, white).convert("L").point(lambda v: 255 if v > threshold else 0)
    return diff.getbbox() or (0, 0, img.width, img.height)


def build(src: Path, out_size: int, motif_ratio: float) -> Image.Image:
    img = Image.open(src).convert("RGB")
    img = trim_label_band(img)

    box = motif_bbox(img)
    pad = 4
    box = (max(0, box[0] - pad), max(0, box[1] - pad),
           min(img.width, box[2] + pad), min(img.height, box[3] + pad))
    motif = img.crop(box)
    # the source cards carry a thin grey rounded border; shave it so it does not
    # reappear as a stray line inside the padded card
    shave = max(1, int(round(min(motif.size) * 0.02)))
    if motif.width > shave * 3 and motif.height > shave * 3:
        motif = motif.crop((shave, shave, motif.width - shave, motif.height - shave))

    canvas = Image.new("RGBA", (out_size, out_size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(canvas)
    outer_m = int(round(out_size * OUTER_MARGIN))
    frame_w = int(round(out_size * FRAME))
    inner_m = int(round(out_size * INNER_MARGIN))

    draw.rounded_rectangle(
        (outer_m, outer_m, out_size - outer_m - 1, out_size - outer_m - 1),
        radius=int(round(out_size * OUTER_RADIUS)),
        fill=FRAME_COLOUR,
    )
    inner = (outer_m + frame_w, outer_m + frame_w,
             out_size - outer_m - frame_w - 1, out_size - outer_m - frame_w - 1)
    draw.rounded_rectangle(inner, radius=int(round(out_size * INNER_RADIUS)), fill=(255, 255, 255, 255))

    art_w = inner[2] - inner[0] + 1
    art_h = inner[3] - inner[1] + 1
    target = int(min(art_w, art_h) * motif_ratio)

    scale = min(target / motif.width, target / motif.height)
    new = (max(1, int(motif.width * scale)), max(1, int(motif.height * scale)))
    motif = motif.resize(new, Image.LANCZOS)

    x = inner[0] + (art_w - new[0]) // 2
    y = inner[1] + (art_h - new[1]) // 2
    canvas.paste(motif, (x, y))
    return canvas


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("src_dir", type=Path)
    parser.add_argument("out_dir", type=Path)
    parser.add_argument("--size", type=int, default=709)
    parser.add_argument("--motif", type=float, default=0.78)
    args = parser.parse_args()

    args.out_dir.mkdir(parents=True, exist_ok=True)
    files = sorted(p for p in args.src_dir.iterdir() if p.suffix.lower() in (".png", ".jpg") and p.stem[:1].isalpha())
    for path in files:
        out = build(path, args.size, args.motif)
        out.save(args.out_dir / f"{path.stem}.png")
    print(f"rebuilt {len(files)} cards into {args.out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
