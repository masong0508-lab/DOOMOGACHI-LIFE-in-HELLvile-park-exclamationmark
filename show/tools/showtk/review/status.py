"""showtk.review.status - `show status <ep>`: where the episode stands (WP13, sec.13.1).

Shots by state, open lints, cache hits, TTS lines and the deliveries with their
sign-off state.  States, in order:
    slug      no set yet (the rundown's slug card)
    staged    a set, but a line in it is still provisional (no take locked)
    voiced    every line in it has a locked take
    final     voiced, rendered (master in the cache) and no lint error on it
"""
import json
import re
from collections import Counter

from ..edl import Masters
from ..sidecars import build_dir, ep_dir, tc

RULE = re.compile(r"\[([A-Z0-9][A-Z0-9-]*)\]\s*(ERROR|error|WARN|warning|ok)?")


def read_lints(ep):
    """(errors, warnings, Counter by rule, {shot: errors}) from build/<ep>/lint.txt or lint.json."""
    b = build_dir(ep)
    errs = warns = 0
    by_rule, by_shot = Counter(), Counter()
    js = b / "lint.json"
    if js.exists():
        data = json.loads(js.read_text())
        items = data.get("diags", data) if isinstance(data, dict) else data
        for d in items:
            sev = d.get("severity", "error")
            if sev == "error":
                errs += 1
                if d.get("shot"):
                    by_shot[d["shot"]] += 1
            elif sev == "warning":
                warns += 1
            if sev != "ok":
                by_rule[d.get("rule", "?")] += 1
        return errs, warns, by_rule, by_shot, js
    txt = b / "lint.txt"
    if not txt.exists():
        return None
    for ln in txt.read_text().splitlines():
        m = RULE.search(ln)
        if not m:
            continue
        rest = ln[m.end():].strip().lower()
        if (m.group(2) or "").lower() == "ok":
            continue
        if" ok" in " " + rest[:3] or rest.startswith("ok") or "waived" in rest[:8]:
            continue
        if "warning" in rest[:10] or (m.group(2) or "").lower().startswith("warn"):
            warns += 1
        else:
            errs += 1
            sm = re.search(r":\s*([a-z]+\d+[a-z]?)\s+\+\d+", ln)
            if sm:
                by_shot[sm.group(1)] += 1
        by_rule[m.group(1)] += 1
    return errs, warns, by_rule, by_shot, txt


def shot_state(tl, s, masters, lint_shots):
    if not s.set or s.set_kind == "slug" or s.raw.get("slug"):
        return "slug"
    lines = [ln for ln in tl.lines if ln.shot == s.id and not ln.walla]
    if any(ln.provisional for ln in lines):
        return "staged"
    if masters.path(s.id) is not None and not lint_shots.get(s.id):
        return "final"
    return "voiced"


def status(tl, masters_dir=None):
    ep = tl.episode
    masters = Masters(tl, masters_dir)
    lints = read_lints(ep)
    lint_shots = lints[3] if lints else {}
    states = Counter()
    frames = Counter()
    rendered = 0
    for s in tl.shots:
        st = shot_state(tl, s, masters, lint_shots)
        states[st] += 1
        frames[st] += s.len
        rendered += masters.path(s.id) is not None
    out = ['%s "%s": %d shots, %d f (%s), %d scenes, %d lines' % (
        ep, tl.title, len(tl.shots), tl.length, tc(tl.length)[:8], len(tl.scenes()), len(tl.lines))]
    out.append("  %-8s %6s %8s" % ("state", "shots", "frames"))
    for st in ("slug", "staged", "voiced", "final"):
        out.append("  %-8s %6d %8d" % (st, states[st], frames[st]))
    out.append("  render cache: %d/%d shots have a master" % (rendered, len(tl.shots)))
    if lints:
        e, w, by_rule, _, src = lints
        top = ", ".join("%s %d" % kv for kv in by_rule.most_common(8))
        out.append("  lints (%s): %d errors, %d warnings%s" % (src.name, e, w, ("; " + top) if top else ""))
    else:
        out.append("  lints: none yet (run `show lint %s`)" % ep)
    tts = [ln for ln in tl.lines if ln.tts and not ln.walla]
    prov = [ln for ln in tl.lines if ln.provisional]
    out.append("  lines: %d TEMP TTS, %d provisional (no take locked)" % (len(tts), len(prov)))
    dd = ep_dir(ep) / "deliveries"
    if dd.exists():
        try:
            from ..standards.signoff import check as signoff_check
        except ImportError:
            signoff_check = None
        for man in sorted(dd.glob("*/manifest.json")):
            m = json.loads(man.read_text())
            so = signoff_check(ep, m["render_hash"])[0] if signoff_check else "?"
            out.append("  delivery %-18s %d f  render %s  sign-off: %s" % (
                man.parent.name, m.get("n_frames", 0), m["render_hash"][:12], so))
    return "\n".join(out), {"states": dict(states), "rendered": rendered, "shots": len(tl.shots)}
