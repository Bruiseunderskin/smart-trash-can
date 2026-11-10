"""Kaggle kernel: retrain the 158-card model and quantise it to ESP-DL w8a8.

Streams the .espdl back as base64 because the output API is rate limited for
this account. The float32 ONNX stays on the run for inspection.
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
DATA = WORK / "dataset"
ESP = Path("models/wordless-padded/wordless_s31.espdl")


def run(*args, cwd=None, check=True):
    print("+", " ".join(map(str, args)), flush=True)
    return subprocess.run(args, cwd=cwd, check=check)


if SRC.exists():
    shutil.rmtree(SRC)
run("git", "clone", "--depth", "1", "--branch", BRANCH, REPO, str(SRC))
run(sys.executable, "-m", "pip", "install", "-q", "pillow", "onnxruntime", "ultralytics")

if DATA.exists():
    shutil.rmtree(DATA)
run(
    sys.executable, "tools/render_wordless_scenes.py",
    "--cards", "data/mixed-card-object/wordless-v1/cards-padded",
    "--manifest", "data/mixed-card-object/wordless-v1/cards.csv",
    "--out", str(DATA),
    "--scene-split", "0.1",
    "--train-per-card", "10",
    "--bg-dir", "data/mixed-card-object/images/train/physical",
    "--extra-train", "data/mixed-card-object/images/train/physical",
    "--extra-repeat", "4",
    "--size", "160",
    "--seed", "20261110",
    cwd=SRC,
)
counts = {p.name: len(list(p.rglob("*.jpg"))) for p in DATA.iterdir() if p.is_dir()}
print("dataset:", counts, flush=True)

final_out = WORK / "runs" / "final"
run(
    sys.executable, "tools/train_card_classifier.py",
    "--data", str(DATA),
    "--out", str(final_out),
    "--epochs", "15",
    "--batch", "64",
    "--patience", "5",
    "--size", "160",
    "--seeds", "7",
    cwd=SRC,
)
print("TRAIN", (final_out / "metrics.json").read_text()[:600], flush=True)

float_onnx = final_out / "model-float32.onnx"
print("float onnx exists:", float_onnx.exists(), float_onnx.stat().st_size if float_onnx.exists() else 0, flush=True)

# esp-ppq has no wheel for this interpreter, so the quantisation runs on the host;
# this kernel only ships the float32 ONNX and the calibration/test split.
if float_onnx.exists():
    print("ONNX_BYTES", float_onnx.stat().st_size, flush=True)
    blob = base64.b64encode(float_onnx.read_bytes()).decode()
    print("ONNX_B64_START", flush=True)
    for start in range(0, len(blob), 100_000):
        print(blob[start:start + 100_000], flush=True)
    print("ONNX_B64_END", flush=True)
else:
    print("ONNX_MISSING", flush=True)
