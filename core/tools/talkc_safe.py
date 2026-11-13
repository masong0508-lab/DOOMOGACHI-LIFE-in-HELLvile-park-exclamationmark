"""talkc_safe.py - Talk safety data for talkc.py (owner: TK, T4; docs/TALK.md
"Safety"). Imported by talkc.py; stdlib only.

1. The safety bank TB_SAFETY (every build: the PS1 loads it too, because
   the S3 output lint runs on every line on every platform):
     header 48 B: u32 magic 'SAF1', u32 nreg, u32 reg_off, u32 salt_lo,
       u32 salt_hi, u32 nexact, u32 exact_off, u32 ndel, u32 del_off,
       u32 str_off, u32 str_len, u32 0
     reg    nreg x 8 B: u32 word (string offset), u8 class (TR_*), u8 flags
            (SR_HEAD: a leading *, SR_TAIL: a trailing *, SR_FORBID: never
            in Talk output, SR_SUGGEST: a flirting word, adults-only rooms
            with no pet), u16 0. The register words (core/data/story/
            register.tsv, the story lint's own list and matcher rules) plus
            every surface of the never-generated non-clean concepts.
     exact  nexact x u64, sorted: the hate list (salted FNV-1a 64 of the
            squeezed word, see hate_key())
     del    ndel x u64, sorted: the same of every one-letter deletion of the
            hate words of 5 letters or more (the edit distance 1 match)
     strings NUL-terminated words
2. safety/hate.hash: the hate list as salted hashes. No slur is ever
   written in this repository. `talkc.py --hate-hash PLAIN` turns a local
   plaintext list (one word a line; the reviewer keeps it, it is never
   committed) into the file.
3. The template lints TK1-TK9 (design section 7.4) over grammar/<lang>/*.tlk
   and voice/<lang>/*.tlk, and the seeded bad templates in
   core/tests/talk_bad/*.tlk.

The template syntax the lints read (T5a may add to it, never loosen it):
  @rule NAME [guard: EXPR] [tags: TAG,TAG] [slot: x=LEAF,LEAF] [act: ACT]
    "TEXT" [w=N] [guard: EXPR] [needs: EXPR]
  EXPR is predicates joined by &, a ! negates one. A line's guard is the
  rule's AND its own. {name} in TEXT is a slot or a sub-rule (a rule NAME,
  or every rule NAME.*); [CODE] is a line code ([LOVE], [R4] ...).
  (T5a, talkc_gen.py) act: is the reply act of the rule's lines; needs: is
  more guard terms (x.countable ...); w=N the weight (1 by default).
"""
import os
import re

SAF_MAGIC = 0x31464153          # "SAF1"
SR = {"head": 1, "tail": 2, "forbid": 4, "suggest": 8, "allow": 16}
TR = {"clean": 0, "crude": 1, "strong": 2, "sexual": 3, "hate": 4, "realworld": 5, "distress": 6}
FNV64_OFF = 0xCBF29CE484222325
FNV64_PRIME = 0x100000001B3
M64 = (1 << 64) - 1
LEET = {"0": "o", "1": "i", "3": "e", "4": "a", "5": "s", "7": "t", "8": "b", "9": "g", "@": "a", "$": "s"}

# the rule names / tags whose banks the lints treat specially (TK3, TK6, TK7)
ROMANCE_ROOTS = ("core.flirt", "core.confess", "core.ask_out", "core.kiss", "beat.romance_adult",
                 "player.flirty", "romance")
KID_BANKS = ("voice.kid", "kid_moment", "beat.kid_moment")
REFUSAL_BANKS = ("refuse", "deflect", "distress", "end")   # T5a: the K-11 and K-7 banks too
# TK10 (T5B-KIDFIX): the cool-act banks (replies to insult, mock, tease, criticize, complain, blame,
# dismiss, threaten) and the acts that turn on the listener
COOL_ROOTS = ("core.insult", "core.mock", "core.tease", "core.criticize", "core.complain", "core.blame",
              "core.dismiss", "core.threaten")
COOL_ATTACKS = ("insult", "mock", "tease", "blame", "threaten", "dismiss")


def squeeze(w):
    """letters only, leet folded, every run of one letter made one letter
    (talk_safe.c squeeze() is the same)"""
    out = []
    for ch in w.lower():
        ch = LEET.get(ch, ch)
        if "a" <= ch <= "z":
            if not out or out[-1] != ch:
                out.append(ch)
    return "".join(out)


def fnv64(salt, s):
    h = FNV64_OFF
    for byte in salt.to_bytes(8, "little") + s.encode("latin-1"):
        h ^= byte
        h = (h * FNV64_PRIME) & M64
    return h


def hate_key(salt, w):
    return fnv64(salt, squeeze(w))


def deletions(s):
    """every one-letter deletion but the first letter's (the fuzzy match
    keeps the first letter: talk_safe.c hate_fuzzy())"""
    return sorted(set(s[:i] + s[i + 1:] for i in range(1, len(s))))


def load_hate(path, die, rel):
    """safety/hate.hash: 'salt HEX', then 'x HEX' (a squeezed word) and
    'd HEX' (one of its deletions) lines"""
    salt, exact, dele = None, [], []
    if not os.path.exists(path):
        die(rel(path), "missing (the hashed hate list, docs/TALK.md)")
    for n, line in enumerate(open(path, encoding="utf-8").read().split("\n"), 1):
        where = "%s:%d" % (rel(path), n)
        if not line.strip() or line.startswith("#"):
            continue
        cols = line.split()
        if len(cols) != 2 or cols[0] not in ("salt", "x", "d") or not re.match(r"^[0-9A-F]{16}$", cols[1]):
            die(where, "expected 'salt|x|d HEX16'")
        v = int(cols[1], 16)
        if cols[0] == "salt":
            if salt is not None:
                die(where, "salt twice")
            salt = v
        elif salt is None:
            die(where, "the salt line comes first")
        else:
            (exact if cols[0] == "x" else dele).append(v)
    if salt is None or not exact:
        die(rel(path), "no salt or no entries")
    if len(set(exact)) != len(exact) or len(set(dele)) != len(dele):
        die(rel(path), "a hash twice")
    return salt, sorted(exact), sorted(dele)


def write_hate(plain, out, salt, rel):
    words = []
    for line in open(plain, encoding="utf-8").read().split("\n"):
        line = line.split("#", 1)[0].strip()
        if line:
            words.append(line)
    exact, dele = set(), set()
    for w in words:
        s = squeeze(w)
        if len(s) < 3:
            continue
        for v in (s, s + "s", s + "es"):
            exact.add(fnv64(salt, squeeze(v)))
        if len(s) >= 5:
            for d in deletions(s):
                dele.add(fnv64(salt, d))
    lines = ["# core/data/talk/safety/hate.hash - the Talk hate list as SALTED HASHES (owner: TK, T4;",
             "# docs/TALK.md \"Safety\", design 7.6). No slur is written anywhere in this repository.",
             "# Each word was squeezed (letters only, leet folded: 0>o 1>i 3>e 4>a 5>s 7>t 8>b 9>g @>a",
             "# $>s; every run of one letter made one) and hashed: FNV-1a 64 over the 8-byte",
             "# little-endian salt followed by the word. x = a word (and its -s / -es plural), d = one",
             "# of its one-letter deletions after the first letter (words of 5 letters or more: the edit",
             "# distance 1 match, which keeps the first letter).",
             "# Regenerate from the reviewer's local plaintext list (never committed) with",
             "#   python3 core/tools/talkc.py --hate-hash PLAIN",
             "salt %016X" % salt]
    lines += ["x %016X" % v for v in sorted(exact)]
    lines += ["d %016X" % v for v in sorted(dele - exact)]
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    return len(words), len(exact), len(dele - exact)


def _u16(v):
    return (v & 0xFFFF).to_bytes(2, "little")


def _u32(v):
    return (v & 0xFFFFFFFF).to_bytes(4, "little")


def suggestive_rx(b, suggestive):
    """the surfaces of the flirting words (sexual class, not explicit)"""
    out = []
    for k in sorted(suggestive):
        c = b.by_key[k]
        out += [c.lemma.lower()] + ([c.plural.lower()] if c.plural else []) + [s.lower() for s in c.syn]
    return out


def register_entries(b, suggestive):
    """[(body, cls, flags)]: register.tsv words, then the never-generated
    non-clean concepts' surfaces"""
    from talkc import surfaces
    sugg = suggestive_rx(b, suggestive)
    allowed_strong = [s.lower() for c in b.concepts if c.reg == "strong" and c.age != "never"
                      for s in surfaces(c)]
    capped = set()
    for c in b.concepts:
        if c.cat.key == "safety.capped":
            capped.update(s.lower() for s in surfaces(c))
    out, seen = [], set()
    for w in b.register:
        body = w.text.strip("*")
        fl = (SR["head"] if w.text.startswith("*") else 0) | (SR["tail"] if w.text.endswith("*") else 0)
        if w.cls == "sexual":
            # the whole sexual class is never generated (T1): a flirting
            # word is marked, and forbidden in Talk output all the same
            fl |= SR["forbid"]
            if any(w.rx.search(s) for s in sugg):
                fl |= SR["suggest"]
        elif w.cls == "strong":
            if w.cap is not None or not any(w.rx.search(s) for s in allowed_strong):
                fl |= SR["forbid"]
        key = (body, fl & 3)
        if key in seen:
            continue
        seen.add(key)
        out.append((body, TR[w.cls], fl))
    for c in b.concepts:
        if c.age != "never" or c.reg == "clean" and not (c.cat.key == "safety.minor"):
            continue
        cls = TR[c.reg]
        for s in surfaces(c):
            body = s.lower()
            key = (body, 0)
            if key in seen or not re.match(r"^[a-z0-9' -]+$", body):
                continue
            seen.add(key)
            fl = SR["forbid"]
            if c.reg == "sexual" and c.key in suggestive:
                fl |= SR["suggest"]
            out.append((body, cls, fl))
    # phrases that may be generated although a forbidden word is in them
    # ("tug of war", "water pistol"): the lint blanks them out first
    forbidden = [body for body, cls, fl in out if fl & SR["forbid"] and " " not in body]
    allow = set()
    for c in b.concepts:
        if c.age == "never" or c.reg != "clean":
            continue
        for s in surfaces(c):
            low = s.lower()
            if " " not in low and "-" not in low:
                continue
            words = re.split(r"[ -]+", low)
            if any(fw in words for fw in forbidden) and re.match(r"^[a-z0-9' -]+$", low):
                allow.add(low)
    for low in sorted(allow):
        out.insert(0, (low, 0, SR["allow"]))
    return out


def build_safety_bank(b, suggestive, salt, exact, dele):
    regs = register_entries(b, suggestive)
    strings = bytearray()
    rows = bytearray()
    for body, cls, fl in regs:
        rows += _u32(len(strings)) + bytes((cls, fl)) + _u16(0)
        strings += body.encode("latin-1") + b"\0"
    hdr_len = 48
    reg_off = hdr_len
    ex_off = reg_off + len(rows)
    del_off = ex_off + 8 * len(exact)
    s_off = del_off + 8 * len(dele)
    hdr = b"".join(_u32(x) for x in (SAF_MAGIC, len(regs), reg_off, salt & 0xFFFFFFFF, salt >> 32,
                                    len(exact), ex_off, len(dele), del_off, s_off, len(strings), 0))
    body = hdr + bytes(rows) + b"".join(v.to_bytes(8, "little") for v in exact) + \
        b"".join(v.to_bytes(8, "little") for v in dele) + bytes(strings)
    info = {"reg": len(regs), "forbid": sum(1 for r in regs if r[2] & SR["forbid"]),
            "suggest": sum(1 for r in regs if r[2] & SR["suggest"]), "exact": len(exact), "del": len(dele)}
    return body, info


# ---------------------------------------------------------------------------
# template lints TK1-TK7 and TK10 (TK8, TK9 are in talkc.py's check)
# ---------------------------------------------------------------------------
class Tlk:
    def __init__(self):
        self.rules = []        # dicts: name, guard(set), tags(set), slots{x: [leaves]}, lines[(text, guard, where)]


def _guard(expr, where, die):
    out = set()
    if not expr:
        return out
    for t in expr.split("&"):
        t = t.strip()
        if not re.match(r"^!?[a-z_][a-z0-9_=.]*$", t):
            die(where, "guard term %r" % t)
        out.add(t)
    return out


def read_tlk(path, die, rel):
    T = Tlk()
    cur = None
    for n, line in enumerate(open(path, encoding="utf-8").read().split("\n"), 1):
        where = "%s:%d" % (rel(path), n)
        s = line.strip()
        if not s or s.startswith("#"):
            continue
        if s.startswith("@rule"):
            m = re.match(r"^@rule\s+([a-z0-9_.]+)((?:\s+(?:guard|tags|slot|act):\s*[^\s]+)*)\s*$", s)
            if not m:
                die(where, "@rule NAME [guard: EXPR] [tags: T,T] [slot: x=LEAF,LEAF] [act: ACT]")
            cur = {"name": m.group(1), "guard": set(), "tags": set(), "slots": {}, "lines": [], "where": where,
                   "act": None, "opts": [], "file": rel(path)}
            for k, v in re.findall(r"(guard|tags|slot|act):\s*([^\s]+)", m.group(2)):
                if k == "act":
                    if not re.match(r"^[a-z_]+$", v):
                        die(where, "act: ACT (a talk.h TA_* name, lower case)")
                    cur["act"] = v
                elif k == "guard":
                    cur["guard"] |= _guard(v, where, die)
                elif k == "tags":
                    cur["tags"] |= set(t for t in v.split(",") if t)
                else:
                    mm = re.match(r"^([a-z]+)=([a-z0-9_.,*]+)$", v)
                    if not mm:
                        die(where, "slot: x=LEAF,LEAF")
                    cur["slots"][mm.group(1)] = mm.group(2).split(",")
            T.rules.append(cur)
            continue
        m = re.match(r'^"((?:[^"\\]|\\.)*)"((?:\s+(?:w=\d+|guard:\s*[^\s]+|needs:\s*[^\s]+))*)\s*$', s)
        if not m or cur is None:
            die(where, 'a line is "TEXT" [w=N] [guard: EXPR] [needs: ...], after an @rule')
        g, needs = set(), set()
        for v in re.findall(r"guard:\s*([^\s]+)", m.group(2)):
            g |= _guard(v, where, die)
        for v in re.findall(r"needs:\s*([^\s]+)", m.group(2)):
            needs |= _guard(v, where, die)
        w = re.findall(r"w=(\d+)", m.group(2))
        cur["lines"].append((m.group(1), cur["guard"] | g, where))
        cur["opts"].append({"guard": g, "needs": needs, "w": int(w[-1]) if w else 1})
    return T


def _visible(text):
    t = re.sub(r"\{[^}]*\}", " ", text)
    t = re.sub(r"\[[A-Z0-9_]+\]", " ", t)
    return t.replace("/", " ")


def _starts(name, roots):
    return any(name == r or name.startswith(r + ".") or name.startswith(r + "_") for r in roots)


def lint_tlk(T, b, suggestive, die):
    """problems (strings) of one or more parsed .tlk files"""
    from talkc import surfaces
    probs = []
    regs = b.register
    sugg = suggestive_rx(b, suggestive)
    never_words = []
    for c in b.concepts:
        if c.age == "never":
            never_words += [s.lower() for s in surfaces(c)]
    capped = [w for w in regs if w.cap is not None]
    capped_concepts = [s.lower() for c in b.concepts if c.cat.key == "safety.capped" for s in surfaces(c)]
    names = [r["name"] for r in T.rules]

    def reach(roots_pred):
        seen = set(n for n in names if roots_pred(n))
        work = list(seen)
        while work:
            n = work.pop()
            for r in T.rules:
                if r["name"] != n:
                    continue
                for text, g, wh in r["lines"]:
                    for ref in re.findall(r"\{([a-z0-9_.]+)", text):
                        for m in names:
                            if (m == ref or m.startswith(ref + ".")) and m not in seen:
                                seen.add(m)
                                work.append(m)
        return seen

    romance = reach(lambda n: _starts(n, ROMANCE_ROOTS) or
                    any("romance" in r["tags"] for r in T.rules if r["name"] == n))
    for r in T.rules:
        kid = _starts(r["name"], KID_BANKS) or "kid" in r["tags"]
        refusal = _starts(r["name"], REFUSAL_BANKS)
        if refusal and "nojoke" not in r["tags"]:
            probs.append("%s: TK7: refusal bank %s needs tags: nojoke (reviewed: no joke at the refused "
                         "child's expense)" % (r["where"], r["name"]))
        for x, leaves in r["slots"].items():
            for leaf in leaves:
                root = leaf.split(".")[0]
                if root == "safety" or leaf.startswith("safety"):
                    probs.append("%s: TK5: slot %s=%s: a safety category is never generated" % (r["where"], x, leaf))
                elif root == "romance" and not {"adults_only", "!kid_present"} <= r["guard"]:
                    probs.append("%s: TK5: slot %s=%s is adults-only: the rule needs guard: adults_only&!kid_present"
                                 % (r["where"], x, leaf))
        for text, g, where in r["lines"]:
            vis = _visible(text)
            low = vis.lower()
            hits = [w for w in regs if w.rx.search(vis)]
            sugg_hit = any(re.search(r"(?<![\w-])" + re.escape(s) + r"(?![\w])", low) for s in sugg)
            if sugg_hit or any(w.cls == "sexual" for w in hits):
                probs.append("%s: TK4: a sexual-class word (even a flirting one) is never generated: write the"
                             " innuendo without it" % where)
            for w in capped:
                if w.rx.search(vis):
                    probs.append("%s: TK2: %r is a capped word (register.tsv cap %d): never in a Talk bank"
                                 % (where, w.text, w.cap))
            for s in capped_concepts:
                if re.search(r"(?<![\w])" + re.escape(s) + r"(?![\w])", low):
                    probs.append("%s: TK2: %r is a capped swear: never in a Talk bank" % (where, s))
            explicit = [w for w in hits if w.cls == "sexual" and not any(w.rx.search(s) for s in sugg)]
            if explicit:
                probs.append("%s: TK4: explicit word %r: the sexual class is refuse-only, never generated"
                             % (where, explicit[0].text))
            for s in never_words:
                if len(s) >= 3 and re.search(r"(?<![\w])" + re.escape(s) + r"(?![\w])", low):
                    probs.append("%s: TK5: %r is a never-generated (safety) concept" % (where, s))
                    break
            if hits and "!kid_present" not in g:
                probs.append("%s: TK1: register word %r needs guard: !kid_present" % (where, hits[0].text))
            if (sugg_hit or any(w.cls == "sexual" for w in hits)) and "!pet" not in g:
                probs.append("%s: TK1: a sexual-class word needs guard: !pet" % where)
            if ("jmat" in g or "jmat" in r["tags"] or "[JMAT]" in text) and "mature_adult_ok" not in g:
                probs.append("%s: TK4: a jmat variant needs guard: mature_adult_ok" % where)
            if r["name"] in romance:
                if not {"adults_only", "!kid_present"} <= g:
                    probs.append("%s: TK3: %s is reachable from a romance act: guard: adults_only&!kid_present"
                                 % (where, r["name"]))
                if (sugg_hit or "suggestive" in r["tags"]) and "!pet" not in g:
                    probs.append("%s: TK3: a suggestive romance line needs guard: !pet" % where)
            if kid:
                if hits:
                    probs.append("%s: TK6: kid-voice bank %s has register word %r" % (where, r["name"], hits[0].text))
                if "[LOVE]" in text:
                    probs.append("%s: TK6: kid-voice bank %s uses the LOVE face" % (where, r["name"]))
            if (_starts(r["name"], COOL_ROOTS) and r["act"] in COOL_ATTACKS and "gentle" not in r["tags"]
                    and "kid_speaker" not in g and "!kid_present" not in g):
                probs.append("%s: TK10: %s turns on the listener (act: %s) and may play with a child in the "
                             "room: guard: !kid_present, or tags: gentle (reviewed: a silly deflection)"
                             % (where, r["name"], r["act"]))
            # TK11 (T5B-ROM review): no act ends a relationship, so a decline never says it is over
            if (r["name"] in romance and r["act"] in ("no", "deflect_romance")
                    and re.search(r"\b(it'?s over|we'?re done|say goodbye|part ways|break up|breaking up"
                                  r"|end it|end this|run out of road|good run|can'?t stay)\b", low)):
                probs.append("%s: TK11: a decline to a romance act must not end the relationship in text "
                             "(no break-up act exists): soft decline only" % where)
            if refusal and ("[LOVE]" in text or "[HEARTS]" in text):
                probs.append("%s: TK7: a refusal never uses the LOVE face or hearts" % where)
    return probs
