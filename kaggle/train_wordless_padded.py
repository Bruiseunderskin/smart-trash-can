"""Kaggle kernel: render the padded wordless card scenes and train the classifier.

Runs on a GPU Kaggle kernel; the repository is cloned at the branch under test so
the cards, renderer and trainer are exactly the committed ones.
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


def run(*args, cwd=None):
    print("+", " ".join(map(str, args)), flush=True)
    subprocess.run(args, cwd=cwd, check=True)


if SRC.exists():
    shutil.rmtree(SRC)
run("git", "clone", "--depth", "1", "--branch", BRANCH, REPO, str(SRC))

subprocess.run([sys.executable, "-m", "pip", "install", "-q", "pillow", "onnxruntime"], check=False)

if DATA.exists():
    shutil.rmtree(DATA)
run(
    sys.executable, "tools/render_wordless_scenes.py",
    "--cards", "data/mixed-card-object/wordless-v1/cards-padded",
    "--manifest", "data/mixed-card-object/wordless-v1/cards.csv",
    "--out", str(DATA),
    "--train-per-card", "200",
    "--val-per-card", "15",
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
    "--epochs", "14",
    "--batch", "64",
    "--workers", "2",
    cwd=SRC,
)

print((out_dir / "metrics.json").read_text()[:2000], flush=True)
print(json.dumps({"done": True, "counts": counts}))
