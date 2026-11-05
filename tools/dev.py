#!/usr/bin/env python3
"""Shared local/CI checks. Run from any directory; no flashing or auto-downloads."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
ARDUINO = ('ESP32S3_Mic_Test', 'MG90_LidServo_ESP32S3', 'PCA9685_4Servo_Test')
IDF = ('pca9685_servo_test', 'tflite_probe', 'ei_latency_probe', 'tof_bringup')
BOARDS = ('smart-trash-can', 'esp32-s31-korvo-1')
S31 = ('s31_bringup', 'esp32-s31-korvo-1')


def idf_launcher():
    name = 'idf.py.exe' if os.name == 'nt' else 'idf.py'
    return shutil.which(name)


def run(*args, cwd=ROOT):
    print('+', ' '.join(map(str, args)), flush=True)
    subprocess.run(args, cwd=cwd, check=True)


def compile_project(project):
    idf = idf_launcher()
    if project in S31:
        if not idf:
            raise RuntimeError('Activate ESP-IDF 6.1 for ESP32-S31.')
        version = subprocess.check_output([idf, '--version'], text=True)
        import re
        match = re.search(r'v(\d+)\.(\d+)', version)
        if not match or tuple(map(int, match.groups())) < (6, 1):
            raise RuntimeError('ESP32-S31 requires ESP-IDF >=6.1; activate its export.sh.')
    if project == 's31_bringup':
        run(idf, '--preview', '-C', str(ROOT / 'firmware/esp32s31/bringup'),
            '-B', str(ROOT / 'build/idf/s31_bringup'), '-D', 'IDF_TARGET=esp32s31', 'build')
        return
    if project in ARDUINO:
        if not shutil.which('arduino-cli'):
            raise RuntimeError('Install arduino-cli, esp32:esp32@3.3.12 and ESP32Servo@3.0.9 first.')
        run('arduino-cli', 'compile', '--fqbn', 'esp32:esp32:esp32s3', str(ROOT / 'firmware' / 'arduino' / project))
    elif project in BOARDS:
        if not idf:
            raise RuntimeError('Activate ESP-IDF 6.0.2 first.')
        board_dir = 'espressif/' + project if project == 'esp32-s31-korvo-1' else project
        run(sys.executable, 'scripts/build.py', board_dir, '--name', project, cwd=ROOT / 'firmware' / 'xiaozhi-esp32')
    elif project in IDF:
        if not idf:
            raise RuntimeError('Activate ESP-IDF 6.0.2 with export.sh first.')
        path = ROOT / 'firmware' / 'esp32s3' / project
        if project in ('tflite_probe', 'ei_latency_probe') and not (path / 'main/models/waste4_int8.tflite').is_file():
            raise RuntimeError(f'Provide {path}/main/models/waste4_int8.tflite as described in its README.')
        # -D fixes the target without destructive set-target/fullclean.
        run(idf, '-C', str(path), '-B', str(ROOT / 'build/idf' / project), '-D', 'IDF_TARGET=esp32s3', 'build')


def require_submitted_content(stage):
    commands = [['git', 'diff', '--quiet']]
    if stage != 'pre-commit':
        commands.append(['git', 'diff', '--cached', '--quiet'])
    for command in commands:
        result = subprocess.run(command, cwd=ROOT)
        if result.returncode:
            raise RuntimeError('Uncommitted tracked changes would mask submitted content. Stage all changes before pre-commit; commit tracked changes before push/PR checks.')


def checks(stage, base):
    require_submitted_content(stage)
    run('git', 'diff', '--check')
    run('git', 'diff', '--cached', '--check')
    run(sys.executable, 'tools/check_docs.py')
    if stage == 'pre-commit':
        return
    try:
        import PIL  # noqa: F401
        import numpy  # noqa: F401
    except ImportError as error:
        raise RuntimeError('Install tools/requirements-dev.txt in your Python environment.') from error
    run(sys.executable, '-m', 'unittest', 'discover', '-s', 'tools/skills/lvgl-preview/tests', '-v')
    run(sys.executable, '-m', 'unittest', 'discover', '-s', 'tools/tests', '-v')
    # Never silently skip compilation for changed first-party firmware.
    changed = subprocess.check_output(['git', 'diff', '--name-only', f'{base}...HEAD'], cwd=ROOT, text=True).splitlines()
    changed += subprocess.check_output(['git', 'diff', '--name-only', 'HEAD'], cwd=ROOT, text=True).splitlines()
    for project in ARDUINO:
        if any(p.startswith(f'firmware/arduino/{project}/') and not p.endswith('.md') for p in changed):
            compile_project(project)
    for project in IDF:
        if any(p.startswith(f'firmware/esp32s3/{project}/') and not p.endswith('.md') for p in changed):
            compile_project(project)
    if any(p.startswith('firmware/xiaozhi-esp32/') and not p.endswith('.md') for p in changed):
        compile_project('smart-trash-can')
    if any(p.startswith('firmware/esp32s31/') and not p.endswith('.md') for p in changed):
        compile_project('s31_bringup')
    if any(p.startswith('firmware/xiaozhi-esp32/') and not p.endswith('.md') for p in changed):
        compile_project('esp32-s31-korvo-1')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('pre-commit', 'pre-push', 'pre-pr', 'compile', 'install-hooks', 'pr'))
    parser.add_argument('--base', default='origin/main')
    parser.add_argument('--project', choices=ARDUINO + IDF + BOARDS + ('s31_bringup',))
    args, extra = parser.parse_known_args()
    if args.action != 'pr' and extra:
        parser.error('unrecognized arguments: ' + ' '.join(extra))
    if args.action == 'install-hooks':
        current = subprocess.run(['git', 'config', '--get', 'core.hooksPath'], cwd=ROOT, capture_output=True, text=True)
        if current.stdout.strip() not in ('', '.githooks'):
            raise RuntimeError('Existing core.hooksPath found; integrate the hooks manually instead of overwriting it.')
        run('git', 'config', '--local', 'core.hooksPath', '.githooks')
    elif args.action == 'compile':
        if not args.project:
            parser.error('compile requires --project')
        compile_project(args.project)
    else:
        checks('pre-pr' if args.action == 'pr' else args.action, args.base)
        if args.action == 'pr':
            run('gh', 'pr', 'create', *([x for x in extra if x != '--']))


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, subprocess.CalledProcessError) as error:
        print(f'Check failed: {error}', file=sys.stderr)
        sys.exit(1)
