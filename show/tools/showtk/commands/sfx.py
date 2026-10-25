"""`show sfx ...` (WP10): build | list | check (AT10.4) over series/sfx.shw."""
from showtk.sfx import library as _lib


def _run(a):
    return _lib.main([a.sfx_cmd] + list(getattr(a, "ids", []) or []))


def register(subparsers):
    p = subparsers.add_parser("sfx", help="the SFX library (series/sfx.shw)")
    s = p.add_subparsers(dest="sfx_cmd", required=True)
    b = s.add_parser("build")
    b.add_argument("ids", nargs="*")
    s.add_parser("list")
    s.add_parser("check")
    p.set_defaults(func=_run)
    return p
