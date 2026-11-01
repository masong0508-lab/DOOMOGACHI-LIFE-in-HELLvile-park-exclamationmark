#!/usr/bin/env python3
"""speedtable.py - the GBA speed tour's harness log as a table (owner: gba).

    speedtable.py LOG                       markdown table of every measured scene
    speedtable.py LOG --base BASE           ... with BASE's numbers beside them
    speedtable.py LOG --base BASE --parity  ... and fail unless every scene's
                                            vhash-seq frames equal BASE's
    speedtable.py --compare A B             only the frame parity of A vs B

LOG is the output of `gba/build/harness ROM --script
harness/scripts/speed_tour.txt` (stdout and stderr; `make -C gba test-speed`
writes build/speed_tour.log). It reads the harness lines

    harness: fps LABEL: X logic ticks/s, Y drawn frames/s, work avg A ms max M ms (...)
    harness: vhash-seq LABEL: N frames, ticks T0-T1, seq HASH
    harness: vhash-frames LABEL: TICK:HASH TICK:HASH ...
    harness: SCRIPT:LINE LABEL: Y drawn frames/s, expected >= MIN   (a failed gate)

`fps start` lines (the start of a measurement) are skipped.

Frame parity compares two logs' vhash-frames scene by scene on the logic
ticks both ROMs drew (a faster ROM presents more of them). A ROM that enters
a scene a tick earlier or later (the harness's waits end on a video frame)
shifts the ticks, so the best offset within +-3 ticks is used and reported;
every frame both drew at that offset must be identical.

Standard library only.
"""
import argparse
import re
import sys

FPS_RE = re.compile(r'^harness: fps (?:(?P<label>.*?): )?(?P<logic>[\d.]+) logic ticks/s, '
                    r'(?P<fps>[\d.]+) drawn frames/s, work avg (?P<avg>[\d.]+) ms '
                    r'max (?P<max>[\d.]+) ms')
SEQ_RE = re.compile(r'^harness: vhash-seq (?P<label>.*): (?P<n>\d+) frames, '
                    r'ticks (?P<t0>\d+)-(?P<t1>\d+), seq (?P<seq>[0-9a-f]{8})$')
FRAMES_RE = re.compile(r'^harness: vhash-frames (?P<label>.*): (?P<list>[0-9a-f: ]*)$')
GATE_RE = re.compile(r'^harness: \S+:\d+ (?P<label>.*): (?P<fps>[\d.]+) drawn frames/s, '
                     r'expected >= (?P<min>[\d.]+)$')
MAX_OFFSET = 3


class Scene:
    def __init__(self, label):
        self.label = label
        self.logic = self.fps = self.avg = self.max = None
        self.seq = None
        self.nframes = 0
        self.frames = {}          # tick -> hash
        self.gate = None          # failed gate minimum


def parse(path):
    """scenes in tour order, by label"""
    scenes = {}
    order = []
    failed = []

    def get(label):
        if label not in scenes:
            scenes[label] = Scene(label)
            order.append(label)
        return scenes[label]

    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f:
            line = line.rstrip('\n')
            m = FPS_RE.match(line)
            if m:
                label = m.group('label') or ''
                if label == 'start':
                    continue
                s = get(label)
                s.logic, s.fps = float(m.group('logic')), float(m.group('fps'))
                s.avg, s.max = float(m.group('avg')), float(m.group('max'))
                continue
            m = SEQ_RE.match(line)
            if m:
                s = get(m.group('label'))
                s.seq, s.nframes = m.group('seq'), int(m.group('n'))
                continue
            m = FRAMES_RE.match(line)
            if m:
                s = get(m.group('label'))
                for item in m.group('list').split():
                    tick, h = item.split(':')
                    s.frames[int(tick)] = h
                continue
            m = GATE_RE.match(line)
            if m:
                get(m.group('label')).gate = float(m.group('min'))
                continue
            if line.startswith('harness: ') and ('HANG' in line or 'expected' in line
                                                 or 'failed' in line or 'unknown' in line):
                failed.append(line)
    return [scenes[k] for k in order], failed


def num(v, fmt='%.1f'):
    return '-' if v is None else fmt % v


def table(scenes, base=None):
    by_label = {s.label: s for s in base} if base else {}
    if base:
        head = ('| scene | logic ticks/s | drawn fps | work avg ms | work max ms '
                '| base fps | base avg ms | fps change |')
        rule = '|---|---:|---:|---:|---:|---:|---:|---:|'
    else:
        head = '| scene | logic ticks/s | drawn fps | work avg ms | work max ms | vhash-seq |'
        rule = '|---|---:|---:|---:|---:|---|'
    rows = [head, rule]
    for s in scenes:
        if s.fps is None:
            continue
        fps = num(s.fps)
        if s.gate is not None:
            fps += ' FAIL (>= %g)' % s.gate
        cells = [s.label or '(no label)', num(s.logic), fps, num(s.avg), num(s.max)]
        if base:
            b = by_label.get(s.label)
            if b and b.fps:
                cells += [num(b.fps), num(b.avg), '%+.1f%%' % (100.0 * (s.fps - b.fps) / b.fps)]
            else:
                cells += ['-', '-', '-']
        else:
            cells.append(s.seq or '-')
        rows.append('| ' + ' | '.join(cells) + ' |')
    return '\n'.join(rows)


def match_frames(a, b):
    """(offset, common, mismatched ticks of a): offset 0 when every common
    frame matches there (always, for two `sync on` logs), else the nearest
    offset where every common frame matches, else offset 0's mismatches"""
    results = []
    for d in sorted(range(-MAX_OFFSET, MAX_OFFSET + 1), key=abs):
        common = sorted(t for t in a if t + d in b)
        bad = [t for t in common if a[t] != b[t + d]]
        results.append((d, len(common), bad))
    for d, common, bad in results:
        if common and not bad:
            return d, common, bad
    return results[0]


def parity(new, base, out):
    """print a frame parity report; True when every scene matches"""
    ok = True
    by_label = {s.label: s for s in base}
    for s in new:
        if not s.frames:
            continue
        b = by_label.get(s.label)
        if b is None or not b.frames:
            out.write('parity %-28s no vhash-frames in the base log\n' % s.label)
            ok = False
            continue
        d, common, bad = match_frames(s.frames, b.frames)
        if not common:
            out.write('parity %-28s FAIL: no common ticks within +-%d\n' % (s.label, MAX_OFFSET))
            ok = False
        elif bad:
            out.write('parity %-28s FAIL: %d of %d frames differ (offset %+d, first at tick %d)\n'
                      % (s.label, len(bad), common, d, bad[0]))
            ok = False
        else:
            out.write('parity %-28s ok: %d frames identical%s\n'
                      % (s.label, common, ' (offset %+d ticks)' % d if d else ''))
    missing = [b.label for b in base if b.frames and b.label not in {s.label for s in new if s.frames}]
    for label in missing:
        out.write('parity %-28s FAIL: missing from the new log\n' % label)
        ok = False
    return ok


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('log', nargs='?', help='harness log of the speed tour')
    ap.add_argument('--base', help='a baseline log (another ROM) to show beside it')
    ap.add_argument('--parity', action='store_true',
                    help='with --base: fail unless every vhash-seq frame equals the base')
    ap.add_argument('--compare', nargs=2, metavar=('A', 'B'),
                    help='only the vhash-seq frame parity of log A against log B')
    args = ap.parse_args()

    if args.compare:
        a, _ = parse(args.compare[0])
        b, _ = parse(args.compare[1])
        return 0 if parity(a, b, sys.stdout) else 1
    if not args.log:
        ap.error('a LOG is needed')
    scenes, failed = parse(args.log)
    base = parse(args.base)[0] if args.base else None
    rc = 0
    print(table(scenes, base))
    for line in failed:
        print(line, file=sys.stderr)
        rc = 1
    for s in scenes:
        if s.gate is not None:
            print('speedtable: %s: %.1f drawn fps, the tour gates >= %g (the harness stops at the '
                  'first failed gate)' % (s.label, s.fps, s.gate), file=sys.stderr)
            rc = 1
    if not any(s.fps is not None for s in scenes):
        print('speedtable: no fps lines in %s' % args.log, file=sys.stderr)
        rc = 1
    if args.parity:
        if not base:
            ap.error('--parity needs --base')
        print()
        if not parity(scenes, base, sys.stdout):
            rc = 1
    return rc


if __name__ == '__main__':
    sys.exit(main())
