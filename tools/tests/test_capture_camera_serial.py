from __future__ import annotations

import sys
import unittest
import zlib
from io import BytesIO
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY_ROOT))


def encoded_frame(payload: bytes, *, index: int = 0) -> bytes:
    crc32 = zlib.crc32(payload) & 0xFFFFFFFF
    return (
        f"boot log before frame\r\n"
        f"CAMERA_BEGIN index={index} width=1280 height=720 bytes={len(payload)} "
        f"crc32={crc32:08x}\r\n"
    ).encode() + payload + b"\r\nCAMERA_END\r\nCAMERA_READY\r\n"


class CameraStreamDecoderTest(unittest.TestCase):
    def test_decodes_binary_payload_and_checks_crc(self) -> None:
        from tools.capture_camera_serial import CameraStreamDecoder

        payload = b"\xff\xd8\x00marker-decoy\r\nCAMERA_END\r\n\xff\xd9"
        decoder = CameraStreamDecoder()

        frames = decoder.feed(encoded_frame(payload))

        self.assertEqual(len(frames), 1)
        self.assertEqual(frames[0].payload, payload)
        self.assertEqual(frames[0].index, 0)
        self.assertEqual(frames[0].width, 1280)
        self.assertEqual(frames[0].height, 720)
        self.assertTrue(decoder.ready)

    def test_decodes_when_header_and_payload_arrive_in_fragments(self) -> None:
        from tools.capture_camera_serial import CameraStreamDecoder

        payload = bytes(range(256))
        stream = encoded_frame(payload, index=7)
        decoder = CameraStreamDecoder()
        frames = []

        for offset in range(0, len(stream), 3):
            frames.extend(decoder.feed(stream[offset : offset + 3]))

        self.assertEqual(len(frames), 1)
        self.assertEqual(frames[0].payload, payload)
        self.assertEqual(frames[0].index, 7)

    def test_rejects_truncated_payload(self) -> None:
        from tools.capture_camera_serial import CameraStreamDecoder, StreamProtocolError

        decoder = CameraStreamDecoder()
        decoder.feed(b"CAMERA_BEGIN index=0 width=1 height=1 bytes=4 crc32=00000000\r\nabc")

        with self.assertRaisesRegex(StreamProtocolError, "truncated"):
            decoder.finish()

    def test_rejects_crc_mismatch(self) -> None:
        from tools.capture_camera_serial import CameraStreamDecoder, StreamProtocolError

        payload = b"not-a-jpeg"
        stream = (
            f"CAMERA_BEGIN index=0 width=1 height=1 bytes={len(payload)} "
            "crc32=00000000\r\n"
        ).encode() + payload + b"\r\nCAMERA_END\r\n"
        decoder = CameraStreamDecoder()

        with self.assertRaisesRegex(StreamProtocolError, "CRC32"):
            decoder.feed(stream)

    def test_reads_jpeg_dimensions_without_pillow(self) -> None:
        from PIL import Image
        from tools.capture_camera_serial import jpeg_dimensions

        image_bytes = BytesIO()
        Image.new("RGB", (37, 19), "red").save(image_bytes, format="JPEG")

        self.assertEqual(jpeg_dimensions(image_bytes.getvalue()), (37, 19))

    def test_trigger_is_terminated_for_board_fgets(self) -> None:
        from tools.capture_camera_serial import trigger_bytes

        self.assertEqual(trigger_bytes("c"), b"c\n")
        self.assertEqual(trigger_bytes("capture\r\n"), b"capture\r\n")

    def test_detects_ready_marker_split_across_trimmed_log_buffer(self) -> None:
        from tools.capture_camera_serial import CameraStreamDecoder

        decoder = CameraStreamDecoder()
        decoder.feed(b"x" * 20_000 + b"CAMERA_RE")
        decoder.feed(b"ADY\r\n")

        self.assertTrue(decoder.ready)

    def test_accepts_console_double_carriage_return(self) -> None:
        from tools.capture_camera_serial import CameraStreamDecoder

        payload = b"\xff\xd8data\xff\xd9"
        crc32 = zlib.crc32(payload) & 0xFFFFFFFF
        stream = (
            f"CAMERA_BEGIN index=3 width=1280 height=720 bytes={len(payload)} "
            f"crc32={crc32:08x}\r\r\n"
        ).encode() + payload + b"\r\r\n"
        decoder = CameraStreamDecoder()

        frames = decoder.feed(stream)

        self.assertEqual(len(frames), 1)
        self.assertEqual(frames[0].index, 3)
        self.assertEqual(frames[0].payload, payload)


if __name__ == "__main__":
    unittest.main()
