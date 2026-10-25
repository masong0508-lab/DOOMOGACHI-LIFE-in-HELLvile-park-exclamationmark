"""show slug - the slug animatic of an episode (WP2, AT2.2).

    show slug ep101 [--render] [--jobs 2] [--no-encode]

Writes build/ep101/slug/EP101.SHW: one slug card per beat of
docs/show/PILOT_BEATSHEET.md (60 beats, 39,600 frames). --render captures it
through the cache (show render).
"""
from .. import paths, render, slug


def _run(a):
    if a.episode != "ep101":
        raise SystemExit("show slug: only ep101 has a beat sheet")
    bs = slug.beats()
    out = paths.episode_build(a.episode) / "slug"
    out.mkdir(parents=True, exist_ok=True)
    shw = out / "EP101.SHW"
    shw.write_bytes(slug.episode(bs).build())
    print(f"slug: {len(bs)} beats, {sum(b.frames for b in bs)} frames -> {paths.rel(shw)}")
    if a.render:
        render.render(shw, jobs=a.jobs, encode=not a.no_encode, out=out / "render")
    return 0


def register(sub):
    p = sub.add_parser("slug", help="the slug animatic: one card per beat (AT2.2)")
    p.add_argument("episode", nargs="?", default="ep101")
    p.add_argument("--render", action="store_true")
    p.add_argument("--jobs", "-j", type=int, default=2)
    p.add_argument("--no-encode", action="store_true", help="hashes and P-logs only, no masters")
    p.set_defaults(func=_run)
