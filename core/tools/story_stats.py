#!/usr/bin/env python3
"""story_stats.py - per-pack style metrics for the script rewrite (P0;
docs/design/story/SCRIPT_STYLE.md, the targets; docs/STORY_ENGINE.md
"Script tools"). Measure a pack before and after a rewrite and put the
table in the commit message (SCRIPT_STYLE.md rule M7).

    python3 core/tools/story_stats.py [PACK ...] [--csv FILE] [--data DIR]

No PACK means every pack. Counted over each pack's unique strings (the
lines, choice questions and options, captions and quest-log text a player
reads), like the scout's numbers in the plan:

  lines     unique strings
  words     visible words (codes, @-codes and '/' removed)
  w/line    average words per line
  paren%    narrator parentheticals: lines that start with '(' (target <= 25%)
  short%    lines of two words or fewer that are not choice options
  pause/l   {.} {..} {...} per line (target <= 0.25)
  omin%     lines with an ominous keyword (grape, tally, wall, ten years,
            counting, whisper, watch, stare, slowly, still, the other side, cold)
  deadpan   {d} codes in the pack
  heart     say lines tagged [HEART]; punch: tagged [PUNCH]
"""
import csv
import os
import re
import signal
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import textpack as tp  # noqa: E402

OMIN = re.compile(r"\b(grape|tally|wall|ten years|counting|whisper|watch|stare|slowly|still|the other side|cold)\b", re.I)
PAUSE = re.compile(r"\{(\.|\.\.|\.\.\.)\}")
COLS = ("pack", "lines", "words", "w/line", "paren%", "short%", "pause/l", "omin%", "deadpan", "heart", "punch")


def tags(comp):
    """say-line tags per pack: (#HEART, #PUNCH) from the sources"""
    out = {}
    for p in comp.packs:
        h = u = 0
        for s in p.scripts:
            for _, raw in s[1]:
                line = tp.strip_comment(raw)
                if line.strip().startswith("say "):
                    h += "[HEART]" in line.upper()
                    u += "[PUNCH]" in line.upper()
        out[p.name] = (h, u)
    return out


def measure(p, tg):
    strs = tp.pack_keys(p)
    n = len(strs)
    vis = [re.sub(r"@\w", "X", tp.visible(s)).replace("/", " ") for s in strs]
    words = [len(v.split()) for v in vis]
    short = sum(1 for s, w in zip(strs, words) if w <= 2 and s not in p.opts)
    paren = sum(1 for v in vis if v.strip().startswith("("))
    pause = sum(len(PAUSE.findall(s)) for s in strs)
    omin = sum(1 for v in vis if OMIN.search(v))
    dead = sum(s.count("{d}") for s in strs)
    pc = lambda k: 100.0 * k / n if n else 0.0
    return {"pack": p.name, "lines": n, "words": sum(words), "w/line": sum(words) / n if n else 0.0,
            "paren%": pc(paren), "short%": pc(short), "pause/l": pause / n if n else 0.0, "omin%": pc(omin),
            "deadpan": dead, "heart": tg[0], "punch": tg[1],
            "_raw": (n, sum(words), paren, short, pause, omin, dead, tg[0], tg[1])}


def row_text(r):
    return "%-9s %5d %6d %6.1f %6.0f%% %6.0f%% %7.2f %5.0f%% %7d %5d %5d" % (
        r["pack"], r["lines"], r["words"], r["w/line"], r["paren%"], r["short%"], r["pause/l"], r["omin%"],
        r["deadpan"], r["heart"], r["punch"])


def main(argv):
    names, csv_out = [], None
    i = 0
    while i < len(argv):
        if argv[i] == "--csv":
            csv_out = argv[i + 1]
            i += 2
        elif argv[i] == "--data":
            tp.DATA = os.path.abspath(argv[i + 1])
            i += 2
        elif argv[i].startswith("-"):
            print(__doc__)
            return 2
        else:
            names.append(argv[i])
            i += 1
    comp = tp.Compiler(tp.load_symbols())
    files = []
    for d, _, fs in os.walk(tp.DATA):
        for f in fs:
            if f.endswith(".dsc"):
                files.append(os.path.join(d, f))
    for f in sorted(files):
        comp.read(f)
    comp.build()
    tg = tags(comp)
    by = {p.name: p for p in comp.packs}
    for n in names:
        if n not in by:
            print("story_stats: no pack %s" % n, file=sys.stderr)
            return 1
    packs = [by[n] for n in names] if names else [p for p in comp.packs if p.name != "vmtest"]
    rows = [measure(p, tg[p.name]) for p in packs]
    print("%-9s %5s %6s %6s %7s %7s %7s %6s %7s %5s %5s" % COLS)
    for r in rows:
        print(row_text(r))
    if len(rows) > 1:
        t = [sum(r["_raw"][k] for r in rows) for k in range(9)]
        n = t[0] or 1
        tot = {"pack": "TOTAL", "lines": t[0], "words": t[1], "w/line": t[1] / n, "paren%": 100.0 * t[2] / n,
               "short%": 100.0 * t[3] / n, "pause/l": t[4] / n, "omin%": 100.0 * t[5] / n, "deadpan": t[6],
               "heart": t[7], "punch": t[8]}
        print(row_text(tot))
        rows.append(tot)
    if csv_out:
        with open(csv_out, "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(COLS)
            for r in rows:
                w.writerow([("%.2f" % r[c]) if isinstance(r[c], float) else r[c] for c in COLS])
    return 0


if __name__ == "__main__":
    signal.signal(signal.SIGPIPE, signal.SIG_DFL)
    try:
        sys.exit(main(sys.argv[1:]))
    except tp.Fail as e:
        print("story_stats: error: %s" % e, file=sys.stderr)
        sys.exit(1)
