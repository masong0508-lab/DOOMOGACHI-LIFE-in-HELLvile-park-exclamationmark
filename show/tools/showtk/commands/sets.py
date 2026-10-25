"""`show sets ...` - the pilot's sets (ART track).

    show sets list                       every set, its kind, planes, props and variants
    show sets check [-v]                 parse + bake + coverage + PIC-PAL + VRAM budget
    show sets sheet [SET ...] [--out D]  contact sheets at 320x180 and x3 (+ SETS_OVERVIEW.png)
    show sets bake SET OUTDIR            every plane and prop of a set as indexed PNGs

Until the ENGINE's cli.py loads this file: python3 -m showtk.commands.sets ...
(from show/tools), or python3 -m showtk.art.setkit ...
"""
import sys


def _run(argv):
    from showtk.art.setkit.__main__ import main
    return main(argv)


def _list(a):
    return _run(["list"])


def _check(a):
    return _run(["check"] + (["-v"] if a.verbose else []))


def _sheet(a):
    return _run(["sheet"] + list(a.sets) + (["--out", a.out] if a.out else []))


def _bake(a):
    return _run(["bake", a.set, a.outdir])


def register(subparsers):
    p = subparsers.add_parser("sets", help="the pilot's sets: list, check, contact sheets, bake")
    sp = p.add_subparsers(dest="sets_cmd")
    sp.add_parser("list", help="every set and its variants").set_defaults(func=_list)
    c = sp.add_parser("check", help="parse, bake, coverage, PIC-PAL, VRAM")
    c.add_argument("-v", "--verbose", action="store_true")
    c.set_defaults(func=_check)
    s = sp.add_parser("sheet", help="contact sheets (320x180 and x3)")
    s.add_argument("sets", nargs="*")
    s.add_argument("--out")
    s.set_defaults(func=_sheet)
    b = sp.add_parser("bake", help="indexed PNGs of a set's planes and props")
    b.add_argument("set")
    b.add_argument("outdir")
    b.set_defaults(func=_bake)
    p.set_defaults(func=lambda a: (p.print_help(), 0)[1])
    return p


def main(argv=None):
    import argparse
    ap = argparse.ArgumentParser(prog="show")
    register(ap.add_subparsers(dest="cmd"))
    args = ap.parse_args(["sets"] + list(sys.argv[1:] if argv is None else argv))
    return args.func(args) or 0


if __name__ == "__main__":
    sys.exit(main())
