#!/usr/bin/env python3
"""langstale.py - stale UI translations: rows of core/lang/es*.tsv whose
English key no longer matches any C string literal (P0; the script rewrite
of C-literal text, packages W12-W14, uses it).

The UI tables are keyed by a hash of the exact English (tools/langpack.py),
so rewording a literal in core/src silently drops its Spanish back to
English. This lists those rows: the English of every es*.tsv row is looked
up in the string literals (adjacent literals joined, after the C
preprocessor, with langextract.py's parser) of core/src, core/gen and
core/include. Report only: it always exits 0. A row can also be listed when
its English is built at run time (a formatted string), so read the list as
"check these", and compare the count with the baseline in
docs/STORY_ENGINE.md "Script tools".

    python3 tools/langstale.py [--quiet] [--jobs N]

(It is not a generator; L3's langtool is meant to absorb it.)
"""
import concurrent.futures
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import langextract as lx  # noqa: E402

ROOT = lx.ROOT
SRC_DIRS = ("core/src", "core/gen", "core/include/doomo")


def literals(path):
    src = subprocess.run(["gcc", "-E", "-P", "-I", os.path.join(ROOT, "core", "include"), path],
                         capture_output=True, text=True, errors="replace").stdout
    out = set()
    for m in lx.LIT.finditer(src):
        parts = re.findall(r'"((?:[^"\\\n]|\\.)*)"', m.group(0))
        out.add(lx.esc(b"".join(lx.unesc(p) for p in parts)))
    return out


def main(argv):
    quiet = "--quiet" in argv
    jobs = int(argv[argv.index("--jobs") + 1]) if "--jobs" in argv else 2
    files = []
    for d in SRC_DIRS:
        root = os.path.join(ROOT, d)
        for f in sorted(os.listdir(root)):
            if f.endswith((".c", ".h")):
                files.append(os.path.join(root, f))
    have = set()
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as ex:
        for s in ex.map(literals, files):
            have |= s
    d = os.path.join(ROOT, "core", "lang")
    total = stale = 0
    per = {}
    for fn in sorted(os.listdir(d)):
        if not (fn.startswith("es") and fn.endswith(".tsv")):
            continue
        for n, line in enumerate(open(os.path.join(d, fn), encoding="utf-8").read().split("\n"), 1):
            if not line.strip() or line.startswith("#"):
                continue
            f = line.split("\t")
            if len(f) < 2 or not f[1].strip():
                continue
            total += 1
            if f[0] not in have:
                stale += 1
                per[fn] = per.get(fn, 0) + 1
                if not quiet:
                    print("stale\tcore/lang/%s:%d\t%s" % (fn, n, f[0]))
    print("langstale: %d of %d UI rows match no C string literal (%s)" % (
        stale, total, ", ".join("%s %d" % kv for kv in sorted(per.items())) or "none"))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
