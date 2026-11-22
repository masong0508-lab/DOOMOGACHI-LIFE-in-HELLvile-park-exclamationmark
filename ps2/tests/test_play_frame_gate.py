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
            still_black = inspect([black, black])
            self.assertEqual(still_black["status"], "black_viewport")
            self.assertFalse(still_black["viewport_changed"])
            result = inspect([visible, visible])
            self.assertEqual(result["status"], "rendered_pixels_observed")
            self.assertFalse(result["viewport_changed"])
            self.assertGreater(result["frames"][0]["visible_fraction"], 0.15)
            self.assertEqual(inspect([black, visible])["status"], "black_viewport")
            # Animating a shape entirely inside the video ROI must change
            # the fingerprint, unlike an emulator FPS/status-bar update.
            second = d / "next.png"
            subprocess.run([
                "convert", "-size", "1280x720", "xc:black",
                "-fill", "gold", "-draw", "rectangle 270,150 750,500",
                str(second)
            ], check=True)
            motion = inspect([visible, second])
            self.assertEqual(motion["status"], "rendered_pixels_observed")
            self.assertTrue(motion["viewport_changed"])
            self.assertNotEqual(motion["frames"][0]["game_pixels_sha"],
                                motion["frames"][1]["game_pixels_sha"])

    def test_reject_unexpected_dimensions(self):
        with tempfile.TemporaryDirectory() as temp:
            image = Path(temp) / "tiny.png"
            subprocess.run(["convert", "-size", "100x100", "xc:white",
                            str(image)], check=True)
            with self.assertRaises(ValueError):
                inspect([image, image])


if __name__ == "__main__":
    unittest.main()
