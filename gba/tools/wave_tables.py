#!/usr/bin/env python3
"""Pre-computed wave-channel tables for gba/src/audio.c (integer maths only, byte-identical everywhere).

  python3 gba/tools/wave_tables.py          print the block
  python3 gba/tools/wave_tables.py --check  fail if the block in gba/src/audio.c is stale
  python3 gba/tools/wave_tables.py --write  rewrite the block in gba/src/audio.c

Every table is the 16 bytes (32 4-bit samples, high nibble first) packed as 4 little-endian words, for one
waveform at one volume 1..15 (the amplitude of audio.c's wave_vol[]), and for the bass pluck (the triangle
`mix` 0..14 of 16 of the way from the saw; mix 16 is the plain triangle).
"""
import os, sys

TABLES = [
    [0xFF, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0xFF, 0xFF, 0xFF, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0],
    [0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10],
    [0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF],
    [0x8F, 0x3C, 0xA1, 0x57, 0xE2, 0x09, 0xB6, 0x4D, 0x72, 0xC8, 0x1F, 0x95, 0x3A, 0xE6, 0x0B, 0x64],
]
AMP = [0, 68, 136, 204, 136, 170, 204, 238, 182, 205, 228, 250, 205, 222, 239, 256]   # audio.c wave_vol[].amp
TRI, SAW = 3, 4
BEGIN, END = "/* BEGIN gba/tools/wave_tables.py */", "/* END gba/tools/wave_tables.py */"

def nib(t, k): return t[k >> 1] & 15 if k & 1 else t[k >> 1] >> 4
def scale(s, amp): return (15 * 256 + (2 * s - 15) * amp + 256) >> 9

def build(w, amp, mix):
    out = []
    for j in range(16):
        b = 0
        for h in range(2):
            k = j * 2 + h
            v = nib(TABLES[w], k)
            if mix < 16: v = (nib(TABLES[SAW], k) * (16 - mix) + v * mix + 8) >> 4
            if amp < 256: v = scale(v, amp)
            b |= v << (4 if h == 0 else 0)
        out.append(b)
    return [sum(out[i * 4 + j] << (8 * j) for j in range(4)) for i in range(4)]

def rows(fn, n):
    return ",\n".join("    {" + ", ".join("{" + ", ".join("0x%08X" % x for x in fn(i, v)) + "}" for v in range(1, 16)) + "}" for i in range(n)) if False else None

def block():
    L = [BEGIN,
         "/* wave channel tables, [waveform][volume - 1] / [pluck mix / 2][volume - 1]: 4 words of wave RAM each */",
         "static const uint32_t wave_plain[WAVE_COUNT][15][4] = {"]
    for w in range(6):
        L.append("    {")
        for v in range(1, 16):
            L.append("        {" + ", ".join("0x%08X" % x for x in build(w, AMP[v], 16)) + "},")
        L.append("    },")
    L += ["};", "#if GBA_PLUCK", "static const uint32_t wave_pluck[8][15][4] = {"]
    for m in range(0, 16, 2):
        L.append("    {")
        for v in range(1, 16):
            L.append("        {" + ", ".join("0x%08X" % x for x in build(TRI, AMP[v], m)) + "},")
        L.append("    },")
    L += ["};", "#endif", END]
    return "\n".join(L) + "\n"

if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    path = os.path.join(here, "..", "src", "audio.c")
    new = block()
    if len(sys.argv) < 2: sys.stdout.write(new); sys.exit(0)
    src = open(path).read()
    a, b = src.find(BEGIN), src.find(END)
    if a < 0 or b < 0: sys.exit("markers not found in audio.c")
    cur = src[a:b + len(END) + 1]
    if sys.argv[1] == "--check":
        if cur != new: sys.exit("gba/src/audio.c wave tables are stale: run wave_tables.py --write")
        print("wave tables ok"); sys.exit(0)
    open(path, "w").write(src[:a] + new + src[b + len(END) + 1:])
