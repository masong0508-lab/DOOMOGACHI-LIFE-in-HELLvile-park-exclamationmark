"""Host images: input-only visual differences must pass, autoplay must fail."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from check_story_causality import check_controller_reports, inspect_causality


class ControllerReportsTests(unittest.TestCase):
    def test_same_iso_with_an_idle_control(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            pairs = []
            reports = []
            for side in ("active", "idle"):
                folder = root / side
                folder.mkdir()
                frames = [folder / f"boot-{i}.png" for i in range(1, 4)]
                pairs.append(frames)
                reports.append(dict(
                    boot_media_type="disc", boot_image_sha256="a" * 64,
                    capture_offsets_seconds=[3, 12, 22],
                    x11_key_events=[],
                    pngs=[frame.name for frame in frames],
                ))
            reports[0]["x11_key_events"] = [
                {"seconds": 6, "key": "Return"},
                {"seconds": 8, "key": "z"},
                {"seconds": 10, "key": "z"},
            ]
            for frames, report in zip(pairs, reports):
                (frames[0].parent / "report.json").write_text(json.dumps(report))
            self.assertEqual(check_controller_reports(*pairs), "a" * 64)

            for report in reports:
                report["capture_offsets_seconds"] = [9, 24, 34]
            reports[0]["x11_key_events"] = [
                {"seconds": seconds, "key": key} for seconds, key in
                ((12, "Return"), (14, "z"), (16, "z"), (18, "z"), (20, "z"))
            ]
            for frames, report in zip(pairs, reports):
                (frames[0].parent / "report.json").write_text(json.dumps(report))
            self.assertEqual(check_controller_reports(*pairs), "a" * 64)

            idle_report = pairs[1][0].parent / "report.json"
            idle_report.write_text(json.dumps(dict(reports[1], boot_image_sha256="b" * 64)))
            with self.assertRaisesRegex(ValueError, "same ISO"):
                check_controller_reports(*pairs)
            idle_report.write_text(json.dumps(dict(reports[1], x11_key_events=reports[0]["x11_key_events"])))
            with self.assertRaisesRegex(ValueError, "no keys"):
                check_controller_reports(*pairs)


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

            # The controller can reach Story at both snapshots while the
            # cutscene still uses the same background.
            captures = []
            for side, images in (("keys", [open_a, scene1, scene1_clone]),
                                 ("idle", [open_b, idle1, idle2])):
                directory = folder / side
                directory.mkdir()
                frames = [directory / f"boot-{i}.png" for i in range(1, 4)]
                for frame, image in zip(frames, images):
                    shutil.copyfile(image, frame)
                keys = [{"key": "Return", "seconds": 6}, {"key": "z", "seconds": 8},
                        {"key": "z", "seconds": 10}] if side == "keys" else []
                (directory / "report.json").write_text(json.dumps(dict(
                    boot_media_type="disc", boot_image_sha256="a" * 64,
                    capture_offsets_seconds=[3, 12, 22], x11_key_events=keys,
                    pngs=[frame.name for frame in frames],
                )))
                captures.append(frames)
            self.assertEqual(inspect_causality(*captures, controller=True)["status"],
                             "controller_input_changes_scenes")
            shutil.copyfile(scene1, captures[1][1])
            shutil.copyfile(scene1_clone, captures[1][2])
            self.assertEqual(inspect_causality(*captures, controller=True)["status"],
                             "input_causality_not_proven")


if __name__ == "__main__":
    unittest.main()
