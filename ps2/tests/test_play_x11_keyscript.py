"""Play! X11 key delivery arguments: no emulator or keyboard changes in tests."""
import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from play_biosfree_smoke import parse_x11_key_events


class PlayX11KeyScriptTests(unittest.TestCase):
    def test_default_play_qt_start_and_cross(self):
        self.assertEqual(
            parse_x11_key_events(["Return@6", "z@8", "z@10"], 22),
            [(6, "Return"), (8, "z"), (10, "z")],
        )
        self.assertEqual(parse_x11_key_events([], 22), [])

    def test_reject_malformed_or_unsafe_keys(self):
        invalid = (
            ["Return"], ["start@6"], ["Shift+Return@6"],
            ["Return@-2"], ["Return@0"], ["Return@22"],
            ["Return@23"], ["Return@8", "z@6"],
            ["Return@8", "z@8"], ["z@6", "Return@a"],
        )
        for entries in invalid:
            with self.subTest(entries=entries), self.assertRaises(ValueError):
                parse_x11_key_events(entries, 22)


if __name__ == "__main__":
    unittest.main()
