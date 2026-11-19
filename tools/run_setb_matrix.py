#!/usr/bin/env python3
"""Run the whole Set B evaluation in one command: matrix -> inference -> sweep -> summary.

Designed for a Kaggle notebook (see notebooks/kaggle-setb-matrix.ipynb) but works
anywhere numpy + Pillow are installed, and additionally onnxruntime when it is
time to run the models.

Three stages, each skippable if its artefacts already exist:

1. `render`  — calls tools/render_card_matrix.py to build the 158-card x 6-condition
               matrix with a reproducible manifest (reuses the file if present).
2. `infer`   — runs each ONNX model over the manifest and writes one row per image
               with the four class probabilities in LABELS order.
3. `sweep`   — for tau in {0, 0.5, 1.0} x (min_conf, min_margin) grid, reports
               post-decision accuracy, coverage, macro-F1 and ECE, then writes a
               markdown summary plus a machine-readable JSON.

The acceptance lines from issue #67 are printed as PASS/FAIL so a run is readable
without opening the JSON.

Usage (Kaggle):

    python3 tools/run_setb_matrix.py \\
        --cards models/wordless-padded/cards \\
        --cards-csv models/wordless-padded/cards.csv \\
        --backgrounds models/wordless-padded/real-photos \\
        --models models/wordless-padded/model-float32.onnx,models/wordless-padded/model-int8.onnx \\
        --work /kaggle/working/setb --per-condition 20

Nothing here writes images into the output: the matrix stays in `--work` and the
report keeps only CSVs and JSON.
"""

from __future__ import annotations

import argparse
import csv
import json
import subprocess
import sys
from pathlib import Path
from typing import Iterable, Sequence

LABELS: tuple[str, ...] = ("food", "hazardous", "other", "recyclable")
ROOT = Path(__file__).resolve().parent.parent
PROBLEM_CARDS = ("B-14", "B-15", "B-16", "B-17", "B-18", "B-22", "B-28")
IMAGENET_MEAN = (0.485, 0.456, 0.406)
IMAGENET_STD = (0.229, 0.224, 0.225)


# --------------------------------------------------------------------------- #
# pure helpers (no numpy, no onnxruntime): unit-tested in CI
# --------------------------------------------------------------------------- #

def parse_csv_list(text: str) -> list[str]:
    """Split a comma-separated CLI value, dropping blanks and duplicates."""
    seen: list[str] = []
    for item in text.split(","):
        value = item.strip()
        if value and value not in seen:
            seen.append(value)
    return seen


def parse_float_list(text: str) -> list[float]:
    values = []
    for item in parse_csv_list(text):
        try:
            values.append(float(item))
        except ValueError as error:
            raise SystemExit(f"not a number: {item!r}") from error
    return values


def read_labels(path: Path) -> list[str]:
    """Read labels.txt and check it is a permutation of the frozen four classes."""
    labels = [line.strip() for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]
    if sorted(labels) != sorted(LABELS):
        raise SystemExit(f"{path} must contain exactly {sorted(LABELS)}, got {labels}")
    return labels


def sweep_grid(taus: Sequence[float], confs: Sequence[float], margins: Sequence[float]) -> list[tuple[float, float, float]]:
    """Cartesian product of the three knobs, in a stable order."""
    return [(tau, conf, margin) for tau in taus for conf in confs for margin in margins]


def passes_acceptance(report: dict, min_coverage: float = 0.85) -> dict[str, bool]:
    """Compare one sweep result against the acceptance lines from issue #67."""
    return {
        "coverage>=0.85": report["coverage"] >= min_coverage,
        "accuracy>=0.95": report["post_decision_accuracy"] >= 0.95,
        "ece<=0.05": report["ece"] <= 0.05,
    }


def problem_card_floor(per_card: dict[str, float]) -> float:
    """Lowest accuracy among the cards the teammate flagged, 1.0 if none present."""
    seen = [per_card[card] for card in PROBLEM_CARDS if card in per_card]
    return min(seen) if seen else 1.0


def choose(reports: Iterable[dict], min_coverage: float = 0.85) -> dict:
    """Pick the report with the best post-decision accuracy at acceptable coverage."""
    eligible = [r for r in reports if r["coverage"] >= min_coverage]
    pool = eligible or list(reports)
    if not pool:
        raise SystemExit("no sweep results")
    return max(pool, key=lambda r: (r["post_decision_accuracy"], r["macro_f1"], r["coverage"]))


def summary_markdown(model_reports: dict[str, list[dict]], best: dict[str, dict]) -> str:
    """Render a compact markdown table for one model -> many sweep reports."""
    lines = ["| model | tau | min_conf | min_margin | accuracy | coverage | macro-F1 | ECE | worst flagged card |",
             "|---|---:|---:|---:|---:|---:|---:|---:|---:|"]
    for model, reports in model_reports.items():
        for report in reports:
            lines.append(
                "| {model} | {tau:.1f} | {conf:.2f} | {margin:.2f} | {acc:.4f} | {cov:.3f} | {f1:.4f} | {ece:.4f} | {worst:.2f} |".format(
                    model=model,
                    tau=report["tau"],
                    conf=report["min_conf"],
                    margin=report["min_margin"],
                    acc=report["post_decision_accuracy"],
                    cov=report["coverage"],
                    f1=report["macro_f1"],
                    ece=report["ece"],
                    worst=problem_card_floor(report["per_card"]),
                )
            )
    lines.append("")
    lines.append("Best operating point per model (coverage >= 0.85 preferred):")
    for model, report in best.items():
        checks = passes_acceptance(report)
        verdict = "PASS" if all(checks.values()) else "FAIL"
        failed = ", ".join(name for name, ok in checks.items() if not ok)
        suffix = "" if verdict == "PASS" else f" (fails: {failed})"
        lines.append(
            f"- **{model}**: tau={report['tau']}, conf={report['min_conf']}, margin={report['min_margin']} "
            f"-> accuracy {report['post_decision_accuracy']:.4f}, coverage {report['coverage']:.3f}, "
            f"ECE {report['ece']:.4f} — {verdict}{suffix}"
        )
    return "\n".join(lines) + "\n"


# --------------------------------------------------------------------------- #
# stage 1: matrix
# --------------------------------------------------------------------------- #

def ensure_matrix(args: argparse.Namespace) -> Path:
    """Return the manifest path, rendering the matrix first if it is missing."""
    manifest = args.work / "matrix" / "manifest.csv"
    if manifest.exists() and not args.force_render:
        print(f"[matrix] reusing {manifest}")
        return manifest
    command = [
        sys.executable,
        str(ROOT / "tools" / "render_card_matrix.py"),
        "--cards", str(args.cards),
        "--cards-csv", str(args.cards_csv),
        "--out", str(args.work / "matrix"),
        "--per-condition", str(args.per_condition),
        "--size", str(args.size),
    ]
    if args.cards_padded:
        command += ["--cards-padded", str(args.cards_padded)]
    if args.backgrounds:
        command += ["--backgrounds", str(args.backgrounds)]
    if args.conditions:
        command += ["--conditions", args.conditions]
    print("[matrix] rendering:", " ".join(command))
    subprocess.run(command, check=True, cwd=ROOT)
    if not manifest.exists():
        raise SystemExit("matrix manifest was not produced")
    return manifest


# --------------------------------------------------------------------------- #
# stage 2: inference
# --------------------------------------------------------------------------- #

def load_manifest(manifest: Path) -> list[dict[str, str]]:
    with manifest.open(newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))
    if not rows:
        raise SystemExit(f"{manifest} has no rows")
    return rows


def preprocess(path: Path, size: int):
    """RGB, resize to the model input, ImageNet normalise, CHW float32."""
    import numpy as np  # local import: keeps this module importable without numpy
    from PIL import Image

    with Image.open(path) as image:
        frame = image.convert("RGB").resize((size, size), Image.Resampling.BILINEAR)
        array = np.asarray(frame, dtype=np.float32) / 255.0
    array = (array - np.array(IMAGENET_MEAN, dtype=np.float32)) / np.array(IMAGENET_STD, dtype=np.float32)
    return np.transpose(array, (2, 0, 1))[None, ...]


def infer(model: Path, manifest_rows: Sequence[dict[str, str]], matrix_root: Path, size: int, labels: Sequence[str]) -> list[list[float]]:
    import onnxruntime as ort  # local import: only needed when models actually run

    session = ort.InferenceSession(str(model), providers=["CPUExecutionProvider"])
    name = session.get_inputs()[0].name
    out: list[list[float]] = []
    total = len(manifest_rows)
    for index, row in enumerate(manifest_rows):
        tensor = preprocess(matrix_root / row["path"], size)
        logits = session.run(None, {name: tensor})[0][0]
        # Models export a softmax head; normalise defensively in case they do not.
        values = [float(v) for v in logits]
        total_value = sum(values)
        if total_value <= 0 or abs(total_value - 1.0) > 1e-3:
            values = [v / total_value if total_value else 1.0 / len(values) for v in values]
        out.append(values)
        if (index + 1) % 1000 == 0:
            print(f"[infer] {model.name}: {index + 1}/{total}")
    return out


def write_rows(path: Path, manifest_rows: Sequence[dict[str, str]], labels: Sequence[str], probs: Sequence[Sequence[float]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(["card_id", "truth", *LABELS])
        for row, values in zip(manifest_rows, probs):
            by_label = dict(zip(labels, values))
            writer.writerow([row["card_id"], row["class"], *[f"{by_label[label]:.6f}" for label in LABELS]])


# --------------------------------------------------------------------------- #
# stage 3: sweep
# --------------------------------------------------------------------------- #

def run_sweep(rows_path: Path, taus: Sequence[float], confs: Sequence[float], margins: Sequence[float]):
    sys.path.insert(0, str(ROOT))
    from tools import stc_confusion as sc

    parsed = sc.read_rows(rows_path)
    reports = []
    for tau, conf, margin in sweep_grid(taus, confs, margins):
        reports.append(sc.summarise(parsed, tau, conf, margin))
    return reports


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cards", type=Path, default=Path("models/wordless-padded/cards"))
    parser.add_argument(
        "--cards-padded",
        type=Path,
        help="padded sprite directory; defaults to <cards>/../cards-padded when present",
    )
    parser.add_argument("--cards-csv", type=Path, default=Path("models/wordless-padded/cards.csv"))
    parser.add_argument("--backgrounds", type=Path, default=Path("models/wordless-padded/real-photos"))
    parser.add_argument("--labels", type=Path, default=Path("models/wordless-padded/labels.txt"))
    parser.add_argument("--models", default="models/wordless-padded/model-float32.onnx,models/wordless-padded/model-int8.onnx")
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--per-condition", type=int, default=20)
    parser.add_argument("--size", type=int, default=160)
    parser.add_argument("--conditions", default="", help="subset passed through to render_card_matrix")
    parser.add_argument("--taus", default="0,0.5,1.0")
    parser.add_argument("--min-confs", default="0.45,0.55,0.65,0.75")
    parser.add_argument("--min-margins", default="0.05,0.15,0.25")
    parser.add_argument("--min-coverage", type=float, default=0.85)
    parser.add_argument("--force-render", action="store_true")
    parser.add_argument("--skip-infer", action="store_true", help="reuse rows-<model>.csv if present")
    args = parser.parse_args()

    labels = read_labels(args.labels)
    taus = parse_float_list(args.taus)
    confs = parse_float_list(args.min_confs)
    margins = parse_float_list(args.min_margins)
    models = [Path(m) for m in parse_csv_list(args.models)]

    args.work.mkdir(parents=True, exist_ok=True)
    manifest = ensure_matrix(args)
    manifest_rows = load_manifest(manifest)
    print(f"[matrix] {len(manifest_rows)} rows from {manifest}")

    all_reports: dict[str, list[dict]] = {}
    best: dict[str, dict] = {}

    for model in models:
        rows_path = args.work / f"rows-{model.stem}.csv"
        if args.skip_infer and rows_path.exists():
            print(f"[infer] reusing {rows_path}")
        else:
            probs = infer(model, manifest_rows, args.work / "matrix", args.size, labels)
            write_rows(rows_path, manifest_rows, labels, probs)
            print(f"[infer] wrote {rows_path}")
        reports = run_sweep(rows_path, taus, confs, margins)
        all_reports[model.stem] = reports
        best[model.stem] = choose(reports, args.min_coverage)
        (args.work / f"sweep-{model.stem}.json").write_text(json.dumps(reports, indent=2, ensure_ascii=False), encoding="utf-8")

    markdown = summary_markdown(all_reports, best)
    summary_path = args.work / "summary.md"
    summary_path.write_text(markdown, encoding="utf-8")
    print(markdown)
    print(f"[done] summary -> {summary_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
