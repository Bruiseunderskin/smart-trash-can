#!/usr/bin/env python3
"""Package only version-controlled LVGL, design and portable-skill deliverables."""
from pathlib import Path
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[3]
DEST = ROOT / 'ui/lvgl/dist/smart-trash-can-lvgl-800x480.zip'
paths = subprocess.check_output(
    ['git', 'ls-files', '-z', 'design', 'ui/lvgl', 'tools/skills/lvgl-preview'], cwd=ROOT
).decode().split('\0')
DEST.parent.mkdir(parents=True, exist_ok=True)
with zipfile.ZipFile(DEST, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
    for name in sorted(filter(None, paths)):
        if name.startswith('ui/lvgl/dist/'):
            continue
        info = zipfile.ZipInfo(name, date_time=(2026, 11, 1, 0, 0, 0))
        info.compress_type = zipfile.ZIP_DEFLATED
        info.external_attr = (0o100755 if (ROOT/name).stat().st_mode & 0o111 else 0o100644) << 16
        archive.writestr(info, (ROOT/name).read_bytes())
print(f'{DEST.relative_to(ROOT)}: {DEST.stat().st_size} bytes')
