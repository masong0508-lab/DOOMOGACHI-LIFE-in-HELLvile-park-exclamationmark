#!/usr/bin/env python3
"""leveltext.py - lint the level text tables and the ids the maps use (owner: LEVELS).

    python3 tools/leveltext.py [FILE.txt ...]

With no FILE: every core/data/levels/*.txt. Format: core/data/levels/README.md.
Checks (also: a dark map has 2..4 `j` joke lines and its note text): ids unique and inside the level's range (1000 + 100 * (LV_ index - 7)), at most 2 lines of 34
characters, a clean alternate on every `adult` line, no banned word, and every text id that the
matching map's script (`caption N`, `bark N`, `choice V N ...`) uses exists in the table.
"""
import os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LEVELS = ["steam_room_panic", "the_undercroft", "fright_night", "glum_pipes", "the_wall", "boneyard_buffet",
          "hell_pits_highrise", "department_of_contracts", "ninth_cloud"]   # LV_ 7..15 (NG+ names are added by their packages)
MAXLINE, MAXLINES = 34, 2
# hard rules of CLAUDE.md as plain word checks on the shipped text (a human review still reads the dark levels)
BANNED = ["fuck", "shit", "bitch", "cunt", "nigg", "fagg", "retard", "rape", "suicide", "kill yourself", "kms"]
CHILD = re.compile(r"\b(kid|kids|child|children|boy|girl|baby|toddler|pip|wisp|bolt)\b", re.I)
HARM = re.compile(r"\b(die|dies|dead|hurt|kill|killed|burn|burns|drown|drowns|scream|screams)\b", re.I)


def level_range(name, path):
    n = None
    if name in LEVELS:
        n = 7 + LEVELS.index(name)
    return (1000 + 100 * (n - 7), 1099 + 100 * (n - 7)) if n is not None else None


def parse(path):
    rows, errs = {}, []
    for ln, raw in enumerate(open(path, encoding="utf-8"), 1):
        line = raw.rstrip("\n")
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        parts = [p.strip() for p in line.split("|")]
        m = re.match(r"^(\d+)((?:\s+[aj])*)$", parts[0])
        if len(parts) < 2 or not m:
            errs.append(f"{path}:{ln}: expected `ID | text`")
            continue
        i = int(m.group(1))
        if i in rows:
            errs.append(f"{path}:{ln}: id {i} twice")
        fl = m.group(2).split()
        rows[i] = dict(text=parts[1], alt=parts[2] if len(parts) > 2 else None, adult="a" in fl, joke="j" in fl, line=ln)
    return rows, errs


def map_path(name):
    """the built maps live in core/data/maps; levels not registered yet wait in core/data/maps_pending"""
    for d in ("core/data/maps", "core/data/maps_pending"):
        p = os.path.join(ROOT, d, name + ".txt")
        if os.path.exists(p):
            return p
    return os.path.join(ROOT, "core/data/maps", name + ".txt")


def map_header(mappath):
    """(dark, note id) of the map file"""
    dark, note = False, None
    if os.path.exists(mappath):
        for raw in open(mappath, encoding="utf-8"):
            w = raw.split("#")[0].split()
            if w[:2] == ["dark", "yes"]:
                dark = True
            if len(w) == 2 and w[0] == "note":
                note = int(w[1])
    return dark, note


def script_ids(mappath):
    ids = []
    if not os.path.exists(mappath):
        return ids
    for raw in open(mappath, encoding="utf-8"):
        s = raw.split("#")[0].strip()
        if not s.startswith("on "):
            continue
        for act in s.split(" do ", 1)[1].split(";"):
            w = act.split()
            if not w:
                continue
            if w[0] in ("caption", "bark") and len(w) > 1:
                ids.append(int(w[1]))
            if w[0] == "choice" and len(w) > 2:
                ids.append(int(w[2]))
    return ids


def check_text(i, text, where, errs):
    lines = text.replace("\\n", "\n").split("\n")
    if len(lines) > MAXLINES:
        errs.append(f"{where}: id {i} has {len(lines)} lines (max {MAXLINES})")
    for l in lines:
        if len(l) > MAXLINE:
            errs.append(f"{where}: id {i} line over {MAXLINE} characters: {l!r}")
    low = text.lower()
    for w in BANNED:
        if w in low:
            errs.append(f"{where}: id {i} has a banned word: {w}")
    if CHILD.search(text) and HARM.search(text):
        errs.append(f"{where}: id {i} puts a child word next to a harm word: {text!r} (review by hand)")


def main(argv):
    files = argv or sorted(os.path.join(ROOT, "core/data/levels", f)
                           for f in os.listdir(os.path.join(ROOT, "core/data/levels")) if f.endswith(".txt"))
    errs = []
    L2 = open(os.path.join(ROOT, "core/src/levels2.c"), encoding="utf-8").read()
    for f in files:
        name = os.path.splitext(os.path.basename(f))[0]
        rows, e = parse(f)
        errs += e
        dark, note = map_header(map_path(name))
        rng = level_range(name, f)
        for i, r in rows.items():
            where = f"{os.path.relpath(f, ROOT)}:{r['line']}"
            if rng and not (rng[0] <= i <= rng[1]):
                errs.append(f"{where}: id {i} outside the level's range {rng[0]}..{rng[1]}")
            if note is not None and i == note:      # the content note: one wrapped string, equal to lv_notes in levels2.c
                if r["text"] not in L2:
                    errs.append(f"{where}: the content note differs from core/src/levels2.c lv_notes")
                continue
            check_text(i, r["text"], where, errs)
            if r["adult"] and not r["alt"]:
                errs.append(f"{where}: id {i} is adult and has no clean alternate")
            if r["alt"]:
                check_text(i, r["alt"], where + " (alt)", errs)
        if rng and rng[1] not in rows:
            errs.append(f"{name}: id {rng[1]} (the card line) is missing")
        if dark:      # plan 6.2: two to four bleak or deadpan laughs (marked `j`), and the content note's text
            jokes = sum(1 for r in rows.values() if r["joke"])
            if not 2 <= jokes <= 4:
                errs.append(f"{name}: a dark level needs 2..4 joke lines (marked `ID j |`), it has {jokes}")
            if note is None or note not in rows:
                errs.append(f"{name}: the content note id {note} is not in the table")
        for i in script_ids(map_path(name)):
            if i not in rows:
                errs.append(f"{name}: the map's script uses text id {i}, not in the table")
        print(f"{'ok  ' if not errs else 'FAIL'} {name}: {len(rows)} lines")
    for e in errs:
        print("leveltext:", e, file=sys.stderr)
    return 1 if errs else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
