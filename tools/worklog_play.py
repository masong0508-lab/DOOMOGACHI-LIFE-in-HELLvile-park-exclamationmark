#!/usr/bin/env python3
"""worklog_play.py - the slowest drawn ticks of the PLAY tab window (last 330 ticks of a GBA harness --worklog log),
and whether they repeat: a spike that comes back on a beat is the player (a note starting), one that does not is drawing.

  make -C gba profile-play            writes gba/build/play_idle.log and play_loop.log and runs this
  python3 tools/worklog_play.py play_loop.log [play_idle.log] [N]

Prints the window, the N slowest ticks (default 10), the gaps between them, and, with an idle log, the loop window's extra
median and extra peak over idle (the part of the loop cost that is not the pads). The functions that cost extra in those
ticks come from the second pass of the Makefile target (build/play_loop_spikes.log). Reads only; never gates.
"""
import re, sys, statistics as st

def load(path):
    d = {}
    for line in open(path):
        m = re.match(r"harness: work (\d+) ([\d.]+)", line)
        if m:
            d[int(m.group(1))] = float(m.group(2))
    if not d:
        sys.exit(path + ": no 'harness: work' lines (run the harness with --worklog)")
    last = max(d)
    return {t: v for t, v in d.items() if t > last - 330}

def summary(name, d):
    v = list(d.values())
    return "%s: %d drawn ticks, median %.1f ms, worst %.1f ms, >33.4: %d" % (name, len(v), st.median(v), max(v), sum(x > 33.4 for x in v))

def main():
    args = sys.argv[1:]
    n = 10
    if args and args[-1].isdigit():
        n = int(args.pop())
    if not args:
        sys.exit(__doc__)
    d = load(args[0])
    med = st.median(d.values())
    print(summary("window " + args[0], d))
    worst = sorted(d, key=lambda t: -d[t])[:n]
    print("%-7s %-8s %s" % ("tick", "work ms", "+median"))
    for t in worst:
        print("%-7d %-8.1f %+.1f" % (t, d[t], d[t] - med))
    ts = sorted(worst)
    gaps = [b - a for a, b in zip(ts, ts[1:])]
    print("the %d slowest, in tick order: %s" % (len(ts), " ".join(map(str, ts))))
    print("gaps between them: %s" % (" ".join(map(str, gaps)) if gaps else "-"))
    g = min(gaps) if gaps else 0
    if g >= 3 and all(min(x % g, g - x % g) <= 1 for x in gaps):   # every gap is a whole number of the shortest one
        print("the gaps repeat: the slow ticks come on a beat (a note starting, the player), not from the pads")
    if len(args) > 1:
        i = load(args[1])
        print(summary("idle   " + args[1], i))
        print("loop over idle: median %+.1f ms, worst %+.1f ms (the pads are the same in both)"
              % (med - st.median(i.values()), max(d.values()) - max(i.values())))

main()
