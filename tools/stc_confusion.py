#!/usr/bin/env python3
"""Decision-layer tools for the printed-card classifier: prior correction, abstain, metrics.

Everything here is pure Python (no numpy) so it runs on the S31-side host tools,
in CI, and inside unit tests on any machine.

The three operations, in the order they belong in a pipeline:

1. `adjust_prior` — post-hoc prior correction (logit adjustment). The model was
   trained on a corpus where `recyclable` is roughly 2.3x more frequent than the
   other classes and supplies every licensed real photo, so its posterior is
   pulled toward `recyclable`. Dividing by the estimated prior and renormalising
   removes most of that pull without retraining.
2. `decide` — reject the prediction when the top score or the top-2 margin is too
   small, instead of opening a bin on a coin flip.
3. `confusion`, `per_class`, `ece` — the measurements that decide whether 1 and 2
   actually helped, reported per card and per class rather than as one number.

CLI reads a CSV with a header of exactly:

    card_id,truth,food,hazardous,other,recyclable

`card_id` is optional for analysis but required for the per-card table.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
from pathlib import Path
from typing import Iterable, Sequence

LABELS: tuple[str, ...] = ("food", "hazardous", "other", "recyclable")
INDEX = {label: i for i, label in enumerate(LABELS)}


class ProbabilityError(ValueError):
    """Raised when a probability row cannot be interpreted."""


def softmax(logits: Sequence[float]) -> list[float]:
    """Numerically stable softmax over an arbitrary-length logit vector."""
    if not logits:
        raise ProbabilityError("empty logit vector")
    peak = max(logits)
    exps = [math.exp(x - peak) for x in logits]
    total = sum(exps)
    if total <= 0.0:
        raise ProbabilityError("softmax denominator is not positive")
    return [e / total for e in exps]


def renormalise(weights: Sequence[float]) -> list[float]:
    """Scale non-negative weights so they sum to one."""
    total = sum(weights)
    if total <= 0.0:
        raise ProbabilityError("weights sum to zero; cannot renormalise")
    return [w / total for w in weights]


def estimate_prior(rows: Iterable[Sequence[float]]) -> list[float]:
    """Estimate the model's effective prior as the mean predicted distribution.

    This is the P2P idea (Bhat et al., WACV 2025): the prior the network actually
    learned drifts from the empirical class frequencies, so measure it from the
    model's own outputs on a representative, unlabelled sample rather than from
    label counts.
    """
    rows = list(rows)
    if not rows:
        raise ProbabilityError("no rows to average")
    width = len(rows[0])
    if any(len(r) != width for r in rows):
        raise ProbabilityError("ragged probability rows")
    return [sum(r[i] for r in rows) / len(rows) for i in range(width)]


def label_prior(truths: Iterable[str]) -> list[float]:
    """Empirical class frequencies over the four labels, in LABELS order."""
    counts = [0] * len(LABELS)
    total = 0
    for truth in truths:
        if truth not in INDEX:
            raise ProbabilityError(f"unknown label: {truth!r}")
        counts[INDEX[truth]] += 1
        total += 1
    if total == 0:
        raise ProbabilityError("no labels")
    return [c / total for c in counts]


def adjust_prior(
    probs: Sequence[float],
    prior: Sequence[float],
    tau: float = 1.0,
    floor: float = 1e-6,
) -> list[float]:
    """Divide each probability by `prior**tau`, then renormalise.

    `tau = 0` is a no-op (returns the input distribution unchanged); `tau = 1`
    fully removes the estimated prior. Values between 0 and 1 are the usual
    choice when the estimated prior is noisy.
    """
    if len(probs) != len(prior):
        raise ProbabilityError("probability and prior lengths differ")
    if tau < 0:
        raise ValueError("tau must be non-negative")
    if tau == 0:
        return renormalise(list(probs))
    weights = []
    for p, q in zip(probs, prior):
        if q <= 0.0:
            q = floor
        weights.append(max(p, floor) / (q**tau))
    return renormalise(weights)


def top_two(probs: Sequence[float]) -> tuple[int, float, int, float]:
    """Return (best_index, best_prob, second_index, second_prob)."""
    if len(probs) < 2:
        raise ProbabilityError("need at least two classes for a margin")
    order = sorted(range(len(probs)), key=lambda i: probs[i], reverse=True)
    best, second = order[0], order[1]
    return best, probs[best], second, probs[second]


def decide(
    probs: Sequence[float],
    min_conf: float = 0.55,
    min_margin: float = 0.15,
) -> str | None:
    """Return the predicted label, or None when the row should be rejected.

    Two independent gates: absolute confidence and the gap to the runner-up.
    A row that is confident but nearly tied is exactly the failure mode these
    cards exhibit (bottle vs medicine), so the margin gate matters more than the
    confidence gate.
    """
    best, best_p, _, second_p = top_two(probs)
    if best_p < min_conf:
        return None
    if best_p - second_p < min_margin:
        return None
    return LABELS[best]


def confusion(pairs: Iterable[tuple[str, str | None]]) -> dict[str, dict[str, int]]:
    """Count (truth, predicted) pairs; rejections are counted under `<reject>`."""
    table = {label: {other: 0 for other in LABELS} for label in LABELS}
    rejected = {label: 0 for label in LABELS}
    for truth, pred in pairs:
        if truth not in INDEX:
            raise ProbabilityError(f"unknown label: {truth!r}")
        if pred is None:
            rejected[truth] += 1
            continue
        if pred not in INDEX:
            raise ProbabilityError(f"unknown label: {pred!r}")
        table[truth][pred] += 1
    return {"rows": table, "rejected": rejected}


def per_class(table: dict[str, dict[str, int]]) -> dict[str, dict[str, float]]:
    """Precision, recall and F1 per class from a confusion table."""
    rows = table["rows"]
    out: dict[str, dict[str, float]] = {}
    for label in LABELS:
        tp = rows[label][label]
        predicted = sum(rows[truth][label] for truth in LABELS)
        support = sum(rows[label].values())
        precision = tp / predicted if predicted else 0.0
        recall = tp / support if support else 0.0
        f1 = 2 * precision * recall / (precision + recall) if precision + recall else 0.0
        out[label] = {
            "precision": precision,
            "recall": recall,
            "f1": f1,
            "support": float(support),
        }
    return out


def macro_f1(scores: dict[str, dict[str, float]]) -> float:
    return sum(scores[label]["f1"] for label in LABELS) / len(LABELS)


def accuracy(pairs: Iterable[tuple[str, str | None]]) -> float:
    """Post-decision accuracy: rejected rows count as misses, not as skips."""
    pairs = list(pairs)
    if not pairs:
        raise ProbabilityError("no rows")
    return sum(1 for truth, pred in pairs if pred == truth) / len(pairs)


def ece(
    rows: Iterable[tuple[bool, float]],
    bins: int = 10,
) -> float:
    """Expected calibration error over (correct, confidence) pairs."""
    rows = list(rows)
    if not rows:
        raise ProbabilityError("no rows")
    if bins <= 0:
        raise ValueError("bins must be positive")
    buckets: list[list[tuple[bool, float]]] = [[] for _ in range(bins)]
    for correct, conf in rows:
        index = min(int(conf * bins), bins - 1)
        if index < 0:
            index = 0
        buckets[index].append((correct, conf))
    total = len(rows)
    error = 0.0
    for bucket in buckets:
        if not bucket:
            continue
        mean_conf = sum(c for _, c in bucket) / len(bucket)
        mean_acc = sum(1 for ok, _ in bucket if ok) / len(bucket)
        error += (len(bucket) / total) * abs(mean_acc - mean_conf)
    return error


def read_rows(path: Path) -> list[dict[str, object]]:
    """Parse the evaluation CSV described in the module docstring."""
    rows: list[dict[str, object]] = []
    with path.open(newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        missing = [label for label in LABELS if label not in (reader.fieldnames or [])]
        if missing:
            raise ProbabilityError(f"missing probability columns: {', '.join(missing)}")
        for line in reader:
            probs = []
            for label in LABELS:
                try:
                    probs.append(float(line[label]))
                except (TypeError, ValueError) as error:
                    raise ProbabilityError(f"bad value in column {label}") from error
            rows.append(
                {
                    "card_id": line.get("card_id") or "",
                    "truth": (line.get("truth") or "").strip(),
                    "probs": probs,
                }
            )
    if not rows:
        raise ProbabilityError("no data rows")
    return rows


def summarise(
    rows: Sequence[dict[str, object]],
    tau: float,
    min_conf: float,
    min_margin: float,
) -> dict[str, object]:
    """Run the full decision pipeline over parsed rows and return a report dict."""
    prior = estimate_prior([r["probs"] for r in rows])  # type: ignore[arg-type]
    raw_pairs: list[tuple[str, str | None]] = []
    adj_pairs: list[tuple[str, str | None]] = []
    calibration: list[tuple[bool, float]] = []
    per_card: dict[str, dict[str, int]] = {}

    for row in rows:
        truth = str(row["truth"])
        probs = list(row["probs"])  # type: ignore[arg-type]
        adjusted = adjust_prior(probs, prior, tau)
        best, best_p, _, _ = top_two(adjusted)
        raw_best, _, _, _ = top_two(probs)

        raw_pairs.append((truth, LABELS[raw_best]))
        decision = decide(adjusted, min_conf=min_conf, min_margin=min_margin)
        adj_pairs.append((truth, decision))
        calibration.append((decision == truth, best_p))

        card = str(row.get("card_id") or "")
        if card:
            bucket = per_card.setdefault(card, {"correct": 0, "total": 0})
            bucket["total"] += 1
            if decision == truth:
                bucket["correct"] += 1

    table = confusion(adj_pairs)
    scores = per_class(table)
    return {
        "rows": len(rows),
        "tau": tau,
        "min_conf": min_conf,
        "min_margin": min_margin,
        "estimated_prior": dict(zip(LABELS, prior)),
        "raw_accuracy": accuracy(raw_pairs),
        "post_decision_accuracy": accuracy(adj_pairs),
        "coverage": sum(1 for _, pred in adj_pairs if pred is not None) / len(adj_pairs),
        "rejected": table["rejected"],
        "confusion": table["rows"],
        "per_class": scores,
        "macro_f1": macro_f1(scores),
        "ece": ece(calibration),
        "per_card": {
            card: (counts["correct"] / counts["total"]) if counts["total"] else 0.0
            for card, counts in sorted(per_card.items())
        },
    }


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rows", type=Path, required=True, help="evaluation CSV")
    parser.add_argument("--tau", type=float, default=1.0, help="prior correction strength (0 = off)")
    parser.add_argument("--min-conf", type=float, default=0.55)
    parser.add_argument("--min-margin", type=float, default=0.15)
    parser.add_argument("--json-out", type=Path, help="write the full report here")
    args = parser.parse_args(argv)

    report = summarise(read_rows(args.rows), args.tau, args.min_conf, args.min_margin)
    print(f"rows={report['rows']} tau={report['tau']}")
    print(f"raw accuracy      {report['raw_accuracy']:.4f}")
    print(f"post-decision     {report['post_decision_accuracy']:.4f}")
    print(f"coverage          {report['coverage']:.4f}")
    print(f"macro-F1          {report['macro_f1']:.4f}")
    print(f"ECE               {report['ece']:.4f}")
    print("estimated prior   " + ", ".join(f"{k}={v:.3f}" for k, v in report["estimated_prior"].items()))
    worst = sorted(report["per_card"].items(), key=lambda kv: kv[1])[:5]
    if worst:
        print("weakest cards     " + ", ".join(f"{card}={acc:.2f}" for card, acc in worst))
    if args.json_out:
        args.json_out.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
        print(f"wrote {args.json_out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
