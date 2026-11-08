import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import wave


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools/generate_stc_voice_assets.py"


class VoiceAssetGenerationTests(unittest.TestCase):
    def test_converts_validated_mono_wav_to_raw_pcm(self):
        with tempfile.TemporaryDirectory() as temp_name:
            temp = Path(temp_name)
            source_dir = temp / "source"
            output_dir = temp / "output"
            source_dir.mkdir()
            phrases = temp / "phrases.json"
            phrases.write_text(
                json.dumps(
                    {"open": {"text": "test", "pcm": "stc_open.pcm"}}
                ),
                encoding="utf-8",
            )
            with wave.open(str(source_dir / "open.wav"), "wb") as wav:
                wav.setnchannels(1)
                wav.setsampwidth(2)
                wav.setframerate(16000)
                wav.writeframes(b"\x01\x00\x02\x00")

            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--phrases",
                    str(phrases),
                    "--source-dir",
                    str(source_dir),
                    "--output-dir",
                    str(output_dir),
                ],
                capture_output=True,
                text=True,
            )

            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(
                (output_dir / "stc_open.pcm").read_bytes(),
                b"\x01\x00\x02\x00",
            )

    def test_rejects_wav_with_wrong_sample_rate(self):
        with tempfile.TemporaryDirectory() as temp_name:
            temp = Path(temp_name)
            source_dir = temp / "source"
            output_dir = temp / "output"
            source_dir.mkdir()
            phrases = temp / "phrases.json"
            phrases.write_text(
                json.dumps(
                    {"open": {"text": "test", "pcm": "stc_open.pcm"}}
                ),
                encoding="utf-8",
            )
            with wave.open(str(source_dir / "open.wav"), "wb") as wav:
                wav.setnchannels(1)
                wav.setsampwidth(2)
                wav.setframerate(8000)
                wav.writeframes(b"\x00\x00")

            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--phrases",
                    str(phrases),
                    "--source-dir",
                    str(source_dir),
                    "--output-dir",
                    str(output_dir),
                ],
                capture_output=True,
                text=True,
            )

            self.assertNotEqual(result.returncode, 0)
            self.assertIn("expected mono / 16000 Hz / 16-bit PCM", result.stderr)


if __name__ == "__main__":
    unittest.main()
