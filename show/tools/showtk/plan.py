"""plan.py - per-shot budgets and the badge estimate (spec 7.1 phase 6, 8.7, 8.10).

The planner warns, it never blocks. Per shot: chunk bytes (<= 160 KB, or a
second texture chunk), looks, actors, crowd members, texture pages, events,
and a badge: PS1-LEGAL unless a budget says EMU-ONLY. The text goes to
plan.txt and into timeline.json (shots[].badge, chunk_bytes).
"""
from . import docunits
from . import shw_format as F
from .errors import Diag

CROWD_MAX = 160          # members a 2D crowd can draw per frame inside the prim budget (8.5)
VRAM_SET_PAGES = 5       # the set pool (8.7)


def plan(ep, res, compilers):
    lines = []
    tl = {s["id"]: s for s in res.timeline["shots"]} if res.timeline else {}
    tot_bytes = 0
    lines.append("# plan.txt - %s, %d shots, %d f (%s)%s" % (
        ep.prj.ep, len(compilers), sum(c.shot.len for c in compilers), docunits.tc(ep.total),
        " [slug]" if res.slug else ""))
    lines.append("%-14s %-8s %6s %5s  %-18s %3s %5s %5s %5s %5s %4s  %s" % (
        "shot", "start", "len", "px", "set", "act", "crowd", "looks", "pages", "ev", "KB", "badge"))
    for c in compilers:
        s = c.shot
        n_bytes = getattr(c, "chunk_bytes", 0)
        tot_bytes += n_bytes
        slug = getattr(c, "slug", False) or getattr(c, "is_slug", False)
        badge = "PS1-LEGAL"
        why = []
        if n_bytes > F.CHUNK_MAX:
            badge = "EMU-ONLY"
            why.append("chunk %d KB > 160 KB" % (n_bytes // 1024))
        if not slug:
            members = sum(x["n"] for x in c.crowds)
            if members > CROWD_MAX:
                badge = "EMU-ONLY"
                why.append("%d crowd members > %d" % (members, CROWD_MAX))
            if len(c.texpages) > VRAM_SET_PAGES:
                badge = "EMU-ONLY"
                why.append("%d texture pages > %d" % (len(c.texpages), VRAM_SET_PAGES))
            for w in why:
                res.diags.append(Diag("PLAN-RAM" if "chunk" in w else "PLAN-PRIM" if "crowd" in w else "PLAN-VRAM",
                                      "%s: badge %s (the planner warns, never blocks)" % (w, badge), shot=s.id,
                                      severity="warning", **s.where()))
        c.badge = badge
        if s.id in tl:
            tl[s.id]["badge"] = badge
            tl[s.id]["chunk_bytes"] = n_bytes
        if slug:
            lines.append("%-14s %-8d %6d %5s  %-18s %3s %5s %5s %5s %5d %4.1f  %s" % (
                s.id[:14], s.start, s.len, "-", "(slug card)", "-", "-", "-", "-", getattr(c, "ev_n", 1),
                n_bytes / 1024, badge))
        else:
            lines.append("%-14s %-8d %6d %5d  %-18s %3d %5d %5d %5d %5d %4.1f  %s%s" % (
                s.id[:14], s.start, s.len, s.px, (c.set_name or "?")[:18], len(c.actors),
                sum(x["n"] for x in c.crowds), len(c.looks), len(c.texpages), getattr(c, "ev_n", 0),
                n_bytes / 1024, badge, (" (%s)" % "; ".join(why)) if why else ""))
    lines.append("total %d KB in %d chunks; EP file %d KB" % (tot_bytes // 1024, len(compilers),
                                                               len(res.shw or b"") // 1024))
    res.plan = "\n".join(lines) + "\n"
    if not res.slug:
        real = [c for c in compilers if not (getattr(c, "slug", False) or getattr(c, "is_slug", False))]
        if real:
            res.diags.append(Diag("PLAN", "%s; badge %s (est.)" % (
                ", ".join("%s %d KB chunk" % (c.shot.id, max(1, (c.chunk_bytes + 1023) // 1024)) for c in real[:8])
                + (", ..." if len(real) > 8 else ""),
                "PS1-LEGAL" if all(c.badge == "PS1-LEGAL" for c in real) else "mixed"),
                shot="%s..%s" % (real[0].shot.id, real[-1].shot.id) if len(real) > 1 else real[0].shot.id,
                severity="info"))
