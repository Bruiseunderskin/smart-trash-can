#!/usr/bin/env python3
"""Deterministic S31 game replay selection and flash-pack builder."""

from __future__ import annotations

import argparse
import csv
import hashlib
import random
import struct
import zlib
from dataclasses import dataclass
from pathlib import Path

PACK_MAGIC = b"STCR"
PACK_VERSION = 1
ROUNDS = 5
FRAMES_PER_ROUND = 5
EXPECTED_IMAGES = 40
HEADER = struct.Struct("<4sHHI25H")
RECORD = struct.Struct("<BBH8sIII")


@dataclass(frozen=True)
class ReplayItem:
    path: Path | None
    class_index: int
    card_id: str
    variant: int
    data: bytes = b""


@dataclass(frozen=True)
class ReplayRound:
    class_index: int
    card_id: str
    images: tuple[ReplayItem, ...]


@dataclass(frozen=True)
class ParsedPack:
    seed: int
    items: tuple[ReplayItem, ...]
    playlist: tuple[ReplayRound, ...]


def load_items(scene_dir: Path) -> list[ReplayItem]:
    scene_dir = Path(scene_dir)
    labels_path = scene_dir / "labels.csv"
    with labels_path.open(newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))
    items: list[ReplayItem] = []
    seen: set[str] = set()
    for row in rows:
        filename = row["file"]
        if filename in seen:
            raise ValueError(f"duplicate replay filename: {filename}")
        seen.add(filename)
        path = scene_dir / filename
        if not path.is_file():
            raise ValueError(f"missing replay image: {path}")
        class_index = int(row["expected_index"])
        if class_index not in range(4):
            raise ValueError(f"invalid class index for {filename}")
        variant = int(filename.rsplit("_", 1)[1].split(".", 1)[0])
        items.append(ReplayItem(path, class_index, row["card_id"], variant,
                                path.read_bytes()))
    items.sort(key=lambda item: item.path.name)
    if len(items) != EXPECTED_IMAGES:
        raise ValueError(f"expected {EXPECTED_IMAGES} replay images, got {len(items)}")
    return items


def build_playlist(items: list[ReplayItem], seed: int) -> tuple[ReplayRound, ...]:
    if not all(item.path is not None for item in items):
        # Parsed packs carry their deterministic playlist already.
        raise ValueError("playlist construction requires source paths")
    by_card: dict[str, list[ReplayItem]] = {}
    for item in items:
        by_card.setdefault(item.card_id, []).append(item)
    eligible = {
        card_id: group for card_id, group in by_card.items()
        if len(group) >= FRAMES_PER_ROUND
    }
    if not eligible:
        raise ValueError("replay set has no card with five scene variants")

    rng = random.Random(seed)
    candidates_by_class: dict[int, list[str]] = {index: [] for index in range(4)}
    card_class: dict[str, int] = {}
    for card_id in sorted(eligible):
        indices = {item.class_index for item in eligible[card_id]}
        if len(indices) != 1:
            raise ValueError(f"card identity {card_id} spans multiple classes")
        class_index = next(iter(indices))
        candidates_by_class[class_index].append(card_id)
        card_class[card_id] = class_index

    selected: list[str] = []
    for class_index in range(4):
        candidates = candidates_by_class[class_index]
        if not candidates:
            raise ValueError(f"replay set has no class {class_index}")
        selected.append(rng.choice(candidates))
    remaining = sorted(set(eligible) - set(selected))
    if remaining:
        selected.append(rng.choice(remaining))
    else:
        # The current test set has only one eligible hazardous/other card.
        # A fifth round may reuse one eligible identity; game play still has
        # five distinct scene variants for every round.
        selected.append(rng.choice(sorted(eligible)))
    if len(selected) < ROUNDS:
        raise ValueError("replay set needs at least five card identities")
    selected = selected[:ROUNDS]
    rng.shuffle(selected)

    rounds: list[ReplayRound] = []
    for card_id in selected:
        group = sorted(by_card[card_id], key=lambda item: item.path.name)
        rng.shuffle(group)
        frames = tuple(group[:FRAMES_PER_ROUND])
        rounds.append(ReplayRound(card_class[card_id], card_id, frames))
    if {round_.class_index for round_ in rounds} != set(range(4)):
        raise ValueError("five-round playlist must cover all four model classes")
    return tuple(rounds)


def build_pack(items: list[ReplayItem], playlist: tuple[ReplayRound, ...],
               seed: int) -> bytes:
    item_index = {id(item): index for index, item in enumerate(items)}
    playlist_indices: list[int] = []
    for round_ in playlist:
        if len(round_.images) != FRAMES_PER_ROUND:
            raise ValueError("playlist round must contain five frames")
        for frame in round_.images:
            playlist_indices.append(item_index[id(frame)])
    if len(playlist_indices) != ROUNDS * FRAMES_PER_ROUND:
        raise ValueError("pack needs exactly 25 playlist indices")

    header = HEADER.pack(PACK_MAGIC, PACK_VERSION, len(items), seed,
                         *playlist_indices)
    records_offset = len(header)
    blobs_offset = records_offset + RECORD.size * len(items)
    records: list[bytes] = []
    blobs: list[bytes] = []
    cursor = blobs_offset
    for item in items:
        data = item.data
        records.append(RECORD.pack(
            item.class_index,
            len(item.card_id),
            item.variant,
            item.card_id.encode("ascii"),
            cursor,
            len(data),
            zlib.crc32(data) & 0xFFFFFFFF,
        ))
        blobs.append(data)
        cursor += len(data)
    return header + b"".join(records) + b"".join(blobs)


def parse_pack(data: bytes) -> ParsedPack:
    if len(data) < HEADER.size:
        raise ValueError("replay pack is truncated")
    unpacked = HEADER.unpack_from(data, 0)
    magic, version, count, seed = unpacked[:4]
    if magic != PACK_MAGIC:
        raise ValueError("bad replay pack magic")
    if version != PACK_VERSION:
        raise ValueError(f"unsupported replay pack version {version}")
    if count == 0 or count > 255:
        raise ValueError("invalid replay image count")
    playlist_indices = unpacked[4:]
    records_offset = HEADER.size
    blobs_offset = records_offset + RECORD.size * count
    if blobs_offset > len(data):
        raise ValueError("replay record table is truncated")

    items: list[ReplayItem] = []
    for index in range(count):
        class_index, id_length, variant, card_bytes, offset, length, crc = \
            RECORD.unpack_from(data, records_offset + RECORD.size * index)
        if class_index not in range(4) or id_length not in range(1, 9):
            raise ValueError("invalid replay record")
        if offset < blobs_offset or offset + length > len(data):
            raise ValueError("replay blob is outside pack")
        payload = data[offset:offset + length]
        if zlib.crc32(payload) & 0xFFFFFFFF != crc:
            raise ValueError("replay blob CRC mismatch")
        card_id = card_bytes[:id_length].decode("ascii")
        items.append(ReplayItem(None, class_index, card_id, variant, payload))

    rounds: list[ReplayRound] = []
    for round_index in range(ROUNDS):
        indices = playlist_indices[
            round_index * FRAMES_PER_ROUND:(round_index + 1) * FRAMES_PER_ROUND]
        if any(index >= count for index in indices):
            raise ValueError("playlist references a missing image")
        frames = tuple(items[index] for index in indices)
        if len({frame.card_id for frame in frames}) != 1:
            raise ValueError("playlist frames must share one card identity")
        rounds.append(ReplayRound(frames[0].class_index, frames[0].card_id, frames))
    return ParsedPack(seed, tuple(items), tuple(rounds))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scene-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--seed", type=int, default=20261111)
    args = parser.parse_args()

    items = load_items(args.scene_dir)
    playlist = build_playlist(items, args.seed)
    packed = build_pack(items, playlist, args.seed)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(packed)
    print(f"replay pack: {len(packed)} bytes "
          f"sha256={hashlib.sha256(packed).hexdigest()}")
    for index, round_ in enumerate(playlist):
        names = ", ".join(item.path.name for item in round_.images)
        print(f"round {index}: card={round_.card_id} class={round_.class_index} {names}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
