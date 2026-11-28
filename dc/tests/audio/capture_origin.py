#!/usr/bin/env python3
"""Remove only pre-script capture, using dcrun's sample-count marker.

The raw WAV is retained for boot and audio debugging. Silence inside gameplay
(including genuine musical rests) is untouched. No waveform-based trimming.
"""
import argparse
import re
import wave
from pathlib import Path

MARKER = re.compile(r'^dcrun: script audio start frame (\d+) \(tick (\d+)\)$', re.M)


def align_capture(log, source, dest):
    marks = MARKER.findall(Path(log).read_text(encoding='utf-8', errors='replace'))
    if len(marks) != 1 or int(marks[0][1]) != 0:
        raise ValueError('expected exactly one tick-zero script audio marker')
    start = int(marks[0][0])
    with wave.open(str(source), 'rb') as inp:
        total = inp.getnframes()
        if inp.getsampwidth() != 2 or inp.getnchannels() != 2 or inp.getframerate() <= 0:
            raise ValueError('expected 16-bit stereo PCM capture')
        if not 0 <= start < total:
            raise ValueError('script audio marker lies outside capture')
        inp.setpos(start)
        with wave.open(str(dest), 'wb') as out:
            out.setparams(inp.getparams())
            remaining = total - start
            while remaining:
                chunk = inp.readframes(min(remaining, 16384))
                if not chunk or len(chunk) % 4:
                    raise ValueError('truncated or invalid PCM capture')
                out.writeframesraw(chunk)
                remaining -= len(chunk) // 4
        print(f'script origin: {start} frames ({start / inp.getframerate():.3f} s); {total - start} gameplay frames')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log')
    parser.add_argument('source')
    parser.add_argument('dest')
    args = parser.parse_args()
    try:
        align_capture(args.log, args.source, args.dest)
    except (OSError, ValueError, wave.Error, EOFError) as exc:
        parser.exit(1, f'capture origin: {exc}\n')


if __name__ == '__main__':
    main()
