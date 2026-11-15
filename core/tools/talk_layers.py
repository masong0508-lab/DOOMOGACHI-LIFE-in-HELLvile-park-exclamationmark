#!/usr/bin/env python3
"""talk_layers.py - read-only analysis of the Talk grammar for docs/design/talk/TALK_MULTILEVEL.md
(owner: TK; package TM0-DESIGN).

It reads core/data/talk/grammar/<lang>/*.tlk and voice/<lang>/*.tlk (the syntax is
core/tools/talkc_gen.py's) and prints, with no side effects:

  layers   how many rules and alternatives sit in each layer of the multilevel plan
           (a reaction, b content, c colour, d hook, plus the parts that stay as they are)
  fillers  the alternatives that read as a filler non-response ("Hm.", a shrug, "...") and the
           ones in a topic act that say nothing about the topic (a possible non sequitur)
  acts     per player act: core alternatives, with and without a topic slot
  matrix   player act x reply kind (accept agree deny refuse deflect counter askback): alternatives per cell
  all      the three above

  python3 core/tools/talk_layers.py [all|layers|fillers|acts|matrix] [--lang en] [--list N] [--tsv FILE]

--list N   print N sample lines of every filler class; --tsv FILE writes every finding
           (class, file:line, rule, text) for a reviewer. Nothing here is used by the build.
"""
import os
import re
import sys
from collections import Counter, defaultdict

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data", "talk"))
# a guard that already needs a topic (or says there is none): the line is not topic-blind
TOPIC_GUARD = re.compile(r"(^|&)(!?topic|likes_topic|fears_topic|hates_topic|stance=(pro|con)|root=\w+|x\.\w+)(&|$)")
ASKED_ACTS = {"ask_about", "ask_opinion", "change_topic", "gossip", "opine_pos", "opine_neg"}
TOPIC_ACTS = {"ask_about", "ask_opinion", "tell", "opine_pos", "opine_neg", "change_topic", "continue",
              "agree", "disagree", "answer", "gossip"}
SLOT = re.compile(r"\{(x|food|hobby|music|fear|hate|like|mem)(\.[a-z]+)?(?::[a-z]+)*\}")
SUB = re.compile(r"\{([a-z_]+(?:\.[a-z_0-9]+)+)\}")
CODES = re.compile(r"\{(\.\.\.|\.\.|\.|d|!|big|/|w|r|punch)\}|\[[A-Z]+\]")
FILLER = re.compile(r"^(hm+|mm+|uh+|um+|oh|ah+|huh|eh|sure|ok|okay|fine|yeah|yep|yes|no|nope|nah|right|well|"
                    r"wow|cool|nice|whatever|indeed|i see|if you say so|go on|interesting|really|true|maybe|"
                    r"perhaps|hmph|meh|so|anyway|noted|sigh|ha|haha|heh|lol)\W*$", re.I)


def visible(t):
    t = CODES.sub("", t)
    t = re.sub(r"\*[^*]*\*", "", t)           # *shrugs*
    return re.sub(r"\s+", " ", t).strip()


def load(lang):
    """-> list of dicts: file, line, rule, guard, act, tags, text"""
    out = []
    dirs = [os.path.join(ROOT, "grammar", lang), os.path.join(ROOT, "voice", lang)]
    for d in dirs:
        if not os.path.isdir(d):
            continue
        for fn in sorted(os.listdir(d)):
            if not fn.endswith(".tlk"):
                continue
            rule = None
            for ln, raw in enumerate(open(os.path.join(d, fn), encoding="utf-8"), 1):
                line = raw.rstrip("\n")
                if line.startswith("@rule"):
                    m = re.match(r"@rule\s+(\S+)(.*)$", line)
                    rest = m.group(2)
                    g = re.search(r"guard:\s*(.*?)(?=\s+(?:act|tags|needs):|$)", rest)
                    a = re.search(r"act:\s*(\S+)", rest)
                    t = re.search(r"tags:\s*(.*?)(?=\s+(?:act|guard|needs):|$)", rest)
                    rule = dict(name=m.group(1), guard=g.group(1) if g else "", act=a.group(1) if a else "",
                                tags=t.group(1) if t else "")
                elif rule and line.startswith("  \"") and line.rstrip().find('"', 3) > 0:
                    body = line.strip()
                    end = body.rfind('"')
                    text = body[1:end]
                    out.append(dict(file=fn, line=ln, rule=rule["name"], guard=rule["guard"], act=rule["act"],
                                    tags=rule["tags"], text=text, opts=body[end + 1:].strip()))
    return out


def family(name):
    p = name.split(".")
    return p[0] if p[0] != "core" else "core." + (p[1] if len(p) > 1 else "")


def layer_of(a):
    """the layer a rule would serve in the multilevel plan (docs/design/talk/TALK_MULTILEVEL.md section 2)"""
    n = a["rule"]
    top = n.split(".")[0]
    if top in ("open", "tag", "handoff"):
        return {"open": "c colour (opener)", "tag": "c colour (tag)", "handoff": "d hook (hand-off)"}[top]
    if top in ("follow", "kidask"):
        return "d hook (follow-up)"
    if top == "cb":
        return "d hook (callback)"
    if top in ("refuse", "deflect", "distress", "safe", "pet", "end", "grief", "comfort"):
        return "x safety / sincere (kept as is)"
    if top in ("beat", "idle", "prod", "excuse", "grumble", "jab", "row", "ultimatum", "makeup"):
        return "x drama manager (kept as is)"
    if top == "react":
        return "a reaction (resident to resident)"
    if top == "core":
        if re.search(r"\.(pro|con|neutral|agree|disagree)\b", n):
            return "a reaction + b content (fused core)"
        if "type=" in a["guard"] or "speaker=" in a["guard"] or re.search(r"\.(type|voice)\b", n):
            return "c colour (voiced core)"
        return "b content (core)"
    return "x other"


def classes(a):
    """filler classes of one alternative"""
    v = visible(a["text"])
    got = []
    top = a["rule"].split(".")[0]
    if top not in ("core", "react", "follow", "beat"):
        return got
    if FILLER.match(v):
        got.append("F1 the whole line is a filler word")
    elif v and len(v) < 12 and top == "core":
        got.append("F2 under 12 visible characters")
    if top == "core":
        act = a["rule"].split(".")[1] if "." in a["rule"] else ""
        if act in TOPIC_ACTS and not SLOT.search(a["text"]) and not SUB.search(a["text"]) \
                and not TOPIC_GUARD.search(a["guard"]) and "notopic" not in a["rule"]:
            got.append("N1a question about a topic, answer names no topic" if act in ASKED_ACTS
                       else "N1b topic act, line is topic-blind (generic listener)")
        if re.search(r"\b(anyway|by the way|speaking of|unrelated|random|did you know)\b", v, re.I) \
                and not SLOT.search(a["text"]):
            got.append("N2 topic-change phrase with no topic slot")
    return got


def cmd_layers(rows):
    by = defaultdict(lambda: [set(), 0])
    for a in rows:
        k = layer_of(a)
        by[k][0].add(a["rule"])
        by[k][1] += 1
    print("layer                                     rules   alternatives")
    tot = 0
    for k in sorted(by):
        print("%-40s %6d %8d" % (k, len(by[k][0]), by[k][1]))
        tot += by[k][1]
    print("%-40s %6s %8d" % ("total", "", tot))


def cmd_acts(rows):
    d = defaultdict(lambda: [0, 0, 0])
    for a in rows:
        p = a["rule"].split(".")
        if p[0] != "core" or len(p) < 2:
            continue
        d[p[1]][0] += 1
        if SLOT.search(a["text"]):
            d[p[1]][1] += 1
        if not SLOT.search(a["text"]) and not SUB.search(a["text"]):
            d[p[1]][2] += 1
    print("core entry          alts  with topic/like slot  plain (no slot, no sub-rule)")
    for k in sorted(d, key=lambda k: -d[k][0]):
        print("%-18s %5d %10d %20d" % (k, d[k][0], d[k][1], d[k][2]))


ACTS_TSV = os.path.join(ROOT, "acts.tsv")


def load_acts():
    """-> {act: (class, [reply acts])} from acts.tsv (lower case)"""
    d = {}
    for raw in open(ACTS_TSV, encoding="utf-8"):
        if raw.startswith("#") or not raw.strip():
            continue
        a, c, r = raw.rstrip("\n").split("\t")
        d[a.lower()] = (c, [x.lower() for x in r.split(",")])
    return d


# the reply kinds of the reaction layer (TALK_MULTILEVEL.md 3.1): which reply acts (`act:` of a rule) serve each
KINDS = {
    "accept": ["yes", "thank", "agree", "offer", "encourage"],
    "agree": ["agree", "opine_pos", "sympathize", "laugh"],
    "deny": ["no", "disagree", "opine_neg"],
    "refuse": ["no", "complain", "deflect_romance", "refuse_minor", "refuse_sexual", "refuse_hate"],
    "deflect": ["joke", "dismiss", "tease", "backchannel"],
    "counter": ["change_topic", "offer", "answer", "tell"],
    "askback": ["ask_about", "ask_opinion", "ask_feeling", "ask_repeat"],
}


def cmd_matrix(rows):
    """player act x reply kind: how many alternatives of core.<act> carry a reply act that serves the kind"""
    acts = load_acts()
    cell = defaultdict(lambda: defaultdict(int))
    for a in rows:
        p = a["rule"].split(".")
        if p[0] != "core" or len(p) < 2 or p[1] not in acts:
            continue
        reply = a["act"] or (acts[p[1]][1][0] if acts[p[1]][1] else "")
        for k, lst in KINDS.items():
            if reply in lst:
                cell[p[1]][k] += 1
    print("player act        " + " ".join("%8s" % k for k in KINDS) + "   kinds the act allows with 0 alternatives")
    gaps = 0
    for act in acts:
        if acts[act][0] in ("safety",):
            continue
        allowed = {k for k, lst in KINDS.items() if any(x in lst for x in acts[act][1])}
        miss = sorted(k for k in allowed if cell[act][k] == 0)
        gaps += len(miss)
        print("%-17s " % act + " ".join("%8d" % cell[act][k] for k in KINDS) + "   " + ",".join(miss))
    print("allowed (act, kind) cells with no alternative at all:", gaps)


def cmd_fillers(rows, nlist, tsv):
    cnt = Counter()
    ex = defaultdict(list)
    byrule = defaultdict(Counter)
    fil = []
    for a in rows:
        for c in classes(a):
            cnt[c] += 1
            ex[c].append(a)
            byrule[c][family(a["rule"])] += 1
            fil.append((c, a))
    print("class                                          alternatives")
    for c in sorted(cnt):
        print("%-48s %5d" % (c, cnt[c]))
        for fam, n in byrule[c].most_common(6):
            print("      %-30s %4d" % (fam, n))
        for a in ex[c][:nlist]:
            print("      %s:%d %s: %s" % (a["file"], a["line"], a["rule"], visible(a["text"])))
    if tsv:
        with open(tsv, "w", encoding="utf-8") as f:
            f.write("class\tfile:line\trule\tguard\ttext\n")
            for c, a in fil:
                f.write("%s\t%s:%d\t%s\t%s\t%s\n" % (c[:2], a["file"], a["line"], a["rule"], a["guard"], a["text"]))
        print("wrote", tsv, len(fil), "rows")


def main(argv):
    lang, nlist, tsv, cmd = "en", 0, None, "all"
    i = 1
    while i < len(argv):
        x = argv[i]
        if x == "--lang":
            lang = argv[i + 1]; i += 1
        elif x == "--list":
            nlist = int(argv[i + 1]); i += 1
        elif x == "--tsv":
            tsv = argv[i + 1]; i += 1
        else:
            cmd = x
        i += 1
    rows = load(lang)
    print("%d alternatives in %d rules" % (len(rows), len({a["rule"] for a in rows})))
    if cmd in ("all", "layers"):
        cmd_layers(rows)
    if cmd in ("all", "acts"):
        print()
        cmd_acts(rows)
    if cmd in ("all", "matrix"):
        print()
        cmd_matrix(rows)
    if cmd in ("all", "fillers"):
        print()
        cmd_fillers(rows, nlist, tsv)


if __name__ == "__main__":
    main(sys.argv)
