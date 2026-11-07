#!/usr/bin/env python3
"""worklog_spikes.py - the slowest drawn ticks of the LAST 330 ticks of a GBA harness --worklog log, with what the
song is doing there (Music Studio PERFORM, gba/harness/scripts/studio_sung.txt).

  make -C gba profile-sung            writes gba/build/sung.log and runs this
  python3 tools/worklog_spikes.py sung.log [N] [--gate]

Per slow tick: the harness tick, the tick counted from the A press that raised the curtain (show_t; the window
opens 40 ticks after A), the work in ms, how far it is over the window median, and a hint:
  - a gap of about 13 ms over the median COULD be one chibi body drawn from scratch (a cache miss: portrait.c
    bodyspr_image), but it was not the 50.8 ms tick: that one was the first draw of a new line asking
    studio_part_of() for the line's part (about 100 part_mask() calls, fixed in nov07p). The hint cannot tell the
    two apart, so `make -C gba profile-sung` also prints which functions cost extra in the frames over 45 ms
    (build/sung_spikes.log): trust that list, not this hint;
  - ticks next to a line onset (show_t about 161 and 292 in the Park United! choir) are the line change: the new
    line's syllables, the part label, the singer masks and the voice tuning (tune_voice, ~2.5 ms on a memo miss).
Also lists the ticks the GBA did not draw (logic ran, no frame) so a slow tick can be seen to cost two or more.
Reads only; never gates, unless --gate is given: then the exit status is 1 when a tick is over the 50.2 ms line
(the 3-vblank cliff, 16.7 fps for that tick). `make -C gba profile-sung SUNG_GATE=--gate` passes it.
"""
import re, sys, statistics as st

ONSETS = {161: "line 1 starts (13 singers)", 292: "line 2 starts (12 singers)", 30: "curtain up, song starts"}

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

def hint(show_t, over):
    h = []
    for s, name in sorted(ONSETS.items()):
        if abs(show_t - s) <= 4:
            h.append("near " + name)
    if 10.5 <= over <= 16.5:
        h.append("+13 ms: a chibi body render OR a first-seen line label; see sung_spikes.log")
    return "; ".join(h)

def main():
    args = [a for a in sys.argv[1:] if a != "--gate"]
    gate = len(args) != len(sys.argv) - 1
    if not args:
        sys.exit(__doc__)
    d = load(args[0])
    n = int(args[1]) if len(args) > 1 else 12
    ticks = sorted(d)
    med = st.median(d.values())
    first = ticks[0]
    a_press = first - 40
    print("window: ticks %d..%d, %d drawn, median %.1f ms, worst %.1f ms, >50.2: %d, >33.4: %d"
          % (ticks[0], ticks[-1], len(ticks), med, max(d.values()),
             sum(v > 50.2 for v in d.values()), sum(v > 33.4 for v in d.values())))
    print("%-6s %-7s %-8s %-8s %s" % ("tick", "show_t", "work ms", "+median", "hint"))
    for t in sorted(ticks, key=lambda t: -d[t])[:n]:
        print("%-6d %-7d %-8.1f %-8.1f %s" % (t, t - a_press, d[t], d[t] - med, hint(t - a_press, d[t] - med)))
    missing = [t for t in range(ticks[0], ticks[-1] + 1) if t not in d]
    print("ticks logic ran but the GBA did not draw: %d of %d" % (len(missing), ticks[-1] - ticks[0] + 1))
    near = [t for t in ticks if d[t] > 50.2]
    for t in near:
        nxt = t + 1
        print("over the 50.2 ms line: tick %d (%.1f ms)%s" % (t, d[t], "  [next tick not drawn]" if nxt in missing else ""))
    if gate and near:
        sys.exit(1)

main()
