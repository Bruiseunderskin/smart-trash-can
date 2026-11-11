#!/usr/bin/env python3
"""Collect the real photos that sit next to the renders, with their provenance.

Only two sources are allowed, both with a stated licence:

  * `data/field-captures` — our own photos, project owned.
  * TrashNet (`garythung/trashnet`, MIT) — public waste photos. Classes map
    glass / paper / plastic / metal / cardboard → recyclable, trash → other.
    Nothing maps to food or hazardous, so those classes get renders only.

The TrashNet sample is capped per class so the real photos stay a small
fraction of the training set and cannot pull the class prior.

    python3 tools/fetch_real_photos.py --out models/wordless-padded/real-photos

Writes `real-photos.csv` with one row per file: class, source, licence, origin.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import io
import random
import zipfile
from pathlib import Path

from PIL import Image

TRASHNET_REPO = "garythung/trashnet"
TRASHNET_FILE = "dataset-resized.zip"
TRASHNET_LICENCE = "MIT (dataset card: https://huggingface.co/datasets/garythung/trashnet)"
TRASHNET_MAP = {
    "glass": "recyclable",
    "paper": "recyclable",
    "plastic": "recyclable",
    "metal": "recyclable",
    "cardboard": "recyclable",
    "trash": "other",
}
CAPTURE_MAP = {"可回收物": "recyclable", "廚餘": "food", "有害垃圾": "hazardous", "其他垃圾": "other"}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, default=Path("models/wordless-padded/real-photos"))
    parser.add_argument("--captures", type=Path, default=Path("data/field-captures"))
    parser.add_argument("--cap-per-class", type=int, default=300)
    parser.add_argument("--seed", type=int, default=20261111)
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()

    if args.out.exists() and not args.force:
        raise SystemExit(f"{args.out} exists; pass --force to rebuild it")

    from huggingface_hub import hf_hub_download

    archive = hf_hub_download(TRASHNET_REPO, TRASHNET_FILE, repo_type="dataset")
    handle = zipfile.ZipFile(archive)
    members = [
        name for name in handle.namelist()
        if name.startswith("dataset-resized/")
        and name.lower().endswith((".jpg", ".jpeg", ".png"))
        and "__MACOSX" not in name
    ]

    by_class: dict[str, list[str]] = {}
    for name in members:
        label = TRASHNET_MAP.get(name.split("/")[1])
        if label:
            by_class.setdefault(label, []).append(name)

    rng = random.Random(args.seed)
    rows: list[dict[str, str]] = []
    for label, names in sorted(by_class.items()):
        chosen = sorted(names)
        rng.shuffle(chosen)
        for index, name in enumerate(chosen[: args.cap_per_class]):
            folder = args.out / label
            folder.mkdir(parents=True, exist_ok=True)
            target = folder / f"trashnet_{label}_{index:03d}.jpg"
            with Image.open(io.BytesIO(handle.read(name))) as image:
                image.convert("RGB").save(target, "JPEG", quality=92)
            rows.append({
                "file": target.relative_to(args.out).as_posix(),
                "class": label,
                "source": f"{TRASHNET_REPO}/{name}",
                "licence": TRASHNET_LICENCE,
                "sha256": sha256(target),
            })

    captures = sorted(
        path for path in args.captures.rglob("*")
        if path.suffix.lower() in {".jpg", ".jpeg", ".png"} and path.parent.name in CAPTURE_MAP
    )
    for path in captures:
        label = CAPTURE_MAP[path.parent.name]
        folder = args.out / label
        folder.mkdir(parents=True, exist_ok=True)
        target = folder / f"capture_{path.name}"
        Image.open(path).convert("RGB").save(target, "JPEG", quality=92)
        rows.append({
            "file": target.relative_to(args.out).as_posix(),
            "class": label,
            "source": f"data/field-captures/{path.parent.name}/{path.name}",
            "licence": "project owned",
            "sha256": sha256(target),
        })

    with (args.out / "real-photos.csv").open("w", newline="") as handle_out:
        writer = csv.DictWriter(handle_out, fieldnames=["file", "class", "source", "licence", "sha256"])
        writer.writeheader()
        writer.writerows(sorted(rows, key=lambda row: row["file"]))

    counts: dict[str, int] = {}
    for row in rows:
        counts[row["class"]] = counts.get(row["class"], 0) + 1
    print(f"real photos: {len(rows)} {counts}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
