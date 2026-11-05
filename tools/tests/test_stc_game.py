from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
BOARD = ROOT / "firmware/xiaozhi-esp32/main/boards/espressif/esp32-s31-korvo-1-stc"


class GameCoreTests(unittest.TestCase):
    def test_game_contract(self):
        compiler = shutil.which("gcc")
        if not compiler:
            self.skipTest("gcc is required for host-side game tests")
        with tempfile.TemporaryDirectory() as temp_dir:
            executable = Path(temp_dir) / "stc_game_test"
            command = [
                compiler,
                "-std=c11",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-I",
                str(ROOT / "contracts"),
                "-I",
                str(BOARD),
                str(ROOT / "tools/tests/stc_game_test.c"),
                str(BOARD / "stc_game.c"),
                "-o",
                str(executable),
            ]
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(
                result.returncode,
                0,
                f"Game test failed to compile:\n{result.stdout}\n{result.stderr}",
            )
            result = subprocess.run([str(executable)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
