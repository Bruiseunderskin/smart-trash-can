#!/usr/bin/env python3
"""Capture live SC101IOT JPEG frames streamed over the ESP32-S31 serial port."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
import time
import zlib
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Any


BEGIN_PATTERN = re.compile(
    rb"^CAMERA_BEGIN index=(\d+) width=(\d+) height=(\d+) bytes=(\d+) crc32=([0-9a-fA-F]{8})$"
)
MAX_HEADER_BYTES = 16 * 1024
MAX_FRAME_BYTES = 4 * 1024 * 1024
READY_MARKER = b"CAMERA_READY"


class StreamProtocolError(RuntimeError):
    """Raised when the camera serial stream violates the framing contract."""


@dataclass(frozen=True)
class CameraFrame:
    index: int
    width: int
    height: int
    crc32: int
    payload: bytes


class CameraStreamDecoder:
    """Incrementally decode ``CAMERA_BEGIN`` length-prefixed binary frames."""

    def __init__(self) -> None:
        self._buffer = bytearray()
        self._pending: tuple[int, int, int, int, int] | None = None
        self._payload = bytearray()
        self._ready_scan_tail = b""
        self.ready = False

    def feed(self, data: bytes) -> list[CameraFrame]:
        self._buffer.extend(data)
        ready_scan = self._ready_scan_tail + data
        self._ready_scan_tail = ready_scan[-(len(READY_MARKER) - 1) :]
        if READY_MARKER in ready_scan:
            self.ready = True

        frames: list[CameraFrame] = []
        while True:
            if self._pending is None:
                marker = self._buffer.find(b"CAMERA_BEGIN ")
                if marker < 0:
                    self._trim_waiting_buffer()
                    break
                if marker:
                    del self._buffer[:marker]

                line_end = self._line_end()
                if line_end < 0:
                    if len(self._buffer) > MAX_HEADER_BYTES:
                        raise StreamProtocolError("camera header exceeds size limit")
                    break

                raw_line = bytes(self._buffer[:line_end]).rstrip(b"\r")
                del self._buffer[: line_end + self._newline_width(line_end)]
                match = BEGIN_PATTERN.match(raw_line)
                if not match:
                    raise StreamProtocolError(
                        f"invalid camera header: {raw_line.decode(errors='replace')}"
                    )
                index, width, height, size = (
                    int(value) for value in match.groups()[:4]
                )
                crc32 = int(match.group(5), 16)
                if not 0 < size <= MAX_FRAME_BYTES:
                    raise StreamProtocolError(f"invalid camera payload size: {size}")
                self._pending = (index, width, height, size, crc32)
                self._payload.clear()
                continue

            _, _, _, size, _ = self._pending
            needed = size - len(self._payload)
            if needed and self._buffer:
                take = min(needed, len(self._buffer))
                self._payload.extend(self._buffer[:take])
                del self._buffer[:take]
            if len(self._payload) < size:
                break

            frame = self._complete_pending_frame()
            frames.append(frame)
        return frames

    def finish(self) -> None:
        if self._pending is not None:
            expected = self._pending[3]
            raise StreamProtocolError(
                f"truncated camera payload: received {len(self._payload)} of {expected} bytes"
            )

    def _complete_pending_frame(self) -> CameraFrame:
        assert self._pending is not None
        index, width, height, size, expected_crc32 = self._pending
        actual_crc32 = zlib.crc32(self._payload) & 0xFFFFFFFF
        if actual_crc32 != expected_crc32:
            raise StreamProtocolError(
                f"CRC32 mismatch: expected {expected_crc32:08x}, got {actual_crc32:08x}"
            )

        while self._buffer.startswith(b"\r\n"):
            del self._buffer[:2]
        while self._buffer.startswith(b"\n"):
            del self._buffer[:1]
        if self._buffer.startswith(b"CAMERA_END"):
            line_end = self._line_end()
            if line_end < 0:
                line_end = len(self._buffer)
                newline_width = 0
            else:
                newline_width = self._newline_width(line_end)
            del self._buffer[: line_end + newline_width]

        payload = bytes(self._payload)
        self._pending = None
        self._payload.clear()
        self.ready = READY_MARKER in self._buffer
        return CameraFrame(
            index=index,
            width=width,
            height=height,
            crc32=expected_crc32,
            payload=payload,
        )

    def _line_end(self) -> int:
        crlf = self._buffer.find(b"\r\n")
        lf = self._buffer.find(b"\n")
        candidates = [position for position in (crlf, lf) if position >= 0]
        return min(candidates) if candidates else -1

    def _newline_width(self, line_end: int) -> int:
        return 2 if self._buffer[line_end : line_end + 2] == b"\r\n" else 1

    def _trim_waiting_buffer(self) -> None:
        if len(self._buffer) <= MAX_HEADER_BYTES:
            return
        keep = b"CAMERA_BEGIN ".__len__() - 1
        del self._buffer[: len(self._buffer) - keep]


def jpeg_dimensions(payload: bytes) -> tuple[int, int]:
    if not payload.startswith(b"\xff\xd8"):
        raise StreamProtocolError("JPEG payload is missing the SOI marker")

    sof_markers = {
        0xC0,
        0xC1,
        0xC2,
        0xC3,
        0xC5,
        0xC6,
        0xC7,
        0xC9,
        0xCA,
        0xCB,
        0xCD,
        0xCE,
        0xCF,
    }
    offset = 2
    while offset + 4 <= len(payload):
        if payload[offset] != 0xFF:
            offset += 1
            continue
        while offset < len(payload) and payload[offset] == 0xFF:
            offset += 1
        if offset >= len(payload):
            break
        marker = payload[offset]
        offset += 1

        if marker == 0xD9:
            break
        if marker in {0x01, *range(0xD0, 0xD8)}:
            continue
        if offset + 2 > len(payload):
            break
        segment_length = int.from_bytes(payload[offset : offset + 2], "big")
        if segment_length < 2 or offset + segment_length > len(payload):
            raise StreamProtocolError("invalid JPEG segment length")
        if marker in sof_markers:
            if segment_length < 7:
                raise StreamProtocolError("JPEG SOF segment is too short")
            height = int.from_bytes(payload[offset + 3 : offset + 5], "big")
            width = int.from_bytes(payload[offset + 5 : offset + 7], "big")
            if width <= 0 or height <= 0:
                raise StreamProtocolError("JPEG has invalid dimensions")
            return width, height
        offset += segment_length
    raise StreamProtocolError("JPEG payload has no supported SOF marker")


def validate_jpeg(frame: CameraFrame) -> None:
    if not frame.payload.rstrip(b"\x00\r\n").endswith(b"\xff\xd9"):
        raise StreamProtocolError(f"frame {frame.index} is missing the JPEG EOI marker")
    dimensions = jpeg_dimensions(frame.payload)
    if dimensions != (frame.width, frame.height):
        raise StreamProtocolError(
            f"JPEG dimensions {dimensions[0]}x{dimensions[1]} do not match "
            f"header {frame.width}x{frame.height}"
        )


def _safe_label(label: str) -> str:
    cleaned = re.sub(r"[^0-9A-Za-z\u0080-\uffff_-]+", "_", label).strip("_")
    return cleaned or "unlabeled"


def trigger_bytes(command: str) -> bytes:
    encoded = command.encode("ascii")
    if not encoded.endswith((b"\n", b"\r")):
        encoded += b"\n"
    return encoded


def _write_capture(
    frame: CameraFrame,
    output_dir: Path,
    label: str,
    port: str,
    baudrate: int,
    capture_number: int,
) -> dict[str, Any]:
    captured_at = datetime.now().astimezone()
    prefix = captured_at.strftime("%Y%m%dT%H%M%S_%f")
    filename = f"{prefix}_{capture_number:03d}_{_safe_label(label)}.jpg"
    image_path = output_dir / filename
    image_path.write_bytes(frame.payload)

    return {
        "file": filename,
        "label": label,
        "captured_at": captured_at.isoformat(),
        "port": port,
        "baudrate": baudrate,
        "frame_index": frame.index,
        "width": frame.width,
        "height": frame.height,
        "bytes": len(frame.payload),
        "crc32": f"{frame.crc32:08x}",
        "sha256": hashlib.sha256(frame.payload).hexdigest(),
        "source": "SC101IOT live camera via ESP32-S31 UART",
        "excluded_from_training": True,
        "excluded_from_calibration": True,
        "excluded_from_model_selection": True,
    }


def _write_manifest(path: Path, captures: list[dict[str, Any]]) -> None:
    path.write_text(
        json.dumps(
            {
                "schema_version": 1,
                "camera": "SC101IOT",
                "board": "ESP32-S31-Korvo-1 V1.1",
                "transport": "UART JPEG stream",
                "captures": captures,
            },
            ensure_ascii=False,
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM5")
    parser.add_argument("--baudrate", type=int, default=460800)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--manifest", type=Path)
    parser.add_argument("--label", default="unlabeled")
    parser.add_argument("--count", type=int, default=1)
    parser.add_argument("--timeout", type=float, default=30.0)
    parser.add_argument("--trigger", default="c")
    parser.add_argument("--reset", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.count < 1:
        raise ValueError("--count must be positive")
    trigger = trigger_bytes(args.trigger)
    if not args.trigger:
        raise ValueError("--trigger must not be empty")

    try:
        import serial
    except ImportError as exc:
        raise RuntimeError("pyserial is required for live camera capture") from exc

    args.output.mkdir(parents=True, exist_ok=True)
    manifest_path = args.manifest or args.output / "captures.json"
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    if manifest_path.exists():
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        captures = list(manifest.get("captures", []))
    else:
        captures = []

    decoder = CameraStreamDecoder()
    with serial.Serial(args.port, args.baudrate, timeout=0.2) as port:
        port.dtr = False
        port.rts = False
        if args.reset:
            port.rts = True
            time.sleep(0.15)
            port.rts = False

        def wait_for_ready() -> None:
            deadline = time.monotonic() + args.timeout
            while time.monotonic() < deadline:
                data = port.read(4096)
                if data:
                    decoder.feed(data)
                    if decoder.ready:
                        return
            raise TimeoutError("camera did not report CAMERA_READY before timeout")

        wait_for_ready()
        for capture_number in range(1, args.count + 1):
            port.write(trigger)
            port.flush()
            decoder.ready = False
            received: CameraFrame | None = None
            deadline = time.monotonic() + args.timeout
            while time.monotonic() < deadline:
                data = port.read(4096)
                if not data:
                    continue
                frames = decoder.feed(data)
                if frames:
                    received = frames[-1]
                    break
            decoder.finish()
            if received is None:
                raise TimeoutError(f"camera frame {capture_number} did not arrive before timeout")

            validate_jpeg(received)
            metadata = _write_capture(
                received,
                args.output,
                args.label,
                args.port,
                args.baudrate,
                capture_number,
            )
            captures.append(metadata)
            _write_manifest(manifest_path, captures)
            print(
                f"CAPTURED {metadata['file']} {metadata['width']}x{metadata['height']} "
                f"{metadata['bytes']} bytes sha256={metadata['sha256']}",
                flush=True,
            )

    print(f"MANIFEST {manifest_path.resolve()}", flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, ValueError, TimeoutError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
