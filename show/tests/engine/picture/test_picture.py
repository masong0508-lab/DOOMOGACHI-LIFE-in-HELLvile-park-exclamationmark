"""test_picture.py - ENGINE lane 2 (WP5 strokes/tilemap/parallax/scale/matte,
WP7 text, WP8 camera and effects): the acceptance tests AT5.1-AT5.3,
AT7.1-AT7.2, AT8.1-AT8.3 and the record formats.

    python3 -m pytest show/tests/engine/picture -m "not slow"   (host driver, seconds)
    python3 -m pytest show/tests/engine/picture                 (+ the PS1 build and emulator)

The host driver (pic_host.c) runs the same lane-2 C code as show.exe on a
software raster; it proves the numbers. The PS1 run proves the pixels.
"""
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import pictest                                                 # noqa: E402
import shots                                                   # noqa: E402
from showtk.picture import camera, fmt, fx, lints, pxt, strokes, text, tilemap   # noqa: E402

try:
    import pytest
    slow = pytest.mark.slow
except ImportError:                                            # pragma: no cover
    def slow(f):
        return f


# ---- formats ------------------------------------------------------------------
def test_record_sizes_match_the_c_headers(tmp_path):
    src = tmp_path / "sizes.c"
    names = sorted(fmt.SIZES)
    src.write_text('#include <stdio.h>\n#include "show/show_cam.h"\n#include "show/show_fx.h"\n'
                   '#include "show/show_strokes.h"\n#include "show/show_tilemap.h"\n#include "show/show_text.h"\n'
                   "int main(void){\n" + "".join('printf("%s %%u\\n",(unsigned)sizeof(%s));\n' % (n, n) for n in names)
                   + "return 0;}\n")
    exe = tmp_path / "sizes"
    subprocess.run(["gcc", "-std=gnu99", "-I", str(pictest.SHOW / "include"), "-I",
                    str(pictest.REPO / "core" / "include"), str(src), "-o", str(exe)], check=True)
    out = subprocess.run([str(exe)], stdout=subprocess.PIPE, text=True, check=True).stdout
    got = {k: int(v) for k, v in (l.split() for l in out.splitlines())}
    assert got == fmt.SIZES


def test_child_hand_font_c_is_current():
    assert pxt.main(["--check"]) == 0
    f = pxt.child_font()
    for ch in "Every Midsummer night, the whole park had one party. It was called the Mingle.":
        assert ch == " " or ord(ch) in f.glyphs, ch


# ---- AT5.1: drift exact over 10,000 frames ---------------------------------------
def test_at51_parallax_drift_has_no_accumulated_error():
    n = 10000
    b, info = shots.drift_shot(frames=n)
    rows, _ = pictest.run_host(b, "at51", draw=False)
    assert len(rows) == n
    for r in rows:
        t = r["t"]
        cam_x = 3000 * (t - 90)                     # doc:1.0 = 5/6 px/f = 3000 cam units
        assert int(r["cam"].split(",")[0]) == cam_x
        for i, (pn, pd) in enumerate(info["pars"]):
            ox = int(r["p%d" % i].split(",")[0])
            assert ox == (info["at"] * 3600 - (cam_x * pn) // pd) // 3600     # exact, every frame
    # the rates: exactly 1/24, 1/12 and 1/6 px per frame (0.0417 / 0.0833 / 0.1667)
    ox = [[int(r["p%d" % i].split(",")[0]) for r in rows] for i in range(3)]
    for i, per in enumerate((24, 12, 6)):
        for t in range(0, n - per, 97):
            assert ox[i][t] - ox[i][t + per] == 1
    (pictest.OUT / "parallax").mkdir(parents=True, exist_ok=True)
    (pictest.OUT / "parallax" / "AT5.1.txt").write_text(
        "AT5.1 PASS: 10,000 frames, planes at 1/24, 1/12, 1/6 px/f, every frame equals "
        "at - floor(cam * par) exactly; frame 9999: %s\n" % rows[-1])


def test_python_camera_equals_the_players():
    b, info = shots.camera_moves_shot()
    rows, _ = pictest.run_host(b, "cammoves", draw=False)
    cam = info["cam"]
    for r in rows:
        x, y, s = cam.at(r["t"])
        assert r["cam"] == "%d,%d,%d" % (x, y, s), (r["t"], r["cam"], (x, y, s))


# ---- AT5.2: the reveal reaches 100 % on its scripted frame -----------------------------
def test_at52_reveal_completes_on_the_scripted_frame():
    b, info = shots.calendar_shot()
    rows, _ = pictest.run_host(b, "at52", draw=False)
    by = {r["t"]: r for r in rows}

    def rv(t, s):
        d, n = by[t]["s%d" % s].split("/")
        return int(d), int(n)

    d0, n0 = rv(0, 0)
    assert 0 < d0 < n0                               # the circle starts on +0
    assert rv(23, 0)[0] < rv(23, 0)[1]
    assert rv(24, 0)[0] == rv(24, 0)[1]              # 100 % on +24
    assert rv(29, 1)[0] == 0 and rv(30, 1)[0] > 0    # the scribble starts on +30
    assert rv(59, 1)[0] < rv(59, 1)[1] and rv(60, 1)[0] == rv(60, 1)[1]
    # the foley's curve (Python, no boil) agrees on the frames
    c = strokes.reveal_curve(info["strokes"][0], 2, 0, 24, 30)
    assert c[24] == c[29] and c[23] < c[24] and c[0] > 0


# ---- AT5.3: 2D frames are 100 % on-palette ------------------------------------------
def test_at53_2d_frames_are_on_palette():
    import numpy as np
    from showtk.picture import bake
    pal = bake._palette_rgb()
    allowed = {(pal[3 * i], pal[3 * i + 1], pal[3 * i + 2]) for i in range(256)}
    total = 0
    for name, (b, _) in (("cal", shots.calendar_shot()), ("txt", shots.super_shot()),
                         ("tm", shots.tilemap_shot(frames=90)), ("split", shots.split_shot())):
        frames = [30, 45, 60, 89] if name != "split" else [5, 15, 40, 100]
        _, d = pictest.run_host(b, "at53_" + name, frames=frames)
        for f in frames:
            if name == "cal" and f < 8:
                continue                             # the fade-in blends (a camfx class, spec 2.3)
            a = pictest.read_ppm(d / ("f%04d.ppm" % f))[30:210]
            cols = {tuple(c) for c in np.unique(a.reshape(-1, 3), axis=0)}
            assert cols <= allowed, (name, f, sorted(cols - allowed)[:5])
            total += a.shape[0] * a.shape[1]
    assert total > 0


# ---- AT7.1 / AT7.2 --------------------------------------------------------------------
def test_at71_co02_super_lands_its_52_glyphs_on_frames_120_to_222():
    b, info = shots.super_shot()
    rows, _ = pictest.run_host(b, "at71", draw=False)
    shot_start = 90                                   # co02 starts at episode frame 90
    first = {}
    for r in rows:
        k, n = map(int, r["g0"].split("/"))
        assert n == 52
        for g in range(k):
            first.setdefault(g, r["t"])
    assert len(first) == 52
    assert first[0] + shot_start == 120 and first[51] + shot_start == 222
    assert [first[g] for g in range(52)] == [30 + 2 * g for g in range(52)]
    assert text.glyph_frames(info["items"][0]) == [30 + 2 * g for g in range(52)]


def test_at72_time_read_fires_on_co03():
    items = [text.super_("It was called the Mingle.", "type", 13, 120, at="lower", until=63,
                         font="child_hand", sid="s_co03")]
    d = lints.time_read(items, 120, shot="co03")
    assert len(d) == 1 and d[0].rule == "TIME-READ" and d[0].severity == "warning"
    assert d[0].head() == "co03 +13 [TIME-READ]"
    for part in ("on screen 50 f (+13..+63)", "5 words need 60 f", "end it at +73"):
        assert part in d[0].msg, d[0].msg
    ok = [text.super_("It was called the Mingle.", "type", 13, 120, until=73, font="child_hand")]
    assert lints.time_read(ok, 120) == []


# ---- AT8.1: the co02 iris -------------------------------------------------------------
def test_at81_co02_iris_opens_over_90_to_103_on_the_bandstand():
    b, info = shots.iris_shot()
    kind, (op, slot, bval) = info["ev"]
    assert kind == "beat" and op == fx.BOP["IRIS_IN"] and bval == 5          # hold 5, r 0
    rows, d = pictest.run_host(b, "at81", frames=list(range(0, 15)))
    radius = [int(r["iris"]) for r in rows[:16]]
    assert all(rr >= 0 for rr in radius[:13]) and radius[13] == -1           # open by +13 (f 103)
    assert radius[12] > radius[8] > radius[7]
    # centred on the bandstand: the open circle (first frame it is inside the
    # screen) is symmetric about x = 160, y = 92 + 30
    f = next(t for t in range(8, 13) if 20 < radius[t] < 90)
    a = pictest.read_ppm(d / ("f%04d.ppm" % f))
    a[0:4, 0:64] = 0                                                     # the frame stamp
    lit = (a.sum(axis=2) > 0)
    ys, xs = lit.nonzero()
    assert abs((xs.min() + xs.max()) / 2 - 160) <= 1 and abs((ys.min() + ys.max()) / 2 - 122) <= 1


# ---- AT8.2: the flash lint -------------------------------------------------------------
def test_at82_flash_lint_catches_a_4f_full_screen_flash():
    d = lints.time_flash([(10, "WHITE", 16, 4, 7)], shot="co06")
    assert any(x.rule == "TIME-FLASH" and x.is_error for x in d)
    assert not [x for x in lints.time_flash([(10, "WHITE", 8, 3, 7)]) if x.is_error]
    assert any(x.is_error for x in lints.time_flash([(10, "WHITE", 8, 2, 7), (15, "WHITE", 8, 2, 8)]))


# ---- AT8.3: a shake never shows an uncovered edge -------------------------------------
def test_at83_shake_never_shows_an_uncovered_edge():
    import numpy as np
    for profile in ("SHK_RUMBLE", "SHK_SHOTGUN", "SHK_IMPACT"):
        b, info = shots.shake_shot(profile=profile)
        rows, d = pictest.run_host(b, "at83_" + profile, frames="all")
        moved = 0
        for r in rows:
            a = pictest.read_ppm(d / ("f%04d.ppm" % r["t"]))[30:210]
            magenta = (a[:, :, 0] > 200) & (a[:, :, 1] < 80) & (a[:, :, 2] > 200)
            assert not magenta.any(), (profile, r["t"], r["shake"])
            moved += r["shake"] != "0,0"
        assert moved >= 5
    # the lint agrees: an exact-width plane fails, the overscanned one passes
    cam = camera.Camera()
    good = [("near", 336, 196, 1, (0, 1), (-8, -8), True, 3)]
    bad = [("near", 320, 180, 1, (0, 1), (0, 0), True, 3)]
    sp = [fx.SHK.index("SHK_SHOTGUN")]
    assert lints.pic_cover(good, cam, 40, sp) == []
    assert lints.pic_cover(bad, cam, 40, sp)


# ---- TILEMAP ------------------------------------------------------------------------
def test_tilemap_round_trip_and_exact_draw():
    b, info = shots.tilemap_shot()
    payload, meta = tilemap.compile_plane(info["plate"])
    assert (tilemap.unpack(payload) == info["plate"]).all()
    assert meta["tiles"] < meta["cols"] * meta["rows"]              # tiles were shared
    _, d = pictest.run_host(b, "tm", frames=[0])
    a = pictest.read_ppm(d / "f0000.ppm")
    from showtk.picture import bake
    pal = bake._palette_rgb()
    want = info["plate"][0:240, 32:352]                                # at=(-32,-30): screen = plate - (32, 0)
    got_idx = None
    rgb = [(pal[3 * i], pal[3 * i + 1], pal[3 * i + 2]) for i in range(256)]
    exp = [[rgb[v] for v in row] for row in want[:, :320]]
    import numpy as np
    assert (a[4:] == np.array(exp, np.uint8)[4:]).all()          # rows 0-3: the frame stamp
    assert got_idx is None


def test_tilemap_scale_move_opens_no_seams():
    import numpy as np
    b, info = shots.tilemap_shot(scale_to="1.00", frames=30)
    rows, d = pictest.run_host(b, "tm_scale", frames=[0, 5, 10, 29])
    for f in (0, 5, 10, 29):
        a = pictest.read_ppm(d / ("f%04d.ppm" % f))[30:210]
        magenta = (a[:, :, 0] > 200) & (a[:, :, 1] < 80) & (a[:, :, 2] > 200)
        assert not magenta.any(), f
    assert rows[0]["cam"].split(",")[2] != rows[29]["cam"].split(",")[2]


# ---- the Split ------------------------------------------------------------------------
def test_split_runs_five_courses_of_two_frames_holds_and_closes():
    b, _ = shots.split_shot()
    rows, _ = pictest.run_host(b, "split", draw=False)
    by = {r["t"]: r.get("split") for r in rows}
    assert by[9] is None
    heights = [int(by[t].split(",")[3]) for t in range(10, 20)]
    assert heights == [36, 36, 72, 72, 108, 108, 144, 144, 240, 240]
    assert all(by[t] == "160,-30,160,240" for t in range(18, 80))
    outs = [int(by[t].split(",")[3]) for t in range(80, 90)]
    assert outs == [240, 240, 144, 144, 108, 108, 72, 72, 36, 36] and by[90] is None


def test_lints_wipe_and_split_rules():
    assert lints.time_wipe([(0, "stack", 5, 2, 60, 160, 1)]) == []
    assert lints.time_wipe([(0, "stack", 4, 2, 60, 160, 1)])
    assert lints.time_wipe([(0, "stack", 5, 2, 30, 160, 1)])
    assert lints.time_wipe([(0, "stack", 5, 2, 60, 150, 1)])
