"""talkc_parse.py - the parser bank TB_PARSE for talkc.py (owner: TK, T3;
docs/TALK.md "The parser"). Imported by talkc.py; stdlib only.

Inputs:
  core/data/talk/rules/<lang>.tsv   the parser's word classes and rules
  the lexicon (every concept's lemma, plural and synonyms, including the
  never-generated safety concepts: the parser must KNOW them to refuse them)
  core/data/talk/safety/phrases_<lang>.tsv   extra safety phrases (T4)
  core/data/talk/safety/suggestive_<lang>.tsv   which sexual-class concepts
                                   are flirting words, not explicit ones (T4)

rules/<lang>.tsv (UTF-8, '#' comments):
  @class NAME[ +topic]<TAB>member, member, multi word member, ...
      A closed word class (YOU, QWORD, LIKEV ...). Members are surface
      words (canonical: see canon()). A word in a class is a function word
      in that use: its concept (if it has one) is never the topic, unless
      the class says +topic.
  PRIO<TAB>PATTERN<TAB>ACT[(c)][ ACT[(c)]]
      PRIO 1..899 (higher first; ties keep file order). The safety classes
      are not rules: talk_safe.c classifies them before any rule runs.
      PATTERN: space separated elements, matched against the tokens
      (unanchored unless it starts with ^):
        ^  $        start / end of the line
        .*          any number of tokens (lazy)
        .           any one token
        NAME        a token of class NAME
        [PRED]      a predicate (PREDS below)
        "word"      that literal (canonical) word
        (A|[P]|"w") any of these atoms ($ among them: or the end of the line)
        !ATOM       one token that is NOT the atom (or alternation)
        ATOM?       optional
        ATOM*       any number of them (lazy)
        c:ATOM      capture the token into c (a lowercase letter; the act
                    names it: ASK_ABOUT(c) takes its concept as the topic,
                    or its person as the subject / other party)
      ACT: a TA_* name from talk.h without TA_ (read from the header, so
      the ids never drift); the second act is optional.

The bank (TB_PARSE, DC and native only; all little endian):
  header 64 B: u32 magic 'PRS1', u32 nwords, u32 words_off, u32 ntagsets,
    u32 tagsets_off, u32 nrules, u32 rules_off, u32 rules_len, u32 nsafe,
    u32 safe_off, u32 str_off, u32 str_len, u32 max_words, u32 nclasses,
    u32 0, u32 0
  words   nwords x 12 B, sorted by key bytes: u32 key (string offset),
          u16 concept (0 none), u16 flags (TW_*), u16 tagset, u8 nwords
          (words in the key), u8 key length
  tagsets ntagsets x 16 B: 4 x u32 class bits (tagset 0 is empty)
  rules   each: u16 prio, u8 act0, u8 act1, u8 cap0, u8 cap1, u8 nel,
          u8 nlit, then nel x 24 B elements: u32 cls[4], u32 preds, u8 op,
          u8 flags (1 optional, 2 not, 4 any number, 8 or the end), u8 cap,
          u8 nlit (its literals), then nlit x u32: the FNV-1a 32 of each
          literal word, element by element
  safe    nsafe x u16: the word indices of single-word safety entries (the
          classifier's edit-distance-1 pass)
  strings NUL-terminated keys
"""
import os
import re

TW = {"func": 0x0001, "suggest": 0x0002, "sexual": 0x0004, "hate": 0x0008, "distress": 0x0010,
      "minor": 0x0020, "realworld": 0x0040, "strong": 0x0080, "crude": 0x0100, "capped": 0x0200,
      "topic": 0x0400, "group": 0x0800, "minorx": 0x1000 | 0x0020, "name": 0x2000}
TW_SAFETY = TW["sexual"] | TW["hate"] | TW["distress"] | TW["minor"] | TW["suggest"]
PREDS = ["ANY", "CONCEPT", "PERSON", "ABSENT", "ADJPOS", "ADJNEG", "QMARK", "EXCL", "NUM", "UNK",
         "SEXUAL", "SUGGEST", "HATE", "DISTRESS", "MINOR", "REALWORLD", "STRONG", "CRUDE", "FOOD",
         "ACTIVITY", "FEELING", "PLACE", "THING", "ROMANCE", "PEOPLE", "ADJ", "JOB", "TIME",
         "CREATURE", "FEAR", "NAME", "LORE"]
OP = {"match": 0, "gap": 1, "start": 2, "end": 3}
EF = {"opt": 1, "neg": 2, "star": 4, "orend": 8}
MAX_CLASSES = 128
FIXED_CLASSES = ["EVERYONE", "HATEV", "YEARS", "YOU", "I", "IM", "GREETW", "HATEADJ", "ATTRACT"]
MAX_ELEMS = 12
MAX_KEY_WORDS = 4
PRS_MAGIC = 0x31535250          # "PRS1"

# Latin-1 letters folded to their base (talk_lex.c fold_latin1 is the same table)
_FOLD_SRC = "àáâãäåçèéêëìíîï" \
            "ñòóôõöùúûüýÿ"
_FOLD_DST = "aaaaaaceeeeiiiinooooouuuuyy"
FOLD = dict(zip(_FOLD_SRC, _FOLD_DST))
FOLD.update({"ß": "ss", "æ": "ae", "ø": "o", "‘": "'", "’": "'", "´": "'"})


def canon(s):
    """the canonical key of a surface form or an input word run: ASCII
    lowercase, accents folded, '.' dropped, letters / digits / ' kept,
    everything else a space, runs of spaces one, apostrophes trimmed at the
    ends of words. talk_lex.c talk_lex_canon() is the same function."""
    out = []
    for ch in s:
        ch = ch.lower() if ord(ch) < 0x250 else ch
        for c in FOLD.get(ch, ch):
            if "A" <= c <= "Z":
                c = c.lower()
            if "a" <= c <= "z" or "0" <= c <= "9" or c == "'":
                out.append(c)
            elif c == ".":
                pass
            else:
                out.append(" ")
    words = [w.strip("'") for w in "".join(out).split()]
    return " ".join(w for w in words if w)


def act_ids(talk_h):
    """{name: id} of talk.h's TA_* enum (TA_NONE = 0, in order)"""
    src = open(talk_h, encoding="utf-8").read()
    m = re.search(r"enum\s*\{\s*TA_NONE\s*=\s*0,(.*?)TA_COUNT", src, re.S)
    if not m:
        raise ValueError("talk.h: the TA_* enum")
    body = re.sub(r"/\*.*?\*/", "", m.group(1), flags=re.S)
    names = [t.strip() for t in body.split(",") if t.strip()]
    ids = {"NONE": 0}
    for i, n in enumerate(names):
        if not n.startswith("TA_"):
            raise ValueError("talk.h: %r in the TA_* enum" % n)
        ids[n[3:]] = i + 1
    return ids


class Rules:
    def __init__(self):
        self.classes = []          # [(name, topic, [canon keys], where)]
        self.class_bit = {}
        self.rules = []            # dicts
        self.literals = {}         # canon word -> class bit


def _split_atoms(s):
    """'(A|B|"c")' -> ['A','B','"c"']; 'A' -> ['A']"""
    if s.startswith("(") and s.endswith(")"):
        return [a for a in s[1:-1].split("|")]
    return [s]


def load_rules(path, acts, die, rel):
    R = Rules()
    if not os.path.exists(path):
        return None
    lines = open(path, encoding="utf-8").read().split("\n")
    pending = []
    for n, line in enumerate(lines, 1):
        where = "%s:%d" % (rel(path), n)
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        if line.startswith("@class"):
            cols = line.split("\t")
            head = cols[0].split()
            if len(cols) != 2 or len(head) not in (2, 3) or (len(head) == 3 and head[2] != "+topic"):
                die(where, "@class NAME[ +topic]<TAB>member, member ...")
            name = head[1]
            if not re.match(r"^[A-Z][A-Z0-9_]*$", name) or name in PREDS:
                die(where, "class name %r: UPPERCASE, not a predicate name" % name)
            if name in R.class_bit:
                die(where, "class %s twice" % name)
            members = []
            for m in cols[1].split(","):
                k = canon(m)
                if not k:
                    continue
                if len(k.split()) > MAX_KEY_WORDS:
                    die(where, "%r: at most %d words" % (m, MAX_KEY_WORDS))
                if k in members:
                    die(where, "%s: %r twice" % (name, m.strip()))
                members.append(k)
            if not members:
                die(where, "class %s is empty" % name)
            R.class_bit[name] = len(R.classes)
            R.classes.append((name, len(head) == 3, members, where))
            continue
        cols = line.split("\t")
        if len(cols) != 3 or not cols[0].strip().isdigit():
            die(where, "PRIO<TAB>PATTERN<TAB>ACT[(c)][ ACT[(c)]]")
        pending.append((int(cols[0]), cols[1].strip(), cols[2].strip(), where))
    order = 0
    for prio, pat, actspec, where in pending:
        if not 1 <= prio <= 899:
            die(where, "PRIO 1..899 (the safety classes are not rules)")
        caps = []
        els = []
        for tok in pat.split():
            el = {"op": "match", "flags": 0, "cap": 0, "cls": set(), "preds": set()}
            if tok == "^":
                el["op"] = "start"
                els.append(el)
                continue
            if tok == "$":
                el["op"] = "end"
                els.append(el)
                continue
            if tok == ".*":
                el["op"] = "gap"
                els.append(el)
                continue
            m = re.match(r"^(?:([a-z]):)?(!)?(.+?)([?*])?$", tok)
            if not m:
                die(where, "element %r" % tok)
            cap, neg, body, opt = m.groups()
            if cap:
                if cap not in caps:
                    caps.append(cap)
                if len(caps) > 3:
                    die(where, "at most 3 captures")
                el["cap"] = caps.index(cap) + 1
            if neg:
                el["flags"] |= EF["neg"]
            if opt == "?":
                el["flags"] |= EF["opt"]
            elif opt == "*":
                el["flags"] |= EF["star"]
            if body == ".":
                el["preds"].add("ANY")
            else:
                for a in _split_atoms(body):
                    if a == "$" and body.startswith("("):
                        el["flags"] |= EF["orend"]
                    elif a.startswith("[") and a.endswith("]"):
                        if a[1:-1] not in PREDS:
                            die(where, "unknown predicate %s (one of %s)" % (a, " ".join(PREDS)))
                        el["preds"].add(a[1:-1])
                    elif a.startswith('"') and a.endswith('"') and len(a) > 2:
                        w = canon(a[1:-1])
                        if not w or " " in w:
                            die(where, "literal %s: one word (make a class for a phrase)" % a)
                        el.setdefault("lits", [])
                        if w not in el["lits"]:
                            el["lits"].append(w)
                    elif a in R.class_bit:
                        el["cls"].add(a)
                    else:
                        die(where, "unknown class or bad atom %r" % a)
            els.append(el)
        if not els or len(els) > MAX_ELEMS:
            die(where, "1..%d elements" % MAX_ELEMS)
        out_acts = []
        for a in actspec.split():
            m = re.match(r"^([A-Z_]+)(?:\(([a-z])\))?$", a)
            if not m or m.group(1) not in acts or m.group(1) == "NONE":
                die(where, "act %r: a TA_* name of talk.h without TA_" % a)
            aid = acts[m.group(1)]
            if aid >= acts["REFUSE_SEXUAL"]:
                die(where, "%s is a safety act: the gate makes those, never a rule" % m.group(1))
            c = 0
            if m.group(2):
                if m.group(2) not in caps:
                    die(where, "%s(%s): no capture %s in the pattern" % (m.group(1), m.group(2), m.group(2)))
                c = caps.index(m.group(2)) + 1
            out_acts.append((aid, c))
        if not 1 <= len(out_acts) <= 2:
            die(where, "one or two acts")
        R.rules.append({"prio": prio, "els": els, "acts": out_acts, "where": where, "order": order})
        order += 1
    # talk_parse.c / talk_safe.c know these classes by their bit
    names = [c[0] for c in R.classes]
    if names[:len(FIXED_CLASSES)] != FIXED_CLASSES:
        die(rel(path), "the first classes must be %s, in that order (talk_safe.c reads them by bit)"
            % ", ".join(FIXED_CLASSES))
    for r in R.rules:
        for el in r["els"]:
            if len(el.get("lits", [])) > 255:
                die(r["where"], "at most 255 literals in one element")
    if len(R.classes) > MAX_CLASSES:
        die(rel(path), "%d classes and literals, at most %d" % (len(R.classes), MAX_CLASSES))
    R.rules.sort(key=lambda r: (-r["prio"], r["order"]))
    # the everyday words the parser must know (rules/common_<lang>.txt)
    R.common = []
    cpath = os.path.join(os.path.dirname(path), "common_" + os.path.basename(path)[:-4] + ".txt")
    if os.path.exists(cpath):
        for line in open(cpath, encoding="utf-8").read().split("\n"):
            if line.startswith("#"):
                continue
            for w in line.split():
                k = canon(w)
                if k and k not in R.common:
                    R.common.append(k)
    # first names that are no word (rules/names_<lang>.txt): known, so the
    # typo pass never turns "Pedro" into a safety word; talk_lex.c makes one
    # a PERSON when a resident of the town has that name
    R.names = []
    npath = os.path.join(os.path.dirname(path), "names_" + os.path.basename(path)[:-4] + ".txt")
    if os.path.exists(npath):
        for n, line in enumerate(open(npath, encoding="utf-8").read().split("\n"), 1):
            if line.startswith("#"):
                continue
            for w in line.split():
                k = canon(w)
                if not k or " " in k:
                    die("%s:%d" % (rel(npath), n), "a name is one word: %r" % w)
                if k not in R.names:
                    R.names.append(k)
    return R


def load_phrases(path, die, rel):
    """safety/phrases_<lang>.tsv: CLASS<TAB>phrase, phrase ... (CLASS sexual,
    suggest, hate, distress, minor, realworld, group: a group of real
    people, which is hate after a HATEV word)"""
    out = []
    if not os.path.exists(path):
        return out
    for n, line in enumerate(open(path, encoding="utf-8").read().split("\n"), 1):
        where = "%s:%d" % (rel(path), n)
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        cols = line.split("\t")
        if len(cols) != 2 or cols[0] not in ("sexual", "suggest", "hate", "distress", "minor", "minorx", "realworld", "group"):
            die(where, "CLASS<TAB>phrase, phrase ... (sexual suggest hate distress minor minorx realworld group)")
        for p in cols[1].split(","):
            k = canon(p)
            if k:
                if len(k.split()) > MAX_KEY_WORDS:
                    die(where, "%r: at most %d words" % (p, MAX_KEY_WORDS))
                out.append((k, cols[0], where))
    return out


def load_suggestive(path, by_key, die, rel):
    keys = set()
    if not os.path.exists(path):
        return keys
    for n, line in enumerate(open(path, encoding="utf-8").read().split("\n"), 1):
        where = "%s:%d" % (rel(path), n)
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        for k in line.split():
            c = by_key.get(k)
            if c is None:
                die(where, "%s is not a concept" % k)
            if c.reg != "sexual":
                die(where, "%s is not a sexual-class concept" % k)
            keys.add(k)
    return keys


def word_table(b, R, phrases, suggestive, problems):
    """{key: [concept_id, flags, set(class names)]} over every concept surface,
    the class members and the safety phrases"""
    tab = {}
    for c in b.concepts:
        fl = 0
        if c.reg == "sexual":
            fl |= TW["suggest"] if c.key in suggestive else TW["sexual"]
        elif c.reg in ("hate", "distress", "realworld", "strong", "crude"):
            fl |= TW[c.reg]
        if c.cat.key == "safety.capped":
            fl |= TW["capped"]
        if c.fl & 0x0800:                                  # TCF_MINOR_WORD
            fl |= TW["minor"]
        if c.cat.key == "safety.minor":                    # only ever a child: with a liking word, K-7
            fl |= TW["minorx"]
        for s in [c.lemma] + ([c.plural] if c.plural else []) + c.syn:
            k = canon(s)
            if not k:
                continue
            if len(k.split()) > MAX_KEY_WORDS:
                continue
            e = tab.get(k)
            if e is None:
                tab[k] = [c.id, fl, set()]
            elif e[0] != c.id:
                # two concepts share a canonical key ("lo-fi" / "lo fi"): the
                # stronger safety class wins, then the lower id (deterministic)
                if (fl & TW_SAFETY) and not (e[1] & TW_SAFETY):
                    tab[k] = [c.id, fl | e[1], e[2]]
                elif not ((e[1] & TW_SAFETY) and not (fl & TW_SAFETY)) and c.id < e[0]:
                    tab[k] = [c.id, fl | e[1], e[2]]
                else:
                    e[1] |= fl & (TW_SAFETY | TW["strong"] | TW["crude"] | TW["capped"] | TW["realworld"])
    for name, topic, members, where in R.classes:
        for k in members:
            e = tab.setdefault(k, [0, 0, set()])
            e[2].add(name)
            if not name.startswith('"'):
                e[1] |= TW["topic"] if topic else TW["func"]
    for k, cls, where in phrases:
        e = tab.setdefault(k, [0, 0, set()])
        e[1] |= TW[cls]
    for k in getattr(R, "common", ()):
        tab.setdefault(k, [0, 0, set()])
    for k in getattr(R, "names", ()):
        e = tab.setdefault(k, [0, 0, set()])
        if not e[0] and not (e[1] & ~TW["name"]) and not e[2]:
            e[1] |= TW["name"]                 # a word of its own ("bob", "will") stays a word
    # every word of a phrase is a word the parser knows, so the typo pass
    # never "corrects" the "kill" of "kill myself" or the "heil" of a hate phrase
    for k in list(tab):
        if " " in k:
            for w in k.split():
                if len(w) > 1:
                    tab.setdefault(w, [0, 0, set()])
    # a word that is a function word somewhere and +topic elsewhere stays a topic candidate
    for k, e in tab.items():
        if e[1] & TW["topic"]:
            e[1] &= ~TW["func"]
    return tab


def _u16(v):
    return (v & 0xFFFF).to_bytes(2, "little")


def _u32(v):
    return (v & 0xFFFFFFFF).to_bytes(4, "little")


def fnv32(s):
    """FNV-1a 32 of a canonical word (talk_lex.c tok_hash)"""
    h = 0x811C9DC5
    for b in s.encode("latin-1"):
        h = ((h ^ b) * 0x01000193) & 0xFFFFFFFF
    return h


def build_parse_bank(tab, R):
    keys = sorted(tab, key=lambda k: k.encode("latin-1"))
    strings = bytearray()
    str_off = {}
    for k in keys:
        str_off[k] = len(strings)
        strings += k.encode("latin-1") + b"\0"
    tagsets = [(0, 0, 0, 0)]
    tagset_ix = {(0, 0, 0, 0): 0}
    words = bytearray()
    safe = []
    for i, k in enumerate(keys):
        cid, fl, cls = tab[k]
        bits = [0, 0, 0, 0]
        for name in cls:
            bit = R.class_bit[name]
            bits[bit >> 5] |= 1 << (bit & 31)
        t = tuple(bits)
        if t not in tagset_ix:
            tagset_ix[t] = len(tagsets)
            tagsets.append(t)
        nw = len(k.split())
        if len(k) > 255:
            raise ValueError("key too long: %r" % k)
        words += _u32(str_off[k]) + _u16(cid) + _u16(fl) + _u16(tagset_ix[t]) + bytes((nw, len(k)))
        if nw == 1 and fl & (TW["sexual"] | TW["suggest"] | TW["hate"] | TW["distress"] | TW["minor"] | TW["group"]) \
                and len(k) >= 3:
            safe.append(i)
    rules = bytearray()
    pred_bit = {p: i for i, p in enumerate(PREDS)}
    for r in R.rules:
        (a0, c0) = r["acts"][0]
        (a1, c1) = r["acts"][1] if len(r["acts"]) > 1 else (0, 0)
        lits = [w for el in r["els"] for w in el.get("lits", [])]
        if len(lits) > 64:
            raise ValueError("%s: over 64 literals in one rule (talk_parse.c LIT_MAX)" % r["where"])
        rules += _u16(r["prio"]) + bytes((a0, a1, c0, c1, len(r["els"]), len(lits)))
        for el in r["els"]:
            bits = [0, 0, 0, 0]
            for name in el["cls"]:
                bit = R.class_bit[name]
                bits[bit >> 5] |= 1 << (bit & 31)
            pm = 0
            for p in el["preds"]:
                pm |= 1 << pred_bit[p]
            rules += b"".join(_u32(x) for x in bits) + _u32(pm) + \
                bytes((OP[el["op"]], el["flags"], el["cap"], len(el.get("lits", []))))
        rules += b"".join(_u32(fnv32(w)) for w in lits)
    tag_b = b"".join(b"".join(_u32(x) for x in t) for t in tagsets)
    safe_b = b"".join(_u16(i) for i in safe)
    hdr_len = 64
    words_off = hdr_len
    tags_off = words_off + len(words)
    rules_off = tags_off + len(tag_b)
    safe_off = rules_off + len(rules)
    s_off = safe_off + len(safe_b)
    max_words = max(len(k.split()) for k in keys)
    hdr = b"".join(_u32(x) for x in (PRS_MAGIC, len(keys), words_off, len(tagsets), tags_off, len(R.rules),
                                    rules_off, len(rules), len(safe), safe_off, s_off, len(strings),
                                    max_words, len(R.classes), 0, 0))
    assert len(hdr) == hdr_len
    body = hdr + bytes(words) + tag_b + bytes(rules) + safe_b + bytes(strings)
    info = {"words": len(keys), "tagsets": len(tagsets), "rules": len(R.rules), "classes": len(R.classes),
            "safe": len(safe), "strings": len(strings)}
    return body, info


# ---------------------------------------------------------------------------
# corpora (TK8: they must parse; the C tests measure them)
# ---------------------------------------------------------------------------
def check_corpus(path, acts, by_key, die, rel, kind):
    """parse_<lang>.tsv:  TEXT<TAB>ACT[,ACT2]<TAB>TOPIC|-<TAB>TARGET   (@room A, B, C lines)
       safety_<lang>.tsv: TEXT<TAB>EXPECT<TAB>ROOM   (EXPECT: ok K7 DEFLECT ROMANCE or a refusal)
       adversarial_<lang>.tsv: TEXT<TAB>EXPECT<TAB>ROOM"""
    if not os.path.exists(path):
        return 0
    n = 0
    room = None
    for i, line in enumerate(open(path, encoding="utf-8").read().split("\n"), 1):
        where = "%s:%d" % (rel(path), i)
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        if line.startswith("@room"):
            room = [s.strip() for s in line[5:].split(",") if s.strip()]
            if not 1 <= len(room) <= 4:
                die(where, "@room NAME, NAME ... (1..4)")
            continue
        cols = line.split("\t")
        if kind == "parse":
            if len(cols) != 4:
                die(where, "TEXT<TAB>ACT[,ACT]<TAB>TOPIC|-<TAB>TARGET")
            if room is None:
                die(where, "no @room line before it")
            for a in cols[1].split(","):
                if a not in acts or a == "NONE":
                    die(where, "act %r" % a)
            if cols[2] != "-" and cols[2] not in by_key:
                die(where, "topic %r is not a concept" % cols[2])
            if cols[3] not in ("-", "all") and cols[3] not in room:
                die(where, "target %r: '-' (the ring), 'all' or a name of the @room" % cols[3])
        else:
            if len(cols) != 3:
                die(where, "TEXT<TAB>EXPECT<TAB>ROOM")
            if cols[1] not in ("ok", "K7", "DEFLECT", "ROMANCE", "REFUSE_SEXUAL", "REFUSE_HATE", "DISTRESS",
                               "REFUSE_MINOR"):
                die(where, "expect %r: ok K7 DEFLECT ROMANCE REFUSE_SEXUAL REFUSE_HATE DISTRESS REFUSE_MINOR"
                    % cols[1])
            if cols[2] not in ("adults", "kid", "kidtarget", "pet", "teenavatar", "kidavatar"):
                die(where, "room %r: adults kid kidtarget pet teenavatar kidavatar" % cols[2])
        if len(cols[0]) > 80:
            die(where, "the line is over 80 characters (the Dreamcast field)")
        n += 1
    return n
