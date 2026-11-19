import sys
import tempfile
import unittest
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from tools import stc_preprocess as pre  # noqa: E402


def solid(width, height, colour):
    return Image.new("RGB", (width, height), colour)


class NormalisationTests(unittest.TestCase):
    def test_div255_stays_in_unit_range(self):
        array = pre.to_array(solid(32, 32, (200, 100, 50)), 16)
        self.assertEqual(array.shape, (16, 16, 3))
        self.assertEqual(array.dtype, np.float32)
        self.assertLessEqual(float(array.max()), 1.0)
        self.assertGreaterEqual(float(array.min()), 0.0)
        self.assertTrue(np.allclose(array[0, 0], np.array([200, 100, 50]) / 255.0, atol=1e-6))

    def test_imagenet_is_a_different_input_space(self):
        image = solid(32, 32, (200, 100, 50))
        plain = pre.to_array(image, 16)
        shifted = pre.to_array(image, 16, "imagenet")
        expected = (np.array([200, 100, 50], dtype=np.float32) / 255.0 - np.array(pre.IMAGENET_MEAN)) / np.array(pre.IMAGENET_STD)
        self.assertTrue(np.allclose(shifted[0, 0], expected, atol=1e-5))
        self.assertLess(float(shifted.min()), 0.0)  # blue channel goes negative
        self.assertFalse(np.allclose(plain, shifted))

    def test_unknown_normalisation_is_an_error(self):
        with self.assertRaises(ValueError):
            pre.to_array(solid(8, 8, (0, 0, 0)), 8, "bgr")

    def test_non_positive_size_is_an_error(self):
        with self.assertRaises(ValueError):
            pre.to_array(solid(8, 8, (0, 0, 0)), 0)


class CropTests(unittest.TestCase):
    def test_wide_image_is_cropped_from_the_centre(self):
        image = Image.new("RGB", (40, 20), (0, 0, 0))
        image.paste((255, 255, 255), (10, 0, 30, 20))  # white centre square
        cropped = pre.square_crop(image)
        self.assertEqual(cropped.size, (20, 20))
        self.assertEqual(cropped.getpixel((10, 10)), (255, 255, 255))
        self.assertEqual(cropped.getpixel((0, 10)), (255, 255, 255))

    def test_square_image_is_untouched(self):
        image = solid(12, 12, (1, 2, 3))
        self.assertIs(pre.square_crop(image), image)

    def test_tall_image_is_cropped_vertically(self):
        image = Image.new("RGB", (20, 40), (0, 0, 0))
        image.paste((255, 255, 255), (0, 10, 20, 30))
        cropped = pre.square_crop(image)
        self.assertEqual(cropped.size, (20, 20))
        self.assertEqual(cropped.getpixel((10, 0)), (255, 255, 255))


class BatchTests(unittest.TestCase):
    def test_load_batch_is_nchw(self):
        with tempfile.TemporaryDirectory() as name:
            path = Path(name) / "card.png"
            solid(48, 48, (120, 30, 240)).save(path)
            batch = pre.load_batch(path, 24)
        self.assertEqual(batch.shape, (1, 3, 24, 24))
        self.assertEqual(batch.dtype, np.float32)


if __name__ == "__main__":
    unittest.main()
