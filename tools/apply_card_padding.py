#!/usr/bin/env python3
"""Add a padded frame around a classification card.

Why: the printed cards currently run the artwork to the paper edge, so the whole
card reads as "all white". The teaching staff asked for a padded sample — a
white card sitting inside a thick rounded frame, like the reference image.

Geometry is taken from that reference (all fractions of the card width):

    outer margin   2.5%   (transparent, keeps the printer bleed)
    frame          9.5%   (solid colour, rounded corners)
    inner margin   3.0%   (white, so the art never touches the frame)
    outer radius   13.0%
    inner radius    8.0%

Usage:
    python3 tools/apply_card_padding.py docs/images/cards/set-a docs/images/cards/set-a-padded
"""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image, ImageDraw

FRAME_COLOUR = (63, 127, 79, 255)
OUTER_MARGIN = 0.025
FRAME = 0.095
INNER_MARGIN = 0.030
OUTER_RADIUS = 0.130
INNER_RADIUS = 0.080


def pad_card(source: Path, out_size: int = 709) -> Image.Image:
    card = Image.open(source).convert("RGBA").resize((out_size, out_size), Image.LANCZOS)

    canvas = Image.new("RGBA", (out_size, out_size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(canvas)

    outer_m = int(round(out_size * OUTER_MARGIN))
    frame_w = int(round(out_size * FRAME))
    inner_m = int(round(out_size * INNER_MARGIN))

    outer_box = (outer_m, outer_m, out_size - outer_m - 1, out_size - outer_m - 1)
    draw.rounded_rectangle(
        outer_box,
        radius=int(round(out_size * OUTER_RADIUS)),
        fill=FRAME_COLOUR,
    )

    inner_box = (
        outer_m + frame_w,
        outer_m + frame_w,
        out_size - outer_m - frame_w - 1,
        out_size - outer_m - frame_w - 1,
    )
    draw.rounded_rectangle(
        inner_box,
        radius=int(round(out_size * INNER_RADIUS)),
        fill=(255, 255, 255, 255),
    )

    art_box = (
        inner_box[0] + inner_m,
        inner_box[1] + inner_m,
        inner_box[2] - inner_m,
        inner_box[3] - inner_m,
    )
    art_w = art_box[2] - art_box[0] + 1
    art_h = art_box[3] - art_box[1] + 1

    art = card.resize((art_w, art_h), Image.LANCZOS)
    mask = Image.new("L", (art_w, art_h), 0)
    ImageDraw.Draw(mask).rounded_rectangle(
        (0, 0, art_w - 1, art_h - 1),
        radius=int(round(out_size * INNER_RADIUS * 0.8)),
        fill=255,
    )
    canvas.paste(art, (art_box[0], art_box[1]), mask)
    return canvas


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source_dir", type=Path)
    parser.add_argument("out_dir", type=Path)
    parser.add_argument("--size", type=int, default=709)
    parser.add_argument("--only", nargs="*", default=None, help="subset of file names")
    args = parser.parse_args()

    args.out_dir.mkdir(parents=True, exist_ok=True)
    files = sorted(p for p in args.source_dir.glob("*.png"))
    # sheet/mockup files are composites of the cards, not cards themselves
    files = [p for p in files if "sheet" not in p.stem.lower() and "mockup" not in p.stem.lower()]
    if args.only:
        wanted = set(args.only)
        files = [p for p in files if p.name in wanted]

    for path in files:
        padded = pad_card(path, args.size)
        padded.save(args.out_dir / path.name)
        print(f"{path.name} -> {args.out_dir / path.name}")
    print(f"{len(files)} cards padded")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
