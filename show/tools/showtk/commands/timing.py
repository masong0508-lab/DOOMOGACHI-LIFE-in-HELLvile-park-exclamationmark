"""show timing <ep> <shot|range> [--static] (WP13, AT13.1): every event's frame, the picture
change measured on the master, the audio onset on the mix, the rung of each gap, mouth tracks.

    show timing ep101 co01..co03
    show timing ep101 co02 --static          # no masters needed: the compiled frames only
Exit 1 when an event's picture change or onset is off its frame.
"""
import argparse
import sys


def add_args(p):
    p.add_argument("ep")
    p.add_argument("range", nargs="?", default="all")
    p.add_argument("--static", action="store_true", help="the compiled timeline only (no masters, no mix)")
    p.add_argument("--masters", help="folder of <shot>.mkv masters (default: the render cache)")
    p.add_argument("--mix", help="mix WAV to measure onsets on (default build/<ep>/audio/mix_web.wav)")


def run(a):
    from showtk.deliver import find_mix
    from showtk.review import timing
    from showtk.sidecars import load_timeline
    tl = load_timeline(a.ep)
    mix = None if a.static else find_mix(a.ep, a.mix, ("mix_web.wav", "mix_temp.wav"))
    txt, res = timing.report(tl, None if a.range == "all" else a.range, masters_dir=a.masters,
                             mix=mix, static=a.static)
    print(txt)
    return 1 if any(r["status"] == "OFF" for r in res) else 0


def register(subparsers):
    p = subparsers.add_parser("timing", help="event-to-frame report: picture changes, onsets, rungs",
                              description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    add_args(p)
    p.set_defaults(func=run)
    return p


if __name__ == "__main__":
    ap = argparse.ArgumentParser(prog="show timing")
    add_args(ap)
    sys.exit(run(ap.parse_args()))
