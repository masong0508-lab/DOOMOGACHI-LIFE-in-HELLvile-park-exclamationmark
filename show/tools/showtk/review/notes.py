"""showtk.review.notes - the creator's notes, one Markdown file per review round
(WP13, sec.13.3): show/<ep>/notes/round-NN.md.

    show notes add ep101 "co02: the SUPER types too slowly" [--shot co02] [--new-round]
    show notes list ep101 [--open]
    show notes done ep101 2.1 --fix "co02 SUPER at 18 cps (a1b2c3d)"

A note is one line: "- [ ] 2.1 co02: text  (2026-10-26)"; done flips the box and
appends "-> fix".  Numbers are <round>.<n>, so a commit message can cite "note 2.1".
"""
import datetime
import re

from ..sidecars import ep_dir

NOTE = re.compile(r"^- \[( |x)\] (\d+)\.(\d+) (.*)$")


def _dir(ep):
    d = ep_dir(ep) / "notes"
    d.mkdir(parents=True, exist_ok=True)
    return d


def rounds(ep):
    d = _dir(ep)
    out = []
    for p in sorted(d.glob("round-*.md")):
        m = re.match(r"round-(\d+)\.md$", p.name)
        if m:
            out.append((int(m.group(1)), p))
    return out


def read(ep):
    """[(round, n, done, text, path, line_index)]"""
    out = []
    for r, p in rounds(ep):
        for i, ln in enumerate(p.read_text().splitlines()):
            m = NOTE.match(ln)
            if m:
                out.append((int(m.group(2)), int(m.group(3)), m.group(1) == "x", m.group(4), p, i))
    return out


def add(ep, text, shot=None, new_round=False, today=None):
    rs = rounds(ep)
    r = (rs[-1][0] + 1 if new_round else rs[-1][0]) if rs else 1
    p = _dir(ep) / ("round-%02d.md" % r)
    if not p.exists():
        p.write_text("# %s review round %02d\n\nThe creator's notes; Claude ticks each with its fix.\n\n" % (ep, r))
    n = 1 + max([x[1] for x in read(ep) if x[0] == r] or [0])
    today = today or datetime.date.today().isoformat()
    text = " ".join(str(text).split())
    if shot and not text.startswith(shot):
        text = "%s: %s" % (shot, text)
    with open(p, "a", encoding="utf-8") as f:
        f.write("- [ ] %d.%d %s  (%s)\n" % (r, n, text, today))
    return "%d.%d" % (r, n), p


def done(ep, ident, fix=""):
    r, n = (int(x) for x in str(ident).split("."))
    for rr, nn, is_done, text, p, i in read(ep):
        if (rr, nn) == (r, n):
            lines = p.read_text().splitlines()
            ln = lines[i].replace("- [ ]", "- [x]", 1)
            if fix:
                ln += "  -> " + " ".join(fix.split())
            lines[i] = ln
            p.write_text("\n".join(lines) + "\n")
            return p
    raise KeyError("no note %s in %s/notes" % (ident, ep))


def listing(ep, open_only=False):
    rows = []
    for r, n, d, text, p, _ in read(ep):
        if open_only and d:
            continue
        rows.append("%s %d.%d %s" % ("[x]" if d else "[ ]", r, n, text))
    return "\n".join(rows) if rows else "(no notes)"
