#!/usr/bin/env python3
"""Check first-party directory guides and local Markdown file links (stdlib only)."""
from pathlib import Path
import re
import subprocess
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]

# Generated asset trees: images only, no prose of their own. The parent guide
# (models/wordless-padded/README.md, data/README.md) documents them.
ASSET_TREES = (
    Path('models/wordless-padded/cards'),
    Path('models/wordless-padded/cards-padded'),
    Path('models/wordless-padded/real-photos'),
    Path('models/wordless-padded/dataset'),
)


def check(root):
    tracked = subprocess.check_output(
        ['git', 'ls-files', '-z'], cwd=root
    ).decode().split('\0')
    paths = [Path(p) for p in tracked if p]
    board_prefix = Path('firmware/xiaozhi-esp32/main/boards/smart-trash-can')
    owned = [p for p in paths if not p.is_relative_to(Path('firmware/xiaozhi-esp32')) or p.is_relative_to(board_prefix)]
    directories = {Path('.')}
    for path in owned:
        directories.update(p for p in path.parents
                           if not p.is_relative_to(Path('firmware/xiaozhi-esp32')) or p.is_relative_to(board_prefix))
    errors = []
    for directory in sorted(directories):
        if any(directory.is_relative_to(tree) for tree in ASSET_TREES):
            continue
        guide = root / directory / 'README.md'
        if not guide.is_file() or directory / 'README.md' not in paths:
            errors.append(f'{directory}: missing README.md')
    # The imported tree is excluded; our board is a documented project entry.
    board = root / 'firmware/xiaozhi-esp32/main/boards/smart-trash-can/README.md'
    if not board.is_file() or board.relative_to(root) not in paths:
        errors.append('custom smart-trash-can board: missing README.md')
    markdown = [p for p in owned if p.suffix.lower() == '.md']
    tracked_targets = {str(p) for p in paths}
    tracked_dirs = {str(parent) for p in paths for parent in p.parents}
    checked = 0
    for path in markdown:
        text = (root / path).read_text(encoding='utf-8')
        # Example links in fenced code are not navigation.
        text = re.sub(r'^(`{3,}|~{3,}).*?^\1[^\n]*$', '', text,
                      flags=re.MULTILINE | re.DOTALL)
        for match in re.finditer(r'!?\[[^\]\n]*\]\((<[^>]+>|[^\s)]+)(?:\s+"[^"]*")?\)', text):
            target = match.group(1).strip('<>')
            parsed = urlsplit(target)
            if parsed.scheme or parsed.netloc or not parsed.path:
                continue
            checked += 1
            resolved = ((root / path).parent / unquote(parsed.path)).resolve()
            if not resolved.is_relative_to(root):
                errors.append(f'{path}: link escapes repository: {target}')
            elif str(resolved.relative_to(root)) not in tracked_targets | tracked_dirs:
                errors.append(f'{path}: broken link: {target}')
    return errors, len(directories), len(markdown), checked


def main():
    errors, directories, markdown, checked = check(ROOT)
    for error in errors:
        print(error)
    print(f'{directories} first-party directories, {markdown} Markdown files, '
          f'{checked} local links, {len(errors)} errors')
    return bool(errors)


if __name__ == '__main__':
    raise SystemExit(main())
