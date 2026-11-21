"""Ensure black emulator frames never count as gameplay-rendered CI evidence."""
import sys
from pathlib import Path
import subprocess
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from check_play_frames import inspect


class PlayFrameGateTests(unittest.TestCase):
    def test_black_and_visible_viewports(self):
        with tempfile.TemporaryDirectory() as temp:
            d = Path(temp)
            black = d / "black.png"
            visible = d / "visible.png"
            subprocess.run(["convert", "-size", "1280x720", "xc:black",
                            str(black)], check=True)
            subprocess.run([
                "convert", "-size", "1280x720", "xc:black",
                "-fill", "gold", "-draw", "rectangle 180,120 600,450",
                str(visible)
            ], check=True)
            self.assertEqual(inspect([black, black])["status"], "black_viewport")
            result = inspect([visible, visible])
            self.assertEqual(result["status"], "rendered_pixels_observed")
            self.assertGreater(result["frames"][0]["visible_fraction"], 0.15)
            self.assertEqual(inspect([black, visible])["status"], "black_viewport")

    def test_reject_unexpected_dimensions(self):
        with tempfile.TemporaryDirectory() as temp:
            image = Path(temp) / "tiny.png"
            subprocess.run(["convert", "-size", "100x100", "xc:white",
                            str(image)], check=True)
            with self.assertRaises(ValueError):
                inspect([image, image])


if __name__ == "__main__":
    unittest.main()
