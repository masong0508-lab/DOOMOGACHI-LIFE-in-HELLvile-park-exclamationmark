"""lint/anim_density.py - ANIM-DENSITY: the creator's quality bar ("people won't like slideshows").

Two modes:

  plan    static analysis of the compiled plan. Runs the real compiler (showc) and records every
          resolved op of every shot (frame, op, args, macro expanded), then per shot:
            ANIM-DENSITY   fewer than 3 independently moving elements (idle actors, movers,
                           camera motion, the set's visible ambient layers [max 2], crowds,
                           fx/boil, a 3D host's own motion)
            ANIM-HOLD      a window of more than 12 frames with nothing moving, outside a
                           declared hold (freeze/still/deadpan/still_hold spans, protected beats)
            ANIM-GESTURE   a spoken line with no gesture or expression change by its speaker
            ANIM-LISTEN    an on-screen listener in a dialogue shot who never reacts
            ANIM-CAM       a shot of 60 f or more with no camera motion at all
            ANIM-OVERWORLD dialogue on an OVERWORLD set, or an overworld shot over 4 s
  frames  frame-diff over rendered frames (a PNG folder or an FFV1/any video): runs of more than
          12 pixel-identical consecutive frames, mapped to shots when --timeline is given.

    cd show/tools && python3 -m showtk.lint.anim_density plan ep101 [--shots a1_01a..a1_03c] [--by-file]
    cd show/tools && python3 -m showtk.lint.anim_density frames DIR|FILE.mkv [--timeline T.json] [--thresh 0]

Everything is a warning: the bar says treat them as bugs to fix, not as build breakers.
"""
import argparse
import re
import sys
from collections import Counter, defaultdict

from ..errors import Diag

MIN_ELEMENTS = 3
MAX_STATIC = 12
CAM_MIN_LEN = 60
OVERWORLD_MAX = 120
GESTURE_OPS = {"pose", "expr", "headturn", "squash", "face", "pop", "twitch", "move", "walk", "panic",
               "state", "freeze", "still", "eyes"}
REACT_OPS = GESTURE_OPS | {"blink"}
HOLD_OPS = {"freeze", "still"}
CAM_MOVES = {"to", "pan", "creep", "whip", "crane"}
CAMFX = {"shake", "punch", "zoom", "hitstop"}
# how long an event visibly moves when it carries no over= (frames)
SPAN = {"pose": 16, "squash": 6, "blink": 3, "headturn": 8, "expr": 6, "fx": 20, "shake": 10,
        "pop": 10, "face": 2, "twitch": 12, "panic": 30, "zoom": 8, "flash": 4, "punch": 4,
        "sign": 6, "card": 6, "show": 2, "hide": 2, "prop.set": 2, "reveal": 12, "wipe": 12,
        "dissolve": 12, "split": 12, "iris_in": 12, "iris_out": 12, "fade": 8, "cam3d": 60}
HOST_MOTION = {"view3d": 2, "overworld": 1, "room": 1}


class Op:
    __slots__ = ("f", "kw", "words", "kv", "line", "file", "macro", "time")

    def __init__(self, f, c):
        self.f, self.kw, self.words, self.kv = f, c.kw, list(c.words), dict(c.kv)
        self.line, self.file = c.line, c.file
        self.macro = getattr(c, "from_macro", None)
        self.time = c.time


def compile_recording(ep="ep101", shots=None):
    """run the real compiler with ShotCompiler.op wrapped: -> (EpisodeCompiler, Result)"""
    from .. import showc
    orig = showc.ShotCompiler.op

    def op(self, f, c, head, ast):
        if not hasattr(self, "_anim_ops"):
            self._anim_ops = []
        self._anim_ops.append(Op(f, c))
        return orig(self, f, c, head, ast)

    showc.ShotCompiler.op = op
    try:
        epc = showc.EpisodeCompiler(ep, shots=shots, bake=False)
        res = epc.run()
    finally:
        showc.ShotCompiler.op = orig
    return epc, res


def _frames(v, default):
    if v is None:
        return default
    m = re.match(r"^\+?(\d+)$", str(v))
    if m:
        return int(m.group(1))
    m = re.match(r"^\+?R(\d+)$", str(v))
    if m:
        return int(m.group(1))
    m = re.match(r"^\+(\d+)\.\.\+(\d+)$", str(v))
    if m:
        return int(m.group(2)) - int(m.group(1))
    m = re.match(r"^([\d.]+)s$", str(v))
    if m:
        return int(float(m.group(1)) * 30)
    return default


def visible_ambs(setdef, variant):
    """the set's amb ids on in this variant (hidden=yes ones only when the variant shows them)"""
    if setdef is None:
        return []
    ambs, show, hide = [], set(), set()
    for c in setdef.stmt.children:
        if c.kw == "variant" and c.words and c.words[0] == variant:
            show |= set((c.get("show") or "").split(",")) - {""}
            hide |= set((c.get("hide") or "").split(",")) - {""}
    for c in setdef.stmt.children:
        if c.kw != "amb" or not c.words:
            continue
        aid = c.words[0]
        on = c.get("hidden") != "yes" or aid in show
        if aid in hide:
            on = False
        if on:
            ambs.append(aid)
    return ambs


def analyse_shot(sc, protected=False, by_id=None):
    """-> (stats dict, [Diag])"""
    sh = sc.shot
    L = sh.len
    ops = getattr(sc, "_anim_ops", [])
    setdef = getattr(sc, "setdef", None)
    variant = (sh.kv.get("set") or "").partition(":")[2]
    kind = getattr(sc, "set_kind", "2d")
    ambs = visible_ambs(setdef, variant)
    w = sh.where()
    out = []

    def warn(rule, msg, frame=None, line=None):
        kw = dict(w)
        if line is not None:
            kw["line"] = line
        out.append(Diag(rule, msg, shot=sh.id, frame=frame, severity="warning", **kw))

    actors = {a["id"]: a for a in sc.actors}
    who2id = {a["who"]: a["id"] for a in sc.actors}

    def aid(name):
        return name if name in actors else who2id.get(name, name)

    # ---- elements ---------------------------------------------------------------------
    elems = set()
    for a in sc.actors:
        if a["idle"] == 0:
            elems.add("actor:" + a["id"])
    for cid in getattr(sc, "crowd_ix", {}):
        elems.add("crowd:" + cid)
    for i, aname in enumerate(ambs[:2]):
        elems.add("amb:" + aname)
    for k in range(HOST_MOTION.get(kind, 0)):
        elems.add("host:%d" % k)
    setup_drift = any(c.kw == "cam" and c.words and c.words[0] == "drift" for c in sc.st.children
                      if c.time is None)
    cam_moving = setup_drift
    for o in ops:
        if o.kw in ("move", "walk", "squash", "pose") and o.words:
            elems.add("actor:" + aid(o.words[0]))
        elif o.kw == "cam" and o.words and o.words[0] in CAM_MOVES:
            cam_moving = True
        elif o.kw in CAMFX:
            elems.add("camfx")
        elif o.kw in ("fx", "boil"):
            elems.add("fx:" + (o.words[0] if o.words else o.kw))
        elif o.kw in ("sign", "card", "super", "reveal"):
            elems.add("text")
        elif o.kw == "cam3d" or (o.kw == "world" and o.words[:2] == ["cam", "to"]):
            cam_moving = True           # a host's rail or keyed camera (OVERWORLD / ROOM / VIEW3D)
    src = sh.kv.get("replay") or sh.kv.get("base")
    if src and by_id:
        s2 = by_id.get(src.split("@")[0])
        if s2 is not None and any(o.kw in ("cam", "cam3d") for o in getattr(s2, "_anim_ops", [])):
            cam_moving = True           # a replay re-runs its source's camera
    if cam_moving:
        elems.add("camera")

    # ---- motion mask --------------------------------------------------------------------
    live = bytearray(L + 1)
    held = bytearray(L + 1)
    continuous = bool(ambs) or kind in HOST_MOTION or setup_drift or bool(getattr(sc, "crowd_ix", {}))
    boil_on = any(o.kw == "boil" and (not o.words or o.words[0] == "on") for o in ops)
    continuous = continuous or boil_on

    def mark(buf, a, b):
        for i in range(max(0, a), min(L, b) + 1):
            buf[i] = 1

    # idle actors live except under their own freeze/still spans
    idle_ids = [a["id"] for a in sc.actors if a["idle"] == 0]
    stops = defaultdict(list)          # actor -> [(f, kind)] freeze/still/thaw
    for o in ops:
        if o.kw in ("freeze", "still", "thaw") and o.words:
            stops[aid(o.words[0])].append((o.f, o.kw))
    held_by = defaultdict(list)       # actor -> [(a, b)] its declared holds
    for a in actors:
        on, t0 = True, 0
        for f, k in sorted(stops.get(a, [])):
            if k in HOLD_OPS and on:
                if a in idle_ids:
                    mark(live, t0, f)
                on, t0 = False, f
            elif k == "thaw" and not on:
                held_by[a].append((t0, f))
                on, t0 = True, f
        if not on:
            held_by[a].append((t0, L))
        elif a in idle_ids:
            mark(live, t0, L)
        for x, y in held_by[a]:
            mark(held, x, y)
    for o in ops:
        if o.kw in ("freeze", "still") and not o.words:
            mark(held, o.f, o.f + _frames(o.kv.get("n"), 13))
        n = _frames(o.kv.get("over"), SPAN.get(o.kw, 0))
        if o.kw == "cam":
            n = _frames(o.kv.get("over"), 30 if (o.words and o.words[0] in CAM_MOVES) else 0)
            if o.words and o.words[0] == "creep":
                n = L
        if o.kw == "say":
            continue
        if n:
            mark(live, o.f, o.f + n)
    # lines: the speaker's lips flap for the line
    says = []
    for o in ops:
        if o.kw == "say" and o.words:
            try:
                t = sc.lib.line_timing(o.words[0], sc.cast)
                n = int(t["len"])
            except Exception:
                n = 45
            spk = aid(o.words[1]) if len(o.words) > 1 else None
            says.append((o, spk, n))
            if spk in actors and o.kv.get("mouth") != "none":
                mark(live, o.f, o.f + n)
    if protected:
        mark(held, 0, L)
    if not continuous:
        run, start = 0, 0
        for i in range(L):
            if live[i] or held[i]:
                if run > MAX_STATIC:
                    warn("ANIM-HOLD", "%d f pixel-static (+%d..+%d) outside a declared hold" % (run, start, i - 1),
                         frame=start)
                run = 0
            else:
                if run == 0:
                    start = i
                run += 1
        if run > MAX_STATIC:
            warn("ANIM-HOLD", "%d f pixel-static (+%d..+%d) outside a declared hold" % (run, start, L - 1),
                 frame=start)

    n_el = len(elems)
    if n_el < MIN_ELEMENTS and L > MAX_STATIC:      # a flash insert (<= 12 f) cannot hold too long
        warn("ANIM-DENSITY", "%d independently moving element(s) (%s); the bar is %d" % (
            n_el, ", ".join(sorted(elems)) or "none", MIN_ELEMENTS))

    # ---- dialogue ---------------------------------------------------------------------------
    speakers = set()
    for k, (o, spk, n) in enumerate(says):
        speakers.add(spk)
        if spk not in actors or actors[spk]["idle"] != 0:
            continue                    # VO / off-screen / an authored still (possessed, Mascotty)
        if any(x <= o.f <= y for x, y in held_by.get(spk, [])):
            continue                    # a deadpan / freeze is the gag: no gesture wanted
        end = o.f + n
        got = any(p.kw in GESTURE_OPS and p.words and aid(p.words[0]) == spk and o.f - 6 <= p.f <= end
                  for p in ops)
        if not got:
            warn("ANIM-GESTURE", "line %s: %s has no gesture or expression change in +%d..+%d" % (
                o.words[0], spk, o.f, end), frame=o.f, line=o.line)
    if says:
        for a in actors:
            if a in speakers or actors[a]["idle"] != 0:
                continue
            if not any(p.kw in REACT_OPS and p.words and aid(p.words[0]) == a for p in ops):
                warn("ANIM-LISTEN", "listener %s never reacts (blink/glance/expr) to the dialogue" % a)

    if not cam_moving and L >= CAM_MIN_LEN and kind != "view3d":
        warn("ANIM-CAM", "%d f with no camera motion (push-in, pan, track, whip, crane, drift)" % L)
    if kind == "overworld" and (says or L > OVERWORLD_MAX):
        warn("ANIM-OVERWORLD", "OVERWORLD is establishing-only: %s" % (
            "dialogue plays here (stage it on a set)" if says else "%d f is over ~4 s" % L))
    stats = {"shot": sh.id, "file": sh.where().get("file"), "len": L, "elements": n_el,
             "camera": cam_moving, "ambs": len(ambs), "says": len(says), "kind": kind}
    return stats, out


def run(ep="ep101", shots=None):
    """-> (stats list, diags) for every compiled, non-slug shot"""
    epc, res = compile_recording(ep, shots)
    stats, diags = [], []
    beats = {}
    for b in getattr(epc.rundown, "beats", {}).values() if isinstance(getattr(epc.rundown, "beats", None), dict) else []:
        beats[b.id] = b
    by_id = {sc.shot.id: sc for sc in epc.compilers if hasattr(sc, "st")}
    for sc in epc.compilers:
        if getattr(sc, "slug", False) or getattr(sc, "is_slug", False) or not hasattr(sc, "st"):
            continue
        b = getattr(sc.shot, "beat", None)
        prot = bool(getattr(getattr(epc.rundown, "by_id", {}).get(b), "protected", False))
        s, d = analyse_shot(sc, prot, by_id)
        stats.append(s)
        diags += d
    return stats, diags


def lint(ep="ep101", shots=None):
    """the lint runner's entry: -> [Diag]"""
    return run(ep, shots)[1]


# ---- frame-diff mode ------------------------------------------------------------------------
def iter_frames(src):
    """yield uint8 numpy frames from a PNG folder or any video ffmpeg reads"""
    import subprocess
    from pathlib import Path

    import numpy as np
    p = Path(src)
    if p.is_dir():
        from PIL import Image
        for f in sorted(p.glob("*.png")):
            yield np.asarray(Image.open(f).convert("RGB"))
        return
    probe = subprocess.run(["ffprobe", "-v", "error", "-select_streams", "v:0", "-show_entries",
                            "stream=width,height", "-of", "csv=p=0", str(p)], capture_output=True, text=True)
    wd, ht = [int(x) for x in probe.stdout.strip().split(",")[:2]]
    proc = subprocess.Popen(["ffmpeg", "-v", "error", "-i", str(p), "-f", "rawvideo", "-pix_fmt", "rgb24", "-"],
                            stdout=subprocess.PIPE)
    n = wd * ht * 3
    while True:
        buf = proc.stdout.read(n)
        if len(buf) < n:
            break
        yield np.frombuffer(buf, np.uint8).reshape(ht, wd, 3)
    proc.wait()


def frame_runs(frames, thresh=0):
    """-> [(start, length)] runs where consecutive frames differ by <= thresh (mean abs, 0..255)"""
    import numpy as np
    runs, prev, start, i = [], None, 0, -1
    for i, fr in enumerate(frames):
        if prev is not None:
            d = float(np.abs(fr.astype(np.int16) - prev.astype(np.int16)).mean())
            if d > thresh:
                if i - start > MAX_STATIC:
                    runs.append((start, i - start))
                start = i
        prev = fr
    if i - start + 1 > MAX_STATIC:
        runs.append((start, i - start + 1))
    return runs


def frames_report(src, timeline=None, thresh=0.0, offset=0):
    import json
    shots = []
    holds = set()
    if timeline:
        t = json.load(open(timeline))
        shots = [s for s in t["shots"] if s.get("compiled")]
        prot = {b["id"] for b in t.get("beats", []) if b.get("protected")}
        for s in shots:
            if s.get("beat") in prot:
                holds.update(range(s["start"], s["end"]))
    out = []
    for st, n in frame_runs(iter_frames(src), thresh):
        a = st + offset
        if all(f in holds for f in range(a, a + n)):
            continue
        sid = next((s["id"] for s in shots if s["start"] <= a < s["end"]), None)
        out.append(Diag("ANIM-DENSITY", "%d frames pixel-static from frame %d" % (n, a), shot=sid,
                        frame=(a - next(s["start"] for s in shots if s["id"] == sid)) if sid else a,
                        severity="warning"))
    return out


def summary(stats, diags, by_file=True):
    lines = []
    seg = lambda f: (f or "?").split("/")[-1].replace(".shw", "")
    shots_by = Counter(seg(s["file"]) for s in stats)
    rules = defaultdict(Counter)
    shots_warned = defaultdict(set)
    for d in diags:
        k = seg(d.file)
        rules[k][d.rule] += 1
        shots_warned[k].add(d.shot)
    for k in sorted(shots_by):
        lines.append("%-8s %3d shots, %3d with warnings: %s" % (
            k, shots_by[k], len(shots_warned[k]), ", ".join("%s %d" % kv for kv in sorted(rules[k].items())) or "clean"))
    lines.append("ANIM total: %d warning(s) on %d shot(s)" % (len(diags), len({d.shot for d in diags})))
    return "\n".join(lines)


def main(argv=None):
    ap = argparse.ArgumentParser(prog="anim_density")
    sub = ap.add_subparsers(dest="mode", required=True)
    p = sub.add_parser("plan")
    p.add_argument("episode", nargs="?", default="ep101")
    p.add_argument("--shots")
    p.add_argument("--quiet", action="store_true", help="the per-file summary only")
    p.add_argument("--file", help="only diagnostics from this shots file (a1, co_mt, ...)")
    q = sub.add_parser("frames")
    q.add_argument("src")
    q.add_argument("--timeline")
    q.add_argument("--thresh", type=float, default=0.0)
    q.add_argument("--offset", type=int, default=0, help="episode frame of the first rendered frame")
    a = ap.parse_args(argv)
    if a.mode == "plan":
        stats, diags = run(a.episode, a.shots)
        if a.file:
            diags = [d for d in diags if (d.file or "").endswith("/%s.shw" % a.file)]
            stats = [s for s in stats if (s["file"] or "").endswith("/%s.shw" % a.file)]
        if not a.quiet:
            for d in diags:
                print(d.format())
        print(summary(stats, diags))
        return 0
    diags = frames_report(a.src, a.timeline, a.thresh, a.offset)
    for d in diags:
        print(d.format())
    print("ANIM frames: %d static run(s) over %d f" % (len(diags), MAX_STATIC))
    return 0


if __name__ == "__main__":
    sys.exit(main())
