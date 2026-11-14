#!/usr/bin/env python3
"""Score a five-class classifier on labelled card scenes, separating abstain from wrong.

The deployment question is not one accuracy number: a card may be rejected
(`unknown` or below the threshold) instead of misclassified, and those two cases
cost differently. This prints both, per class, plus a per-image CSV.

    python3 tools/evaluate_cards.py --model models/wordless-unknown/model-int8.onnx \
        --dir models/wordless-padded/dataset/val \
        --out runs/unknown-v1/eval/val-float32.csv
"""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path


def load_labels(model: Path) -> list[str]:
    path = model.parent / "labels.txt"
    if not path.is_file():
        raise SystemExit(f"missing {path}")
    return [line.strip() for line in path.read_text().splitlines() if line.strip()]


def preprocess(path: Path, size: int):
    import numpy as np
    from PIL import Image

    with Image.open(path) as raw:
        image = raw.convert("RGB")
        side = min(image.size)
        left = (image.width - side) // 2
        top = (image.height - side) // 2
        image = image.crop((left, top, left + side, top + side)).resize((size, size), Image.BILINEAR)
        array = np.asarray(image, dtype=np.float32) / 255.0
    return array.transpose(2, 0, 1)[None]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", type=Path, required=True)
    parser.add_argument("--dir", type=Path, required=True, help="ImageFolder root: one folder per class")
    parser.add_argument("--out", type=Path, default=None)
    parser.add_argument("--size", type=int, default=160)
    parser.add_argument("--threshold", type=float, default=0.55)
    args = parser.parse_args()

    import onnxruntime as ort

    labels = load_labels(args.model)
    abstain_index = labels.index("unknown") if "unknown" in labels else None
    session = ort.InferenceSession(str(args.model), providers=["CPUExecutionProvider"])
    input_name = session.get_inputs()[0].name

    rows = []
    for folder in sorted(p for p in args.dir.iterdir() if p.is_dir()):
        if folder.name not in labels:
            continue
        for path in sorted(folder.rglob("*")):
            if path.suffix.lower() not in {".jpg", ".jpeg", ".png"}:
                continue
            probs = session.run(None, {input_name: preprocess(path, args.size)})[0][0]
            index = int(probs.argmax())
            confidence = float(probs[index])
            rejected = confidence < args.threshold or index == abstain_index
            rows.append({
                "image": str(path.relative_to(args.dir)),
                "truth": folder.name,
                "prediction": labels[index],
                "confidence": confidence,
                "abstained": int(rejected),
                "correct": int(not rejected and labels[index] == folder.name),
            })

    if not rows:
        raise SystemExit(f"no labelled images under {args.dir}")

    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        with args.out.open("w", newline="") as handle:
            writer = csv.DictWriter(handle, fieldnames=list(rows[0]), lineterminator="\n")
            writer.writeheader()
            writer.writerows(rows)

    print("| truth | n | correct | wrong | abstained | recall on accepted |")
    print("|---|---:|---:|---:|---:|---:|")
    summary = {}
    for truth in sorted({row["truth"] for row in rows}):
        sub = [row for row in rows if row["truth"] == truth]
        correct = sum(row["correct"] for row in sub)
        abstained = sum(row["abstained"] for row in sub)
        wrong = len(sub) - correct - abstained
        accepted = len(sub) - abstained
        recall = f"{correct / accepted:.3f}" if accepted else "n/a"
        print(f"| {truth} | {len(sub)} | {correct} | {wrong} | {abstained} | {recall} |")
        summary[truth] = {"images": len(sub), "correct": correct, "wrong": wrong,
                          "abstained": abstained}

    total = len(rows)
    accepted = total - sum(row["abstained"] for row in rows)
    correct = sum(row["correct"] for row in rows)
    print(f"\n{total} images, {accepted} accepted ({accepted / total:.1%}), "
          f"{correct}/{accepted} correct among accepted, "
          f"{total - correct - sum(row['abstained'] for row in rows)} wrong, "
          f"{sum(row['abstained'] for row in rows)} abstained")
    if args.out:
        args.out.with_suffix(".json").write_text(json.dumps({
            "model": str(args.model),
            "dir": str(args.dir),
            "threshold": args.threshold,
            "per_class": summary,
            "totals": {"images": total, "accepted": accepted, "correct": correct},
        }, indent=2) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
