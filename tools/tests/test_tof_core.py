from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
PROJECT = ROOT / "firmware/esp32s3/tof_bringup"


class TofCoreTests(unittest.TestCase):
    def test_drop_detector_contract(self):
        compiler = shutil.which("gcc")
        if not compiler:
            self.skipTest("gcc is required for host-side TOF tests")

        with tempfile.TemporaryDirectory() as temp_dir:
            executable = Path(temp_dir) / "tof_core_test"
            command = [
                compiler,
                "-std=c11",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-I",
                str(ROOT / "contracts"),
                "-I",
                str(PROJECT / "main"),
                str(ROOT / "tools/tests/tof_core_test.c"),
                str(PROJECT / "main/drop_detector.c"),
                str(PROJECT / "main/calibration.c"),
                "-o",
                str(executable),
            ]
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(
                result.returncode,
                0,
                f"TOF host test failed to compile:\n{result.stdout}\n{result.stderr}",
            )
            result = subprocess.run([str(executable)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
