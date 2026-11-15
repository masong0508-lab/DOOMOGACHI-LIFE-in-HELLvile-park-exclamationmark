#!/usr/bin/env python3
# doomo-generator
"""texbank.py - LV7a: the per-level texture bank (plan 8.6 / 8.8). Format: core/include/doomo/texbank.h.

A bank is described by core/data/texbanks/NAME.bank:

    bank NAME                                 (once, first; NAME = the file name)
    tex SLOT [frames=N] [speed=T] [mode=loop|pingpong] KIND args...

Colours: RAMP.shade (STONE.3: the 15 environment ramps of palette.h, shades 0..7) or PAL.index.
Kinds (every one tiles seamlessly on 64 x 64; the bank has at most 8 textures, 32 frames):
    fill C
    checker C1 C2 N        cells of N px (N divides 64)
    bricks C1 C2 MORTAR    rows of 8 px, bricks of 16 px, every second row shifted, C1 / C2 mixed
    stripes C1 C2 N        diagonal stripes of N px (2N divides 64); with frames=F they slide
    noise C1 C2 SEED       speckle; frame f uses SEED + f

Output: core/gen/tb_NAME.c (const data for the host and the Dreamcast; compiled out on the PS1 and the
GBA) with `--out-root DIR` (the make gen convention), or the raw blob with `--bin NAME FILE`, the
pack-ready bytes the level packs of LV8 (tools/packbreach.py) copy as they are:
    import texbank; blob = texbank.build_file(path)
"""
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
BANK_DIR = os.path.join(ROOT, "core", "data", "texbanks")
TEX_PX = 64 * 64
MAGIC = b"LVTB"
VERSION = 1
MAX_SLOTS = 8
MAX_FRAMES = 4
MAX_TOTAL = 32
NAME_LEN = 12
MODES = {"loop": 0, "pingpong": 1}     # DOOMO_ANIM_LOOP / DOOMO_ANIM_PINGPONG


class BankError(Exception):
    pass


def _ramps():
    with open(os.path.join(ROOT, "core", "include", "doomo", "palette.h")) as f:
        text = f.read()
    names = re.findall(r"RAMP_([A-Z_]+)", re.search(r"enum\s*\{[^}]*RAMP_STONE[^}]*\}", text, re.S).group(0))
    base = int(re.search(r"#define PAL_RAMP_BASE\s+(\d+)", text).group(1))
    count = int(re.search(r"#define PAL_RAMP_COUNT\s+(\d+)", text).group(1))
    shades = int(re.search(r"#define PAL_RAMP_SHADES\s+(\d+)", text).group(1))
    if len(names) != count:
        raise BankError("palette.h: %d ramp names for PAL_RAMP_COUNT %d" % (len(names), count))
    return names, base, shades


_RAMPS = None


def colour(tok):
    global _RAMPS
    if _RAMPS is None:
        _RAMPS = _ramps()
    names, base, shades = _RAMPS
    m = re.fullmatch(r"([A-Z_]+)\.(\d+)", tok)
    if not m:
        raise BankError("colour '%s': write RAMP.shade (STONE.3) or PAL.index" % tok)
    r, n = m.group(1), int(m.group(2))
    if r == "PAL":
        if not 1 <= n <= 255:
            raise BankError("colour '%s': palette index 1..255 (0 is clear)" % tok)
        return n
    if r not in names:
        raise BankError("colour '%s': no ramp %s (have %s)" % (tok, r, " ".join(names)))
    if n >= shades:
        raise BankError("colour '%s': shade 0..%d" % (tok, shades - 1))
    return base + names.index(r) * shades + n


class Rng:
    """xorshift32: the same bytes on every machine."""

    def __init__(self, seed):
        self.s = (seed * 2654435761 + 12345) & 0xFFFFFFFF or 1

    def next(self):
        s = self.s
        s ^= (s << 13) & 0xFFFFFFFF
        s ^= s >> 17
        s ^= (s << 5) & 0xFFFFFFFF
        self.s = s
        return s


def _int(tok, lo, hi, what):
    try:
        v = int(tok)
    except ValueError:
        raise BankError("%s '%s' is not a number" % (what, tok))
    if not lo <= v <= hi:
        raise BankError("%s %d outside %d..%d" % (what, v, lo, hi))
    return v


def paint(kind, args, f, frames):
    """One 64 x 64 frame (bytes)."""
    px = bytearray(TEX_PX)
    if kind == "fill":
        if len(args) != 1:
            raise BankError("fill C")
        px[:] = bytes([colour(args[0])]) * TEX_PX
    elif kind == "checker":
        if len(args) != 3:
            raise BankError("checker C1 C2 N")
        c1, c2, n = colour(args[0]), colour(args[1]), _int(args[2], 1, 32, "cell")
        if 64 % n:
            raise BankError("checker cell %d does not divide 64" % n)
        for y in range(64):
            for x in range(64):
                px[y * 64 + x] = c1 if ((x // n) + (y // n)) % 2 == 0 else c2
    elif kind == "bricks":
        if len(args) != 3:
            raise BankError("bricks C1 C2 MORTAR")
        c1, c2, mo = colour(args[0]), colour(args[1]), colour(args[2])
        rng = Rng(7)
        shade = [[rng.next() & 1 for _ in range(4)] for _ in range(8)]
        for y in range(64):
            row = y // 8
            off = 8 * (row % 2)
            for x in range(64):
                xx = (x + off) % 64
                if y % 8 == 7 or xx % 16 == 15:
                    px[y * 64 + x] = mo
                else:
                    px[y * 64 + x] = c2 if shade[row][xx // 16] else c1
    elif kind == "stripes":
        if len(args) != 3:
            raise BankError("stripes C1 C2 N")
        c1, c2, n = colour(args[0]), colour(args[1]), _int(args[2], 1, 16, "stripe")
        if 64 % (2 * n):
            raise BankError("stripes: 2 x %d does not divide 64" % n)
        shift = f * (2 * n) // frames
        for y in range(64):
            for x in range(64):
                px[y * 64 + x] = c1 if ((x + y + shift) // n) % 2 == 0 else c2
    elif kind == "noise":
        if len(args) != 3:
            raise BankError("noise C1 C2 SEED")
        c1, c2, seed = colour(args[0]), colour(args[1]), _int(args[2], 0, 65535, "seed")
        rng = Rng(seed + f)
        for i in range(TEX_PX):
            px[i] = c1 if rng.next() & 0x300 else c2
    else:
        raise BankError("no kind '%s' (fill checker bricks stripes noise)" % kind)
    return bytes(px)


def avg_of(px):
    """The most frequent palette index of a frame (lowest wins a tie)."""
    cnt = {}
    for c in px:
        cnt[c] = cnt.get(c, 0) + 1
    return max(sorted(cnt), key=lambda c: cnt[c])


def build(text, name=None):
    """Spec text -> (bank name, blob bytes). Every error names its line."""
    bname = None
    slots = []
    for ln, raw in enumerate(text.splitlines(), 1):
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        t = line.split()
        try:
            if t[0] == "bank":
                if len(t) != 2 or bname is not None or slots:
                    raise BankError("bank NAME: once, first")
                bname = t[1]
                if not re.fullmatch(r"[a-z][a-z0-9_]*", bname):
                    raise BankError("bank name '%s': lowercase letters, digits, _" % bname)
            elif t[0] == "tex":
                if bname is None:
                    raise BankError("`bank NAME` comes first")
                if len(t) < 3:
                    raise BankError("tex SLOT [options] KIND args")
                if not re.fullmatch(r"[a-z][a-z0-9_]{0,%d}" % (NAME_LEN - 2), t[1]):
                    raise BankError("slot name '%s': lowercase, at most %d characters" % (t[1], NAME_LEN - 1))
                if any(s["name"] == t[1] for s in slots):
                    raise BankError("slot name '%s' given twice" % t[1])
                if len(slots) >= MAX_SLOTS:
                    raise BankError("a bank has at most %d textures" % MAX_SLOTS)
                s = {"name": t[1], "frames": 1, "speed": 1, "mode": 0}
                i = 2
                while i < len(t) and "=" in t[i]:
                    k, v = t[i].split("=", 1)
                    if k == "frames":
                        s["frames"] = _int(v, 1, MAX_FRAMES, "frames")
                    elif k == "speed":
                        s["speed"] = _int(v, 1, 255, "speed")
                    elif k == "mode":
                        if v not in MODES:
                            raise BankError("mode loop or pingpong")
                        s["mode"] = MODES[v]
                    else:
                        raise BankError("option '%s' (frames= speed= mode=)" % k)
                    i += 1
                if i >= len(t):
                    raise BankError("no kind")
                s["frames_px"] = [paint(t[i], t[i + 1:], f, s["frames"]) for f in range(s["frames"])]
                if s["frames"] > 1 and t[i] not in ("stripes", "noise"):
                    raise BankError("only stripes and noise can have frames=")
                slots.append(s)
            else:
                raise BankError("unknown statement '%s'" % t[0])
        except BankError as e:
            raise BankError("line %d: %s" % (ln, e))
    if bname is None or not slots:
        raise BankError("empty bank: `bank NAME` and at least one `tex`")
    if name is not None and name != bname:
        raise BankError("the file is %s.bank but says `bank %s`" % (name, bname))
    if sum(s["frames"] for s in slots) > MAX_TOTAL:
        raise BankError("more than %d frames" % MAX_TOTAL)
    ent = b""
    pix = b""
    for s in slots:
        ent += s["name"].encode().ljust(NAME_LEN, b"\0") + struct.pack("<BBBB", s["frames"], s["speed"], s["mode"],
                                                                         avg_of(s["frames_px"][0]))
        pix += b"".join(s["frames_px"])
    n = 12 + len(ent) + len(pix)
    return bname, MAGIC + struct.pack("<BBHI", VERSION, len(slots), 0, n) + ent + pix


def build_file(path):
    with open(path) as f:
        return build(f.read(), os.path.splitext(os.path.basename(path))[0])[1]


def c_source(name, blob):
    lines = ["/* generated by tools/texbank.py from core/data/texbanks/%s.bank: do not edit. The texture bank of\n"
             " * %s (format: doomo/texbank.h), %d bytes. Compiled out on the PS1 and the GBA: their level packs\n"
             " * carry the same bytes (LV8). */" % (name, name, len(blob)),
             "#include \"doomo/texbank.h\"", "",
             "#if DOOMO_TEXBANK_ACTIVE", "",
             "const uint8_t doomo_texbank_%s[%d] = {" % (name, len(blob))]
    for i in range(0, len(blob), 24):
        lines.append("    " + ",".join(str(b) for b in blob[i:i + 24]) + ",")
    lines += ["};", "const uint32_t doomo_texbank_%s_len = %d;" % (name, len(blob)), "", "#endif", ""]
    return "\n".join(lines)


def banks():
    return sorted(f[:-5] for f in os.listdir(BANK_DIR) if f.endswith(".bank")) if os.path.isdir(BANK_DIR) else []


def main(argv):
    root = ROOT
    i = 0
    try:
        while i < len(argv):
            if argv[i] == "--out-root" and i + 1 < len(argv):
                root = argv[i + 1]
                i += 2
            elif argv[i] == "--bin" and i + 2 < len(argv):
                blob = build_file(os.path.join(BANK_DIR, argv[i + 1] + ".bank"))
                with open(argv[i + 2], "wb") as f:
                    f.write(blob)
                return 0
            else:
                sys.exit("usage: texbank.py [--out-root DIR] | --bin NAME FILE")
        for name in banks():
            blob = build_file(os.path.join(BANK_DIR, name + ".bank"))
            out = os.path.join(root, "core", "gen", "tb_%s.c" % name)
            os.makedirs(os.path.dirname(out), exist_ok=True)
            with open(out, "w", newline="\n") as f:
                f.write(c_source(name, blob))
    except BankError as e:
        sys.exit("texbank: %s" % e)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
