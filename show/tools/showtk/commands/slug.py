"""show slug - the timed slug animatic from the rundown alone (spec 14, WP4 v0).

    show slug ep101                 SLUG.SHW + timeline.json in show/build/ep101/slug/
    show slug ep101 --render        then render it on the PS1 (when the render tool is on the branch)

Every beat of the rundown becomes one slug card (beat id, title, act, TC in)
for exactly its length, so the animatic is the episode's real running time
(101: 60 beats, 39,600 f = 22:00).
"""
import importlib
import sys


def _run(args):
    from .. import docunits, errors, paths, showc
    res = showc.compile_episode(args.episode, slug=True)
    out = args.out or (paths.episode_build(args.episode) / "slug")
    written = res.write(out)
    errors.report([d for d in res.diags if d.severity != "info"], out=sys.stdout)
    tl = res.timeline
    print("show slug %s: %d beats, %d slug cards, %d f = %s -> %s" % (
        args.episode, len(tl["beats"]), len(tl["shots"]), tl["total_frames"], docunits.tc(tl["total_frames"]),
        paths.rel(written[0])))
    if res.errors:
        return 1
    if args.render:
        try:
            render = importlib.import_module("showtk.render")
        except ImportError:
            print("show slug: the render tool (showtk/render.py) is not on this branch yet; the SHW is ready",
                  file=sys.stderr)
            return 1
        return int(render.render_shw(written[0], out=out) or 0)
    return 0


def register(sub):
    p = sub.add_parser("slug", help="the slug animatic from the rundown (one card per beat, exact running time)")
    p.add_argument("episode")
    p.add_argument("--out", help="output dir (default show/build/<ep>/slug/)")
    p.add_argument("--render", action="store_true", help="render it with showtk.render when available")
    p.set_defaults(func=_run)
