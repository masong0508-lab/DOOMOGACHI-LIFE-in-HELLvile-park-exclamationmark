"""AT10.2: the theme's bar map gives 48 f per bar (150 bpm = 12 f per beat).

The bar map is imported from the master's own MIDI (tempo, metre and the
section markers the kit writes into *_full.mid), and checked against the
audio: the CRACK (bar 1, beat 1) is the master's sample 0, and the master's
onsets sit on the bar map's 16th grid.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools"))

from showtk.music import audiopaths as P  # noqa: E402
from showtk.music import barmap as BM  # noqa: E402
from showtk.music import cuelib, qa  # noqa: E402

MT_MID = "midi/main_title/tracks/doomtastic_times_main_title_full.mid"


def test_main_title_barmap_48f():
    bm = BM.import_midi(P.theme_dir() / MT_MID)
    assert bm.bpm == 150.0 and bm.meter == 4
    assert bm.frames_per_bar == 48.0
    assert [bm.frame(b) for b in range(1, 6)] == [0, 48, 96, 144, 192]
    assert bm.frame(17, 2) == 16 * 48 + 12            # chorus, beat 2
    assert bm.smp(2) == 76800
    secs = {name: b for name, b, n in bm.sections}
    assert secs == {"cold open": 1, "intro": 2, "verse": 10, "wall window": 14,
                    "pre-chorus": 15, "chorus": 17, "post-chorus": 25, "button": 29}, secs
    print(f"AT10.2 main title: {bm.bpm} bpm, {bm.frames_per_bar} f per bar, {bm.bars} bars,"
          f" sections {secs}")


def test_main_title_audio_on_the_map():
    meta = cuelib.load_meta("theme_main_title")
    bm = BM.BarMap.from_json(meta["barmap"])
    assert bm.frames_per_bar == 48.0
    y = cuelib.load_audio("theme_main_title")
    on = qa.first_onset(y)
    assert on is not None and on < 48, on                # the CRACK = bar 1, beat 1 = sample 0
    ons = qa.onsets(y)
    g = qa.grid_check(ons, bm, grid=0.25)
    print(f"AT10.2 main title audio: first onset smp {on}; {g['n']} onsets, median "
          f"{g['median_ms']} ms from the 16th grid, {g['within_10ms']} % within 10 ms")
    assert g["median_ms"] <= 10.0 and g["within_10ms"] >= 70.0


def test_eyecatch_barmap():
    meta = cuelib.load_meta("theme_eyecatch_out")
    bm = BM.BarMap.from_json(meta["barmap"])
    assert bm.frames_per_bar == 48.0 and bm.bars == 3


if __name__ == "__main__":
    test_main_title_barmap_48f()
    test_main_title_audio_on_the_map()
    test_eyecatch_barmap()
    print("AT10.2 PASS")
