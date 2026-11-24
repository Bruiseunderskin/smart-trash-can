import struct
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SFX = ROOT / "firmware/assets/sfx"


class S31SfxAssetsTest(unittest.TestCase):
    def test_ogg_opus_assets_are_mono_48khz(self):
        expected = {
            "sfx-scan.ogg",
            "sfx-decide.ogg",
            "sfx-correct.ogg",
            "sfx-wrong.ogg",
            "sfx-retry.ogg",
            "sfx-round.ogg",
        }
        self.assertEqual({path.name for path in SFX.glob("*.ogg")}, expected)

        for path in SFX.glob("*.ogg"):
            data = path.read_bytes()
            self.assertEqual(data[:4], b"OggS", path.name)
            head = data.index(b"OpusHead")
            self.assertEqual(data[head + 8], 1, path.name)  # version
            self.assertEqual(data[head + 9], 1, path.name)  # mono
            self.assertEqual(
                struct.unpack_from("<I", data, head + 12)[0],
                16000,
                path.name,
            )

    def test_sfx_generator_matches_the_16khz_codec_and_voice_headroom(self):
        script = (
            ROOT / "firmware/assets/sfx/make_sfx.py"
        ).read_text(encoding="utf-8")
        self.assertIn("SR = 16000", script)
        self.assertIn("* 0.22", script)
        self.assertIn('"-ar", "16000"', script)
    def test_s31_assets_build_includes_voice_and_sfx(self):
        cmake = (
            ROOT / "firmware/xiaozhi-esp32/main/CMakeLists.txt"
        ).read_text(encoding="utf-8")
        builder = (
            ROOT / "firmware/xiaozhi-esp32/scripts/build_default_assets.py"
        ).read_text(encoding="utf-8")

        self.assertIn('"${CMAKE_CURRENT_LIST_DIR}/../../assets/voice"', cmake)
        self.assertIn('"${CMAKE_CURRENT_LIST_DIR}/../../assets/sfx"', cmake)
        self.assertIn("action='append'", builder)

    def test_mono_voice_is_converted_only_by_the_audio_codec(self):
        voice = (
            ROOT
            / "firmware/xiaozhi-esp32/main/boards/espressif/"
            "esp32-s31-korvo-1-stc/stc_voice.cc"
        ).read_text(encoding="utf-8")

        self.assertIn(
            "output.assign(pcm + offset, pcm + offset + count);",
            voice,
        )
        self.assertIn("Es8389AudioCodec::Write() owns the conversion", voice)
        self.assertNotIn("output.resize(count * 2);", voice)


if __name__ == "__main__":
    unittest.main()
