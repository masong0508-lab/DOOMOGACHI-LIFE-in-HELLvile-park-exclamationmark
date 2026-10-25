#!/usr/bin/env python3
# doomo-generator
"""textpack.py - the story content compiler (owner: S2,
docs/design/story/SPEC.md sec.5, 6.2, C3, C5, C12, C14; story_final.md
sec.15.2, 16.2-16.3; the language is documented in docs/STORY_ENGINE.md).

Reads every core/data/story/**/*.dsc and writes

    core/gen/text_<pack>.c   one const pack per @pack (GBA + native only;
                             the PS1 build excludes them, C3)
    core/gen/text_dir.c      the pack directory: txt_plat_pack() for ROM
                             platforms (the PS1 has ps1/src/txtload.c)
    core/gen/txt_dict.c      the shared dictionary and canonical Huffman
                             table (every platform; resident on the PS1)
    core/gen/side_table.c    the @mission rows (story_vm.h StorySideRow)
    core/gen/possess_table.c the @possessor rows (StoryPossessRow)
    core/gen/story_ids.h     PACK_* / SCR_* / BSCR_* / TXT_* ids for C code

and, with --bin FILE, the PS1 disc file TEXT.BIN (a build product, never
committed): a 2 KB directory sector, then every pack sector-aligned.

    python3 core/tools/textpack.py [--out-root DIR] [--bin TEXT.BIN]

Deterministic: the same inputs give byte-identical outputs (check-gen).
Every lint failure names the .dsc file and line; fix the input, never
core/gen/.

THE PACK (little endian, 4-aligned sections, offsets from the pack start)
    0  u16 magic 'TX', u8 version (1), u8 pack id
    4  u16 n_strings, n_scripts, n_ops, n_beats
   12  u16 n_events, u16 flags (1 = legacy text: no box lint; kind << 4)
   16  u32 off_strings, off_scripts, off_ops, off_beats, off_events, off_streams
   40  sections: u16 str_off[n] (into the streams) | StoryScript[n] (6 B:
       u16 first_op, u16 n_ops, u8 n_labels, u8 pack) | StoryOp[n] (4 B) |
       u16 beat {first_ev, n_ev}[n] | BeatEv[n] (8 B) | the Huffman streams
Ids: text id, script id and beat id = pack << 9 | index (txt.h R6).

THE CODEC (SF sec.16.3): symbols 0..255 are bytes, 256 + k is dictionary
token k (the most valuable words and phrases over all text, <= DICT_MAX);
one global canonical Huffman code (lengths <= 16) over the symbols; every
string is byte-aligned and ends with symbol 0.
"""
import heapq
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
CORE = os.path.join(ROOT, "core")
INC = os.path.join(CORE, "include", "doomo")
DATA = os.path.join(CORE, "data", "story")

PACK_BITS = 7
INDEX_BITS = 9
MAX_PACKS = 1 << PACK_BITS
MAX_INDEX = 1 << INDEX_BITS
KINDS = {"episode": 0, "common": 1, "side": 2, "feast": 3, "legacy": 4}
CAPS = {"episode": 12 * 1024, "common": 6 * 1024, "side": 8 * 1024, "feast": 10 * 1024, "legacy": 12 * 1024}
DICT_MAX = 256
MAX_CODE_LEN = 16
TXT_NONE = 0xFFFF
SECTOR = 2048
# the authored possessor rows exorcism.c's weak table holds (SPEC S3 M1):
# possess_table.c defines the strong table once .dsc blocks cover them all
POSSESS_ROWS_FULL = 12

# the dialogue box (ui_dialog.c): 240x160 -> 3 lines of 28 characters
BOX_LINES_240, BOX_CHARS_240 = 3, 28
OPT_CHARS = 27

# the line codes the typewriter knows (ui_dialog.c); anything else in {} fails
LINE_CODES = re.compile(r"\{(p \d+|\.|\.\.|\.\.\.|d|!|big|/|w|r|punch|cut \d+)\}")
TEN_YEARS_OK = ("ep41", "s139", "s179", "s187")

# DATA op tags (the a byte): what the inline operand is for
D_CMP, D_LINE, D_WHO, D_OPT, D_LABEL, D_UNLOCK, D_ARG, D_SCRIPT = 0, 1, 2, 3, 4, 5, 6, 7
# speaker bytes (OP_SAY a): 0..0xFB a resident id, else one of these
W_TARGET, W_BIG, W_NAR, W_PLAYER = 0xFC, 0xFD, 0xFE, 0xFF
# 16-bit "who" codes (DATA D_WHO, BOP_SAY a): 0..999 residents, 1000 the
# player (DOOMO_PLAYER_ID), these two specials
WHO_NAR, WHO_TARGET = 0xFFFF, 0xFFFD


class Fail(Exception):
    pass


def die(where, msg):
    raise Fail("%s: %s" % (where, msg))


# ---------------------------------------------------------------------------
# symbols from the C headers
# ---------------------------------------------------------------------------
HEADERS = [
    "types.h", "town.h", "roster.h", "items.h", "audio.h", "camfx.h", "beat.h", "portrait.h",
    "story.h", "story_vm.h", "map.h", "palette.h", "txt.h", "scenes.h", "tween.h", "unlocks.h",
    "unlock_ids.h", "home.h", "house_api.h", "exorcism.h",
]
EXTRA_HEADERS = [os.path.join(CORE, "src", "story_int.h"), os.path.join(CORE, "src", "overworld_int.h"),
                 os.path.join(CORE, "src", "story_ch12.h")]


def strip_comments(s):
    s = re.sub(r"/\*.*?\*/", " ", s, flags=re.S)
    return re.sub(r"//[^\n]*", " ", s)


# (S3) the Chapters 3-4 glue's constants (CB_C34, C34A_*, the cast's ids)
EXTRA_HEADERS.append(os.path.join(CORE, "src", "story_ch34.h"))


def c_eval(expr, syms):
    e = expr.strip()
    if not e:
        return None
    e = re.sub(r"\b(0x[0-9a-fA-F]+|\d+)[uUlL]*\b", lambda m: str(int(m.group(1), 0)), e)
    e = re.sub(r"\((u?int\d+_t|int|unsigned)\)", "", e)
    names = set(re.findall(r"[A-Za-z_]\w*", e))
    env = {}
    for n in names:
        if n not in syms:
            return None
        env[n] = syms[n]
    if re.search(r"[^\w\s\+\-\*/\(\)<>\|&]", e):
        return None
    try:
        v = eval(e.replace("/", "//"), {"__builtins__": {}}, env)
    except Exception:
        return None
    return int(v)


def load_symbols():
    syms = {}
    paths = [os.path.join(INC, h) for h in HEADERS] + EXTRA_HEADERS
    for h in paths:
        if not os.path.exists(h):
            continue
        src = strip_comments(open(h).read())
        for m in re.finditer(r"#define[ \t]+([A-Z_][A-Z0-9_]*)[ \t]+([^\n(][^\n]*)", src):
            v = c_eval(m.group(2), syms)
            if v is not None:
                syms[m.group(1)] = v
        for m in re.finditer(r"enum\s*\w*\s*\{(.*?)\}", src, flags=re.S):
            cur = -1
            for item in m.group(1).split(","):
                item = item.strip()
                if not item:
                    continue
                if "=" in item:
                    name, val = item.split("=", 1)
                    name = name.strip()
                    v = c_eval(val, syms)
                    cur = v if v is not None else cur + 1
                else:
                    name = item
                    cur += 1
                if re.match(r"^[A-Za-z_]\w*$", name):
                    syms[name] = cur
    return syms


# ---------------------------------------------------------------------------
# the .dsc language
# ---------------------------------------------------------------------------
WHO = {"nar": WHO_NAR, "narrator": WHO_NAR, "target": WHO_TARGET}
WHO_IDS = {"mayor": "ID_MAYOR", "nana": "ID_NANA", "rivet": "ID_RIVET", "pip": "ID_PIP", "fuzz": "ID_FUZZ",
           "bloom": "ID_BLOOM", "vex": "ID_VEX", "vapula": "ID_VAPULA", "steamy": "ID_STEAMY",
           "lulla": "ID_LULLA", "hattie": "ID_HATTIE", "grampy": "ID_GRAMPY", "sootkin": "ID_SOOTKIN",
           "anvil": "ID_ANVIL", "smog": "ID_LADY_SMOG", "nox": "ID_MOTHER_NOX",
           "tallyworn": "ID_TALLYWORN", "haze": "ID_HAZE", "grumble": "ROSTER_ID_GRUMBLEGUTS",
           "fizz": "69",
           # S2 content (Chapters 1-2): more of the cast by name (roster ids)
           "sandwichy": "4", "bonejangles": "5", "sprinklepop": "81", "ribbonetta": "83",
           "mascotty": "106", "cantor": "426"}
LINE_FLAGS = {"DEADPAN": 1, "PUNCH": 2, "PILL": 4, "SHOUT": 8, "HEART": 16, "GURGLE": 32}
PEND = {"creator": "PEND_CREATOR", "pet": "PEND_PET", "exo_practice": "PEND_EXO_PRACTICE",
        "exorcism": "PEND_EXORCISM", "workshop": "PEND_WORKSHOP", "combat": "PEND_COMBAT",
        "cutscene": "PEND_CUTSCENE", "credits": "PEND_CREDITS", "other": "PEND_OTHER",
        "home": "PEND_HOME", "cooking": "PEND_COOKING", "concert": "PEND_CONCERT", "dream": "PEND_DREAM",
        "timing": "PEND_TIMING", "minigame": "PEND_MINIGAME_SIDE"}
EXPRS = ("NEUTRAL", "HAPPY", "SAD", "ANGRY", "LOVE", "SHOCK", "SICK", "SLEEPY", "SURPRISE", "SMUG",
         "POSSESSED", "SCARED", "WINK")
# Bracket notation in scripts (SPEC sec.5, C14). The list is closed: any
# other [CODE] fails the build (docs/STORY_ENGINE.md).
BRACKET = re.compile(r"^\[(.*)\]$")

TOKEN = re.compile(r'"((?:[^"\\]|\\.)*)"|(\S+)')


def tokens(line):
    out = []
    for m in TOKEN.finditer(line):
        if m.group(1) is not None:
            out.append(("str", m.group(1).replace('\\"', '"').replace("\\\\", "\\")))
        else:
            out.append(("word", m.group(2)))
    return out


def strip_comment(raw):
    """drop a # comment that is not inside a quoted string"""
    out, q = [], False
    i = 0
    while i < len(raw):
        c = raw[i]
        if c == '"' and (i == 0 or raw[i - 1] != "\\"):
            q = not q
        if c == "#" and not q and (i == 0 or raw[i - 1] in " \t"):
            break
        out.append(c)
        i += 1
    return "".join(out).rstrip()


class Pack:
    def __init__(self, name, kind, attrs, path):
        self.name, self.kind, self.attrs, self.path = name, kind, attrs, path
        self.id = None
        self.strings = []
        self.str_index = {}
        self.named = []           # (NAME, text, where): string directives
        self.scripts = []         # [name, body, compiled]
        self.beats = []
        self.legacy = attrs.get("lint") == "legacy" or kind == "legacy"
        self.minutes = int(attrs.get("minutes", "0"))
        self.chapter = int(attrs.get("chapter", "0"))

    def text_new(self, s, where):
        """(S4) always a fresh string (no de-duplication): a mission's title
        and hint are a pair (hint = title + 1, story_vm.h StorySideRow)"""
        check_text(s, where, self)
        if len(self.strings) >= MAX_INDEX:
            die(where, "pack %s has more than %d strings" % (self.name, MAX_INDEX))
        self.strings.append(s)
        return (self.id << INDEX_BITS) | (len(self.strings) - 1)

    def text(self, s, where):
        check_text(s, where, self)
        if s not in self.str_index:
            if len(self.strings) >= MAX_INDEX:
                die(where, "pack %s has more than %d strings" % (self.name, MAX_INDEX))
            self.str_index[s] = len(self.strings)
            self.strings.append(s)
        return (self.id << INDEX_BITS) | self.str_index[s]


def visible(s):
    return LINE_CODES.sub("", s)


def check_text(s, where, pack):
    for m in re.finditer(r"\{[^}]*\}", s):
        if not LINE_CODES.fullmatch(m.group(0)):
            die(where, "unknown line code %s (the closed list is in docs/STORY_ENGINE.md)" % m.group(0))
    if re.search(r"\[[A-Z!][^\]]*\]", s):
        die(where, "bracket code inside a line: %r" % s)
    if "Ten years I" in s and not any(pack.name.startswith(k) for k in TEN_YEARS_OK):
        die(where, '"Ten years I" is only allowed in Ep 4.1, S139, S179 and S187')
    if any(ord(c) > 0x9F for c in s):
        die(where, "text is ASCII plus icon glyphs 0x80..0x9F")
    if len(visible(s).replace("//", "").replace("/", "")) > 199:
        die(where, "a line longer than 199 characters does not fit the dialogue entry")
    if not pack.legacy:
        for page in visible(s).split("//"):
            # ui_dialog.c parse_text: '/' is a line break and eats the spaces around it
            lines = wrap_lines(re.sub(r" */ *", "\n", page), BOX_CHARS_240)
            if lines > BOX_LINES_240:
                die(where, "a page needs %d lines at 240x160 (max %d x %d): %r" %
                    (lines, BOX_LINES_240, BOX_CHARS_240, page))


def wrap_lines(s, maxc):
    """the line count of ui.c's word wrap (fixed advance)"""
    n = 0
    p = s.strip()
    while p:
        k, last = 0, -1
        while k < len(p) and p[k] != "\n" and k < maxc:
            if p[k] == " ":
                last = k
            k += 1
        if k >= len(p) or p[k] == "\n":
            adv = k + (1 if k < len(p) else 0)
        elif p[k] == " ":
            adv = k + 1
        elif last > 0:
            adv = last + 1
        else:
            adv = k
        p = p[adv:].lstrip(" ")
        n += 1
    return n


def load_push_args():
    """core/data/story/push_args.txt (C12): KIND ARG lines, ARG may be a range a..b"""
    allowed = {}
    path = os.path.join(DATA, "push_args.txt")
    if not os.path.exists(path):
        return None
    for i, raw in enumerate(open(path).read().split("\n")):
        line = strip_comment(raw).strip()
        if not line:
            continue
        parts = line.split()
        if len(parts) != 2:
            die("core/data/story/push_args.txt:%d" % (i + 1), "expected KIND ARG")
        allowed.setdefault(parts[0], []).append(parts[1])
    return allowed


class Compiler:
    def __init__(self, syms):
        self.syms = syms
        self.packs = []
        self.script_names = {}
        self.beat_names = {}
        self.missions = []        # (where, attrs, pack)
        self.possessors = []
        self.armed = []           # @armed: when an authored possession is the story's
        self.spots = []           # (S4) the givers' cells (@mission at=), in first-use order
        self.push_args = load_push_args()

    def sym(self, name, where, prefix=None):
        if re.match(r"^-?\d+$", name) or re.match(r"^0x[0-9a-fA-F]+$", name):
            return int(name, 0)
        if prefix and not name.startswith(prefix) and prefix + name.upper() in self.syms:
            return self.syms[prefix + name.upper()]
        if prefix and not name.startswith(prefix) and prefix + name in self.syms:
            return self.syms[prefix + name]
        if name in self.syms:
            return self.syms[name]
        die(where, "unknown name %s" % name)

    def who(self, w, where):
        lw = w.lower()
        if lw in WHO:
            return WHO[lw]
        if lw in ("me", "player"):
            return self.syms["DOOMO_PLAYER_ID"]
        if lw in WHO_IDS:
            return self.sym(WHO_IDS[lw], where)
        return self.sym(w, where)

    # ---- reading ----
    def read(self, path):
        rel = os.path.relpath(path, ROOT)
        lines = open(path).read().split("\n")
        pack = None
        i = 0

        def block(i):
            body = []
            while i < len(lines):
                l2 = lines[i]
                i += 1
                if l2.strip() == "end" and not l2.startswith((" ", "\t")):
                    return body, i
                body.append(("%s:%d" % (rel, i), l2))
            die("%s:%d" % (rel, i), "missing 'end' (a block ends with 'end' in column 0)")
        while i < len(lines):
            raw = lines[i]
            i += 1
            where = "%s:%d" % (rel, i)
            line = strip_comment(raw)
            if not line.strip():
                continue
            t = line.strip()
            if t.startswith("@pack"):
                parts = t.split()
                if len(parts) < 2:
                    die(where, "@pack NAME kind=...")
                attrs = dict(p.split("=", 1) for p in parts[2:] if "=" in p)
                old = [p for p in self.packs if p.name == parts[1]]
                if old:                     # a pack continued in another file
                    if attrs.get("kind", "episode") != old[0].kind:
                        die(where, "pack %s reopened with another kind" % parts[1])
                    pack = old[0]
                    continue
                pack = Pack(parts[1], attrs.get("kind", "episode"), attrs, rel)
                if pack.kind not in CAPS:
                    die(where, "unknown pack kind %s" % pack.kind)
                if not re.match(r"^[a-z][a-z0-9_]{0,7}$", pack.name):
                    die(where, "pack names are 1-8 characters [a-z0-9_]")
                self.packs.append(pack)
                continue
            if pack is None:
                die(where, "content before @pack")
            if t.startswith("@mission"):
                self.missions.append((where, self.attrs(t[len("@mission"):], where), pack))
                continue
            if t.startswith("@possessor"):
                self.possessors.append((where, self.attrs(t[len("@possessor"):], where), pack))
                continue
            if t.startswith("@armed"):
                self.armed.append((where, self.attrs(t[len("@armed"):], where), pack))
                continue
            if t.startswith("string "):
                tk = tokens(t)
                if len(tk) != 3 or tk[2][0] != "str":
                    die(where, 'string NAME "text"')
                pack.named.append((tk[1][1], tk[2][1], where))
                continue
            if t.startswith("script "):
                body, i = block(i)
                pack.scripts.append([t.split()[1], body, None, where])
                continue
            if t.startswith("beatscript "):
                body, i = block(i)
                pack.beats.append([t.split()[1], body, None, where])
                continue
            die(where, "expected @pack, @mission, @possessor, @armed, string, script or beatscript, got %r" % t)

    def attrs(self, s, where):
        """[ID] key=value key="quoted value" ..."""
        out = {}
        s = s.strip()
        m = re.match(r"^([A-Za-z0-9_]+)(\s|$)", s)
        if m and "=" not in s[:m.end()]:
            out["id"] = m.group(1)
            s = s[m.end():]
        pos = 0
        for m in re.finditer(r'\s*(\w+)=("(?:[^"\\]|\\.)*"|\S+)', s):
            if m.start() != pos:
                die(where, "expected key=value near %r" % s[pos:m.start() + 10])
            v = m.group(2)
            if v.startswith('"'):
                v = v[1:-1].replace('\\"', '"').replace("\\\\", "\\")
            out[m.group(1)] = v
            pos = m.end()
        if s[pos:].strip():
            die(where, "expected key=value near %r" % s[pos:pos + 20])
        return out

    # ---- compiling a script ----
    def compile_script(self, pack, name, body):
        ops = []          # [op, a, b] or [op, a, ('label'|'script'|'beat', name)]
        labels = {}
        dcount = 0
        pend_expr = [None]
        where0 = body[0][0] if body else pack.path
        for where, raw in body:
            line = strip_comment(raw).strip()
            if not line:
                continue
            m = re.match(r"^([A-Za-z_]\w*):$", line)
            if m:
                if m.group(1) in labels:
                    die(where, "label %s defined twice" % m.group(1))
                labels[m.group(1)] = len(ops)
                continue
            b = BRACKET.match(line)
            if b:
                self.bracket(ops, b.group(1).strip(), where, pend_expr)
                continue
            tk = tokens(line)
            cmd = tk[0][1].lower()
            args = tk[1:]
            words = [a[1] for a in args]

            def need(n):
                if len(args) < n:
                    die(where, "%s needs %d argument(s)" % (cmd, n))

            if cmd == "say":
                need(2)
                who = self.who(words[0], where)
                k = 1
                expr = pend_expr[0] or 0
                pend_expr[0] = None
                if args[k][0] == "word":
                    expr = self.sym(words[k].upper(), where, "EXPR_")
                    k += 1
                if k >= len(args) or args[k][0] != "str":
                    die(where, "say WHO [EXPR] \"line\" [FLAGS]")
                text = args[k][1]
                flags = 0
                for w in words[k + 1:]:
                    f = w.strip("[]").upper()
                    if f not in LINE_FLAGS:
                        die(where, "unknown line flag %s" % w)
                    flags |= LINE_FLAGS[f]
                dcount += text.count("{d}") + (1 if flags & LINE_FLAGS["DEADPAN"] else 0)
                self.emit_say(ops, who, pack.text(text, where), flags, expr)
            elif cmd in ("choice", "player_line"):
                m2 = re.match(r'^\S+\s+(\S+)\s+("(?:[^"\\]|\\.)*")\s*\{(.*)\}\s*$', line)
                if not m2:
                    die(where, '%s WHO "question" { "option" [-> label] ; ... }' % cmd)
                who = self.who(m2.group(1), where)
                q = tokens(m2.group(2))[0][1]
                opts = []
                for part in m2.group(3).split(";"):
                    part = part.strip()
                    if not part:
                        continue
                    m3 = re.match(r'^("(?:[^"\\]|\\.)*")\s*(?:->\s*(\w+))?$', part)
                    if not m3:
                        die(where, "bad option %r" % part)
                    opts.append((tokens(m3.group(1))[0][1], m3.group(2)))
                if not 1 <= len(opts) <= 4:
                    die(where, "a choice has 1..4 options")
                ops.append(["OP_CHOICE" if cmd == "choice" else "OP_PLAYER_LINE", len(opts), pack.text(q, where)])
                self.emit_who(ops, who)
                for o, _ in opts:
                    if len(visible(o)) > OPT_CHARS:
                        die(where, "an option longer than %d characters: %r" % (OPT_CHARS, o))
                    ops.append(["OP_DATA", D_OPT, pack.text(o, where)])
                for _, lab in opts:
                    ops.append(["OP_DATA", D_LABEL, ("label", lab) if lab else TXT_NONE])
            elif cmd in ("setf", "clrf"):
                need(1)
                f = self.sym(words[0], where, "SF_")
                if not 0 <= f < 256:
                    die(where, "flag %s out of range" % words[0])
                ops.append(["OP_SETF" if cmd == "setf" else "OP_CLRF", f, 0])
            elif cmd in ("jif", "jifn"):
                need(2)
                f = self.sym(words[0], where, "SF_")
                if not 0 <= f < 256:
                    die(where, "flag %s out of range" % words[0])
                ops.append(["OP_JIF" if cmd == "jif" else "OP_JIFN", f, ("label", words[1])])
            elif cmd in ("jlt", "jge"):
                need(3)
                ops.append(["OP_JLT" if cmd == "jlt" else "OP_JGE", self.var(words[0], where), ("label", words[2])])
                ops.append(["OP_DATA", D_CMP, self.sym(words[1], where) & 0xFFFF])
            elif cmd in ("jmp", "goto"):
                need(1)
                ops.append(["OP_JMP", 0, ("label", words[0])])
            elif cmd in ("setv", "addv"):
                need(2)
                ops.append(["OP_SETV" if cmd == "setv" else "OP_ADDV", self.var(words[0], where),
                            self.sym(words[1], where) & 0xFFFF])
            elif cmd == "beat":
                need(1)
                ops.append(["OP_SET_BEAT", 0, self.sym(words[0], where, "BEAT_")])
            elif cmd in ("give", "take"):
                need(1)
                n = int(words[1]) if len(words) > 1 else 1
                ops.append(["OP_GIVE" if cmd == "give" else "OP_TAKE", self.sym(words[0], where, "ITEM_"), n])
            elif cmd == "reward":
                need(1)
                money = int(words[0])
                item, cnt, unl = 0, 0, 0
                rest = [w for w in words[1:] if "=" not in w]
                if rest:
                    item = self.sym(rest[0], where, "ITEM_")
                    cnt = int(rest[1]) if len(rest) > 1 else 1
                for w in words[1:]:
                    if w.startswith("unlock="):
                        unl = self.sym(w[7:], where)
                    elif w.startswith("furn="):
                        ops.append(["OP_HOME", self.syms["HOME_GRANT"], ("furn", w[5:])])
                if money % 10 or money > 2550:
                    die(where, "reward money is stored /10 (0..2550)")
                ops.append(["OP_REWARD", money // 10, item | (cnt << 8)])
                if unl:
                    ops.append(["OP_DATA", D_UNLOCK, unl])
            elif cmd == "befriend":
                need(2)
                who = self.who(words[0], where)
                ops.append(["OP_BEFRIEND", W_BIG, int(words[1]) & 0xFFFF])
                ops.append(["OP_DATA", D_WHO, who])
            elif cmd in ("possess", "unpossess"):
                need(1)
                who = self.who(words[0], where)
                row = int(words[1]) if cmd == "possess" and len(words) > 1 else -1   # the @possessor row
                ops.append(["OP_POSSESS" if cmd == "possess" else "OP_UNPOSSESS", W_BIG, row & 0xFFFF])
                ops.append(["OP_DATA", D_WHO, who])
            elif cmd == "push":
                need(1)
                kind = PEND.get(words[0].lower())
                if not kind:
                    die(where, "unknown push kind %s" % words[0])
                arg = 0
                caps = None
                tpat = tskin = None
                for w in words[1:]:
                    if w.startswith("captions="):
                        caps = w[9:]
                    elif w.startswith("arg="):
                        arg = self.arg_value(w[4:], where)
                    elif w.startswith("pattern="):          # push timing pattern=three skin=laugh [drift]
                        tpat = self.sym(w[8:], where, "TPAT_")
                    elif w.startswith("skin="):
                        tskin = self.sym(w[5:], where, "TSKIN_")
                    elif w == "drift":
                        tskin = (tskin or 0) | self.syms["TIMING_DRIFT"]
                    elif w in ("->",) or "," in w:
                        pass
                    elif arg == 0 and not w.startswith(("ok", "fail")):
                        arg = self.arg_value(w, where)
                if tpat is not None or tskin is not None:
                    if words[0].lower() != "timing":
                        die(where, "pattern= / skin= / drift are for push timing")
                    arg = (tpat or 0) | ((tskin or 0) << 8)
                self.check_push(words[0].lower(), arg, where)
                if pack.kind == "side" and words[0].lower() == "combat" and caps is None:
                    die(where, "a side mission's push combat needs captions=a,b,c (SPEC C5)")
                if caps:
                    # the caption stash (C5): decoded into resident slots before the push
                    names = [c for c in caps.split(",") if c]
                    if len(names) > self.syms.get("TXT_STASH_LINES", 4):
                        die(where, "at most %d captions" % self.syms.get("TXT_STASH_LINES", 4))
                    for k, nm in enumerate(names):
                        if nm not in self.string_names:
                            die(where, "captions=%s: no string %s (declare it with string NAME \"...\")" % (caps, nm))
                        ops.append(["OP_STASH", k, self.string_names[nm]])
                ops.append(["OP_PUSH", self.syms[kind], arg & 0xFFFF])
            elif cmd == "verb":
                # verb escort|find|photo|deliver [WHO] [cell=x,y] [fragile] then=SCRIPT: an
                # overworld verb (story_verbs.c); SCRIPT runs when it ends, v1 = the result
                need(1)
                kind = self.sym(words[0], where, "VERB_")
                who, cell, fl, then = None, 0, 0, None
                for w in words[1:]:
                    if w.startswith("cell="):
                        cell = self.arg_value(w, where)
                    elif w.startswith("then="):
                        then = w[5:]
                    elif w == "fragile":
                        fl = self.syms["VERB_ARG_FRAGILE"]
                    else:
                        who = self.who(w, where)
                if who is not None and not 0 <= who < self.syms.get("DOOMO_MAX_RESIDENTS", 1000):
                    die(where, "verb: WHO must be a resident")
                if kind in (self.syms["VERB_ESCORT"], self.syms["VERB_FIND"]) and not cell:
                    die(where, "verb %s needs cell=x,y" % words[0])
                if kind != self.syms["VERB_FIND"] and who is None:
                    die(where, "verb %s needs WHO" % words[0])
                if not then:
                    die(where, "verb %s needs then=SCRIPT (it runs when the verb ends, v1 = the result)" % words[0])
                ops.append(["OP_NATIVE", self.syms["STORY_CB_VERB"] + kind, (who or 0) & 0xFFFF])
                ops.append(["OP_DATA", D_ARG, (cell | fl) & 0xFFFF])
                ops.append(["OP_DATA", D_SCRIPT, ("script", then)])
            elif cmd == "stash":
                # stash SLOT "line" | stash SLOT NAME: a resident caption for the next push
                need(2)
                slot = int(words[0])
                if not 0 <= slot < self.syms.get("TXT_STASH_LINES", 4):
                    die(where, "stash slots are 0..%d" % (self.syms.get("TXT_STASH_LINES", 4) - 1))
                if args[1][0] == "str":
                    if len(visible(args[1][1])) >= self.syms.get("TXT_STASH_LEN", 64):
                        die(where, "a stashed line is under %d characters" % self.syms.get("TXT_STASH_LEN", 64))
                    tid = pack.text(args[1][1], where)
                elif words[1] in self.string_names:
                    tid = self.string_names[words[1]]
                else:
                    die(where, "stash SLOT \"line\" or stash SLOT NAME")
                ops.append(["OP_STASH", slot, tid])
            elif cmd == "native":
                need(1)
                cb = self.sym(words[0], where, "CB_")
                arg = self.arg_value(words[1], where) if len(words) > 1 else 0
                if len(words) > 2:                 # native CB ACTION PARAM: the param in the high byte
                    arg = (arg & 0xFF) | ((self.arg_value(words[2], where) & 0xFF) << 8)
                if not 0 < cb < 256:
                    die(where, "native callback ids are 1..255")
                ops.append(["OP_NATIVE", cb, arg & 0xFFFF])
            elif cmd in ("wait", "hold", "waita"):
                need(1)
                op = {"wait": "OP_WAIT", "hold": "OP_HOLD", "waita": "OP_WAIT_A"}[cmd]
                ops.append([op, int(words[0]) & 0xFF, 0])
            elif cmd == "sfx":
                need(1)
                ops.append(["OP_SFX", 0, self.sym(words[0], where, "SFX_")])
            elif cmd == "music":
                need(1)
                ops.append(["OP_MUSIC", 0, self.sym(words[0], where, "SONG_")])
            elif cmd == "mute":
                ops.append(["OP_MUTE", 0, int(words[0]) if words else 0xFFFF])
            elif cmd == "duck":
                need(2)
                ops.append(["OP_DUCK", min(255, int(words[0])), int(words[1])])
            elif cmd == "tapestop":
                ops.append(["OP_TAPESTOP", 0, int(words[0]) if words else 5])
            elif cmd == "resume_on_beat":
                ops.append(["OP_RESUME_ON_BEAT", 0, 0])
            elif cmd == "shake":
                need(1)
                ops.append(["OP_SHAKE", self.sym(words[0], where, "SHK_"), 0])
            elif cmd == "punch":
                need(2)
                ops.append(["OP_PUNCH", int(words[0]) & 0xFF, int(words[1]) & 0xFFFF])
            elif cmd == "flash":
                need(3)
                ops.append(["OP_FLASH", self.sym(words[0], where, "PAL_"), int(words[1]) | (int(words[2]) << 8)])
            elif cmd == "fade":
                need(1)
                ops.append(["OP_FADE", 0, int(words[0])])
            elif cmd == "pal":
                need(2)
                ops.append(["OP_PAL_LERP", self.sym(words[0], where, "PAL_"), int(words[1])])
            elif cmd == "tint":
                need(2)
                ops.append(["OP_TINT", self.sym(words[0], where, "TINT_"), int(words[1])])
            elif cmd == "iris":
                need(1)
                ops.append(["OP_IRIS", 1 if words[0] == "heart" else 0, int(words[1]) if len(words) > 1 else 0])
            elif cmd == "cut":
                need(1)
                ops.append(["OP_CUT", 0, int(words[0])])
            elif cmd == "insert":
                need(2)
                if not 3 <= int(words[1]) <= 4:
                    die(where, "an insert is 3-4 frames")
                ops.append(["OP_INSERT", int(words[0]), int(words[1])])
            elif cmd == "freeze_cap":
                if len(args) < 2 or args[1][0] != "str":
                    die(where, 'freeze_cap STYLE "caption"')
                ops.append(["OP_FREEZE_CAP", int(words[0]), pack.text(args[1][1], where)])
            elif cmd == "cast":
                need(3)
                who = self.who(words[0], where)
                ops.append(["OP_CAST", W_BIG, (int(words[1]) & 0xFF) | ((int(words[2]) & 0xFF) << 8)])
                ops.append(["OP_DATA", D_WHO, who])
                if len(words) > 3:
                    ops.append(["OP_DATA", D_ARG, self.arg_value(words[3], where)])
            elif cmd == "npc":
                need(2)
                who = self.who(words[0], where)
                act = self.sym(words[1], where, "NACT_")
                arg = self.arg_value(words[2], where) if len(words) > 2 else 0
                ops.append(["OP_NPC", W_BIG, act | ((arg & 0xFF) << 8)])
                ops.append(["OP_DATA", D_WHO, who])
                if arg & ~0xFF:                    # (S2) the full 16-bit arg: a cell, a resident, brad | frames << 8
                    ops.append(["OP_DATA", D_ARG, arg & 0xFFFF])
            elif cmd == "cam":
                need(1)
                ops.append(["OP_CAM", self.sym(words[0], where, "CAM_"), self.arg_value(words[1], where) if len(words) > 1 else 0])
            elif cmd == "teleport":
                need(3)
                ops.append(["OP_TELEPORT", self.sym(words[0], where, "MAPID_"), (int(words[1]) << 8) | int(words[2])])
            elif cmd == "home":
                need(1)
                sub = self.sym(words[0], where, "HOME_")
                if len(words) > 1 and words[1].startswith("furn="):     # (S2) home GRANT|PLACE_GIFT furn=KEY
                    ops.append(["OP_HOME", sub, ("furn", words[1][5:])])
                else:
                    arg = self.arg_value(words[1], where) if len(words) > 1 else 0
                    ops.append(["OP_HOME", sub, arg & 0xFFFF])
            elif cmd == "quest":
                need(2)
                st = {"offered": 1, "active": 2, "done": 3}.get(words[1].lower())
                if st is None:
                    st = int(words[1])
                qid = int(words[0][1:]) if words[0][:1] in "Ss" else int(words[0])
                ops.append(["OP_QUEST", 0, (qid & 0xFFF) | (st << 12)])
            elif cmd == "time":
                need(1)
                ops.append(["OP_TIME", self.sym(words[0].upper(), where, "TIME_"), int(words[1]) if len(words) > 1 else 0])
            elif cmd == "rng":
                need(2)
                ops.append(["OP_RNG", self.var(words[0], where), int(words[1])])
            elif cmd == "later":
                need(2)
                mins = words[0].rstrip("m")
                ops.append(["OP_LATER", int(mins), ("script", words[1])])
            elif cmd == "call":
                need(1)
                ops.append(["OP_CALL", 0, ("script", words[0])])
            elif cmd == "ret":
                ops.append(["OP_RET", 0, 0])
            elif cmd == "runbeat":
                need(1)
                ops.append(["OP_BEAT", 0, ("beat", words[0])])
            elif cmd == "end":
                ops.append(["OP_END", 0, 0])
            else:
                die(where, "unknown op %s" % cmd)
        if dcount > 1:
            die(where0, "script %s has %d {d} deadpans (max 1 per conversation)" % (name, dcount))
        if not ops or ops[-1][0] != "OP_END":
            ops.append(["OP_END", 0, 0])
        return ops, labels

    def var(self, w, where):
        v = int(w[1:]) if w[:1] in "vV" and w[1:].isdigit() else int(w) if w.isdigit() else None
        if v is None or not 0 <= v < self.syms.get("VM_VARS", 16):
            die(where, "a var is v0..v%d" % (self.syms.get("VM_VARS", 16) - 1))
        return v

    def emit_who(self, ops, who):
        ops.append(["OP_DATA", D_WHO, who & 0xFFFF])

    def emit_say(self, ops, who, tid, flags, expr):
        player = self.syms["DOOMO_PLAYER_ID"]
        a = {WHO_TARGET: W_TARGET, WHO_NAR: W_NAR, player: W_PLAYER}.get(who, who if who < W_TARGET else W_BIG)
        ops.append(["OP_SAY", a, tid])
        if flags or expr:
            ops.append(["OP_DATA", D_LINE, flags | (expr << 8)])
        if a == W_BIG:
            ops.append(["OP_DATA", D_WHO, who])

    def bracket(self, ops, code, where, pend_expr):
        """the closed list of bracket codes (SPEC sec.5, C14)"""
        u = code.upper()
        m = re.match(r"^R(\d+)$", u)
        if m:
            ops.append(["OP_WAIT", int(m.group(1)), 0])
            return
        m = re.match(r"^HOLD (\d+)$", u)
        if m:
            ops.append(["OP_HOLD", int(m.group(1)), 0])
            return
        m = re.match(r"^MUTE (\d+)$", u)
        if m:
            ops.append(["OP_MUTE", 0, int(m.group(1))])
            return
        m = re.match(r"^DUCK (\d+) (\d+)$", u)
        if m:
            ops.append(["OP_DUCK", min(255, int(m.group(1))), int(m.group(2))])
            return
        if u == "TAPESTOP":
            ops.append(["OP_TAPESTOP", 0, 5])
            return
        if u == "SCRATCH":               # tape-stop 5 + SHK_BRAKE + mute 14
            ops.append(["OP_TAPESTOP", 0, 5])
            ops.append(["OP_SHAKE", self.syms["SHK_BRAKE"], 0])
            ops.append(["OP_MUTE", 0, 14])
            return
        if u == "SILENCE":
            ops.append(["OP_MUTE", 0, 0xFFFF])
            return
        if u in ("WAIT A", "WAITA"):
            ops.append(["OP_WAIT_A", 0, 0])
            return
        m = re.match(r"^FLASH W(\d+),(\d+)$", u)
        if m:
            ops.append(["OP_FLASH", self.syms["PAL_WHITE"], int(m.group(1)) | (int(m.group(2)) << 8)])
            return
        if u.startswith("SHK_"):
            ops.append(["OP_SHAKE", self.sym(u, where), 0])
            return
        m = re.match(r"^DAY (\d+)$", u)
        if m:
            ops.append(["OP_TIME", self.syms["TIME_SET_DAY"], int(m.group(1))])
            return
        if u == "NIGHT":
            ops.append(["OP_TIME", self.syms["TIME_SET_NIGHT"], 0])
            return
        if u in ("NO BUTTON", "NO GAG"):
            return                         # markers the lint reads in beat scripts
        m = re.match(r"^INSERT (\d+)$", u)
        if m:
            ops.append(["OP_INSERT", 0, int(m.group(1))])
            return
        m = re.match(r"^EXPR (\w+)$", u)
        if m:
            pend_expr[0] = self.sym(m.group(1), where, "EXPR_")
            return
        if u in EXPRS:
            pend_expr[0] = self.sym("EXPR_" + u, where)
            return
        if u.startswith("SFX "):
            ops.append(["OP_SFX", 0, self.sym(u[4:].strip(), where, "SFX_")])
            return
        if "SFX_" + u in self.syms:
            ops.append(["OP_SFX", 0, self.syms["SFX_" + u]])
            return
        die(where, "unknown bracket code [%s] (the closed list is in docs/STORY_ENGINE.md)" % code)

    def check_push(self, kind, arg, where):
        if self.push_args is None:
            return
        allowed = self.push_args.get(kind)
        if allowed is None:
            die(where, "push %s is not in core/data/story/push_args.txt (SPEC C12)" % kind)
        for a in allowed:
            if a == "*":
                return
            if ".." in a:
                lo, hi = a.split("..")
                if int(lo, 0) <= arg <= int(hi, 0):
                    return
            elif int(a, 0) == arg:
                return
        die(where, "push %s %d: the arg is not in core/data/story/push_args.txt (SPEC C12)" % (kind, arg))

    def arg_value(self, w, where):
        if w.startswith("cell="):
            x, y = w[5:].split(",")
            return int(x) | (int(y) << 8)
        return self.sym(w, where)

    # ---- compiling a beat script ----
    def compile_beat(self, pack, name, body):
        evs = []
        lab_ids = {"start": 0, "L0": 0, "L_PUNCH": 30, "L_BUTTON": 31}
        nexti = [1]
        anchor = 0
        cur_where = [pack.path]

        def lab(n):
            if n not in lab_ids:
                if nexti[0] >= 30:
                    die(cur_where[0], "beatscript %s: too many labels (max 29 + L_PUNCH/L_BUTTON)" % name)
                lab_ids[n] = nexti[0]
                nexti[0] += 1
            return lab_ids[n]
        S = self.syms
        for where, raw in body:
            cur_where[0] = where
            line = strip_comment(raw).strip()
            if not line:
                continue
            m = re.match(r"^@?(\w+):$", line) or re.match(r"^@(\w+)$", line)
            if m:
                anchor = lab(m.group(1))
                continue
            m = re.match(r"^\+(\d+)\s+(.*)$", line)
            if not m:
                die(where, "beat lines are 'LABEL:' or '+FRAMES op args'")
            at = int(m.group(1))
            for stmt in m.group(2).split(";"):
                stmt = stmt.strip()
                if not stmt:
                    continue
                tk = tokens(stmt)
                cmd = tk[0][1].lower()
                w = [t[1] for t in tk[1:]]

                def ev(op, a=0, b=0):
                    evs.append((anchor, at, len(evs), S[op], a, b))
                if cmd == "label":
                    ev("BOP_LABEL", lab(w[0]))
                elif cmd == "waita":
                    ev("BOP_WAIT_A", lab(w[0]), (int(w[1]) & 127) | ((int(w[2]) if len(w) > 2 else 0) << 7))
                elif cmd == "hold":
                    ev("BOP_HOLD", lab(w[0]), int(w[1]))
                elif cmd in ("say", "caption"):
                    strs = [t[1] for t in tk[1:] if t[0] == "str"]
                    if not strs:
                        die(where, '%s needs a quoted line' % cmd)
                    who = self.who(w[0], where) if cmd == "say" else 0
                    if who == WHO_NAR:
                        who = -1
                    elif who == WHO_TARGET:
                        who = -3
                    ev("BOP_SAY" if cmd == "say" else "BOP_CAPTION", who, pack.text(strs[0], where))
                elif cmd == "pop":
                    ev("BOP_POP", int(w[0]), self.sym(w[1], where, "TWT_"))
                elif cmd == "expr":
                    ev("BOP_EXPR", int(w[0]), self.sym(w[1].upper(), where, "EXPR_"))
                elif cmd == "act":
                    ev("BOP_ACT", int(w[0]), int(w[1]))
                elif cmd in ("freeze", "thaw", "panic", "still", "twitch"):
                    ev("BOP_" + cmd.upper(), int(w[0]) if w else -1, int(w[1]) if len(w) > 1 else 0)
                elif cmd == "hitstop":
                    ev("BOP_HITSTOP", int(w[0]))
                elif cmd == "shake":
                    ev("BOP_SHAKE", self.sym(w[0], where, "SHK_"))
                elif cmd == "punch":
                    ev("BOP_PUNCH", int(w[0]), int(w[1]))
                elif cmd == "zoom":
                    ev("BOP_ZOOM", int(w[0]), int(w[1]))
                elif cmd == "flash":
                    ev("BOP_FLASH", self.sym(w[0], where, "PAL_"), int(w[1]) | (int(w[2]) << 8))
                elif cmd == "fade":
                    ev("BOP_FADE", 0, int(w[0]) | ((int(w[1]) if len(w) > 1 else 0) << 8))
                elif cmd in ("iris_out", "iris_in"):
                    ev("BOP_IRIS_OUT" if cmd == "iris_out" else "BOP_IRIS_IN", int(w[0]) if w else 0,
                       int(w[1]) if len(w) > 1 else 0)
                elif cmd == "cut":
                    ev("BOP_CUT", int(w[0]), int(w[1]) if len(w) > 1 else 0)
                elif cmd == "insert":
                    if not 3 <= int(w[1]) <= 4:
                        die(where, "an insert is 3-4 frames")
                    ev("BOP_INSERT", int(w[0]), int(w[1]))
                elif cmd == "sfx":
                    ev("BOP_SFX", self.sym(w[0], where, "SFX_"), int(w[1]) if len(w) > 1 else 0)
                elif cmd == "music":
                    ev("BOP_MUSIC", self.sym(w[0], where, "SONG_"))
                elif cmd == "mute":
                    ev("BOP_MUSIC_MUTE", int(w[0]))
                elif cmd == "duck":
                    ev("BOP_MUSIC_DUCK", int(w[1]), int(w[0]))
                elif cmd == "tapestop":
                    ev("BOP_TAPESTOP", int(w[0]) if w else 5)
                elif cmd == "flag":
                    ev("BOP_FLAG", self.sym(w[0], where, "SF_"), int(w[1]) if len(w) > 1 else 1)
                elif cmd == "nobutton":
                    ev("BOP_NOBUTTON")
                elif cmd == "npc":
                    who = self.who(w[0], where)
                    ev("BOP_NPC", who, self.sym(w[1], where, "NACT_") | ((self.arg_value(w[2], where) & 0xFF if len(w) > 2 else 0) << 8))
                elif cmd == "cam":
                    ev("BOP_CAM", self.sym(w[0], where, "CAM_"), self.arg_value(w[1], where) if len(w) > 1 else 0)
                elif cmd == "tint":
                    ev("BOP_TINT", self.sym(w[0], where, "TINT_"), int(w[1]))
                elif cmd == "pal":
                    ev("BOP_PAL_LERP", self.sym(w[0], where, "PAL_"), int(w[1]) | ((int(w[2]) if len(w) > 2 else 0) << 8))
                elif cmd == "shot":
                    ev("BOP_SHOT", int(w[0]))
                elif cmd == "headturn":
                    ev("BOP_HEADTURN", self.who(w[0], where), int(w[1]) | ((int(w[2]) if len(w) > 2 else 0) << 8))
                elif cmd == "end":
                    ev("BOP_END")
                else:
                    die(where, "unknown beat op %s" % cmd)
        evs.sort(key=lambda e: (e[0], e[1], e[2]))
        if not any(e[3] == S["BOP_END"] for e in evs):
            die(body[-1][0] if body else pack.path, "beatscript %s has no end" % name)
        return [(e[1], e[0], e[3], e[4], e[5]) for e in evs]

    def build(self):
        names = set()
        for i, p in enumerate(sorted(self.packs, key=lambda p: p.name)):
            names.add(p.name)
            p.id = i
            if i >= MAX_PACKS:
                die(p.path, "more than %d packs" % MAX_PACKS)
        self.packs.sort(key=lambda p: p.id)
        self.string_names = {}
        for p in self.packs:
            # script index 0 of every pack is an empty script, so no script
            # id is ever 0 (0 means "none" in STRY.later and DoomoPossessor)
            p.scripts.insert(0, ["__none_%s" % p.name, [], None, p.path])
            for k, s in enumerate(p.scripts):
                if s[0] in self.script_names:
                    die(s[3], "script %s defined twice" % s[0])
                if k >= MAX_INDEX:
                    die(s[3], "more than %d scripts in pack %s" % (MAX_INDEX, p.name))
                self.script_names[s[0]] = (p.id << INDEX_BITS) | k
            for k, b in enumerate(p.beats):
                if b[0] in self.beat_names:
                    die(b[3], "beatscript %s defined twice" % b[0])
                self.beat_names[b[0]] = (p.id << INDEX_BITS) | k
            for nm, text, where in p.named:
                if nm in self.string_names:
                    die(where, "string %s defined twice" % nm)
                self.string_names[nm] = p.text(text, where)
        for p in self.packs:
            for s in p.scripts:
                s[2] = self.compile_script(p, s[0], s[1])
            for b in p.beats:
                b[2] = self.compile_beat(p, b[0], b[1])
        furn = load_furn_ids()
        for p in self.packs:
            for s in p.scripts:
                ops, labels = s[2]
                out = []
                for op in ops:
                    b = op[2]
                    if isinstance(b, tuple):
                        kind, name = b
                        if kind == "label":
                            if name not in labels:
                                die(s[3], "script %s: jump to undefined label %s" % (s[0], name))
                            b = labels[name]
                        elif kind == "script":
                            if name not in self.script_names:
                                die(s[3], "script %s: unknown script %s" % (s[0], name))
                            b = self.script_names[name]
                        elif kind == "beat":
                            if name not in self.beat_names:
                                die(s[3], "script %s: unknown beatscript %s" % (s[0], name))
                            b = self.beat_names[name]
                        elif kind == "furn":
                            if furn is not None and name not in furn:
                                die(s[3], "script %s: unknown furniture %s" % (s[0], name))
                            b = furn.get(name, 0xFFFF) if furn is not None else 0xFFFF
                    out.append((self.syms[op[0]], op[1] & 0xFF, b & 0xFFFF))
                s[2] = out


def load_furn_ids():
    """core/gen/furn_ids.h (H4's furngen) -> {key: id}, or None before it lands"""
    path = os.path.join(CORE, "gen", "furn_ids.h")
    if not os.path.exists(path):
        return None
    out = {}
    for m in re.finditer(r"#define\s+FURN_([A-Z0-9_]+)\s+(\d+)", open(path).read()):
        out[m.group(1).lower()] = int(m.group(2))
    return out


# ---------------------------------------------------------------------------
# the codec: dictionary + canonical Huffman
# ---------------------------------------------------------------------------
def build_dictionary(strings):
    cand = {}
    for s in strings:
        for m in re.finditer(r"[A-Za-z'@]+[,.!?]? ?|\{[^}]*\}|\.\.\. ?", s):
            w = m.group(0)
            if len(w) >= 3:
                cand[w] = cand.get(w, 0) + 1
    scored = [(-(len(w) - 2) * n + len(w), w) for w, n in cand.items() if n >= 3]
    scored.sort()
    return [w for sc, w in scored[:DICT_MAX] if sc < 0]


def tokenize(s, dic_by_first):
    out = []
    i = 0
    b = s.encode("latin-1")
    while i < len(s):
        best = None
        for k, w in dic_by_first.get(s[i], ()):
            if s.startswith(w, i):
                best = (k, w)
                break                        # longest first
        if best:
            out.append(256 + best[0])
            i += len(best[1])
        else:
            out.append(b[i])
            i += 1
    out.append(0)
    return out


def huffman_lengths(freq):
    """code lengths (<= MAX_CODE_LEN) for the symbols with freq > 0 (deterministic)"""
    syms = sorted(s for s, f in freq.items() if f > 0)
    if len(syms) == 1:
        return {syms[0]: 1}
    heap = [(freq[s], s, (s,)) for s in syms]
    heapq.heapify(heap)
    depth = {s: 0 for s in syms}
    order = 1 << 20
    while len(heap) > 1:
        f1, k1, a = heapq.heappop(heap)
        f2, k2, b = heapq.heappop(heap)
        for s in a + b:
            depth[s] += 1
        heapq.heappush(heap, (f1 + f2, order, a + b))
        order += 1
    if max(depth.values()) > MAX_CODE_LEN:
        for s in depth:
            depth[s] = min(depth[s], MAX_CODE_LEN)
        while sum(2 ** (MAX_CODE_LEN - d) for d in depth.values()) > 2 ** MAX_CODE_LEN:
            s = max((d, -freq[x], x) for x, d in depth.items() if d < MAX_CODE_LEN)[2]
            depth[s] += 1
    return depth


def canonical(lengths):
    order = sorted(lengths.items(), key=lambda kv: (kv[1], kv[0]))
    codes = {}
    code = 0
    prev = 0
    for s, l in order:
        code <<= (l - prev)
        codes[s] = (code, l)
        code += 1
        prev = l
    counts = [0] * (MAX_CODE_LEN + 1)
    for _, l in order:
        counts[l] += 1
    return codes, counts, [s for s, _ in order]


def encode(symbols, codes):
    bits = []
    for s in symbols:
        c, l = codes[s]
        for k in range(l - 1, -1, -1):
            bits.append((c >> k) & 1)
    out = bytearray()
    for i in range(0, len(bits), 8):
        byte = 0
        for k, bit in enumerate(bits[i:i + 8]):
            byte |= bit << (7 - k)
        out.append(byte)
    return bytes(out)


def decode(data, counts, order, dic):
    """the reference decoder (txt.c does the same)"""
    out = []
    pos = 0
    while True:
        code = first = index = 0
        for l in range(1, MAX_CODE_LEN + 1):
            bit = (data[pos >> 3] >> (7 - (pos & 7))) & 1
            pos += 1
            code |= bit
            n = counts[l]
            if code - first < n:
                sym = order[index + code - first]
                break
            index += n
            first += n
            first <<= 1
            code <<= 1
        else:
            raise Fail("bad stream")
        if sym == 0:
            return "".join(out)
        out.append(chr(sym) if sym < 256 else dic[sym - 256])


# ---------------------------------------------------------------------------
# writers
# ---------------------------------------------------------------------------
def u16(v):
    return bytes((v & 0xFF, (v >> 8) & 0xFF))


def u32(v):
    return bytes((v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF, (v >> 24) & 0xFF))


def align4(b):
    return b + b"\0" * ((-len(b)) & 3)


def pack_bytes(p, streams):
    scripts = b"".join(u16(first) + u16(n) + bytes((nl, p.id)) for first, n, nl in p.script_spans)
    ops = b"".join(bytes((op, a)) + u16(b) for op, a, b in p.all_ops)
    beats = b"".join(u16(first) + u16(n) for first, n in p.beat_spans)
    evs = b"".join(u16(at) + bytes((anc, op)) + u16(a & 0xFFFF) + u16(b & 0xFFFF) for at, anc, op, a, b in p.all_events)
    str_off = b""
    data = b""
    for s in streams:
        str_off += u16(len(data))
        data += s
    if len(data) > 0xFFFF:
        raise Fail("%s: pack %s streams over 64 KB" % (p.path, p.name))
    hdr_len = 40
    body = b""
    offs = []
    for sec in (str_off, scripts, ops, beats, evs, data):
        offs.append(hdr_len + len(body))
        body = align4(body + sec)
    flags = (1 if p.legacy else 0) | (KINDS[p.kind] << 4)
    hdr = u16(0x5854) + bytes((1, p.id)) + u16(len(p.strings)) + u16(len(p.script_spans)) + \
        u16(len(p.all_ops)) + u16(len(p.beat_spans)) + u16(len(p.all_events)) + u16(flags)
    for o in offs:
        hdr += u32(o)
    assert len(hdr) == hdr_len
    return hdr + body


def c_words(blob):
    blob = align4(bytes(blob))
    words = [blob[i] | (blob[i + 1] << 8) | (blob[i + 2] << 16) | (blob[i + 3] << 24) for i in range(0, len(blob), 4)]
    lines = []
    for i in range(0, len(words), 6):
        lines.append("    " + ", ".join("0x%08Xu" % w for w in words[i:i + 6]) + ",")
    return "\n".join(lines), len(words)


def c_ident(name):
    return re.sub(r"[^a-z0-9_]", "_", name.lower())


def c_str(s):
    out = []
    for ch in s:
        o = ord(ch)
        if ch == "\\":
            out.append("\\\\")
        elif ch == '"':
            out.append('\\"')
        elif o < 32 or o > 126:
            out.append("\\%03o" % o)
        else:
            out.append(ch)
    return "".join(out)


# (S4) the verb key of story_final.md sec.14, one bit each in the u16
# StorySideRow.verbs; every mission talks, so "talk" has no bit. The names
# S2 used before any mission existed stay as aliases.
VERBS = ("fetch", "deliver", "find", "photo", "combat", "exorcism", "weld", "pet", "minigame", "fishing",
         "cooking", "concert", "housing", "creator", "escort", "timing")
VERB_ALIAS = {"talk": None, "match": None, "fight": "combat", "exorcise": "exorcism", "cook": "cooking",
              "fish": "fishing", "studio": "concert", "dream": "pet", "text": "minigame", "visit": "find"}
SIDES = {"hill": 0, "hell": 1, "park": 2, "both": 3}


def mission_rows(comp):
    rows = {}
    for where, a, pack in comp.missions:
        sid = a.get("id", "")
        m = re.match(r"^S(\d{3})$", sid)
        if not m or not 100 <= int(m.group(1)) <= 217:
            die(where, "@mission needs an id S100..S217")
        n = int(m.group(1)) - 100
        if n in rows:
            die(where, "mission %s defined twice" % sid)
        retired = a.get("kind") == "none"
        if not retired and "later" not in a:
            die(where, "mission %s has no later= (every mission has a Later callback)" % sid)
        verbs = 0
        for v in a.get("verbs", "").split(","):
            if v in VERB_ALIAS:
                v = VERB_ALIAS[v]
            if not v:
                continue
            if v not in VERBS:
                die(where, "unknown verb %s (%s)" % (v, ", ".join(VERBS)))
            verbs |= 1 << VERBS.index(v)
        ch = a.get("ch", "1-4").split("-")
        lo, hi = int(ch[0]), int(ch[-1])
        req = a.get("requires", "")
        reqn = int(req[1:]) - 100 + 1 if re.match(r"^S\d{3}$", req) else 0
        later = int(a.get("later", "0").rstrip("m"))
        if not retired and not 1 <= later <= 10:
            die(where, "later= is 1..10 minutes after turn-in")
        offer = a.get("offer", "")
        if offer and offer not in comp.script_names:
            die(where, "unknown offer script %s" % offer)
        active = a.get("active", "")
        if active and active not in comp.script_names:
            die(where, "unknown active script %s" % active)
        title = a.get("title", "")
        # (S4) the title and its quest-log hint are a pair: hint = title + 1
        if title:
            tid = pack.text_new(title, where)
            hint = a.get("hint", "")
            if len(visible(hint)) > 95:
                die(where, "a hint is at most 95 characters (the HUD objective)")
            pack.text_new(hint, where)
        else:
            tid = TXT_NONE
        # (S4) at=X,Y: where the giver stands; after=BEAT: an episode done first
        spot = 0
        if a.get("at"):
            m2 = re.match(r"^(\d+),(\d+)$", a["at"])
            if not m2 or int(m2.group(1)) > 63 or int(m2.group(2)) > 63:
                die(where, "at=X,Y is a cell (0..63)")
            xy = (int(m2.group(1)), int(m2.group(2)))
            if xy not in comp.spots:
                comp.spots.append(xy)
            spot = comp.spots.index(xy) + 1
            if spot > 255:
                die(where, "more than 255 giver spots")
        after = comp.sym(a["after"], where, "BEAT_") if a.get("after") else 0
        if not 0 <= after < 256:
            die(where, "after= is a BEAT_* id")
        spack = pack.id
        if offer in comp.script_names:
            spack = comp.script_names[offer] >> INDEX_BITS
        # R8: a trophy is a quest-log entry (and a catalog piece), never an unlock id
        trophy = a.get("trophy", "")
        if trophy and len(trophy) > 27:
            die(where, "a trophy name is at most 27 characters")
        trid = pack.text(trophy, where) if trophy else TXT_NONE
        rows[n] = dict(sid=n, kind=0 if retired else 1, giver=comp.sym(a.get("giver", "0"), where) & 0xFFFF,
                       lo=lo, hi=hi, side=SIDES.get(a.get("side", "hill"), 0),
                       chain=ord(a["chain"][0]) if a.get("chain") else 0, step=int(a.get("step", "0")),
                       req=reqn, later=later, verbs=verbs, reward=int(a.get("reward", "0").split()[0]) & 0xFFFF,
                       offer=comp.script_names.get(offer, TXT_NONE), active=comp.script_names.get(active, TXT_NONE),
                       title=tid, trophy=trid, pack=spack, tag=after, spot=spot)
    return rows


def armed_rows(comp):
    """@armed HOST row=N from=BEAT to=BEAT [unless=SF_A|SF_B]: the story beats in
    which `row` of the authored possessor table is the story's possession of
    HOST. story_rearm_possessors() (story.c) re-arms such a host after a load,
    so a reload keeps the authored possessor and a natural possession outside
    the window stays the sim's (exorcism.c no longer guesses)."""
    out = []
    for where, a, pack in comp.armed:
        if "id" not in a or "row" not in a or "from" not in a or "to" not in a:
            die(where, "@armed HOST row=N from=BEAT to=BEAT [unless=SF_A|SF_B]")
        host = comp.who(a["id"], where)
        if not 0 <= host < comp.syms.get("DOOMO_MAX_RESIDENTS", 1000):
            die(where, "@armed: the host is a resident id")
        row = int(a["row"])
        if not 0 <= row < 255:
            die(where, "@armed: row 0..254")
        lo = comp.sym(a["from"], where, "BEAT_")
        hi = comp.sym(a["to"], where, "BEAT_")
        unless = [comp.sym(f, where, "SF_") for f in a.get("unless", "").split("|") if f]
        if len(unless) > 2:
            die(where, "@armed: at most two unless= flags")
        unless += [0xFFFF] * (2 - len(unless))
        out.append((host, row, lo, hi, unless))
    return out


def c_strs(v, n):
    parts = [p.strip() for p in v.split("|")] if v else []
    if len(parts) > n:
        raise Fail("at most %d entries: %s" % (n, v))
    return "{ " + ", ".join(['"%s"' % c_str(p) if p else "0" for p in parts] + ["0"] * (n - len(parts))) + " }"


def possessor_row(comp, where, a):
    """one DoomoPossessor initializer (exorcism.h): name="X" host=ID|script|practice
    name_res=ID offer="A|B|C|D" clue="a|b|c|d" saved=SCRIPT lost=SCRIPT near_miss=K
    herring=K skin=EXS_* flags=EXF_A|EXF_B near_flag=SF_* lost_flag=SF_*"""
    def sym(k, dflt, prefix=None):
        v = a.get(k)
        if v is None:
            return dflt
        return comp.sym(v, where, prefix)
    def script(k):
        v = a.get(k)
        if not v:
            return 0
        if v not in comp.script_names:
            die(where, "@possessor %s=%s: unknown script" % (k, v))
        return comp.script_names[v]
    if "name" not in a:
        die(where, "@possessor needs name=")
    host = a.get("host", "script")
    host = 0xFFFF if host == "script" else 0xFFFE if host == "practice" else comp.who(host, where)
    flags = 0
    for fl in a.get("flags", "").split("|"):
        if fl:
            flags |= comp.sym(fl, where, "EXF_")
    return ('    { "%s", %s, %s, %du, %du, %du, %du, %d, %d, %d, %d, %du, %du },\n' % (
        c_str(a["name"]), c_strs(a.get("offer", ""), 4), c_strs(a.get("clue", ""), 4), host,
        sym("name_res", 0xFFFF), script("saved"), script("lost"), sym("near_miss", 0xFF), sym("herring", 0xFF),
        sym("skin", 0, "EXS_"), flags, sym("near_flag", 0, "SF_"), sym("lost_flag", 0, "SF_")))


def main(argv):
    out_root = ROOT
    bin_out = None
    i = 0
    while i < len(argv):
        if argv[i] == "--out-root":
            out_root = os.path.abspath(argv[i + 1])
            i += 2
        elif argv[i] == "--data":                 # tests: compile another tree of .dsc
            global DATA
            DATA = os.path.abspath(argv[i + 1])
            i += 2
        elif argv[i] == "--bin":
            bin_out = os.path.abspath(argv[i + 1])
            i += 2
        else:
            print("textpack: unknown argument %s" % argv[i], file=sys.stderr)
            return 2
    syms = load_symbols()
    comp = Compiler(syms)
    files = []
    for d, _, fs in os.walk(DATA):
        for f in fs:
            if f.endswith(".dsc"):
                files.append(os.path.join(d, f))
    for f in sorted(files):
        comp.read(f)
    comp.build()
    missions = mission_rows(comp)
    packs = comp.packs
    for p in packs:
        p.all_ops, p.script_spans = [], []
        for s in p.scripts:
            p.script_spans.append((len(p.all_ops), len(s[2]), 0))
            p.all_ops.extend(s[2])
        p.all_events, p.beat_spans = [], []
        for b in p.beats:
            p.beat_spans.append((len(p.all_events), len(b[2])))
            p.all_events.extend(b[2])
    # the codec over all text
    all_strings = [s for p in packs for s in p.strings]
    dic = build_dictionary(all_strings)
    by_first = {}
    for k, w in sorted(enumerate(dic), key=lambda kw: (-len(kw[1]), kw[0])):
        by_first.setdefault(w[0], []).append((k, w))
    toks = {s: tokenize(s, by_first) for s in all_strings}
    freq = {0: 1}
    for s in all_strings:
        for t in toks[s]:
            freq[t] = freq.get(t, 0) + 1
    lengths = huffman_lengths(freq)
    codes, counts, order = canonical(lengths)
    blobs = []
    for p in packs:
        streams = [encode(toks[s], codes) for s in p.strings]
        for s, st in zip(p.strings, streams):
            if decode(st, counts, order, dic) != s:
                raise Fail("%s: codec round trip failed for %r" % (p.path, s))
        blob = pack_bytes(p, streams)
        if len(blob) > CAPS[p.kind]:
            raise Fail("%s: pack %s is %d B, over the %s cap of %d B" % (p.path, p.name, len(blob), p.kind, CAPS[p.kind]))
        blobs.append(blob)
    gen = os.path.join(out_root, "core", "gen")
    os.makedirs(gen, exist_ok=True)
    head = "/* GENERATED by core/tools/textpack.py from core/data/story/ - do not edit. */\n"
    for p, blob in zip(packs, blobs):
        body, n = c_words(blob)
        with open(os.path.join(gen, "text_%s.c" % c_ident(p.name)), "w") as f:
            f.write(head)
            f.write("/* pack %d \"%s\" (%s, %s): %d B, %d strings, %d scripts, %d ops, %d beats, %d events */\n" %
                    (p.id, p.name, p.kind, p.path, len(blob), len(p.strings), len(p.scripts), len(p.all_ops),
                     len(p.beats), len(p.all_events)))
            f.write("#include \"doomo/types.h\"\n\n")
            f.write("const uint32_t doomo_text_pack_%s[%d] = {\n%s\n};\n" % (c_ident(p.name), max(1, n), body or "    0"))
    with open(os.path.join(gen, "text_dir.c"), "w") as f:
        f.write(head)
        f.write("/* The pack directory for ROM platforms (GBA, native): a pack is \"loaded\"\n"
                " * by pointing at it. The PS1 excludes core/gen/text_*.c (this file\n"
                " * too) and reads TEXT.BIN into its window instead (ps1/src/txtload.c). */\n")
        f.write("#include \"doomo/txt.h\"\n\n")
        for p in packs:
            f.write("extern const uint32_t doomo_text_pack_%s[];\n" % c_ident(p.name))
        f.write("\nstatic const uint32_t *const packs[%d] = {\n" % max(1, len(packs)))
        for p in packs:
            f.write("    doomo_text_pack_%s,\n" % c_ident(p.name))
        if not packs:
            f.write("    0\n")
        f.write("};\n\nconst uint8_t *txt_plat_pack(int pack)\n{\n"
                "    if (pack < 0 || pack >= %d) return 0;\n"
                "    return (const uint8_t *)packs[pack];\n}\n\n"
                "int txt_plat_load(int pack)\n{\n"
                "    return txt_plat_pack(pack) != 0;\n}\n" % len(packs))
    with open(os.path.join(gen, "txt_dict.c"), "w") as f:
        f.write(head)
        f.write("/* The shared text codec tables (SF sec.16.3): %d dictionary tokens and the\n"
                " * canonical Huffman code over %d symbols (lengths <= %d). Every platform\n"
                " * links this file (resident on the PS1), plus the pack directory: names,\n"
                " * kinds and sizes (TEXT.BIN sectors are derived from the sizes). */\n" %
                (len(dic), len(order), MAX_CODE_LEN))
        f.write("#include \"doomo/txt.h\"\n\n")
        f.write("const uint16_t doomo_txt_npacks = %d;\n" % len(packs))
        f.write("const char doomo_txt_pack_names[%d][9] = {\n" % max(1, len(packs)))
        for p in packs:
            f.write("    \"%s\",\n" % p.name)
        if not packs:
            f.write("    \"\"\n")
        f.write("};\n")
        f.write("const uint8_t doomo_txt_pack_kinds[%d] = { %s };\n" %
                (max(1, len(packs)), ", ".join(str(KINDS[p.kind]) for p in packs) or "0"))
        f.write("const uint8_t doomo_txt_pack_chapters[%d] = { %s };\n" %
                (max(1, len(packs)), ", ".join(str(p.chapter) for p in packs) or "0"))
        f.write("const uint16_t doomo_txt_pack_minutes[%d] = { %s };\n" %
                (max(1, len(packs)), ", ".join(str(p.minutes) for p in packs) or "0"))
        f.write("const uint16_t doomo_txt_pack_bytes[%d] = { %s };\n" %
                (max(1, len(packs)), ", ".join(str(len(b)) for b in blobs) or "0"))
        f.write("const uint8_t doomo_txt_huff_count[%d] = { %s };\n" % (MAX_CODE_LEN + 1, ", ".join(str(c) for c in counts)))
        f.write("const uint16_t doomo_txt_huff_nsym = %d;\n" % len(order))
        f.write("const uint16_t doomo_txt_huff_sym[%d] = {\n" % max(1, len(order)))
        for i in range(0, len(order), 12):
            f.write("    " + ", ".join(str(s) for s in order[i:i + 12]) + ",\n")
        if not order:
            f.write("    0\n")
        f.write("};\n")
        offs = [0]
        chars = ""
        for w in dic:
            chars += w
            offs.append(len(chars))
        f.write("const uint16_t doomo_txt_dict_n = %d;\n" % len(dic))
        f.write("const uint16_t doomo_txt_dict_off[%d] = {\n" % (len(dic) + 1))
        for i in range(0, len(offs), 12):
            f.write("    " + ", ".join(str(o) for o in offs[i:i + 12]) + ",\n")
        f.write("};\n")
        f.write("const char doomo_txt_dict_chars[%d] =\n" % (len(chars) + 1))
        if not chars:
            f.write('    ""\n')
        for k in range(0, len(chars), 60):
            f.write('    "%s"\n' % c_str(chars[k:k + 60]))
        f.write("    ;\n")
    with open(os.path.join(gen, "side_table.c"), "w") as f:
        f.write(head)
        f.write("/* The side missions S100..S217 (@mission headers; story_vm.h StorySideRow).\n"
                " * Rows are indexed by mission number - 100; kind 0 = retired / unwritten. */\n")
        f.write("#include \"doomo/story_vm.h\"\n\n")
        f.write("const StorySideRow doomo_side_rows[STORY_SIDE2_ROWS] = {\n")
        for n in range(118):
            r = missions.get(n)
            if not r:
                f.write("    { %d, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFFFFu, 0xFFFFu, 0xFFFFu, 0xFFFFu, 0, 0 },\n" % n)
                continue
            f.write("    { %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %du, %du, %du, 0x%04Xu, 0x%04Xu, 0x%04Xu, 0x%04Xu, %d, %d },\n" % (
                r["sid"], r["kind"], r["lo"], r["hi"], r["side"], r["chain"], r["step"], r["req"], r["later"], r["tag"],
                r["giver"], r["verbs"], r["reward"], r["offer"], r["active"], r["title"], r["trophy"], r["pack"],
                r["spot"]))
        f.write("};\n")
        # (S4) the givers' cells (at=), indexed by StorySideRow.spot - 1
        f.write("\n/* (S4) where the givers stand: StorySideRow.spot - 1 -> { x, y } */\n")
        f.write("const uint8_t doomo_side_spots[%d][2] = {\n" % max(1, len(comp.spots)))
        for x, y in comp.spots:
            f.write("    { %d, %d },\n" % (x, y))
        if not comp.spots:
            f.write("    { 0, 0 }\n")
        f.write("};\nconst uint16_t doomo_side_spot_count = %d;\n" % len(comp.spots))
    with open(os.path.join(gen, "possess_table.c"), "w") as f:
        f.write(head)
        f.write("/* Authored possessors (the @possessor blocks, placed by row=N: the row\n"
                " * index is the true-name id the VM's POSSESS op carries; exorcism.h\n"
                " * DoomoPossessor, S3). Until the blocks cover every row this file\n"
                " * defines no table and exorcism.c's weak default rows hold; the blocks\n"
                " * written so far are doomo_possess_dsc[] (tests). */\n")
        f.write("#include \"doomo/doomo.h\"\n#include \"doomo/exorcism.h\"\n#include \"doomo/story_vm.h\"\n\n")
        # (S2) rows are placed by row=N (default: the next index in file order).
        # The table only becomes strong (replacing exorcism.c's weak default
        # rows) once the blocks cover rows 0..POSSESS_ROWS_FULL-1 without a
        # gap: until then every package's rows live in the defaults and the
        # blocks written so far are emitted as doomo_possess_dsc[] for the
        # test that holds them equal to the default rows (test_story.c).
        placed = {}
        nxt = 0
        for where, a, pack in comp.possessors:
            r = int(a["row"]) if "row" in a else nxt
            if r in placed:
                die(where, "@possessor row=%d defined twice" % r)
            placed[r] = (where, a)
            nxt = r + 1
        order = sorted(placed)
        full = order and order == list(range(len(order))) and len(order) >= POSSESS_ROWS_FULL
        if full:
            f.write("static const DoomoPossessor rows[%d] = {\n" % len(order))
            for r in order:
                f.write(possessor_row(comp, placed[r][0], placed[r][1]))
            f.write("};\nconst DoomoPossessor *const doomo_possess_table = rows;\n"
                    "const uint16_t doomo_possess_count = %d;\n\n" % len(order))
        f.write("/* the @possessor blocks written so far and their rows (S2: tests) */\n")
        f.write("const DoomoPossessor doomo_possess_dsc[%d] = {\n" % max(1, len(order)))
        for r in order:
            f.write(possessor_row(comp, placed[r][0], placed[r][1]))
        if not order:
            f.write("    { 0, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, 0xFFFFu, 0xFFFFu, 0, 0, 0xFF, 0xFF, 0, 0, 0, 0 }\n")
        f.write("};\nconst uint8_t doomo_possess_dsc_row[%d] = { %s };\n" %
                (max(1, len(order)), ", ".join(str(r) for r in order) or "0"))
        f.write("const uint16_t doomo_possess_dsc_count = %d;\n\n" % len(order))
        armed = armed_rows(comp)
        f.write("/* @armed: the beats in which a host's authored possession is the story's\n"
                " * (story.c story_rearm_possessors re-arms them after a load). */\n")
        f.write("const StoryArmed doomo_story_armed[%d] = {\n" % max(1, len(armed)))
        for host, row, lo, hi, unless in armed:
            f.write("    { %du, %d, %d, %d, 0, { %du, %du } },\n" % (host, row, lo, hi, unless[0], unless[1]))
        if not armed:
            f.write("    { 0xFFFFu, 0, 0, 0, 0, { 0xFFFFu, 0xFFFFu } }\n")
        f.write("};\nconst uint16_t doomo_story_armed_count = %d;\n" % len(armed))
    with open(os.path.join(gen, "story_ids.h"), "w") as f:
        f.write(head)
        f.write("#ifndef DOOMO_STORY_IDS_H\n#define DOOMO_STORY_IDS_H\n\n")
        for p in packs:
            f.write("#define PACK_%s %d\n" % (c_ident(p.name).upper(), p.id))
        f.write("#define TXT_NPACKS %d\n\n" % len(packs))
        for name, v in sorted(comp.script_names.items(), key=lambda kv: kv[1]):
            if not name.startswith("__"):
                f.write("#define SCR_%s %du\n" % (name.upper(), v))
        f.write("\n")
        for name, v in sorted(comp.beat_names.items(), key=lambda kv: kv[1]):
            f.write("#define BSCR_%s %du\n" % (name.upper(), v))
        f.write("\n")
        for name, v in sorted(comp.string_names.items(), key=lambda kv: kv[1]):
            f.write("#define TXT_%s %du\n" % (name.upper(), v))
        f.write("\n#endif\n")
    if bin_out:
        # TEXT.BIN: sector 0 = {u32 'TEXT', u32 n, n x {u32 sector, u32 bytes}}, then
        # every pack at a sector boundary (ps1/src/txtload.c)
        dir_ = u32(0x54584554) + u32(len(packs))
        sec = 1
        body = b""
        for p, blob in zip(packs, blobs):
            dir_ += u32(sec) + u32(len(blob))
            pad = (-len(blob)) % SECTOR
            body += blob + b"\0" * pad
            sec += (len(blob) + pad) // SECTOR
        if len(dir_) > SECTOR:
            raise Fail("TEXT.BIN directory over one sector")
        os.makedirs(os.path.dirname(bin_out), exist_ok=True)
        with open(bin_out, "wb") as f:
            f.write(dir_ + b"\0" * (SECTOR - len(dir_)) + body)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except Fail as e:
        print("textpack: error: %s" % e, file=sys.stderr)
        sys.exit(1)
