#!/usr/bin/env python3
"""Score a model across the capture envelope a child actually produces.

Why this exists: the Set B failure in issue #67 was not a class-prior problem. The
classifier was trained on cards occupying 52-78% of the frame, and a card held up
to the camera (which is what a child does) scored 0.684 float32 / 0.526 int8, with
hazardous collapsing into recyclable at 0.99 confidence. Validation on the training
distribution could not see that.

Each regime simulates one axis of a real capture without needing a physical card:

    nominal      the training render distribution (reference)
    tight        card fills 95-110% of the frame, sharp, print art
    tight_pearl  the same with the pearl sprite, i.e. what the printed card looks like
    held         card fills 80-95% of the frame over a real photo background
    isp          nominal plus phone sharpening and colour punch, always on
    low_light    brightness x0.55 with sensor noise
    motion       directional blur, as when the card moves during capture
    jpeg60       heavy JPEG compression at 160 px

Every image is generated from seeds, so a run is reproducible and needs no photos
of the cards. It is a simulation, not field accuracy, and the report says so.

    python3 tools/evaluate_capture_stress.py \
        --cards models/wordless-padded/cards \
        --sprites models/wordless-padded/cards-padded \
        --cards-csv models/wordless-padded/cards.csv \
        --backgrounds models/wordless-padded/real-photos \
        --models models/wordless-padded/model-float32.onnx,models/wordless-padded/model-int8.onnx \
        --n 8 --out /tmp/setb/stress

Exits 1 when any regime falls below `--fail-under`, so it can gate a rebuild.
"""

from __future__ import annotations

import argparse
import csv
import io
import json
import random
import sys
from collections import defaultdict
from pathlib import Path
from typing import Iterable, Sequence

LABELS: tuple[str, ...] = ("food", "hazardous", "other", "recyclable")
REGIMES: tuple[str, ...] = ("nominal", "tight", "tight_pearl", "held", "isp", "low_light", "motion", "jpeg60")


# --------------------------------------------------------------------------- #
# pure helpers: no numpy, no PIL, unit-tested in CI
# --------------------------------------------------------------------------- #

def per_class_accuracy(rows: Iterable[dict]) -> dict[str, float]:
    """Recall per true class; classes with no rows are absent, not zero."""
    hits: dict[str, int] = defaultdict(int)
    totals: dict[str, int] = defaultdict(int)
    for row in rows:
        totals[row["truth"]] += 1
        hits[row["truth"]] += int(row["truth"] == row["prediction"])
    return {name: hits[name] / totals[name] for name in sorted(totals)}


def per_card_accuracy(rows: Iterable[dict]) -> dict[str, float]:
    hits: dict[str, int] = defaultdict(int)
    totals: dict[str, int] = defaultdict(int)
    for row in rows:
        if not row["card_id"]:
            continue
        totals[row["card_id"]] += 1
        hits[row["card_id"]] += int(row["truth"] == row["prediction"])
    return {card: hits[card] / totals[card] for card in sorted(totals)}


def confusion_counts(rows: Iterable[dict]) -> dict[str, int]:
    """Confusion pairs as `truth->prediction`, wrong predictions only."""
    counts: dict[str, int] = defaultdict(int)
    for row in rows:
        if row["truth"] != row["prediction"]:
            counts[f"{row['truth']}->{row['prediction']}"] += 1
    return dict(sorted(counts.items(), key=lambda item: -item[1]))


def read_labels(path: Path | None) -> list[str]:
    """Model class order, defaulting to the frozen four buckets.

    `models/wordless-unknown/labels.txt` carries a fifth `unknown` exit, so the
    reported predictions can be a label outside the four buckets; those count as
    misses here and show up in the confusion pairs.
    """
    if path is None:
        return list(LABELS)
    labels = [line.strip() for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]
    missing = set(LABELS) - set(labels)
    if missing:
        raise ValueError(f"{path} is missing the deployment classes {sorted(missing)}")
    return labels


def summarise(rows: Sequence[dict], model: str, fail_under: float = 0.95, labels: Sequence[str] = LABELS) -> dict:
    """Group per-image rows into one report per regime."""
    by_regime: dict[str, list[dict]] = defaultdict(list)
    for row in rows:
        by_regime[row["regime"]].append(row)
    regimes = {}
    for regime in REGIMES:
        subset = by_regime.get(regime, [])
        if not subset:
            continue
        correct = sum(int(row["truth"] == row["prediction"]) for row in subset)
        per_card = per_card_accuracy(subset)
        regimes[regime] = {
            "images": len(subset),
            "accuracy": correct / len(subset),
            "per_class": per_class_accuracy(subset),
            "per_card": per_card,
            "worst_cards": sorted(per_card.items(), key=lambda item: (item[1], item[0]))[:5],
            "confusion": confusion_counts(subset),
            "pass": correct / len(subset) >= fail_under,
        }
    return {
        "model": model,
        "labels": list(labels),
        "regimes": regimes,
        "fail_under": fail_under,
        "worst_regime": min(regimes, key=lambda name: regimes[name]["accuracy"]) if regimes else None,
        "pass": bool(regimes) and all(entry["pass"] for entry in regimes.values()),
        "caveat": "Simulated captures from card artwork; not printed-card or field accuracy.",
    }


def markdown(reports: Sequence[dict]) -> str:
    lines = [
        "| model | regime | images | accuracy | food | hazardous | other | recyclable | worst card | pass |",
        "|---|---|---:|---:|---:|---:|---:|---:|---|---|",
    ]
    for report in reports:
        for regime, entry in report["regimes"].items():
            classes = entry["per_class"]
            worst = entry["worst_cards"][0] if entry["worst_cards"] else ("-", 1.0)
            lines.append(
                "| {model} | {regime} | {images} | {acc:.3f} | {food:.2f} | {haz:.2f} | {other:.2f} | {rec:.2f} | {worst_card} {worst_acc:.2f} | {flag} |".format(
                    model=report["model"],
                    regime=regime,
                    images=entry["images"],
                    acc=entry["accuracy"],
                    food=classes.get("food", 0.0),
                    haz=classes.get("hazardous", 0.0),
                    other=classes.get("other", 0.0),
                    rec=classes.get("recyclable", 0.0),
                    worst_card=worst[0],
                    worst_acc=worst[1],
                    flag="PASS" if entry["pass"] else "FAIL",
                )
            )
    return "\n".join(lines) + "\n"


# --------------------------------------------------------------------------- #
# image simulation: numpy + Pillow
# --------------------------------------------------------------------------- #

def load_backgrounds(root: Path | None) -> list[Path]:
    if root is None:
        return []
    paths = sorted(path for path in root.rglob("*") if path.suffix.lower() in (".jpg", ".jpeg", ".png"))
    if not paths:
        raise SystemExit(f"no background images under {root}")
    return paths


def background(size: int, rng: random.Random, paths: list[Path]):
    from PIL import Image, ImageEnhance, ImageFilter

    if not paths:
        return Image.new("RGB", (size, size), (236, 233, 226))
    with Image.open(rng.choice(paths)) as source:
        image = source.convert("RGB")
        width, height = image.size
        side = min(width, height)
        left = rng.randint(0, max(0, width - side))
        top = rng.randint(0, max(0, height - side))
        image = image.crop((left, top, left + side, top + side)).resize((size, size), Image.LANCZOS)
    image = image.filter(ImageFilter.GaussianBlur(rng.uniform(1.5, 4.0)))
    return ImageEnhance.Brightness(image).enhance(rng.uniform(0.75, 1.05))


def paste_centred(card, size: int, rng: random.Random, low: float, high: float, use_background: bool, paths: list[Path]):
    from PIL import Image

    scene = background(size, rng, paths) if use_background else Image.new("RGB", (size, size), (248, 247, 243))
    target = int(size * rng.uniform(low, high))
    sprite = card.copy()
    sprite.thumbnail((target, target), Image.LANCZOS)
    x = (size - sprite.width) // 2 + rng.randint(-2, 2)
    y = (size - sprite.height) // 2 + rng.randint(-2, 2)
    scene.paste(sprite, (x, y), sprite if sprite.mode == "RGBA" else None)
    return scene


def directional_blur(scene, rng: random.Random):
    from PIL import Image, ImageFilter  # noqa: F401  (Image used by rotate)

    angle = rng.uniform(-30, 30)
    rotated = scene.rotate(angle, resample=Image.BICUBIC).filter(ImageFilter.GaussianBlur(rng.uniform(1.2, 2.2)))
    return rotated.rotate(-angle, resample=Image.BICUBIC)


def jpeg_roundtrip(scene, quality: int):
    from PIL import Image

    buffer = io.BytesIO()
    scene.save(buffer, "JPEG", quality=quality)
    buffer.seek(0)
    return Image.open(buffer).convert("RGB")


def simulate(regime: str, card, size: int, rng: random.Random, paths: list[Path]):
    """One synthetic capture for a regime; deterministic for a given rng state."""
    from PIL import ImageEnhance, ImageFilter

    if regime == "nominal":
        from render_card_scenes import render

        return render(card, size, rng, paths or None)
    if regime == "tight":
        return paste_centred(card, size, rng, 0.95, 1.10, False, paths)
    if regime == "tight_pearl":
        return paste_centred(card, size, rng, 0.95, 1.10, False, paths)
    if regime == "held":
        return paste_centred(card, size, rng, 0.80, 0.95, True, paths)
    if regime == "isp":
        from render_card_scenes import phone_isp, render

        # the camera pipeline applied on top of the render, unconditionally
        return phone_isp(render(card, size, rng, paths or None), rng)
    if regime == "low_light":
        from render_card_scenes import render

        scene = ImageEnhance.Brightness(render(card, size, rng, paths or None)).enhance(0.55)
        scene = scene.filter(ImageFilter.GaussianBlur(0.6))
        return ImageEnhance.Contrast(scene).enhance(1.05)
    if regime == "motion":
        from render_card_scenes import render

        return directional_blur(render(card, size, rng, paths or None), rng)
    if regime == "jpeg60":
        from render_card_scenes import render

        return jpeg_roundtrip(render(card, size, rng, paths or None), 60)
    raise SystemExit(f"unknown regime: {regime}")


def card_source(regime: str) -> str:
    """Which sprite a regime draws: pearl print for tight_pearl, artwork otherwise."""
    return "sprites" if regime == "tight_pearl" else "cards"


# --------------------------------------------------------------------------- #
# inference
# --------------------------------------------------------------------------- #

def infer(model_path: Path, images: Sequence[tuple[str, str, str, object]], size: int, normalise: str,
          labels: Sequence[str] = LABELS) -> list[dict]:
    """Run one model over (regime, card_id, truth, image) and return per-image rows."""
    import numpy as np
    import onnxruntime as ort

    from stc_preprocess import to_array

    session = ort.InferenceSession(str(model_path), providers=["CPUExecutionProvider"])
    input_name = session.get_inputs()[0].name
    rows = []
    for regime, card_id, truth, image in images:
        batch = to_array(image, size, normalise).transpose(2, 0, 1)[None, ...]
        probabilities = np.asarray(session.run(None, {input_name: batch})[0][0], dtype=np.float32)
        prediction = labels[int(probabilities.argmax())]
        rows.append({
            "regime": regime,
            "card_id": card_id,
            "truth": truth,
            "prediction": prediction,
            "confidence": float(probabilities.max()),
        })
    return rows


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--cards", type=Path, required=True, help="card artwork folder")
    parser.add_argument("--sprites", type=Path, required=True, help="pearl sprite folder")
    parser.add_argument("--cards-csv", type=Path, required=True)
    parser.add_argument("--backgrounds", type=Path, default=None)
    parser.add_argument("--models", default="", help="comma separated ONNX paths; empty when only --dump-dir is wanted")
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--n", type=int, default=8, help="captures per card per regime")
    parser.add_argument("--size", type=int, default=160)
    parser.add_argument("--normalise", choices=("div255", "imagenet"), default="div255")
    parser.add_argument("--regimes", default=",".join(REGIMES))
    parser.add_argument("--kind", default="printed", help="cards.csv kind filter; empty string keeps every card")
    parser.add_argument("--labels", type=Path, default=None, help="model labels.txt; default: the four frozen buckets")
    parser.add_argument("--seed", type=int, default=20261119)
    parser.add_argument("--fail-under", type=float, default=0.95)
    parser.add_argument("--dump-dir", type=Path, default=None,
                        help="write the generated captures too, so a .espdl native graph can be scored on identical images")
    args = parser.parse_args()

    from PIL import Image

    regimes = [name.strip() for name in args.regimes.split(",") if name.strip()]
    unknown = sorted(set(regimes) - set(REGIMES))
    if unknown:
        parser.error(f"unknown regimes: {unknown}; expected a subset of {list(REGIMES)}")

    labels = read_labels(args.labels)
    rows_csv = list(csv.DictReader(args.cards_csv.open()))
    cards = [row for row in rows_csv if not args.kind or row["kind"] == args.kind]
    if not cards:
        raise SystemExit(f"no cards of kind {args.kind!r} in {args.cards_csv}")

    backgrounds = load_backgrounds(args.backgrounds)
    images: list[tuple[str, str, str, object]] = []
    for regime in regimes:
        source = args.sprites if card_source(regime) == "sprites" else args.cards
        for row in cards:
            path = source / f"{row['card_id']}.png"
            if not path.exists():
                raise SystemExit(f"missing art: {path}")
            card = Image.open(path).convert("RGBA")
            for index in range(args.n):
                rng = random.Random(f"{args.seed}:{regime}:{row['card_id']}:{index}")
                images.append((regime, row["card_id"], row["class"], simulate(regime, card, args.size, rng, backgrounds)))

    if args.dump_dir:
        for index, (regime, card_id, truth, image) in enumerate(images):
            folder = args.dump_dir / regime / truth
            folder.mkdir(parents=True, exist_ok=True)
            image.save(folder / f"{card_id}_{index % args.n}.png")
        print(f"wrote {len(images)} captures to {args.dump_dir}")

    if not args.models and not args.dump_dir:
        parser.error("pass --models, --dump-dir, or both")

    args.out.mkdir(parents=True, exist_ok=True)
    reports = []
    for model in [item.strip() for item in args.models.split(",") if item.strip()]:
        path = Path(model)
        rows = infer(path, images, args.size, args.normalise, labels)
        with (args.out / f"rows-{path.stem}.csv").open("w", newline="") as handle:
            writer = csv.DictWriter(handle, fieldnames=list(rows[0]), lineterminator="\n")
            writer.writeheader()
            writer.writerows(rows)
        report = summarise(rows, path.stem, args.fail_under, labels)
        report.update({"normalise": args.normalise, "captures_per_card": args.n, "seed": args.seed,
                       "cards": len(cards), "kind": args.kind})
        (args.out / f"stress-{path.stem}.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        reports.append(report)
        print(f"{path.stem}: " + "  ".join(
            f"{name}={entry['accuracy']:.3f}" for name, entry in report["regimes"].items()
        ))

    (args.out / "summary.md").write_text(markdown(reports) + "\n", encoding="utf-8")
    failed = [report["model"] for report in reports if not report["pass"]]
    print(f"\nwrote {args.out}/summary.md")
    if failed:
        print(f"FAIL (below {args.fail_under}): {failed}")
        return 1
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except ValueError as error:
        print(f"error: {error}", file=sys.stderr)
        raise SystemExit(2)
