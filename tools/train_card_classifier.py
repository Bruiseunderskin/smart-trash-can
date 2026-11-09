#!/usr/bin/env python3
"""Train a 4-class card classifier on the rendered card scenes (CPU friendly).

This exists so training can run on a hosted CPU runner (GitHub Actions) while the
Mac is unavailable. It trains MobileNetV3-small on 160x160 scenes rendered from
the padded cards, then writes float32 and dynamic-int8 ONNX plus a metrics file.

    python3 tools/train_card_classifier.py --data /tmp/dataset --out runs/card-cls
"""

from __future__ import annotations

import argparse
import json
import time
from pathlib import Path

import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import DataLoader
from torchvision import datasets, models, transforms

LABELS = ("food", "hazardous", "other", "recyclable")
IMAGENET_MEAN = (0.485, 0.456, 0.406)
IMAGENET_STD = (0.229, 0.224, 0.225)


def build_loaders(root: Path, size: int, batch: int, workers: int):
    train_tf = transforms.Compose([
        transforms.Resize((size, size)),
        transforms.RandomHorizontalFlip(),
        transforms.ColorJitter(0.2, 0.2, 0.1),
        transforms.ToTensor(),
        transforms.Normalize(IMAGENET_MEAN, IMAGENET_STD),
    ])
    val_tf = transforms.Compose([
        transforms.Resize((size, size)),
        transforms.ToTensor(),
        transforms.Normalize(IMAGENET_MEAN, IMAGENET_STD),
    ])
    train = datasets.ImageFolder(root / "train", transform=train_tf)
    val = datasets.ImageFolder(root / "val", transform=val_tf)
    if train.classes != list(LABELS):
        raise SystemExit(f"unexpected class order {train.classes}, expected {LABELS}")
    return (
        DataLoader(train, batch_size=batch, shuffle=True, num_workers=workers),
        DataLoader(val, batch_size=batch, shuffle=False, num_workers=workers),
        train.classes,
    )


def macro_f1(y_true: np.ndarray, y_pred: np.ndarray, n: int) -> float:
    scores = []
    for c in range(n):
        tp = np.sum((y_pred == c) & (y_true == c))
        fp = np.sum((y_pred == c) & (y_true != c))
        fn = np.sum((y_pred != c) & (y_true == c))
        precision = tp / (tp + fp) if tp + fp else 0.0
        recall = tp / (tp + fn) if tp + fn else 0.0
        scores.append(2 * precision * recall / (precision + recall) if precision + recall else 0.0)
    return float(np.mean(scores))


def evaluate(model, loader, device):
    model.eval()
    y_true, y_pred = [], []
    with torch.no_grad():
        for images, labels in loader:
            logits = model(images.to(device))
            y_pred.extend(logits.argmax(1).cpu().numpy().tolist())
            y_true.extend(labels.numpy().tolist())
    return np.array(y_true), np.array(y_pred)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--epochs", type=int, default=6)
    parser.add_argument("--batch", type=int, default=32)
    parser.add_argument("--size", type=int, default=160)
    parser.add_argument("--lr", type=float, default=3e-4)
    parser.add_argument("--workers", type=int, default=2)
    parser.add_argument("--seed", type=int, default=7)
    args = parser.parse_args()

    torch.manual_seed(args.seed)
    np.random.seed(args.seed)
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")

    train_loader, val_loader, classes = build_loaders(args.data, args.size, args.batch, args.workers)
    model = models.mobilenet_v3_small(weights=models.MobileNet_V3_Small_Weights.IMAGENET1K_V1)
    model.classifier[-1] = nn.Linear(model.classifier[-1].in_features, len(classes))
    model.to(device)

    criterion = nn.CrossEntropyLoss()
    optimiser = torch.optim.AdamW(model.parameters(), lr=args.lr, weight_decay=1e-4)

    history = []
    started = time.time()
    for epoch in range(1, args.epochs + 1):
        model.train()
        total, correct, loss_sum = 0, 0, 0.0
        for images, labels in train_loader:
            images, labels = images.to(device), labels.to(device)
            optimiser.zero_grad()
            logits = model(images)
            loss = criterion(logits, labels)
            loss.backward()
            optimiser.step()
            loss_sum += float(loss) * labels.size(0)
            correct += int((logits.argmax(1) == labels).sum())
            total += labels.size(0)
        y_true, y_pred = evaluate(model, val_loader, device)
        val_acc = float((y_true == y_pred).mean())
        entry = {
            "epoch": epoch,
            "train_loss": round(loss_sum / max(total, 1), 4),
            "train_acc": round(correct / max(total, 1), 4),
            "val_acc": round(val_acc, 4),
            "val_macro_f1": round(macro_f1(y_true, y_pred, len(classes)), 4),
        }
        history.append(entry)
        print(json.dumps(entry), flush=True)

    args.out.mkdir(parents=True, exist_ok=True)
    torch.save(model.state_dict(), args.out / "model.pt")

    model.eval()
    onnx_path = args.out / "model-float32.onnx"
    torch.onnx.export(
        model,
        torch.zeros(1, 3, args.size, args.size, device=device),
        onnx_path,
        input_names=["input"],
        output_names=["logits"],
        opset_version=17,
        dynamo=False,
    )

    int8_path = None
    try:
        from onnxruntime.quantization import QuantType, quantize_dynamic

        int8_path = args.out / "model-int8.onnx"
        quantize_dynamic(str(onnx_path), str(int8_path), weight_type=QuantType.QInt8)
    except Exception as exc:  # onnxruntime is optional
        print(f"int8 export skipped: {exc}", flush=True)

    cm = np.zeros((len(classes), len(classes)), dtype=int)
    for t, p in zip(y_true, y_pred):
        cm[t, p] += 1

    metrics = {
        "classes": list(classes),
        "input": f"{args.size}x{args.size} RGB, ImageNet normalisation",
        "epochs": args.epochs,
        "device": str(device),
        "train_seconds": round(time.time() - started, 1),
        "val_accuracy": round(float((y_true == y_pred).mean()), 4),
        "val_macro_f1": round(macro_f1(y_true, y_pred, len(classes)), 4),
        "confusion_matrix": cm.tolist(),
        "history": history,
        "artifacts": {
            "state_dict": "model.pt",
            "onnx_float32": "model-float32.onnx",
            "onnx_int8": int8_path.name if int8_path else None,
        },
        "caveat": (
            "Scenes are rendered from the same 22 card artworks used for training, so this measures "
            "card-recognition on synthetic scenes only. It is not real-object or field accuracy."
        ),
    }
    (args.out / "metrics.json").write_text(json.dumps(metrics, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({k: metrics[k] for k in ("val_accuracy", "val_macro_f1", "train_seconds")}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
