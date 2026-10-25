"""make_sheets.py - the lane-2 contact sheets (host raster, same C as show.exe):
    PYTHONPATH=show/tools python3 show/tests/engine/picture/make_sheets.py
writes show_out/engine/<feature>/sheet.png for every lane-2 feature."""
import sys
from pathlib import Path
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import pictest   # noqa: E402
import shots     # noqa: E402

JOBS = [
    ("strokes_reveal", shots.calendar_shot, [0, 10, 24, 30, 40, 50, 60, 89]),
    ("super_text", shots.super_shot, [20, 30, 40, 60, 80, 100, 120, 140]),
    ("iris_circle", lambda: shots.iris_shot(False), [0, 2, 5, 8, 10, 12, 14, 20]),
    ("iris_heart", lambda: shots.iris_shot(True), [30, 34, 38, 42, 46, 50, 54, 59]),
    ("shake", shots.shake_shot, [0, 1, 2, 3, 4, 5, 6, 8]),
    ("tilemap", lambda: shots.tilemap_shot(scale_to="1.00", frames=30), [0, 5, 10, 15, 20, 25, 29, 29]),
    ("split_wipe", shots.split_shot, [5, 10, 12, 14, 16, 20, 40, 100]),
    ("camera_moves", shots.camera_moves_shot, [0, 30, 60, 90, 120, 142, 170, 199]),
]


def main():
    for name, fn, frames in JOBS:
        b, _ = fn()
        fr = sorted(set(frames))
        _, d = pictest.run_host(b, "sheet_" + name, frames=fr)
        ims = [pictest.read_ppm(d / ("f%04d.ppm" % f)) for f in fr]
        out = pictest.OUT / name / "sheet.png"
        pictest.sheet(ims, ["%s f%d" % (name, f) for f in fr], out)
        print(out)


if __name__ == "__main__":
    main()
