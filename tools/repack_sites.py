#!/usr/bin/env python3
"""repack_sites.py - find and convert the DoomoResident member uses that move into the gene
(customisation v2, P0.M0a "accessors first"; docs/design/customization/P0_M0A_STATUS.md).

It does not guess from text. It builds a probe copy of core/include in which the members of
DoomoResident that the gene absorbs (look, voice, persona, pronoun, bday_*, like_*, hate_food, fear,
catchphrase) are renamed, runs `gcc -fsyntax-only` on every C file, and reads the compiler's
"'DoomoResident' has no member named 'x'" errors. Every error is a real use of a resident, so a
DoomoLook, a DoomoCitizen or another struct with a member of the same name is never touched.

  repack_sites.py report  [ROOT]   count the remaining uses per file (the to-do list)
  repack_sites.py convert [ROOT]   rewrite what is mechanical, repeat until nothing more converts,
                                   then print the sites that need a hand (--list shows them all)
  repack_sites.py list    [ROOT]   print every remaining site (file:line, kind, text)

Options: --only SUBSTR (limit to paths containing it), --jobs N, --list (with convert).

Rewrites (E is the resident expression, & added for `.`):
  E->look.F            RES_LOOK(E, GF_F)            E->look.species   RES_SPECIES(E)
  E->voice[i]          RES_VOICE(E, i)              E->pronoun        RES_PRONOUN(E)   (and the rest)
  E->look.F = v;       RES_SET_LOOK(E, GF_F, v)     likewise with op= : RES_SET_X(E, RES_X(E) op (v))
Left for a hand: ++ / --, &E->member, whole-look uses (E->look passed or copied), offsetof, sizeof,
designated initialisers, a call result used as a struct, an `op=` whose resident expression has a call
in it, and an assignment whose end is not a plain `;` `,` or `)`.

Files that are the v1 <-> gene bridge keep touching the members on purpose and are skipped (EXCLUDE).
A C file that cannot be compiled on this host (a cross-toolchain header is missing) is listed as
"unprobed"; the text scan then shows how many member-looking uses it hides.
Python 3 standard library only. Idempotent: a converted site no longer errors in the probe.
"""
import argparse
import collections
import concurrent.futures
import os
import re
import shutil
import subprocess
import sys
import tempfile

IDENT = ["pronoun", "bday_month", "bday_day", "like_food", "like_hobby", "like_music",
         "like_color", "hate_food", "fear", "catchphrase"]
MEMBERS = ["look", "voice", "persona"] + IDENT
# The v1 bridge: these read and write the members by design (gene_pack_v1 / gene_unpack_v1 and their tests).
EXCLUDE = ("core/src/gene.c", "core/include/doomo/gene.h", "core/tests/test_gene.c")
SKIP_DIRS = {".git", "build", "out", "node_modules", "__pycache__"}
KEYWORDS = {"return", "case", "else", "sizeof", "if", "while", "for", "do", "switch", "goto",
            "typeof", "defined"}
PROBE_PREFIX = "x_"

ERR = re.compile(r"^(?P<file>.+?):(?P<line>\d+):(?P<col>\d+): error: '[^']*DoomoResident[^']*'"
                 r"(?: \{aka [^}]*\})? has no member named '(?P<mem>\w+)'")
FATAL = re.compile(r"fatal error|cannot find|No such file")


# ---------------------------------------------------------------- source scanning helpers
def is_id(c):
    return c.isalnum() or c == "_"


def match_back(s, i):
    """s[i] is ')' or ']'; return the index of its opening bracket, or None."""
    close = s[i]
    opener = "(" if close == ")" else "["
    depth = 0
    while i >= 0:
        c = s[i]
        if c == close:
            depth += 1
        elif c == opener:
            depth -= 1
            if depth == 0:
                return i
        i -= 1
    return None


def match_fwd(s, i):
    """s[i] is '(' or '['; return the index just after its closing bracket, or None."""
    n = len(s)
    depth = 0
    while i < n:
        c = s[i]
        if c in "\"'":
            i = skip_str(s, i)
            continue
        if c in "([":
            depth += 1
        elif c in ")]":
            depth -= 1
            if depth == 0:
                return i + 1
        i += 1
    return None


def skip_str(s, i):
    q = s[i]
    i += 1
    n = len(s)
    while i < n and s[i] != q:
        i += 2 if s[i] == "\\" else 1
    return i + 1


def skip_ws(s, i):
    n = len(s)
    while i < n:
        if s[i] in " \t\r\n":
            i += 1
        elif s.startswith("\\\n", i):
            i += 2
        elif s.startswith("/*", i):
            j = s.find("*/", i + 2)
            i = n if j < 0 else j + 2
        elif s.startswith("//", i):
            j = s.find("\n", i)
            i = n if j < 0 else j
        else:
            break
    return i


def expr_start(s, end):
    """Start index of the postfix expression that ends at s[end-1] (the resident expression E)."""
    i = end
    while True:
        c = s[i - 1] if i > 0 else ""
        if c in (")", "]"):
            o = match_back(s, i - 1)
            if o is None:
                return None
            i = o
            p = s[i - 1] if i > 0 else ""
            if s[o] == "[" or is_id(p) or p in (")", "]"):
                continue
        elif c and is_id(c):
            j = i
            while j > 0 and is_id(s[j - 1]):
                j -= 1
            if s[j:i] in KEYWORDS:
                break
            i = j
        else:
            break
        k = i
        while k > 0 and s[k - 1] in " \t\r\n":
            k -= 1
        if s[k - 2:k] == "->" or (s[k - 1:k] == "." and s[k - 2:k] != ".."):
            i = k - (2 if s[k - 2:k] == "->" else 1)
            while i > 0 and s[i - 1] in " \t\r\n":
                i -= 1
            continue
        break
    return i


def rhs_end(s, i):
    """End (exclusive) of an assignment's right side starting at i, or None if it is not a plain one."""
    n = len(s)
    depth = 0
    pend = 0
    while i < n:
        c = s[i]
        if c in "\"'":
            i = skip_str(s, i)
            continue
        if s.startswith("//", i):
            j = s.find("\n", i)
            i = n if j < 0 else j
            continue
        if s.startswith("/*", i):
            j = s.find("*/", i + 2)
            i = n if j < 0 else j + 2
            continue
        if c in "([{":
            depth += 1
        elif c in ")]}":
            if depth == 0:
                return i
            depth -= 1
        elif depth == 0:
            if c in ";,":
                return i
            if c == "?":
                pend += 1
            elif c == ":":
                if pend == 0:
                    return None
                pend -= 1
        i += 1
    return None


def side_effect_free(e):
    """True when evaluating E twice is harmless: names, ->, ., [simple], no call, ++, --, =."""
    if re.search(r"\+\+|--|[^=!<>]=[^=]|\w\s*\(", e):
        return False
    return True


# ---------------------------------------------------------------- the probe
def make_probe(root, tmp):
    src = os.path.join(root, "core", "include")
    dst = os.path.join(tmp, "include")
    shutil.copytree(src, dst)
    # lang.h / music.h include "../../gen/*_ids.h" (generated by `make -C core lib`)
    os.symlink(os.path.join(root, "core", "gen"), os.path.join(tmp, "gen"))
    p = os.path.join(dst, "doomo", "resident.h")
    t = open(p).read()
    a = t.index("typedef struct DoomoResident {")
    b = t.index("} DoomoResident;", a)
    body = t[a:b]
    for m in MEMBERS:
        body = re.sub(r"(\s)%s(\s*[\[;])" % m, r"\1%s%s\2" % (PROBE_PREFIX, m), body)
    open(p, "w").write(t[:a] + body + t[b:])
    p = os.path.join(dst, "doomo", "resident_acc.h")
    if os.path.exists(p):
        t = open(p).read()
        t = re.sub(r"(->|\.)(%s)\b" % "|".join(MEMBERS), r"\1%s\2" % PROBE_PREFIX, t)
        open(p, "w").write(t)
    return dst


def c_files(root, only):
    out = []
    for d, dirs, fs in os.walk(root):
        dirs[:] = [x for x in dirs if x not in SKIP_DIRS]
        for f in fs:
            if f.endswith(".c"):
                p = os.path.join(d, f)
                rel = os.path.relpath(p, root)
                if rel in EXCLUDE or (only and only not in rel):
                    continue
                out.append(p)
    return sorted(out)


def flags_for(root, probe, path):
    rel = os.path.relpath(path, root)
    f = ["gcc", "-fsyntax-only", "-w", "-fdiagnostics-column-unit=byte", "-fdiagnostics-column-origin=1",
         "-fno-diagnostics-show-caret", "-fno-diagnostics-color", "-I", probe]
    if rel.startswith(("core/src/", "core/gen/")):
        f += ["-std=c99"]
    else:
        f += ["-std=gnu11", "-D_POSIX_C_SOURCE=200809L", "-I", os.path.join(root, "core", "native"),
              "-I", os.path.join(root, "core", "tests"), "-I", os.path.dirname(path),
              "-I", os.path.join(root, "trailer"), "-I", os.path.join(root, "pilot_proof")]
    return f + [path]


def probe_one(args):
    root, probe, path = args
    env = dict(os.environ, LC_ALL="C", LANGUAGE="C")   # ASCII quotes in gcc's messages
    r = subprocess.run(flags_for(root, probe, path), capture_output=True, text=True, env=env)
    sites = set()
    fatal = False
    for line in r.stderr.splitlines():
        m = ERR.match(line)
        if m:
            sites.add((m.group("file"), int(m.group("line")), int(m.group("col")), m.group("mem")))
        elif FATAL.search(line) and "fatal error" in line:
            fatal = True
    return path, sites, fatal


def probe_all(root, only, jobs):
    tmp = tempfile.mkdtemp(prefix="repack_")
    try:
        probe = make_probe(root, tmp)
        files = c_files(root, only)
        sites, unprobed = set(), []
        with concurrent.futures.ThreadPoolExecutor(jobs) as ex:
            for path, s, fatal in ex.map(probe_one, [(root, probe, f) for f in files]):
                sites |= s
                if fatal:
                    unprobed.append(path)
        # keep only sites inside the tree (a system header cannot be one) and not the excluded bridge
        keep = set()
        for f, ln, col, mem in sites:
            f = os.path.realpath(f)
            if not f.startswith(os.path.realpath(root) + os.sep):
                continue
            if os.path.relpath(f, os.path.realpath(root)) in EXCLUDE:
                continue
            keep.add((f, ln, col, mem))
        return sorted(keep), unprobed
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


# ---------------------------------------------------------------- one site -> an edit
class Hand(Exception):
    pass


def gf(field):
    return "GF_" + field.upper()


def plan(s, off, mem):
    """(start, end, replacement) for the member at s[off:off+len(mem)], or raise Hand(reason)."""
    n = len(mem)
    if s[off:off + n] != mem:
        raise Hand("position mismatch")
    k = off
    while k > 0 and s[k - 1] in " \t\r\n":
        k -= 1
    if s[k - 2:k] == "->":
        opn, op_at = "->", k - 2
    elif s[k - 1:k] == ".":
        opn, op_at = ".", k - 1
    else:
        raise Hand("no . or -> before the member (offsetof or sizeof?)")
    e_end = op_at
    while e_end > 0 and s[e_end - 1] in " \t\r\n":
        e_end -= 1
    e0 = expr_start(s, e_end)
    if e0 is None or e0 == e_end:
        raise Hand("designated initialiser or no resident expression")
    E = s[e0:e_end]
    if opn == ".":
        if re.match(r"^[A-Za-z_]\w*\s*\(", E) and E.endswith(")"):
            raise Hand("member of a returned struct")
        m = re.fullmatch(r"\(\s*\*(.+)\)", E, re.S)
        ptr = m.group(1).strip() if m and match_back(E, len(E) - 1) == 0 else "&" + E
    else:
        ptr = E
    # what comes after the member
    j = off + n
    end = j
    if mem == "look":
        j2 = skip_ws(s, j)
        if s[j2] != ".":
            raise Hand("whole look used (copy, pass or compare)")
        j2 = skip_ws(s, j2 + 1)
        m = re.match(r"\w+", s[j2:])
        if not m:
            raise Hand("odd look access")
        field = m.group(0)
        end = j2 + len(field)
        if field == "species":
            getter, setter, args = "RES_SPECIES(%s)" % ptr, "RES_SET_SPECIES(%s, " % ptr, None
            rd = lambda: getter
        else:
            rd = lambda: "RES_LOOK(%s, %s)" % (ptr, gf(field))
            setter = "RES_SET_LOOK(%s, %s, " % (ptr, gf(field))
    elif mem in ("voice", "persona"):
        j2 = skip_ws(s, j)
        if s[j2] != "[":
            raise Hand("whole %s array used" % mem)
        close = match_fwd(s, j2)
        if close is None:
            raise Hand("odd index")
        idx = s[j2 + 1:close - 1]
        end = close
        name = mem.upper()
        rd = lambda: "RES_%s(%s, %s)" % (name, ptr, idx)
        setter = "RES_SET_%s(%s, %s, " % (name, ptr, idx)
    else:
        name = mem.upper()
        rd = lambda: "RES_%s(%s)" % (name, ptr)
        setter = "RES_SET_%s(%s, " % (name, ptr)
    # what is around the site
    a = skip_ws(s, end)
    b = e0
    while b > 0 and s[b - 1] in " \t\r\n":
        b -= 1
    if s[b - 2:b] in ("++", "--") or s[a:a + 2] in ("++", "--"):
        raise Hand("++ or -- (use RES_SET_x)")
    if s[b - 1:b] == "&":
        c0 = b - 1
        while c0 > 0 and s[c0 - 1] in " \t\r\n":
            c0 -= 1
        if not (c0 > 0 and (is_id(s[c0 - 1]) or s[c0 - 1] in ")]")) and s[c0 - 1:c0] != "&":
            raise Hand("address of the member")
    # assignment?
    m = re.match(r"(<<|>>|[-+*/%&|^])?=(?!=)", s[a:a + 3])
    if m and not s.startswith("==", a):
        op = m.group(1)
        r0 = a + m.end()
        r1 = rhs_end(s, r0)
        if r1 is None:
            raise Hand("assignment that does not end in ; , or )")
        rhs = s[r0:r1].strip()
        if not rhs:
            raise Hand("empty right side")
        if op is None:
            return e0, r1, setter + rhs + ")"
        if not side_effect_free(ptr):
            raise Hand("%s= on an expression with a call in it" % op)
        return e0, r1, setter + "%s %s (%s))" % (rd(), op, rhs)
    return e0, end, rd()


def edits_for_file(path, sites):
    s = open(path, errors="surrogateescape").read()
    starts = [0]
    for ln in s.split("\n"):
        starts.append(starts[-1] + len(ln) + 1)
    raw = open(path, "rb").read()
    # columns are bytes; map to str offsets line by line
    lines = s.split("\n")
    edits, hand = [], []
    for f, ln, col, mem in sites:
        text = lines[ln - 1]
        bcol = len(text.encode("utf-8", "surrogateescape")[:col - 1].decode("utf-8", "surrogateescape"))
        off = starts[ln - 1] + bcol
        try:
            edits.append(plan(s, off, mem) + (ln, mem))
        except Hand as h:
            hand.append((ln, mem, str(h), text.strip()[:110]))
    return s, edits, hand


def apply(path, s, edits):
    edits.sort()
    out, pos, done = [], 0, 0
    for st, en, rep, _, _ in edits:
        if st < pos:
            continue            # nested in an earlier edit: the next pass converts it
        out.append(s[pos:st])
        out.append(rep)
        pos = en
        done += 1
    out.append(s[pos:])
    open(path, "w", errors="surrogateescape").write("".join(out))
    return done


# ---------------------------------------------------------------- commands
def text_hits(root, only):
    rx = re.compile(r"(->|\.)(%s)\b" % "|".join(MEMBERS))
    hits = collections.Counter()
    for d, dirs, fs in os.walk(root):
        dirs[:] = [x for x in dirs if x not in SKIP_DIRS]
        for f in fs:
            if f.endswith((".c", ".h")):
                p = os.path.join(d, f)
                rel = os.path.relpath(p, root)
                if rel in EXCLUDE or (only and only not in rel):
                    continue
                hits[rel] = len(rx.findall(open(p, errors="ignore").read()))
    return hits


def summarize(root, sites, unprobed, only, show_all):
    by = collections.defaultdict(collections.Counter)
    for f, ln, col, mem in sites:
        by[os.path.relpath(f, os.path.realpath(root))]["look.species" if False else
                                                       ("look" if mem == "look" else
                                                        "voice/persona" if mem in ("voice", "persona") else "ident")] += 1
    total = sum(sum(c.values()) for c in by.values())
    print("%d compiler-confirmed resident member uses in %d files" % (total, len(by)))
    for p, c in sorted(by.items(), key=lambda x: -sum(x[1].values())):
        print("%5d  %-48s %s" % (sum(c.values()), p, dict(c)))
    if unprobed:
        print("\nunprobed (cross-toolchain headers missing here), text hits only:")
        hits = text_hits(root, only)
        for p in unprobed:
            rel = os.path.relpath(p, root)
            print("  %-48s %d member-looking uses" % (rel, hits.get(rel, 0)))


def cmd_report(root, only, jobs, show_all=False):
    sites, unprobed = probe_all(root, only, jobs)
    summarize(root, sites, unprobed, only, show_all)
    return sites


def group(sites):
    g = collections.defaultdict(list)
    for s in sites:
        g[s[0]].append(s)
    return g


def cmd_convert(root, only, jobs, list_all):
    for rnd in range(1, 16):
        sites, unprobed = probe_all(root, only, jobs)
        changed = 0
        for path, ss in group(sites).items():
            s, edits, hand = edits_for_file(path, ss)
            if edits:
                changed += apply(path, s, edits)
        print("pass %d: %d sites seen, %d rewritten" % (rnd, len(sites), changed), flush=True)
        if not changed:
            break
    sites, unprobed = probe_all(root, only, jobs)
    print()
    cmd_list(root, sites, unprobed, only, jobs, list_all)


def cmd_list(root, sites, unprobed, only, jobs, list_all=True):
    rest = collections.defaultdict(list)
    for path, ss in group(sites).items():
        s, edits, hand = edits_for_file(path, ss)
        for st, en, rep, ln, mem in edits:
            rest[path].append((ln, mem, "convertible (nested; run convert again)", ""))
        for h in hand:
            rest[path].append(h)
    n = sum(len(v) for v in rest.values())
    print("%d sites left, %d files" % (n, len(rest)))
    for path, v in sorted(rest.items()):
        rel = os.path.relpath(path, os.path.realpath(root))
        print("%s  (%d)" % (rel, len(v)))
        if list_all or len(v) <= 6:
            for ln, mem, why, text in sorted(v):
                print("   %5d  %-8s %-44s %s" % (ln, mem, why, text))
    if unprobed:
        hits = text_hits(root, only)
        print("\nunprobed files (text hits, not compiler-confirmed):")
        for p in unprobed:
            rel = os.path.relpath(p, root)
            print("  %-48s %d" % (rel, hits.get(rel, 0)))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("cmd", choices=["report", "convert", "list"])
    ap.add_argument("root", nargs="?", default=None)
    ap.add_argument("--only", default="")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--list", action="store_true", help="convert: print every site left, not just the short files")
    a = ap.parse_args()
    root = os.path.realpath(a.root or os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
    if a.cmd == "report":
        cmd_report(root, a.only, a.jobs)
    elif a.cmd == "list":
        sites, unprobed = probe_all(root, a.only, a.jobs)
        cmd_list(root, sites, unprobed, a.only, a.jobs, True)
    else:
        cmd_convert(root, a.only, a.jobs, a.list)


if __name__ == "__main__":
    sys.exit(main())
