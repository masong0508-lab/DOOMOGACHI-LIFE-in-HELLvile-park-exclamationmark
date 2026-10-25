"""show status <ep> (WP13): shots by state (slug, staged, voiced, final), open lints,
render-cache hits, TTS lines and each delivery's sign-off state.

    show status ep101
"""
import argparse
import sys


def add_args(p):
    p.add_argument("ep")
    p.add_argument("--masters")


def run(a):
    from showtk.review import status
    from showtk.sidecars import load_timeline
    txt, _ = status.status(load_timeline(a.ep), a.masters)
    print(txt)
    return 0


def register(subparsers):
    p = subparsers.add_parser("status", help="where the episode stands: shot states, lints, cache, sign-off",
                              description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    add_args(p)
    p.set_defaults(func=run)


if __name__ == "__main__":
    ap = argparse.ArgumentParser(prog="show status")
    add_args(ap)
    sys.exit(run(ap.parse_args()))
