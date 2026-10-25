"""`show voice ...` (spec §14). Also runnable as `python3 -m showtk.voice ...` from show/tools.

  show voice <ep> [--changed] [--line ID ...] [--force] [--no-asr]   render / ingest + lock
  show voice audition <char> "<text>" [--out DIR] [--n 6]          the voice + 5 neighbours
  show voice casting [--roles a,b] [--out DIR]                     3 numbered options per main role
  show voice ingest <wav> --line <id> [--ep ep101]                 a recording -> vo/ -> lock
  show voice verify <ep> [--no-rerender]                           AT9.1: store + re-render hashes
  show voice report <ep> [--require human-voice]                   provenance (AT9.4)
  show voice lint <ep>                                             AUD-ASR, G2P-OOV, STD-AGE, closures, AUD-HEEHEE
  show voice mouth <ep> <line>                                     the mouth track and its closure check
  show voice bed <ep> <bed> --frames N --out f.wav                 a walla bed from its locked lines
  show voice lib make <name> | lib ingest <wav> <name>             library takes
  show voice import-lines <ep> <script.md ...>                     lines.shw from the script drafts
  show voice g2p "<text>"                                          the phonemes Kokoro will get
  show voice strip <ep> <line ...> --out x.png                     waveform + spectrogram + mouth + closures
"""
from __future__ import annotations

import argparse
import sys

SUBS = {"strip", "audition", "casting", "ingest", "verify", "report", "lint", "mouth", "bed", "lib", "import-lines", "g2p"}


def register(subparsers):
    p = subparsers.add_parser("voice", help="dialogue: TTS / recordings -> takes, mouth tracks, voice.lock",
                              description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("args", nargs=argparse.REMAINDER)
    p.set_defaults(func=lambda a: main(a.args))
    return p


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    if not argv or argv[0] in ("-h", "--help"):
        print(__doc__)
        return 0
    cmd = argv[0]
    if cmd not in SUBS:
        return _build(argv)
    return globals()["_" + cmd.replace("-", "_")](argv[1:])


def _build(argv):
    ap = argparse.ArgumentParser(prog="show voice")
    ap.add_argument("ep")
    ap.add_argument("--changed", action="store_true", help="only lines whose inputs changed (the default)")
    ap.add_argument("--line", action="append", help="only this line id (repeatable)")
    ap.add_argument("--force", action="store_true")
    ap.add_argument("--no-asr", action="store_true")
    a = ap.parse_args(argv)
    from . import build
    build.build(a.ep, only=set(a.line) if a.line else None, force=a.force, do_asr=not a.no_asr)
    return 0


def _audition(argv):
    ap = argparse.ArgumentParser(prog="show voice audition")
    ap.add_argument("char")
    ap.add_argument("text")
    ap.add_argument("--out")
    ap.add_argument("--n", type=int, default=6)
    a = ap.parse_args(argv)
    from . import audition
    audition.audition(a.char, a.text, out=a.out, n=a.n)
    return 0


def _casting(argv):
    ap = argparse.ArgumentParser(prog="show voice casting")
    ap.add_argument("--roles")
    ap.add_argument("--out")
    a = ap.parse_args(argv)
    from . import audition
    audition.casting(roles=a.roles.split(",") if a.roles else None, out=a.out)
    return 0


def _ingest(argv):
    ap = argparse.ArgumentParser(prog="show voice ingest")
    ap.add_argument("wav")
    ap.add_argument("--line", required=True)
    ap.add_argument("--ep", default="ep101")
    ap.add_argument("--scratch", action="store_true", help="a guide read (_s<n>) rather than a take")
    a = ap.parse_args(argv)
    from . import ingest
    ingest.ingest_file(a.wav, a.line, a.ep, scratch=a.scratch)
    return 0


def _verify(argv):
    ap = argparse.ArgumentParser(prog="show voice verify")
    ap.add_argument("ep")
    ap.add_argument("--no-rerender", action="store_true")
    a = ap.parse_args(argv)
    from . import build
    return 0 if build.verify(a.ep, rerender=not a.no_rerender) else 1


def _report(argv):
    ap = argparse.ArgumentParser(prog="show voice report")
    ap.add_argument("ep")
    ap.add_argument("--require", choices=["human-voice"])
    a = ap.parse_args(argv)
    from . import report
    return report.report(a.ep, require=a.require)


def _lint(argv):
    ap = argparse.ArgumentParser(prog="show voice lint")
    ap.add_argument("ep")
    a = ap.parse_args(argv)
    from . import report
    return report.lint(a.ep)


def _mouth(argv):
    ap = argparse.ArgumentParser(prog="show voice mouth")
    ap.add_argument("ep")
    ap.add_argument("line")
    a = ap.parse_args(argv)
    from . import report
    return report.mouth(a.ep, a.line)


def _bed(argv):
    ap = argparse.ArgumentParser(prog="show voice bed")
    ap.add_argument("ep")
    ap.add_argument("bed")
    ap.add_argument("--frames", type=int, required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args(argv)
    from . import walla
    walla.write_bed(a.ep, a.bed, a.frames, a.out)
    return 0


def _lib(argv):
    ap = argparse.ArgumentParser(prog="show voice lib")
    ap.add_argument("op", choices=["make", "ingest", "list"])
    ap.add_argument("rest", nargs="*")
    ap.add_argument("--force", action="store_true")
    a = ap.parse_args(argv)
    from . import libtakes
    if a.op == "make":
        for n in a.rest or list(libtakes.RECIPES):
            libtakes.make(n, force=a.force)
    elif a.op == "ingest":
        libtakes.ingest(a.rest[0], a.rest[1])
    else:
        for k, v in sorted(libtakes.index().items()):
            print(f"{k:24s} {v['sha256'][:12]} synthetic={v.get('synthetic')} {v.get('made', '')}")
        for k, why in libtakes.PENDING.items():
            print(f"{k:24s} pending: {why}")
    return 0


def _import_lines(argv):
    ap = argparse.ArgumentParser(prog="show voice import-lines")
    ap.add_argument("ep")
    ap.add_argument("sources", nargs="+")
    a = ap.parse_args(argv)
    from . import importer
    p, n, b = importer.import_lines(a.ep, a.sources)
    print(f"{p}: {n} lines, {b} walla beds")
    return 0


def _strip(argv):
    ap = argparse.ArgumentParser(prog="show voice strip")
    ap.add_argument("ep")
    ap.add_argument("lines", nargs="+")
    ap.add_argument("--out", required=True)
    a = ap.parse_args(argv)
    from . import strip
    print(strip.strip_lines(a.ep, a.lines, a.out))
    return 0


def _g2p(argv):
    from . import g2p
    G = g2p.G2P()
    for t in argv:
        u = G(t)
        print(u.phon, sorted(u.flags) or "", f"OOV {u.oov}" if u.oov else "")
    return 0
