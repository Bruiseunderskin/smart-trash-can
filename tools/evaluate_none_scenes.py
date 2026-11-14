#!/usr/bin/env python3
"""Report how a classifier handles scenes with no card.

`unknown` scenes carry no truth label other than "not one of the four bins", so
this tool reports per-group counts instead of one accuracy figure:

  * predicted class distribution,
  * false-accept count at the operating threshold (a bin was offered for an empty scene),
  * confidence p50 / p95.

    python3 tools/evaluate_none_scenes.py --model models/wordless-unknown/model-int8.onnx \
        --dir models/wordless-padded/dataset/holdout --out runs/unknown-v1/eval/holdout.csv
"""

from __future__ import annotations

import argparse
import csv
import json
import statistics
import sys
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
    parser.add_argument("--model", type=Path, required=True, help="ONNX classifier; labels.txt sits beside it")
    parser.add_argument("--dir", type=Path, required=True, help="folder of scenes with no card")
    parser.add_argument("--out", type=Path, default=None, help="per-image CSV")
    parser.add_argument("--size", type=int, default=160)
    parser.add_argument("--threshold", type=float, default=0.55)
    parser.add_argument("--group-by", default="parent", choices=["parent", "file-prefix", "none"],
                        help="how to group the report rows")
    args = parser.parse_args()

    import onnxruntime as ort

    labels = load_labels(args.model)
    session = ort.InferenceSession(str(args.model), providers=["CPUExecutionProvider"])
    input_name = session.get_inputs()[0].name

    images = sorted(p for p in args.dir.rglob("*") if p.suffix.lower() in {".jpg", ".jpeg", ".png"})
    if not images:
        raise SystemExit(f"no images under {args.dir}")

    rows = []
    for path in images:
        probs = session.run(None, {input_name: preprocess(path, args.size)})[0][0]
        index = int(probs.argmax())
        if args.group_by == "parent":
            group = path.parent.name
        elif args.group_by == "file-prefix":
            group = path.stem.split("_")[0]
        else:
            group = "all"
        rows.append({
            "image": str(path.relative_to(args.dir)),
            "group": group,
            "prediction": labels[index],
            "confidence": float(probs[index]),
            "false_accept": int(labels[index] != "unknown" and float(probs[index]) >= args.threshold),
        })

    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        with args.out.open("w", newline="") as handle:
            writer = csv.DictWriter(handle, fieldnames=list(rows[0]), lineterminator="\n")
            writer.writeheader()
            writer.writerows(rows)

    groups: dict[str, list[dict]] = {}
    for row in rows:
        groups.setdefault(row["group"], []).append(row)

    print(f"| group | n | false-accept @{args.threshold} | predicted unknown | p50 conf | p95 conf |")
    print("|---|---:|---:|---:|---:|---:|")
    report = {}
    for group, sub in sorted(groups.items()):
        confs = sorted(row["confidence"] for row in sub)
        fa = sum(row["false_accept"] for row in sub)
        unknown = sum(1 for row in sub if row["prediction"] == "unknown")
        p50 = statistics.median(confs)
        p95 = confs[min(len(confs) - 1, int(round(0.95 * (len(confs) - 1))))]
        print(f"| {group} | {len(sub)} | {fa} ({fa / len(sub):.1%}) | {unknown} | {p50:.3f} | {p95:.3f} |")
        report[group] = {
            "images": len(sub),
            "false_accept": fa,
            "false_accept_rate": fa / len(sub),
            "predicted_unknown": unknown,
            "confidence_p50": p50,
            "confidence_p95": p95,
            "predictions": {name: sum(1 for row in sub if row["prediction"] == name)
                            for name in sorted({row["prediction"] for row in sub})},
        }

    if args.out:
        summary = args.out.with_suffix(".json")
        summary.write_text(json.dumps({
            "model": str(args.model),
            "labels": labels,
            "threshold": args.threshold,
            "groups": report,
        }, indent=2) + "\n")
        print(f"\nwrote {args.out} and {summary}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
