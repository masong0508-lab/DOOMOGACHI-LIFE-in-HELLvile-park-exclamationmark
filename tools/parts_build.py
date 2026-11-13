#!/usr/bin/env python3
# doomo-generator
"""parts_build.py - compile the part scripts (customisation v2, P1.M1; SPEC 4.4; CR2).

Reads the text files of every core/data/parts/<dir>/ (all dirs but catalog/: one per slot, e.g. hat/,
or a group of slots, e.g. face/) and writes

    core/gen/parts_<dir>.c     a PartSet each (include/doomo/parts.h)
    core/gen/parts_index.c     the registry: every set, list -> set, the smallest scripted value

    python3 tools/parts_build.py [--out-root DIR]
    python3 tools/parts_build.py --selftest          check the rejects below

SOURCE FORMAT (one `#` comment per line allowed; coordinates are normalised, 256 = one radius,
stored in steps of 2; radii 0..255):

    stamp NAME W H            1bpp stamp, W,H 1..16, then H rows of . and X
    def NAME P1 P2 ..         a template (family grammar): op lines up to `enddef`, where $P1 ... are
                              replaced by the `use` arguments ($P+N, $P-N and $P*N also work)
    list FIELD VALUE          the script of one style: FIELD is a gene field of fields.txt or DF_x
    block group=G anchor=head|torso|frame lod=tiny,mid,big|all view=portrait,figure,fighter,chibi|all
      mat FIELD [light|dark]      anchor head|torso|frame    shade NAME      solid [noline,nocast,..,same]
      decal    mirror on|off    scale N (64 = 1.0)     region none|head|eye_l|eye_r|muzzle|torso
      ellipse X Y RX RY         capsule X0 Y0 X1 Y1 R0 R1      tri X0 Y0 X1 Y1 X2 Y2
      line X0 Y0 X1 Y1 C        dot X Y C              stamp NAME X Y [Q4]
      ifview V,.. / ifyaw 0,1,.. / ifanim blink,breath,twitch,wag   ...ops...   endif
      use NAME A1 A2 ..         a template's ops, inline
      -- CR2 macro ops (the family grammar; parts.h PO_CROWN ..):
      crown dome|cone|cylinder|flat|slouch|peaked|beret X Y W H
      brim none|bill|small|wide|floppy|cocked X Y W H
      band X Y W H              strap X0 Y0 X1 Y1 W       bow X Y S        ring X Y R T
      chain X0 Y0 X1 Y1 N SAG   frill X0 X1 Y N R         pouch X Y W H
    end

ANCHORS: head (256 = the head radius, projected on the head with its yaw), torso (256 = the torso
half size) and frame (CR3): a frame the slot's hook sets, in the hook's own units: one eye for pupils,
eye whites and eyes (256 = the eye radius, x mirrored for the left eye), a cheek for blush, a brow for
brows, the nose, the mouth (256 = the mouth half width), the pair of eyes for glasses (x = +-256 are
the eye centres, radii in eye radii) and one leg for legwear (x 256 = the leg radius, y 256 = the leg's
length from the foot up when negative). A frame block is drawn once per frame.

HOOKS: a list's script is drawn by its painter's hook (src/portrait_script.c, ps_slot), in the layer
groups that painter runs in (HOOKS below; "any" = the painter runs once and draws every block). A
block in a group its list's hook never runs is rejected; a list without a hook is only for VM demo
values (the catalog marks them state=test).

Rejected at build time: an unknown field, name, op or template, a coordinate outside -256..254, a
block over 255 bytes, a block in a group its list is never drawn in, a chibi block on a deep field
that is not chibi-visible (look_pool.c DFF_CHIBI: such parts cost nothing on chibis), a stamp bigger
than 16x16, and any solid part over 600 px at bust scale (estimated from its primitives at a nominal
bust head of 18x16 px / torso 18x15 px half sizes). Integer-only output, byte-identical on every machine.
"""
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
FIELDS = os.path.join(ROOT, "core", "data", "look", "fields.txt")
LOOKF = os.path.join(ROOT, "core", "include", "doomo", "look_fields.h")
LOOKPOOL = os.path.join(ROOT, "core", "src", "look_pool.c")
PL_DEEP0 = 128

OPS = ["END", "ANCHOR", "MAT", "SHADE", "SOLID", "DECAL", "ELLIPSE", "CAPSULE", "TRI", "LINE", "DOT",
       "STAMP", "MIRROR", "SCALE", "REGION", "IFVIEW", "IFYAW", "IFANIM",
       "CROWN", "BRIM", "BAND", "STRAP", "BOW", "RING", "CHAIN", "FRILL", "POUCH"]
OP = {n: i for i, n in enumerate(OPS)}
CROWNS = ["dome", "cone", "cylinder", "flat", "slouch", "peaked", "beret"]
BRIMS = ["none", "bill", "small", "wide", "floppy", "cocked"]
SHADES = ["ball", "flat", "vgrad", "gloss", "soft", "hgrad"]
PFLAGS = {"noline": 1, "nocast": 2, "nocont": 4, "fuzz": 8, "darkline": 16, "same": 0x80}

# The painter hooks (src/portrait_script.c "slot hooks"): list -> the layer groups its hook runs
# (draw_step order: 0 back, 1 body, 2 head base, 3 face, 4 neck / front hair, 5 hat / front, 6 finish).
# None = the painter runs once per image and draws every block of the script.
HOOKS = {
    "head_shape": {2}, "eye_style": {3, 4}, "brow_style": {4}, "nose_style": {3}, "mouth_style": {3},
    "facial_hair": {3}, "hair_style": {0, 4}, "ears": {0, 4}, "horns": {5}, "hat": {5}, "halo": {6},
    "accessory": {0, 4, 5}, "aura": {0, 6}, "top": {1, 4}, "bottom": {1}, "shoes": {1},
    "tail": None, "wings": None,
    # CR3: the gene's new fields (their painters set frames: an eye, a cheek, the pair of eyes, a leg)
    "pupil": {3}, "sclera": {3}, "blush_style": {3}, "glasses": {5}, "legwear": {1},
    "outer": {0, 1, 4}, "fur_pattern": {3},
}
REGIONS = ["none", "head", "eye_l", "eye_r", "muzzle", "torso"]
VIEWS = ["portrait", "figure", "fighter", "chibi"]
LODS = ["tiny", "mid", "big"]
ANIMS = ["blink", "breath", "twitch", "wag"]
MAX_PART_PX = 600
# bust-scale half sizes in px, measured on the bust (portrait.c setup_portrait: the head radius is
# 14 + head_size / 4 px, about 18 x 16 at the largest head; the torso about 18 x 15). CR2 corrected
# these from 36 x 42 / 20 x 26, which overstated every part's area about four times.
NOMINAL = {"head": (18, 16), "torso": (18, 15), "frame": (6, 6)}
# CR4 A3: a torso block that never draws on the bust (view= without portrait) meets the figure's torso at
# most (10 x 6 px half sizes, measured: pb_layout), not the bust's, so a tail or a wing is estimated at
# the size it is drawn
NOMINAL_TORSO_BODY = (10, 6)
ANCHORS = ["head", "torso", "frame"]


class PartError(Exception):
    pass


def field_ids():
    """gene fields by name (their GF_* id) and the deep fields DF_x (PL_DEEP0 + DF index)"""
    ids = {}
    n = 0
    for line in open(FIELDS):
        line = line.split("#", 1)[0].split()
        if line:
            ids[line[0]] = n
            n += 1
    m = re.search(r"enum\s*\{([^}]*DF_COUNT[^}]*)\}", open(LOOKF).read())
    for i, name in enumerate(x.strip() for x in m.group(1).split(",") if x.strip()):
        if name == "DF_COUNT":
            break
        ids[name] = PL_DEEP0 + i
    return ids


def chibi_deep():
    """deep fields that resolve for chibis (look_pool.c doomo_df_flags with DFF_CHIBI)"""
    return set(re.findall(r"\[(DF_\w+)\]\s*=\s*[^,\]]*DFF_CHIBI", open(LOOKPOOL).read()))


def expr(tok, args, where):
    """a template argument: $P (any word), or arithmetic on numbers: $P+N, $P-N, $P*N, -$P"""
    if "$" not in tok:
        return tok
    m = re.fullmatch(r"\$(\w+)", tok)
    if m and m.group(1) in args:                        # a plain argument: any word (a crown kind ...)
        return args[m.group(1)]
    t = re.sub(r"\$(\w+)", lambda m: str(args[m.group(1)]) if m.group(1) in args else "?", tok)
    if "?" in t or not re.fullmatch(r"[-+*0-9]+", t):
        raise PartError("%s: bad template argument %r" % (where, tok))
    v = 0
    for sign, num in re.findall(r"([-+]?)([0-9*]+)", t):
        prod = 1
        for f in num.split("*"):
            prod *= int(f)
        v += -prod if sign == "-" else prod
    return str(v)


def coord(tok, where):
    v = int(tok)
    if not -256 <= v <= 254:
        raise PartError("%s: coordinate %d outside -256..254" % (where, v))
    return (v // 2) & 255                              # int8 in steps of two


def radius(tok, where):
    v = int(tok)
    if not 0 <= v <= 255:
        raise PartError("%s: radius %d outside 0..255" % (where, v))
    return v


def mask_of(names, table, where):
    if names == "all":
        return (1 << len(table)) - 1
    m = 0
    for n in names.split(","):
        if n not in table:
            raise PartError("%s: unknown name %r (one of %s)" % (where, n, ", ".join(table)))
        m |= 1 << table.index(n)
    return m


class Area:
    """Pixel-area estimate of the solid part being built (bust scale)."""
    def __init__(self):
        self.px = 0.0
        self.worst = 0.0

    def close(self):
        self.worst = max(self.worst, self.px)
        self.px = 0.0


def compile_block(lines, stamps, fields, where, views=15):
    """lines: [(lineno, tokens)] of one block's body. Returns (op bytes, top): top is how far the
    block reaches above the head centre, in head units (0 for torso geometry)."""
    out = bytearray()
    ifs = []                                            # offsets of skip operands to patch
    anchor, mirror, scale = "head", 1, 1.0
    area = Area()
    top = [0]

    def nominal():
        return NOMINAL_TORSO_BODY if anchor == "torso" and not views & 1 else NOMINAL[anchor]

    def sx():
        return nominal()[0] / 256.0 * scale

    def sy():
        return nominal()[1] / 256.0 * scale

    def reach(y_top):
        """a shape whose highest point is normalised y_top"""
        if anchor == "head":
            top[0] = max(top[0], int(-y_top * scale))

    def ell(w_, h_, n=1.0):
        area.px += 3.14159 * w_ * sx() * h_ * sy() * mirror * n

    for ln, t in lines:
        w = "%s:%d" % (where, ln)
        k = t[0]
        try:
            if k == "anchor":
                anchor = t[1]
                if anchor not in NOMINAL:
                    raise PartError("%s: anchor must be head, torso or frame" % w)
                out += bytes([OP["ANCHOR"], ANCHORS.index(anchor)])
            elif k == "mat":
                if t[1] not in fields:
                    raise PartError("%s: unknown field %r" % (w, t[1]))
                tone = 0 if len(t) < 3 else {"light": 1, "dark": 2}[t[2]]
                out += bytes([OP["MAT"], fields[t[1]], tone])
            elif k == "shade":
                out += bytes([OP["SHADE"], SHADES.index(t[1])])
            elif k == "solid":
                fl = 0
                for n in (t[1].split(",") if len(t) > 1 else []):
                    fl |= PFLAGS[n]
                area.close()
                out += bytes([OP["SOLID"], fl])
            elif k == "decal":
                area.close()
                out += bytes([OP["DECAL"]])
            elif k == "mirror":
                mirror = 2 if t[1] == "on" else 1
                out += bytes([OP["MIRROR"], 1 if t[1] == "on" else 0])
            elif k == "scale":
                v = int(t[1])
                if not 1 <= v <= 255:
                    raise PartError("%s: scale outside 1..255" % w)
                scale = v / 64.0
                out += bytes([OP["SCALE"], v])
            elif k == "region":
                out += bytes([OP["REGION"], REGIONS.index(t[1])])
            elif k == "ellipse":
                out += bytes([OP["ELLIPSE"], coord(t[1], w), coord(t[2], w), radius(t[3], w), radius(t[4], w)])
                area.px += 3.14159 * int(t[3]) * sx() * int(t[4]) * sy() * mirror
                reach(int(t[2]) - int(t[4]))
            elif k == "capsule":
                out += bytes([OP["CAPSULE"]] + [coord(t[i], w) for i in range(1, 5)] +
                             [radius(t[5], w), radius(t[6], w)])
                r = (int(t[5]) + int(t[6])) / 2.0 * sx()
                ln_ = (((int(t[3]) - int(t[1])) * sx()) ** 2 + ((int(t[4]) - int(t[2])) * sy()) ** 2) ** 0.5
                area.px += (3.14159 * r * r + 2 * r * ln_) * mirror
                reach(min(int(t[2]), int(t[4])) - max(int(t[5]), int(t[6])))
            elif k == "tri":
                v = [int(x) for x in t[1:7]]
                out += bytes([OP["TRI"]] + [coord(x, w) for x in t[1:7]])
                area.px += abs((v[0] * (v[3] - v[5]) + v[2] * (v[5] - v[1]) + v[4] * (v[1] - v[3])) / 2.0) * sx() * sy() * mirror
                reach(min(v[1], v[3], v[5]))
            elif k == "line":
                out += bytes([OP["LINE"]] + [coord(t[i], w) for i in range(1, 5)] + [int(t[5])])
            elif k == "dot":
                out += bytes([OP["DOT"], coord(t[1], w), coord(t[2], w), int(t[3])])
            elif k == "stamp":
                if t[1] not in stamps:
                    raise PartError("%s: unknown stamp %r" % (w, t[1]))
                q = int(t[4]) if len(t) > 4 else 16
                if not 1 <= q <= 255:
                    raise PartError("%s: stamp scale outside 1..255" % w)
                out += bytes([OP["STAMP"], list(stamps).index(t[1]), coord(t[2], w), coord(t[3], w), q])
            elif k in ("crown", "brim"):
                kinds = CROWNS if k == "crown" else BRIMS
                if t[1] not in kinds:
                    raise PartError("%s: %s kind must be one of %s" % (w, k, ", ".join(kinds)))
                cx_, cy_, cw, ch = int(t[2]), int(t[3]), int(t[4]), int(t[5])
                out += bytes([OP[k.upper()], kinds.index(t[1]), coord(t[2], w), coord(t[3], w),
                              radius(t[4], w), radius(t[5], w)])
                if k == "crown":
                    ell(cw, ch, {"dome": 0.5, "cone": 0.64, "cylinder": 1.27, "flat": 0.64, "slouch": 0.65,
                                 "peaked": 0.8, "beret": 0.5}[t[1]])
                    reach(cy_ - ch * (2 if t[1] in ("cylinder", "cone", "peaked") else 1))
                else:
                    ell(cw * (3 if t[1] in ("wide", "floppy") else 2) // 2, ch, 1.0)
                    reach(cy_ - ch)
            elif k in ("band", "pouch"):
                out += bytes([OP[k.upper()], coord(t[1], w), coord(t[2], w), radius(t[3], w), radius(t[4], w)])
                area.px += 4 * int(t[3]) * sx() * int(t[4]) * sy() * mirror
                reach(int(t[2]) - int(t[4]))
            elif k == "strap":
                out += bytes([OP["STRAP"]] + [coord(t[i], w) for i in range(1, 5)] + [radius(t[5], w)])
                ln_ = (((int(t[3]) - int(t[1])) * sx()) ** 2 + ((int(t[4]) - int(t[2])) * sy()) ** 2) ** 0.5
                area.px += 2 * int(t[5]) * sx() * ln_ * mirror
                reach(min(int(t[2]), int(t[4])) - int(t[5]))
            elif k == "bow":
                out += bytes([OP["BOW"], coord(t[1], w), coord(t[2], w), radius(t[3], w)])
                ell(int(t[3]), int(t[3]), 1.6)
                reach(int(t[2]) - int(t[3]))
            elif k == "ring":
                out += bytes([OP["RING"], coord(t[1], w), coord(t[2], w), radius(t[3], w), radius(t[4], w)])
                area.px += 2 * 3.14159 * int(t[3]) * sx() * int(t[4]) * sy() * mirror
                reach(int(t[2]) - int(t[3]) - int(t[4]))
            elif k == "chain":
                n_ = int(t[5])
                if not 1 <= n_ <= 24:
                    raise PartError("%s: chain links 1..24" % w)
                out += bytes([OP["CHAIN"]] + [coord(t[i], w) for i in range(1, 5)] + [n_, radius(t[6], w)])
                reach(min(int(t[2]), int(t[4])))
            elif k == "frill":
                n_ = int(t[4])
                if not 1 <= n_ <= 24:
                    raise PartError("%s: frill scallops 1..24" % w)
                out += bytes([OP["FRILL"], coord(t[1], w), coord(t[2], w), coord(t[3], w), n_, radius(t[5], w)])
                ell(int(t[5]), int(t[5]), n_)
                reach(int(t[3]) - int(t[5]))
            elif k in ("ifview", "ifyaw", "ifanim"):
                table = {"ifview": VIEWS, "ifyaw": None, "ifanim": ANIMS}[k]
                if table is None:
                    m = 0
                    for n in t[1].split(","):
                        if not 0 <= int(n) <= 4:
                            raise PartError("%s: yaw outside 0..4" % w)
                        m |= 1 << int(n)
                else:
                    m = mask_of(t[1], table, w)
                out += bytes([OP[k.upper()], m, 0])
                ifs.append(len(out) - 1)
            elif k == "endif":
                if not ifs:
                    raise PartError("%s: endif without an if" % w)
                at = ifs.pop()
                skip = len(out) - at - 1
                if skip > 255:
                    raise PartError("%s: if body over 255 bytes" % w)
                out[at] = skip
            else:
                raise PartError("%s: unknown op %r" % (w, k))
        except (IndexError, ValueError, KeyError) as e:
            raise PartError("%s: bad operands for %r (%s)" % (w, k, e))
    if ifs:
        raise PartError("%s: if without an endif" % where)
    area.close()
    if area.worst > MAX_PART_PX:
        raise PartError("%s: a solid part is about %d px at bust scale (limit %d)" % (where, area.worst, MAX_PART_PX))
    if len(out) > 255:
        raise PartError("%s: block is %d bytes (limit 255)" % (where, len(out)))
    return bytes(out), top[0]


def parse(path, fields, templates=None):
    """-> (stamps {name: (w, h, bytes)} in file order, entries {(list, value): [block bytes]},
    tops {(list, value): head units above the centre}). `templates` collects `def`s (shared by every
    file of a dir, defined before use)."""
    stamps, entries, tops = {}, {}, {}
    templates = {} if templates is None else templates
    names = {v: k for k, v in fields.items()}
    deep_chibi = chibi_deep()
    cur = None
    block = None                                        # [(group, anchor, lodviews), [(ln, tokens)], where]
    tdef = None                                         # [name, params, lines] while inside a def
    where = os.path.relpath(path, ROOT) if path.startswith(ROOT) else os.path.basename(path)
    rows = None
    for ln, raw in enumerate(open(path), 1):
        line = raw.split("#", 1)[0].strip()
        w = "%s:%d" % (where, ln)
        if rows is not None:
            if not line:
                continue
            name, sw, sh, got = rows
            if len(line) != sw or set(line) - set(".X"):
                raise PartError("%s: stamp row must be %d of . and X" % (w, sw))
            got.append(line)
            if len(got) == sh:
                data = bytearray()
                for r in got:
                    bits = int("".join("1" if ch == "X" else "0" for ch in r), 2) << (-sw % 8)
                    data += bits.to_bytes((sw + 7) // 8, "big")
                stamps[name] = (sw, sh, bytes(data))
                rows = None
            continue
        if not line:
            continue
        t = line.split()
        if tdef is not None:
            if t[0] == "enddef":
                templates[tdef[0]] = (tdef[1], tdef[2])
                tdef = None
            elif t[0] in ("def", "list", "block", "end", "stamp"):
                raise PartError("%s: %r inside a def" % (w, t[0]))
            else:
                tdef[2].append((ln, t))
            continue
        if t[0] == "def" and block is None:
            if len(t) < 2 or t[1] in templates:
                raise PartError("%s: def NAME P1 .. (a new name)" % w)
            tdef = [t[1], t[2:], []]
        elif t[0] == "stamp" and block is None and len(t) == 4:
            sw, sh = int(t[2]), int(t[3])
            if not (1 <= sw <= 16 and 1 <= sh <= 16):
                raise PartError("%s: stamp is %dx%d (1..16 allowed)" % (w, sw, sh))
            if t[1] in stamps:
                raise PartError("%s: stamp %r defined twice" % (w, t[1]))
            rows = (t[1], sw, sh, [])
        elif t[0] == "list":
            if block is not None:
                raise PartError("%s: list inside a block" % w)
            if t[1] not in fields:
                raise PartError("%s: unknown field %r" % (w, t[1]))
            v = int(t[2])
            if not 0 <= v <= 254:
                raise PartError("%s: value outside 0..254" % w)
            cur = (fields[t[1]], v)
            if cur in entries:
                raise PartError("%s: list %s %s defined twice" % (w, t[1], t[2]))
            entries[cur] = []
            tops[cur] = 0
        elif t[0] == "block":
            if cur is None or block is not None:
                raise PartError("%s: block outside a list / inside a block" % w)
            kv = dict(x.split("=", 1) for x in t[1:])
            grp = int(kv.get("group", "0"))
            if not 0 <= grp <= 6:
                raise PartError("%s: group outside 0..6" % w)
            lname = names[cur[0]]
            if lname in HOOKS and HOOKS[lname] is not None and grp not in HOOKS[lname]:
                raise PartError("%s: %s is drawn in group(s) %s, never in %d" %
                                (w, lname, ",".join(str(x) for x in sorted(HOOKS[lname])), grp))
            if kv.get("anchor", "head") not in NOMINAL:
                raise PartError("%s: anchor must be head, torso or frame" % w)
            lv = mask_of(kv.get("lod", "all"), LODS, w) | (mask_of(kv.get("view", "all"), VIEWS, w) << 3)
            if cur[0] >= PL_DEEP0 and lname not in deep_chibi and (lv & 1) and (lv >> 3) & (1 << VIEWS.index("chibi")):
                raise PartError("%s: %s is not chibi-visible: give the block lod=mid,big or no chibi view" % (w, lname))
            block = [(grp, ANCHORS.index(kv.get("anchor", "head")), lv), [], w]
        elif t[0] == "end":
            if block is None:
                raise PartError("%s: end without a block" % w)
            body = list(block[1])
            if block[0][1] != 0:
                body.insert(0, (ln, ["anchor", ANCHORS[block[0][1]]]))
            ops, top = compile_block(body, stamps, fields, block[2], block[0][2] >> 3)
            g, a, lv = block[0]
            entries[cur].append(bytes([g, a, lv, len(ops)]) + ops)
            tops[cur] = max(tops[cur], top)
            block = None
        elif block is not None:
            if t[0] == "use":
                if len(t) < 2 or t[1] not in templates:
                    raise PartError("%s: unknown template %r" % (w, t[1] if len(t) > 1 else ""))
                params, body = templates[t[1]]
                if len(t) - 2 != len(params):
                    raise PartError("%s: %s takes %d arguments" % (w, t[1], len(params)))
                args = dict(zip(params, t[2:]))
                for bl, bt in body:
                    if bt[0] == "use":
                        raise PartError("%s: templates do not nest" % w)
                    block[1].append((ln, [expr(x, args, w) for x in bt]))
            else:
                block[1].append((ln, t))
        else:
            raise PartError("%s: unexpected %r" % (w, t[0]))
    if block is not None or rows is not None or tdef is not None:
        raise PartError("%s: unterminated block, stamp or def" % where)
    return stamps, entries, tops


PART_BLK = 2048


def lz_pack(raw):
    """The save codec (core/src/save.c): c < 0x80 = c+1 literals, c >= 0x80 = copy (c & 0x7F) + 3 bytes
    from the u16 LE distance that follows. One block is packed on its own (no history across blocks)."""
    out, lit, i, n = bytearray(), bytearray(), 0, len(raw)

    def flush():
        nonlocal lit
        while lit:
            k = min(len(lit), 128)
            out.append(k - 1)
            out.extend(lit[:k])
            del lit[:k]
    while i < n:
        best, bd = 0, 0
        for j in range(max(0, i - 65535), i):
            k = 0
            while i + k < n and k < 130 and raw[j + k] == raw[i + k]:
                k += 1
            if k > best:
                best, bd = k, i - j
        if best >= 4:                      # a 3-byte copy costs 3 bytes: only worth it from 4
            flush()
            out.append(0x80 | (best - 3))
            out += bytes([bd & 255, bd >> 8])
            i += best
        else:
            lit.append(raw[i])
            i += 1
    flush()
    return bytes(out)


def lz_unpack(c, n):
    out, i = bytearray(), 0
    while len(out) < n:
        b = c[i]
        i += 1
        if b < 0x80:
            out += c[i:i + b + 1]
            i += b + 1
        else:
            d = c[i] | (c[i + 1] << 8)
            i += 2
            for _ in range((b & 0x7F) + 3):
                out.append(out[-d])
    return bytes(out)


def lz_blocks(refs, ops, dirname):
    """[(raw start, bytes)]: whole entries packed greedily into blocks of at most PART_BLK bytes."""
    starts = [r[2] for r in refs] or [0]
    ends = starts[1:] + [len(ops)]
    blocks, b0 = [], 0
    for s, e in zip(starts, ends):
        if e - s > PART_BLK:
            raise PartError("%s: a script of %d B is over the %d B LZ block" % (dirname, e - s, PART_BLK))
        if e - b0 > PART_BLK:
            blocks.append((b0, bytes(ops[b0:s])))
            b0 = s
    blocks.append((b0, bytes(ops[b0:])))
    return blocks


def part_dirs():
    base = os.path.join(ROOT, "core", "data", "parts")
    return [d for d in sorted(os.listdir(base)) if d != "catalog" and os.path.isdir(os.path.join(base, d))]


def emit(dirname, fields):
    d = os.path.join(ROOT, "core", "data", "parts", dirname)
    if not re.fullmatch(r"[a-z][a-z0-9_]*", dirname):
        raise PartError("%s: a part dir name is lower case letters, digits and _" % dirname)
    stamps, entries, tops, templates = {}, {}, {}, {}
    for f in sorted(os.listdir(d)) if os.path.isdir(d) else []:
        if not f.endswith(".txt"):
            continue
        s, e, tp = parse(os.path.join(d, f), fields, templates)
        for k in s:
            if k in stamps:
                raise PartError("%s: stamp %r defined in two files" % (dirname, k))
        stamps.update(s)
        for k in e:
            if k in entries:
                raise PartError("%s: list entry %r defined in two files" % (dirname, k))
        entries.update(e)
        tops.update(tp)
    if len(stamps) > 255:
        raise PartError("%s: over 255 stamps in one set" % dirname)
    ops = bytearray()
    refs = []
    for key in sorted(entries):
        refs.append((key[0], key[1], len(ops)))
        for b in entries[key]:
            ops += b
        ops += bytes([0xFF, 0, 0, 0])
    lists = []
    for i, r in enumerate(refs):
        if not lists or lists[-1][0] != r[0]:
            lists.append([r[0], i, 0])
        lists[-1][2] += 1
    if len(ops) >= 65536:
        raise PartError("%s: scripts over 64 KB" % dirname)
    if not ops:
        ops = bytearray([0xFF, 0, 0, 0])
    bits, srefs = bytearray(), []
    for name, (w, h, data) in stamps.items():
        srefs.append((w, h, len(bits)))
        bits += data
    if not bits:
        bits = bytearray([0])

    blocks = lz_blocks(refs, ops, dirname)
    z, zofs, rofs = bytearray(), [], []
    for start, raw in blocks:
        rofs.append(start)
        zofs.append(len(z))
        c = lz_pack(raw)
        assert lz_unpack(c, len(raw)) == raw
        z += c
    rofs.append(len(ops))
    zofs.append(len(z))
    if len(z) >= 65536:
        raise PartError("%s: LZ scripts over 64 KB" % dirname)

    def arr(b):
        return ",\n".join("    " + ", ".join("0x%02x" % x for x in b[i:i + 16]) for i in range(0, len(b), 16)) + ","

    L = ["/* parts_%s.c - GENERATED by tools/parts_build.py from core/data/parts/%s/." % (dirname, dirname),
         " * DO NOT EDIT: edit the text files and run `python3 tools/parts_build.py`. */",
         '#include "doomo/doomo.h"', '#include "doomo/parts.h"', "",
         "static const PartRef refs[] = {"]
    L += ["    { %d, %d, %d }," % (r[1], min(255, (tops[(r[0], r[1])] + 7) // 8), r[2]) for r in refs] or ["    { 0, 0, 0 },"]
    L += ["};", "static const PartList lists[] = {"]
    L += ["    { %d, %d, %d }," % tuple(x) for x in lists] or ["    { 0xFFFF, 0, 0 },"]
    L += ["};", "#ifndef DOOMO_PARTS_LZ", "static const uint8_t ops[] = {", arr(ops), "};", "#endif",
          "static const StampRef stamps[] = {"]
    L += ["    { %d, %d, %d }," % r for r in srefs] or ["    { 0, 0, 0 },"]
    L += ["};", "static const uint8_t bits[] = {", arr(bits), "};", "",
          "#if defined(DOOMO_PARTS_LZ) || defined(DOOMO_PARTS_LZ_TEST)",
          "/* %d LZ block(s) of at most %d raw bytes, each ending on an entry boundary */" % (len(blocks), PART_BLK),
          "static const uint8_t z[] = {", arr(z), "};",
          "static const uint16_t zofs[] = { %s };" % ", ".join(str(x) for x in zofs),
          "static const uint16_t rofs[] = { %s };" % ", ".join(str(x) for x in rofs),
          "const PartLz doomo_parts_%s_lz = { z, zofs, rofs, %d };" % (dirname, len(blocks)),
          "#endif", "#ifdef DOOMO_PARTS_LZ",
          "const PartSet doomo_parts_%s = { refs, %d, 0, stamps, %d, bits, &doomo_parts_%s_lz, lists, %d };" % (dirname, len(refs), len(srefs), dirname, len(lists)),
          "#elif defined(DOOMO_PARTS_LZ_TEST)   /* host tests: the raw form, and the LZ form to check */",
          "const PartSet doomo_parts_%s = { refs, %d, ops, stamps, %d, bits, &doomo_parts_%s_lz, lists, %d };" % (dirname, len(refs), len(srefs), dirname, len(lists)),
          "#else",
          "const PartSet doomo_parts_%s = { refs, %d, ops, stamps, %d, bits, 0, lists, %d };" % (dirname, len(refs), len(srefs), len(lists)),
          "#endif", ""]
    minv = {}
    for r in refs:
        minv[r[0]] = min(minv.get(r[0], 255), r[1])
    return "\n".join(L), len(refs), len(ops), len(z), len(blocks), minv


def emit_index(dirs, minvs):
    """core/gen/parts_index.c: the registry of every set"""
    owner = {}
    for i, d in enumerate(dirs):
        for lst in minvs[d]:
            if lst in owner:
                raise PartError("list %d has scripts in two dirs (%s, %s): keep a list in one dir" % (lst, dirs[owner[lst]], d))
            owner[lst] = i
    allmin = {}
    for d in dirs:
        allmin.update(minvs[d])
    L = ["/* parts_index.c - GENERATED by tools/parts_build.py: the registry of the part-script sets.",
         " * DO NOT EDIT: add a dir under core/data/parts/ and run `python3 tools/parts_build.py`. */",
         '#include "doomo/doomo.h"', '#include "doomo/parts.h"', ""]
    L += ["extern const PartSet doomo_parts_%s;" % d for d in dirs]
    L += ["", "const PartSet *const doomo_part_scripts[%d] = {" % max(1, len(dirs))]
    L += ["    &doomo_parts_%s," % d for d in dirs] or ["    0,"]
    L += ["};", "const uint8_t doomo_part_script_count = %d;" % len(dirs), "",
          "const PartListSet doomo_part_script_lists[%d] = {" % max(1, len(owner))]
    L += ["    { %d, %d, %d }," % (lst, owner[lst], allmin[lst]) for lst in sorted(owner)] or ["    { 0xFFFF, 0, 255 },"]
    L += ["};", "const uint8_t doomo_part_script_nlists = %d;" % len(owner), "",
          "/* the smallest scripted value of each gene list (0xFF: none) */",
          "const uint8_t doomo_part_minv[128] = {"]
    mv = [allmin.get(i, 255) for i in range(128)]
    L += ["    " + ", ".join(str(x) for x in mv[i:i + 16]) + "," for i in range(0, 128, 16)]
    L += ["};", ""]
    if len(dirs) > 255:
        raise PartError("over 255 part dirs")
    return "\n".join(L)


def selftest():
    import tempfile
    fields = field_ids()
    cases = [
        ("big part", "list eye_style 1\nblock group=3\n mat skin\n solid\n ellipse 0 0 255 255\nend\n"),
        ("bad coordinate", "list eye_style 1\nblock group=3\n ellipse 400 0 5 5\nend\n"),
        ("bad field", "list nope 1\n"),
        ("bad op", "list eye_style 1\nblock group=3\n frob 1\nend\n"),
        ("bad stamp", "stamp s 20 8\n"),
        ("open if", "list eye_style 1\nblock group=3\n ifview chibi\n dot 0 0 2\nend\n"),
        ("hook group", "list hat 30\nblock group=2\n dot 0 0 2\nend\n"),
        ("chibi deep", "list DF_LASH 5\nblock group=3 lod=all view=all\n dot 0 0 2\nend\n"),
        ("bad crown", "list hat 30\nblock group=5\n crown tophat 0 -200 100 100\nend\n"),
        ("unknown template", "list hat 30\nblock group=5\n use nope 1\nend\n"),
        ("template args", "def t A B\n dot $A $B 2\nenddef\nlist hat 30\nblock group=5\n use t 1\nend\n"),
        ("big crown", "list hat 30\nblock group=5\n solid\n crown cylinder 0 0 255 255\nend\n"),
        ("big torso part on the bust", "list tail 30\nblock group=1 anchor=torso view=all\n scale 255\n mat skin\n"
         " solid\n ellipse 0 0 100 100\nend\n"),
    ]
    ok = 0
    for name, text in cases:
        with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as f:
            f.write(text)
        try:
            parse(f.name, fields)
            print("selftest: %s was NOT rejected" % name)
            return 1
        except PartError:
            ok += 1
        finally:
            os.unlink(f.name)
    print("selftest: %d rejects OK" % ok)
    # a template expands to the same bytes as the ops written out, and the top extent is measured
    with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as f:
        f.write("def peak X H\n mat hat_color\n solid\n tri -$X -200 $X -200 0 -200-$H\nenddef\n"
                "list hat 40\nblock group=5\n use peak 60 50\nend\n"
                "list hat 41\nblock group=5\n mat hat_color\n solid\n tri -60 -200 60 -200 0 -250\nend\n")
    try:
        _, e, tp = parse(f.name, fields)
    finally:
        os.unlink(f.name)
    hat = fields["hat"]
    if e[(hat, 40)] != e[(hat, 41)] or tp[(hat, 40)] != 250:
        print("selftest: a template did not expand like the written-out ops")
        return 1
    print("selftest: templates and top extents OK")
    # CR4 A3: the same torso part is fine when the block never draws on the bust (the figure's torso is smaller)
    with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as f:
        f.write("list tail 30\nblock group=1 anchor=torso view=figure,fighter,chibi\n scale 255\n mat skin\n"
                " solid\n ellipse 0 0 100 100\nend\n")
    try:
        parse(f.name, fields)
    except PartError as e:
        print("selftest: a torso block off the bust was rejected: %s" % e)
        return 1
    finally:
        os.unlink(f.name)
    print("selftest: torso blocks off the bust are estimated on the figure torso OK")
    # the LZ blocks: round trip, whole entries per block, none over PART_BLK
    import random
    rng = random.Random(7)
    ops, refs = bytearray(), []
    for k in range(40):
        refs.append((0, k, len(ops)))
        body = bytes(rng.choice((0, 1, 2, 0x1e, 0xf6)) for _ in range(rng.randrange(20, 240)))
        ops += body + bytes([0xFF, 0, 0, 0])
    blocks = lz_blocks(refs, ops, "selftest")
    ends = [r[2] for r in refs] + [len(ops)]
    for start, raw in blocks:
        if start not in ends or start + len(raw) not in ends or len(raw) > PART_BLK:
            print("selftest: LZ block is not on entry boundaries")
            return 1
        if lz_unpack(lz_pack(raw), len(raw)) != raw:
            print("selftest: LZ round trip failed")
            return 1
    if b"".join(r for _, r in blocks) != bytes(ops) or len(blocks) < 2:
        print("selftest: LZ blocks do not tile the ops")
        return 1
    print("selftest: LZ %d blocks OK" % len(blocks))
    return 0


def main(argv):
    root = ROOT
    i = 1
    while i < len(argv):
        if argv[i] == "--out-root" and i + 1 < len(argv):
            root = os.path.abspath(argv[i + 1])
            i += 2
        elif argv[i] == "--selftest":
            return selftest()
        else:
            print("usage: parts_build.py [--out-root DIR | --selftest]", file=sys.stderr)
            return 2
    fields = field_ids()
    try:
        dirs = part_dirs()
        minvs = {}
        os.makedirs(os.path.join(root, "core", "gen"), exist_ok=True)
        for d in dirs:
            text, n, size, zsize, nblk, minvs[d] = emit(d, fields)
            with open(os.path.join(root, "core", "gen", "parts_%s.c" % d), "w") as f:
                f.write(text)
            print("parts_build: %s: %d scripts, %d B (%d B in %d LZ blocks) -> core/gen/parts_%s.c" % (d, n, size, zsize, nblk, d))
        with open(os.path.join(root, "core", "gen", "parts_index.c"), "w") as f:
            f.write(emit_index(dirs, minvs))
    except PartError as e:
        print("parts_build: error: %s" % e, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
