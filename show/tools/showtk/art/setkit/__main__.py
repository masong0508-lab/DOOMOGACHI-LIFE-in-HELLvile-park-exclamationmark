"""python3 -m showtk.art.setkit {list|sheet|check|bake|overview} ... (run from show/tools)."""
import argparse
import os
import sys

from .setfile import Library
from . import check as checkmod


def default_out():
    sp = os.environ.get("SHOW_OUT")
    if sp:
        return os.path.join(sp, "art", "sets")
    return os.path.join(os.getcwd(), "sets_out")


def cmd_list(lib, a):
    for sid, sd in sorted(lib.sets.items()):
        vs = ",".join(v.id for v in sd.variants) or "-"
        print("%-28s %-9s px=%d planes=%-2d props=%-3d variants=%s  %s" % (
            sid, sd.kind, sd.px, len(sd.planes), len(sd.props), vs, sd.where))


def cmd_sheet(lib, a):
    from .sheet import contact_sheet, overview
    out = a.out or default_out()
    os.makedirs(out, exist_ok=True)
    ids = a.sets or sorted(lib.sets)
    for sid in ids:
        sd = lib.sets[sid]
        if sd.kind != "2d":
            continue
        p = os.path.join(out, sid + ".png")
        tiles = contact_sheet(lib, sd, p)
        print("%s  %d frames, off-palette %d" % (p, len(tiles), sum(t[2] for t in tiles)))
    if not a.sets:
        p = os.path.join(out, "SETS_OVERVIEW.png")
        overview(lib, [lib.sets[s] for s in sorted(lib.sets)], p)
        print(p)


def cmd_overview(lib, a):
    from .sheet import overview
    out = a.out or default_out()
    os.makedirs(out, exist_ok=True)
    p = os.path.join(out, "SETS_OVERVIEW.png")
    overview(lib, [lib.sets[s] for s in sorted(lib.sets)], p)
    print(p)


def cmd_check(lib, a):
    rep = checkmod.check_all(lib, verbose=a.verbose)
    print(rep.text())
    return 0 if rep.ok() else 1


def cmd_bake(lib, a):
    from . import gen
    sd = lib.sets[a.set]
    os.makedirs(a.outdir, exist_ok=True)
    g = gen()
    for st in sd.planes + sd.props:
        for k, v in st.args.items():
            if isinstance(v, str) and v.startswith("gen:") and g.kind_of(v) == "image":
                img = g.bake(v)
                p = os.path.join(a.outdir, "%s.%s.%s.png" % (sd.id, st.id, k))
                img.save_png(p)
                print(p, img.w, img.h)


def main(argv=None):
    ap = argparse.ArgumentParser(prog="showtk.art.setkit")
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("list")
    s = sub.add_parser("sheet")
    s.add_argument("sets", nargs="*")
    s.add_argument("--out")
    s = sub.add_parser("overview")
    s.add_argument("--out")
    s = sub.add_parser("check")
    s.add_argument("-v", "--verbose", action="store_true")
    s = sub.add_parser("bake")
    s.add_argument("set")
    s.add_argument("outdir")
    a = ap.parse_args(argv)
    lib = Library().load_dir()
    fn = {"list": cmd_list, "sheet": cmd_sheet, "check": cmd_check, "bake": cmd_bake,
          "overview": cmd_overview}[a.cmd]
    return fn(lib, a) or 0


if __name__ == "__main__":
    sys.exit(main())
