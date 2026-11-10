"""Kaggle kernel: five-fold card-identity cross-validation, then the shipped model.

Each fold holds out whole cards, so a fold score measures unseen cards. After the
folds the same recipe trains on every card and exports int8 weights, which are
streamed out as base64 because the output API is rate limited for this account.
"""

import base64
import json
import shutil
import subprocess
import sys
from pathlib import Path

BRANCH = "feat/wordless-padded"
REPO = "https://github.com/Bruiseunderskin/smart-trash-can.git"
WORK = Path("/kaggle/working")
SRC = WORK / "src"
PHYSICAL = "data/mixed-card-object/images/train/physical"
FOLDS = 5


def run(*args, cwd=None):
    print("+", " ".join(map(str, args)), flush=True)
    subprocess.run(args, cwd=cwd, check=True)


def read_metrics(path: Path) -> dict:
    return json.loads(path.read_text())


if SRC.exists():
    shutil.rmtree(SRC)
run("git", "clone", "--depth", "1", "--branch", BRANCH, REPO, str(SRC))
subprocess.run([sys.executable, "-m", "pip", "install", "-q", "pillow", "onnxruntime", "ultralytics"], check=False)

results = []

for fold in range(FOLDS):
    data = WORK / f"dataset-fold{fold}"
    if data.exists():
        shutil.rmtree(data)
    run(
        sys.executable, "tools/render_wordless_scenes.py",
        "--cards", "data/mixed-card-object/wordless-v1/cards-padded",
        "--manifest", "data/mixed-card-object/wordless-v1/cards.csv",
        "--out", str(data),
        "--fold", str(fold),
        "--balance-train-per-class", "800",
        "--balance-val-per-class", "200",
        "--bg-dir", PHYSICAL,
        "--extra-train", PHYSICAL,
        "--extra-repeat", "6",
        "--size", "160",
        "--seed", str(20261110 + fold),
        cwd=SRC,
    )
    counts = {p.name: len(list(p.rglob("*.jpg"))) for p in data.iterdir() if p.is_dir()}
    print(f"fold {fold} dataset:", counts, flush=True)

    out_dir = WORK / "runs" / f"fold{fold}"
    run(
        sys.executable, "tools/train_card_classifier.py",
        "--data", str(data),
        "--out", str(out_dir),
        "--epochs", "15",
        "--batch", "64",
        "--patience", "5",
        "--size", "160",
        "--seeds", "7",
        cwd=SRC,
    )
    metrics = read_metrics(out_dir / "metrics.json")
    results.append({
        "fold": fold,
        "val_accuracy": metrics["val_accuracy"],
        "val_macro_f1": metrics["val_macro_f1"],
        "confusion_matrix": metrics["confusion_matrix"],
        "counts": counts,
    })
    print("FOLD_RESULT", json.dumps(results[-1]), flush=True)
    shutil.rmtree(data, ignore_errors=True)

accs = [r["val_accuracy"] for r in results]
mean = sum(accs) / len(accs)
var = sum((a - mean) ** 2 for a in accs) / max(1, len(accs) - 1)
sd = var ** 0.5
summary = {"folds": results, "mean_accuracy": round(mean, 4), "sd_accuracy": round(sd, 4)}
print("CV_SUMMARY", json.dumps(summary), flush=True)

# final model: every card participates, scenes split instead of identities
final_data = WORK / "dataset-final"
if final_data.exists():
    shutil.rmtree(final_data)
run(
    sys.executable, "tools/render_wordless_scenes.py",
    "--cards", "data/mixed-card-object/wordless-v1/cards-padded",
    "--manifest", "data/mixed-card-object/wordless-v1/cards.csv",
    "--out", str(final_data),
    "--scene-split", "0.1",
    "--train-per-card", "40",
    "--bg-dir", PHYSICAL,
    "--extra-train", PHYSICAL,
    "--extra-repeat", "6",
    "--size", "160",
    "--seed", "20261110",
    cwd=SRC,
)

final_out = WORK / "runs" / "final"
run(
    sys.executable, "tools/train_card_classifier.py",
    "--data", str(final_data),
    "--out", str(final_out),
    "--epochs", "15",
    "--batch", "64",
    "--patience", "5",
    "--size", "160",
    "--seeds", "7",
    cwd=SRC,
)
print("FINAL", (final_out / "metrics.json").read_text()[:800], flush=True)

onnx = final_out / "model-int8.onnx"
if onnx.exists():
    blob = base64.b64encode(onnx.read_bytes()).decode()
    print("ONNX_B64_START", flush=True)
    for start in range(0, len(blob), 100_000):
        print(blob[start:start + 100_000], flush=True)
    print("ONNX_B64_END", flush=True)
print(json.dumps({"done": True, "mean": summary["mean_accuracy"], "sd": summary["sd_accuracy"]}))
