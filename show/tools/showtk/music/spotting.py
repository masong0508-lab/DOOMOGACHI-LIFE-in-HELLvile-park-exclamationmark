"""Music spotting: epNNN/cues.shw -> MX events on the episode clock, the temp
MX track and the cue sheet (WP10; SHOW_ENGINE_SPEC.md sec.5.5, 6.2, 6.6).

cues.shw holds two kinds of statement:

    spot <cue> [from=bar:B.b] [bus=MX] [gain=<dB>]          the cue's defaults (sec.6.2)
    @<time> mx <cue> <op> [args] [k=v]                      a timed music op (sec.5.5)

<time> is one of
    <beat>+N / <beat>-N      frames from a rundown beat's frame 0 (e101_co_1+60)
    tc:HH:MM:SS:FF           programme timecode at 30 fps
    <cue>:bar:B.b[+N]        a bar of a playing cue (its bar map), plus N frames
<op>: start [from=bar:B.b] [gain=dB] | stop [over=F] | mute F | duck <dB> F |
      to <dB> [over=F] | bend <a>-><b> over=F | tapestop F | resume

These are exactly the `mx` ops a shot carries (`+60 mx mingle_box start`), so
STAGING can move any of them into the shots unchanged; the frame each one
resolves to is printed in the cue sheet. The rundown is ep/episode.shw when
it exists, else the beat sheet's Appendix F rows (docs/show/PILOT_BEATSHEET.md,
the timing source).
"""
from __future__ import annotations

import dataclasses as dc
import json
import re
from pathlib import Path

import numpy as np

from . import audiopaths as P
from . import automation as A
from . import barmap as BM
from . import cuelib
from . import shwlite as S

FPS, SR, SPF = 30, 48000, 1600


# ---- the rundown ----------------------------------------------------------------
def rundown(ep: str = "ep101"):
    """-> [(beat_id, start_frame, len_frames)] in programme order."""
    eps = P.episode_dir(ep) / "episode.shw"
    rows = []
    if eps.exists():
        for st in S.parse_file(eps, blocks=("act",)):
            for b in ([st] + st.body):
                if b.head == "beat" and "len" in b.kv:
                    rows.append((b.name, S.frames(b.kv["len"])))
    if not rows:
        txt = (P.REPO / "docs" / "show" / "PILOT_BEATSHEET.md").read_text(encoding="utf-8")
        for m in re.finditer(r"X\((e101_[a-z0-9_]+)\s*,\s*([0-9.]+)\)", txt):
            rows.append((m.group(1), int(round(float(m.group(2)) * FPS))))
    out, f = [], 0
    for bid, n in rows:
        out.append((bid, f, n))
        f += n
    return out


def tc(frame: int) -> str:
    s, ff = divmod(int(frame), FPS)
    m, s = divmod(s, 60)
    h, m = divmod(m, 60)
    return f"{h:02d}:{m:02d}:{s:02d}:{ff:02d}"


# ---- cues.shw ---------------------------------------------------------------------
@dc.dataclass
class Op:
    time: str
    cue: str
    op: str
    args: list
    kv: dict
    where: str
    frame: int | None = None
    smp: int | None = None


def load(ep: str = "ep101", path=None):
    path = Path(path or P.episode_dir(ep) / "cues.shw")
    spots, ops = {}, []
    for st in S.parse_file(path, blocks=()):
        if st.head == "spot":
            spots[st.name] = dict(st.kv)
        elif st.head.startswith("@") and len(st.words) >= 3 and st.words[1] == "tapestop":
            # the spec's own op (sec.5.5 `tapestop F`): stops every cue playing at that time
            ops.append(Op(st.head[1:], "*", "tapestop", st.words[2:], dict(st.kv), st.where()))
        elif st.head.startswith("@"):
            if len(st.words) < 4 or st.words[1] != "mx":
                raise S.ShwError(f"{st.where()}: expected `@<time> mx <cue> <op> ...`")
            ops.append(Op(st.head[1:], st.words[2], st.words[3], st.words[4:], dict(st.kv), st.where()))
        else:
            raise S.ShwError(f"{st.where()}: unknown statement `{st.head}`")
    return spots, ops


_REL = re.compile(r"^([A-Za-z0-9_]+?)(?:([+-])(\d+))?$")


def resolve(ops, spots, rd, metas):
    """Give every op its episode frame and sample; returns the ops in time order."""
    beats = {b: (f0, n) for b, f0, n in rd}
    starts = {}                      # cue -> (start smp, offset smp) of its latest instance
    out = []
    for o in ops:
        t = o.time
        add = 0
        m = re.match(r"^(.*?)([+-]\d+)$", t)
        if m and not t.startswith("tc:"):
            t, add = m.group(1), int(m.group(2))
        if t.startswith("tc:"):
            hh, mm, ss, ff = [int(x) for x in t[3:].split(":")]
            fr = ((hh * 60 + mm) * 60 + ss) * FPS + ff
            smp = fr * SPF
        elif ":bar:" in t:
            cue, _, bar = t.partition(":bar:")
            if cue not in starts:
                raise S.ShwError(f"{o.where}: {cue} is not playing at {o.time}")
            s0, off = starts[cue]
            bm = BM.BarMap.from_json(metas[cue]["barmap"])
            b, bt = BM.parse_bar("bar:" + bar)
            smp = s0 + bm.smp(b, bt) - off
            fr = int(round(smp / SPF))
        else:
            if t not in beats:
                raise S.ShwError(f"{o.where}: unknown beat `{t}`")
            fr = beats[t][0]
            smp = fr * SPF
        fr += add
        smp += add * SPF
        if ":bar:" in o.time:
            # bar anchors keep the exact sample of the bar line (sec.10.1); the frame is the
            # nearest one, which is what the picture uses
            fr = int(round(smp / SPF))
        else:
            smp = fr * SPF
        o.frame, o.smp = fr, smp
        if o.op == "start":
            frm = o.kv.get("from", spots.get(o.cue, {}).get("from", "bar:1.1"))
            bm = BM.BarMap.from_json(metas[o.cue]["barmap"])
            starts[o.cue] = (smp, bm.at(frm))
        out.append(o)
    return sorted(out, key=lambda o: (o.smp, 0 if o.op == "start" else 1))


def events(ops, spots, metas):
    """Resolved ops -> audio_events-shaped dicts (sec.6.6), with instance ids."""
    evs, live, playing = [], {}, {}
    exp = []
    for o in ops:                    # a global tapestop applies to every cue still sounding then
        if o.cue == "*":
            exp += [dc.replace(o, cue=c) for c, end in playing.items() if end > o.smp]
            continue
        exp.append(o)
        if o.op == "start":
            n = int(metas.get(o.cue, {}).get("length_smp") or 0)
            off = BM.BarMap.from_json(metas[o.cue]["barmap"]).at(
                o.kv.get("from", spots.get(o.cue, {}).get("from", "bar:1.1"))) if o.cue in metas else 0
            playing[o.cue] = o.smp + max(0, n - off)
        elif o.op == "stop":
            playing.pop(o.cue, None)
    for o in exp:
        sp = spots.get(o.cue, {})
        e = {"smp": o.smp, "frame": o.frame, "src": f"cue:{o.cue}", "bus": sp.get("bus", "MX")}
        if o.op == "start":
            e["from"] = o.kv.get("from", sp.get("from", "bar:1.1"))
            e["gain_db"] = S.db(o.kv.get("gain", sp.get("gain", "0")))
            live[o.cue] = live.get(o.cue, 0) + 1
        else:
            e["op"] = o.op
            if o.op == "mute":
                e["over_smp"] = S.frames(o.args[0]) * SPF
            elif o.op == "duck":
                e["gain_db"] = S.db(o.args[0])
                e["over_smp"] = S.frames(o.args[1]) * SPF
            elif o.op == "to":
                e["gain_db"] = S.db(o.args[0])
                e["over_smp"] = S.frames(o.kv.get("over", "0")) * SPF
            elif o.op == "bend":
                a, _, b = o.args[0].partition("->")
                e["from_st"], e["to_st"] = float(a), float(b)
                e["over_smp"] = S.frames(o.kv.get("over", "1")) * SPF
            elif o.op == "tapestop":
                e["over_smp"] = S.frames(o.args[0]) * SPF
            elif o.op == "stop":
                e["over_smp"] = S.frames(o.kv.get("over", "0")) * SPF
            elif o.op == "resume":
                pass
            else:
                raise S.ShwError(f"{o.where}: unknown mx op `{o.op}`")
        e["inst"] = f"{o.cue}#{live.get(o.cue, 1)}"
        e["where"] = o.where
        evs.append(e)
    for k, e in enumerate(evs):
        e["k"] = k
    return evs


def instances(evs):
    """Group events into cue instances: {inst: {"start": ev, "ops": [...]}}."""
    inst = {}
    for e in evs:
        if "op" not in e:
            inst[e["inst"]] = {"start": e, "ops": []}
        elif e["inst"] in inst:
            inst[e["inst"]]["ops"].append(e)
    return inst


def render_mx(evs, metas, total_smp, defs=None):
    """The temp MX bus (float32 stereo, total_smp long) from the events."""
    out = np.zeros((total_smp, 2), dtype=np.float32)
    for name, it in instances(evs).items():
        st = it["start"]
        cue = st["src"][4:]
        bm = BM.BarMap.from_json(metas[cue]["barmap"])
        off = bm.at(st["from"])
        stop = [e for e in it["ops"] if e.get("op") == "stop"]
        audio_len = metas[cue]["length_smp"] - off
        end = st["smp"] + audio_len
        if stop:
            s = min(stop, key=lambda e: e["smp"])
            end = min(end, s["smp"] + max(A.DECLICK, s.get("over_smp", 0)))
        end = min(end, total_smp)
        n = end - st["smp"]
        if n <= 0:
            continue
        bends = A.bend_curve(it["ops"], st["smp"], n)
        if bends is not None and defs is not None:
            # pitch automation: re-render the cue with the bend in its notes
            idx = np.nonzero(np.abs(np.diff(np.concatenate([[0.0], bends]))) > 0)[0]
            pts = [(0, 0.0)] + [(int(i) + off, float(bends[i])) for i in idx] + \
                  [(n + off, float(bends[-1]))]
            audio, _ = cuelib.build(defs[cue], bends=pts, write=False)
        else:
            audio = cuelib.load_audio(cue)
        beat_smp = float(bm.spb_smp)
        seg = A.render_instance(audio, n, st["smp"], off, it["ops"], st.get("gain_db", 0.0), beat_smp)
        out[st["smp"]: end] += seg.astype(np.float32)
    return out


def write_events(evs, path):
    with open(path, "w", encoding="utf-8") as f:
        json.dump(evs, f, indent=0)
