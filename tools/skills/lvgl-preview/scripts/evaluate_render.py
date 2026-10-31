#!/usr/bin/env python3
"""Evaluate native LVGL framebuffers and evidence; never synthesize human approval."""
from __future__ import annotations
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import sys

REQUIRED_CHECKS = ('build', 'layout', 'glyphs', 'contrast_touch', 'interaction', 'visual_review')

def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def inside(root: Path, name: str) -> Path:
    root = root.resolve()
    path = (root / name).resolve()
    if not path.is_relative_to(root):
        raise ValueError(f'evidence path escapes project root: {name}')
    return path

def image_metrics(path: Path, width: int, height: int) -> dict:
    from PIL import Image
    with Image.open(path) as image:
        image.verify()
    with Image.open(path) as image:
        if image.size != (width, height):
            raise ValueError(f'{path.name}: dimensions {image.size}, expected {(width, height)}')
        sample = image.convert('RGBA')
        pixel_hash = hashlib.sha256(sample.tobytes()).hexdigest()
        sample.thumbnail((160, 160), Image.Resampling.NEAREST)
        pixels = list(sample.get_flattened_data())
        visible = [p[:3] for p in pixels if p[3] > 0]
        if not visible:
            raise ValueError(f'{path.name}: fully transparent render')
        colors = Counter(visible)
        foreground = 1 - max(colors.values()) / len(visible)
        if foreground < 0.005:
            raise ValueError(f'{path.name}: blank or nearly uniform render ({foreground:.4%} non-modal pixels)')
        return {'width': width, 'height': height, 'encoding': image.format,
                'pixel_sha256': pixel_hash, 'sample_colors': len(colors), 'non_modal_fraction': round(foreground, 6)}

def evaluate(root: Path, manifest: dict, state: dict) -> dict:
    failures, missing, frames = [], [], []
    try:
        from PIL import Image  # noqa: F401
    except ImportError:
        return {'result': 'INSUFFICIENT_EVIDENCE', 'failures': [], 'missing': ['Pillow is required; install scripts/../requirements.txt in the Python environment'], 'frames': [], 'hardware_verified': False}
    profile = manifest.get('display', {})
    if profile != state.get('display'):
        failures.append('manifest display profile differs from the native preview run')
    if not state.get('runtime_gate'):
        missing.append('native runtime-liveness evidence')
    try:
        executable_hash = digest(inside(root, state['executable']))
        if executable_hash != state.get('executable_sha256'):
            failures.append('executable changed since the preview run')
        for name, expected in state.get('source_hashes', {}).items():
            if digest(inside(root, name)) != expected:
                failures.append(f'preview source/configuration changed: {name}')
        if not state.get('source_hashes'):
            missing.append('preview source/configuration hashes; rerun the updated preview runner')
    except (KeyError, OSError, ValueError) as exc:
        failures.append(str(exc))
        executable_hash = None
    seen_states, seen_ids = set(), set()
    for frame in manifest.get('frames', []):
        try:
            frame_id = frame['id']
            if frame_id in seen_ids:
                raise ValueError(f'duplicate frame id: {frame_id}')
            seen_ids.add(frame_id)
            seen_states.add(frame['state'])
            path = inside(root, frame['path'])
            actual_hash = digest(path)
            if frame.get('sha256') != actual_hash:
                raise ValueError(f'{frame_id}: framebuffer hash missing or stale')
            if frame.get('evidence_kind') not in ('framebuffer', 'window'):
                raise ValueError(f'{frame_id}: evidence_kind must be framebuffer or window')
            metrics = image_metrics(path, profile['width'], profile['height'])
            frames.append({'id': frame_id, 'state': frame['state'], 'path': frame['path'],
                           'sha256': actual_hash, 'evidence_kind': frame['evidence_kind'], **metrics})
        except (KeyError, OSError, ValueError) as exc:
            failures.append(str(exc))
    required_states = set(manifest.get('required_states', []))
    if not required_states or not manifest.get('frames'):
        missing.append('required states and native framebuffer captures')
    elif required_states - seen_states:
        failures.append(f'missing states: {sorted(required_states - seen_states)}')
    if set(manifest.get('required_frames', [])) - seen_ids:
        failures.append(f'missing state/category variants: {sorted(set(manifest["required_frames"]) - seen_ids)}')
    evidence_results = {}
    for name in REQUIRED_CHECKS:
        entry = manifest.get('checks', {}).get(name)
        if not entry:
            missing.append(f'{name} evidence')
            continue
        try:
            path = inside(root, entry['evidence'])
            if digest(path) != entry.get('sha256'):
                raise ValueError(f'{name}: evidence hash missing or stale')
            report = json.loads(path.read_text(encoding='utf-8'))
            check = report.get('checks', {}).get(name, {})
            status = check.get('status')
            evidence_results[name] = check
            if status == 'FAIL':
                failures.append(f'{name}: {check.get("details", "reported failure")}')
            elif status != 'PASS':
                missing.append(f'{name}: {status or "no result"}')
            if name in ('build', 'layout', 'glyphs', 'interaction') and report.get('executable_sha256') != executable_hash:
                raise ValueError(f'{name}: evidence is for a different executable')
            if name == 'visual_review':
                reviewed = report.get('frame_hashes', {})
                for frame in frames:
                    if reviewed.get(frame['id']) != frame['sha256'] and report.get('frame_pixel_hashes', {}).get(frame['id']) != frame['pixel_sha256']:
                        raise ValueError(f'visual review missing/stale for {frame["id"]}')
        except (KeyError, OSError, ValueError) as exc:
            failures.append(str(exc))
    result = 'FAIL' if failures else 'INSUFFICIENT_EVIDENCE' if missing else 'PASS'
    return {'result': result, 'display': profile, 'frames': frames, 'checks': evidence_results,
            'failures': failures, 'missing': missing, 'hardware_verified': False,
            'limits': ['Pixel statistics detect empty captures, not semantic correctness.',
                       'Layout/glyph/interaction checks come from the native application.',
                       'Visual review is agent evidence, not human approval or hardware proof.']}

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project-root', required=True)
    parser.add_argument('--manifest', required=True, help='manifest path relative to project root')
    args = parser.parse_args()
    from preview import PreviewError, append_ledger
    root = Path(args.project_root).expanduser().resolve()
    state_path = root / '.lvgl-preview/state.json'
    try:
        state = json.loads(state_path.read_text(encoding='utf-8'))
        manifest = json.loads(inside(root, args.manifest).read_text(encoding='utf-8'))
        report = evaluate(root, manifest, state)
        report['preview_window'] = 'lifecycle independent of evaluation; no persisted PID signalled'
        state.pop('user_decision', None)  # migrate the old mandatory pending field without inventing feedback
        state['evaluation'] = report['result']
        state['completion_status'] = 'completed' if report['result'] == 'PASS' else 'needs_repair' if report['result'] == 'FAIL' else 'evidence_incomplete'
        report['run_id'] = state['run_id']
        report_path = root / '.lvgl-preview/render-eval.json'
        report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
        state['evaluation_report'] = '.lvgl-preview/render-eval.json'
        state_path.write_text(json.dumps(state, indent=2) + '\n', encoding='utf-8')
        append_ledger(root / '.lvgl-preview', {'id': state['run_id'], 'evaluation': report['result'], 'reason': 'native render evidence evaluated', 'hardware_verified': False})
        print(json.dumps({'result': report['result'], 'frames': len(report['frames']), 'report': str(report_path), 'failures': report['failures'], 'missing': report['missing']}, ensure_ascii=False, indent=2))
        return {'PASS': 0, 'FAIL': 1, 'INSUFFICIENT_EVIDENCE': 2}[report['result']]
    except (OSError, ValueError, KeyError, PreviewError) as exc:
        print(f'FAIL {exc}', file=sys.stderr)
        return 1
if __name__ == '__main__':
    raise SystemExit(main())
