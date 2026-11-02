import importlib.util
from pathlib import Path
import subprocess
import unittest
import sys
from unittest.mock import MagicMock
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location('dev', Path(__file__).parents[1] / 'dev.py')
DEV = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(DEV)


class DevelopmentChecks(unittest.TestCase):
    def test_precommit_runs_only_fast_checks(self):
        with patch.object(DEV, 'require_submitted_content'), patch.object(DEV, 'run') as run, patch.object(DEV.subprocess, 'check_output') as changed:
            DEV.checks('pre-commit', 'origin/main')
            self.assertEqual(run.call_count, 3)
            changed.assert_not_called()

    def test_changed_sketch_compiles_but_readme_does_not(self):
        with patch.object(DEV, 'require_submitted_content'), patch.dict(sys.modules, {'PIL': MagicMock()}), patch.object(DEV, 'run'), patch.object(DEV, 'compile_project') as compile_project:
            with patch.object(DEV.subprocess, 'check_output', side_effect=['arduino/PCA9685_4Servo_Test/PCA9685_4Servo_Test.ino\n', '']):
                DEV.checks('pre-push', 'origin/main')
            compile_project.assert_called_once_with('PCA9685_4Servo_Test')
            compile_project.reset_mock()
            with patch.object(DEV.subprocess, 'check_output', side_effect=['arduino/PCA9685_4Servo_Test/README.md\n', '']):
                DEV.checks('pre-pr', 'origin/main')
            compile_project.assert_not_called()

    def test_unstaged_content_is_rejected(self):
        with patch.object(DEV.subprocess, 'run', return_value=MagicMock(returncode=1)):
            with self.assertRaisesRegex(RuntimeError, 'Uncommitted tracked'):
                DEV.require_submitted_content('pre-commit')

    def test_prepush_also_checks_staged_content(self):
        with patch.object(DEV.subprocess, 'run', side_effect=[MagicMock(returncode=0), MagicMock(returncode=1)]):
            with self.assertRaises(RuntimeError):
                DEV.require_submitted_content('pre-push')

    def test_missing_toolchain_stops(self):
        with patch.object(DEV.shutil, 'which', return_value=None):
            with self.assertRaisesRegex(RuntimeError, 'ESP-IDF'):
                DEV.compile_project('pca9685_servo_test')

    def test_failed_checks_prevent_pr_creation(self):
        with patch.object(DEV.sys, 'argv', ['dev.py', 'pr', '--', '--title', 'Example']), patch.object(DEV, 'checks', side_effect=subprocess.CalledProcessError(1, 'test')), patch.object(DEV, 'run') as run:
            with self.assertRaises(subprocess.CalledProcessError):
                DEV.main()
            run.assert_not_called()


if __name__ == '__main__':
    unittest.main()
