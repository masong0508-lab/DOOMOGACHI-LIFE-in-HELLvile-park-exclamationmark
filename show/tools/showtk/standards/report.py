"""showtk.standards.report - `show standards <ep>` (sec.13.7): what needs a human eye.

Writes build/<ep>/standards.txt (Claude reads it) and standards.html with one
contact-sheet row of stills per flagged scene (the creator ticks from it):
  1 scenes with a child, teen or KID voice, and why each is clean
  2 tagged sexual/suggestive moments (implied, offscreen, innuendo)
  3 every advance with the target's yes/no and the stepback after a no
  4 jokes about groups, beliefs or real people (STD-HATE warnings, satire lines)
  5 the gore register: BRUTAL shots and truth inserts, first person vs 75 s, flashes
  6 untagged matches (STD-TAGSCAN) to resolve
then every STD-* finding and the eight checklist items.
"""
import html
from pathlib import Path

import numpy as np

from ..sidecars import build_dir, tc
from . import lints as L
from .signoff import CHECKS


def _stills(tl, shots, masters_dir, out_png, title):
    try:
        from ..review.pictures import Pictures
        from ..review.sheet import compose
        pics = Pictures(tl, masters_dir)
        frames = []
        for s in shots[:8]:
            frames += [s.start, s.start + s.len // 2, s.end - 1]
        got = pics.frames(frames)
        tiles = [(got[f], "%s f%d %s" % (tl.shot_at(f).id, f, tc(f)), "") for f in frames if f in got]
        if not tiles:
            return None
        compose(tiles, 1, cols=6, title=title).save(out_png)
        return out_png
    except (FileNotFoundError, ValueError, RuntimeError):
        return None


def build(tl, masters_dir=None, out_dir=None):
    out_dir = Path(out_dir) if out_dir else build_dir(tl.episode)
    out_dir.mkdir(parents=True, exist_ok=True)
    stills = out_dir / "standards"
    stills.mkdir(exist_ok=True)
    diags = L.run(tl)
    kids = L.kid_scenes(tl)
    scenes = tl.scenes()
    sec = {i: [] for i in range(1, 7)}
    imgs = {}
    for sc, reasons in kids.items():
        shots = scenes.get(sc, [])
        bad = [d for d in diags if d.rule == "STD-KIDS" and d.shot in {s.id for s in shots}]
        span = "f%d-%d" % (shots[0].start, shots[-1].end - 1) if shots else ""
        sec[1].append("scene %s (%s, %s): %s -- %s" % (
            sc, ",".join(s.id for s in shots[:6]) + ("..." if len(shots) > 6 else ""), span,
            "; ".join(sorted(set(reasons))[:4]),
            "NOT CLEAN: %d STD-KIDS error(s)" % len(bad) if bad else
            "clean: no innuendo, suggestive, advance, contact, rear or truth-insert tags; filter on"))
        imgs[sc] = _stills(tl, shots, masters_dir, stills / ("kids_%s.png" % sc), "scene %s (child in scene)" % sc)
    for ts, kind, ident, shot, fr, src, scene, item in L._all_tags(L.Ctx(tl, L.load_lists())):
        hit = ts & (L.SEXUAL_OK | {"suggestive"})
        if hit:
            sec[2].append("%s %s (scene %s, shot %s): %s" % (kind, ident, scene, shot, ",".join(sorted(hit))))
            s = tl.shot(shot) if shot and tl.has_shot(shot) else None
            if s and scene not in imgs:
                imgs[scene] = _stills(tl, scenes[scene], masters_dir, stills / ("tagged_%s.png" % scene),
                                      "scene %s (tagged %s)" % (scene, ",".join(sorted(hit))))
    for i, ln in enumerate(tl.lines):
        if "advance" in ln.tag_names():
            tgt = ln.to or next((t.get("to") for t in ln.tags if t["name"] == "advance"), "?")
            reply = next((p for p in tl.lines[i + 1:] if p.who == tgt and p.scene == ln.scene
                          and p.tag_names() & {"yes", "no"}), None)
            step = ""
            if reply and "no" in reply.tag_names():
                nxt = next((p for p in tl.lines[tl.lines.index(reply) + 1:] if p.who == ln.who), None)
                step = " | stepback: %s" % (("%s \"%s\"" % (nxt.id, nxt.text)) if nxt else "none")
            sec[3].append("%s %s: \"%s\"  ->  %s%s" % (ln.id, ln.who, ln.text, (
                "%s %s \"%s\"" % (reply.id, "YES" if "yes" in reply.tag_names() else "NO", reply.text))
                if reply else "NO ANSWER", step))
    for d in diags:
        if d.rule == "STD-HATE":
            sec[4].append(d.format())
        if d.rule == "STD-TAGSCAN":
            sec[6].append(d.format() + "\n    resolution: ______")
    for ln in tl.lines:
        if "satire" in ln.tag_names():
            sec[4].append("satire: %s %s \"%s\"" % (ln.id, ln.who, ln.text))
    for s in tl.shots:
        if s.gore == "BRUTAL" or (s.insert and "gore_truth" in L._tagset(s.tags)):
            sec[5].append("%s %s f%d-%d (scene %s)%s" % ("truth insert" if s.insert else "BRUTAL", s.id,
                          s.start, s.end - 1, s.scene, " -- CHILD IN SCENE" if s.scene in kids else
                          " -- no child in the scene"))
    fp = sum(s.len for s in tl.shots if s.fp)
    sec[5].append("first person: %.1f s of 75 s" % (fp / 30.0))
    fl = L.flash_events(tl)
    sec[5].append("flashes: %d scripted%s" % (len(fl), "".join(
        "\n    %s +%d %s %d f%s" % (s.id, e.t, pal, n, " full" if full else " local") for s, e, pal, n, full in fl)))
    titles = {1: "Scenes with a child, teen or KID voice (hard limit 3)",
              2: "Tagged sexual or suggestive moments (hard limits 1, 2)",
              3: "Every advance, with the yes or no (hard limit 2)",
              4: "Jokes about groups, beliefs or real people (hard limit 4)",
              5: "The gore register, first person and flashes",
              6: "Untagged matches (STD-TAGSCAN) to resolve"}
    errs = [d for d in diags if d.is_error]
    txt = ["%s \"%s\" standards report: %d STD error(s), %d warning(s) -> %s" % (
        tl.episode, tl.title, len(errs), len(diags) - len(errs),
        "BLOCKS --release" if errs else "lints green; the checklist below is the human gate")]
    for i in range(1, 7):
        txt.append("\n%d. %s" % (i, titles[i]))
        txt += ["  " + r for r in sec[i]] or ["  (none)"]
    txt.append("\nFindings:")
    txt += ["  " + d.format() for d in diags] or ["  (none)"]
    txt.append("\nThe checklist (reply e.g. \"all 8 fine\" or \"ok except 7: ...\"; `show signoff %s`):" % tl.episode)
    txt += ["  [ ] %d %s (%s)" % (n, t, lim) for n, t, lim in CHECKS]
    text = "\n".join(txt) + "\n"
    (out_dir / "standards.txt").write_text(text)
    body = ["<!doctype html><meta charset=utf-8><title>%s standards</title>" % tl.episode,
            "<style>body{font:14px sans-serif;background:#18161e;color:#ece8dc;max-width:1200px;margin:auto;padding:16px}"
            "pre{white-space:pre-wrap}img{max-width:100%%;border:1px solid #444}</style>",
            "<pre>%s</pre>" % html.escape(text)]
    for sc, p in imgs.items():
        if p:
            body.append("<h3>scene %s</h3><img src='standards/%s'>" % (html.escape(str(sc)), p.name))
    (out_dir / "standards.html").write_text("\n".join(body))
    return out_dir / "standards.txt", out_dir / "standards.html", diags


__all__ = ["build", "np"]
