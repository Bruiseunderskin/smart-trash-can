from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
BOARD = ROOT / "firmware/xiaozhi-esp32/main/boards/espressif/esp32-s31-korvo-1-stc"


class AppFsmTests(unittest.TestCase):
    def test_mode_state_machine(self):
        compiler = shutil.which("gcc")
        if not compiler:
            self.skipTest("gcc is required for host-side app FSM tests")
        with tempfile.TemporaryDirectory() as temp_dir:
            executable = Path(temp_dir) / "stc_app_fsm_test"
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
                str(ROOT / "tools/tests/stc_app_fsm_test.c"),
                str(BOARD / "stc_app_fsm.c"),
                "-o",
                str(executable),
            ]
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(
                result.returncode,
                0,
                f"App FSM test failed to compile:\n{result.stdout}\n{result.stderr}",
            )
            result = subprocess.run([str(executable)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
