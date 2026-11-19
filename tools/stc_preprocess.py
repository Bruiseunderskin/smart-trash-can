#!/usr/bin/env python3
"""One definition of what the classification models expect as input.

`contracts/model/preprocess.json` fixes the on-device pipeline:

    center_crop_square -> bilinear_resize -> RGB uint8, normalisation in graph

The exported ONNX graphs (`models/wordless-padded/model-*.onnx`) start at `Conv`,
so they take plain 0-1 RGB — there is no ImageNet normalisation node in the graph.
Feeding ImageNet-normalised values to those graphs scored 0.625 where 0-1 scored
1.000 on the same 120 images (PR #72), and the int8 calibration path kept the old
transform, which is why its activation ranges never matched the graph.

Every host tool that feeds a model must go through this module, so the next
mismatch shows up in one place instead of four.

Only numpy and Pillow are needed at this level, so CI can test it without torch.
"""

from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image

IMAGENET_MEAN = (0.485, 0.456, 0.406)
IMAGENET_STD = (0.229, 0.224, 0.225)
NORMALISATIONS = ("div255", "imagenet")


def check_normalisation(normalise: str) -> str:
    """Reject an unknown normalisation instead of silently training on garbage."""
    if normalise not in NORMALISATIONS:
        raise ValueError(f"unknown normalisation {normalise!r}; expected one of {list(NORMALISATIONS)}")
    return normalise


def square_crop(image: Image.Image) -> Image.Image:
    """Centre crop to a square, matching the firmware's `center_crop_square`."""
    width, height = image.size
    if width == height:
        return image
    side = min(width, height)
    left = (width - side) // 2
    top = (height - side) // 2
    return image.crop((left, top, left + side, top + side))


def to_array(image: Image.Image, size: int, normalise: str = "div255") -> np.ndarray:
    """PIL image -> HWC float32 array in the model's input space."""
    check_normalisation(normalise)
    if size < 1:
        raise ValueError(f"size must be positive, got {size}")
    frame = square_crop(image.convert("RGB")).resize((size, size), Image.Resampling.BILINEAR)
    array = np.asarray(frame, dtype=np.float32) / 255.0
    if normalise == "imagenet":
        array = (array - np.array(IMAGENET_MEAN, dtype=np.float32)) / np.array(IMAGENET_STD, dtype=np.float32)
    return array


def to_tensor(image: Image.Image, size: int, normalise: str = "div255"):
    """PIL image -> CHW float32 torch tensor; torch is imported only when needed."""
    import torch

    return torch.from_numpy(to_array(image, size, normalise).transpose(2, 0, 1))


def load_array(path: Path, size: int, normalise: str = "div255") -> np.ndarray:
    with Image.open(path) as image:
        return to_array(image, size, normalise)


def load_batch(path: Path, size: int, normalise: str = "div255") -> np.ndarray:
    """One image as a NCHW float32 batch, ready for onnxruntime."""
    return load_array(path, size, normalise).transpose(2, 0, 1)[None, ...]
