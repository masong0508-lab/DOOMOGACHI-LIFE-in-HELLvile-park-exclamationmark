"""show compile - .shw text -> EP###.SHW + timeline.json + audio_events.json (spec 7, 14).

    show compile ep101                      the whole episode
    show compile ep101 --shots co01..co03   a range (or a beat id); positions stay the episode's
    show compile ep101 --print              also print the resolved table (spec 6.4)
    show compile ep101 --out DIR            default show/build/ep101/

Exit 1 on any error; every problem is printed as file:line: shot +frame [RULE].
"""
import sys
import time


def table(res):
    from .. import docunits
    tl = res.timeline
    out = ["%-10s %-13s %-12s %5s  %-18s %s" % ("shot", "frames", "TC in", "len", "set / px", "sound on frame 0")]
    for s in tl["shots"]:
        if not s.get("compiled") or s.get("slug"):
            continue
        f0 = [e for e in res.audio["events"] if s["start"] * docunits.SPF == e["smp"]]
        snd = ", ".join("%s (smp %d)" % (e["src"], e["smp"]) for e in f0) or "-"
        out.append("%-10s %-13s %-12s %5d  %-18s %s" % (s["id"], "%d-%d" % (s["start"], s["end"] - 1), s["tc_in"],
                                                       s["len"], "%s / %s" % (s.get("set"), s.get("px")), snd))
        for e in s.get("events", []):
            k = " k=%d" % e["k"] if e.get("k") is not None else ""
            span = ("-%d" % (e["end"] - 1)) if e.get("end") not in (None, e["f"]) else ""
            extra = ""
            if e.get("glyph_frames"):
                g = e["glyph_frames"]
                extra = " glyph 1 at f %d, glyph %d at f %d (%s cps)" % (g[0], e["glyphs"], g[-1], e.get("cps"))
            out.append("    f %5d%-6s %-8s %s%s%s" % (e["f"], span, e["kind"], e["text"], k, extra))
    return "\n".join(out)


def _run(args):
    from .. import errors, paths, showc
    t0 = time.time()
    res = showc.compile_episode(args.episode, shots=args.shots)
    out = args.out or paths.episode_build(args.episode)
    written = res.write(out)
    shown = [d for d in res.diags if d.severity != "info" or args.verbose]
    errors.report(shown, out=sys.stdout)
    if args.print:
        print(table(res))
    n = len(res.errors)
    nw = len([d for d in res.diags if d.severity == "warning"])
    print("show compile %s%s: %d shots, %d f (%s), %d audio events; %d error(s), %d warning(s); %.2f s -> %s" % (
        args.episode, " --shots " + args.shots if args.shots else "",
        len([s for s in res.timeline["shots"] if s["compiled"]]), res.timeline["total_frames"],
        __import__("showtk.docunits", fromlist=["tc"]).tc(res.timeline["total_frames"]),
        len(res.audio["events"]), n, nw, time.time() - t0, paths.rel(written[0])))
    return 1 if n else 0


def register(sub):
    p = sub.add_parser("compile", help="compile an episode's .shw files to EP###.SHW + timeline.json + audio_events.json")
    p.add_argument("episode")
    p.add_argument("--shots", help="a range co01..co03, a list, or a beat id")
    p.add_argument("--out", help="output dir (default show/build/<ep>/)")
    p.add_argument("--print", action="store_true", help="print the resolved frames/samples per shot (spec 6.4)")
    p.add_argument("-v", "--verbose", action="store_true", help="also print the ok lines (COMP-LOCK, AUD-FRAME0, PLAN)")
    p.set_defaults(func=_run)
