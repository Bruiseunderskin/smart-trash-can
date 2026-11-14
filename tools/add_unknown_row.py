#!/usr/bin/env python3
"""Add an `unknown` row to a four-class ONNX classifier, leaving the four bins alone.

The deployed artefact is an ONNX graph, and the repo keeps no torch checkpoint for
the current model, so the row is fitted on the graph itself:

  1. read the features feeding the final `Gemm` (the 1280-wide pooled vector);
  2. fit a logistic probe for "no card here" against everything else;
  3. widen the `Gemm` weight/bias from 4 rows to 5 and rename the output shape.

The four known rows and every other initializer are copied byte-for-byte, so the
four logits are unchanged. The probe can still out-score them on a real object,
which is why raw accuracy is reported separately from accuracy among accepted
images.

    python3 tools/add_unknown_row.py --model models/wordless-padded/model-float32.onnx \
        --data models/wordless-padded/dataset --out models/wordless-unknown/model-float32.onnx
"""

from __future__ import annotations

import argparse
from pathlib import Path

UNKNOWN = "unknown"


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


def features(session, paths, size: int, feature_name: str):
    import numpy as np

    # the exported graph fixes the batch dimension at 1, so images go through one at a time
    out = []
    for path in paths:
        out.append(session.run([feature_name], {"images": preprocess(path, size)})[0])
    return np.concatenate(out, axis=0)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", type=Path, required=True, help="four-class float32 ONNX")
    parser.add_argument("--data", type=Path, required=True, help="ImageFolder root with train/unknown")
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--size", type=int, default=160)
    parser.add_argument("--holdout", type=Path, default=None,
                        help="folder of unseen unknown scenes; reported only, never fitted")
    parser.add_argument("--l2", type=float, default=1e-3)
    args = parser.parse_args()

    import numpy as np
    import onnx
    import onnxruntime as ort
    from onnx import helper, numpy_helper

    model = onnx.load(str(args.model))
    graph = model.graph

    gemm = next(node for node in graph.node if node.op_type == "Gemm")
    weight = numpy_helper.to_array(next(i for i in graph.initializer if i.name == gemm.input[1]))
    bias = numpy_helper.to_array(next(i for i in graph.initializer if i.name == gemm.input[2]))
    feature_name = gemm.input[0]
    if weight.shape[0] != 4:
        raise SystemExit(f"expected a four-class head, found {weight.shape[0]} rows")

    probe_model = onnx.load(str(args.model))
    probe_model.graph.output.append(helper.make_tensor_value_info(feature_name, onnx.TensorProto.FLOAT, None))
    session = ort.InferenceSession(probe_model.SerializeToString(), providers=["CPUExecutionProvider"])

    train = args.data / "train"
    paths, labels = [], []
    for folder in sorted(p for p in train.iterdir() if p.is_dir()):
        for path in sorted(folder.rglob("*")):
            if path.suffix.lower() in {".jpg", ".jpeg", ".png"}:
                paths.append(path)
                labels.append(1.0 if folder.name == UNKNOWN else 0.0)
    if not paths or sum(labels) == 0:
        raise SystemExit(f"{train} must hold the four card classes plus {UNKNOWN}")

    x = features(session, paths, args.size, feature_name)
    y = np.asarray(labels, dtype=np.float32)
    print(f"{len(paths)} training images, {int(y.sum())} {UNKNOWN}", flush=True)

    mean = x.mean(0)
    std = x.std(0)
    std[std < 1e-6] = 1.0
    xs = (x - mean) / std
    pos_weight = float((y == 0).sum() / max(1.0, y.sum()))

    # logistic regression by Newton steps on the standardised features
    w = np.zeros(xs.shape[1], dtype=np.float64)
    b = np.zeros(1, dtype=np.float64)
    for _ in range(50):
        logits = xs @ w + b
        p = 1.0 / (1.0 + np.exp(-np.clip(logits, -60, 60)))
        grad_w = xs.T @ ((p - y) * np.where(y > 0, pos_weight, 1.0)) + args.l2 * w
        grad_b = np.sum((p - y) * np.where(y > 0, pos_weight, 1.0))
        p_weighted = p * np.where(y > 0, pos_weight, 1.0)
        hess = (xs * (p_weighted * (1 - p))[:, None]).T @ xs + args.l2 * np.eye(xs.shape[1])
        step = np.linalg.solve(hess, grad_w)
        w -= step
        b -= grad_b / max(1.0, np.trace(hess))
        if np.abs(step).max() < 1e-6:
            break

    w_raw = (w / std).astype(np.float32)
    b_raw = np.asarray([b[0] - float((w * mean / std).sum())], dtype=np.float32)

    new_weight = np.concatenate([weight, w_raw[None, :]], axis=0)
    new_bias = np.concatenate([bias, b_raw], axis=0)

    for index, initializer in enumerate(graph.initializer):
        if initializer.name == gemm.input[1]:
            graph.initializer[index].CopyFrom(numpy_helper.from_array(new_weight, gemm.input[1]))
        elif initializer.name == gemm.input[2]:
            graph.initializer[index].CopyFrom(numpy_helper.from_array(new_bias, gemm.input[2]))

    for value in list(graph.value_info) + list(graph.output):
        if value.name in {gemm.output[0], graph.output[0].name}:
            value.type.tensor_type.shape.dim[1].dim_value = 5
    onnx.checker.check_model(model)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    onnx.save(model, str(args.out))
    print(f"wrote {args.out}")

    scored = ort.InferenceSession(str(args.out), providers=["CPUExecutionProvider"])
    scores = scored.run(None, {"images": preprocess(paths[0], args.size)})[0]
    print(f"smoke output shape {scores.shape}, unknown logit present: {scores.shape[1] == 5}")

    if args.holdout:
        holdout = sorted(p for p in args.holdout.rglob("*") if p.suffix.lower() in {".jpg", ".jpeg", ".png"})
        if holdout:
            probs = np.concatenate([scored.run(None, {"images": preprocess(p, args.size)})[0] for p in holdout], axis=0)
            picked = probs.argmax(1)
            print(f"holdout: {int((picked == 4).sum())}/{len(holdout)} predicted unknown, "
                  f"mean confidence {float(probs.max(1).mean()):.3f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
