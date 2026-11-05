#!/usr/bin/env python3
"""sfxbin.py - write SFX.BIN, the PS1 disc's sound effect PCM (owner: ps1).

    sfxbin.py core/gen/sfx_data.c SFX.BIN

core/gen/sfx_data.c (tools/sfxgen.py) holds the effects as one array of
signed 8-bit PCM; the PS1 EXE compiles it out (DOOMO_SFX_ON_DISC) to keep
~47 KB of RAM, and src/sfxdisc.c reads the bytes from this file. The file
is the array as raw bytes, padded to a whole number of 2048-byte sectors,
so the offset of an effect (doomo_sfx_info) is its byte offset in the file.
Stdlib only, deterministic.
"""
import re
import sys


def main(argv):
    if len(argv) != 3:
        sys.stderr.write(__doc__)
        return 2
    text = open(argv[1], encoding="utf-8").read()
    m = re.search(r"doomo_sfx_pcm_data\[\d+\]\s*=\s*\{(.*?)\};", text, re.S)
    t = re.search(r"doomo_sfx_pcm_total\s*=\s*(\d+)u", text)
    if not m or not t:
        sys.stderr.write("sfxbin: no doomo_sfx_pcm_data in %s\n" % argv[1])
        return 1
    vals = [int(v) for v in re.findall(r"-?\d+", m.group(1))]
    total = int(t.group(1))
    if len(vals) != total or any(v < -128 or v > 127 for v in vals):
        sys.stderr.write("sfxbin: %d samples, expected %d\n" % (len(vals), total))
        return 1
    data = bytes(v & 0xFF for v in vals)
    data += bytes(-len(data) % 2048)
    with open(argv[2], "wb") as f:
        f.write(data)
    print("sfxbin: %s: %d bytes of PCM, %d sectors" % (argv[2], total, len(data) // 2048))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
