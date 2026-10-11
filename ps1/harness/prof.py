#!/usr/bin/env python3
"""prof.py - per-function table of a harness PC profile (prof/profend).

usage: prof.py doomogachi.map OUT/prof_NAME.txt [--callers] [-n 30]

Samples are "pc ra count" lines (one sample per vsync). Addresses inside
the scene overlay slot are ambiguous (every overlay group links there); the
symbol listed is the nearest one of any group, so read those with the
running scene in mind. Stdlib only.
"""
import bisect
import sys


def load_map(path):
    syms = []
    for line in open(path):
        p = line.split()
        if len(p) >= 3 and p[1] in "TtWw":
            try:
                syms.append((int(p[2], 16), p[0]))
            except ValueError:
                pass
    syms.sort()
    return syms


def name_of(syms, addrs, a):
    i = bisect.bisect_right(addrs, a) - 1
    if i < 0:
        return "?"
    # every symbol at that address (overlay groups share the slot)
    base = syms[i][0]
    if a - base > 0x20000:
        return "?%08x" % a
    names = sorted({n for (x, n) in syms if x == base})
    return "/".join(names[:3])


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("-")]
    n = 30
    if "-n" in sys.argv:
        n = int(sys.argv[sys.argv.index("-n") + 1])
        args = [a for a in args if a != str(n)]
    syms = load_map(args[0])
    addrs = [s[0] for s in syms]
    fn, pair, total = {}, {}, 0
    for line in open(args[1]):
        p = line.split()
        if len(p) != 3:
            continue
        pc, ra, c = int(p[0], 16), int(p[1], 16), int(p[2])
        f = name_of(syms, addrs, pc)
        fn[f] = fn.get(f, 0) + c
        k = (f, name_of(syms, addrs, ra))
        pair[k] = pair.get(k, 0) + c
        total += c
    print("%d samples" % total)
    for f, c in sorted(fn.items(), key=lambda x: -x[1])[:n]:
        print("%6.1f%%  %s" % (100.0 * c / total, f))
    if "--callers" in sys.argv:
        print("-- pc function <- ra function")
        for (f, r), c in sorted(pair.items(), key=lambda x: -x[1])[:n]:
            print("%6.1f%%  %s <- %s" % (100.0 * c / total, f, r))


if __name__ == "__main__":
    main()
