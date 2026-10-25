#!/usr/bin/env python3
"""apply_patch.py - apply one of show/engine/patches/*.patch to a COPY.

    apply_patch.py SRC PATCH OUT

SRC is a game file (read, never written), PATCH a unified diff made with
`diff -u`, OUT the patched copy under show/build/<preset>/gen/. The game's
own build never sees OUT (spec 4.3).

Strict on purpose (AT1.4): every hunk's context and removed lines must
match the source exactly (no fuzz). A hunk may move (the game added or
removed lines above it); the nearest exact match to its recorded line wins.
Anything else stops the show build with the file names and the hunk, and
OUT is not written, so a stale copy can never be compiled.
Stdlib only, deterministic.
"""
import os
import re
import sys

HUNK = re.compile(r"^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@")


def parse(patch_text):
    hunks, cur = [], None
    for line in patch_text.splitlines():
        m = HUNK.match(line)
        if m:
            cur = {"old_start": int(m.group(1)), "header": line, "lines": []}
            hunks.append(cur)
        elif cur is not None and line[:1] in (" ", "-", "+"):
            cur["lines"].append(line)
        elif cur is not None and line == "":
            cur["lines"].append(" ")       # an empty context line lost its space
        elif line.startswith("\\"):
            continue                        # "\ No newline at end of file"
    return hunks


def find(src, old, want):
    """index of `old` in `src` nearest to line `want` (0-based), or -1"""
    n = len(old)
    best = -1
    for i in range(0, len(src) - n + 1):
        if src[i:i + n] == old and (best < 0 or abs(i - want) < abs(best - want)):
            best = i
    return best


def main(argv):
    if len(argv) != 4:
        print(__doc__)
        return 2
    src_path, patch_path, out_path = argv[1:]
    src = open(src_path, encoding="latin-1").read().split("\n")
    hunks = parse(open(patch_path, encoding="latin-1").read())
    if not hunks:
        print("show: %s: no hunks" % patch_path, file=sys.stderr)
        return 1
    out, pos, shift = [], 0, 0
    for k, h in enumerate(hunks, 1):
        old = [l[1:] for l in h["lines"] if l[0] in " -"]
        new = [l[1:] for l in h["lines"] if l[0] in " +"]
        want = h["old_start"] - 1 + shift
        at = find(src[pos:], old, want - pos)
        if at < 0:
            print("show: patch %s no longer applies to %s: hunk %d (%s) - its context "
                  "changed in the game. Refresh the patch in show/engine/patches/ "
                  "(the game build is not affected)." % (patch_path, src_path, k, h["header"]),
                  file=sys.stderr)
            return 1
        at += pos
        out += src[pos:at]
        out += new
        pos = at + len(old)
        shift = at - (h["old_start"] - 1)
    out += src[pos:]
    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    tmp = out_path + ".tmp"
    with open(tmp, "w", encoding="latin-1", newline="\n") as f:
        f.write("\n".join(out))
    os.replace(tmp, out_path)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
