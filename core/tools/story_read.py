#!/usr/bin/env python3
"""story_read.py - the reading script: a pack's conversations as a reviewer
reads them (P0; docs/STORY_ENGINE.md "Script tools").

    python3 core/tools/story_read.py PACK [PACK ...] [--ops] [--data DIR]

For every script (and beat script) of the pack, in file order: the speaker's
name (cast names and roster ids resolved), the expression, the line with '/'
as a line break and '//' as a new box, the line tags ([HEART], [PUNCH], ...)
and the typewriter codes as written ({d}, {..}, ...), choices with the label
each option jumps to, labels where the flow can land, and the @mission
header (title, hint, trophy). Other ops are listed in brackets so the timing
and the hand-offs stay visible; --ops shows every one of them, the default
shows only the ones a reader needs (waits, beats, pushes, flags, quests,
calls, laters, rewards, music and sfx).

It reads the .dsc sources through textpack.py's compiler (so a pack that does
not compile is reported, not guessed at). Nothing is written.
"""
import os
import re
import signal
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import textpack as tp  # noqa: E402

SHOWN = ("beat", "push", "setf", "clrf", "jif", "jifn", "jlt", "jge", "jmp", "goto", "quest", "call",
         "later", "reward", "give", "take", "music", "sfx", "possess", "unpossess", "verb", "native",
         "runbeat", "stash", "befriend", "time", "rng", "end", "ret", "freeze_cap", "insert", "cut")
CAST = {"nar": "(narrator)", "narrator": "(narrator)", "target": "(the subject)", "me": "@P", "player": "@P",
        "mayor": "MAYOR GIGGS", "nana": "NANA PUDDIN", "rivet": "RIVET", "pip": "PIP", "fuzz": "FATHER FUZZ",
        "bloom": "DR. BLOOM", "vex": "VEX", "vapula": "VAPULA", "steamy": "STEAMY", "lulla": "MISS LULLA",
        "hattie": "HATTIE HAHA", "grampy": "GRAMPY BEAN", "sootkin": "SOOTKIN", "anvil": "ANVIL",
        "smog": "LADY SMOG", "nox": "MOTHER NOX", "tallyworn": "TALLYWORN", "haze": "HAZE",
        "grumble": "GRUMBLEGUTS"}


def roster_names():
    path = os.path.join(tp.CORE, "gen", "roster_data.c")
    names = {}
    if os.path.exists(path):
        for m in re.finditer(r"/\*\s*(\d+) \*/ \{ \{[^}]*\}, \"((?:[^\"\\]|\\.)*)\"", open(path).read()):
            names[int(m.group(1))] = m.group(2)
    return names


class Reader:
    def __init__(self, comp, names):
        self.comp = comp
        self.names = names

    def who(self, w):
        lw = w.lower()
        if lw in CAST:
            base = CAST[lw]
        else:
            base = None
        try:
            rid = self.comp.who(w, "?")
        except tp.Fail:
            return w.upper()
        if base and not base.startswith(("(", "@")) and rid in self.names:
            return self.names[rid].upper()
        if base:
            return base
        if rid in self.names:
            return "%s (%d)" % (self.names[rid].upper(), rid)
        return w.upper()

    def text(self, s, indent):
        boxes = s.split("//")
        out = []
        for bi, box in enumerate(boxes):
            lines = [l.strip() for l in box.split("/")]
            for li, l in enumerate(lines):
                out.append(("" if bi == 0 and li == 0 else " " * indent) + l)
            if bi + 1 < len(boxes):
                out.append(" " * indent + "--- (next box)")
        return "\n".join(out)

    def script(self, name, body, where):
        print("=== script %s  (%s) ===" % (name, where))
        pend_expr = None
        for w, raw in body:
            line = tp.strip_comment(raw).strip()
            if not line:
                continue
            if re.match(r"^[A-Za-z_]\w*:$", line):
                print("%s" % line)
                continue
            b = tp.BRACKET.match(line)
            if b:
                code = b.group(1).strip()
                if code.upper() in tp.EXPRS:
                    pend_expr = code.upper()
                    continue
                if code.upper().startswith("EXPR "):
                    pend_expr = code.split()[1].upper()
                    continue
                print("    [%s]" % code)
                continue
            tk = tp.tokens(line)
            cmd = tk[0][1].lower()
            if cmd == "say":
                words = [t for t in tk[1:]]
                spk = self.who(words[0][1])
                k = 1
                expr = pend_expr
                pend_expr = None
                if k < len(words) and words[k][0] == "word":
                    expr = words[k][1].upper()
                    k += 1
                txt = words[k][1] if k < len(words) else ""
                tags = " ".join(t[1].upper() if t[1].startswith("[") else "[%s]" % t[1].upper()
                                for t in words[k + 1:])
                head = "%s%s: " % (spk, " [%s]" % expr if expr and expr != "NEUTRAL" else "")
                print(head + self.text(txt, len(head)) + ("   " + tags if tags else ""))
                continue
            if cmd in ("choice", "player_line"):
                m = re.match(r'^\S+\s+(\S+)\s+("(?:[^"\\]|\\.)*")\s*\{(.*)\}\s*$', line)
                if not m:
                    print("    [%s]" % line)
                    continue
                spk = self.who(m.group(1))
                q = tp.tokens(m.group(2))[0][1]
                head = "%s%s: " % ("PLAYER LINE " if cmd == "player_line" else "", spk)
                print(head + self.text(q, len(head)))
                for n, part in enumerate(p.strip() for p in m.group(3).split(";") if p.strip()):
                    m3 = re.match(r'^("(?:[^"\\]|\\.)*")\s*(?:->\s*(\w+))?$', part)
                    if m3:
                        o = tp.tokens(m3.group(1))[0][1]
                        print("    %d. \"%s\"%s" % (n + 1, o, (" -> %s" % m3.group(2)) if m3.group(2) else " (falls through)"))
                continue
            if self.all_ops or cmd in SHOWN:
                print("    [%s]" % line)

    def beat(self, name, body, where):
        print("=== beatscript %s  (%s) ===" % (name, where))
        for w, raw in body:
            line = tp.strip_comment(raw).strip()
            if not line:
                continue
            m = re.match(r"^\+(\d+)\s+(.*)$", line)
            if not m:
                print(line)
                continue
            for stmt in m.group(2).split(";"):
                stmt = stmt.strip()
                tk = tp.tokens(stmt) if stmt else []
                if not tk:
                    continue
                cmd = tk[0][1].lower()
                strs = [t[1] for t in tk[1:] if t[0] == "str"]
                if cmd in ("say", "caption") and strs:
                    spk = self.who(tk[1][1]) if cmd == "say" else "CAPTION"
                    head = "+%-3s %s: " % (m.group(1), spk)
                    print(head + self.text(strs[0], len(head)))
                elif self.all_ops:
                    print("    +%s [%s]" % (m.group(1), stmt))


def main(argv):
    packs, all_ops = [], False
    i = 0
    while i < len(argv):
        if argv[i] == "--ops":
            all_ops = True
            i += 1
        elif argv[i] == "--data":
            tp.DATA = os.path.abspath(argv[i + 1])
            i += 2
        elif argv[i].startswith("-"):
            print(__doc__)
            return 2
        else:
            packs.append(argv[i])
            i += 1
    if not packs:
        print(__doc__)
        return 2
    comp = tp.Compiler(tp.load_symbols())
    files = []
    for d, _, fs in os.walk(tp.DATA):
        for f in fs:
            if f.endswith(".dsc"):
                files.append(os.path.join(d, f))
    for f in sorted(files):
        comp.read(f)
    comp.build()
    r = Reader(comp, roster_names())
    r.all_ops = all_ops
    by = {p.name: p for p in comp.packs}
    status = 0
    for name in packs:
        p = by.get(name)
        if p is None:
            print("story_read: no pack %s (packs: %s)" % (name, " ".join(sorted(by))), file=sys.stderr)
            status = 1
            continue
        print("##### pack %s (%s, chapter %d, %d min) #####" % (p.name, p.kind, p.chapter, p.minutes))
        for where, a, pk in comp.missions:
            if pk is p:
                print("@mission %s giver=%s offer=%s active=%s" % (a.get("id"), a.get("giver", "?"),
                                                                 a.get("offer", "-"), a.get("active", "-")))
                for k in ("title", "hint", "trophy"):
                    if a.get(k):
                        print("    %-6s %s" % (k + ":", a[k]))
        for nm, text, where in p.named:
            print("string %s: %s" % (nm, r.text(text, 9 + len(nm))))
        for s in p.scripts:
            if s[0].startswith("__"):
                continue
            print()
            r.script(s[0], s[1], s[3])
        for b in p.beats:
            print()
            r.beat(b[0], b[1], b[3])
        print()
    return status


if __name__ == "__main__":
    signal.signal(signal.SIGPIPE, signal.SIG_DFL)
    try:
        sys.exit(main(sys.argv[1:]))
    except tp.Fail as e:
        print("story_read: error: %s" % e, file=sys.stderr)
        sys.exit(1)
