"""show sheet <ep> <shot|range> and show diff <ep> <range> --against DIR (WP13).

    show sheet ep101 co01..co03 [--every 30] [--scale 1|2] [--out PNG]
        every event frame + every Nth frame, captioned shot +frame, f, TC, event
    show diff ep101 co02 --against show/build/ep101/approved/   [--every 15]
        reference | current per sampled frame, changed tiles outlined in red
"""
import argparse
import sys


def add_sheet(p):
    p.add_argument("ep")
    p.add_argument("range", nargs="?", default="all")
    p.add_argument("--every", type=int, default=30, help="also every Nth episode frame (0: events only)")
    p.add_argument("--scale", type=int, choices=[1, 2], help="tile scale (default 2 up to 24 tiles)")
    p.add_argument("--cols", type=int)
    p.add_argument("--out")
    p.add_argument("--masters")


def run_sheet(a):
    from showtk.review import sheet
    from showtk.sidecars import load_timeline
    tl = load_timeline(a.ep)
    out, tiles = sheet.sheet(tl, None if a.range == "all" else a.range, a.masters, a.every, a.scale,
                             a.out, a.cols)
    print("%s (%d tiles)" % (out, len(tiles)))
    return 0


def add_diff(p):
    p.add_argument("ep")
    p.add_argument("range")
    p.add_argument("--against", required=True, help="folder of the reference <shot>.mkv masters")
    p.add_argument("--every", type=int, default=15)
    p.add_argument("--out")
    p.add_argument("--masters")


def run_diff(a):
    from showtk.review import sheet
    from showtk.sidecars import load_timeline
    tl = load_timeline(a.ep)
    out, changed = sheet.diff(tl, a.range, a.masters, a.against, a.every, a.out)
    print("%s (%d sampled frames changed%s)" % (out, len(changed), (": " + ", ".join(
        "f%d %dpx" % c for c in changed[:12])) if changed else ""))
    return 0


def register(subparsers):
    p = subparsers.add_parser("sheet", help="contact sheet PNG of event frames + every Nth frame",
                              description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    add_sheet(p)
    p.set_defaults(func=run_sheet)
    q = subparsers.add_parser("diff", help="before/after sheet against reference masters",
                              description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    add_diff(q)
    q.set_defaults(func=run_diff)


if __name__ == "__main__":
    ap = argparse.ArgumentParser(prog="show sheet")
    add_sheet(ap)
    sys.exit(run_sheet(ap.parse_args()))
