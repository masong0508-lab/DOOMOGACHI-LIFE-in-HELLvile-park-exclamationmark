#!/usr/bin/env python3
"""story_wayfind.py - the wayfinding and naming lint (clarity C7 + C16; part of
`make -C core story-check`).

C7: every story objective (the HUD pill: `string OBJ_*` and `string O<ep>_*`
in core/data/story/**/*.dsc) tells the player where to go. It must have
  * a parenthetical with a compass word: "(NW)", "(park, S)", "(W, @0/5)", or
  * a parenthetical with a landmark the player is standing at or has on the
    map from the start: "(here)", "(home)", "(plaza)", "(anywhere)", or
    "(arrow, ...)" when the goal arrow points at the next of several
    targets and the line must stay short (Ep 3.3 is drawn under the
    invaded map's hint toast: one line is cheaper on the GBA), or
  * the words "follow the arrow" (the objective has more than one target; the
    HUD compass points at the next one).
It must also fit the pill: at most two lines of 148 px at 240x160 (the
font advances 6 px; @0 counts as one digit, @E as the longest pet name).

C16: one name per thing in system text. The HUD and menus use the map's
plain place names; the Hillvile / Hellvile pun stays in dialogue. The
banned aliases below may not appear in an objective, nor in a string
literal of core/src/*.c (comments are skipped). Dialogue (`say` lines) is
not checked.

Report only (not a finding): the side-quest hints (`hint=` of an @mission,
`string Q<n>_HINT` of the 49 old quests) that name no direction, landmark
or place (a parenthetical, a story place or region name or a landmark word
anywhere in the line counts). They
are shown in the quest log and, after the story, in the HUD pill; the
side-mission rewrite gives them a "where" (the goal arrow already points
at a running verb's person or place).

  python3 core/tools/story_wayfind.py            # lint, exit 1 on a finding
  python3 core/tools/story_wayfind.py --selftest # the seeded bad inputs fail
"""
import pathlib
import re
import sys

CORE = pathlib.Path(__file__).resolve().parent.parent
STORY = CORE / "data" / "story"
SRC = CORE / "src"

COMPASS = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"}
LANDMARKS = {"here", "home", "plaza", "anywhere", "arrow"}   # "arrow": the goal arrow (several targets, one line)
ARROW = "follow the arrow"
OBJ_RE = re.compile(r'^string (OBJ_[A-Z0-9_]+|O[A-Z0-9]{2}_[A-Z0-9_]+) "(.*)"')
HINT_RE = re.compile(r'^string (Q[0-9]+_HINT) "(.*)"|^@mission (S[0-9]+) .*?\bhint="([^"]*)"')

# alias -> the canonical name (C16). Checked in objectives and in C string literals.
BANNED = {
    "Nana's bakery": "Brimstone Bites",
    "Hell Pits": "Hellvile Pits",
    "Hillvile flowers": "flowers (HaHaland)",
    "Hellvile Park - Hillvile Side": "The Park",
    "Stroll HaHaland": "Walk around HaHaland",
    "Fight demon waves": "Demon Arena",
    "Hellvile Arena": "Demon Arena",          # the story's door and region: the same arena as Free Play's
    "The Unity Stage": "Unity Stage",
    "The Lake of Fire": "Lake of Fire",
    "Doomogachi": "Boreagotchi",
}

PILL_W = 148          # plat_screen_w() - 92 at 240x160 (overworld_fx.c draw_objective)
ADV = 6               # FONT_ADVANCE
PET = "Abcdefg"       # the longest pet name (7 letters, town.h name[8])


def text_width(s):
    return len(s) * ADV - 1 if s else 0


def wrap_lines(s, w):
    """ui.c wrap_core: words packed greedily, a word wider than the line is cut."""
    lines, cur = 0, ""
    for word in s.split(" "):
        cand = word if not cur else cur + " " + word
        if text_width(cand) <= w:
            cur = cand
            continue
        if cur:
            lines += 1
        cur = word
        while text_width(cur) > w:
            lines += 1
            cur = cur[w // ADV:]
    return lines + (1 if cur else 0)


def expand(s):
    return s.replace("@0", "0").replace("@E", PET)


def has_direction(s):
    if ARROW in s:
        return True
    for group in re.findall(r"\(([^()]*)\)", s):
        items = {i.strip() for i in group.split(",")}
        if items & COMPASS or items & LANDMARKS:
            return True
    return False


def check_objective(name, text, where):
    out = []
    if not has_direction(text):
        out.append("%s: %s \"%s\" names no direction or landmark (C7)" % (where, name, text))
    n = wrap_lines(expand(text), PILL_W)
    if n > 2:
        out.append("%s: %s \"%s\" takes %d lines in the HUD pill (2 max at 240x160)" % (where, name, text, n))
    for bad, good in BANNED.items():
        if bad in text:
            out.append("%s: %s uses \"%s\"; say \"%s\" (C16)" % (where, name, bad, good))
    return out


def lint_story():
    out, count = [], 0
    for f in sorted(STORY.rglob("*.dsc")):
        for no, line in enumerate(f.read_text(encoding="utf-8").split("\n"), 1):
            m = OBJ_RE.match(line)
            if m:
                count += 1
                out += check_objective(m.group(1), m.group(2), "%s:%d" % (f.relative_to(CORE), no))
    return out, count


def place_names():
    """The story's place and region names (core/src/story_places.c), for the hint report."""
    src = (SRC / "story_places.c").read_text(encoding="utf-8")
    names = set()
    m = re.search(r"place_names\[PLACE_COUNT\] = \{(.*?)\};", src, re.S)
    if m:
        names |= {n for n in re.findall(r'"([^"\n]*)"', m.group(1)) if n}
    names |= set(re.findall(r'\{ MAPID_\w+, \d+, \d+, \d+, \d+, "([^"]+)" \}', src))
    return names


def hint_report():
    """(count of side hints, those that say where) - report only."""
    places = {n.lower() for n in place_names()}
    total, where = 0, 0
    for f in sorted(STORY.rglob("*.dsc")):
        for line in f.read_text(encoding="utf-8").split("\n"):
            m = HINT_RE.match(line)
            if not m:
                continue
            text = m.group(2) if m.group(1) else m.group(4)
            total += 1
            low = text.lower()
            # a compass word or landmark, any parenthetical ("(the shop)", "(west lawn)"), a story place or
            # region by name, or a common landmark word anywhere in the line
            if (has_direction(text) or re.search(r"\([^()]+\)", text) or any(p in low for p in places) or
                    re.search(r"\b(plaza|park|beach|pier|graveyard|fountain|wall|gate|market|lake|shop)\b", low)):
                where += 1
    return total, where


def strip_comments(src):
    src = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), src, flags=re.S)
    return re.sub(r"//[^\n]*", "", src)


def lint_sources():
    out = []
    lit = re.compile(r'"((?:[^"\\\n]|\\.)*)"')
    for f in sorted(SRC.glob("*.c")):
        code = strip_comments(f.read_text(encoding="utf-8", errors="replace"))
        for no, line in enumerate(code.split("\n"), 1):
            for s in lit.findall(line):
                for bad, good in BANNED.items():
                    if bad in s:
                        out.append("%s:%d: \"%s\" in system text; say \"%s\" (C16)" %
                                   (f.relative_to(CORE), no, bad, good))
    return out


def selftest():
    bad = [("OBJ_X", "Say hi to the Mayor"),                     # no direction
           ("OBJ_X", "Go home. (It's fine.)"),                   # a parenthetical, but no direction
           ("OBJ_X", "Get a cupcake at Nana's bakery (NW)"),     # banned alias
           ("OBJ_X", "Meet Steamy, Hattie, Grampy, Nana and Pip and all their friends (S)")]  # 3 lines
    good = [("OBJ_X", "Meet Steamy, Hattie and Grampy (park, S, @0/3)"),
            ("OBJ_X", "Find @E at the bottom (Sealed Gate, E)"),
            ("OBJ_X", "Find 3 odd things: follow the arrow (@0/3)"),
            ("OBJ_X", "Someone is knocking (home, @0/7)")]
    fails = 0
    for n, t in bad:
        if not check_objective(n, t, "selftest"):
            print("selftest: accepted a bad objective: %s" % t)
            fails += 1
    for n, t in good:
        r = check_objective(n, t, "selftest")
        if r:
            print("selftest: refused a good objective: %s" % r[0])
            fails += 1
    if fails:
        return 1
    print("story_wayfind: selftest OK (%d seeded bad objectives refused)" % len(bad))
    return 0


def main(argv):
    if "--selftest" in argv:
        return selftest()
    out, count = lint_story()
    out += lint_sources()
    for line in out:
        print(line)
    if out:
        print("story_wayfind: %d finding(s)" % len(out))
        return 1
    print("story_wayfind: %d objectives name a direction and fit the pill; no banned aliases" % count)
    total, where = hint_report()
    print("story_wayfind: report: %d of %d side-quest hints say where (the rest: the side-mission rewrite)" %
          (where, total))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
