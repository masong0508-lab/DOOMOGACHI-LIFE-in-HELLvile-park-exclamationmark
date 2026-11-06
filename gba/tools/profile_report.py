#!/usr/bin/env python3
"""profile_report.py - turn the log of `make -C gba profile-all` into a markdown report.

For every scene of the speed tour (a `harness: fps LABEL: ...` line followed by the
`harness: profile of ...` block the script's `profile N` printed) it prints the scene's
drawn fps / work ms and its hottest functions, scenes ranked by work avg (slowest first),
with the ms a frame has to lose to fit 30 fps (33.3 ms of work, 30 ms to leave some air).

    python3 gba/tools/profile_report.py build/profile_all.log [TOP]
"""
import re
import sys

FPS_RE = re.compile(r'^harness: fps (?P<label>.*?): (?P<logic>[\d.]+) logic ticks/s, '
                    r'(?P<fps>[\d.]+) drawn frames/s, work avg (?P<avg>[\d.]+) ms '
                    r'max (?P<max>[\d.]+) ms')
PROF_RE = re.compile(r'^harness: profile of \d+ instructions, \d+ cycles .*?, ([\d.]+)% outside known symbols')
FUNC_RE = re.compile(r'^harness:\s+([\d.]+)%\s+(\S+)\s*$')
BUDGET_MS = 30.0


def main():
    path = sys.argv[1]
    top = int(sys.argv[2]) if len(sys.argv) > 2 else 8
    scenes = []
    cur = None
    for line in open(path, errors='replace'):
        line = line.rstrip('\n')
        m = FPS_RE.match(line)
        if m:
            cur = dict(m.groupdict(), funcs=[], outside=None)
            cur['avg'] = float(cur['avg'])
            scenes.append(cur)
            continue
        if cur is None:
            continue
        m = PROF_RE.match(line)
        if m:
            cur['outside'] = float(m.group(1))
            continue
        m = FUNC_RE.match(line)
        if m and cur['outside'] is not None:
            cur['funcs'].append((float(m.group(1)), m.group(2)))
    if not scenes:
        print('(no fps lines in %s)' % path)
        return 1
    scenes.sort(key=lambda s: -s['avg'])
    print('| scene | drawn fps | work avg ms | work max ms | to lose for 30 fps (ms) | hottest functions |')
    print('|---|---:|---:|---:|---:|---|')
    for s in scenes:
        lose = max(0.0, s['avg'] - BUDGET_MS)
        hot = ', '.join('%s %.1f%%' % (n, p) for p, n in s['funcs'][:top]) or '(no profile)'
        print('| %s | %s | %.1f | %s | %.1f | %s |' % (s['label'], s['fps'], s['avg'], s['max'], lose, hot))
    return 0


if __name__ == '__main__':
    sys.exit(main())
