"""Kaggle kernel: render padded wordless card scenes and fine-tune YOLO11n-cls.

Train scene backgrounds come from real object photos, and those photos are also
added to the training split, so the model sees object appearance rather than only
card artwork. Validation stays split by card identity, keeping the unseen-card
number comparable with earlier runs.
"""

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
PHYSICAL = "data/mixed-card-object/images/train/physical"


def run(*args, cwd=None):
    print("+", " ".join(map(str, args)), flush=True)
    subprocess.run(args, cwd=cwd, check=True)


if SRC.exists():
    shutil.rmtree(SRC)
run("git", "clone", "--depth", "1", "--branch", BRANCH, REPO, str(SRC))

subprocess.run([sys.executable, "-m", "pip", "install", "-q", "pillow", "onnxruntime", "ultralytics"], check=False)

if DATA.exists():
    shutil.rmtree(DATA)
run(
    sys.executable, "tools/render_wordless_scenes.py",
    "--cards", "data/mixed-card-object/wordless-v1/cards-padded",
    "--manifest", "data/mixed-card-object/wordless-v1/cards.csv",
    "--out", str(DATA),
    "--balance-train-per-class", "1200",
    "--balance-val-per-class", "24",
    "--bg-dir", PHYSICAL,
    "--extra-train", PHYSICAL,
    "--extra-repeat", "8",
    "--size", "160",
    "--seed", "20261109",
    cwd=SRC,
)

counts = {p.name: len(list(p.rglob("*.jpg"))) for p in DATA.iterdir() if p.is_dir()}
print("dataset:", counts, flush=True)

out_dir = WORK / "runs" / "wordless-padded"
run(
    sys.executable, "tools/train_card_classifier.py",
    "--data", str(DATA),
    "--out", str(out_dir),
    "--epochs", "20",
    "--batch", "64",
    "--patience", "7",
    "--size", "160",
    "--seeds", "7,42",
    cwd=SRC,
)

print((out_dir / "metrics.json").read_text()[:2500], flush=True)

# the Kaggle output API is rate limited for this account, so the quantised model
# also travels out through the log stream where it can be reassembled
import base64

onnx = out_dir / "model-int8.onnx"
if onnx.exists():
    blob = base64.b64encode(onnx.read_bytes()).decode()
    print("ONNX_B64_START", flush=True)
    for start in range(0, len(blob), 100_000):
        print(blob[start:start + 100_000], flush=True)
    print("ONNX_B64_END", flush=True)
print(json.dumps({"done": True, "counts": counts}))
