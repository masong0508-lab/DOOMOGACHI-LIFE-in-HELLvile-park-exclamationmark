#!/usr/bin/env python3
"""talk_assoc.py - association SUGGESTIONS for the Talk word network (owner:
TK, T1; docs/TALK.md "The word network").

    python3 core/tools/talk_assoc.py [--lang en] [--out FILE]
    python3 core/tools/talk_assoc.py --accept SUGGESTIONS [--top 8]

The first form writes core/build/talk/assoc_suggest_<lang>.tsv (never
committed): for every talkable concept, up to 12 other concepts, strongest
first, as OTHER:W (W 1..15), each with the reasons that put it there. The
scores come from:

  pmi    pointwise mutual information of the two concepts' surface forms
         over the game's own text: every quoted line of the story packs
         (core/data/story/**/*.dsc), the roster bios, the furniture
         descriptions, the English UI strings (a line is one context; a
         pair must meet in at least 2 lines)
  attr   the overlap of their attribute lists (Jaccard)
  sib    the same leaf category (a little), the same root (less)
  word   one's lemma contains the other's ("bone throne" -> "bone"), or
         they share a content word ("pink sofa", "pink rug")
  hand   assoc_hand_<lang>.tsv: edges a writer wants whatever the text says
         (the cast and their things: Nana Puddin -> pie)

Hard rules, here and in talkc.py's check: no edge touches a describe.* or
safety.* concept, and an edge INTO an adults-only concept comes only FROM
an adults-only one (activation can never walk from "cider" talk in a
kids' room to the pub), and no edge leads from an adults-only concept to a
word that names a child (TCF_MINOR_WORD). The second form is the reviewer's tool: it copies
the top N of each suggestion line (hand edges always kept) into
core/data/talk/assoc_<lang>.tsv, which is what ships. Review the file
(delete or reweight lines) before committing it: talkc.py never reads the
suggestions.
stdlib only, deterministic.
"""
import math
import os
import zlib
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import talkc  # noqa: E402

STOP = set("""a an the and or of to in on at for with by from up down out over under is are was were be been
being it its it's this that these those i you he she they we me him her them us my your his our their
so but if not no yes do does did done have has had just very really too also than then there here what
who why how when where which some any all one two get got go going gone like make made can could would
should will shall may might must oh ok okay hey""".split())


def corpus_lines():
    out = []
    story = os.path.join(talkc.CORE, "data", "story")
    for d, _, files in sorted(os.walk(story)):
        for f in sorted(files):
            if f.endswith(".dsc"):
                for line in open(os.path.join(d, f), encoding="utf-8", errors="replace"):
                    for q in re.findall(r"\"((?:[^\"\\]|\\.)*)\"", line):
                        out.append(q)
    roster = os.path.join(talkc.CORE, "data", "roster")
    for f in sorted(os.listdir(roster)):
        if f.endswith(".txt"):
            for line in open(os.path.join(roster, f), encoding="utf-8"):
                m = re.match(r"^bio:\s*(.*)", line)
                if m:
                    out.append(m.group(1))
    for line in open(os.path.join(talkc.CORE, "data", "furn", "catalog.txt"), encoding="utf-8"):
        m = re.match(r"^\s*desc\s+\"([^\"]*)\"", line)
        if m:
            out.append(m.group(1))
    lang = os.path.join(talkc.CORE, "lang", "es")
    if os.path.isdir(lang):
        for f in sorted(os.listdir(lang)):
            if f.startswith("ui_") and f.endswith(".tsv"):
                for line in open(os.path.join(lang, f), encoding="utf-8"):
                    if line.strip() and not line.startswith("#"):
                        out.append(line.split("\t")[0])
    clean = []
    for s in out:
        s = re.sub(r"\{[^}]*\}", " ", s).replace("/", " ").lower()
        clean.append(s)
    return clean


def talkable(c):
    return bool(c.cat.flags & talkc.CAT_FLAGS["talk"]) and c.age != "never"


def adult(c):
    return c.age == "adult" or bool(c.cat.flags & talkc.CAT_FLAGS["gated"])


def allowed(a, b):
    """may a -> b ship? (the same rule talkc.py checks)"""
    if not talkable(a) or not talkable(b) or a is b:
        return False
    if adult(b) and not adult(a):
        return False
    if adult(a) and b.fl & talkc.TCF["minor"]:
        return False        # adults-only talk never leads to a child word
    return True


def load_hand(path, by_key):
    edges = {}
    if not os.path.exists(path):
        return edges
    for n, line in enumerate(open(path, encoding="utf-8")):
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        k, rest = line.split("\t", 1)
        if k not in by_key:
            raise talkc.Fail("%s:%d: %s is not a concept" % (talkc.rel(path), n + 1, k))
        for o in rest.split():
            if o not in by_key:
                raise talkc.Fail("%s:%d: %s is not a concept" % (talkc.rel(path), n + 1, o))
            edges.setdefault(k, []).append(o)
    return edges


def suggest(b, hand):
    cs = [c for c in b.concepts if talkable(c)]
    # surface -> concept (longest match first; lowercase)
    surf = {}
    for c in cs:
        for s in talkc.surfaces(c):
            s = s.lower()
            if len(s) >= 3 and s not in STOP:
                surf.setdefault(s, c)
    maxw = max(len(s.split()) for s in surf)
    lines = corpus_lines()
    count, pair = {}, {}
    for line in lines:
        words = re.findall(r"[a-z0-9'\-.]+", line)
        words = [w.strip(".'") for w in words]
        found = set()
        i = 0
        while i < len(words):
            hit = None
            for n in range(min(maxw, len(words) - i), 0, -1):
                ph = " ".join(words[i:i + n])
                if ph in surf:
                    hit = (surf[ph], n)
                    break
            if hit:
                found.add(hit[0].key)
                i += hit[1]
            else:
                i += 1
        found = sorted(found)
        for k in found:
            count[k] = count.get(k, 0) + 1
        for x in range(len(found)):
            for y in range(x + 1, len(found)):
                p = (found[x], found[y])
                pair[p] = pair.get(p, 0) + 1
    N = max(1, len(lines))
    score = {}

    def add(a, b2, v, why):
        if a == b2:
            return
        d = score.setdefault(a, {})
        s, r = d.get(b2, (0.0, set()))
        d[b2] = (s + v, r | {why})

    for (x, y), n in pair.items():
        if n < 3:
            continue
        pmi = math.log2(n * N / (count[x] * count[y]))
        if pmi <= 2.0:
            continue
        v = 0.35 * min(1.0, pmi / 8.0)
        add(x, y, v, "pmi")
        add(y, x, v, "pmi")
    by_leaf, by_root, by_word = {}, {}, {}
    for c in cs:
        by_leaf.setdefault(c.cat.key, []).append(c)
        by_root.setdefault(c.cat.root, []).append(c)
        for w in set(re.findall(r"[a-z]{3,}", c.lemma.lower())) - STOP:
            by_word.setdefault(w, []).append(c)
    lemma_of = {c.lemma.lower(): c for c in cs}
    # attributes: only a row's own (a category's defaults say nothing about
    # the row), weighted by rarity (idf), cosine
    own = {c.key: set(c.attrs) for c in cs if set(c.attrs) != set(c.cat.attrs)}
    df = {}
    for k, a in own.items():
        for x in a:
            df.setdefault(x, []).append(k)
    idf = {x: math.log(len(own) / float(len(ks))) for x, ks in df.items()}
    norm = {k: math.sqrt(sum(idf[x] ** 2 for x in a)) or 1.0 for k, a in own.items()}
    for k, a in sorted(own.items()):
        acc = {}
        for x in a:
            if len(df[x]) > 120:
                continue
            for o in df[x]:
                if o != k:
                    acc[o] = acc.get(o, 0.0) + idf[x] ** 2
        for o, v in acc.items():
            cos = v / (norm[k] * norm[o])
            same = b.by_key[o].cat.root == b.by_key[k].cat.root
            if cos >= (0.25 if same else 0.5):          # across roots only when close
                add(k, o, 0.35 * cos * (1.3 if same else 1.0), "attr")
    for c in cs:
        leaf = by_leaf[c.cat.key]
        if len(leaf) <= 60:
            for o in leaf:
                if o is not c:
                    add(c.key, o.key, 0.12, "sib")
        words = set(re.findall(r"[a-z]{3,}", c.lemma.lower())) - STOP
        for w in words:
            if w in lemma_of and lemma_of[w] is not c:
                add(c.key, lemma_of[w].key, 0.5, "word")      # "bone throne" -> bone
                add(lemma_of[w].key, c.key, 0.2, "word")
            peers = by_word.get(w, [])
            if len(peers) <= 40:
                for o in peers:
                    if o is not c:
                        add(c.key, o.key, 0.3, "word")
    for k, outs in hand.items():
        for o in outs:
            add(k, o, 1.0, "hand")
    out = {}
    for c in cs:
        d = score.get(c.key, {})
        cand = [(s, o, r) for o, (s, r) in d.items() if allowed(c, b.by_key[o])]
        # ties (a big leaf's siblings) by a fixed hash, not the alphabet
        cand.sort(key=lambda t: (-round(t[0], 6), zlib.crc32((c.key + ">" + t[1]).encode())))
        out[c.key] = [(o, max(1, min(15, int(round(s * 15)))), r) for s, o, r in cand[:12] if s >= 0.12]
    return out, len(lines)


def write_suggestions(sug, path, nlines):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("# talk_assoc.py suggestions over %d lines of game text: NOT shipped. Review, then\n" % nlines)
        f.write("# copy what you accept into core/data/talk/assoc_<lang>.tsv (or use --accept).\n")
        for k in sorted(sug):
            if sug[k]:
                f.write("%s\t%s\t# %s\n" % (k, " ".join("%s:%d" % (o, w) for o, w, _ in sug[k]),
                                             " ".join("%s=%s" % (o, "+".join(sorted(r))) for o, w, r in sug[k])))


def accept(sugg_path, out_path, top, note):
    rows = []
    for line in open(sugg_path, encoding="utf-8"):
        if line.startswith("#") or not line.strip():
            continue
        cols = line.rstrip("\n").split("\t")
        k, edges = cols[0], cols[1].split()
        reasons = {}
        if len(cols) > 2:
            for t in cols[2].lstrip("# ").split():
                o, r = t.split("=", 1)
                reasons[o] = r
        keep = [e for e in edges[:top]]
        keep += [e for e in edges[top:] if "hand" in reasons.get(e.split(":")[0], "")]
        rows.append((k, keep))
    with open(out_path, "w", encoding="utf-8", newline="\n") as f:
        f.write("# core/data/talk/assoc_en.tsv - the REVIEWED association edges of the Talk word\n")
        f.write("# network (owner: TK; docs/TALK.md). KEY<TAB>OTHER:W ... (W 1..15, strongest first,\n")
        f.write("# at most 16). Proposed by core/tools/talk_assoc.py, accepted here by a reviewer:\n")
        f.write("# talkc.py ships exactly these lines (top 8 per concept in ASSOC, all in EDGES).\n")
        f.write("# %s\n" % note)
        for k, keep in rows:
            if keep:
                f.write("%s\t%s\n" % (k, " ".join(keep)))
    return len(rows), sum(len(k) for _, k in rows)


def main(argv):
    args = argv[1:]
    lang, out, mode, top, src = "en", None, "suggest", 8, None
    note = "Review: TK1 agent pass (top 8 kept, hand edges always), owner pass pending."
    i = 0
    while i < len(args):
        if args[i] == "--lang":
            lang = args[i + 1]; i += 2
        elif args[i] == "--out":
            out = args[i + 1]; i += 2
        elif args[i] == "--accept":
            mode, src = "accept", args[i + 1]; i += 2
        elif args[i] == "--top":
            top = int(args[i + 1]); i += 2
        elif args[i] == "--note":
            note = args[i + 1]; i += 2
        else:
            print(__doc__)
            return 2
    try:
        if mode == "accept":
            out = out or os.path.join(talkc.DATA, "assoc_%s.tsv" % lang)
            n, e = accept(src, out, top, note)
            print("talk_assoc: %s: %d concepts, %d edges accepted" % (talkc.rel(out), n, e))
            return 0
        b = talkc.Build(lang)
        hand = load_hand(os.path.join(talkc.DATA, "assoc_hand_%s.tsv" % lang), b.by_key)
        sug, nlines = suggest(b, hand)
        out = out or os.path.join(talkc.CORE, "build", "talk", "assoc_suggest_%s.tsv" % lang)
        write_suggestions(sug, out, nlines)
        print("talk_assoc: %s: %d concepts with suggestions, %d edges (corpus %d lines)"
              % (talkc.rel(out), sum(1 for v in sug.values() if v), sum(len(v) for v in sug.values()), nlines))
    except talkc.Fail as e:
        print("talk_assoc: " + str(e), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
