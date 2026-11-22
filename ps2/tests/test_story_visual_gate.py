"""Host regression test: animated title must not pass Story transition gate."""
import sys
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from check_story_progress import inspect_story


@unittest.skipUnless(shutil.which("convert") and shutil.which("identify"),
                     "ImageMagick required")
class StoryProgressVisualGateTests(unittest.TestCase):
    def test_story_transition_vs_idle_title(self):
        with tempfile.TemporaryDirectory() as d:
            folder = Path(d)
            title, idle, scene1, scene2 = (folder / f"{s}.png" for s in
                                          ("title", "idle", "scene1", "scene2"))

            def render(path, color, small_sprite=False):
                cmd = ["convert", "-size", "1280x720", "xc:black",
                       "-fill", color, "-draw", "rectangle 93,20 876,607"]
                if small_sprite:
                    cmd += ["-fill", "pink", "-draw", "rectangle 300,210 335,255"]
                subprocess.run(cmd + [str(path)], check=True)

            render(title, "beige")
            render(idle, "beige", True)
            render(scene1, "skyblue")
            render(scene2, "#182542")
            good = inspect_story([title, scene1, scene2])
            self.assertEqual(good["status"], "large_scene_transitions_observed")
            self.assertGreater(good["changed_fraction"]["opening_to_middle"], 0.35)
            self.assertGreater(good["changed_fraction"]["middle_to_later"], 0.35)
            still = inspect_story([title, idle, scene1])
            self.assertEqual(still["status"], "scene_transition_not_proven")
            self.assertLess(still["changed_fraction"]["opening_to_middle"], 0.02)

    def test_invalid_frames(self):
        with tempfile.TemporaryDirectory() as d:
            first = Path(d) / "frame.png"
            subprocess.run(["convert", "-size", "100x100", "xc:gold", str(first)], check=True)
            with self.assertRaisesRegex(ValueError, "distinct"):
                inspect_story([first, first, first])
            with self.assertRaisesRegex(ValueError, "Exactly three"):
                inspect_story([first, first])
            with self.assertRaisesRegex(ValueError, "threshold"):
                inspect_story([first, first, first], min_changed_fraction=0)
            paths = [Path(d) / f"frame-{i}.png" for i in range(3)]
            for path in paths:
                subprocess.run(["convert", "-size", "100x100", "xc:gold", str(path)], check=True)
            with self.assertRaises(ValueError):
                inspect_story(paths)


if __name__ == "__main__":
    unittest.main()
