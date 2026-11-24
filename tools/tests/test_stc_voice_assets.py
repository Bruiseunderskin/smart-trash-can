import hashlib
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
    def test_retry_asset_is_the_approved_raw_pcm(self):
        retry = (
            ROOT / "firmware/assets/voice/stc_retry.pcm"
        ).read_bytes()

        self.assertEqual(len(retry), 91392)
        self.assertEqual(
            hashlib.sha256(retry).hexdigest(),
            "0dc356f27ac9d0302cf871804c2e1bc0"
            "443605decaf38befe6fe8f0048bbabd6",
        )

    def test_all_engagement_phrases_are_present_as_raw_pcm(self):
        phrases = json.loads(
            (ROOT / "firmware/assets/voice/phrases.json").read_text(
                encoding="utf-8"
            )
        )
        expected = {
            "open",
            "correct",
            "wrong",
            "timeout",
            "retry",
            "summary",
            "cheer_correct_a",
            "cheer_correct_b",
            "encourage_wrong_a",
            "encourage_wrong_b",
            "summary_tip",
        }
        self.assertEqual(set(phrases), expected)
        for key, spec in phrases.items():
            data = (ROOT / "firmware/assets/voice" / spec["pcm"]).read_bytes()
            self.assertGreater(len(data), 16000, key)
            self.assertEqual(len(data) % 2, 0, key)

    def test_voice_playback_uses_40ms_chunks(self):
        voice = (
            ROOT
            / "firmware/xiaozhi-esp32/main/boards/espressif/"
            "esp32-s31-korvo-1-stc/stc_voice.cc"
        ).read_text(encoding="utf-8")
        self.assertIn(
            "constexpr size_t kChunkSamples = 640;  // 40 ms at 16 kHz.",
            voice,
        )
        self.assertIn("vTaskDelay(pdMS_TO_TICKS(40));", voice)
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

    def test_keys_regenerates_only_selected_phrase(self):
        with tempfile.TemporaryDirectory() as temp_name:
            temp = Path(temp_name)
            source_dir = temp / "source"
            output_dir = temp / "output"
            source_dir.mkdir()
            phrases = temp / "phrases.json"
            phrases.write_text(
                json.dumps(
                    {
                        "open": {"text": "open", "pcm": "stc_open.pcm"},
                        "retry": {"text": "retry", "pcm": "stc_retry.pcm"},
                    }
                ),
                encoding="utf-8",
            )
            with wave.open(str(source_dir / "retry.wav"), "wb") as wav:
                wav.setnchannels(1)
                wav.setsampwidth(2)
                wav.setframerate(16000)
                wav.writeframes(b"\x03\x00\x04\x00")

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
                    "--keys",
                    "retry",
                ],
                capture_output=True,
                text=True,
            )

            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertFalse((output_dir / "stc_open.pcm").exists())
            self.assertEqual(
                (output_dir / "stc_retry.pcm").read_bytes(),
                b"\x03\x00\x04\x00",
            )


if __name__ == "__main__":
    unittest.main()
