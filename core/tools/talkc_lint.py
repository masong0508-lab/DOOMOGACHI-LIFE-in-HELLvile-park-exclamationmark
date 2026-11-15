"""talkc_lint.py - the coherence lint of the Talk grammar, rules L1 to L10 (owner: TK; TM2b;
docs/design/talk/TALK_MULTILEVEL.md section 7.1, docs/TALK.md "The coherence lint"). Imported by
talkc_gen.py (build_grammar) and talkc.py (--lint, --lint-update); stdlib only.

"The layers do not match the act and the topic" is a measurable rule. The lint reads the parsed
rules (talkc_safe.read_tlk) and the acts table, derives the LAYER and the reply KIND of every rule
(from its name, its `act:` and its guards) and reports findings, each a line of text:

  L1   a core rule's act: is not a legal reply to its entry act               fails at once
  L2   the reply kind and the stance= guard disagree                          ratchet
  L3   a question about a topic answered by a line that never names it         ratchet
  L4   "it" / "that" / "this" with no topic to point at                       ratchet
  L5   a no / refusal / deflection of a request or question that is a dead end ratchet
  L6   a filler word (or under 12 visible characters) as the whole reply       ratchet
  L7   a follow-up that does not follow, or asks about nothing                 ratchet
  L8   depth without a gate (depth=)                                           fails at once
  L9   a trait outside its lane (trait=)                                       fails at once
  L10  a non sequitur marker ("anyway" with no topic slot)                    ratchet

The ratchet: core/data/talk/lint_baseline.tsv holds the findings that exist today (code, rule, text,
tab separated, one line each). A finding that is not in the baseline fails the build; a baseline
line that no longer matches a finding fails it too ("delete it"), so the count only goes down. A
fix is a guard (`topic`, `!topic`) or a tag (`tags: generic`, `tags: closes`), never a deletion.
`python3 core/tools/talkc.py --lint` prints the table, `--lint-update` rewrites the baseline.
"""
import os
import re

CODES = ("L1", "L2", "L3", "L4", "L5", "L6", "L7", "L8", "L9", "L10")
HARD = ("L1", "L8", "L9")                     # fail at once; the others are a ratchet against the baseline
TITLES = {
    "L1": "reply act not legal after the entry act",
    "L2": "reply kind disagrees with the stance guard",
    "L3": "topic present, topic unused",
    "L4": "pronoun with nothing to point at",
    "L5": "dead-end reply",
    "L6": "filler as the whole reply",
    "L7": "hook out of place",
    "L8": "depth without gate",
    "L9": "trait outside its lane",
    "L10": "non sequitur marker",
}

# ---- text helpers (the same reading as core/tools/talk_layers.py) -------------------------------
SLOT = re.compile(r"\{(x|food|hobby|music|fear|hate|like|mem)(\.[a-z]+)?(?::[a-z]+)*\}")
SUBRULE = re.compile(r"\{([a-z_]+(?:\.[a-z_0-9]+)+)\}")
CODES_RX = re.compile(r"\{(\.\.\.|\.\.|\.|d|!|big|/|w|r|punch)\}|\{(p|cut) \d+\}|\[[A-Z]+\]")
FILLER = re.compile(r"^(hm+|mm+|uh+|um+|oh|ah+|huh|eh|sure|ok|okay|fine|yeah|yep|yes|no|nope|nah|right|well|"
                    r"wow|cool|nice|whatever|indeed|i see|if you say so|go on|interesting|really|true|maybe|"
                    r"perhaps|hmph|meh|so|anyway|noted|sigh|ha|haha|heh|lol)\W*$", re.I)
PRONOUN = re.compile(r"\b(it|its|it's|that|this|them)\b", re.I)
NONSEQ = re.compile(r"\b(anyway|by the way|speaking of|unrelated|random|did you know)\b", re.I)
# a reason or a counter-offer in a line that turns something down (L5)
ESCAPE = re.compile(r"\?|\b(because|but|though|however|instead|rather|unless|maybe|perhaps|how about|what about|"
                    r"try|later|tomorrow|tonight|until|otherwise|first|busy|tired|asleep|broke|nap|can't|cannot|"
                    r"too|if you|ask)\b", re.I)


def visible(text):
    t = CODES_RX.sub("", text)
    t = re.sub(r"\*[^*]*\*", "", t)           # *shrugs*
    return re.sub(r"\s+", " ", t).strip()


def _short(text, n=70):
    v = visible(text).replace("\t", " ")
    return v if len(v) <= n else v[:n]


# a guard term that says the topic exists (or is about it): a pronoun has something to point at
TOPIC_POS = re.compile(r"^(topic|likes_topic|fears_topic|hates_topic|stance=(pro|con)|root=\w+|x\.\w+)$")
# ... or that the author decided about it either way (L3: "!topic" is the line for no topic)
TOPIC_ANY = re.compile(r"^!?(topic|likes_topic|fears_topic|hates_topic)$|^(stance=(pro|con)|root=\w+|x\.\w+)$")

# ---- layers and kinds (TALK_MULTILEVEL.md 2 and 7.1; the prototype is talk_layers.py) -----------
ASKED = ("ask_about", "ask_opinion", "change_topic", "gossip")      # L3: a question about a topic
OPINE = ("opine_pos", "opine_neg")                                  # L4 adds these
REQUEST_ENTRIES = ("ask_favor", "invite")                           # L5: a request: a no closes the door
QUESTION_ENTRIES = ("ask_fact", "ask_advice", "ask_about", "ask_opinion", "ask_who", "ask_repeat")
# L5: a question may be answered with a no (an answer); only a dismissal or a topic change closes it
L6_EXEMPT = ("backchannel", "wave", "nod", "high_five", "laugh", "greet", "farewell", "hug", "give", "thank")
EDGY = ("hotheaded", "romantic", "snob", "jealous", "mean")         # TALK_MULTILEVEL.md 5.4
TRAITS = ("hotheaded", "romantic", "goofball", "snob", "neat", "slob", "bookworm", "cheerful", "gloomy", "jealous",
          "loyal", "mean", "kind", "lazy", "ambitious", "paranoid", "selfassured", "shy", "outgoing", "creative",
          "glutton", "morbid", "curious", "honest")
SINCERE_BANKS = ("refuse", "deflect", "distress", "end", "grief", "comfort", "pet", "safe", "kidask")
ENTRY_VALENCE = {"opine_pos": 1, "opine_neg": -1}


def layer_of(rule):
    """the layer a rule serves in the multilevel plan: a reaction, b content, c colour, d hook, x kept as is"""
    n = rule["name"]
    top = n.split(".")[0]
    if top in ("open", "tag"):
        return "c"
    if top in ("handoff", "follow", "cb"):
        return "d"
    if top == "react":
        return "a"
    if top == "core":
        if re.search(r"\.(pro|con|neutral|agree|disagree)\b", n):
            return "ab"
        if any(t.startswith("type=") or t.startswith("speaker=") for t in rule["guard"]) or \
                re.search(r"\.(type|voice)\b", n):
            return "c"
        return "b"
    return "x"


KIND = {}
for _a in ("YES", "OFFER", "PROMISE"):
    KIND[_a] = "accept"
for _a in ("AGREE", "OPINE_POS"):
    KIND[_a] = "agree"
for _a in ("NO", "DISAGREE", "OPINE_NEG"):
    KIND[_a] = "deny"
for _a in ("REFUSE_SEXUAL", "REFUSE_MINOR", "REFUSE_HATE"):
    KIND[_a] = "refuse"
for _a in ("DEFLECT_ROMANCE", "DISMISS", "CHANGE_TOPIC"):
    KIND[_a] = "deflect"
for _a in ("ASK_ABOUT", "ASK_OPINION", "ASK_FEELING", "ASK_FACT", "ASK_WHO", "ASK_ADVICE", "ASK_FAVOR",
           "ASK_REPEAT"):
    KIND[_a] = "askback"


def kind_of(act):
    """the reply kind of an act name (TK_*): accept agree deny refuse deflect askback, else say"""
    return KIND.get((act or "").upper(), "say")


def entry_of(rule):
    """the player's act a core rule answers (lower case, as named) or None"""
    n = rule["name"].split(".")
    return n[1] if n[0] == "core" and len(n) > 1 else None


def _slot_reader(rules):
    """-> f(rule name, depth): True when the rule (or a sub-rule it calls, 4 deep) can name the topic"""
    by_name = {}
    for r in rules:
        by_name.setdefault(r["name"], []).append(r)
    names = list(by_name)
    memo = {}

    def has_slot(nm, depth=0):
        if nm in memo:
            return memo[nm]
        if depth > 4:
            return False
        memo[nm] = False
        got = False
        for m in names:
            if m == nm or m.startswith(nm + "."):
                for r in by_name[m]:
                    for text, _g, _w in r["lines"]:
                        if SLOT.search(text) or any(has_slot(s, depth + 1) for s in SUBRULE.findall(text)):
                            got = True
        memo[nm] = got
        return got
    return has_slot


def lint(rules, acts):
    """-> [(code, key, where, message)]; key is code<TAB>rule<TAB>text, the baseline's line.
    rules: the parsed .tlk rules; acts: {ACT: (class, [replies])} (talkc_gen.load_acts)"""
    out = []
    seen = set()
    has_slot = _slot_reader(rules)

    def add(code, r, text, where, msg):
        key = "%s\t%s\t%s" % (code, r["name"], _short(text))
        if key in seen:
            return
        seen.add(key)
        out.append((code, key, where, "TK13 %s %s: rule %s: %s" % (code, TITLES[code], r["name"], msg)))

    def names_topic(text):
        return bool(SLOT.search(text) or any(has_slot(s) for s in SUBRULE.findall(text)))

    for r in rules:
        name = r["name"]
        top = name.split(".")[0]
        act = (r["act"] or "").upper()
        ent = entry_of(r)
        ent_u = ent.upper() if ent else None
        rtags = r["tags"]
        # L1: a core rule's reply act is one the entry act allows
        if ent_u and ent_u in acts and act and act not in acts[ent_u][1]:
            out.append(("L1", "L1\t%s\t%s" % (name, act), r["where"],
                        "TK13 L1 %s: rule %s: reply act %s may not answer %s (acts.tsv)" %
                        (TITLES["L1"], name, act, ent_u)))
        for text, g, where in r["lines"]:
            full = set(g)                                   # the rule's guard and the line's
            vis = visible(text)
            stance = [t.split("=")[1] for t in full if t.startswith("stance=")]
            # L2: the reply kind against the stance guard (an AGREE or DISAGREE reads the entry act's side)
            if act and stance:
                val = 0
                if act in ("YES", "OPINE_POS"):
                    val = 1
                elif act in ("NO", "OPINE_NEG"):
                    val = -1
                elif act in ("AGREE", "DISAGREE") and ent in ENTRY_VALENCE:
                    val = ENTRY_VALENCE[ent] * (1 if act == "AGREE" else -1)
                if (val < 0 and "pro" in stance) or (val > 0 and "con" in stance):
                    add("L2", r, text, where, "act %s says the opposite of its stance guard %s" %
                        (act.lower(), "/".join(sorted(set(stance)))))
            if top == "core" and ent in ASKED + OPINE:
                # L3: a question about a topic answered with a line that never names it
                if ent in ASKED and "generic" not in rtags and not names_topic(text) \
                        and not any(TOPIC_ANY.match(t) for t in full):
                    add("L3", r, text, where, "answers a question about a topic and names no topic "
                        "(add a slot, a topic / !topic guard or tags: generic)")
                # L4: a pronoun with nothing to point at
                if PRONOUN.search(vis) and "generic" not in rtags \
                        and not (any(TOPIC_POS.match(t) for t in full) or names_topic(text)):
                    add("L4", r, text, where, "says it / that / this and has no topic guard and no slot "
                        "(add guard: topic, or tags: generic when it is not the topic)")
            # L5: a dead end after a request or a question
            if top == "core" and "closes" not in rtags \
                    and ((ent in REQUEST_ENTRIES and kind_of(act) in ("deflect", "deny", "refuse"))
                         or (ent in QUESTION_ENTRIES and kind_of(act) in ("deflect", "refuse"))) \
                    and ent is not None and not ESCAPE.search(vis) and not SUBRULE.search(text):
                add("L5", r, text, where, "turns it down and gives no reason, counter or question "
                    "(or tags: closes)")
            # L6: a filler as the whole reply
            if top in ("core", "react", "follow"):
                a6 = ent if top == "core" else (name.split(".")[1] if top == "react" and "." in name else "")
                if a6 not in L6_EXEMPT and (FILLER.match(vis) or (vis and len(vis) < 12)):
                    add("L6", r, text, where, "the whole reply is %r (%d visible characters)" % (vis, len(vis)))
            # L10: a non sequitur marker
            if top == "core" and NONSEQ.search(vis) and not SLOT.search(text) and "generic" not in rtags:
                add("L10", r, text, where, "a topic-change phrase with no topic slot (add a slot or "
                    "tags: generic)")
        # L7: a follow-up that does not follow what it follows
        if top == "follow" and act:
            followed = set()
            for t in r["guard"]:
                if t.startswith("act="):
                    followed.add(t.split("=")[1].upper())
            by_class = not followed
            if by_class:                                    # pact=warm: any act of the class
                pact = [t.split("=")[1] for t in r["guard"] if t.startswith("pact=")]
                followed = set(a for a, (cls, reps) in acts.items() if cls in pact)

            def after(a):
                # the resident answered with one of replies[a]; the follow-up is adjacent after that answer
                return a in acts and any(act in acts[x][1] for x in acts[a][1] if x in acts)
            ok = [a for a in followed if after(a)]
            if followed and (not ok if by_class else len(ok) < len(followed)):
                bad = sorted(a.lower() for a in followed if not after(a))
                add("L7", r, name, r["where"], "its question act %s cannot follow any reply to %s (acts.tsv)" %
                    (act.lower(), ", ".join(bad)))
            if act in followed and kind_of(act) == "askback":
                add("L7", r, name, r["where"], "it repeats the question the player just asked (%s)" % act.lower())
            if act in ("ASK_ABOUT", "ASK_OPINION"):
                for text, g, where in r["lines"]:
                    if PRONOUN.search(visible(text)) and not (any(TOPIC_POS.match(t) for t in g) or SLOT.search(text)):
                        add("L7", r, text, where, "asks about a topic (it / that) but has no topic guard and no slot")
        # L8 / L9: the guards of the multilevel plan (new rules only: nothing in the bank uses them yet)
        for _t, g, where in (r["lines"] or [("", set(r["guard"]), r["where"])]):
            for t in sorted(g):
                m = re.match(r"^depth=(\d+)$", t)
                if m:
                    d = int(m.group(1))
                    rel = [x.split("=")[1] for x in g if x.startswith("rel=")]
                    if d >= 2 and not (rel or any(x.startswith("fact.") for x in g)):
                        out.append(("L8", "L8\t%s\tdepth=%d" % (name, d), where,
                                    "TK13 L8 %s: rule %s: depth>=2 needs a rel= or fact.* guard"
                                    % (TITLES["L8"], name)))
                    if d >= 3 and not any(x in ("best", "partner", "family") for x in rel):
                        out.append(("L8", "L8\t%s\tdepth=%d rel" % (name, d), where,
                                    "TK13 L8 %s: rule %s: depth>=3 needs rel=best, partner or family"
                                    % (TITLES["L8"], name)))
                if re.match(r"^!?trait=", t):
                    tr = t.split("=")[1].replace("_", "").replace("-", "")
                    if tr not in TRAITS:
                        out.append(("L9", "L9\t%s\t%s" % (name, t), where,
                                    "TK13 L9 %s: rule %s: %s is not one of the 24 traits" % (TITLES["L9"], name, t)))
                    elif top in SINCERE_BANKS or name.startswith("beat.kid"):
                        out.append(("L9", "L9\t%s\t%s" % (name, t), where,
                                    "TK13 L9 %s: rule %s: a trait guard in a refusal, grief, distress, pet or "
                                    "K-7 bank" % (TITLES["L9"], name)))
                    elif tr in EDGY and not (not t.startswith("!") and "adults_only" in g and "!kid_present" in g):
                        out.append(("L9", "L9\t%s\t%s" % (name, t), where,
                                    "TK13 L9 %s: rule %s: the edgy trait %s needs guard: adults_only&!kid_present"
                                    % (TITLES["L9"], name, t)))
    return out


# ---- the baseline ----------------------------------------------------------------------------------
def load_baseline(path):
    keys = []
    if path and os.path.exists(path):
        for line in open(path, encoding="utf-8").read().split("\n"):
            if line.strip() and not line.startswith("#"):
                keys.append(line.rstrip("\r"))
    return keys


BASELINE_HEAD = """# core/data/talk/lint_baseline.tsv - the coherence lint's frozen findings (owner: TK; TM2b;
# docs/design/talk/TALK_MULTILEVEL.md 7.1, docs/TALK.md "The coherence lint").
# CODE<TAB>RULE<TAB>TEXT: a finding talkc.py --lint reports today. A finding that is not here fails the
# build; a line here that no longer matches a finding fails it too (delete the line): the count only
# goes down. Fix with a guard (topic / !topic) or a tag (generic / closes), never by deleting a line of
# the bank. Rewrite with: python3 core/tools/talkc.py --lint-update
"""


def write_baseline(path, findings):
    keys = sorted(set(k for c, k, w, m in findings if c not in HARD),
                  key=lambda k: (CODES.index(k.split("\t")[0]), k))
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(BASELINE_HEAD)
        for k in keys:
            f.write(k + "\n")
    return len(keys)


def check(findings, baseline, strict, die):
    """fail on a finding outside the baseline (and, in strict mode, on a baseline line with no finding)"""
    base = set(baseline) if strict else set()
    msgs = [(where, msg) for code, key, where, msg in findings if code in HARD or key not in base]
    if msgs:
        where, msg = msgs[0]
        die(where, msg + ("  (+%d more lint findings)" % (len(msgs) - 1) if len(msgs) > 1 else ""))
    if strict:
        seen = set(k for c, k, w, m in findings)
        stale = [k for k in baseline if k not in seen]
        if stale:
            die("lint_baseline.tsv", "%d baseline line(s) no longer match a finding: delete them (the ratchet "
                "only goes down); first: %s" % (len(stale), stale[0].replace("\t", " | ")))


def table(findings, baseline):
    """-> text: findings and baseline lines per rule code"""
    cnt = {c: 0 for c in CODES}
    for c, k, w, m in findings:
        cnt[c] += 1
    base = {c: 0 for c in CODES}
    for k in baseline:
        c = k.split("\t")[0]
        if c in base:
            base[c] += 1
    lines = ["rule  findings  baseline  what"]
    for c in CODES:
        lines.append("%-5s %8d  %8d  %s" % (c, cnt[c], base[c], TITLES[c]))
    return "\n".join(lines)
