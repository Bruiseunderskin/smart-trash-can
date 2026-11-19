import hashlib
import struct
import unittest
from pathlib import Path

from tools.game_replay import (
    PACK_MAGIC,
    PACK_VERSION,
    build_pack,
    build_playlist,
    load_items,
    parse_pack,
)

ROOT = Path(__file__).resolve().parents[2]
SCENE_DIR = ROOT / "data/mixed-card-object/wordless-v1/test-scenes"


class GameReplayTests(unittest.TestCase):
    def setUp(self):
        self.items = load_items(SCENE_DIR)

    def test_loads_all_forty_scene_images(self):
        self.assertEqual(len(self.items), 40)
        self.assertEqual({item.class_index for item in self.items}, {0, 1, 2, 3})

    def test_fixed_seed_builds_five_stable_five_frame_rounds(self):
        first = build_playlist(self.items, seed=20261111)
        second = build_playlist(self.items, seed=20261111)
        self.assertEqual(first, second)
        self.assertEqual(len(first), 5)
        self.assertEqual({round_.class_index for round_ in first}, {0, 1, 2, 3})
        for round_ in first:
            self.assertEqual(len(round_.images), 5)
            self.assertEqual(len({item.card_id for item in round_.images}), 1)

    def test_pack_is_deterministic_and_parseable(self):
        playlist = build_playlist(self.items, seed=20261111)
        first = build_pack(self.items, playlist, seed=20261111)
        second = build_pack(self.items, playlist, seed=20261111)
        self.assertEqual(hashlib.sha256(first).hexdigest(),
                         hashlib.sha256(second).hexdigest())
        magic, version, count, seed = struct.unpack_from("<4sHHI", first, 0)
        self.assertEqual(magic, PACK_MAGIC)
        self.assertEqual(version, PACK_VERSION)
        self.assertEqual(count, 40)
        self.assertEqual(seed, 20261111)
        parsed = parse_pack(first)
        self.assertEqual(len(parsed.playlist), 5)
        self.assertEqual([len(round_.images) for round_ in parsed.playlist],
                         [5, 5, 5, 5, 5])

    def test_pack_rejects_bad_magic(self):
        playlist = build_playlist(self.items, seed=20261111)
        packed = bytearray(build_pack(self.items, playlist, seed=20261111))
        packed[0] = 0
        with self.assertRaises(ValueError):
            parse_pack(bytes(packed))


if __name__ == "__main__":
    unittest.main()
