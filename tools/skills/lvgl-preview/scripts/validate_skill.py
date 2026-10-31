#!/usr/bin/env python3
"""Validate a standalone skill, or its location within a repository."""
from __future__ import annotations
import argparse
import ast
import json
from pathlib import Path
import re

def validate(root: Path) -> list[str]:
    candidates = [root, root/'skills/lvgl-preview', root/'tools/skills/lvgl-preview']
    skill = next((p for p in candidates if (p/'SKILL.md').is_file()), None)
    if skill is None:
        return ['lvgl-preview/SKILL.md is missing']
    failures = []
    text = (skill/'SKILL.md').read_text(encoding='utf-8')
    meta = re.match(r'^---\n(.*?)\n---\n', text, re.S)
    if not meta or not re.search(r'^name: lvgl-preview$', meta[1], re.M):
        failures.append('skill name/frontmatter is invalid')
    description = re.search(r'^description: (.+)$', meta[1] if meta else '', re.M)
    if not description or not 1 <= len(description[1]) <= 1024:
        failures.append('description must be 1–1024 characters')
    for phrase in ['hardware', '--project-root', 'evaluate_render.py', 'INSUFFICIENT_EVIDENCE']:
        if phrase not in text:
            failures.append(f'missing evaluation contract: {phrase}')
    preview = skill/'scripts/preview.py'
    preview_text = preview.read_text() if preview.is_file() else ''
    for forbidden in ['Leaves the preview pending until', 'record a decision first']:
        if forbidden in text or forbidden in preview_text:
            failures.append(f'obsolete mandatory approval gate: {forbidden}')
    for name in ['scripts/preview.py','scripts/evaluate_render.py','scripts/record_decision.py','references/project-detection.md','references/render-evaluation.md','evals/evals.json','requirements.txt']:
        if not (skill/name).is_file():
            failures.append(f'missing {name}')
    for path in skill.rglob('*.py'):
        if '__pycache__' in path.parts:
            continue
        try:
            ast.parse(path.read_text(), filename=str(path))
        except SyntaxError as exc:
            failures.append(str(exc))
    for path in skill.rglob('*.md'):
        value = path.read_text(encoding='utf-8')
        if re.search(r'/(?:Users|home)/',value):
            failures.append(f'personal absolute path: {path.relative_to(skill)}')
        for link in re.findall(r'\]\(([^)#]+)(?:#[^)]*)?\)',value):
            if link.startswith(('https://','http://','mailto:','#')):
                continue
            if not (path.parent/link).is_file():
                failures.append(f'broken link {link} in {path.name}')
    try:
        data=json.loads((skill/'evals/evals.json').read_text())
        if data.get('skill_name')!='lvgl-preview' or not data.get('evals'):
            failures.append('invalid evaluation scenarios')
    except (OSError,ValueError) as exc:
        failures.append(str(exc))
    for name in ['preview.py','evaluate_render.py','record_decision.py','validate_skill.py']:
        script = skill/'scripts'/name
        if not script.is_file():
            failures.append(f'missing scripts/{name}')
            continue
        if not script.stat().st_mode & 0o111:
            failures.append(f'not executable: {name}')
    return failures

def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',default=str(Path(__file__).resolve().parents[1]))
    args=parser.parse_args()
    failures=validate(Path(args.root).expanduser().resolve())
    for item in failures:print('FAIL '+item)
    if not failures:print('PASS standalone skill structure, links, scripts and evaluation contract')
    return bool(failures)
if __name__=='__main__':raise SystemExit(main())
