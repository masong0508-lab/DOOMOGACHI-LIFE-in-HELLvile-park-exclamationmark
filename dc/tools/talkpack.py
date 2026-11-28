#!/usr/bin/env python3
"""Lossless sector-aligned Talk bank for Dreamcast idle prefetch.

TLZ1 header sector: magic, little-endian raw size, zero padding.
Data sectors: u16 payload length, literal/match tokens, zero padding.
Literal: 0..127 = 1..128 bytes. Short match: 0x80..0xbf, length
3..66 and one byte distance-1. Long match: 0xc0..0xff, length
3..66 and LE16 distance. Matches may reference earlier sectors.
At most 8192 bytes are expanded per sector to bound frame work.
"""
import struct
import sys
from pathlib import Path

SECTOR = 2048
MAX_OUTPUT = 8192


def pack(raw):
    if not raw.startswith(b'TALK') or len(raw) < 20 or len(raw) != struct.unpack_from('<I', raw, 16)[0]:
        raise ValueError('invalid Talk bank header')
    if len(raw) > 8 * 1024 * 1024:
        raise ValueError('Talk bank exceeds Dreamcast loader limit')
    out = bytearray(b'TLZ1' + struct.pack('<I', len(raw)))
    out.extend(bytes(SECTOR - len(out)))
    pos = 0
    last = {}
    older = {}

    def remember(k, p):
        if k in last:
            older[k] = last[k]
        last[k] = p

    while pos < len(raw):
        start = pos
        payload = bytearray()
        while pos < len(raw) and pos - start < MAX_OUTPUT and len(payload) < SECTOR - 4:
            limit = min(66, len(raw) - pos, MAX_OUTPUT - (pos - start))
            k = raw[pos:pos + 4]
            n = 0
            dist = 0
            if limit >= 4:
                for prev in (last.get(k, -65536), older.get(k, -65536)):
                    d = pos - prev
                    if not 0 < d <= 65535:
                        continue
                    m = 4
                    while m < limit and raw[prev + m] == raw[pos + m]:
                        m += 1
                    if m > n:
                        n, dist = m, d
            if n >= 4:
                if dist <= 256:
                    payload.extend((0x80 | (n - 3), dist - 1))
                else:
                    payload.extend((0xC0 | (n - 3), dist & 255, dist >> 8))
                for j in range(pos, pos + n):
                    remember(raw[j:j + 4], j)
                pos += n
            else:
                off = pos
                while (pos - off < 128 and pos < len(raw) and pos - start < MAX_OUTPUT
                       and len(payload) + 1 + (pos - off) < SECTOR - 2):
                    k = raw[pos:pos + 4]
                    prev = last.get(k, -65536)
                    if pos > off and pos + 4 <= len(raw) and 0 < pos - prev <= 65535:
                        break
                    remember(k, pos)
                    pos += 1
                payload.append(pos - off - 1)
                payload.extend(raw[off:pos])
        out.extend(struct.pack('<H', len(payload)))
        out.extend(payload)
        out.extend(bytes(SECTOR - 2 - len(payload)))
    return bytes(out)


if __name__ == '__main__':
    if len(sys.argv) != 3:
        raise SystemExit('usage: talkpack.py INPUT.BIN OUTPUT.LZB')
    raw = Path(sys.argv[1]).read_bytes()
    packed = pack(raw)
    Path(sys.argv[2]).write_bytes(packed)
    print(f'Talk prefetch: {len(raw)} bytes -> {len(packed)} bytes ({len(packed) // SECTOR} sectors)')
