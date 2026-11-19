#!/usr/bin/env python3
"""Evaluate a saved ESP-PPQ native graph, retaining one row per image.

The native graph mirrors the exported `.espdl`, so this is the host-side check of
the quantised model. It is not device execution: no S31, no ESP-NN kernels.

Input space comes from `stc_preprocess`, the same definition the ONNX runner and
the calibration path use. Before that change this script fed ImageNet-normalised
values to a graph that starts at `Conv` and expects 0-1 RGB, so every int8 number
it produced described a different input than the firmware feeds.

    python3 tools/evaluate_espdl_native.py \
        --graph /tmp/setb/wordless_s31.native \
        --data /tmp/setb/val --out /tmp/setb/espdl-val
"""

from __future__ import annotations

import argparse
import csv
import json
import sys
from pathlib import Path

import torch
from torch.utils.data import DataLoader

sys.path.insert(0, str(Path(__file__).resolve().parent))

from quantize_espdl import TestDataset, contract_transform, image_files  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--graph", type=Path, required=True)
    parser.add_argument("--data", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--labels", type=Path, default=None, help="class order of the model; defaults to the data folders")
    parser.add_argument("--imgsz", type=int, default=160)
    parser.add_argument("--normalise", choices=("div255", "imagenet"), default="div255")
    args = parser.parse_args()

    files, labels, names = image_files(args.data)
    if args.labels:
        model_names = [line.strip() for line in args.labels.read_text(encoding="utf-8").splitlines() if line.strip()]
        if not set(names) <= set(model_names):
            raise SystemExit(f"data classes {names} missing from {args.labels}")
        names = model_names

    from esp_ppq.executor.torch import TorchExecutor
    from esp_ppq.parser.native import NativeImporter

    loader = DataLoader(
        TestDataset(files, labels, contract_transform(args.imgsz, args.normalise)),
        batch_size=1, shuffle=False, num_workers=0,
    )
    graph = NativeImporter().build(str(args.graph))
    executor = TorchExecutor(graph=graph, device="cpu")
    torch.set_num_threads(2)

    rows = []
    matrix = [[0] * len(names) for _ in names]
    with torch.no_grad():
        for index, (x, y) in enumerate(loader):
            result = executor(x)
            result = result[0] if isinstance(result, (list, tuple)) else result
            values = result[0].float()
            prediction = int(values.argmax())
            truth = int(y[0])
            matrix[truth][prediction] += 1
            rows.append({
                "image": str(files[index].relative_to(args.data)),
                "truth": names[truth],
                "prediction": names[prediction],
                "confidence": float(values[prediction]),
                "correct": int(truth == prediction),
            })

    args.out.parent.mkdir(parents=True, exist_ok=True)
    with args.out.with_suffix(".csv").open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0]), lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)
    summary = {
        "backend": "ESP-PPQ native graph CPU simulation (not device execution)",
        "normalise": args.normalise,
        "classes": names,
        "images": len(rows),
        "correct": sum(row["correct"] for row in rows),
        "confusion_matrix_rows_truth_columns_prediction": matrix,
    }
    args.out.with_suffix(".json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(summary))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
