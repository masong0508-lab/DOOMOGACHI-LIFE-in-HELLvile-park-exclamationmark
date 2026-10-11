#!/usr/bin/env python3
"""packart.py - keep the core's textures and skies compressed in PS1 RAM.

The PS1 EXE must fit, with everything else, in 2 MB of RAM. The world
textures (60 frames of 64x64) and skies (256x96 frames) are 360 KB of 8bpp
pixels that only the PS1 platform ever reads, to upload them into its VRAM
caches (docs/PS1.md section 3). So the PS1 build compiles the art without
them and links them packed instead:

  art_data_ps1.c   core/gen/art_data.c minus the doomo_art_tex_pix and
                   doomo_art_sky_pix arrays. Those two symbols become
                   VIRTUAL addresses (ld --defsym, see CMakeLists.txt): the
                   core keeps computing frame pointers from them (textures.c)
                   but never reads through them on the PS1.
  sprites          (the same for the 194 sprite frames, ~230 KB: the static
                   spr_pix array becomes the virtual doomo_art_spr_pix_v;
                   the core only hands sprite images to plat_* calls, and
                   vram.c resolves a pointer into any frame, interior
                   pointers of crops included, before it uploads)
  packed_art.c     every frame LZ-compressed on its own (random access), an
                   offset index per frame and each sky frame's top / bottom
                   edge colour (r3d.c fills above / below the sky strip with
                   them). ps1/src/packart.c decodes a frame on a VRAM cache
                   miss.

Codec (the core's save codec, save.c): control byte c < 0x80: c+1 literal
bytes follow; c >= 0x80: copy (c & 0x7F) + 3 bytes from distance d (u16
LE that follows), 1 <= d <= bytes already produced in the frame.

usage: packart.py core/gen/art_data.c OUT_DIR
Stdlib only, deterministic output.
"""
import os
import re
import sys

ARRAYS = (("doomo_art_tex_pix", "tex"), ("doomo_art_sky_pix", "sky"))
SKY_W, SKY_H = 256, 96


def lz(data, maxoff=65535, maxlen=130):
    """Greedy LZ77 with hash chains (64 tries)."""
    out = bytearray()
    lits = bytearray()
    n = len(data)
    head = {}
    prev = [-1] * n

    def flush():
        nonlocal lits
        while lits:
            k = min(len(lits), 128)
            out.append(k - 1)
            out.extend(lits[:k])
            lits = lits[k:]

    def insert(p):
        if p + 3 <= n:
            h = data[p:p + 3]
            prev[p] = head.get(h, -1)
            head[h] = p

    i = 0
    while i < n:
        best, boff = 0, 0
        if i + 3 <= n:
            c = head.get(data[i:i + 3], -1)
            tries = 0
            ml = min(maxlen, n - i)
            while c >= 0 and i - c <= maxoff and tries < 64:
                l = 0
                while l < ml and data[c + l] == data[i + l]:
                    l += 1
                if l > best:
                    best, boff = l, i - c
                    if best == ml:
                        break
                c = prev[c]
                tries += 1
        if best >= 3:
            flush()
            out.append(0x80 | (best - 3))
            out.append(boff & 255)
            out.append(boff >> 8)
            for k in range(best):
                insert(i + k)
            i += best
        else:
            lits.append(data[i])
            insert(i)
            i += 1
            if len(lits) == 128:
                flush()
    flush()
    return bytes(out)


def unlz(src, n):
    out = bytearray()
    i = 0
    while len(out) < n:
        c = src[i]
        i += 1
        if c < 0x80:
            out.extend(src[i:i + c + 1])
            i += c + 1
        else:
            d = src[i] | (src[i + 1] << 8)
            i += 2
            for _ in range((c & 0x7F) + 3):
                out.append(out[-d])
    return bytes(out)


def cut_array(text, name):
    """(frames as bytes, text without the array definition)"""
    m = re.search(r"^const uint8_t %s\[[^\n]*= \{\n" % name, text, re.M)
    if not m:
        raise SystemExit("packart: %s not found" % name)
    end = text.index("\n};\n", m.end())
    body = text[m.end():end]
    frames = []
    for blk in re.finditer(r"/\*[^*]*\*/ \{([^}]*)\}", body):
        vals = [int(v) for v in re.findall(r"\d+", blk.group(1))]
        frames.append(bytes(vals))
    if not frames or any(len(f) != len(frames[0]) for f in frames):
        raise SystemExit("packart: %s: unexpected frame layout" % name)
    stub = "/* %s: packed for the PS1 (ps1/tools/packart.py, packed_art.c) */\n" % name
    return frames, text[:m.start()] + stub + text[end + 4:]


def cut_sprites(text):
    """(raw sprite bytes, [(start, size)] per frame, text with spr_pix virtual)"""
    m = re.search(r"^static const uint8_t spr_pix\[\] = \{\n", text, re.M)
    if not m:
        raise SystemExit("packart: spr_pix not found")
    end = text.index("\n};\n", m.end())
    body = re.sub(r"/\*.*?\*/", "", text[m.end():end], flags=re.S)
    raw = bytes(int(v) for v in re.findall(r"\d+", body))
    frames = []
    for fm in re.finditer(r"\{(\d+), (\d+), spr_pix \+ (\d+), DOOMO_ART_SPR_UID", text):
        w, h, off = int(fm.group(1)), int(fm.group(2)), int(fm.group(3))
        frames.append((off, w * h))
    frames.sort()
    for (o1, n1), (o2, _) in zip(frames, frames[1:] + [(len(raw), 0)]):
        if o1 + n1 > o2:
            raise SystemExit("packart: sprite frames overlap")
    if not frames or frames[0][0] != 0:
        raise SystemExit("packart: unexpected sprite layout")
    stub = ("/* spr_pix: packed for the PS1 (ps1/tools/packart.py, packed_art.c); a\n"
            " * virtual address (ld --defsym), never read through on the PS1 */\n"
            "extern const uint8_t doomo_art_spr_pix_v[];\n"
            "#define spr_pix doomo_art_spr_pix_v\n")
    return raw, frames, text[:m.start()] + stub + text[end + 4:]


def c_bytes(data, indent="    "):
    lines = []
    for k in range(0, len(data), 24):
        lines.append(indent + ",".join(str(b) for b in data[k:k + 24]) + ",")
    return "\n".join(lines)


def main(argv):
    if len(argv) != 3:
        print(__doc__)
        return 2
    src, out_dir = argv[1], argv[2]
    text = open(src).read()
    packed = {}
    for name, key in ARRAYS:
        frames, text = cut_array(text, name)
        packed[key] = frames
    spr_raw, spr_frames, text = cut_sprites(text)
    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, "art_data_ps1.c"), "w") as f:
        f.write("/* generated by ps1/tools/packart.py from core/gen/art_data.c - do not edit */\n")
        f.write(text)
    o = ["/* generated by ps1/tools/packart.py from core/gen/art_data.c - do not edit.",
         " * Every texture / sky frame LZ-compressed on its own (see packart.py). */",
         "#include <stdint.h>", ""]
    total_raw = total_packed = 0
    for key in ("tex", "sky"):
        frames = packed[key]
        blobs = [lz(fr) for fr in frames]
        for fr, b in zip(frames, blobs):
            assert unlz(b, len(fr)) == fr
        ofs = [0]
        for b in blobs:
            ofs.append(ofs[-1] + len(b))
        total_raw += sum(len(fr) for fr in frames)
        total_packed += ofs[-1] + 4 * len(ofs)
        o.append("const uint16_t ps1_pack_%s_frames = %d;" % (key, len(frames)))
        o.append("const uint32_t ps1_pack_%s_size = %d;" % (key, len(frames[0])))
        o.append("const uint32_t ps1_pack_%s_ofs[%d] = {" % (key, len(ofs)))
        o.append("    " + ",".join(str(v) for v in ofs))
        o.append("};")
        o.append("const uint8_t ps1_pack_%s[%d] = {" % (key, ofs[-1]))
        o.append(c_bytes(b"".join(blobs)))
        o.append("};")
        o.append("")
    # sprites: frames of different sizes; ps1_pack_spr_start[] (raw offset
    # of every frame, sorted, + the total) finds the frame of a pointer
    blobs = []
    for off, n in spr_frames:
        fr = spr_raw[off:off + n]
        b = lz(fr)
        assert unlz(b, len(fr)) == fr
        blobs.append(b)
    ofs = [0]
    for b in blobs:
        ofs.append(ofs[-1] + len(b))
    starts = [off for off, _ in spr_frames] + [len(spr_raw)]
    biggest = max(n for _, n in spr_frames)
    total_raw += len(spr_raw)
    total_packed += ofs[-1] + 8 * len(ofs)
    o.append("const uint16_t ps1_pack_spr_frames = %d;" % len(spr_frames))
    o.append("const uint32_t ps1_pack_spr_start[%d] = {" % len(starts))
    o.append("    " + ",".join(str(v) for v in starts))
    o.append("};")
    o.append("const uint32_t ps1_pack_spr_ofs[%d] = {" % len(ofs))
    o.append("    " + ",".join(str(v) for v in ofs))
    o.append("};")
    o.append("const uint8_t ps1_pack_spr[%d] = {" % ofs[-1])
    o.append(c_bytes(b"".join(blobs)))
    o.append("};")
    o.append("/* one decoded sprite frame (the biggest is %d bytes) */" % biggest)
    o.append("uint8_t ps1_pack_spr_buf[%d] __attribute__((aligned(4)));" % ((biggest + 3) & ~3))
    o.append("")
    edges = []
    for fr in packed["sky"]:
        if len(fr) != SKY_W * SKY_H:
            raise SystemExit("packart: sky frames are not %dx%d" % (SKY_W, SKY_H))
        edges += [fr[SKY_W // 2], fr[(SKY_H - 1) * SKY_W + SKY_W // 2]]
    o.append("/* per sky frame: top row / bottom row colour at the centre column */")
    o.append("const uint8_t ps1_pack_sky_edge[%d] = { %s };" %
             (len(edges), ",".join(str(e) for e in edges)))
    with open(os.path.join(out_dir, "packed_art.c"), "w") as f:
        f.write("\n".join(o) + "\n")
    print("packart: %d texture + %d sky + %d sprite frames, %d KB -> %d KB" % (
        len(packed["tex"]), len(packed["sky"]), len(spr_frames), total_raw // 1024,
        total_packed // 1024))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
