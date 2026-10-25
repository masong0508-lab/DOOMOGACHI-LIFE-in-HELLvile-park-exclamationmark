#!/usr/bin/env python3
"""gen_wp6.py - the WP6 PS1 test program's data (show/tests/engine/wp6).

Builds every test scene with showtk.actors.ShotCast (the compiler side of
WP6) from the ART lane's cast (show/series), and writes the packed records as
a C file the test driver links:  gen_wp6.py OUT.c [OUT.json]

The JSON sidecar lists each scene's start frame, length and what the test
expects, so test_wp6.py and the contact sheets agree with the driver.
"""
import json
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "tools"))

from showtk import actors as A                       # noqa: E402
from showtk.art.cast.lookdata import Bible, roster   # noqa: E402

BOP = {}


def beat_ops():
    """BOP_* values from core/include/doomo/beat.h (via the game's own symbol loader)"""
    global BOP
    if not BOP:
        for k in ("BOP_END", "BOP_FREEZE", "BOP_THAW", "BOP_PANIC", "BOP_STILL", "BOP_EXPR",
                  "BOP_POP", "BOP_HEADTURN", "BOP_SHOT"):
            BOP[k] = A.sym(k)
    return BOP


# ---- a crude text -> viseme track for TEST lines (AUDIO's voice.lock has the real ones)
def test_mouth(text, rate=3):
    """letters -> visemes, `rate` frames each (>= 2), M closures (2 f) for p/b/m;
    a TEST track only: the real ones come from voice.lock (WP9, spec 9.3)"""
    groups = []
    t = text.lower()
    i = 0
    while i < len(t):
        c = t[i]
        two = t[i:i + 2]
        if two in ("th", "sh", "ch"):
            groups.append(("C", rate)); i += 2; continue
        if two in ("oo", "ou", "ew"):
            groups.append(("U", rate + 1)); i += 2; continue
        if two in ("ee", "ea", "ai", "ay"):
            groups.append(("E", rate + 1)); i += 2; continue
        if c in "pbm":
            groups.append(("M", 2))
        elif c in "fv":
            groups.append(("F", rate))
        elif c in "a":
            groups.append(("A", rate + 1))
        elif c in "o":
            groups.append(("O", rate + 1))
        elif c in "uw":
            groups.append(("U", rate))
        elif c in "eiy":
            groups.append(("E", rate))
        elif c in "tdnszcgkjlrqx":
            groups.append(("C", 2))
        elif c in "h":
            groups.append(("E", 2))
        elif c in " ,":
            groups.append(("X", 2 if c == " " else 5))
        elif c in ".!?":
            groups.append(("X", 4))
        i += 1
    s = "".join(v * n for v, n in groups).strip("X")
    return s + "XX"


def c_bytes(name, b):
    b = bytes(b)
    rows = [", ".join("0x%02x" % x for x in b[i:i + 16]) for i in range(0, len(b), 16)]
    body = ",\n    ".join(rows) if rows else "0"
    return "static const uint8_t %s[%d] __attribute__((aligned(4))) = {\n    %s\n};\n" % (
        name, max(1, len(b)), body)


class Scene:
    def __init__(self, name, frames, mode="play", note=""):
        self.name, self.frames, self.mode, self.note = name, frames, mode, note
        self.sc = A.ShotCast(BIBLE)
        self.beat = []          # (at, op, a, b)
        self.ops = []           # (t, sop, [args])
        self.shots = []         # (t, png name)
        self.arg = 0            # mode argument (sheet: LOOK slot)
        self.backdrop = 0       # 0 sky + floor, 1 sheet grey
        self.expect = {}


BIBLE = None


def adults48():
    """48 distinct adult citizens from the ART lane's adult K-sets (crowds.shw)"""
    seen, out = set(), []
    for kid in ("giggle_adults", "plaza_dawn", "tent_crowd", "bakeoff_crowd"):
        for lab, b in BIBLE.kset_looks(kid):
            if lab not in seen:
                seen.add(lab)
                out.append((lab, b))
    ros = roster()
    rid = 113
    while len(out) < 48:            # top up with more Hillvile adults
        if rid < 500 and "KID" not in ros[rid][2] and "TEEN" not in ros[rid][2]:
            lab = "%d %s" % (rid, ros[rid][0])
            if lab not in seen:
                from showtk.art.cast.lookdata import Look
                lk = Look("r%d" % rid, "roster")
                lk.values = dict(ros[rid][1])
                out.append((lab, lk.pack()))
                seen.add(lab)
        rid += 1
    return out[:48]


def scenes():
    beat = beat_ops()
    out = []

    # ---- model sheets (AT6.1): pages drawn by the driver, look in slot 0 ----
    for who in ("mayor", "nana"):
        s = Scene("sheet_" + who, 0, mode="sheet")
        s.sc.char_slot(who)
        s.backdrop = 1
        s.arg = 0
        out.append(s)

    # ---- AT6.2: freeze stops the wings for exactly 13 f -------------------
    s = Scene("at62_freeze", 96, note="Mayor figure px=2; freeze 0 at +40, thaw at +53")
    s.sc.actor("mayor", "mayor", kind="figure", at=(160, 174), face="R", px=2)
    s.beat += [(40, beat["BOP_FREEZE"], 0, 0), (53, beat["BOP_THAW"], 0, 0)]
    s.expect = {"freeze_at": 40, "thaw_at": 53, "frames": 13}
    s.shots = [(30, "at62_f030"), (44, "at62_f044_frozen"), (52, "at62_f052_frozen"), (60, "at62_f060")]
    out.append(s)

    # ---- AT6.5: two actors on one clock value both blink on the due frame --
    # (the Mayor's and Nana's roster eyes are EYE_HAPPY_ARC, which never blink:
    # two round-eyed adults show it; Kevin talks through the blink, so the
    # mouth override and the blink live on the same frames)
    s = Scene("at65_blink", 116, mode="at65", note="Sunny + Kevin figures, phase 0, the shot clock")
    s.sc.actor("sunny", "sunny", kind="figure", at=(110, 174), face="R", px=2, phase=0)
    s.sc.actor("kevin", "kevin", kind="figure", at=(214, 174), face="L", px=2, phase=0)
    s.sc.mouth("kevin", 96, "XXEEECCAAAOOOMMEEECCXX")
    s.expect = {"due": [106, 107, 108, 109]}
    s.shots = [(105, "at65_f105_open"), (106, "at65_f106_both_shut")]
    out.append(s)

    # ---- AT6.3: co03, 12 kids cross the bridge with 0-3 f ripples ----------
    s = Scene("at63_kids", 150, note="co03: bunnies R from -32, imps L from 352, spacing 26, doc:2.2 px/f")
    sp = 2.2 * 5 / 6
    s.sc.crowd("bunnies", "kset:storybook_bunnies", 6, form="lane", seed=301, at=(-32, 118),
               spacing=26, speed=sp, ripple=3, cycle="walk4", px=2)
    s.sc.crowd("imps", "kset:storybook_imps", 6, form="lane", seed=302, at=(352, 124),
               spacing=26, speed=-sp, ripple=3, cycle="walk4", px=2)
    s.shots = [(1, "at63_f001"), (30, "at63_f030"), (75, "at63_f075"), (110, "at63_f110"), (140, "at63_f140")]
    s.expect = {"speed_q16": int(round(sp * 65536))}
    out.append(s)

    # ---- AT6.4: 150 citizens from 48 looks at 0 evictions per frame --------
    s = Scene("at64_crowd150", 150, note="scatter 100 standing (k=48, bob, flip mix) + 2 lanes of 25 walking "
                                         "(16 of the same looks): 150 citizens, 48 looks")
    looks = adults48()
    s.sc.crowd("fair", looks, 100, form="scatter", seed=19, area=(6, 74, 308, 80), bob=True, flipmix=True)
    s.sc.crowd("walk_r", looks[:16], 25, form="lane", seed=23, at=(300, 178), spacing=12, speed=0.5,
               ripple=3, cycle="walk4", front=True)
    s.sc.crowd("walk_l", looks[:16], 25, form="lane", seed=29, at=(20, 166), spacing=12, speed=-0.5,
               ripple=3, cycle="walk4", front=True)
    s.shots = [(1, "at64_f001"), (60, "at64_f060"), (149, "at64_f149")]
    s.expect = {"n": 150, "k": 48}
    out.append(s)

    # ---- the dialogue proof (M1-T10): two-shot + close-ups, lip flap ------
    lines = [
        ("mayor", "Perfect.", "co15_perfect"),
        ("nana", "Two of everything, dear.", "co20_two"),
        ("mayor", "Stay.", "co18_stay"),
        ("nana", "It's been waiting for someone.", "a1_09_nana_07"),
    ]
    stay = "XXXXXXCCCXXXXAAAAAAAAXXXXXXXXXX"         # spec 9.3, measured from Kokoro
    tracks = {}
    for who, text, lid in lines:
        trk = stay if lid == "co18_stay" else test_mouth(text)
        tracks[lid] = trk
    dx = []
    # shot 1: two-shot, px=1 figures; the Mayor says "Perfect."
    s = Scene("dx1_twoshot", 60, note="two-shot px=1: MAYOR 'Perfect.'")
    s.sc.actor("mayor", "mayor", kind="figure", at=(128, 166), face="R", expr="EXPR_HAPPY")
    s.sc.actor("nana", "nana", kind="figure", at=(196, 166), face="L")
    s.sc.mouth("mayor", 14, tracks["co15_perfect"])
    dx.append(s)
    # shot 2: CU Nana, px=2 bust; "Two of everything, dear."
    s = Scene("dx2_cu_nana", 90, note="CU px=2: NANA 'Two of everything, dear.'")
    s.sc.actor("nana", "nana", kind="bust", at=(160, 180), face="L", px=2, expr="EXPR_HAPPY")
    s.sc.mouth("nana", 10, tracks["co20_two"])
    dx.append(s)
    # shot 3: CU Mayor, px=2 bust; "Stay."
    s = Scene("dx3_cu_mayor", 60, note="CU px=2: MAYOR 'Stay.'")
    s.sc.actor("mayor", "mayor", kind="bust", at=(160, 180), face="R", px=2)
    s.sc.mouth("mayor", 8, tracks["co18_stay"])
    s.ops.append((6, *s.sc.op("mayor", "expr", "EXPR_ANGRY")))
    dx.append(s)
    # shot 4: two-shot; Nana "It's been waiting for someone." (the Mayor listens, blinks)
    s = Scene("dx4_twoshot", 90, note="two-shot px=1: NANA 'It's been waiting for someone.'")
    s.sc.actor("mayor", "mayor", kind="figure", at=(128, 166), face="R")
    s.sc.actor("nana", "nana", kind="figure", at=(196, 166), face="L", expr="EXPR_HAPPY")
    s.sc.mouth("nana", 10, tracks["a1_09_nana_07"])
    s.ops.append((52, *s.sc.op("mayor", "headturn", 64, 2)))
    s.ops.append((70, *s.sc.op("mayor", "headturn", 192, 2)))
    dx.append(s)
    for s in dx:
        s.mode = "dx"
        s.expect = {"tracks": {lid: t for lid, t in tracks.items()}}
        step = 3
        s.shots = [(t, "%s_f%03d" % (s.name, t)) for t in range(0, s.frames, step)]
    out += dx
    return out


def emit(path_c, path_json):
    global BIBLE
    BIBLE = Bible()
    if BIBLE.errors:
        print("gen_wp6: the cast has errors:\n  " + "\n  ".join(str(e) for e in BIBLE.errors), file=sys.stderr)
    sc = scenes()
    parts = ['/* generated by show/tests/engine/wp6/gen_wp6.py: do not edit */',
             '#include "wp6_data.h"', ""]
    table = []
    start = 1
    meta = []
    for i, s in enumerate(sc):
        secs = s.sc.pack()
        for k in ("LOOK", "ACTR", "CRWD", "TRAK", "FLIP"):
            parts.append(c_bytes("s%d_%s" % (i, k.lower()), secs[k]))
        bev = sorted(s.beat) + [(s.frames + 1, beat_ops()["BOP_END"], 0, 0)]
        parts.append("static const BeatEv s%d_beat[] = {\n%s\n};" % (
            i, ",\n".join("    { %d, 0, %d, %d, %d }" % (at, op, a, b) for at, op, a, b in bev)))
        ops = sorted(s.ops, key=lambda o: o[0])
        flat = []
        for t, sop, args in ops:
            args = list(args) + [0] * (6 - len(args))
            flat.append("    { %d, %d, { %s } }" % (t, sop, ", ".join(str(a) for a in args[:6])))
        parts.append("static const Wp6Op s%d_ops[] = {\n%s\n};" % (i, ",\n".join(flat) if flat else "    { -1, 0, { 0 } }"))
        frames = s.frames
        if s.mode == "sheet":
            frames = 3 * 7                  # 7 pages, 3 frames each (driver: SHEET_PAGES)
        table.append("    { \"%s\", %d, %d, %d, %d, s%d_look, %d, s%d_actr, %d, s%d_crwd, %d, s%d_trak, %d, "
                     "s%d_flip, %d, s%d_beat, %d, s%d_ops, %d }" % (
                         s.name, {"play": 0, "sheet": 1, "at65": 2, "dx": 3}[s.mode], frames, s.arg,
                         s.backdrop, i, len(secs["LOOK"]) // 48, i, len(secs["ACTR"]) // 32, i,
                         len(secs["CRWD"]) // 48, i, len(secs["TRAK"]), i, len(secs["FLIP"]), i, len(bev),
                         i, len(ops)))
        meta.append({"name": s.name, "mode": s.mode, "start": start, "frames": frames, "note": s.note,
                     "shots": [[t, n] for t, n in s.shots], "expect": s.expect,
                     "actors": [a for a, _ in s.sc.actors], "crowds": [c for c, _ in s.sc.crowds],
                     "pool_blocks_est": s.sc.blocks_estimate()})
        start += frames
    parts.append("const Wp6Scene wp6_scenes[] = {\n%s\n};\nconst int wp6_nscenes = %d;" % (
        ",\n".join(table), len(sc)))
    with open(path_c, "w") as f:
        f.write("\n".join(parts) + "\n")
    if path_json:
        with open(path_json, "w") as f:
            json.dump({"scenes": meta, "total": start - 1}, f, indent=1)
    return meta


if __name__ == "__main__":
    emit(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else None)
