#!/usr/bin/env python3
"""mpack.py - build, validate and inspect user content packs (DMPK v1).

E7 of the life-sim plan. The format is core/include/doomo/pack.h; every
constant here is read from that header, so the two cannot drift. A pack
holds art (full-screen stills, portrait / figure overrides) and the scene
scripts of the mature scene slots (core/src/pack.c reads it, E2). The tool
checks structure only: it never looks at what the art shows, and the
repository ships only the SFW example pack (`example`).

usage:
  mpack.py build MANIFEST --out PACK        build a pack from a manifest
  mpack.py validate PACK [--port P] [--gba-default-cap]
                                            exit 0 and "ok" when valid
  mpack.py info PACK                        header and entry table
  mpack.py example --out PACK               the repository's SFW example pack

MANIFEST (one directive per line, # comments, paths relative to the file):
  name    Pack name (<= 20 ASCII)
  author  Author (<= 20 ASCII)
  port    gba | ps1 | dc | native
  flags   sfw_example attested             (optional)
  codec   auto | raw | lz                  (optional, default auto; scripts are always raw)
  cg       SLOT FILE                       full-screen still of mature slot SLOT
  portrait ID   FILE                       portrait override of resident id ID
  figure   ID   FILE                       standing figure override of id ID
  script   SLOT PACKID FILE                a textpack.py --user image (.pack)
                                           built with --pack-id PACKID

The LZ codec is the core's save codec (save.c, packart.py): control byte
c < 0x80 = c+1 literals; c >= 0x80 = copy (c & 0x7F) + 3 bytes from the
u16 LE distance that follows. Stdlib only, deterministic output.
"""
import os
import re
import struct
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
PACK_H = os.path.join(ROOT, "core", "include", "doomo", "pack.h")
TXT_H = os.path.join(ROOT, "core", "include", "doomo", "txt.h")


class Fail(Exception):
    pass


def _defines(path):
    """The plain numeric #defines of a header: literals and + - * / << of them."""
    out = {}
    for m in re.finditer(r"^#define\s+(\w+)\s+(.+?)\s*(?:/\*.*)?$", open(path).read(), re.M):
        expr = re.sub(r"(?<=[0-9A-Fa-f])[uUlL]+\b", "", m.group(2).strip())
        if re.fullmatch(r"[0-9xXa-fA-F\s()+\-*<>/]+", expr) and re.search(r"\d", expr):
            try:
                out[m.group(1)] = int(eval(expr.replace("/", "//"), {"__builtins__": {}}))
            except Exception:
                pass
    return out


def _enum(text, names_after):
    m = re.search(r"enum\s*\{\s*" + names_after + r".*?\};", text, re.S)
    if not m:
        raise Fail("pack.h: enum starting with %s not found" % names_after)
    return re.findall(r"\b(PK_[A-Z0-9_]+)\b", m.group(0))


def load_format():
    h = open(PACK_H).read()
    d = _defines(PACK_H)
    f = {k: d[k] for k in (
        "PACK_MAGIC", "PACK_VERSION", "PACK_HEADER_SIZE", "PACK_ENTRY_SIZE", "PACK_NAME_LEN",
        "PACK_AUTHOR_LEN", "PACK_F_SFW_EXAMPLE", "PACK_F_ATTESTED", "PACK_UID_BASE",
        "PACK_CAP_GBA_DEFAULT", "PACK_CAP_GBA_HARD", "PACK_CAP_DISC", "PACK_ENTRY_CAP_PS1",
        "PACK_ENTRIES_MAX", "PACK_CG_W_GBA", "PACK_CG_H_GBA", "PACK_CG_W", "PACK_CG_H", "MSLOT_MAX")}
    f["ports"] = [n[len("PK_PORT_"):].lower() for n in _enum(h, "PK_PORT_GBA") if n.startswith("PK_PORT_") and n != "PK_PORT_COUNT"]
    f["kinds"] = [n[len("PK_KIND_"):].lower() for n in _enum(h, "PK_KIND_CG") if n != "PK_KIND_COUNT"]
    f["codecs"] = [n[len("PK_CODEC_"):].lower() for n in _enum(h, "PK_CODEC_RAW") if n != "PK_CODEC_COUNT"]
    t = _defines(TXT_H)
    f["USER_FIRST"] = t["TXT_PACK_USER_FIRST"]
    mp = re.search(r"#define\s+TXT_PACK_MAX\s+(\d+)", open(TXT_H).read())
    f["USER_END"] = int(mp.group(1)) if mp else 128
    return f


# ---- the LZ codec (same stream as save.c / packart.py) ---------------------
def lz_encode(data, maxoff=65535, maxlen=130):
    n, out, i, lits = len(data), bytearray(), 0, bytearray()
    head = {}

    def flush():
        while lits:
            chunk = lits[:128]
            del lits[:128]
            out.append(len(chunk) - 1)
            out.extend(chunk)

    while i < n:
        best, bd = 0, 0
        if i + 3 <= n:
            key = bytes(data[i:i + 3])
            for j in reversed(head.get(key, [])[-64:]):
                if i - j > maxoff:
                    break
                k = 0
                lim = min(maxlen, n - i)
                while k < lim and data[j + k] == data[i + k]:
                    k += 1
                if k > best:
                    best, bd = k, i - j
                    if k == lim:
                        break
        if best >= 3:
            flush()
            out.append(0x80 | (best - 3))
            out += struct.pack("<H", bd)
            step = best
        else:
            lits.append(data[i])
            step = 1
        for q in range(i, min(i + step, n - 2)):
            head.setdefault(bytes(data[q:q + 3]), []).append(q)
        i += step
    flush()
    return bytes(out)


def lz_decode(buf, limit=64 * 1024 * 1024):
    out, i, n = bytearray(), 0, len(buf)
    while i < n:
        c = buf[i]
        i += 1
        if c < 0x80:
            c += 1
            if i + c > n:
                raise Fail("LZ stream: literal run past the end")
            out += buf[i:i + c]
            i += c
        else:
            if i + 2 > n:
                raise Fail("LZ stream: truncated copy")
            d = buf[i] | (buf[i + 1] << 8)
            i += 2
            if d < 1 or d > len(out):
                raise Fail("LZ stream: copy distance %d with %d bytes produced" % (d, len(out)))
            for _ in range((c & 0x7F) + 3):
                out.append(out[-d])
        if len(out) > limit:
            raise Fail("LZ stream: expands past %d bytes" % limit)
    return bytes(out)


def crc32(b):
    return zlib.crc32(b) & 0xFFFFFFFF      # IEEE, reflected, init / xorout 0xFFFFFFFF


def _ascii(s, n, what):
    try:
        b = s.encode("ascii")
    except UnicodeEncodeError:
        raise Fail("%s must be ASCII" % what)
    if len(b) > n or b"\0" in b:
        raise Fail("%s is at most %d ASCII characters" % (what, n))
    return b.ljust(n, b"\0")


# ---- build -----------------------------------------------------------------
def build(spec, F):
    """spec: dict(name, author, port, flags, codec, items=[(kind, key, arg, bytes)])"""
    port = F["ports"].index(spec["port"])
    items = []
    counters = {}
    seen = set()
    for kind, a, b, data in spec["items"]:
        k = F["kinds"].index(kind)
        if kind == "script":
            slot, packid = a, b
            if not 0 <= slot < F["MSLOT_MAX"]:
                raise Fail("script slot %d: 0..%d" % (slot, F["MSLOT_MAX"] - 1))
            if not F["USER_FIRST"] <= packid < F["USER_END"]:
                raise Fail("script pack id %d: %d..%d" % (packid, F["USER_FIRST"], F["USER_END"] - 1))
            idx, arg = slot, packid
        else:
            idx = counters.get(kind, 0)
            counters[kind] = idx + 1
            arg = a
            if kind == "cg" and not 0 <= arg < F["MSLOT_MAX"]:
                raise Fail("cg slot %d: 0..%d" % (arg, F["MSLOT_MAX"] - 1))
        uid = F["PACK_UID_BASE"] + (k << 16) + idx
        if uid in seen:
            raise Fail("duplicate %s %d" % (kind, a))
        seen.add(uid)
        codec = spec.get("codec", "auto")
        stored, cid = data, 0
        # scripts stay RAW: pack.c hands them to txt_user_set() in place
        if kind != "script" and codec in ("auto", "lz"):
            z = lz_encode(data)
            if codec == "lz" or len(z) < len(data):
                stored, cid = z, F["codecs"].index("lz")
        items.append((uid, k, cid, arg, stored))
    if len(items) > F["PACK_ENTRIES_MAX"]:
        raise Fail("%d entries (max %d)" % (len(items), F["PACK_ENTRIES_MAX"]))
    items.sort()
    dir_end = F["PACK_HEADER_SIZE"] + F["PACK_ENTRY_SIZE"] * len(items)
    off = (dir_end + 3) & ~3
    body, ents = bytearray(), []
    for uid, k, cid, arg, stored in items:
        ents.append(struct.pack("<IIIBBH", uid, off + len(body), len(stored), k, cid, arg))
        body += stored
        while len(body) % 4:
            body.append(0)
    flags = 0
    for fl in spec.get("flags", ()):
        flags |= {"sfw_example": F["PACK_F_SFW_EXAMPLE"], "attested": F["PACK_F_ATTESTED"]}[fl]
    total = off + len(body)
    tail = (struct.pack("<HBBI", len(items), port, flags, F["PACK_HEADER_SIZE"])
            + _ascii(spec["name"], F["PACK_NAME_LEN"], "name")
            + _ascii(spec["author"], F["PACK_AUTHOR_LEN"], "author"))
    rest = tail + b"".join(ents)
    rest += b"\0" * (off - F["PACK_HEADER_SIZE"] - len(b"".join(ents)))
    # header up to the crc field, then crc, then everything the crc covers
    head = struct.pack("<IHHI", F["PACK_MAGIC"], F["PACK_VERSION"], F["PACK_HEADER_SIZE"], total)
    covered = tail + b"".join(ents)
    covered += b"\0" * (off - 16 - len(covered))      # directory padding to the first payload
    covered += bytes(body)
    # layout check: the header is 64 B, entries follow it
    assert 16 + len(tail) == F["PACK_HEADER_SIZE"], "header layout"
    pack = head + struct.pack("<I", crc32(covered)) + covered
    assert len(pack) == total
    return pack


# ---- validate / info -------------------------------------------------------
def parse(pack, F):
    if len(pack) < F["PACK_HEADER_SIZE"]:
        raise Fail("shorter than the %d-byte header" % F["PACK_HEADER_SIZE"])
    magic, ver, hsz, total, crc = struct.unpack_from("<IHHII", pack, 0)
    n, port, flags, dir_off = struct.unpack_from("<HBBI", pack, 16)
    name = pack[24:44].split(b"\0")[0].decode("ascii", "replace")
    author = pack[44:64].split(b"\0")[0].decode("ascii", "replace")
    ents = []
    for i in range(n):
        o = dir_off + i * F["PACK_ENTRY_SIZE"]
        if o + F["PACK_ENTRY_SIZE"] > len(pack):
            break
        ents.append(struct.unpack_from("<IIIBBH", pack, o))
    return dict(magic=magic, ver=ver, hsz=hsz, total=total, crc=crc, n=n, port=port, flags=flags,
                dir_off=dir_off, name=name, author=author, ents=ents)


def validate(pack, F, want_port=None, gba_default_cap=False):
    """Returns a list of problems (empty = valid)."""
    bad = []
    P = parse(pack, F) if len(pack) >= F["PACK_HEADER_SIZE"] else None
    if P is None:
        return ["shorter than the %d-byte header" % F["PACK_HEADER_SIZE"]]
    if P["magic"] != F["PACK_MAGIC"]:
        return ["bad magic 0x%08X (not a DMPK pack)" % P["magic"]]
    if P["ver"] != F["PACK_VERSION"]:
        bad.append("version %d (this tool knows %d)" % (P["ver"], F["PACK_VERSION"]))
    if P["hsz"] != F["PACK_HEADER_SIZE"]:
        bad.append("header_size %d, want %d" % (P["hsz"], F["PACK_HEADER_SIZE"]))
    if P["total"] != len(pack):
        bad.append("total_size %d but the file is %d bytes" % (P["total"], len(pack)))
    if crc32(pack[16:P["total"]]) != P["crc"]:
        bad.append("crc32 mismatch (header 0x%08X, bytes 0x%08X)" % (P["crc"], crc32(pack[16:P["total"]])))
    if P["port"] >= len(F["ports"]):
        return bad + ["port %d is not a known port" % P["port"]]
    pname = F["ports"][P["port"]]
    if want_port and pname != want_port:
        bad.append("built for port %s, wanted %s" % (pname, want_port))
    if P["flags"] & ~(F["PACK_F_SFW_EXAMPLE"] | F["PACK_F_ATTESTED"]):
        bad.append("unknown header flags 0x%02X" % P["flags"])
    for fld, n in (("name", F["PACK_NAME_LEN"]), ("author", F["PACK_AUTHOR_LEN"])):
        raw = pack[24:44] if fld == "name" else pack[44:64]
        z = raw.find(b"\0")
        if (z >= 0 and any(raw[z:])) or any(c > 127 or (c < 32 and c) for c in raw):
            bad.append("%s is not NUL-padded ASCII" % fld)
    cap = F["PACK_CAP_DISC"]
    if pname == "gba":
        cap = F["PACK_CAP_GBA_DEFAULT"] if gba_default_cap else F["PACK_CAP_GBA_HARD"]
    if len(pack) > cap:
        bad.append("%d bytes is over the %s cap of %d" % (len(pack), pname, cap))
    if P["n"] > F["PACK_ENTRIES_MAX"]:
        return bad + ["%d entries (max %d)" % (P["n"], F["PACK_ENTRIES_MAX"])]
    if P["dir_off"] != F["PACK_HEADER_SIZE"]:
        return bad + ["dir_off %d, v1 wants %d" % (P["dir_off"], F["PACK_HEADER_SIZE"])]
    dir_end = P["dir_off"] + P["n"] * F["PACK_ENTRY_SIZE"]
    if dir_end > len(pack) or len(P["ents"]) != P["n"]:
        return bad + ["the entry directory runs past the end"]
    last, spans, scripts = -1, [], {}
    for i, (uid, off, size, kind, codec, arg) in enumerate(P["ents"]):
        tag = "entry %d (uid 0x%08X)" % (i, uid)
        if uid <= last:
            bad.append("%s: uids must be strictly ascending" % tag)
        last = uid
        if kind >= len(F["kinds"]):
            bad.append("%s: kind %d unknown" % (tag, kind))
            continue
        kn = F["kinds"][kind]
        if (uid - F["PACK_UID_BASE"]) >> 16 != kind or uid < F["PACK_UID_BASE"]:
            bad.append("%s: uid does not carry its kind (%s)" % (tag, kn))
        if codec >= len(F["codecs"]):
            bad.append("%s: codec %d unknown" % (tag, codec))
            continue
        if off % 4 or off < dir_end or off + size > len(pack):
            bad.append("%s: payload %d+%d is misaligned, inside the directory or past the end" % (tag, off, size))
            continue
        spans.append((off, off + size, tag))
        if size == 0:
            bad.append("%s: empty payload" % tag)
            continue
        data = pack[off:off + size]
        if pname == "ps1" and size > F["PACK_ENTRY_CAP_PS1"]:
            bad.append("%s: %d bytes is over the PS1 entry cap %d" % (tag, size, F["PACK_ENTRY_CAP_PS1"]))
        if F["codecs"][codec] == "lz":
            try:
                data = lz_decode(data)
            except Fail as e:
                bad.append("%s: %s" % (tag, e))
                continue
        if kn == "cg" and arg >= F["MSLOT_MAX"]:
            bad.append("%s: cg slot %d out of 0..%d" % (tag, arg, F["MSLOT_MAX"] - 1))
        if kn == "script":
            slot = uid & 0xFFFF
            if F["codecs"][codec] != "raw":
                bad.append("%s: scripts must be stored raw (they are read in place)" % tag)
            if slot >= F["MSLOT_MAX"]:
                bad.append("%s: script slot %d out of 0..%d" % (tag, slot, F["MSLOT_MAX"] - 1))
            if not F["USER_FIRST"] <= arg < F["USER_END"]:
                bad.append("%s: text pack id %d is not a user pack (%d..%d)" % (tag, arg, F["USER_FIRST"], F["USER_END"] - 1))
            if arg in scripts:
                bad.append("%s: text pack id %d is already used by uid 0x%08X" % (tag, arg, scripts[arg]))
            scripts[arg] = uid
            if len(data) < 16 or data[:2] != b"TX" or data[2] != 1:
                bad.append("%s: not a textpack image (TX v1)" % tag)
            else:
                if data[3] != arg:
                    bad.append("%s: image is pack id %d but the entry says %d" % (tag, data[3], arg))
                if not struct.unpack_from("<H", data, 14)[0] & 2:
                    bad.append("%s: not a TXF_RAW image (build it with textpack.py --user)" % tag)
    spans.sort()
    for (a0, a1, t0), (b0, b1, t1) in zip(spans, spans[1:]):
        if b0 < a1:
            bad.append("%s overlaps %s" % (t1, t0))
    return bad


def info(pack, F):
    P = parse(pack, F)
    lines = ["name     %s" % P["name"], "author   %s" % P["author"],
             "port     %s" % (F["ports"][P["port"]] if P["port"] < len(F["ports"]) else "?"),
             "flags    0x%02X%s%s" % (P["flags"], " sfw_example" if P["flags"] & F["PACK_F_SFW_EXAMPLE"] else "",
                                      " attested" if P["flags"] & F["PACK_F_ATTESTED"] else ""),
             "size     %d bytes, %d entries, crc32 0x%08X" % (P["total"], P["n"], P["crc"])]
    for uid, off, size, kind, codec, arg in P["ents"]:
        lines.append("  0x%08X %-8s %-3s %7d B at %-7d arg %d" % (
            uid, F["kinds"][kind] if kind < len(F["kinds"]) else "?",
            F["codecs"][codec] if codec < len(F["codecs"]) else "?", size, off, arg))
    return "\n".join(lines)


# ---- manifest --------------------------------------------------------------
def read_manifest(path, F):
    base = os.path.dirname(os.path.abspath(path))
    spec = dict(name=None, author=None, port=None, flags=[], codec="auto", items=[])

    def rd(rel, ln):
        p = os.path.join(base, rel)
        if not os.path.isfile(p):
            raise Fail("%s:%d: no such file %s" % (path, ln, rel))
        return open(p, "rb").read()

    for ln, line in enumerate(open(path), 1):
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        w = line.split(None, 1)
        key, rest = w[0], (w[1].strip() if len(w) > 1 else "")
        try:
            if key in ("name", "author"):
                spec[key] = rest
            elif key == "port":
                if rest not in F["ports"]:
                    raise Fail("port must be one of %s" % ", ".join(F["ports"]))
                spec["port"] = rest
            elif key == "flags":
                for fl in rest.split():
                    if fl not in ("sfw_example", "attested"):
                        raise Fail("unknown flag %s" % fl)
                    spec["flags"].append(fl)
            elif key == "codec":
                if rest not in ("auto", "raw", "lz"):
                    raise Fail("codec: auto, raw or lz")
                spec["codec"] = rest
            elif key in ("cg", "portrait", "figure"):
                a = rest.split(None, 1)
                spec["items"].append((key, int(a[0]), 0, rd(a[1], ln)))
            elif key == "script":
                a = rest.split(None, 2)
                spec["items"].append(("script", int(a[0]), int(a[1]), rd(a[2], ln)))
            else:
                raise Fail("unknown directive %s" % key)
        except (ValueError, IndexError):
            raise Fail("%s:%d: cannot read '%s'" % (path, ln, line))
        except Fail as e:
            raise Fail("%s:%d: %s" % (path, ln, e))
    for k in ("name", "author", "port"):
        if not spec[k]:
            raise Fail("%s: missing '%s'" % (path, k))
    return spec


def example_spec(F):
    """The repository's SFW example: one test-pattern still and the E0 fixture script."""
    w, h = F["PACK_CG_W"], F["PACK_CG_H"]
    cg = bytes(((x // 20 + y // 20) * 7) & 255 for y in range(h) for x in range(w))
    fx = open(os.path.join(ROOT, "core", "tests", "fixtures", "e0_user.pack"), "rb").read()
    return dict(name="SFW Example", author="Doomogachi", port="native", flags=["sfw_example"],
                codec="auto", items=[("cg", 0, 0, cg), ("script", 0, fx[3], fx)])


def main(argv):
    F = load_format()
    try:
        if len(argv) < 2 or argv[1] in ("-h", "--help"):
            print(__doc__)
            return 0
        cmd, args = argv[1], argv[2:]
        out = args[args.index("--out") + 1] if "--out" in args else None
        if cmd == "build" and out:
            pack = build(read_manifest(args[0], F), F)
        elif cmd == "example" and out:
            pack = build(example_spec(F), F)
        elif cmd in ("validate", "info"):
            pack = open(args[0], "rb").read()
            if cmd == "info":
                print(info(pack, F))
                return 0
            port = args[args.index("--port") + 1] if "--port" in args else None
            bad = validate(pack, F, port, "--gba-default-cap" in args)
            for b in bad:
                print("mpack: %s" % b, file=sys.stderr)
            if bad:
                return 1
            print("ok: %s, %d bytes" % (args[0], len(pack)))
            return 0
        else:
            print(__doc__, file=sys.stderr)
            return 2
        bad = validate(pack, F)
        if bad:
            raise Fail("built pack fails validation: " + "; ".join(bad))
        with open(out, "wb") as f:
            f.write(pack)
        print("mpack: wrote %s (%d bytes)" % (out, len(pack)))
        return 0
    except (Fail, OSError) as e:
        print("mpack: %s" % e, file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
