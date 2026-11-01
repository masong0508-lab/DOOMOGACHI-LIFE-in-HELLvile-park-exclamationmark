#!/usr/bin/env python3
"""story_register.py - the PG-21 register lint (E5;
docs/design/story/SCRIPT_STYLE.md §5, §5b, §12; docs/STORY_ENGINE.md
"Script tools").

The register is PG-21 (adult comedy, implied and fade-to-black), inside
hard limits. This tool machine-checks the limit that matters most before
any adult text is written: **nothing adult near a kid** (rule R3).

    python3 core/tools/story_register.py [--data DIR] [--quiet]
    python3 core/tools/story_register.py --selftest [--data DIR]

It compiles every core/data/story/**/*.dsc the way textpack.py does
(nothing is written to core/gen), then checks each script (a
conversation) together with the beat scripts it runs, and every beat
script on its own as well (C code runs the cutscene beat scripts directly,
P0's p0_egg, the F2-F4 stages, the summit, E1, with no script's runbeat
in between):

  K1  a script whose cast includes a kid has no register word
      (core/data/story/register.tsv: strong / crude / sexual) in any of
      its lines, choice questions, options or beat lines (the pet bars the
      sexual class only);
  K2  ... and no LOVE expression (a `say WHO LOVE`, a beat `expr N LOVE`
      or a `[LOVE]` / `[EXPR LOVE]` bracket) unless the only kid is the pet;
  K3  ... and no mature-guarded branch (`jmat` / `jmatn`, E0's "Mature
      content is on" jump): a kids' scene has no mature variant at all,
      and neither does a scene the pet is in;
  K4  every KID citizen a script names is on kid_cast.txt, so the story's
      kid cast is one list (LS0's romance test and E1's adult gate read it);
  S1  every @scene glob of kid_cast.txt matches a script, beat script or
      pack (not in --selftest: the seeded tree holds few scripts);
  C1  game-wide caps from register.tsv's cap column, over the English
      packs (the full f-word once, the s-word a few times; DECISIONS 23).

"The cast" of a script is every resident it names: the speakers of say,
choice and player_line, the residents of npc / cast / befriend / possess /
verb, the npc / say / headturn rows of every beatscript it runs, the
giver of every @mission whose offer= or active= is this script, and the
pet whenever a line is the pet's ("beep." / "bep?") or names it (@E).
"A kid" is a premade citizen with the KID or TEEN trait in
core/data/roster/ (the roster's source of truth), an entry of
core/data/story/kid_cast.txt (the story's kid cast and the pet), or any
citizen in a script that kid_cast.txt marks as a kids' scene (@scene: a
daycare of unnamed children, a child's crayon flashback).

--selftest compiles the seeded bad tree core/tests/story_register/ (or
--data DIR) and passes only when every line marked `# expect: RULE` is
reported with that rule and nothing else is: the lint must fail on it.
It also feeds K3 and S1 a scene and a stale glob directly, and runs the
matcher over the same cases as test_story_register.c (hyphenated words,
cut-off dashes, "the damned" that is not an oath).

Exit status: 0 clean, 1 findings (or a failed self test), 2 usage.
"""
import fnmatch
import os
import re
import signal
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import textpack as tp  # noqa: E402

ROSTER_DIR = os.path.join(tp.ROOT, "core", "data", "roster")
REGISTER = os.path.join(tp.ROOT, "core", "data", "story", "register.tsv")
KID_CAST = os.path.join(tp.ROOT, "core", "data", "story", "kid_cast.txt")
SELFTEST = os.path.join(tp.ROOT, "core", "tests", "story_register")
CLASSES = ("strong", "crude", "sexual")
PET = -2                                  # the pet in a cast set (not a resident id)
PET_LINE = re.compile(r"^\(?\s*(beep|bep)\b[^a-z]*\)?$", re.I)
# commands whose first word is a resident (textpack.py compile_script / compile_beat)
WHO_CMDS = ("say", "choice", "player_line", "befriend", "possess", "unpossess", "cast", "npc", "headturn")
JMAT = ("jmat", "jmatn")                  # E0's mature-guarded jumps (K3)


# A word character, for the matcher: a letter, digit or _, or a hyphen that
# joins two words ("fuck-up", "mother-fucker", "piss-poor"). A hyphen with
# no word character after it is the dash of a cut-off ("Shit- I mean",
# "Oh, fuck-") and ends the word. test_story_register.c's wordch_at() is
# the same rule.
WCH = r"(?:\w|-(?=\w))"


class Word:
    def __init__(self, text, cls, cap, note, where):
        self.text, self.cls, self.cap, self.note, self.where = text, cls, cap, note, where
        body = re.escape(text.strip("*")).replace(r"\ ", r"[\s/]+")
        head = WCH + "*" if text.startswith("*") else ""
        tail = WCH + "*" if text.endswith("*") else ""
        self.rx = re.compile(r"(?<![\w-])" + head + body + tail + r"(?!\w|-\w)", re.I)


def load_register(path=REGISTER):
    """register.tsv: WORD<TAB>CLASS[<TAB>CAP][<TAB># note]; a leading or
    trailing * matches any beginning or ending"""
    out = []
    if not os.path.exists(path):
        raise tp.Fail("%s: missing (the register word list)" % os.path.relpath(path, tp.ROOT))
    for i, raw in enumerate(open(path, encoding="utf-8").read().split("\n")):
        where = "%s:%d" % (os.path.relpath(path, tp.ROOT), i + 1)
        if not raw.strip() or raw.lstrip().startswith("#"):
            continue
        cols = raw.split("\t")
        note = ""
        if cols and cols[-1].lstrip().startswith("#"):
            note = cols.pop().lstrip("# ").strip()
        cols = [c.strip() for c in cols]
        if len(cols) not in (2, 3) or cols[1] not in CLASSES:
            tp.die(where, "expected WORD<TAB>strong|crude|sexual[<TAB>CAP][<TAB># note]")
        if not re.match(r"^\*?[a-z][a-z' -]*\*?$", cols[0]):
            tp.die(where, "a register word is lowercase letters, spaces, ' and - (and a leading or trailing *)")
        cap = None
        if len(cols) == 3 and cols[2] != "":
            if not cols[2].isdigit():
                tp.die(where, "CAP is a number (uses in the whole game)")
            cap = int(cols[2])
        if any(w.text == cols[0] for w in out):
            tp.die(where, "%s listed twice" % cols[0])
        out.append(Word(cols[0], cols[1], cap, note, where))
    return out


roster_traits = {}


def roster_kids():
    """{id: name} of the premade citizens with the KID or TEEN trait, {id: name}"""
    kids, names, cur = {}, {}, None
    for f in sorted(os.listdir(ROSTER_DIR)):
        if not f.endswith(".txt") or f == "edges.txt":
            continue
        for line in open(os.path.join(ROSTER_DIR, f), encoding="utf-8"):
            m = re.match(r"^\[(\d+)\]\s+(.*\S)", line)
            if m:
                cur = int(m.group(1))
                names[cur] = m.group(2)
                continue
            m = re.search(r"(?:^|\s)traits=(\S+)", line.split("#", 1)[0])
            if m and cur is not None:
                roster_traits[cur] = m.group(1)
                if re.search(r"\b(KID|TEEN)\b", m.group(1)):
                    kids[cur] = names[cur]
    return kids, names


class KidCast:
    """kid_cast.txt: ID<TAB>Name, @E<TAB>the pet, @scene<TAB>GLOB; '#' comments"""
    def __init__(self, names, path=KID_CAST):
        self.ids, self.scenes, self.pet = {}, [], False
        if not os.path.exists(path):
            raise tp.Fail("%s: missing (the kid-coded cast)" % os.path.relpath(path, tp.ROOT))
        for i, raw in enumerate(open(path, encoding="utf-8").read().split("\n")):
            where = "%s:%d" % (os.path.relpath(path, tp.ROOT), i + 1)
            line = raw.split("#", 1)[0].strip()
            if not line:
                continue
            cols = [c.strip() for c in line.split("\t") if c.strip()]
            if cols[0] == "@E":
                self.pet = True
                continue
            if cols[0] == "@scene":
                if len(cols) != 2 or not re.match(r"^[a-z0-9_*?]+$", cols[1]):
                    tp.die(where, "@scene<TAB>GLOB (a script or pack name; * and ? wildcards)")
                self.scenes.append((cols[1], where))
                continue
            if not cols[0].isdigit() or len(cols) != 2:
                tp.die(where, "expected ID<TAB>Name, @E<TAB>the pet or @scene<TAB>GLOB")
            rid = int(cols[0])
            if names.get(rid) != cols[1].replace("_", " "):
                tp.die(where, "roster id %d is %r, not %r" % (rid, names.get(rid), cols[1]))
            if rid in self.ids:
                tp.die(where, "%d listed twice" % rid)
            self.ids[rid] = cols[1]

    def scene(self, sc):
        """the @scene entry that makes sc a kids' scene, or None"""
        for g, where in self.scenes:
            if fnmatch.fnmatchcase(sc.name, g) or fnmatch.fnmatchcase(sc.pack.name, g):
                return g
        return None


class Scene:
    """one script (or one beat script) and what it pulls in"""
    def __init__(self, pack, name, where, kind="script"):
        self.pack, self.name, self.where, self.kind = pack, name, where, kind
        self.cast = {}            # id -> first where
        self.lines = []           # (where, text)
        self.love = []            # where
        self.jmat = []            # where


def _who(comp, w, where):
    try:
        return comp.who(w, where)
    except tp.Fail:
        return None


def _scan(comp, sc, body, beats, seen_beats, beat_body=False):
    for where, raw in body:
        line = tp.strip_comment(raw).strip()
        if not line or re.match(r"^[A-Za-z_]\w*:$", line):
            continue
        b = tp.BRACKET.match(line)
        if b:
            if re.sub(r"\s+", " ", b.group(1).strip().upper()) in ("LOVE", "EXPR LOVE"):
                sc.love.append(where)
            continue
        if beat_body and re.match(r"^\+\d+\s", line):
            line = line.split(None, 1)[1] if len(line.split(None, 1)) > 1 else ""
        for part in (line.split(";") if beat_body else [line]):
            tk = tp.tokens(part.strip())
            if not tk:
                continue
            cmd = tk[0][1].lower()
            words = [t[1] for t in tk[1:] if t[0] == "word"]
            strs = [t[1] for t in tk if t[0] == "str"]
            if cmd == "choice" or cmd == "player_line":
                m2 = re.match(r'^\S+\s+(\S+)', part.strip())
                words = [m2.group(1)] if m2 else words
                strs = re.findall(r'"((?:[^"\\]|\\.)*)"', part)
            if cmd in WHO_CMDS and words:
                rid = _who(comp, words[0], where)
                if rid is not None and 0 <= rid < 1000:
                    sc.cast.setdefault(rid, where)
            if cmd == "verb":
                for w in words[1:]:
                    if "=" not in w and w != "fragile":
                        rid = _who(comp, w, where)
                        if rid is not None and 0 <= rid < 1000:
                            sc.cast.setdefault(rid, where)
            if cmd in ("say", "expr") and [w.upper() for w in words[1:2]] == ["LOVE"]:
                sc.love.append(where)
            if cmd in JMAT:
                sc.jmat.append(where)
            if cmd == "runbeat" and words and words[0] in beats and words[0] not in seen_beats:
                seen_beats.add(words[0])
                _scan(comp, sc, beats[words[0]], beats, seen_beats, beat_body=True)
            for s in strs:
                sc.lines.append((where, s))
                v = tp.visible(s)
                if "@E" in s or PET_LINE.match(v.strip()):
                    sc.cast.setdefault(PET, where)


def scenes(comp):
    """every script and every beat script of every pack as a Scene (after
    comp.build()). A script is scanned with the beat scripts it runs; every
    beat script is ALSO scanned on its own, because C code runs many of them
    directly (the cutscene stages: p0_egg, b_f2, b_summit, b_e1...) and no
    script's runbeat ever reaches those. A beat script's cast is its npc /
    say / headturn rows and the pet when a line is the pet's."""
    beats = {}
    for p in comp.packs:
        for b in p.beats:
            beats[b[0]] = b[1]
    givers = {}
    for where, a, _ in comp.missions:
        g = a.get("giver")
        rid = _who(comp, g, where) if g else None
        for k in ("offer", "active"):
            if a.get(k) and rid is not None:
                givers.setdefault(a[k], []).append((rid, where))
    out = []
    for p in comp.packs:
        if p.name == "vmtest":
            continue
        for s in p.scripts:
            if s[0].startswith("__none_"):
                continue
            sc = Scene(p, s[0], s[3])
            _scan(comp, sc, s[1], beats, set())
            for rid, where in givers.get(s[0], []):
                if 0 <= rid < 1000:
                    sc.cast.setdefault(rid, where)
            out.append(sc)
        for b in p.beats:
            sc = Scene(p, b[0], b[3], "beatscript")
            _scan(comp, sc, b[1], beats, {b[0]}, beat_body=True)
            out.append(sc)
    return out


def stale_scenes(scs, kc):
    """S1: every @scene glob of kid_cast.txt matches at least one script, beat
    script or pack, so a script renamed in the rewrite cannot quietly lose its
    kids'-scene protection"""
    out = []
    for g, where in kc.scenes:
        if not any(fnmatch.fnmatchcase(sc.name, g) or fnmatch.fnmatchcase(sc.pack.name, g) for sc in scs):
            out.append((where, "S1", "@scene %s matches no script, beat script or pack: rename it with the "
                        "scene it protects (never drop a kids' scene to make a line pass)" % g))
    return out


def hits(words, text):
    v = tp.visible(text).replace("@P", "").replace("@E", "")
    return [(w, m.group(0)) for w in words for m in w.rx.finditer(v)]


def lint(comp, words, roster, kc, globs=True):
    """[(where, rule, message)]; roster = {id: name} of KID/TEEN citizens;
    globs: check kid_cast.txt's @scene globs against this tree (S1; off for
    a seeded tree, which holds only a few of the story's scripts)"""
    scs = scenes(comp)
    return lint_scenes(scs, words, roster, kc) + (stale_scenes(scs, kc) if globs else []) + caps(comp, words)


def caps(comp, words):
    """C1 over every pack's unique strings (lines, choices, captions, quest
    text, titles: everything a player reads), as story_stats.py counts them"""
    out, totals = [], {}
    for p in comp.packs:
        if p.name == "vmtest":
            continue
        for s in tp.pack_keys(p):
            for w, _ in hits(words, s):
                totals.setdefault(w.text, []).append(p.wheres.get(s, p.path))
    for w in words:
        n = len(totals.get(w.text, []))
        if w.cap is not None and n > w.cap:
            out.append((totals[w.text][w.cap], "C1", "%r used %d times in the game (cap %d, %s)" %
                        (w.text, n, w.cap, w.where)))
    return out


def lint_scenes(scs, words, roster, kc):
    out = []
    for sc in scs:
        kid = sorted(k for k in sc.cast if k in roster or k in kc.ids)
        glob = kc.scene(sc)
        pet = kc.pet and PET in sc.cast
        what = "%s %s" % (sc.kind, sc.name)
        who = "whose cast includes " + ", ".join("%s (%d)" % (roster.get(k) or kc.ids[k], k) for k in kid[:3])
        if glob:
            who = "%s kid_cast.txt marks a kids' scene (@scene %s)" % ("which" if not kid else who + ", and", glob)
        for k in kid:
            if k in roster and k not in kc.ids and "TEEN" not in roster_traits.get(k, ""):
                out.append((sc.cast[k], "K4", "%s (%d) is a KID citizen in %s: list them in "
                            "core/data/story/kid_cast.txt" % (roster[k], k, what)))
        kids = bool(kid) or bool(glob)
        for where, s in sc.lines:
            for w, got in hits(words, s):
                if kids:
                    out.append((where, "K1", "%s word %r in %s, %s" %
                                (w.cls, got, what, who)))
                elif pet and w.cls == "sexual":
                    out.append((where, "K1", "%s word %r in %s, which the pet is in (%s)" %
                                (w.cls, got, what, sc.cast[PET])))
        if kids:
            for where in sc.love:
                out.append((where, "K2", "LOVE expression in %s, %s" % (what, who)))
        if kids or pet:
            for where in sc.jmat:
                out.append((where, "K3", "mature-guarded branch in %s, %s" %
                            (what, who if kids else "which the pet is in")))
    # one finding per (line, rule, word): a beat line is seen from every
    # script that runs it and again on its own; the first message wins
    seen, uniq = set(), []
    for f in out:
        key = (f[0], f[1], f[2].split(" in ", 1)[0])
        if key not in seen:
            seen.add(key)
            uniq.append(f)
    return uniq


def counts(comp, words):
    """{pack: {class: n}} over each pack's unique strings (story_stats.py)"""
    out = {}
    for p in comp.packs:
        c = dict.fromkeys(CLASSES, 0)
        for s in tp.pack_keys(p):
            for w, _ in hits(words, s):
                c[w.cls] += 1
        out[p.name] = c
    return out


def compile_tree():
    comp = tp.Compiler(tp.load_symbols())
    files = []
    for d, _, fs in os.walk(tp.DATA):
        for f in fs:
            if f.endswith(".dsc"):
                files.append(os.path.join(d, f))
    for f in sorted(files):
        comp.read(f)
    comp.build()
    return comp


def expected(data):
    """the `# expect: RULE` markers of a seeded tree: {(relpath:line, RULE)}"""
    out = set()
    for d, _, fs in os.walk(data):
        for f in sorted(fs):
            if not f.endswith(".dsc"):
                continue
            path = os.path.join(d, f)
            for i, raw in enumerate(open(path, encoding="utf-8").read().split("\n")):
                m = re.search(r"#\s*expect:((?:\s+[A-Z]\d)+)", raw)
                for rule in (m.group(1).split() if m else []):
                    out.add(("%s:%d" % (os.path.relpath(path, tp.ROOT), i + 1), rule))
    return out


def main(argv):
    quiet = selftest = False
    i = 0
    while i < len(argv):
        if argv[i] == "--data":
            tp.DATA = os.path.abspath(argv[i + 1])
            i += 2
        elif argv[i] == "--quiet":
            quiet = True
            i += 1
        elif argv[i] == "--selftest":
            selftest = True
            i += 1
        else:
            print(__doc__)
            return 2
    story = os.path.join(tp.ROOT, "core", "data", "story")
    if selftest and tp.DATA == story:
        tp.DATA = SELFTEST
    words = load_register()
    roster, names = roster_kids()
    kc = KidCast(names)
    comp = compile_tree()
    found = lint(comp, words, roster, kc, globs=not selftest)
    if selftest:
        want = expected(tp.DATA)
        got = {(w, r) for w, r, _ in found}
        missing, extra = sorted(want - got), sorted(got - want)
        for w, r in missing:
            print("story_register: selftest: %s: expected %s, not reported" % (w, r))
        for w, r, msg in found:
            if (w, r) in extra:
                print("story_register: selftest: %s: unexpected %s: %s" % (w, r, msg))
        if missing or extra or not want:
            return 1
        # K3 is seeded in bad.dsc (rb_jmat) since E0 added the jmat op; the
        # scanner is also fed scenes directly (jmatn, the control case)
        k3 = []
        for body, cast_line in (([("k3:1", '  say pip HAPPY "Hi!"'), ("k3:2", "  jmat lewd")], True),
                                ([("k3:1", '  say nar "beep."'), ("k3:2", "  jmatn tame")], True),
                                ([("k3:1", '  say vex HAPPY "Hi!"'), ("k3:2", "  jmat lewd")], False)):
            sc = Scene(tp.Pack("k3", "side", {}, "k3"), "k3_scene", "k3:1")
            _scan(comp, sc, body, {}, set())
            got3 = [f for f in lint_scenes([sc], words, roster, kc) if f[1] == "K3"]
            if bool(got3) != cast_line:
                k3.append(body[0][1].strip())
        for b in k3:
            print("story_register: selftest: K3 wrong for a scene with %r and a jmat" % b)
        if k3:
            return 1
        # S1 needs the whole story tree, so feed it a stale glob directly
        stale = KidCast.__new__(KidCast)
        stale.ids, stale.pet = {}, True
        stale.scenes = [("e14_lulla", "kid_cast:1"), ("p0", "kid_cast:2"), ("rb_cut*", "kid_cast:3"),
                        ("zz_gone_*", "kid_cast:4")]      # a script, a pack, a beat script, nothing
        got1 = stale_scenes(scenes(comp), stale)
        if [f[0] for f in got1] != ["kid_cast:4"]:
            print("story_register: selftest: S1 wrong: %r" % (got1,))
            return 1
        # the matcher, the same cases as test_story_register.c's
        bad = []
        for text, want in (("Oh, for fuck's sake.", "fuck"), ("What a fuck-up.", "fuck-up"),
                           ("You mother-fucker!", "mother-fucker"), ("Oh, fuck-", "fuck"),
                           ("Shit- I mean, shoot.", "Shit"), ("Shit-faced again.", "Shit-faced"),
                           ("Piss-poor work.", "Piss-poor"), ("Damn- no.", "Damn"),
                           ("What the / hell?", "What the / hell"),
                           ("The damned souls sing.", None), ("Damnation Pit!", None),
                           ("The Hell Breach!", None), ("A first-class pass.", None),
                           ("Essex is lovely.", None), ("Twenty shots, wasted.", None)):
            got = [g for _, g in hits(words, text)]
            if (want is None and got) or (want is not None and want not in got):
                bad.append((text, want, got))
        for b in bad:
            print("story_register: selftest: matcher: %r: wanted %r, got %r" % b)
        if bad:
            return 1
        if not quiet:
            print("story_register: selftest: the seeded tree fails as marked (%d findings: %s)" %
                  (len(found), ", ".join("%s x%d" % (r, sum(1 for f in found if f[1] == r))
                                         for r in sorted({f[1] for f in found}))))
        return 0
    for where, rule, msg in found:
        print("%s: %s: %s" % (where, rule, msg))
    if not quiet or found:
        sc = scenes(comp)
        kidsc = [s for s in sc if kc.scene(s) or any(k in roster or k in kc.ids for k in s.cast)]
        nb = sum(1 for s in sc if s.kind == "beatscript")
        print("story_register: %d scripts and %d beat scripts, %d with a kid in the cast (%d beat scripts), "
              "%d with the pet; %d register words, %d kid-cast citizens, %d kids' scenes; %d finding(s)" %
              (len(sc) - nb, nb, len(kidsc), sum(1 for s in kidsc if s.kind == "beatscript"),
               sum(1 for s in sc if PET in s.cast), len(words), len(kc.ids), len(kc.scenes), len(found)),
              file=sys.stderr if found else sys.stdout)
    return 1 if found else 0


if __name__ == "__main__":
    signal.signal(signal.SIGPIPE, signal.SIG_DFL)
    try:
        sys.exit(main(sys.argv[1:]))
    except tp.Fail as e:
        print("story_register: error: %s" % e, file=sys.stderr)
        sys.exit(1)
