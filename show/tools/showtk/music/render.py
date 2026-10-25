"""`show music` (WP10): build the cue library and the previews.

    python3 -m showtk.music.render build [cue ...]     render cues -> show_lib/music/
    python3 -m showtk.music.render list                cue ids, sources, bar maps
    python3 -m showtk.music.render import-barmap <master.mid> [--out f.json]
    python3 -m showtk.music.render barmap <cue>        the bar table (bar, sample, frame)

(`show music ...` runs the same code once the CLI auto-loads
showtk/commands/music.py.)
"""
from __future__ import annotations

import argparse
import json
import sys
import time

from . import audiopaths as P
from . import barmap as BM
from . import cuelib


def cmd_build(args):
    defs = cuelib.load_defs(args.shw)
    ids = args.cues or list(defs)
    metas = []
    for cid in ids:
        if cid not in defs:
            print(f"music: no cue {cid} in {args.shw or 'series/music.shw'}", file=sys.stderr)
            return 2
        t = time.time()
        y, meta = cuelib.build(defs[cid])
        metas.append(meta)
        lv = meta["level"]
        print(f"{cid:<26} {len(y) / 48000:7.2f} s  unity {lv['lufs']} LUFS  TP {lv['tp']} dBTP  "
              f"{meta['bpm']} bpm x{meta['bars']} bars  {time.time() - t:5.1f} s", flush=True)
    cuelib.write_lock(metas)
    return 0


def cmd_list(args):
    for cid, d in cuelib.load_defs(args.shw).items():
        print(f"{cid:<26} {d.src:<52} {d.kv.get('desc', '')}")
    return 0


def cmd_import_barmap(args):
    bm = BM.import_midi(args.master)
    js = json.dumps(bm.to_json(), indent=1)
    if args.out:
        with open(args.out, "w", encoding="utf-8") as f:
            f.write(js)
    print(f"{args.master}: {bm.bpm} bpm {bm.meter}/4, {bm.bars} bars, "
          f"{bm.frames_per_bar:.3f} f per bar")
    for name, b0, n in bm.sections:
        print(f"  bar {b0:>3} x{n:<3} {name:<22} smp {bm.smp(b0):>9}  frame {bm.frame(b0):>5}")
    return 0


def cmd_barmap(args):
    bm = cuelib.barmap_of(args.cue)
    print(f"{args.cue}: {bm.bpm} bpm, {bm.meter}/4, {bm.frames_per_bar:.3f} f per bar")
    for b, smp, fr, sec in bm.table():
        print(f"  bar {b:>3}  smp {smp:>9}  frame {fr:>5}  {sec}")
    return 0


def register(sub):
    p = sub.add_parser("music", help="music cues: build, list, bar maps (WP10)")
    p.add_argument("--shw", default=None, help="music.shw (default show/series/music.shw)")
    s = p.add_subparsers(dest="music_cmd", required=True)
    b = s.add_parser("build")
    b.add_argument("cues", nargs="*")
    b.set_defaults(fn=cmd_build)
    s.add_parser("list").set_defaults(fn=cmd_list)
    m = s.add_parser("import-barmap")
    m.add_argument("master")
    m.add_argument("--out")
    m.set_defaults(fn=cmd_import_barmap)
    bm = s.add_parser("barmap")
    bm.add_argument("cue")
    bm.set_defaults(fn=cmd_barmap)
    return p


def main(argv=None):
    ap = argparse.ArgumentParser(prog="show")
    sub = ap.add_subparsers(dest="cmd", required=True)
    register(sub)
    a = ap.parse_args(["music"] + list(argv if argv is not None else sys.argv[1:]))
    return a.fn(a)


if __name__ == "__main__":
    sys.exit(main())
