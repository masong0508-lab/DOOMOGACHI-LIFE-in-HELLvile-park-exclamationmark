"""show frame <ep> <shot> +N [--overlay] (WP13): one frame x3 with the safe areas, the Wall
line, actor anchors and off-palette pixels in magenta.

    show frame ep101 co02 +13 --overlay
"""
import argparse
import sys


def add_args(p):
    p.add_argument("ep")
    p.add_argument("shot")
    p.add_argument("t", help="+N: the frame within the shot")
    p.add_argument("--overlay", action="store_true")
    p.add_argument("--out")
    p.add_argument("--masters")


def run(a):
    from showtk.review import frame
    from showtk.sidecars import load_timeline
    tl = load_timeline(a.ep)
    out, st = frame.frame(tl, a.shot, int(a.t.lstrip("+")), a.masters, a.overlay, a.out)
    print("%s (episode frame %d, %d off-palette px)" % (out, st["frame"], st["off_palette"]))
    return 0


def register(subparsers):
    p = subparsers.add_parser("frame", help="one frame x3 with safe areas, Wall line, anchors, off-palette",
                              description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    add_args(p)
    p.set_defaults(func=run)


if __name__ == "__main__":
    ap = argparse.ArgumentParser(prog="show frame")
    add_args(ap)
    sys.exit(run(ap.parse_args()))
