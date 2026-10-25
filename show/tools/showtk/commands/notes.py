"""show notes add|list|done (WP13, sec.13.3): the creator's notes in show/<ep>/notes/round-NN.md.

    show notes add ep101 "the SUPER types too slowly" --shot co02 [--new-round]
    show notes list ep101 [--open]
    show notes done ep101 2.1 --fix "co02 SUPER at 18 cps (a1b2c3d)"
"""
import argparse
import sys


def add_args(p):
    sp = p.add_subparsers(dest="notes_cmd", metavar="add|list|done")
    a = sp.add_parser("add")
    a.add_argument("ep")
    a.add_argument("text")
    a.add_argument("--shot")
    a.add_argument("--new-round", action="store_true")
    ls = sp.add_parser("list")
    ls.add_argument("ep")
    ls.add_argument("--open", action="store_true")
    d = sp.add_parser("done")
    d.add_argument("ep")
    d.add_argument("id", help="<round>.<n>")
    d.add_argument("--fix", default="")


def run(a):
    from showtk.review import notes
    if a.notes_cmd == "add":
        ident, p = notes.add(a.ep, a.text, a.shot, a.new_round)
        print("note %s -> %s" % (ident, p))
    elif a.notes_cmd == "list":
        print(notes.listing(a.ep, a.open))
    elif a.notes_cmd == "done":
        print("ticked in %s" % notes.done(a.ep, a.id, a.fix))
    else:
        print(__doc__)
        return 2
    return 0


def register(subparsers):
    p = subparsers.add_parser("notes", help="review rounds: add, list and tick the creator's notes",
                              description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    add_args(p)
    p.set_defaults(func=run)


if __name__ == "__main__":
    ap = argparse.ArgumentParser(prog="show notes")
    add_args(ap)
    sys.exit(run(ap.parse_args()))
