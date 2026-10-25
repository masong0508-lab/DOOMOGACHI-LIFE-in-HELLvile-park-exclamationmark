"""show preview <ep> <range> [--out DIR] (WP12/WP13; sec.13.2).

An x3 (960x540) MP4 with burn-ins (TC, episode frame, beat, shot +frame, line id,
TEMP TTS, badge) and the temp mix, per beat segment, so a one-shot change
re-encodes one segment.
    show preview ep101 co01..co03
    show preview ep101 A1                  # an act
Until cli.py loads this folder:  python3 -m showtk.commands.preview ep101 co01..co03
"""
import argparse
import sys


def add_args(p):
    p.add_argument("ep")
    p.add_argument("range", nargs="?", default="all")
    p.add_argument("--out", help="output folder (default show/build/<ep>/previews)")
    p.add_argument("--mix", help="temp mix WAV (default build/<ep>/audio/mix_web.wav or mix_temp.wav)")
    p.add_argument("--masters", help="folder of <shot>.mkv masters (default: the render cache)")


def run(a):
    from showtk import deliver as d
    try:
        m = d.preview(a.ep, None if a.range == "all" else a.range, out=a.out, mix=a.mix,
                      masters_dir=a.masters)
    except (d.DeliverError, RuntimeError, KeyError, ValueError) as e:
        print("show preview: %s" % e, file=sys.stderr)
        return 1
    print(d.summary(m))
    return 0


def register(subparsers):
    p = subparsers.add_parser("preview", help="x3 burned-in preview MP4 with the temp mix",
                              description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    add_args(p)
    p.set_defaults(func=run)
    return p


def main(argv=None):
    p = argparse.ArgumentParser(prog="show preview", description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    add_args(p)
    return run(p.parse_args(argv))


if __name__ == "__main__":
    sys.exit(main())
