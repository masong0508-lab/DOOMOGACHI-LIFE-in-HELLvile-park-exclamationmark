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
    harness: vhash-seq LABEL: N of W frames, ticks T0-T1, seq HASH
    harness: vhash-frames LABEL: TICK:HASH TICK:HASH ...
    harness: SCRIPT:LINE LABEL: Y drawn frames/s, expected >= MIN   (a failed gate)
    harness: SCRIPT:LINE LABEL: Y drawn frames/s, under the floor MIN (gates off)

`fps start` lines (the start of a measurement) are skipped.

Frame parity is exact. The tour runs `sync on` and every vhash-seq draws
each tick of its window (mailbox draw_all), so two ROMs that draw the same
pictures hash the same ticks T0+1 .. T0+W of every scene, one frame each,
with identical hashes, however fast they run. A scene fails when either log
is incomplete (fewer than W frames, a gap, a vhash-seq error), when its
ticks differ from the base's (a ROM that enters a scene late or early, or a
log of another tour script; a shift under which the frames would match is
named as a hint), or when any frame differs.

Standard library only.
"""
import argparse
import re
import sys

FPS_RE = re.compile(r'^harness: fps (?:(?P<label>.*?): )?(?P<logic>[\d.]+) logic ticks/s, '
                    r'(?P<fps>[\d.]+) drawn frames/s, work avg (?P<avg>[\d.]+) ms '
                    r'max (?P<max>[\d.]+) ms')
SEQ_RE = re.compile(r'^harness: vhash-seq (?P<label>.*): (?P<n>\d+)(?: of (?P<want>\d+))? '
                    r'frames, ticks (?P<t0>\d+)-(?P<t1>\d+), seq (?P<seq>[0-9a-f]{8})$')
FRAMES_RE = re.compile(r'^harness: vhash-frames (?P<label>.*): (?P<list>[0-9a-f: ]*)$')
GATE_RE = re.compile(r'^harness: \S+:\d+ (?P<label>.*): (?P<fps>[\d.]+) drawn frames/s, '
                     r'expected >= (?P<min>[\d.]+)$')
UNDER_RE = re.compile(r'^harness: \S+:\d+ (?P<label>.*): (?P<fps>[\d.]+) drawn frames/s, '
                      r'under the floor (?P<min>[\d.]+) \(gates off\)$')
# harness lines that mean the run failed (gates aside)
FAIL_WORDS = ('HANG', 'expected', 'failed', 'unknown', 'no frame presented',
              'no draw_all', 'no tick hold', 'already at tick', 'must be >')
HINT_OFFSET = 3       # how far a tick shift is looked for (a hint only)


class Scene:
    def __init__(self, label):
        self.label = label
        self.logic = self.fps = self.avg = self.max = None
        self.seq = None
        self.nframes = 0
        self.want = None          # the vhash-seq's N (None: an older log)
        self.t0 = self.t1 = None
        self.frames = {}          # tick -> hash
        self.dup = False          # a tick listed twice
        self.gate = None          # failed gate minimum
        self.under = None         # floor missed with --no-fps-gates


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
                s.want = int(m.group('want')) if m.group('want') else None
                s.t0, s.t1 = int(m.group('t0')), int(m.group('t1'))
                continue
            m = FRAMES_RE.match(line)
            if m:
                s = get(m.group('label'))
                for item in m.group('list').split():
                    tick, h = item.split(':')
                    if int(tick) in s.frames:
                        s.dup = True
                    s.frames[int(tick)] = h
                continue
            m = GATE_RE.match(line)
            if m:
                get(m.group('label')).gate = float(m.group('min'))
                continue
            m = UNDER_RE.match(line)
            if m:
                get(m.group('label')).under = float(m.group('min'))
                continue
            if line.startswith('harness: ') and any(w in line for w in FAIL_WORDS):
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
        elif s.under is not None:
            fps += ' (under %g)' % s.under
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


def incomplete(s):
    """why scene s's vhash-seq does not cover its window (one frame from
    each of its W ticks), or None"""
    ticks = sorted(s.frames)
    if s.seq is None:
        return 'vhash-frames without a vhash-seq line'
    if s.want is None:
        return 'no frame count (a log from before the exact parity check)'
    if s.dup:
        return 'a tick listed twice'
    if s.nframes != s.want or len(ticks) != s.want:
        return '%d of %d frames' % (len(ticks), s.want)
    if ticks and (ticks[0] != s.t0 or ticks[-1] != s.t1 or ticks[-1] - ticks[0] + 1 != len(ticks)):
        return 'ticks %d-%d with gaps' % (ticks[0], ticks[-1])
    return None


def shift_hint(a, b):
    """a tick shift d (0 < |d| <= HINT_OFFSET) under which every frame of a
    equals b's frame d ticks later, or None"""
    for d in sorted(range(-HINT_OFFSET, HINT_OFFSET + 1), key=abs):
        if d and all(t + d in b and a[t] == b[t + d] for t in a):
            return d
    return None


def tick_range(frames):
    ticks = sorted(frames)
    return '%d-%d' % (ticks[0], ticks[-1]) if ticks else 'none'


def parity(new, base, out):
    """print a frame parity report; True when every hashed scene of either
    log is complete and identical in the other: the same ticks, one frame
    each, the same hashes"""
    ok = True
    by_label = {s.label: s for s in base}
    hashed = [s for s in new if s.seq is not None or s.frames]
    for s in hashed:
        b = by_label.get(s.label)
        if b is None or (b.seq is None and not b.frames):
            out.write('parity %-28s FAIL: no vhash-seq in the base log\n' % s.label)
            ok = False
            continue
        why, bwhy = incomplete(s), incomplete(b)
        if why or bwhy:
            out.write('parity %-28s FAIL: %s\n' % (s.label, '; '.join(
                w for w in (why and 'new log: ' + why, bwhy and 'base log: ' + bwhy) if w)))
            ok = False
            continue
        if set(s.frames) != set(b.frames):
            d = shift_hint(s.frames, b.frames)
            out.write('parity %-28s FAIL: ticks %s, base ticks %s%s\n'
                      % (s.label, tick_range(s.frames), tick_range(b.frames),
                         ' (each frame equals the base frame of tick %+d: the scene starts at '
                         'another tick, or the logs come from different tour scripts)' % d
                         if d else ''))
            ok = False
            continue
        bad = [t for t in sorted(s.frames) if s.frames[t] != b.frames[t]]
        if bad:
            out.write('parity %-28s FAIL: %d of %d frames differ (first at tick %d)\n'
                      % (s.label, len(bad), len(s.frames), bad[0]))
            ok = False
        else:
            out.write('parity %-28s ok: %d frames identical (ticks %s)\n'
                      % (s.label, len(s.frames), tick_range(s.frames)))
    have = {s.label for s in hashed}
    for b in base:
        if (b.seq is not None or b.frames) and b.label not in have:
            out.write('parity %-28s FAIL: missing from the new log\n' % b.label)
            ok = False
    if not hashed and not any(b.seq is not None or b.frames for b in base):
        out.write('parity: no vhash-seq in either log\n')
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
        a, afail = parse(args.compare[0])
        b, bfail = parse(args.compare[1])
        rc = 0 if parity(a, b, sys.stdout) else 1
        for path, failed in ((args.compare[0], afail), (args.compare[1], bfail)):
            for line in failed:
                print('%s: %s' % (path, line), file=sys.stderr)
                rc = 1
        return rc
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
        for line in parse(args.base)[1]:
            print('%s: %s' % (args.base, line), file=sys.stderr)
            rc = 1
    return rc


if __name__ == '__main__':
    sys.exit(main())
