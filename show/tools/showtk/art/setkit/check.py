"""check.py - the ART package's set checks (run by tests/art and `setkit check`).

  SET-PARSE   every file reads; ids unique; blocks closed
  SET-GEN     every gen: call resolves, bakes, and is the right kind
  SET-SIZE    a plane's declared size= equals its baked size
  SET-REF     props sit on real planes; variants name real planes/props/states;
              strokes name real nibs; bands name real band lists
  SET-COVER   at camera 0 and at every named frame, shaken +-8 px, the 16:9
              picture is fully covered (no hole in the set)
  PIC-PAL     every composed frame of every variant is 100 % on-palette
              (AT5.3 style: the BGR555 of each pixel maps back to a palette entry)
  PLAN-VRAM   the set's texels (opaque bounding boxes of plates and props,
              unique tiles of tilemaps) fit the 5-page set pool (warn > 3 pages)
"""
import numpy as np

from . import gen
from .compose import compose, variants_of, MATTE, Variant, bake
from .setfile import xy, wh, split_list

PAGE = 256 * 256          # one 8bpp texture page, in texels (bytes)
POOL_PAGES = 5


class Report:
    def __init__(self):
        self.errors, self.warnings, self.info = [], [], []

    def err(self, where, rule, msg):
        self.errors.append("%s: [%s] %s" % (where, rule, msg))

    def warn(self, where, rule, msg):
        self.warnings.append("%s: [%s] WARN %s" % (where, rule, msg))

    def ok(self):
        return not self.errors

    def text(self):
        return "\n".join(self.errors + self.warnings + self.info +
                         ["%d errors, %d warnings" % (len(self.errors), len(self.warnings))])


def _gens(st):
    for k, v in st.args.items():
        if isinstance(v, str) and v.startswith("gen:"):
            yield k, v


def vram_bytes(sd):
    g = gen()
    total, seen = 0, set()
    for st in sd.planes + sd.props:
        for k, v in _gens(st):
            if v in seen or g.kind_of(v) != "image":
                continue
            seen.add(v)
            img = g.bake(v)
            if "tiles" in img.meta:
                total += len(img.meta["tiles"]) * 256
            else:
                x0, y0, x1, y1 = img.bbox()
                total += (x1 - x0) * (y1 - y0)
    return total


def palpal(idx):
    """Count off-palette pixels of an index frame through the BGR555 LUT."""
    pal = gen().pal
    rgb = pal.RGB[idx].astype(np.int32)
    lut = pal.rgb555_lut()
    key = (rgb[..., 0] >> 3) | ((rgb[..., 1] >> 3) << 5) | ((rgb[..., 2] >> 3) << 10)
    return int((lut[key] < 0).sum())


_SYMS = None


def repo_symbols():
    """Enum and #define names from the game headers the 3D set kinds use (read-only)."""
    global _SYMS
    if _SYMS is None:
        import os
        import re
        root = gen().pal.ROOT
        names = set()
        for rel in ("core/include/doomo/map.h", "core/include/doomo/home.h", "core/include/doomo/palette.h",
                    "core/gen/furn_ids.h"):
            p = os.path.join(root, rel)
            if os.path.exists(p):
                txt = re.sub(r"/\*.*?\*/", "", open(p).read(), flags=re.S)
                names.update(re.findall(r"\b([A-Z][A-Z0-9_]{2,})\b", txt))
        _SYMS = names
    return _SYMS


def check_3d(sd, rep):
    syms = repo_symbols()
    want = []
    if sd.kind in ("view3d", "overworld"):
        if not sd.args.get("map"):
            rep.err(sd.where, "SET-REF", "%s set %s needs map=MAPID_*" % (sd.kind, sd.id))
        want.append((sd.where, sd.args.get("map")))
        if not sd.cams:
            rep.err(sd.where, "SET-REF", "%s set %s has no cam3d rail" % (sd.kind, sd.id))
    if sd.kind == "room":
        for k in ("home", "room", "preset"):
            want.append((sd.where, sd.args.get(k)))
        for o in sd.objs:
            want.append((o.where, o.get("furn")))
    for v in sd.variants:
        if v.get("tint"):
            want.append((v.where, v.get("tint")))
    for where, sym in want:
        if sym and sym not in syms:
            rep.err(where, "SET-REF", "unknown game symbol '%s'" % sym)


def check_set(lib, sd, rep, verbose=False):
    g = gen()
    n_err0 = len(rep.errors)
    where = sd.where
    planes = {p.id for p in sd.planes}
    props = {p.id: p for p in sd.props}
    if sd.kind not in ("2d", "view3d", "overworld", "room"):
        rep.err(where, "SET-PARSE", "unknown kind '%s'" % sd.kind)
    for st in sd.planes + sd.props:
        for k, v in _gens(st):
            try:
                out = g.bake(v)
            except Exception as e:  # noqa: BLE001
                rep.err(st.where, "SET-GEN", "%s=%s: %s: %s" % (k, v, type(e).__name__, e))
                continue
            kind = g.kind_of(v)
            if kind != "image":
                rep.err(st.where, "SET-GEN", "%s=%s is a %s generator, not an image" % (k, v, kind))
                continue
            if st.kw == "plane" and k in ("image", "tilemap", "tile") and st.get("size"):
                w, h = wh(st.get("size"))
                if (out.w, out.h) != (w, h):
                    rep.err(st.where, "SET-SIZE", "%s is %dx%d, size= says %dx%d" % (v, out.w, out.h, w, h))
            if out.a.dtype != np.uint8:
                rep.err(st.where, "PIC-PAL", "%s is not 8bpp indices" % v)
    for pr in sd.props:
        if pr.get("plane") not in planes:
            rep.err(pr.where, "SET-REF", "prop %s sits on unknown plane '%s'" % (pr.id, pr.get("plane")))
    for st in sd.strokes:
        for s in st.strokes:
            nib = (s.get("nib") or "").split(":", 1)[-1]
            if nib not in lib.nibs:
                rep.err(s.where, "SET-REF", "unknown nib '%s'" % s.get("nib"))
            try:
                pts = g.bake(s.get("path") if s.get("path", "").startswith("gen:") else "gen:" + s.get("path"))
                if not pts:
                    rep.err(s.where, "SET-GEN", "stroke %s has an empty path" % s.id)
            except Exception as e:  # noqa: BLE001
                rep.err(s.where, "SET-GEN", "stroke %s path: %s" % (s.id, e))
    for p in sd.planes:
        b = p.get("bands")
        if b and not b.startswith("sky:") and b not in lib.bands:
            rep.err(p.where, "SET-REF", "unknown bands '%s'" % b)
    for v in sd.variants:
        for k in ("hide", "show"):
            for t in split_list(v.get(k)):
                if t not in planes and t not in props:
                    rep.err(v.where, "SET-REF", "variant %s %s= names unknown '%s'" % (v.id, k, t))
        for t in split_list(v.get("state")):
            pid, _, stn = t.partition(":")
            if pid not in props:
                rep.err(v.where, "SET-REF", "variant %s state= names unknown prop '%s'" % (v.id, pid))
            elif "art." + stn not in props[pid].args:
                rep.err(v.where, "SET-REF", "prop %s has no art.%s" % (pid, stn))
        try:
            g.light.lut(v.get("light", "day"))
        except KeyError as e:
            rep.err(v.where, "SET-REF", str(e))
    if sd.kind != "2d":
        check_3d(sd, rep)
        return
    if len(rep.errors) > n_err0:
        return
    # coverage and PIC-PAL on every variant at every framing (+-8 px shake)
    from .sheet import frames_of
    cams = frames_of(sd)
    for var in variants_of(sd):
        for label, (cx, cy) in cams:
            for dx, dy in ((0, 0), (-8, -8), (8, 8), (-8, 8), (8, -8)):
                try:
                    idx = compose_holes(lib, sd, var, (cx + dx, cy + dy))
                except Exception as e:  # noqa: BLE001
                    rep.err(where, "SET-GEN", "compose %s/%s: %s: %s" % (var.id, label, type(e).__name__, e))
                    return
                crop = idx[MATTE:MATTE + 180]
                holes = int((crop == 0).sum())
                if holes and sd.args.get("matte", "open") != "black":
                    rep.err(where, "SET-COVER", "variant %s at %s %+d,%+d: %d uncovered pixels" % (
                        var.id, label, dx, dy, holes))
                    break
                off = palpal(crop)
                if off:
                    rep.err(where, "PIC-PAL", "variant %s at %s: %d off-palette pixels" % (var.id, label, off))
    vb = vram_bytes(sd)
    pages = vb / PAGE
    if pages > POOL_PAGES:
        rep.err(where, "PLAN-VRAM", "%s needs %.2f texture pages (%d B); the set pool has %d" % (
            sd.id, pages, vb, POOL_PAGES))
    elif pages > 3:
        rep.warn(where, "PLAN-VRAM", "%s needs %.2f texture pages (%d B)" % (sd.id, pages, vb))
    if verbose:
        rep.info.append("%-28s %d variants, %d framings, %.2f pages" % (
            sd.id, len(variants_of(sd)), len(cams), pages))


def compose_holes(lib, sd, var, cam):
    """compose() into a 0-filled frame so holes show up as index 0."""
    return compose(lib, sd, var, cam, base=0)


def check_all(lib, verbose=False):
    rep = Report()
    pal = gen().pal
    # palette sanity: every entry round-trips through BGR555
    lut = pal.rgb555_lut()
    for i in range(1, 256):
        if lut[int(pal.BGR555[i])] < 0:
            rep.err("palette", "PIC-PAL", "entry %d does not round-trip" % i)
    for name in gen().light.NAMES:
        t = gen().light.lut(name)
        if t.dtype != np.uint8 or len(t) != 256 or t[0] != 0:
            rep.err("light.py", "PIC-PAL", "lut %s is not a 256-entry index map with 0 -> 0" % name)
    for sid in sorted(lib.sets):
        check_set(lib, lib.sets[sid], rep, verbose)
    return rep
