"""Tests for tools/check_mode_gate.py.

The point of the checker is to catch a doc/contract disagreement before CI does,
so the tests build tiny fixtures rather than relying on the real files.
"""

from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import check_mode_gate


HEADER_OK = """/* stc_mode.h — 模式閘（contract v1.1.0）
 */
typedef enum {
    STC_IN_CHAT_TIMEOUT,/* CHAT 閒置 60 s */
} stc_input_t;
void stc_mode_tick(uint32_t now_ms);   /* CHAT 60 s 閒置自動回 GAME */
"""

DOC_OK = """# 模式閘
狀態：**v1.1.0，待實作**（2026-11-08）。契約：`contracts/stc_mode.h`（v1.1.0）。

## 3. 轉移表
| CHAT | `IN_CHAT_TIMEOUT`（60 s） | GAME_IDLE | 同上 |

## 6. 60 秒自動退出
- `stc_mode_tick()` 內：`now - t_last_activity > 60000` → `IN_CHAT_TIMEOUT`

| M4 | 真人由圖鑑「問綠豆」進 CHAT、60 s 自動退出 | 需要板 |
| G4 | 自動退出 | CHAT 閒置 60 s ± 1 s 回 GAME_IDLE |
"""


def build(root: Path, header: str, doc: str) -> Path:
    (root / "contracts").mkdir(parents=True, exist_ok=True)
    (root / "docs" / "planning").mkdir(parents=True, exist_ok=True)
    (root / "contracts/stc_mode.h").write_text(header, encoding="utf-8")
    (root / "docs/planning/mode-gate.md").write_text(doc, encoding="utf-8")
    return root


class CheckModeGateTest(unittest.TestCase):
    def test_matching_files_pass(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = build(Path(tmp), HEADER_OK, DOC_OK)
            self.assertEqual(check_mode_gate.check(root), [])

    def test_document_timeout_disagrees(self):
        with tempfile.TemporaryDirectory() as tmp:
            doc = DOC_OK.replace("（60 s）", "（90 s）")
            root = build(Path(tmp), HEADER_OK, doc)
            problems = check_mode_gate.check(root)
            self.assertTrue(any("states 90 s" in p for p in problems), problems)

    def test_milliseconds_disagree(self):
        with tempfile.TemporaryDirectory() as tmp:
            doc = DOC_OK.replace("> 60000`", "> 90000`")
            root = build(Path(tmp), HEADER_OK, doc)
            problems = check_mode_gate.check(root)
            self.assertTrue(any("ms" in p for p in problems), problems)

    def test_version_mismatch(self):
        with tempfile.TemporaryDirectory() as tmp:
            doc = DOC_OK.replace("v1.1.0，待實作", "v1.0.0，待實作")
            root = build(Path(tmp), HEADER_OK, doc)
            problems = check_mode_gate.check(root)
            self.assertTrue(any("version mismatch" in p for p in problems), problems)

    def test_missing_timeout_in_document_fails_loudly(self):
        with tempfile.TemporaryDirectory() as tmp:
            doc = "# 模式閘\n狀態：**v1.1.0，待實作**\n"
            root = build(Path(tmp), HEADER_OK, doc)
            problems = check_mode_gate.check(root)
            self.assertTrue(any("vacuous" in p or "no idle-timeout" in p for p in problems), problems)


if __name__ == "__main__":
    unittest.main()
