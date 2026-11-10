"""contracts/stc_ui.h 與 ui/lvgl 實作必須同名同序（#53 I0）。"""

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

CONTRACT = ROOT / "contracts/stc_ui.h"
IMPLEMENTATION = ROOT / "ui/lvgl/include/stc_ui.h"


def enum_names(text: str, typename: str) -> list[str]:
    match = re.search(r"typedef enum \{([^}]*)\}\s*" + typename, text, re.S)
    if not match:
        raise AssertionError(f"{typename} not found")
    body = match.group(1)
    return [name for name in re.findall(r"\b(STC_\w+)", body) if not name.endswith("_COUNT")]


class ContractUiTest(unittest.TestCase):
    def test_states_match_implementation_order(self):
        contract = enum_names(CONTRACT.read_text(), "stc_ui_state_t")
        impl = enum_names(IMPLEMENTATION.read_text(), "stc_state_t")
        self.assertEqual([name.removeprefix("STC_UI_") for name in contract],
                         [name.removeprefix("STC_") for name in impl])

    def test_actions_match_implementation_order(self):
        contract = enum_names(CONTRACT.read_text(), "stc_ui_action_t")
        impl = enum_names(IMPLEMENTATION.read_text(), "stc_action_t")
        self.assertEqual([name.removeprefix("STC_UI_ACTION_") for name in contract],
                         [name.removeprefix("STC_ACTION_") for name in impl])

    def test_contract_declares_the_create_apply_api(self):
        text = CONTRACT.read_text()
        for symbol in ("stc_ui_create", "stc_ui_apply", "stc_ui_set_action_callback",
                       "stc_ui_show_learning", "stc_ui_check_layout"):
            self.assertIn(symbol, text)


if __name__ == "__main__":
    unittest.main()
