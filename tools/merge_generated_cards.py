#!/usr/bin/env python3
"""Merge generated card faces into cards.csv and assign balanced folds.

Folds are used for card-identity cross-validation: every fold holds out whole
cards, so a fold score answers "unseen card", not "same card, new angle".

    python3 tools/merge_generated_cards.py \
        --csv data/mixed-card-object/wordless-v1/cards.csv \
        --new /config/work/newfaces.csv \
        --cards-dir data/mixed-card-object/wordless-v1/cards \
        --new-cards-dir /config/work/newfaces \
        --folds 5
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import shutil
from collections import defaultdict
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--csv", type=Path, required=True)
    parser.add_argument("--new", type=Path, required=True, help="CSV produced by build_cards_from_photos.py")
    parser.add_argument("--cards-dir", type=Path, required=True)
    parser.add_argument("--new-cards-dir", type=Path, required=True)
    parser.add_argument("--folds", type=int, default=5)
    args = parser.parse_args()

    with args.csv.open() as handle:
        rows = list(csv.DictReader(handle))
    with args.new.open() as handle:
        fresh = list(csv.DictReader(handle))

    existing_ids = {r["card_id"] for r in rows}
    copied = 0
    for row in fresh:
        src = args.new_cards_dir / f"{row['card_id']}.png"
        if not src.exists():
            print(f"missing {src}")
            continue
        dest = args.cards_dir / src.name
        if not dest.exists():
            shutil.copy2(src, dest)
            copied += 1
        rows.append({
            "card_id": row["card_id"],
            "class": row["class"],
            "split": "generated",
            "sha256": hashlib.sha256(dest.read_bytes()).hexdigest(),
            "retouch": "cutout",
            "source": row.get("source", ""),
        })

    # balanced folds inside each class, ordered by id so the split is reproducible
    by_class: dict[str, list[dict]] = defaultdict(list)
    for row in rows:
        by_class[row["class"]].append(row)
    for label, items in by_class.items():
        items.sort(key=lambda r: r["card_id"])
        for i, row in enumerate(items):
            row["fold"] = str(i % args.folds)

    fieldnames = ["card_id", "class", "split", "sha256", "retouch", "source", "fold"]
    with args.csv.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        for row in sorted(rows, key=lambda r: r["card_id"]):
            writer.writerow({k: row.get(k, "") for k in fieldnames})

    print(f"copied {copied} new cards, csv now has {len(rows)} rows")
    for label in sorted(by_class):
        print(label, len(by_class[label]),
              {f: sum(1 for r in by_class[label] if r["fold"] == str(f)) for f in range(args.folds)})
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
