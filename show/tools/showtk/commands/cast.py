"""`show cast ...` - the cast as data (ART track).

    show cast check                    validate cast.shw, looks/*.shw, acting.shw
    show cast sheet mayor nana         model sheets (11 expressions x 8 visemes, chibis)
    show cast sheet --all              every char seen in the episode + CAST_LINEUP.png
    show cast look mayor               the packed 48-byte DoomoLook (hex) and its text
    show cast macro freeze mayor n=21  expand an acting macro

Until the ENGINE's cli.py loads this file: python3 -m showtk.commands.cast ...
(from show/tools).
"""
import sys


def _check(args):
    from showtk.art.cast import acting, lookdata
    b = lookdata.Bible()
    lib = acting.Library()
    for e in b.errors + lib.errors:
        print("ERROR", e)
    print("cast: %d chars, %d looks, %d ksets; acting: %d macros; %d errors" % (
        len(b.chars), len(b.looks), len(b.ksets), len(lib.macros), len(b.errors) + len(lib.errors)))
    return 1 if (b.errors or lib.errors) else 0


def _sheet(args):
    from showtk.art.cast import sheet
    argv = list(args.chars)
    if args.all:
        argv.append("--all")
    if args.lineup:
        argv.append("--lineup")
    if args.out:
        argv += ["--out", args.out]
    if args.scale:
        argv += ["--scale", str(args.scale)]
    return sheet.main(argv)


def _look(args):
    from showtk.art.cast import lookdata
    b = lookdata.Bible()
    if b.errors:
        for e in b.errors:
            print("ERROR", e)
        return 1
    for cid in args.chars:
        c = b.chars.get(cid)
        if c is None or c.look is None:
            print("%s: no portrait look" % cid)
            return 1
        print("%s %s" % (cid, c.look.pack().hex()))
        print("  " + lookdata.dump_look(c.look.values))
    return 0


def _macro(args):
    from showtk.art.cast import acting
    lib = acting.Library()
    kw = dict(a.split("=", 1) for a in args.params)
    for f, op in lib.expand(args.name, args.actor, **kw):
        print("+%d %s" % (f, op))
    return 0


def register(subparsers):
    p = subparsers.add_parser("cast", help="the cast as data: check, sheets, looks, acting macros")
    sp = p.add_subparsers(dest="cast_cmd")
    c = sp.add_parser("check", help="validate cast.shw, looks/*.shw and acting.shw")
    c.set_defaults(func=_check)
    s = sp.add_parser("sheet", help="model sheets from the game's portrait engine")
    s.add_argument("chars", nargs="*")
    s.add_argument("--all", action="store_true")
    s.add_argument("--lineup", action="store_true")
    s.add_argument("--out")
    s.add_argument("--scale", type=int)
    s.set_defaults(func=_sheet)
    lk = sp.add_parser("look", help="print a char's packed DoomoLook")
    lk.add_argument("chars", nargs="+")
    lk.set_defaults(func=_look)
    m = sp.add_parser("macro", help="expand an acting macro")
    m.add_argument("name")
    m.add_argument("actor")
    m.add_argument("params", nargs="*")
    m.set_defaults(func=_macro)
    p.set_defaults(func=lambda a: (p.print_help(), 0)[1])
    return p


def main(argv=None):
    import argparse
    ap = argparse.ArgumentParser(prog="show")
    register(ap.add_subparsers(dest="cmd"))
    args = ap.parse_args(["cast"] + list(sys.argv[1:] if argv is None else argv))
    return args.func(args) or 0


if __name__ == "__main__":
    sys.exit(main())
