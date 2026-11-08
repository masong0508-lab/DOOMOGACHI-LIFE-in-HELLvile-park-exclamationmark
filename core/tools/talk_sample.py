#!/usr/bin/env python3
"""talk_sample.py - Talk T7: the Q7 human review sheet (owner: TK; docs/TALK.md
"The census", design 3.7 quality check Q7). stdlib only.

Q7 is the release gate no machine can pass: a person reads exchanges and
rates each 1-5 for "sounds like a person in this game". The pass is a mean
of 3.5 or better with 2% or fewer rated 1 ("nonsense / word salad"). A bank
that fails is rewritten, not re-weighted.

  python3 core/tools/talk_sample.py draw [--n 300] [--seed S] [--out FILE]
      Run the census (core/build/talk_census, make -C core talk first) over
      enough contexts and draw N exchanges, spread over the grid, into a
      review sheet (Markdown-friendly TSV): number, what the player did and
      about what, who is in the room, the lines, an empty RATING and NOTE.
      The template references are kept in a last column for the bank's
      writer; the reviewer can ignore it ("blind": no template names, no
      persona banks shown).
  python3 core/tools/talk_sample.py score FILE
      Read a rated sheet: the mean, the share of 1s, PASS or FAIL against the
      bar, and the templates of every line rated 1 or 2 (file:line), so the
      writer knows what to rewrite. Exit 1 on FAIL or when a row is unrated.
  python3 core/tools/talk_sample.py selftest
      Score a built-in sheet both ways (the unit check of this tool).
"""
import os
import random
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
CORE = os.path.join(ROOT, "core")
CENSUS = os.path.join(CORE, "build", "talk_census")
GRID = os.path.join(CORE, "data", "talk", "census_en.tsv")
HEADER = ["N", "PLAYER DID", "ABOUT", "IN THE ROOM", "LINES", "RATING (1-5)", "NOTE", "TEMPLATES"]
MEAN_BAR = 3.5
ONES_BAR = 0.02


def alt_locations():
    """alternative id -> 'file:line' of the English banks (talkc_gen)"""
    sys.path.insert(0, HERE)
    import talkc
    import talkc_gen as tg
    b = talkc.Build("en")
    _, _, info = tg.build_grammar(b, tg.grammar_files("en"), talkc.die, talkc.rel)
    return info["alt_where"]


def draw(n, seed, out):
    if not os.path.exists(CENSUS):
        sys.exit("talk_sample: %s is missing (make -C core build/talk_census talk)" % os.path.relpath(CENSUS, ROOT))
    every = 37                                     # a prime: spread over the grid's draws
    contexts = n * every * 2
    with tempfile.TemporaryDirectory() as d:
        raw = os.path.join(d, "sample.tsv")
        subprocess.run([CENSUS, "--grid", GRID, "--contexts", str(contexts), "--seed", str(seed), "--sample", raw,
                        "--every", str(every), "--quiet"], check=True, stdout=subprocess.DEVNULL)
        rows = [line.rstrip("\n").split("\t") for line in open(raw, encoding="latin-1")]
    rnd = random.Random(seed)
    rows = rnd.sample(rows, min(n, len(rows)))
    rows.sort(key=lambda r: int(r[0]))
    where = alt_locations()
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        f.write("# Talk Q7 review sheet (core/tools/talk_sample.py, seed %d). Rate every exchange 1-5:\n" % seed)
        f.write("# 5 = sounds like a person in this game, 1 = nonsense / word salad. Fill RATING, NOTE optional.\n")
        f.write("# Pass: mean %.1f or better and %d%% or fewer rated 1. Then: talk_sample.py score THIS_FILE\n"
                % (MEAN_BAR, int(ONES_BAR * 100)))
        f.write("\t".join(HEADER) + "\n")
        for k, r in enumerate(rows, 1):
            lines = [c for c in r[4:] if not c.startswith("#alts")]
            alts = [c for c in r[4:] if c.startswith("#alts")]
            refs = []
            for a in (alts[0].split()[1:] if alts else []):
                a = int(a)
                refs.append(where[a] if 0 <= a < len(where) else "safe line")
            f.write("\t".join([str(k), r[1], r[2], r[3], " / ".join(lines), "", "", "; ".join(refs)]) + "\n")
    print("talk_sample: %d exchanges in %s" % (len(rows), out))


def score_rows(rows):
    """[(rating, templates)] -> (mean, ones share, unrated, low [(rating, templates, lines)])"""
    rated = [r for r in rows if r[0] is not None]
    unrated = len(rows) - len(rated)
    if not rated:
        return 0.0, 1.0, unrated, []
    mean = sum(r[0] for r in rated) / len(rated)
    ones = sum(1 for r in rated if r[0] == 1) / len(rated)
    low = [r for r in rated if r[0] <= 2]
    return mean, ones, unrated, low


def read_sheet(path):
    rows = []
    for line in open(path, encoding="utf-8"):
        if line.startswith("#") or line.startswith("N\t") or not line.strip():
            continue
        c = line.rstrip("\n").split("\t")
        while len(c) < len(HEADER):
            c.append("")
        v = c[5].strip()
        if v and not (v.isdigit() and 1 <= int(v) <= 5):
            sys.exit("talk_sample: row %s: RATING %r is not 1-5" % (c[0], v))
        rows.append((int(v) if v else None, c[7], c[4]))
    return rows


def score(path):
    rows = read_sheet(path)
    mean, ones, unrated, low = score_rows(rows)
    ok = unrated == 0 and mean >= MEAN_BAR and ones <= ONES_BAR
    print("talk_sample: %d exchanges, %d unrated; mean %.2f (bar %.1f), rated 1: %.1f%% (bar %d%%): %s"
          % (len(rows), unrated, mean, MEAN_BAR, 100 * ones, int(ONES_BAR * 100), "PASS" if ok else "FAIL"))
    for r, refs, lines in low:
        print("  %d  %s\n      %s" % (r, lines, refs))
    return 0 if ok else 1


def selftest():
    good = [(4, "a", "x"), (5, "b", "y"), (3, "c", "z"), (4, "d", "w")]
    bad = [(1, "a", "x"), (2, "b", "y"), (5, "c", "z"), (4, "d", "w")]
    m, o, u, low = score_rows(good)
    if not (abs(m - 4.0) < 1e-9 and o == 0 and u == 0 and not low):
        sys.exit("talk_sample: selftest good")
    m, o, u, low = score_rows(bad)
    if not (m == 3.0 and o == 0.25 and len(low) == 2):
        sys.exit("talk_sample: selftest bad")
    print("talk_sample: selftest OK")
    return 0


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    cmd, args = argv[1], argv[2:]
    if cmd == "draw":
        n, seed, out = 300, 1, "talk_review.tsv"
        i = 0
        while i < len(args):
            if args[i] == "--n":
                n = int(args[i + 1])
            elif args[i] == "--seed":
                seed = int(args[i + 1])
            elif args[i] == "--out":
                out = args[i + 1]
            else:
                print(__doc__)
                return 2
            i += 2
        draw(n, seed, out)
        return 0
    if cmd == "score" and len(args) == 1:
        return score(args[0])
    if cmd == "selftest":
        return selftest()
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
