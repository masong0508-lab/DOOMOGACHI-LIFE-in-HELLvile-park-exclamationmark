#!/usr/bin/env python3
"""worklog_median.py - median / worst work per drawn tick of the LAST 330 ticks of a GBA harness --worklog log.

  python3 tools/worklog_median.py sung.log [other.log ...]

Prints, per log: drawn ticks, median, steady median (first quarter dropped: the first onset), worst, ticks over the
3-vblank cliff (50.2 ms) and over the 2-vblank line (33.4 ms). With two or more logs it also prints the mean saving of
each later log against the first, on the ticks both drew (drawn-tick sets differ between builds, so medians alone mislead).
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

logs = [(p, load(p)) for p in sys.argv[1:]]
if not logs:
    sys.exit(__doc__)
for p, d in logs:
    v = list(d.values()); steady = v[len(v) // 4:]
    print("%s: ticks %d  median %.1f  steady %.1f  worst %.1f  >50.2: %d  >33.4: %d"
          % (p, len(v), st.median(v), st.median(steady), max(v),
             sum(x > 50.2 for x in v), sum(x > 33.4 for x in v)))
for p, d in logs[1:]:
    com = sorted(set(d) & set(logs[0][1]))
    if com:
        diff = [logs[0][1][t] - d[t] for t in com]
        print("%s vs %s: %d common ticks, mean saving %.2f ms, median %.2f ms" % (p, logs[0][0], len(com), st.mean(diff), st.median(diff)))
