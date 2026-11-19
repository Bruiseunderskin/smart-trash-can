#!/usr/bin/env python3
"""Render training scenes from the padded classification cards.

The original simulated-card generator was never committed, so this is a small
reimplementation whose job is to cover the captures the classifier will see:

  * scale 0.45-1.15 of the frame — the card may sit small on a desk, fill the
    frame, or run off the edge after the camera's 4:3-to-square crop. The old
    0.52-0.78 window made the model depend on background being visible, and a
    card held up to the lens scored 0.68 float32 / 0.53 int8 on the printed Set B
    cards (see docs/experiments/setb-capture-envelope.md);
  * viewpoint, shear, light, blur, glare, occlusion and colour cast vary, so the
    model learns the motif rather than one fixed crop;
  * the pearl stock's sheen is re-applied per scene (`relight`), because a baked
    sheen turns into a card-identity cue that a real card does not have;
  * a camera pipeline (`phone_isp`) and sensor noise are applied to half the
    scenes, because phones sharpen and saturate before the model ever sees the
    frame.

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

import numpy as np
from PIL import Image, ImageDraw, ImageEnhance, ImageFilter

LABELS = ("recyclable", "food", "hazardous", "other")

# Card size as a fraction of the frame. The old range stopped at 0.78, which made
# the classifier depend on background being visible: a card filling the frame
# (a child holding it up to the camera) scored 0.68 float32 / 0.53 int8, with
# hazardous collapsing into recyclable. 1.15 allows the card to run off the frame,
# which is what the 4:3-to-square camera crop does.
SCALE_RANGE = (0.45, 1.15)


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


def relight(card: Image.Image, rng: random.Random) -> Image.Image:
    """Slide a broad specular sheen across the card.

    The pearl stock reflects the room, so the same physical card looks different
    from one shot to the next. `rebuild_wordless_cards.pearl_paper` bakes one
    sheen per artwork (seeded by the card hash), which turns the sheen into an
    identity cue that does not exist on a real card and disappears when the light
    moves. This puts a per-scene low-frequency gain on top, so the model has to
    read the motif rather than the reflection.
    """
    strength = rng.uniform(0.0, 0.16)
    if strength < 0.02:
        return card
    ramp = Image.linear_gradient("L").resize(card.size, Image.BILINEAR)
    ramp = ramp.rotate(rng.uniform(0, 360), resample=Image.BILINEAR)
    gain = 1.0 - strength + 2.0 * strength * (np.asarray(ramp, dtype=np.float32) / 255.0)[:, :, None]
    pixels = np.asarray(card, dtype=np.float32).copy()
    pixels[:, :, :3] = np.clip(pixels[:, :, :3] * gain, 0, 255)
    return Image.fromarray(pixels.astype(np.uint8), "RGBA")


def phone_isp(scene: Image.Image, rng: random.Random) -> Image.Image:
    """Approximate the camera pipeline between the sensor and the JPEG.

    Phones sharpen and punch saturation before the frame ever reaches the model.
    Without this, a sharpened capture of the same card lands outside the training
    distribution: `art at 100% of frame + unsharp` dropped float32 to 0.895 and
    int8 to 0.706 on cards the render distribution scores 1.000.
    """
    scene = scene.filter(
        ImageFilter.UnsharpMask(radius=rng.uniform(1.0, 2.0), percent=rng.randint(50, 150), threshold=2)
    )
    scene = ImageEnhance.Color(scene).enhance(rng.uniform(1.0, 1.25))
    return ImageEnhance.Contrast(scene).enhance(rng.uniform(1.0, 1.12))


def render(
    card: Image.Image,
    size: int,
    rng: random.Random,
    bg_paths: list[Path] | None = None,
    scale_range: tuple[float, float] = SCALE_RANGE,
) -> Image.Image:
    scene = make_background(size, rng, bg_paths)

    target = int(size * rng.uniform(*scale_range))
    c = relight(card.copy(), rng)
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
    scene = ImageEnhance.Brightness(scene).enhance(rng.uniform(0.70, 1.12))
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
        bands = [scene.getchannel(i).point(lambda v, gain=gains[i]: min(255, int(v * gain))) for i in range(3)]
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

    # sensor noise, then the camera's own sharpening and colour punch
    if rng.random() < 0.6:
        noise = np.asarray(scene, dtype=np.float32)
        noise += np.random.default_rng(rng.randrange(1 << 30)).normal(0, rng.uniform(1.0, 5.0), noise.shape[:2])[:, :, None]
        scene = Image.fromarray(np.clip(noise, 0, 255).astype(np.uint8), "RGB")
    if rng.random() < 0.5:
        scene = phone_isp(scene, rng)
    return scene


SURFACES = (
    ("desk_white", (236, 233, 226), (222, 218, 210)),
    ("concrete_grey", (150, 148, 144), (128, 126, 122)),
    ("wood_warm", (186, 140, 96), (168, 122, 82)),
    ("wood_dark", (104, 76, 54), (88, 64, 46)),
    ("bin_plastic", (58, 62, 66), (44, 48, 52)),
    ("fabric_blue", (96, 112, 134), (82, 98, 120)),
    ("fabric_green", (104, 122, 96), (90, 108, 84)),
    ("wall_cream", (214, 206, 186), (198, 190, 170)),
    ("metal_brushed", (168, 170, 172), (146, 148, 152)),
    ("floor_tile", (196, 190, 180), (176, 170, 160)),
)


def make_surface(size: int, rng: random.Random) -> Image.Image:
    """A procedural empty surface: desk, wall, floor, bin interior or fabric.

    The repo's photo pool is all garbage close-ups, so cropping a real photo for an
    "empty" scene always put an object back in the frame. Surfaces are drawn instead.
    """
    name, base, band = rng.choice(SURFACES)
    img = Image.new("RGB", (size, size), base)
    draw = ImageDraw.Draw(img)

    if name.startswith("wood") or name == "floor_tile":
        # planks or tiles, with a seam every so often
        step = rng.randint(max(12, size // 14), max(20, size // 6))
        horizontal = name != "floor_tile" or rng.random() < 0.5
        for offset in range(-size, size * 2, step):
            shade = tuple(max(0, min(255, c + rng.randint(-16, 16))) for c in band)
            if horizontal:
                draw.line([(0, offset), (size, offset + rng.randint(-3, 3))], fill=shade, width=max(1, step // 12))
            else:
                draw.line([(offset, 0), (offset + rng.randint(-3, 3), size)], fill=shade, width=max(1, step // 12))
        if name == "floor_tile":
            grid = rng.randint(size // 5, size // 2)
            shade = tuple(max(0, min(255, c + rng.randint(-10, 10))) for c in band)
            for x in range(0, size + grid, grid):
                draw.line([(x, 0), (x, size)], fill=shade, width=2)
            for y in range(0, size + grid, grid):
                draw.line([(0, y), (size, y)], fill=shade, width=2)
    elif name.startswith("fabric"):
        # weave: fine cross hatching plus a couple of soft folds
        for offset in range(0, size, max(3, size // 40)):
            shade = tuple(max(0, min(255, c + rng.randint(-8, 8))) for c in band)
            draw.line([(0, offset), (size, offset)], fill=shade, width=1)
        for _ in range(rng.randint(1, 3)):
            y = rng.randint(0, size)
            thickness = rng.randint(size // 20, size // 6)
            shade = tuple(max(0, min(255, c + rng.randint(-14, 14))) for c in band)
            draw.rectangle([0, y, size, y + thickness], fill=shade)
    elif name == "metal_brushed":
        for offset in range(0, size, max(2, size // 80)):
            shade = tuple(max(0, min(255, c + rng.randint(-6, 6))) for c in band)
            draw.line([(0, offset), (size, offset)], fill=shade, width=1)
    else:
        # plain desk/wall: broad soft bands plus a vignette later
        for _ in range(rng.randint(2, 5)):
            y = rng.randint(0, size)
            h = rng.randint(max(4, size // 12), max(8, size // 4))
            shade = tuple(max(0, min(255, c + rng.randint(-10, 10))) for c in base)
            draw.rectangle([0, y, size, y + h], fill=shade)

    # smudges and stains
    for _ in range(rng.randint(0, 4)):
        layer = Image.new("RGBA", (size, size), (0, 0, 0, 0))
        ld = ImageDraw.Draw(layer)
        cx, cy = rng.randint(0, size), rng.randint(0, size)
        rx, ry = rng.randint(size // 20, size // 3), rng.randint(size // 20, size // 3)
        tint = rng.choice([(255, 255, 255, rng.randint(20, 60)), (0, 0, 0, rng.randint(15, 50)),
                           (150, 120, 90, rng.randint(15, 45))])
        ld.ellipse([cx - rx, cy - ry, cx + rx, cy + ry], fill=tint)
        layer = layer.filter(ImageFilter.GaussianBlur(size / 18))
        img = Image.alpha_composite(img.convert("RGBA"), layer).convert("RGB")

    # surface grain
    sigma = rng.uniform(1.5, 7.0)
    noise = np.random.default_rng(rng.randrange(1 << 30)).normal(0, sigma, (size, size, 3))
    array = np.asarray(img, dtype=np.float32) + noise
    img = Image.fromarray(np.clip(array, 0, 255).astype(np.uint8))
    return img.filter(ImageFilter.GaussianBlur(rng.uniform(0.0, 1.2)))


def render_empty(size: int, rng: random.Random) -> Image.Image:
    """A scene with no card: an empty surface, a covered lens, or hostile light.

    Used for the `unknown` class, so the model has an exit for "no card here".
    The frame is rendered larger and cropped back, so tilting never leaves black corners.
    """
    # 3x margin: rotating a square costs 1/(cos+sin) of its width, and shearing moves the
    # content by up to shear*h/2, so a 1.5x frame still let black corners into the crop.
    big = int(size * 3)
    scene = make_surface(big, rng)

    # hand, sleeve or cloth over the lens: near-uniform, softly shaded
    if rng.random() < 0.10:
        base = rng.choice([(196, 168, 148), (120, 112, 104), (70, 72, 78), (150, 96, 88), (232, 228, 219)])
        scene = Image.new("RGB", (big, big), base)
        draw = ImageDraw.Draw(scene)
        for _ in range(rng.randint(2, 5)):
            shade = tuple(max(0, min(255, c + rng.randint(-40, 40))) for c in base)
            x0 = rng.randint(-big // 3, big)
            y0 = rng.randint(-big // 3, big)
            draw.ellipse(
                [x0, y0, x0 + rng.randint(big // 2, big), y0 + rng.randint(big // 2, big)],
                fill=shade,
            )
        scene = scene.filter(ImageFilter.GaussianBlur(rng.uniform(2.0, 8.0)))

    # camera tilt: the whole frame rotates and shears, then crops back to size.
    # Both ops sample a little outside the rotated square; filling with the surface
    # colour keeps those slivers from becoming black wedges.
    fill = tuple(int(v) for v in np.asarray(scene, dtype=np.float32).reshape(-1, 3).mean(axis=0))
    scene = scene.rotate(rng.uniform(-20, 20), resample=Image.BICUBIC, expand=True, fillcolor=fill)
    shear = rng.uniform(-0.15, 0.15)
    scene = scene.transform(
        scene.size,
        Image.AFFINE,
        (1, shear, -shear * scene.height / 2, 0, 1, 0),
        resample=Image.BICUBIC,
        fillcolor=fill,
    )
    left = max(0, (scene.width - size) // 2)
    top = max(0, (scene.height - size) // 2)
    scene = scene.crop((left, top, left + size, top + size))

    # light: mostly normal, 15% of frames are dim as an unlit room or a covered lens
    dim = rng.random() < 0.10
    scene = ImageEnhance.Brightness(scene).enhance(rng.uniform(0.35, 0.55) if dim else rng.uniform(0.55, 1.60))
    scene = ImageEnhance.Contrast(scene).enhance(rng.uniform(0.70, 1.50))

    # colour temperature of the room lamp
    gains = [rng.uniform(0.80, 1.20) for _ in range(3)]
    bands = [scene.getchannel(i).point(lambda v, g=gains[i]: min(255, int(v * g))) for i in range(3)]
    scene = Image.merge("RGB", bands)

    # shadow falling across the surface
    if rng.random() < 0.6:
        yy, xx = np.mgrid[0:size, 0:size].astype(np.float32)
        theta = rng.uniform(0, 2 * np.pi)
        ramp = np.cos(theta) * (xx - size / 2) + np.sin(theta) * (yy - size / 2)
        spread = float(ramp.max() - ramp.min())
        ramp = (ramp - ramp.min()) / spread if spread > 1e-6 else ramp * 0
        shade = 1.0 - rng.uniform(0.0, 0.40) * ramp
        array = np.asarray(scene, dtype=np.float32) * shade[..., None]
        scene = Image.fromarray(np.clip(array, 0, 255).astype(np.uint8))

    # specular hotspots: a lamp or a window landing on the table
    for _ in range(rng.choice([0, 0, 1, 1, 2])):
        glare = Image.new("L", (size, size), 0)
        gd = ImageDraw.Draw(glare)
        cx = rng.randint(-size // 4, size + size // 4)
        cy = rng.randint(-size // 4, size + size // 4)
        rx = rng.randint(size // 8, max(size // 8 + 1, int(size * 0.30)))
        ry = rng.randint(size // 8, max(size // 8 + 1, int(size * 0.30)))
        gd.ellipse([cx - rx, cy - ry, cx + rx, cy + ry], fill=rng.randint(90, 210))
        glare = glare.filter(ImageFilter.GaussianBlur(size / 10))
        scene = Image.composite(Image.new("RGB", (size, size), (255, 255, 255)), scene, glare)

    heavy = rng.random() < 0.20
    scene = scene.filter(ImageFilter.GaussianBlur(rng.uniform(0.8, 3.5) if heavy else rng.uniform(0.0, 1.2)))

    if rng.random() < 0.25:
        angle = rng.uniform(-25, 25)
        blurred = scene.rotate(angle, resample=Image.BICUBIC).filter(
            ImageFilter.GaussianBlur(rng.uniform(1.0, 2.6))
        ).rotate(-angle, resample=Image.BICUBIC)
        scene = Image.blend(scene, blurred, rng.uniform(0.45, 0.8))

    sigma = rng.uniform(0, 12)
    if sigma > 0.5:
        noise = np.random.default_rng(rng.randrange(1 << 30)).normal(0, sigma, (size, size, 3))
        array = np.asarray(scene, dtype=np.float32) + noise
        scene = Image.fromarray(np.clip(array, 0, 255).astype(np.uint8))
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
