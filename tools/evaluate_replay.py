#!/usr/bin/env python3
"""逐圖評估重播影像，只印逐圖表，不印單一總分（#53 I1）。

    python3 tools/evaluate_replay.py --dir data/field-probe \
        --model models/wordless-padded/model-int8.onnx

輸出欄位：file / truth / pred / conf / correct。逐圖列出，方便對照，不提供
單一準確率捷徑——要看表現自己數表。
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

DEFAULT_LABELS = ["food", "hazardous", "other", "recyclable", "unknown"]


def model_labels(model: Path) -> list[str]:
    """Class order comes from the model directory's labels.txt, not from this file."""
    path = model.parent / "labels.txt"
    if not path.is_file():
        return DEFAULT_LABELS
    names = [line.strip() for line in path.read_text().splitlines() if line.strip()]
    return names or DEFAULT_LABELS


def load_session(model: Path):
    import onnxruntime as ort

    return ort.InferenceSession(str(model), providers=["CPUExecutionProvider"])


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
    parser.add_argument("--dir", type=Path, default=Path("data/field-probe"))
    parser.add_argument("--model", type=Path, default=Path("models/wordless-padded/model-int8.onnx"))
    parser.add_argument("--size", type=int, default=160)
    args = parser.parse_args()

    if not args.dir.is_dir():
        print(f"missing {args.dir}", file=sys.stderr)
        return 2
    session = load_session(args.model)
    input_name = session.get_inputs()[0].name
    LABELS = model_labels(args.model)

    images = sorted(p for p in args.dir.rglob("*") if p.suffix.lower() in {".jpg", ".jpeg", ".png"})
    print("| file | truth | pred | conf | correct |")
    print("|---|---|---|---|---|")
    for path in images:
        truth = path.parent.name
        probs = session.run(None, {input_name: preprocess(path, args.size)})[0][0]
        index = int(probs.argmax())
        print(f"| {path.name} | {truth} | {LABELS[index]} | {float(probs[index]):.4f} | "
              f"{'yes' if LABELS[index] == truth else 'no'} |")
    print(f"\n{len(images)} images listed; no aggregate score is printed on purpose.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
