#!/usr/bin/env python3
"""gbafix.py - finalise a GBA ROM header (owner: gba). Stdlib only.

    python3 gba/tools/gbafix.py ROM.gba [--title DOOMOGACHI] [--code BDME]
                                [--maker DM] [--version 0] [--pad]

* writes the Nintendo logo, title (12 chars), game code (4), maker code (2),
  the fixed 0x96 byte and the software version
* computes the header complement check at 0xBD:
      chk = -(sum(bytes[0xA0:0xBD]) + 0x19) & 0xFF
* --pad pads the file with 0xFF to the next power of two (>= 256 KB), the
  way real carts and flashers like it
* --check only verifies (exit status 1 on a bad header)
"""
import argparse
import sys

LOGO = bytes.fromhex(
    "24FFAE51699AA2213D84820A84E409AD11248B98C0817F21A352BE199309CE20"
    "10464A4AF82731EC58C7E83382E3CEBF85F4DF94CE4B09C194568AC01372A7FC"
    "9F844D73A3CA9A615897A327FC039876231DC7610304AE56BF38840040A70EFD"
    "FF52FE036F9530F197FBC08560D68025A963BE03014E38E2F9A234FFBB3E0344"
    "780090CB88113A9465C07C6387F03CAFD625E48B380AAC7221D4F807")
assert len(LOGO) == 156


def complement(hdr):
    return (-(sum(hdr[0xA0:0xBD]) + 0x19)) & 0xFF


def fix(data, title, code, maker, version):
    if len(data) < 0xC0:
        raise SystemExit("gbafix: file too small for a GBA header")
    data[0x04:0xA0] = LOGO
    t = title.encode("ascii")[:12]
    data[0xA0:0xAC] = t + b"\0" * (12 - len(t))
    c = code.encode("ascii")[:4]
    data[0xAC:0xB0] = c + b"\0" * (4 - len(c))
    m = maker.encode("ascii")[:2]
    data[0xB0:0xB2] = m + b"\0" * (2 - len(m))
    data[0xB2] = 0x96
    data[0xB3] = 0
    data[0xB4] = 0
    data[0xB5:0xBC] = b"\0" * 7
    data[0xBC] = version & 0xFF
    data[0xBE:0xC0] = b"\0\0"
    data[0xBD] = complement(data)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("rom")
    ap.add_argument("--title", default="DOOMOGACHI")
    ap.add_argument("--code", default="BDME")
    ap.add_argument("--maker", default="DM")
    ap.add_argument("--version", type=int, default=0)
    ap.add_argument("--pad", action="store_true")
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    with open(a.rom, "rb") as f:
        data = bytearray(f.read())
    if a.check:
        ok = data[0x04:0xA0] == LOGO and data[0xBD] == complement(data) \
            and data[0xB2] == 0x96
        print("gbafix: header %s" % ("OK" if ok else "BAD"))
        return 0 if ok else 1
    fix(data, a.title, a.code, a.maker, a.version)
    if a.pad:
        size = 256 * 1024
        while size < len(data):
            size *= 2
        data += b"\xff" * (size - len(data))
    with open(a.rom, "wb") as f:
        f.write(data)
    print("gbafix: %s title=%s code=%s maker=%s chk=0x%02X size=%d" %
          (a.rom, a.title, a.code, a.maker, data[0xBD], len(data)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
