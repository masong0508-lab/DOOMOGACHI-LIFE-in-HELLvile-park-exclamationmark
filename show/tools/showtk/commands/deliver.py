"""show deliver <ep> web [--range A..B] [--out DIR] [--release] (WP12; sec.12, sec.13.7).

    show deliver ep101 web --range co01..co03     ep101_co01-co03_web_hd.mp4 + .srt + .vtt + manifest
    show deliver ep101 web --release              + the standards gate and the upload package
Until cli.py loads this folder:  python3 -m showtk.commands.deliver ep101 web ...
"""
import argparse
import sys


def add_args(p):
    p.add_argument("ep")
    p.add_argument("version", nargs="?", default="web", help="web (the only 2026 version)")
    p.add_argument("--range", dest="rng", help="shot/beat/act/frame range, e.g. co01..co03")
    p.add_argument("--out", help="output folder (default show/build/<ep>/deliver/<version>)")
    p.add_argument("--mix", help="the mix WAV (default show/build/<ep>/audio/mix_web.wav)")
    p.add_argument("--masters", help="folder of <shot>.mkv masters (default: the render cache)")
    p.add_argument("--require", choices=["human-voice"], help="refuse if any TTS line remains")
    p.add_argument("--release", action="store_true",
                   help="standards gate + signed checklist + upload package (sec.13.7)")
    p.add_argument("--release-out", help="upload package folder (default build/<ep>/release/<v>)")
    p.add_argument("--allow-silent", action="store_true", help="no mix yet: deliver with silence")


def run(a):
    from showtk import deliver as d
    try:
        m = d.deliver(a.ep, a.version, rng=a.rng, out=a.out, mix=a.mix, masters_dir=a.masters,
                      release=a.release, require=a.require, allow_silent=a.allow_silent,
                      release_out=a.release_out)
    except (d.DeliverError, RuntimeError, KeyError, ValueError) as e:
        print("show deliver: %s" % e, file=sys.stderr)
        return 1
    print(d.summary(m))
    print("  manifest: %s" % m["_paths"]["committed_manifest"])
    if m["_paths"].get("release"):
        print("  upload package: %s" % m["_paths"]["release"])
    return 0


def register(subparsers):
    p = subparsers.add_parser("deliver", help="final files + QC (+ --release: the upload package)",
                              description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    add_args(p)
    p.set_defaults(func=run)
    return p


def main(argv=None):
    p = argparse.ArgumentParser(prog="show deliver", description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    add_args(p)
    return run(p.parse_args(argv))


if __name__ == "__main__":
    sys.exit(main())
