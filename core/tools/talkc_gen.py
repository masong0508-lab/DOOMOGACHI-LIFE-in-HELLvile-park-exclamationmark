"""talkc_gen.py - the Talk generator banks for talkc.py (owner: TK, T5a;
docs/TALK.md "The generator"; design 3.6). Imported by talkc.py; stdlib only.

Inputs: core/data/talk/acts.tsv (the adjacency table), the grammar
grammar/<lang>/*.tlk and voice/<lang>/*.tlk (the syntax is talkc_safe.py's,
which the TK1-TK9 lints read), the lexicon (concept forms), the game tables
the generator maps to concepts (catalog.c likes, the roster's jobs and cast,
the catchphrases).

TEMPLATE TEXT (what a writer types inside "..."):
  plain text           drawn as typed (the font's ASCII; the story codec
                       must know every character)
  {.} {..} {...} {d} {!} {big} {/} {w} {r} {punch} {p N} {cut N}
                       the story's line codes (ui_dialog.c), kept
  [HAPPY] [SAD] [ANGRY] [LOVE] [SHOCK] [SLEEPY] [SICK]
                       the line's face (portrait.h EXPR_*), taken out
  [JMAT]               marks a Mature variant (lint TK4: needs mature_adult_ok)
  {rule.name}          a sub-rule: a rule NAME, or every rule NAME.*
  {SLOT[.FIELD][:MOD[:MOD]]}   a filler:
    concept slots   x (the topic), food hobby music fear hate (the
                    speaker's likes, hated food), like (one of their likes:
                    a CHANGE_TOPIC target), mem (talk memory's last topic),
                    job (the roster job, adults only)
      FIELD         adj (an attribute said of it), good / bad (one with a
                    positive / negative sentiment), assoc (a 1-hop
                    association), it (it / they), is (is / are), has
                    (has / have), does (does / do), was (was / were)
      MOD           pl, indef ("a pie", "an owl", "some butter"), def ("the
                    pie"), lemma, cap (capital first letter), up (ALL
                    CAPS), s (possessive 's)
    name slots      you (the player), me (the speaker), them (the resident
                    addressed), other (another participant), partner, petname (the
                    Boreagotchi's name), lost (K-10: the one the speaker lost
                    lately; the grief and comfort banks only)
      MOD           cap, up, s
    text slots      catch (the speaker's catchphrase, once a conversation)
  A filler adds its own guard: {x} needs `topic`, {x.adj} `x.adj`, {them}
  `to_res`, {other} `other`, {job} `fact.job`, {partner} `fact.partner`,
  {mem} `fact.memory`, {catch} `catch`, {lost} `fact.loss`.
  Write "a {x}" as "{x:indef}": the compiler refuses a hand-made article
  before a concept slot.

COMPILED (the TB_GRAMMAR bank, little endian):
  header 64 B: u32 'TGR1', u16 nrules, nalts, nterms, nsyms, nrefs, ncast,
    njobs, nacts; u32 rules_off, alts_off, terms_off, syms_off, refs_off,
    entry_off, acts_off, map_off, cast_off, jobs_off; u16 nentry_fixed, 0
  rules  nrules x 8 B {u16 alt_first, u8 nalt, u8 act (TA_*), u16
         term_first, u8 nterm, u8 flags (RF_*)}
  alts   nalts x 8 B {u16 string (TB_STRINGS index), u8 weight, u8 face
         (EXPR_* + 1, 0 none), u16 term_first, u8 nterm, u8 flags (AF_*)}
  terms  2 B {u8 pred (TGP_*) | neg << 7, u8 value}
  syms   nsyms x 4 B {u16 ref_first, u16 nref}; refs u16 rule ids
  entry  u16 core[nacts], react[nacts], fixed[TGE_COUNT]: symbols, 0xFFFF none
  acts   nacts x 16 B {u32 replies[3] (bit = a TA_* that may answer), u8
         class (TAC_*), u8 default reply, u16 0}
  map    u16 food[48], hobby[32], music[16], fear[24] (concept ids); u8
         catch[64] (CC_* of each catchphrase)
  cast   ncast x u16 roster id (the TGP_SPEAKER values)
  jobs   njobs x u16 concept of the roster job, by roster id (0 none)
K-10 (grief is not a punchline, SCRIPT_STYLE R6): the rules `grief` and
`grief.*` are the sincere core the generator takes for a grief or loss topic
(a TCF_GRIEF concept) whatever the player's act; `comfort` / `comfort.*` the
sincere reaction another resident may add. Every such rule names its reply
act (`act:`; comfort: agree or backchannel; the generator keeps only those
the adjacency table allows after what was said), is tagged nojoke (the
reviewed promise: no punchline), and its lines have no register word, no
sub-rule, no catchphrase and no LOVE face or [JMAT]. The full bank must
answer every act that is not a safety act with a line that needs no guard.
The TB_STRINGS bank (TBK_TXT, the story's Huffman code) holds the compiled
template texts: the line codes as written; a sub-rule is {#N} (a symbol);
a filler is {S[f][:mods]} with S an upper-case slot letter, f a field
letter and mods letters (talk_gen.c reads them).
"""
import os
import re

GRAMMAR_MAGIC = 0x31524754           # "TGR1"
HDR = 64
RF = {"kid": 1, "refusal": 2, "romance": 4, "sincere": 8, "cast": 16, "tic": 32, "lower": 64}
AF = {"swear": 1, "register": 2, "jmat": 4, "cast": 8, "topic": 16, "like": 32, "tic": 64, "catch": 128}
CC = {"register": 1, "swear": 2, "forbid": 4}
CLASSES = ["open", "ask", "tell", "warm", "cool", "third", "flow", "romance", "gesture", "safety"]
LINE_CODES = {".", "..", "...", "d", "!", "big", "/", "w", "r", "punch"}
CONCEPT_SLOTS = {"x": "X", "food": "F", "hobby": "H", "music": "U", "fear": "E", "hate": "Z", "like": "L",
                 "mem": "K", "job": "J"}
NAME_SLOTS = {"you": "Y", "me": "M", "them": "T", "other": "O", "partner": "P", "petname": "Q", "lost": "G"}
TEXT_SLOTS = {"catch": "C"}
FIELDS = {"adj": "a", "good": "g", "bad": "b", "assoc": "s", "it": "i", "is": "v", "has": "h", "does": "d",
          "was": "w"}
MODS = {"pl": "p", "indef": "n", "def": "t", "lemma": "l", "cap": "c", "up": "u", "s": "s"}
NAME_MODS = {"cap", "up", "s"}
IMPLICIT = {"x": "topic", "them": "to_res", "other": "other", "job": "fact.job", "partner": "fact.partner",
            "mem": "fact.memory", "catch": "catch", "lost": "fact.loss"}
FIELD_NEED = {"adj": "x.adj", "good": "x.good", "bad": "x.bad", "assoc": "x.assoc"}
ABBREV = ("Mr", "Mrs", "Ms", "Dr", "St", "Mme", "Jr", "Sr", "Mt", "Prof", "Rev")   # talk_gen.c abbrev_at


def sentence_count(v):
    """an end mark followed by a space or the end ("...Right." is one; "Dr." none): talk_gen.c sentences()"""
    n = 0
    for m in re.finditer(r"[.!?](?= |$)", v):
        i = m.start()
        if v[i] == ".":
            w = re.search(r"[A-Za-z]*$", v[:i]).group(0)
            if w in ABBREV:
                continue
        n += 1
    return n


# the guard terms a callback (cb.*) line may stand on (talk_gen.c is_fact_pred)
CB_FACTS = {"fact.job", "fact.partner", "fact.pet", "fact.news", "fact.problem", "fact.bday", "ghost", "possessed",
            "hellborn", "night", "fact.memory", "fact.complimented", "fact.insulted", "fact.promised",
            "likes_topic", "fears_topic", "fact.family"}
# fixed entries (talk.h TGE_* order) -> rule name
FIXED = ["open", "cb", "tag", "handoff", "safe", "end.k7", "pet", "idle", "grief", "comfort",
         # T6 (talk_dm.c): beat.<name> goal lines, the prod, the walk-out excuse, the grumble of one
         # cut off, the three steps of an argument, making up, a child asking "are you fighting?"
         "beat", "prod", "excuse", "grumble", "jab", "row", "ultimatum", "makeup", "kidask"]
# K-10: the sincere banks (talk.h TGE_GRIEF, TGE_COMFORT); a comfort line is a reaction
GRIEF_BANKS = ("grief", "comfort")
COMFORT_ACTS = ("AGREE", "BACKCHANNEL")
# a safety act's core entry is named for what it does
SAFETY_ENTRY = {"REFUSE_SEXUAL": "refuse.sexual", "REFUSE_MINOR": "refuse.minor", "REFUSE_HATE": "refuse.hate",
                "DISTRESS": "distress", "DEFLECT_ROMANCE": "deflect.romance"}
REACTS = ("AGREE", "DISAGREE", "LAUGH", "TEASE", "JOKE", "BACKCHANNEL")
# minimum filler lengths for the fit check (the shortest a filler can be)
MIN_FILL = {"concept": 3, "name": 2, "text": 4, "field": 2}
BUBBLE = 26 * 3
SENTENCES_MAX = 3            # talk.h TALK_SENTENCES_MAX


def _u16(v):
    return (v & 0xFFFF).to_bytes(2, "little")


def _u32(v):
    return (v & 0xFFFFFFFF).to_bytes(4, "little")


def c_enum(path, prefix, first=None):
    """{name without the prefix, lower case: value} of a C enum whose members
    start with prefix (explicit '= N' and '= OTHER' aliases followed)"""
    src = open(path, encoding="latin-1").read()
    src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)
    src = re.sub(r"//[^\n]*", "", src)
    out, val = {}, -1
    found = False
    for m in re.finditer(r"enum\s*\{(.*?)\}", src, re.S):
        body = m.group(1)
        if not re.search(r"\b%s[A-Z0-9_]+" % re.escape(prefix), body):
            continue
        if first and not re.search(r"\b%s\b" % re.escape(first), body):
            continue
        found = True
        val = -1
        names = {}
        for item in body.split(","):
            item = item.strip()
            if not item:
                continue
            mm = re.match(r"^([A-Z_][A-Z0-9_]*)\s*(?:=\s*(.+))?$", item)
            if not mm:
                raise ValueError("%s: enum item %r" % (path, item))
            nm, expr = mm.group(1), mm.group(2)
            if expr is not None:
                expr = expr.strip()
                if re.match(r"^\d+$", expr):
                    val = int(expr)
                elif expr in names:
                    val = names[expr]
                elif re.match(r"^0x[0-9A-Fa-f]+$", expr):
                    val = int(expr, 16)
                else:
                    raise ValueError("%s: enum value %r" % (path, expr))
            else:
                val += 1
            names[nm] = val
            if nm.startswith(prefix):
                out[nm[len(prefix):].lower()] = val
        break
    if not found:
        raise ValueError("%s: no enum of %s*" % (path, prefix))
    return out


class Env:
    """what the compiler needs from talk.h and the game sources"""

    def __init__(self, b, die, rel):
        import talkc as T
        CORE = T.CORE
        self.T = T
        inc = os.path.join(CORE, "include", "doomo")
        th = os.path.join(inc, "talk.h")
        self.acts = {k.upper(): v for k, v in c_enum(th, "TA_", "TA_NONE").items()}
        self.nacts = self.acts.pop("COUNT")
        self.tgp = c_enum(th, "TGP_", "TGP_NONE")
        self.first_valued = self.tgp["first_valued"]
        self.tgp_count = self.tgp["count"]
        self.values = {
            "stance": {k: v for k, v in c_enum(th, "TST_").items() if k != "count"},
            "mood": {k: v for k, v in c_enum(th, "TMB_").items() if k != "count"},
            "rel": {k: v for k, v in c_enum(th, "TRB_").items() if k != "count"},
            "time": {k: v for k, v in c_enum(th, "TTOD_").items() if k != "count"},
            "beat": {k: v for k, v in c_enum(th, "TBEAT_").items() if k != "count"},
            "tone": {k: v for k, v in c_enum(th, "TONE_").items() if k != "count"},
            "pact": {k: v for k, v in c_enum(th, "TAC_").items() if k != "count"},
            "act": {k.lower(): v for k, v in self.acts.items()},
            "root": {k: v for k, v in c_enum(th, "TCR_").items() if k not in ("count", "none")},
            "prob": {k: v for k, v in c_enum(os.path.join(inc, "sim.h"), "PROB_").items() if k != "count"},
            "chapter": {k: v for k, v in c_enum(os.path.join(inc, "story.h"), "STORY_CH").items()},
        }
        self.values["chapter"] = {re.sub(r"^_", "", k): v for k, v in self.values["chapter"].items()}
        self.tge = c_enum(th, "TGE_")
        self.tge_count = self.tge.pop("count")
        if [FIXED.index(k.replace("_", ".")) if k.replace("_", ".") in FIXED else -1 for k in
                sorted(self.tge, key=lambda k: self.tge[k])] != list(range(self.tge_count)):
            die("talk.h", "TGE_* and talkc_gen.py FIXED disagree")
        self.tac = [k for k in sorted(self.values["pact"], key=lambda k: self.values["pact"][k])]
        if self.tac != CLASSES:
            die("talk.h", "TAC_* and talkc_gen.py CLASSES disagree: %r" % self.tac)
        exprs = c_enum(os.path.join(inc, "portrait.h"), "EXPR_")
        self.faces = {k.upper(): v + 1 for k, v in exprs.items() if k not in ("count", "talk")}
        cat_c = os.path.join(CORE, "src", "catalog.c")
        self.types = {T.keyify(n): i for i, n in enumerate(T.c_array(cat_c, "doomo_persona_type_names"))}
        self.values["type"] = self.types
        self.cast = [(T.keyify(lemma), rid) for lemma, rid in T.CAST]
        self.values["speaker"] = {k: i for i, (k, rid) in enumerate(self.cast)}
        self.catchphrases = T.c_array(cat_c, "doomo_catchphrases")
        self.cat_c = cat_c


def load_acts(path, env, die, rel):
    """acts.tsv -> {ACT: (class, [replies])}"""
    out = {}
    for n, line in enumerate(open(path, encoding="utf-8").read().split("\n"), 1):
        where = "%s:%d" % (rel(path), n)
        if not line.strip() or line.startswith("#"):
            continue
        cols = line.split("\t")
        if len(cols) != 3:
            die(where, "ACT<TAB>CLASS<TAB>REPLIES")
        act, cls, reps = cols[0].strip(), cols[1].strip(), [r.strip() for r in cols[2].split(",") if r.strip()]
        if act not in env.acts or act == "NONE":
            die(where, "%s is not a talk.h TA_* act" % act)
        if act in out:
            die(where, "%s twice" % act)
        if cls not in CLASSES:
            die(where, "class %r (one of %s)" % (cls, " ".join(CLASSES)))
        if not reps:
            die(where, "no replies")
        for r in reps:
            if r not in env.acts or r == "NONE":
                die(where, "reply %s is not a talk.h TA_* act" % r)
        if len(set(reps)) != len(reps):
            die(where, "a reply twice")
        out[act] = (cls, reps)
    missing = [a for a in env.acts if a != "NONE" and a not in out]
    if missing:
        die(rel(path), "no row for %s" % ", ".join(sorted(missing)))
    return out


def entry_name(act):
    return SAFETY_ENTRY.get(act, "core." + act.lower())


class Compiler:
    def __init__(self, b, env, acts, die, rel, codec):
        self.b, self.env, self.acts, self.die, self.rel, self.codec = b, env, acts, die, rel, codec
        self.register = b.register

    # ---- guard terms ----------------------------------------------------
    def term(self, t, where):
        neg = t.startswith("!")
        name = t[1:] if neg else t
        if "=" in name:
            key, val = name.split("=", 1)
            pred = self.env.tgp.get(key)
            if pred is None or pred < self.env.first_valued:
                self.die(where, "guard %r: %s is not a valued predicate (talk.h TGP_*)" % (t, key))
            table = self.env.values.get(key)
            if table is None:
                self.die(where, "guard %r: no value table for %s" % (t, key))
            if key == "chapter" and re.match(r"^\d+$", val):
                v = int(val)
            elif val in table:
                v = table[val]
            else:
                self.die(where, "guard %r: %s is one of %s" % (t, key, ", ".join(sorted(table))))
            if not 0 <= v < 256:
                self.die(where, "guard %r: value out of range" % t)
            return (pred | (0x80 if neg else 0), v)
        key = name.replace(".", "_")
        pred = self.env.tgp.get(key)
        if pred is None or pred == 0 or pred >= self.env.first_valued:
            self.die(where, "guard %r: no such predicate (talk.h TGP_*)" % t)
        return (pred | (0x80 if neg else 0), 0)

    def terms(self, guard, where):
        out = sorted(set(self.term(t, where) for t in guard))
        pos = {(p & 0x7F, v) for p, v in out if not p & 0x80}
        for p, v in out:
            if p & 0x80 and (p & 0x7F, v) in pos:
                self.die(where, "guard contradicts itself: %s and its negation" % ([k for k, x in
                                                                                     self.env.tgp.items()
                                                                                     if x == p & 0x7F][0]))
        if len(out) > 15:
            self.die(where, "more than 15 guard terms")
        return out

    # ---- text -----------------------------------------------------------
    def compile_text(self, text, where, symbol_of, rule):
        """-> (compiled, face, implicit guard terms, refs, aflags, min_len_without_refs, said slot names)"""
        out, refs, implicit = [], [], set()
        face, af, minlen = 0, 0, 0
        i = 0
        lit = []
        while i < len(text):
            c = text[i]
            if c == "\\" and i + 1 < len(text):
                out.append(text[i + 1])
                lit.append(text[i + 1])
                minlen += 1
                i += 2
                continue
            if c == "[":
                j = text.find("]", i)
                if j < 0:
                    self.die(where, "unclosed [")
                code = text[i + 1:j]
                if code in self.env.faces:
                    if face:
                        self.die(where, "two faces in one line")
                    face = self.env.faces[code]
                elif code == "JMAT":
                    af |= AF["jmat"]
                else:
                    self.die(where, "[%s]: a face (%s) or [JMAT]; other line codes are the story's {...}"
                             % (code, " ".join(sorted(self.env.faces))))
                i = j + 1
                continue
            if c == "{":
                j = text.find("}", i)
                if j < 0:
                    self.die(where, "unclosed {")
                inner = text[i + 1:j]
                i = j + 1
                if inner in LINE_CODES or re.match(r"^(p|cut) \d+$", inner):
                    out.append("{" + inner + "}")
                    continue
                m = re.match(r"^([a-z]+)(?:\.([a-z]+))?((?::[a-z]+)*)$", inner)
                if m and (m.group(1) in CONCEPT_SLOTS or m.group(1) in NAME_SLOTS or m.group(1) in TEXT_SLOTS):
                    slot, field, mods = m.group(1), m.group(2), [x for x in m.group(3).split(":") if x]
                    code = self.slot_code(slot, field, mods, where)
                    # a hand-made article before a concept slot
                    before = "".join(lit)[-4:]
                    if slot in CONCEPT_SLOTS and field in (None, "adj", "good", "bad", "assoc") and \
                            "indef" not in mods and "def" not in mods and re.search(r"(^|\W)(a|an|A|An) $", before):
                        self.die(where, "'a {%s}': write {%s:indef} (the article comes with the word)"
                                 % (inner, inner))
                    out.append("{" + code + "}")
                    lit.append("#")
                    if slot in IMPLICIT:
                        implicit.add(IMPLICIT[slot])
                    if slot == "x" and field in FIELD_NEED:
                        implicit.add(FIELD_NEED[field])
                    if slot == "x":
                        af |= AF["topic"]
                    if slot == "like":
                        af |= AF["like"]
                    if slot == "catch":
                        af |= AF["catch"]
                    if field in ("it", "is", "has", "does", "was"):
                        minlen += MIN_FILL["field"]
                    elif slot in CONCEPT_SLOTS:
                        minlen += MIN_FILL["concept"]
                    elif slot in NAME_SLOTS:
                        minlen += MIN_FILL["name"]
                    else:
                        minlen += MIN_FILL["text"]
                    continue
                if not re.match(r"^[a-z0-9_]+(\.[a-z0-9_]+)*$", inner):
                    self.die(where, "{%s}: not a line code, a slot or a rule name" % inner)
                if inner.split(".")[0] in CONCEPT_SLOTS or inner.split(".")[0] in NAME_SLOTS:
                    self.die(where, "{%s}: a slot with an unknown field or modifier" % inner)
                sym = symbol_of(inner, where)
                refs.append(inner)
                out.append("{#%d}" % sym)
                lit.append("#")
                continue
            out.append(c)
            lit.append(c)
            minlen += 1
            i += 1
        comp = "".join(out)
        bad = [ch for ch in comp if not (0x20 <= ord(ch) <= 0x7E)]
        if bad:
            self.die(where, "characters the font cannot draw: %r" % bad)
        miss = self.codec.missing(comp)
        if miss:
            self.die(where, "characters the story codec has no code for: %r" % miss)
        if "  " in "".join(lit):
            self.die(where, "two spaces in a row")
        vis = re.sub(r"\{[^}]*\}", "", comp)
        if sentence_count(vis) > SENTENCES_MAX:
            self.die(where, "more than %d sentences in one line (Q1; a fragment counts)" % SENTENCES_MAX)
        if comp != comp.strip():
            self.die(where, "a line starts or ends with a space")
        return comp, face, implicit, refs, af, minlen

    def slot_code(self, slot, field, mods, where):
        for m in mods:
            if m not in MODS:
                self.die(where, "{%s}: modifier %r (one of %s)" % (slot, m, " ".join(sorted(MODS))))
        if len(set(mods)) != len(mods):
            self.die(where, "{%s}: a modifier twice" % slot)
        if slot in CONCEPT_SLOTS:
            if field is not None and field not in FIELDS:
                self.die(where, "{%s.%s}: field (one of %s)" % (slot, field, " ".join(sorted(FIELDS))))
            if field in ("it", "is", "has", "does", "was") and set(mods) - {"cap", "up"}:
                self.die(where, "{%s.%s}: only cap / up" % (slot, field))
            if field in ("adj", "good", "bad") and set(mods) - {"cap", "up", "indef"}:
                self.die(where, "{%s.%s}: only indef / cap / up" % (slot, field))
            if len({"pl", "indef", "def", "lemma"} & set(mods)) > 1:
                self.die(where, "{%s}: one of pl / indef / def / lemma" % slot)
            letter = CONCEPT_SLOTS[slot]
        elif slot in NAME_SLOTS:
            if field is not None or set(mods) - NAME_MODS:
                self.die(where, "{%s}: a name takes only cap / up / s" % slot)
            letter = NAME_SLOTS[slot]
        else:
            if field is not None or set(mods) - {"cap", "up"}:
                self.die(where, "{%s}: only cap / up" % slot)
            letter = TEXT_SLOTS[slot]
        code = letter + (FIELDS[field] if field else "")
        if mods:
            code += ":" + "".join(MODS[m] for m in mods)
        return code

    # ---- register words in a line (K-9 cap, kid banks) -------------------
    def register_hits(self, visible):
        return [w for w in self.register if w.rx.search(visible)]


def visible_text(text):
    t = re.sub(r"\{[^}]*\}", " ", text)
    t = re.sub(r"\[[A-Z0-9_]+\]", " ", t)
    return t.replace("/", " ")


def build_grammar(b, files, die, rel, strict=True):
    """compile the .tlk files: -> (grammar bytes, [template strings], info dict)"""
    import talkc as T
    import talkc_safe as ts_
    env = Env(b, die, rel)
    acts = load_acts(os.path.join(T.DATA, "acts.tsv"), env, die, rel)
    comp = Compiler(b, env, acts, die, rel, b.codec)
    tlks = [ts_.read_tlk(f, die, rel) for f in files]
    rules = [r for t in tlks for r in t.rules]
    names = [r["name"] for r in rules]
    for r in rules:
        first = r["name"].split(".")[0]
        if first in CONCEPT_SLOTS or first in NAME_SLOTS or first in TEXT_SLOTS:
            die(r["where"], "rule %s: a rule name may not start with a slot name" % r["name"])
    # the symbols: every entry and every name a line refers to
    sym_names, sym_index = [], {}

    def match(nm):
        return [k for k, n in enumerate(names) if n == nm or n.startswith(nm + ".")]

    def symbol_of(nm, where):
        if nm not in sym_index:
            rs = match(nm)
            if not rs:
                die(where, "{%s}: no rule %s or %s.*" % (nm, nm, nm))
            sym_index[nm] = len(sym_names)
            sym_names.append(nm)
        return sym_index[nm]

    entries_core, entries_react, entries_fixed = [], [], []
    by_id = {v: k for k, v in env.acts.items()}
    for a in range(env.nacts):
        nm = by_id.get(a, "NONE")
        e = entry_name(nm) if a else None
        entries_core.append(symbol_of(e, "acts") if e and match(e) else 0xFFFF)
        r = "react." + nm.lower() if a else None
        entries_react.append(symbol_of(r, "acts") if r and match(r) else 0xFFFF)
    for nm in FIXED:
        entries_fixed.append(symbol_of(nm, "fixed") if match(nm) else 0xFFFF)
    if strict:
        need = [entry_name(a) for a in acts] + list(FIXED[:7]) + list(GRIEF_BANKS) + \
            ["react." + a.lower() for a in REACTS]
        miss = [n for n in need if not match(n)]
        if miss:
            die("grammar", "no rules for: %s" % ", ".join(miss))
    # which rules are reachable from a romance act (the lint's set) and which are kid / refusal banks
    romance_roots = [k for k, r in enumerate(rules)
                     if ts_._starts(r["name"], ts_.ROMANCE_ROOTS) or "romance" in r["tags"]]
    rom = set(names[k] for k in romance_roots)
    work = list(rom)
    while work:
        n = work.pop()
        for r in rules:
            if r["name"] != n:
                continue
            for text, g, wh in r["lines"]:
                for ref in re.findall(r"\{([a-z0-9_.]+)", text):
                    for m in names:
                        if (m == ref or m.startswith(ref + ".")) and m not in rom:
                            rom.add(m)
                            work.append(m)
    strings, str_index = [], {}
    grief_rules = []                 # K-10: (rule name, reply act, [the guard terms of each line])
    rule_rows, alt_rows, term_rows = [], [], []
    alt_where, alt_refs, alt_min = [], [], []
    for k, r in enumerate(rules):
        where = r["where"]
        rname = r["name"]
        # the reply act
        act = 0
        if r["act"]:
            a = r["act"].upper()
            if a not in env.acts:
                die(where, "act: %s is not a talk.h TA_* act" % r["act"])
            act = env.acts[a]
        core_of = None
        for a in acts:
            e = entry_name(a)
            if rname == e or rname.startswith(e + "."):
                core_of = a
        react_of = None
        mm = re.match(r"^react\.([a-z_]+)", rname)
        if mm:
            react_of = mm.group(1).upper()
            if react_of not in env.acts:
                die(where, "react.%s: not a TA_* act" % mm.group(1))
        if core_of:
            cls, reps = acts[core_of]
            if not act:
                act = env.acts[core_of if core_of in SAFETY_ENTRY else reps[0]]
            if by_id[act] not in reps:
                die(where, "%s: reply act %s may not answer %s (acts.tsv)" % (rname, by_id[act], core_of))
        if react_of:
            if act and act != env.acts[react_of]:
                die(where, "%s: a react.<ACT> rule performs ACT" % rname)
            act = env.acts[react_of]
        if rname == "end.k7":
            act = env.acts["REFUSE_SEXUAL"]
        rf = 0
        kid = ts_._starts(rname, ts_.KID_BANKS) or "kid" in r["tags"]
        refusal = ts_._starts(rname, ts_.REFUSAL_BANKS)
        if kid:
            rf |= RF["kid"]
        if refusal:
            rf |= RF["refusal"] | RF["sincere"]
        if rname in rom:
            rf |= RF["romance"]
        if "sincere" in r["tags"]:
            rf |= RF["sincere"]
        if "lower" in r["tags"]:
            rf |= RF["lower"]
        grief = ts_._starts(rname, GRIEF_BANKS)
        if grief:
            # K-10: the sincere banks (talk_gen.c takes them for a grief or loss topic)
            rf |= RF["sincere"]
            if not r["act"]:
                die(where, "K-10: %s names its reply act (act: ...)" % rname)
            if ts_._starts(rname, ("comfort",)) and by_id[act] not in COMFORT_ACTS:
                die(where, "K-10: a comfort line is a reaction: act: %s" % " or ".join(a.lower() for a in COMFORT_ACTS))
            if "nojoke" not in r["tags"]:
                die(where, "K-10: %s needs tags: nojoke (reviewed: grief is not a punchline)" % rname)
        grief_lines = []
        if any(t.startswith("speaker=") for t in r["guard"]):
            rf |= RF["cast"]
        tic = rname.startswith("tag.") and any(t.startswith("type=") for t in r["guard"])
        if tic:
            rf |= RF["tic"]
        rterms = comp.terms(r["guard"], where)
        rule_term_first = len(term_rows)
        term_rows += rterms
        alt_first = len(alt_rows)
        if not r["lines"]:
            die(where, "rule %s has no lines" % rname)
        if len(r["lines"]) > 255:
            die(where, "more than 255 lines in one rule")
        for (text, g, lwhere), opt in zip(r["lines"], r["opts"]):
            ctext, face, implicit, refs, af, minlen = comp.compile_text(text, lwhere, symbol_of, r)
            vis = visible_text(text)
            hits = comp.register_hits(vis)
            # the S3 lint's hate match on every literal word (as the lint reads it: a word with its
            # apostrophe, squeezed) would refuse the line every time
            salt, exact_set = b.hate[0], set(b.hate[1])
            for word in re.findall(r"[A-Za-z0-9'][A-Za-z0-9'-]*", vis):
                if ts_.hate_key(salt, word) in exact_set:
                    die(lwhere, "a word the output lint's hate list refuses (squeezed, as the S3 lint reads it)")
            if hits:
                af |= AF["register"]
                if any(h.cls == "strong" for h in hits):
                    af |= AF["swear"]
                    if "blunt" not in (r["guard"] | opt["guard"]) or "adults_only" not in (r["guard"] | opt["guard"]):
                        die(lwhere, "an everyday swear (K-9) needs guard: blunt&adults_only&!kid_present")
            full = r["guard"] | opt["guard"]
            if any(t.startswith("speaker=") for t in full):
                af |= AF["cast"]
            if tic:
                af |= AF["tic"]
            # a callback stands on a fact that is true now (Q4 by construction)
            if rname == "cb" or rname.startswith("cb."):
                pos = [t for t in full | opt["needs"] if not t.startswith("!")]
                if not any(t in CB_FACTS or t.split("=")[0] in ("prob", "chapter") for t in pos):
                    die(lwhere, "a callback line stands on a fact: its guard names one (%s, prob=, chapter=)"
                        % ", ".join(sorted(CB_FACTS)))
            if "rel=partner" in full and not {"adults_only", "!kid_present"} <= full:
                die(lwhere, "a partner line needs guard: adults_only&!kid_present")
            # the special banks
            if rname == "safe" or rname.startswith("safe."):
                if full or opt["needs"] or re.search(r"\{[#A-Z]", ctext) or hits:
                    die(lwhere, "a safe line has no guard, no filler, no sub-rule and no register word "
                                "(it must play in any room)")
            if refusal or rname == "pet" or rname.startswith("pet.") or kid:
                if af & AF["topic"] and not kid:
                    die(lwhere, "%s: a refusal or the pet never says the topic (no echo)" % rname)
                if hits:
                    die(lwhere, "%s: a refusal, pet or kid bank has no register word" % rname)
                if af & AF["catch"] and refusal:
                    die(lwhere, "%s: no catchphrase in a refusal" % rname)
            if "fact.loss" in implicit and not grief:
                die(lwhere, "K-10: {lost} only in the grief and comfort banks")
            if grief:
                if hits:
                    die(lwhere, "K-10: a grief or comfort line has no register word")
                if refs:
                    die(lwhere, "K-10: a grief or comfort line has no sub-rule (all of it is sincere)")
                if af & (AF["catch"] | AF["jmat"]) or face == env.faces.get("LOVE"):
                    die(lwhere, "K-10: no catchphrase, [JMAT] or LOVE face in a grief or comfort line")
                grief_lines.append((r["guard"] | opt["guard"] | opt["needs"] | implicit) - {"topic"})
            # the line's own terms (rule terms are separate)
            lt = comp.terms(opt["guard"] | opt["needs"] | implicit, lwhere)
            if ctext not in str_index:
                str_index[ctext] = len(strings)
                strings.append(ctext)
            alt_rows.append((str_index[ctext], opt["w"], face, len(term_rows), len(lt), af))
            if not 1 <= opt["w"] <= 255:
                die(lwhere, "w=1..255")
            term_rows += lt
            alt_where.append(lwhere)
            alt_refs.append(refs)
            alt_min.append(minlen)
        rule_rows.append((alt_first, len(alt_rows) - alt_first, act, rule_term_first, len(rterms), rf))
        if grief:
            grief_rules.append((rname, by_id[act], grief_lines))
    if strict:
        # K-10: a grief topic is answered sincerely whatever was said, so every act that is not
        # a safety act has a grief line that may answer it and needs no guard but the act's own
        # (pact= / act=, which hold or not for that act alone); and each comfort act has one
        def free_for(terms, a, cls):
            for t in terms:
                neg = t.startswith("!")
                k, _, v = t.lstrip("!").partition("=")
                if k == "pact":
                    hold = v == cls
                elif k == "act":
                    hold = v == a.lower()
                else:
                    return False
                if hold == neg:
                    return False
            return True
        for a, (cls, reps) in sorted(acts.items()):
            if cls == "safety":
                continue
            if not any(ra in reps and any(free_for(t, a, cls) for t in lines)
                       for nm, ra, lines in grief_rules if ts_._starts(nm, ("grief",))):
                die("grammar", "K-10: no grief line without a guard answers %s (acts.tsv: %s)" % (a, ",".join(reps)))
        for a in COMFORT_ACTS:
            if not any(ra == a and any(not t for t in lines)
                       for nm, ra, lines in grief_rules if ts_._starts(nm, ("comfort",))):
                die("grammar", "K-10: no comfort line without a guard performs %s" % a)
    if len(alt_rows) > 0xFFFE or len(rules) > 0xFFFE or len(term_rows) > 0xFFFF:
        die("grammar", "too many rules, alternatives or terms for u16 ids")
    # the fit check: the shortest a line can be with the shortest fillers
    sym_min = {}

    def symmin(nm, stack):
        if nm in sym_min:
            return sym_min[nm]
        if nm in stack:
            die("grammar", "sub-rules call themselves: %s" % " -> ".join(stack + [nm]))
        if len(stack) > 4:
            die("grammar", "sub-rules nest deeper than 4: %s" % " -> ".join(stack + [nm]))
        best = None
        for rk in match(nm):
            fa, na = rule_rows[rk][0], rule_rows[rk][1]
            for ai in range(fa, fa + na):
                v = altmin(ai, stack + [nm])
                best = v if best is None else min(best, v)
        sym_min[nm] = best or 0
        return sym_min[nm]

    def altmin(ai, stack):
        return alt_min[ai] + sum(symmin(r, stack) for r in alt_refs[ai])

    for ai in range(len(alt_rows)):
        v = altmin(ai, [])
        if v > BUBBLE:
            die(alt_where[ai], "cannot fit the bubble (%d characters with the shortest fillers, %d max)"
                % (v, BUBBLE))
    # the symbol table
    syms, refs = [], []
    for nm in sym_names:
        rs = match(nm)
        syms.append((len(refs), len(rs)))
        refs += rs
    # the game maps
    map_b, map_info = game_maps(b, env, die)
    jobs_b, njobs, job_info = job_map(b, env, die)
    cast_b = b"".join(_u16(rid) for k, rid in env.cast)
    catch_b = catch_classes(b, env)
    # the acts table
    acts_b = bytearray()
    for a in range(env.nacts):
        nm = by_id.get(a)
        bits = [0, 0, 0]
        cls, dflt = 0, 0
        if a and nm in acts:
            c, reps = acts[nm]
            cls = CLASSES.index(c)
            dflt = env.acts[reps[0]]
            for rp in reps:
                v = env.acts[rp]
                bits[v >> 5] |= 1 << (v & 31)
        acts_b += _u32(bits[0]) + _u32(bits[1]) + _u32(bits[2]) + bytes((cls, dflt)) + _u16(0)
    if env.nacts > 96:
        die("talk.h", "more than 96 acts: widen the acts table")
    # pack
    body = bytearray(HDR)
    def at():
        return len(body)
    rules_off = at()
    for fa, na, act, tf, nt, rf in rule_rows:
        body += _u16(fa) + bytes((na, act)) + _u16(tf) + bytes((nt, rf))
    alts_off = at()
    for s, w, face, tf, nt, af in alt_rows:
        body += _u16(s) + bytes((w, face)) + _u16(tf) + bytes((nt, af))
    terms_off = at()
    for p, v in term_rows:
        body += bytes((p, v))
    syms_off = at()
    for f, n in syms:
        body += _u16(f) + _u16(n)
    refs_off = at()
    for r in refs:
        body += _u16(r)
    entry_off = at()
    for v in entries_core + entries_react + entries_fixed:
        body += _u16(v)
    acts_off = at()
    body += acts_b
    map_off = at()
    body += map_b + catch_b
    cast_off = at()
    body += cast_b
    jobs_off = at()
    body += jobs_b
    hdr = _u32(GRAMMAR_MAGIC) + b"".join(_u16(x) for x in (len(rule_rows), len(alt_rows), len(term_rows), len(syms),
                                                           len(refs), len(env.cast), njobs, env.nacts)) + \
        b"".join(_u32(x) for x in (rules_off, alts_off, terms_off, syms_off, refs_off, entry_off, acts_off, map_off,
                                   cast_off, jobs_off)) + _u16(env.tge_count) + _u16(0)
    assert len(hdr) == HDR, len(hdr)
    body[0:HDR] = hdr
    info = {"alt_where": alt_where,
            "files": len(files), "rules": len(rule_rows), "alts": len(alt_rows), "terms": len(term_rows),
            "syms": len(syms), "strings": len(strings), "jobs": job_info, "map": map_info,
            "entries": sum(1 for v in entries_core if v != 0xFFFF)}
    return bytes(body), strings, info


def _surface_index(b):
    idx = {}
    for c in b.concepts:
        for s in [c.lemma] + ([c.plural] if c.plural else []) + list(c.syn):
            idx.setdefault(s.lower(), c)
    return idx


def find_concept(idx, name):
    import talkc as T
    low = name.lower()
    cands = [low, T.lower_name(name).lower()]
    for s in list(cands):
        if s.startswith("the "):
            cands.append(s[4:])
        if s.endswith("ies"):
            cands.append(s[:-3] + "y")
        if s.endswith("es"):
            cands.append(s[:-2])
        if s.endswith("s"):
            cands.append(s[:-1])
    for s in cands:
        if s in idx:
            return idx[s]
    return None


def game_maps(b, env, die):
    import talkc as T
    idx = _surface_index(b)
    out = bytearray()
    info = {}
    for arr, n in (("doomo_food_names", 48), ("doomo_hobby_names", 32), ("doomo_music_names", 16),
                   ("doomo_fear_names", 24)):
        names = T.c_array(env.cat_c, arr)
        if len(names) != n:
            die("catalog.c", "%s has %d names, talkc_gen expects %d" % (arr, len(names), n))
        for nm in names:
            c = find_concept(idx, nm)
            if c is None:
                die("catalog.c", "%s %r has no concept (talkc --seed-from-game makes one)" % (arr, nm))
            if c.age != "any" or c.reg != "clean":
                die("catalog.c", "%s %r maps to %s, which is not kid-safe" % (arr, nm, c.key))
            out += _u16(c.id)
        info[arr] = n
    return bytes(out), info


def job_map(b, env, die):
    import talkc as T
    idx = _surface_index(b)
    names, jobs, traits = T.roster_entries()
    n = 1000
    out = bytearray()
    mapped = 0
    for rid in range(n):
        j = jobs.get(rid)
        cid = 0
        if j:
            c = find_concept(idx, j)
            if c is not None and c.age == "any" and c.reg == "clean" and not (c.fl & T.TCF["minor"]):
                cid = c.id
                mapped += 1
        out += _u16(cid)
    return bytes(out), n, mapped


def catch_classes(b, env):
    out = bytearray()
    never = []
    for c in b.concepts:
        if c.age == "never":
            never += [s.lower() for s in [c.lemma] + ([c.plural] if c.plural else []) + list(c.syn)]
    for cp in env.catchphrases[:64]:
        v = 0
        for w in b.register:
            if w.rx.search(cp):
                v |= CC["register"]
                if w.cls == "strong":
                    v |= CC["swear"]
                if w.cls == "sexual" or w.cap is not None:
                    v |= CC["forbid"]
        low = cp.lower()
        for s in never:
            if len(s) >= 3 and re.search(r"(?<![\w])" + re.escape(s) + r"(?![\w])", low):
                v |= CC["forbid"]
        out.append(v)
    while len(out) < 64:
        out.append(CC["forbid"])
    return bytes(out)


def grammar_files(lang):
    import talkc as T
    out = []
    for sub in ("grammar", "voice"):
        d = os.path.join(T.DATA, sub, lang)
        if os.path.isdir(d):
            out += [os.path.join(d, f) for f in sorted(os.listdir(d)) if f.endswith(".tlk")]
    return out
