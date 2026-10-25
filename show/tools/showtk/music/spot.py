"""`show music spot ep101`: the episode's music on the clock (WP10).

    python3 -m showtk.music.spot ep101            events + the cue sheet
    python3 -m showtk.music.spot ep101 --mx       ... + the temp MX track and its QA
    python3 -m showtk.music.spot ep101 --mx --range tc:00:00:00:00 tc:00:01:40:00

Writes (not git):
    <show_lib>/music/ep101_mx_events.json   the music ops in audio_events.json shape (sec.6.6)
    <show_lib>/music/ep101_mx_temp.flac     the temp MX bus, 48 kHz / 24-bit, 22:00
    <show_out>/music/CUE_SHEET.md           every cue: in, out, level, ops, bar maps, QA
    <show_out>/music/ep101_MX_temp.mp3      the temp MX track to listen to
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
import time

import numpy as np
import soundfile as sf

from . import audiopaths as P
from . import barmap as BM
from . import cuelib, qa, spotting
from .spotting import SPF, SR, tc


def _metas(defs, needed):
    metas = {}
    for cid in sorted(needed):
        if cid not in defs:
            raise SystemExit(f"spot: cue {cid} is not in series/music.shw")
        try:
            metas[cid] = cuelib.load_meta(cid)
        except FileNotFoundError:
            print(f"  building {cid} ...", flush=True)
            cuelib.build(defs[cid])
            metas[cid] = cuelib.load_meta(cid)
    return metas


def run(ep="ep101", mx=False, rng=None, sheet=True):
    defs = cuelib.load_defs()
    spots, ops = spotting.load(ep)
    metas = _metas(defs, {o.cue for o in ops})
    rd = spotting.rundown(ep)
    total = (rd[-1][1] + rd[-1][2]) * SPF
    res = spotting.resolve(ops, spots, rd, metas)
    evs = spotting.events(res, spots, metas)
    P.ensure(P.LIB_MUSIC, P.OUT_MUSIC)
    spotting.write_events(evs, P.LIB_MUSIC / f"{ep}_mx_events.json")
    out = dict(events=evs, rundown=rd, metas=metas, defs=defs, total=total)
    if mx:
        t = time.time()
        y = spotting.render_mx(evs, metas, total, defs)
        a, b = 0, total
        if rng:
            a, b = [spotting.resolve([spotting.Op(x.split("@")[-1], "", "", [], {}, "cli")], {}, rd,
                                     metas)[0].smp for x in rng]
        y = y[a:b]
        flac = P.LIB_MUSIC / f"{ep}_mx_temp.flac"
        sf.write(str(flac), y, SR, subtype="PCM_24")
        mp3 = P.OUT_MUSIC / f"{ep}_MX_temp.mp3"
        subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", str(flac), "-c:a", "libmp3lame",
                        "-b:a", "192k", str(mp3)], check=True)
        out["mx"] = y
        out["mx_secs"] = time.time() - t
        out["mx_qa"] = mx_qa(y, rd, offset=a)
    if sheet:
        from . import cuesheet
        cuesheet.write(out, P.OUT_MUSIC / "CUE_SHEET.md", ep)
    return out


def mx_qa(y, rd, offset=0, thresh_db=-50.0):
    """Per segment: unscored share (MX quieter than thresh, 0.1 s windows), peak."""
    acts = {}
    for bid, f0, n in rd:
        seg = bid.split("_")[1]
        key = {"co": "COLD OPEN", "mt": "MAIN TITLE", "1": "ACT ONE", "ec1": "EYECATCH 1",
               "2": "ACT TWO", "ec2": "EYECATCH 2", "3": "ACT THREE", "ec3": "EYECATCH 3",
               "4": "ACT FOUR", "t": "TAG", "cr": "END CREDITS", "pc": "POST-CREDITS"}.get(seg, seg)
        acts.setdefault(key, [f0, f0 + n])
        acts[key][1] = f0 + n
    rows = []
    m = np.abs(y).max(axis=1) if y.ndim == 2 else np.abs(y)
    w = SR // 10
    for k, (f0, f1) in acts.items():
        a, b = f0 * SPF - offset, f1 * SPF - offset
        if b <= 0 or a >= len(y):
            continue
        a, b = max(0, a), min(len(y), b)
        seg = y[a:b]
        nw = len(seg) // w
        r = np.sqrt(np.mean(seg[: nw * w].reshape(nw, w, -1) ** 2, axis=(1, 2)) + 1e-20)
        quiet = float(np.mean(20 * np.log10(r) < thresh_db)) * 100 if nw else 0.0
        rows.append(dict(segment=k, tc_in=tc(f0), tc_out=tc(f1), unscored_pct=round(quiet, 1),
                         peak_db=round(float(20 * np.log10(max(m[a:b].max(), 1e-12))), 1)))
    return rows


def register_into(p):
    s = None
    for a in p._actions:
        if getattr(a, "dest", None) == "music_cmd":
            s = a
    sp = s.add_parser("spot", help="cues.shw -> events, cue sheet, temp MX")
    sp.add_argument("ep", nargs="?", default="ep101")
    sp.add_argument("--mx", action="store_true")
    sp.add_argument("--range", nargs=2, metavar=("FROM", "TO"))
    sp.set_defaults(fn=lambda a: _cli(a))


def _cli(a):
    out = run(a.ep, mx=a.mx, rng=a.range)
    print(f"{len(out['events'])} music events -> {P.LIB_MUSIC / (a.ep + '_mx_events.json')}")
    if a.mx:
        print(f"temp MX rendered in {out['mx_secs']:.1f} s -> {P.OUT_MUSIC / (a.ep + '_MX_temp.mp3')}")
        for r in out["mx_qa"]:
            print(f"  {r['segment']:<14} {r['tc_in']} - {r['tc_out']}  unscored {r['unscored_pct']:5.1f} %"
                  f"  peak {r['peak_db']} dBFS")
    print(f"cue sheet -> {P.OUT_MUSIC / 'CUE_SHEET.md'}")
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(prog="show music spot")
    ap.add_argument("ep", nargs="?", default="ep101")
    ap.add_argument("--mx", action="store_true")
    ap.add_argument("--range", nargs=2, metavar=("FROM", "TO"))
    return _cli(ap.parse_args(argv))


if __name__ == "__main__":
    sys.exit(main())
