"""Host images: input-only visual differences must pass, autoplay must fail."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from check_story_causality import inspect_causality


@unittest.skipUnless(shutil.which("convert") and shutil.which("identify"), "ImageMagick")
class StoryCausalityTests(unittest.TestCase):
    def test_input_and_zero_input_controls(self):
        with tempfile.TemporaryDirectory() as d:
            folder=Path(d)
            def png(name, color, sprite=False):
                path=folder / (name+".png")
                cmd=["convert", "-size", "1280x720", "xc:black",
                     "-fill",color,"-draw","rectangle 93,20 876,607"]
                if sprite:
                    cmd+=["-fill","red","-draw","rectangle 310,215 350,255"]
                subprocess.run(cmd+[str(path)],check=True,stdout=subprocess.DEVNULL)
                return path
            open_a=png("open-scripted","beige")
            scene1=png("story-day","skyblue")
            scene2=png("story-night","#19253f")
            open_b=png("open-idle","beige")
            idle1=png("idle-middle","beige",True)
            idle2=png("idle-later","beige",True)
            good=inspect_causality([open_a,scene1,scene2],[open_b,idle1,idle2])
            self.assertEqual(good["status"],"scripted_input_changes_scenes")
            self.assertGreater(good["aligned_frame_difference"]["after_first_input"],0.35)
            self.assertGreater(good["aligned_frame_difference"]["later_story"],0.35)
            scene1_clone=png("story-day-clone","skyblue")
            scene2_clone=png("story-night-clone","#19253f")
            autoplay=inspect_causality([open_a,scene1,scene2],
                                      [open_b,scene1_clone,scene2_clone])
            self.assertEqual(autoplay["status"],"input_causality_not_proven")
            with self.assertRaisesRegex(ValueError,"distinct"):
                inspect_causality([open_a,scene1,scene2],[open_a,idle1,idle2])
            with self.assertRaisesRegex(ValueError,"threshold"):
                inspect_causality([open_a,scene1,scene2],[open_b,idle1,idle2],threshold=0)


if __name__ == "__main__":
    unittest.main()
