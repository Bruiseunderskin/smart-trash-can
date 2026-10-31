#!/usr/bin/env python3
"""Safely configure, build, and launch a native LVGL SDL preview."""

from __future__ import annotations

import argparse
import datetime as dt
import json
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time
from typing import Iterable


ROTATIONS = {0, 90, 180, 270}
DEPTHS = {16, 24, 32}
DISPLAY_RE = re.compile(r"\bsdl_hal_init\s*\(\s*\d+\s*,\s*\d+\s*\)")
DEPTH_RE = re.compile(r"^\s*#define\s+LV_COLOR_DEPTH\s+\d+\s*$", re.MULTILINE)
ROTATION_RE = re.compile(r"^[ \t]*lv_display_set_rotation\s*\([^;]+\);[ \t]*$", re.MULTILINE)
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".h", ".hh", ".hpp"}


class PreviewError(RuntimeError):
    """An actionable preview failure."""


def project_root(value: str | None) -> Path:
    candidate = value or os.environ.get("LVGL_PREVIEW_PROJECT") or os.getcwd()
    root = Path(candidate).expanduser().resolve()
    if not root.is_dir():
        raise PreviewError(f"project root does not exist: {root}")
    return root


def inside(root: Path, candidate: Path) -> Path:
    resolved = candidate.expanduser().resolve()
    try:
        resolved.relative_to(root)
    except ValueError as exc:
        raise PreviewError(f"path must stay inside project root: {resolved}") from exc
    return resolved


def relative(root: Path, path: Path) -> str:
    return path.relative_to(root).as_posix()


def read_text(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except UnicodeDecodeError as exc:
        raise PreviewError(f"expected a UTF-8 source file: {path}") from exc


def candidate_files(root: Path) -> Iterable[Path]:
    ignored = {".git", ".lvgl-preview", "build", "cmake-build-debug", "cmake-build-release", "bin"}
    for path in root.rglob("*"):
        if not path.is_file() or path.suffix.lower() not in SOURCE_SUFFIXES:
            continue
        if any(part in ignored for part in path.relative_to(root).parts):
            continue
        yield path


def choose_file(root: Path, explicit: str | None, kind: str, matcher) -> Path:
    if explicit:
        path = inside(root, root / explicit)
        if not path.is_file():
            raise PreviewError(f"{kind} file does not exist: {relative(root, path)}")
        if not matcher(read_text(path)):
            raise PreviewError(f"{kind} file does not contain the expected configuration: {relative(root, path)}")
        return path
    matches = [path for path in candidate_files(root) if matcher(read_text(path))]
    if len(matches) != 1:
        names = ", ".join(relative(root, path) for path in matches[:8]) or "none"
        raise PreviewError(f"expected exactly one {kind} match; found {len(matches)} ({names}). Pass --{kind.replace('_', '-')}-file.")
    return matches[0]


def find_files(root: Path, main_file: str | None, config_file: str | None) -> tuple[Path, Path]:
    main = choose_file(root, main_file, "main", lambda text: bool(DISPLAY_RE.search(text)))
    config = choose_file(root, config_file, "config", lambda text: bool(DEPTH_RE.search(text)))
    return main, config


def rotation_line(rotation: int) -> str:
    return f"  lv_display_set_rotation(lv_display_get_default(), LV_DISPLAY_ROTATION_{rotation});"


def patch_main(text: str, width: int, height: int, rotation: int) -> str:
    match = DISPLAY_RE.search(text)
    if not match:
        raise PreviewError("display initialization disappeared while patching")
    replacement = f"sdl_hal_init({width}, {height})"
    updated = text[: match.start()] + replacement + text[match.end() :]
    updated = re.sub(r"(sdl_hal_init\(\d+,\s*\d+\));+", r"\1;", updated)
    line = rotation_line(rotation)
    if ROTATION_RE.search(updated):
        updated = ROTATION_RE.sub(line, updated, count=1)
    else:
        anchor = re.search(r"^.*sdl_hal_init\([^;]+\);.*$", updated, re.MULTILINE)
        if not anchor:
            raise PreviewError("could not place the display rotation setting")
        updated = updated[: anchor.end()] + "\n" + line + updated[anchor.end() :]
    return updated


def patch_config(text: str, depth: int) -> str:
    updated, count = DEPTH_RE.subn(f"#define LV_COLOR_DEPTH {depth}", text, count=1)
    if count != 1:
        raise PreviewError("color-depth definition disappeared while patching")
    return updated


def atomic_write(path: Path, text: str) -> None:
    temp = path.with_name(f".{path.name}.lvgl-preview-tmp")
    temp.write_text(text, encoding="utf-8")
    shutil.copymode(path, temp)
    temp.replace(path)


def state_dir(root: Path) -> Path:
    path = root / ".lvgl-preview"
    path.mkdir(exist_ok=True)
    (path / "backups").mkdir(exist_ok=True)
    (path / "logs").mkdir(exist_ok=True)
    return path


def json_write(path: Path, data: dict) -> None:
    temp = path.with_name(f".{path.name}.tmp")
    temp.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    temp.replace(path)


def append_ledger(directory: Path, entry: dict) -> None:
    entry = dict(entry)
    entry["timestamp"] = dt.datetime.now(dt.timezone.utc).isoformat()
    with (directory / "ledger.jsonl").open("a", encoding="utf-8") as stream:
        stream.write(json.dumps(entry, sort_keys=True) + "\n")


def backup_files(root: Path, paths: list[Path], run_id: str) -> Path:
    backup = state_dir(root) / "backups" / run_id
    backup.mkdir(parents=True, exist_ok=False)
    for path in paths:
        target = backup / relative(root, path)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
    return backup


def restore_files(root: Path, backup: Path, paths: list[str]) -> None:
    for name in paths:
        source = backup / name
        target = inside(root, root / name)
        if not source.is_file():
            raise PreviewError(f"rollback backup is missing: {name}")
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)


def archive_recorded(root: Path, state: dict) -> None:
    """Replace run bookkeeping without signalling any persisted PID."""
    directory = state_dir(root)
    archive = directory / "runs"
    archive.mkdir(exist_ok=True)
    json_write(archive / (state["run_id"] + ".json"), state)
    append_ledger(directory, {"id": state["run_id"], "evaluation": "superseded", "reason": "replaced for a requested revision; prior window lifecycle unchanged"})


def executable_path(root: Path, explicit: str | None) -> Path:
    if explicit:
        path = inside(root, root / explicit)
        if not path.is_file():
            raise PreviewError(f"executable does not exist: {relative(root, path)}")
        return path
    candidates = [root / "bin/main", root / "build/main", root / "build/Debug/main", root / "build/Release/main"]
    found = [path for path in candidates if path.is_file()]
    if len(found) != 1:
        names = ", ".join(relative(root, path) for path in found) or "none"
        raise PreviewError(f"expected exactly one default executable; found {len(found)} ({names}). Pass --executable.")
    return found[0]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", help="LVGL simulator root; defaults to LVGL_PREVIEW_PROJECT or cwd")
    parser.add_argument("--width", type=int, required=True)
    parser.add_argument("--height", type=int, required=True)
    parser.add_argument("--rotation", type=int, choices=sorted(ROTATIONS), default=0)
    parser.add_argument("--color-depth", type=int, choices=sorted(DEPTHS), default=16)
    parser.add_argument("--main-file", help="source path relative to project root")
    parser.add_argument("--config-file", help="lv_conf.h path relative to project root")
    parser.add_argument("--build-dir", default="build", help="CMake build directory relative to project root")
    parser.add_argument("--executable", help="native executable path relative to project root")
    parser.add_argument("--startup-timeout", type=float, default=2.0)
    parser.add_argument("--dry-run", action="store_true", help="show targets and commands without changing files or building")
    parser.add_argument("--replace", action="store_true", help="archive the previous run and build this revision; prior windows are not signalled")
    parser.add_argument("--no-build", action="store_true", help="skip cmake --build; useful only with an already-built executable")
    args = parser.parse_args()

    if args.width <= 0 or args.height <= 0 or max(args.width, args.height) > 4096:
        raise PreviewError("width and height must be positive and no larger than 4096")
    if args.startup_timeout <= 0 or args.startup_timeout > 60:
        raise PreviewError("startup timeout must be greater than 0 and no more than 60 seconds")
    root = project_root(args.project_root)
    if not (root / "CMakeLists.txt").is_file():
        raise PreviewError(f"CMakeLists.txt not found in project root: {root}")
    main_file, config_file = find_files(root, args.main_file, args.config_file)
    build_dir = inside(root, root / args.build_dir)
    executable = executable_path(root, args.executable) if args.no_build else inside(root, root / (args.executable or "bin/main"))
    plan = {
        "project_root": str(root),
        "main_file": relative(root, main_file),
        "config_file": relative(root, config_file),
        "build_dir": relative(root, build_dir),
        "executable": relative(root, executable),
        "display": {"width": args.width, "height": args.height, "rotation": args.rotation, "color_depth": args.color_depth},
        "build_command": ["cmake", "--build", str(build_dir)],
    }
    if args.dry_run:
        print(json.dumps(plan, indent=2, sort_keys=True))
        return 0

    directory = state_dir(root)
    old_state_path = directory / "state.json"
    if old_state_path.is_file():
        try:
            old_state = json.loads(old_state_path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as exc:
            raise PreviewError(f"invalid existing preview state: {old_state_path}") from exc
        pid = old_state.get("process_id")
        if isinstance(pid, int):
            try:
                os.kill(pid, 0)
            except ProcessLookupError:
                pass
            except PermissionError as exc:
                raise PreviewError(f"existing preview process {pid} cannot be inspected: {exc}") from exc
            else:
                if not args.replace:
                    raise PreviewError(f"existing preview process {pid} is running; use --replace to preview the authorized revision")
                archive_recorded(root, old_state)

    run_id = dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%SZ") + f"-{os.getpid()}"
    touched = [main_file, config_file]
    backup = backup_files(root, touched, run_id)
    backup_names = [relative(root, path) for path in touched]
    try:
        atomic_write(main_file, patch_main(read_text(main_file), args.width, args.height, args.rotation))
        atomic_write(config_file, patch_config(read_text(config_file), args.color_depth))
        if not args.no_build:
            build = subprocess.run(["cmake", "--build", str(build_dir)], cwd=root, text=True, capture_output=True, timeout=300, check=False)
            if build.returncode:
                log = directory / "logs" / f"{run_id}.log"
                log.write_text(build.stdout + build.stderr, encoding="utf-8")
                raise PreviewError(f"build failed; files restored. Read {relative(root, log)}")
        executable = executable_path(root, args.executable)
        plan["executable"] = relative(root, executable)
        log = directory / "logs" / f"{run_id}.log"
        stream = log.open("w", encoding="utf-8")
        process = subprocess.Popen([str(executable)], cwd=root, stdout=stream, stderr=subprocess.STDOUT, start_new_session=True)
        stream.close()  # The child owns its duplicated log descriptor.
        time.sleep(args.startup_timeout)
        if process.poll() is not None:
            stream.close()
            raise PreviewError(f"preview exited before {args.startup_timeout:g}s; read {relative(root, log)}")
        state = {"run_id": run_id, "project_root": str(root), "backup_dir": relative(root, backup), "touched_files": backup_names, "log": relative(root, log), "process_id": process.pid, "runtime_gate": True, "build_gate": "SKIPPED" if args.no_build else "PASS", "evaluation": "NOT_RUN", "completion_status": "ready_for_evaluation", "source_hashes": {relative(root, p): hashlib.sha256(p.read_bytes()).hexdigest() for p in touched}, "executable_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(), "hardware_verified": False, **plan}
        json_write(old_state_path, state)
        append_ledger(directory, {"id": run_id, "evaluation": "NOT_RUN", "reason": "runtime live; evaluate rendered frames before completion", "rollback": "available from recorded backup", **plan})
        (directory / "preview-evaluation.md").write_text("# Preview evaluation\n\nRun evaluate_render.py with native captures and evidence. No mandatory human approval.\n\n" + f"- Display: {args.width}x{args.height}\n- Rotation: {args.rotation}°\n- Color depth: {args.color_depth}\n- Hardware behavior: NOT VERIFIED by this PC preview.\n", encoding="utf-8")
        legacy = directory / "preview-decision.md"
        if legacy.is_file():
            archive = directory / "runs"
            archive.mkdir(exist_ok=True)
            legacy.replace(archive / (run_id + "-legacy-preview-decision.md"))
        print(json.dumps(state, indent=2, sort_keys=True))
        return 0
    except (OSError, subprocess.SubprocessError, PreviewError) as exc:
        if "process" in locals() and process.poll() is None:
            process.terminate()  # Only this invocation's unreaped Popen child, never a persisted PID.
            process.wait(timeout=5)
        restore_files(root, backup, backup_names)
        append_ledger(directory, {"id": run_id, "decision": "rejected", "reason": str(exc), "rollback": "files restored", **plan})
        if isinstance(exc, PreviewError):
            raise
        raise PreviewError(str(exc)) from exc


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except PreviewError as error:
        print(f"FAIL {error}", file=sys.stderr)
        raise SystemExit(1)
