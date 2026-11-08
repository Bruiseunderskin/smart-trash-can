#!/usr/bin/env python3
"""Cross-check the mode-gate contract against its spec document.

The idle timeout is written in two places: `contracts/stc_mode.h` (the contract)
and `docs/mode-gate.md` (the spec plus its acceptance rows). When someone edits
one and forgets the other, the firmware is written against a number the spec
does not agree with — which is exactly how a 60 s / 90 s mix-up shipped once.

This check fails the build when the two disagree. The header is the source of
truth; the document has to follow.

Usage:
    python3 tools/check_mode_gate.py [--root .]
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

HEADER = Path("contracts/stc_mode.h")
DOC = Path("docs/mode-gate.md")

HEADER_SECONDS = re.compile(r"CHAT 閒置 (\d+) s")
HEADER_VERSION = re.compile(r"contract v(\d+\.\d+\.\d+)")
DOC_VERSION = re.compile(r"\*\*v(\d+\.\d+\.\d+)，待實作\*\*")
# every place the document states the timeout
DOC_SECONDS = [
    re.compile(r"IN_CHAT_TIMEOUT`（(\d+) s）"),
    re.compile(r"## \d+\. (\d+) 秒自動退出"),
    re.compile(r"(\d+) s 自動退出"),
    re.compile(r"CHAT 閒置 (\d+) s ± 1 s"),
]


def read(path: Path) -> str:
    if not path.exists():
        raise SystemExit(f"missing file: {path}")
    return path.read_text(encoding="utf-8")


def check(root: Path) -> list[str]:
    header = read(root / HEADER)
    doc = read(root / DOC)
    problems: list[str] = []

    h_seconds = {int(m) for m in HEADER_SECONDS.findall(header)}
    if len(h_seconds) != 1:
        problems.append(
            f"{HEADER}: expected exactly one 'CHAT 閒置 N s' comment, found {sorted(h_seconds) or 'none'}"
        )
        return problems
    expected = h_seconds.pop()

    if expected not in (30, 45, 60, 90, 120):
        problems.append(f"{HEADER}: idle timeout {expected} s is outside the reviewed set")

    h_version = HEADER_VERSION.search(header)
    d_version = DOC_VERSION.search(doc)
    if not h_version:
        problems.append(f"{HEADER}: no 'contract vX.Y.Z' marker")
    if not d_version:
        problems.append(f"{DOC}: no '**vX.Y.Z，待實作**' marker")
    if h_version and d_version and h_version.group(1) != d_version.group(1):
        problems.append(
            f"version mismatch: {HEADER} says v{h_version.group(1)}, {DOC} says v{d_version.group(1)}"
        )

    seen: list[int] = []
    for pattern in DOC_SECONDS:
        for value in pattern.findall(doc):
            seen.append(int(value))
            if int(value) != expected:
                problems.append(
                    f"{DOC}: states {value} s but {HEADER} says {expected} s "
                    f"(pattern {pattern.pattern!r})"
                )

    if not seen:
        problems.append(f"{DOC}: no idle-timeout value found — the check would pass vacuously")

    # milliseconds form
    for value in re.findall(r"t_last_activity > (\d{4,6})", doc):
        if int(value) != expected * 1000:
            problems.append(f"{DOC}: uses {value} ms but the contract says {expected * 1000} ms")

    return problems


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".", type=Path)
    args = parser.parse_args()

    problems = check(args.root)
    if problems:
        print("mode-gate contract check: FAIL")
        for problem in problems:
            print(f"  - {problem}")
        print("\nFix the document to match the header, or change both together.")
        return 1

    header = read(args.root / HEADER)
    seconds = HEADER_SECONDS.search(header).group(1)
    print(f"mode-gate contract check: PASS ({seconds} s idle timeout, versions agree)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
