"""show slug - the slug animatic of an episode (WP2 AT2.2, WP4 v0).

    show slug ep101 [--render] [--jobs 2] [--no-encode] [--out DIR]

Compiles the rundown (show/ep101/rundown.shw: 60 beats, 39,600 f = 22:00)
with showc into build/ep101/slug/EP101.SHW + timeline.json + audio_events.json:
one slug card per beat (a BANDS sky in the segment's ramp, boiling grain, the
beat id and title, the act, the TC in) for exactly the beat's length, so a
re-timed beat in the rundown re-times the animatic. --render captures it
through the cache (show render).
"""
import sys


def _run(a):
    from .. import docunits, errors, paths, showc
    res = showc.compile_episode(a.episode, slug=True)
    out = paths.Path(a.out) if a.out else paths.episode_build(a.episode) / "slug"
    written = res.write(out, name="%s.SHW" % a.episode.upper())
    errors.report([d for d in res.diags if d.severity != "info"], out=sys.stdout)
    tl = res.timeline
    print("show slug %s: %d beats, %d slug cards, %d f = %s -> %s" % (
        a.episode, len(tl["beats"]), len(tl["shots"]), tl["total_frames"], docunits.tc(tl["total_frames"]),
        paths.rel(written[0])))
    if res.errors:
        return 1
    if a.render:
        from .. import render
        render.render(written[0], jobs=a.jobs, encode=not a.no_encode, out=out / "render")
    return 0


def register(sub):
    p = sub.add_parser("slug", help="the slug animatic: one card per beat of the rundown (AT2.2)")
    p.add_argument("episode", nargs="?", default="ep101")
    p.add_argument("--out", help="output dir (default show/build/<ep>/slug/)")
    p.add_argument("--render", action="store_true")
    p.add_argument("--jobs", "-j", type=int, default=2)
    p.add_argument("--no-encode", action="store_true", help="hashes and P-logs only, no masters")
    p.set_defaults(func=_run)
