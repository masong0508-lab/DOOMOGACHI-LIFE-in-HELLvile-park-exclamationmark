#!/usr/bin/env python3
"""test_mpack.py - mpack.py's checks: round trips, the layout against pack.h's
C structs (compiled with the host cc when there is one), and seeded
corruptions that validate() must each refuse. Run: python3 -I test_mpack.py"""
import os
import shutil
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import mpack  # noqa: E402

F = mpack.load_format()
fails = 0


def check(name, cond, extra=""):
    global fails
    print(("ok    " if cond else "FAIL  ") + name + ((" - " + extra) if extra and not cond else ""))
    fails += 0 if cond else 1


def refused(name, pack, needle, **kw):
    bad = mpack.validate(pack, F, **kw)
    check(name, any(needle in b for b in bad), "got %r" % bad)


# LZ
import random
rng = random.Random(7)
for label, data in (("empty", b""), ("one", b"a"), ("zeros", bytes(5000)),
                    ("noise", bytes(rng.randrange(256) for _ in range(3000))),
                    ("pattern", b"abcabcabd" * 400)):
    check("lz round trip: " + label, mpack.lz_decode(mpack.lz_encode(data)) == data)
check("lz shrinks a flat image", len(mpack.lz_encode(bytes(5000))) < 200)
for label, z in (("literal past the end", b"\x05ab"), ("copy before any output", b"\x80\x01\x00"),
                 ("truncated copy", b"\x00a\x80\x01")):
    try:
        mpack.lz_decode(z)
        check("lz refuses " + label, False)
    except mpack.Fail:
        check("lz refuses " + label, True)

ex = mpack.build(mpack.example_spec(F), F)
check("example validates", mpack.validate(ex, F) == [], str(mpack.validate(ex, F)))
check("example is deterministic", ex == mpack.build(mpack.example_spec(F), F))
check("example is flagged SFW", ex[19] & F["PACK_F_SFW_EXAMPLE"] == F["PACK_F_SFW_EXAMPLE"])
check("header is 64 bytes", struct.unpack_from("<H", ex, 6)[0] == 64)

# the C view of the same bytes
cc = shutil.which("cc") or shutil.which("gcc")
if cc:
    with tempfile.TemporaryDirectory() as td:
        open(os.path.join(td, "ex.dmpk"), "wb").write(ex)
        src = os.path.join(td, "t.c")
        open(src, "w").write(r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "doomo/pack.h"
int main(int c, char **v) {
    FILE *f = fopen(v[1], "rb"); unsigned char b[1 << 16]; size_t n = fread(b, 1, sizeof b, f);
    DoomoPackHeader h; DoomoPackEntry e0, e1;
    memcpy(&h, b, sizeof h); memcpy(&e0, b + h.dir_off, sizeof e0); memcpy(&e1, b + h.dir_off + 16, sizeof e1);
    unsigned crc = 0xFFFFFFFFu;
    for (size_t i = 16; i < h.total_size; i++) { crc ^= b[i]; for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & -(crc & 1)); }
    crc ^= 0xFFFFFFFFu;
    printf("%d %u %u %u %u %u %u %u %u %u\n", h.magic == PACK_MAGIC, h.version, h.total_size == n, h.crc32 == crc,
           h.n_entries, e0.uid == PACK_UID(PK_KIND_CG, 0), e1.uid == PACK_UID_SCRIPT(0), e1.kind, e1.arg, (unsigned)strlen(h.name));
    return 0; }''')
        exe = os.path.join(td, "t")
        r = subprocess.run([cc, "-I", os.path.join(mpack.ROOT, "core", "include"), src, "-o", exe],
                           capture_output=True, text=True)
        if r.returncode:
            check("pack.h compiles on the host", False, r.stderr[-300:])
        else:
            out = subprocess.run([exe, os.path.join(td, "ex.dmpk")], capture_output=True, text=True).stdout.split()
            check("the C structs read the pack", out == ["1", "1", "1", "1", "2", "1", "1", str(F["kinds"].index("script")), "112", "11"], str(out))
else:
    print("skip  C struct check (no host cc)")


def mutate(pack, off, val, fix_crc=True):
    b = bytearray(pack)
    b[off] = val
    if fix_crc:
        struct.pack_into("<I", b, 12, mpack.crc32(bytes(b[16:])))
    return bytes(b)


refused("refuses a flipped payload byte (crc)", mutate(ex, len(ex) - 3, ex[-3] ^ 1, False), "crc32")
refused("refuses a truncated file", ex[:-8], "total_size")
refused("refuses bad magic", b"XXXX" + ex[4:], "magic")
refused("refuses a wrong port", ex, "wanted ps1", want_port="ps1")
refused("refuses an unknown kind", mutate(ex, 64 + 12, 9), "kind 9")
refused("refuses an unknown codec", mutate(ex, 64 + 13, 7), "codec 7")
refused("refuses a script id outside the user range",
        mutate(ex, 64 + 16 + 14, 5), "not a user pack")
refused("refuses a script whose image id differs", mutate(ex, 64 + 16 + 14, 113), "image is pack id")
refused("refuses unsorted uids", mutate(ex, 64 + 16 + 2, 0xB0), "ascending")
refused("refuses a misaligned payload", mutate(ex, 64 + 4, ex[64 + 4] + 1), "misaligned")
fx0 = open(os.path.join(mpack.ROOT, "core", "tests", "fixtures", "e0_user.pack"), "rb").read()
big = mpack.build(dict(name="big", author="t", port="gba", flags=[], codec="raw",
                       items=[("portrait", 0, 0, bytes(mpack.F_CAP if hasattr(mpack, "F_CAP") else F["PACK_CAP_GBA_DEFAULT"] + 4096))]), F)
refused("GBA default cap", big, "cap", gba_default_cap=True)
check("GBA hard cap still accepts it", mpack.validate(big, F) == [])
# E8: the GBA mature ROM variant shows stills in place: raw, exactly 240x160
gw, gh = F["PACK_CG_W_GBA"], F["PACK_CG_H_GBA"]
gex = mpack.build(mpack.example_spec(F, "gba"), F)
check("E8 GBA example validates for port gba at the default cap", mpack.validate(gex, F, "gba", True) == [])
check("E8 GBA example is a raw 240x160 still", mpack.parse(gex, F)["ents"][0][2] == gw * gh)
refused("E8 refuses an LZ still for port gba", mpack.build(dict(name="g", author="t", port="gba", flags=[], codec="lz",
        items=[("cg", 0, 0, bytes(gw * gh))]), F), "stored raw")
refused("E8 refuses a 320x240 still for port gba", mpack.build(dict(name="g", author="t", port="gba", flags=[], codec="raw",
        items=[("cg", 0, 0, bytes(F["PACK_CG_W"] * F["PACK_CG_H"]))]), F), "a GBA still is")
refused("E8 the native example is not a gba pack", mpack.build(mpack.example_spec(F), F), "built for port native", ) if False else None
ps1 = mpack.build(dict(name="p", author="t", port="ps1", flags=[], codec="raw",
                       items=[("cg", 0, 0, os.urandom(F["PACK_ENTRY_CAP_PS1"] + 4))]), F)
refused("PS1 entry cap", ps1, "PS1 entry cap")
# E10: a Dreamcast pack's script must fit the 32 KB script buffer
dcbig = mpack.build(dict(name="d", author="t", port="dc", flags=[], codec="raw",
                         items=[("script", 0, fx0[3], fx0 + bytes(F["PACK_SCRIPT_CAP_DC"]))]), F)
refused("E10 refuses a script over the DC buffer", dcbig, "DC script buffer")
check("E10 the example validates for port dc", mpack.validate(mpack.build(mpack.example_spec(F, "dc"), F), F, "dc") == [])
for what, spec, needle in (
        ("a cg slot past MSLOT_MAX", dict(items=[("cg", F["MSLOT_MAX"], 0, b"x")]), "cg slot"),
        ("a duplicate script slot", dict(items=[("script", 1, 112, b"x"), ("script", 1, 113, b"x")]), "duplicate"),
        ("a non-ASCII name", dict(name="Björk", items=[("cg", 0, 0, b"x")]), "ASCII"),
        ("a name over 20 characters", dict(name="x" * 21, items=[("cg", 0, 0, b"x")]), "at most")):
    base = dict(name="t", author="t", port="native", flags=[], codec="raw")
    base.update(spec)
    try:
        mpack.build(base, F)
        check("build refuses " + what, False)
    except mpack.Fail as e:
        check("build refuses " + what, needle in str(e), str(e))
# a script entry that is not a TXF_RAW textpack image
bad_script = mpack.build(dict(name="t", author="t", port="native", flags=[], codec="raw",
                              items=[("script", 0, 112, b"not a textpack image at all")]), F)
refused("refuses a script that is not a textpack image", bad_script, "textpack")
fx = bytearray(open(os.path.join(mpack.ROOT, "core", "tests", "fixtures", "e0_user.pack"), "rb").read())
fx[14] &= ~2
nonraw = mpack.build(dict(name="t", author="t", port="native", flags=[], codec="raw",
                          items=[("script", 0, 112, bytes(fx))]), F)
refused("refuses a script that is not TXF_RAW", nonraw, "TXF_RAW")

# manifest + CLI
with tempfile.TemporaryDirectory() as td:
    open(os.path.join(td, "still.bin"), "wb").write(bytes(range(200)) * 50)
    open(os.path.join(td, "m.txt"), "w").write("name Test\nauthor Me\nport ps1\nflags attested\ncg 3 still.bin\n")
    out = os.path.join(td, "t.dmpk")
    rc = subprocess.run([sys.executable, "-I", os.path.join(HERE, "mpack.py"), "build", os.path.join(td, "m.txt"), "--out", out],
                        capture_output=True, text=True)
    check("CLI build from a manifest", rc.returncode == 0, rc.stderr)
    rc = subprocess.run([sys.executable, "-I", os.path.join(HERE, "mpack.py"), "validate", out, "--port", "ps1"],
                        capture_output=True, text=True)
    check("CLI validate", rc.returncode == 0 and rc.stdout.startswith("ok"), rc.stderr)
    open(os.path.join(td, "bad.txt"), "w").write("name T\nauthor T\nport ps1\ncg 1 missing.bin\n")
    rc = subprocess.run([sys.executable, "-I", os.path.join(HERE, "mpack.py"), "build", os.path.join(td, "bad.txt"), "--out", out],
                        capture_output=True, text=True)
    check("CLI names the manifest line of a missing file", rc.returncode == 1 and "bad.txt:4" in rc.stderr, rc.stderr)

print("mpack: %s" % ("all checks passed" if not fails else "%d FAILED" % fails))
sys.exit(1 if fails else 0)
