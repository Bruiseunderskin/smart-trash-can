"""Exercise the coverage boundary and tracked-link failures in real Git indexes."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location('check_docs', Path(__file__).parents[1] / 'check_docs.py')
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class DocsChecks(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name).resolve()
        subprocess.run(['git', 'init', '-q', str(self.root)], check=True)
        self.write('README.md', '# Project\n')
        self.write('xiaozhi-esp32/main/boards/smart-trash-can/README.md', '# Board\n')

    def write(self, name, text):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
        subprocess.run(['git', 'add', name], cwd=self.root, check=True)

    def test_new_directory_needs_guide(self):
        self.write('new/main.c', '// test')
        self.assertIn('new: missing README.md', MODULE.check(self.root)[0])

    def test_upstream_excluded_custom_board_included(self):
        self.write('xiaozhi-esp32/upstream/foo.c', '// upstream')
        self.assertEqual([], MODULE.check(self.root)[0])
        self.write('xiaozhi-esp32/main/boards/smart-trash-can/nested/foo.c', '// own')
        self.assertTrue(any('nested: missing README' in e for e in MODULE.check(self.root)[0]))

    def test_links_and_untracked_targets(self):
        self.write('README.md', '[good](docs/) [encoded](docs/a%20b.txt) [missing](absent.txt)\n')
        self.write('docs/README.md', '# Docs\n')
        self.write('docs/a b.txt', 'data')
        (self.root / 'absent.txt').write_text('untracked')
        errors = MODULE.check(self.root)[0]
        self.assertEqual(['README.md: broken link: absent.txt'], errors)

    def test_fenced_examples_and_external_links(self):
        self.write('README.md', '```md\n[example](missing)\n```\n[web](https://example.com)\n[heading](#x)\n')
        self.assertEqual([], MODULE.check(self.root)[0])


if __name__ == '__main__':
    unittest.main()
