"""show lint - run the showtk/lint/ checks (ANIM-DENSITY, ...) over an episode.

    show lint ep101 [--shots a..b] [--only anim_density]
"""
import importlib
import sys


def _run(args):
    from .. import errors, lint
    n = 0
    for name in lint.CHECKS:
        if args.only and name != args.only:
            continue
        mod = importlib.import_module("showtk.lint." + name)
        ds = mod.lint(args.episode, args.shots)
        errors.report(ds, out=sys.stdout)
        n += sum(d.is_error for d in ds)
        print("show lint %s [%s]: %d diagnostic(s), %d error(s)" % (args.episode, name, len(ds), n))
    return 1 if n else 0


def register(sub):
    p = sub.add_parser("lint", help="run the showtk/lint/ checks (ANIM-DENSITY) over an episode")
    p.add_argument("episode")
    p.add_argument("--shots")
    p.add_argument("--only")
    p.set_defaults(func=_run)
