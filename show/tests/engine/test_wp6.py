"""test_wp6.py - WP6 acceptance tests (spec 16): AT6.1-AT6.5 and the dialogue proof picture.

    python3 -m pytest show/tests/engine/test_wp6.py -m "not slow"   (seconds: the records)
    python3 -m pytest show/tests/engine/test_wp6.py                 (+ one test build of show.exe
                                                                      and one emulator run, ~4 min)
    python3 show/tests/engine/test_wp6.py                           (the same, without pytest)

The slow tests share ONE build and ONE run of the WP6 test program
(show/tests/engine/wp6/: gen_wp6.py scenes -> wp6_driver.c on the PS1 ->
TTY measurements and captured frames), then write the model sheets and
contact sheets to <scratch>/show_out/engine/<feature>/.
"""
import re
import struct
import sys
import types
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1] / "tools"))
sys.path.insert(0, str(HERE / "wp6"))

from showtk import actors as A          # noqa: E402

try:
    import pytest
    slow = pytest.mark.slow
except ImportError:                     # plain `python3 test_wp6.py`
    pytest = None

    def slow(f):
        return f

HDR = (HERE.parents[1] / "include" / "show" / "show_actor.h").read_text()


# --- fast: the Python records mirror show_actor.h -------------------------------
def _enum(first, last):
    m = re.search(r"enum \{\s*(%s[^}]*%s)[^}]*\}" % (first, last), HDR, re.S)
    assert m, first
    names = [n.strip().split("=")[0].strip() for n in re.sub(r"/\*.*?\*/", "", m.group(1), flags=re.S).split(",")]
    return [n for n in names if n]


def test_record_sizes_match_the_header():
    for struct_name, size in (("ShowActorRec", A.ACTOR_SIZE), ("ShowCrowdRec", A.CROWD_SIZE),
                              ("ShowTrackHdr", A.TRACK_HDR), ("ShowFlipHdr", A.FLIP_HDR)):
        m = re.search(r"DOOMO_STATIC_ASSERT\(sizeof\(%s\) == (\d+)" % struct_name, HDR)
        assert m and int(m.group(1)) == size, struct_name
    assert len(A.pack_actor("figure", 3, 196, 96, face="L")) == 32
    assert len(A.pack_crowd("lane", 0, 6, 6)) == 48
    assert len(A.pack_flip(2, 2, [b"\1\2\3\4"], [3])) == 12 + 4 + 4


def test_enums_match_the_header():
    assert _enum("SHA_BUST", "SHA_FIGHTER") == ["SHA_BUST", "SHA_FIGURE", "SHA_CHIBI", "SHA_FIGHTER"]
    assert [A.KIND[k] for k in ("bust", "figure", "chibi", "fighter")] == [0, 1, 2, 3]
    assert _enum("SHI_ON", "SHI_NONE") == ["SHI_ON", "SHI_STILL", "SHI_FROZEN", "SHI_NONE"]
    assert _enum("VIS_X", "VIS_F") == ["VIS_" + c for c in A.VISEMES]
    shop = _enum("SHOP_EXPR", "SHOP_SHOW")
    assert shop == ["SHOP_" + k.upper() for k, _ in sorted(A.SHOP.items(), key=lambda kv: kv[1])]
    shco = _enum("SHCO_FREEZE", "SHCO_SHOW")
    assert shco == ["SHCO_" + k.upper() for k, _ in sorted(A.SHCO.items(), key=lambda kv: kv[1])]
    assert "#define SHOW_SOP_ACTOR 0x%02x" % A.SHOW_SOP_ACTOR in HDR
    assert "SHOW_SOP_CROWD 0x%02x" % A.SHOW_SOP_CROWD in HDR


def test_actor_record_layout():
    b = A.pack_actor("bust", 7, -12, 150, face="L", expr="EXPR_HAPPY", idle="frozen", phase=39, px=2,
                     status=2, clock=3, flags=1, view=0, wframe=0xFF)
    kind, look, face, expr, x, y, idle, phase, px, status, clock, flags, view, wf = struct.unpack_from("<BBBBhhBBBBBBBB", b)
    assert (kind, look, face, x, y, idle, phase, px, status, clock, flags, wf) == (0, 7, 1, -12, 150, 2, 39, 2, 2, 3, 1, 255)
    assert expr == A.sym("EXPR_HAPPY")
    vis = list(b[16:24])
    assert vis == A.default_visemes() and vis[0] == A.NO_MOUTH and vis[1] == A.sym("MOUTH_FLAT")


def test_crowd_record_layout():
    b = A.pack_crowd("lane", 12, 6, 6, seed=301, x=-32, y=118, h=0, spacing=26, speed=2.2 * 5 / 6, ripple=3,
                     step=4, px=2, special=[(3, 40, A.CF_NOCLOCK)])
    f = struct.unpack("<BBBBHHhhhhhhiBBBBBB2x4H4B4B", b)
    assert f[:8] == (0, 12, 6, 0, 6, 301, -32, 118)
    assert f[10] == 26 * 16 and f[12] == round(2.2 * 5 / 6 * 65536)
    assert f[13:19] == (3, 4, 0xFF, 0, 2, 1)
    assert f[19] == 3 and f[23] == 40 and f[27] == A.CF_NOCLOCK


def test_mouth_track_runs():
    stay = "XXXXXXCCCXXXXAAAAAAAAXXXXXXXXXX"             # spec 9.3 (31 f)
    v = A.mouth_values(stay)
    assert len(v) == 31 and v[6] == 2 and v[13] == 4
    b = A.pack_track(1, "mouth", 40, v)
    actor, chan, start, nruns, frames = struct.unpack_from("<BBHHH", b)
    assert (actor, chan, start, nruns, frames) == (1, 0, 40, 5, 31)
    runs = [struct.unpack_from("<BB", b, 8 + 2 * i) for i in range(nruns)]
    assert runs == [(6, 0), (3, 2), (4, 0), (8, 4), (10, 0)]
    assert len(b) % 4 == 0
    long = A.runs([0] * 600)
    assert long == [(255, 0), (255, 0), (90, 0)]


def test_shot_cast_resolves_the_cast():
    sc = A.ShotCast()
    sc.actor("mayor", "mayor", kind="figure", at=(196, 96), face="L")
    sc.actor("nana", "nana", kind="bust", at=(160, 180), px=2)
    sc.crowd("kids", "kset:storybook_bunnies", 6, form="lane", at=(-32, 118), spacing=26, speed=1.83, ripple=3)
    secs = sc.pack()
    assert len(secs["LOOK"]) == 48 * 8 and len(secs["ACTR"]) == 64 and len(secs["CRWD"]) == 48
    rec = struct.unpack_from("<BBBBhhBBBBBB", secs["ACTR"], 0)
    assert rec[0] == 1 and rec[1] == 0 and rec[7] == 39 and rec[10] == 0        # phase=39 (cast.shw), clock 0
    assert struct.unpack_from("<BBBBhhBBBBBB", secs["ACTR"], 32)[10] == 1      # nana: beat clock 1
    op = sc.op("mayor", "headturn", 64, 2)
    assert op == (A.SHOW_SOP_ACTOR, [0, A.SHOP["headturn"], 64, 0, 2, 0])
    assert sc.crowd_op("kids", "freeze") == (A.SHOW_SOP_CROWD, [0, A.SHCO["freeze"], 0])


# --- slow: the PS1 run ------------------------------------------------------------
_RUN = {}


def ps1():
    """one build + run per session; WP6_REUSE=1 reads the last run instead (sheet tweaks)"""
    if not _RUN:
        import json
        import os
        import wp6run
        if os.environ.get("WP6_REUSE") == "1" and (wp6run.WORK / "tty.log").exists():
            meta = json.loads((wp6run.GEN / "scenes.json").read_text())
            tty, rundir = (wp6run.WORK / "tty.log").read_text(), wp6run.WORK / "run"
        else:
            meta, tty, rundir = wp6run.run_all()
        _RUN.update(meta=meta, tty=tty, dir=rundir, mod=wp6run)
    return _RUN


def _scene(name):
    return next(s for s in ps1()["meta"]["scenes"] if s["name"] == name)


@slow
def test_at61_model_sheets_on_the_ps1():
    """AT6.1: the Mayor's (and Nana's) model sheet renders on the PS1: 11 expressions x 8 visemes"""
    r = ps1()
    w = r["mod"]
    for who in ("mayor", "nana"):
        out = w.OUT / "model_sheets" / ("%s_model_sheet_ps1.png" % who)
        png, nb, nf, nc, nfi = w.model_sheet(r["tty"], r["dir"], who, out)
        assert (nb, nf, nc, nfi) == (11 * 8 + 11, 12, 20, 5), (who, nb, nf, nc, nfi)
    cells = [c for c in w.lines(r["tty"], "cell")]
    assert all(c["crc"] != "00000000" for c in cells), "an empty cell"
    # the visemes change the picture: per expression, the 8 mouths give >= 5 distinct busts
    seq = [l for l in r["tty"].splitlines() if l.startswith("wp6 sheet ") or l.startswith("wp6 cell ")]
    cur, by = None, {}
    for l in seq:
        if l.startswith("wp6 sheet "):
            cur = l.split()[2]
        else:
            c = w.kv(l)
            if c["kind"] == "bust" and int(c["b"]) >= 0:
                by.setdefault((cur, int(c["a"])), set()).add(c["crc"])
    for (who, e), crcs in by.items():
        assert len(crcs) >= 5, (who, e, len(crcs))


@slow
def test_at62_freeze_stops_the_wings_for_13_frames():
    r = ps1()
    rows = [(int(d["t"]), int(d["clk"]), d["uid"]) for d in r["mod"].lines(r["tty"], "at62")]
    clk = {t: c for t, c, _ in rows}
    uid = {t: u for t, _, u in rows}
    stopped = [t for t in range(1, 96) if clk[t] == clk[t - 1]]
    assert stopped == list(range(41, 54)), stopped                   # exactly 13 frames without an advance
    assert len(set(uid[t] for t in range(40, 54))) == 1               # the picture (wings) holds
    # without the freeze the wings would have flapped at clock 80 (t = 41); they flap at t = 54
    assert (clk[40] + 1) >> 3 != clk[40] >> 3 and uid[54] != uid[53]
    w = r["mod"]
    frames = [(r["dir"] / (n + ".png"), "+%03d" % t) for t, n in _scene("at62_freeze")["shots"]]
    w.contact(frames, w.OUT / "freeze" / "at62_freeze13_contact.png", cols=4, scale=2,
              title="AT6.2: freeze mayor at +40, thaw at +53: clock and wings hold 13 f - PS1 frames")


@slow
def test_at63_twelve_kids_cross_with_0_to_3_frame_ripples():
    r = ps1()
    rows = r["mod"].lines(r["tty"], "at63")
    sp = _scene("at63_kids")["expect"]["speed_q16"]
    first = {}
    for d in rows:
        c, m, t = int(d["c"]), int(d["m"]), int(d["t"])
        first.setdefault((c, m), {})[t] = (int(d["x"]), int(d["rip"]), int(d["clk"]))
    assert len(first) == 12
    rips = {}
    for (c, m), ts in first.items():
        rip = ts[0][1]
        assert 0 <= rip <= 3
        rips.setdefault(c, set()).add(rip)
        x0 = ts[0][0]
        moved = [t for t in sorted(ts) if ts[t][0] != x0]
        start = moved[0] if moved else None
        # a member's clock starts r frames late, so it moves first at t = r + 1 (x quantised to px)
        assert start is not None and rip + 1 <= start <= rip + 2, ((c, m), rip, start)
        x10 = ts[10][0]
        exp = x0 + (1 if c == 0 else -1) * (((10 - rip) * sp) >> 16)
        assert abs(x10 - exp) <= 1, ((c, m), x10, exp)
    assert all(len(v) >= 2 for v in rips.values()), rips          # real staggers, not all the same
    w = r["mod"]
    frames = [(r["dir"] / (n + ".png"), "co03 +%03d" % t) for t, n in _scene("at63_kids")["shots"]]
    w.contact(frames, w.OUT / "crowd_lanes" / "at63_kids_cross_contact.png", cols=3,
              title="AT6.3 co03: 12 kids cross (lanes, spacing 26, doc:2.2 px/f, ripples 0-3 f) - PS1 frames")


@slow
def test_at64_150_citizens_from_48_looks_at_0_evictions():
    r = ps1()
    rows = r["mod"].lines(r["tty"], "at64")
    assert len(rows) == 150
    for d in rows:
        t = int(d["t"])
        assert int(d["rerender"]) == 0, d                            # no pool eviction, any frame
        if int(d["up"]):
            assert int(d["new"]) > 0, d                              # VRAM uploads only for new images
        if t >= 24:                                                  # every walk frame seen by then
            assert int(d["new"]) == 0 and int(d["up"]) == 0, d
    assert int(rows[0]["up"]) < 50 * 1024                            # the first frame fits the display list
    assert all(int(d["drawn"]) >= 140 for d in rows)
    assert all(int(d["keys"]) <= 48 + 16 * 4 * 2 for d in rows)
    sc = _scene("at64_crowd150")
    assert sc["expect"]["n"] == 150
    w = r["mod"]
    frames = [(r["dir"] / (n + ".png"), "+%03d" % t) for t, n in sc["shots"]]
    w.contact(frames, w.OUT / "crowd_klook" / "at64_crowd150_contact.png", cols=3,
              title="AT6.4: 150 citizens from 48 looks (K-look reuse), 0 evictions - PS1 frames")


@slow
def test_at65_equal_clocks_both_blink_on_the_due_frame():
    r = ps1()
    drawn = {(int(d["t"]), int(d["a"])): d["crc"] for d in r["mod"].lines(r["tty"], "at65")
             if "crc" in d and "uid" in d}
    solo = {(int(d["t"]), int(d["a"])): d["crc"] for d in r["mod"].lines(r["tty"], "at65")
            if "crc" in d and "uid" not in d}
    clk = {(int(d["t"]), int(d["a"])): int(d["clk"]) for d in r["mod"].lines(r["tty"], "at65") if "clk" in d}
    assert clk[(106, 0)] == clk[(106, 1)] == 106                      # equal clock values
    for a in (0, 1):
        assert drawn[(106, a)] == solo[(106, a)], a                   # each shows its due (blink) phase
        assert drawn[(105, a)] == solo[(105, a)], a
        assert drawn[(106, a)] != drawn[(105, a)], a                  # and the blink is visible
    w = r["mod"]
    sc = _scene("at65_blink")
    frames = [(r["dir"] / (n + ".png"), "+%03d" % t) for t, n in sc["shots"]]
    w.contact(frames, w.OUT / "blink_patch_f" / "at65_both_blink_contact.png", cols=2, scale=3,
              title="AT6.5: Sunny + Kevin on one clock value: both shut on the due frame (+106), Kevin talking")


@slow
def test_dx_proof_lip_flap_follows_the_track():
    r = ps1()
    w = r["mod"]
    meta = {s["name"]: s for s in r["meta"]["scenes"]}
    rows = []
    for l in r["tty"].splitlines():
        if l.startswith("wp6 dx "):
            d = w.kv(l)
            d["scene"] = l.split()[2]
            rows.append(d)
    vis = A.default_visemes()
    tracks = meta["dx1_twoshot"]["expect"]["tracks"]
    plan = {"dx1_twoshot": ("co15_perfect", 0, 14), "dx2_cu_nana": ("co20_two", 0, 10),
            "dx3_cu_mayor": ("co18_stay", 0, 8), "dx4_twoshot": ("a1_09_nana_07", 1, 10)}
    seen = {}
    for name, (lid, actor, start) in plan.items():
        trk = A.mouth_values(tracks[lid])
        got = {int(d["t"]): int(d["mouth"]) for d in rows if d["scene"] == name and int(d["a"]) == actor}
        assert len(got) == meta[name]["frames"], (name, len(got))
        for t, m in got.items():
            i = t - start
            want = vis[trk[i]] if 0 <= i < len(trk) else 0xFF
            want = -1 if want == 0xFF else want
            assert m == want, (name, t, m, want)
        uids = [d["uid"] for d in sorted((d for d in rows if d["scene"] == name and int(d["a"]) == actor),
                                         key=lambda d: int(d["t"]))]
        assert len(set(uids)) >= 4, (name, len(set(uids)))           # the picture flaps
        seen[lid] = len(set(trk))
    assert all(n >= 3 for n in seen.values()), seen                   # >= 3 distinct visemes per line
    # contact sheets: every 3rd frame of each shot, the cut sequence of the proof
    for name in plan:
        frames = [(r["dir"] / (n + ".png"), "%s +%03d" % (name[:3], t)) for t, n in meta[name]["shots"]]
        w.contact(frames, w.OUT / "dx_proof" / ("%s_contact.png" % name), cols=6, scale=1,
                  title="dialogue proof %s (PS1 frames every 3rd)" % name)
    seq = []
    for name in plan:
        for t, n in meta[name]["shots"][::5]:
            seq.append((r["dir"] / (n + ".png"), "%s +%03d" % (name[:3], t)))
    w.contact(seq, w.OUT / "dx_proof" / "dx_proof_cut_contact.png", cols=4, scale=2,
              title="dialogue proof: two-shot / CU Nana / CU Mayor / two-shot (PS1 frames)")


if __name__ == "__main__":
    fails = 0
    for name, fn in list(globals().items()):
        if name.startswith("test_") and isinstance(fn, types.FunctionType):
            try:
                fn()
                print("PASS", name)
            except Exception as e:      # noqa: BLE001
                fails += 1
                print("FAIL", name, type(e).__name__, str(e)[:2000])
    sys.exit(1 if fails else 0)
