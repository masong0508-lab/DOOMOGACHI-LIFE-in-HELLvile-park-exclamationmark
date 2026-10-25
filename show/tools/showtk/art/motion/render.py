"""render.py - draw a set with its ambient layers (and walking cast) at frame t.

Review previews only (the PS1 draws the masters): it follows setkit.compose
(planes in order, props after their plane, parallax `screen = at - cam*par`,
variant LUTs) and adds every `amb` kind with ambient.py's exact math.

    sa = SetAnim(lib, sd, variant)          # bakes once
    idx = sa.frame(t, cam=(x, y), actors=[...])   # uint8 (240, 320) master indices
"""
import numpy as np

from ..setkit import compose as C
from ..setkit.setfile import xy, split_list
from . import ambient as A

FRAME_W, FRAME_H, MATTE = 320, 240, 30


def _g():
    return C._g()


def parse_idx(tok):
    return _g().light.parse_index(tok)


def lamp_components(a):
    """Lantern cores / bulbs in an index image: connected blobs of CANDY.7 and
    WHITE holding both, 2..48 px. -> [(ys, xs, kinds)] in image coords."""
    from scipy import ndimage
    pal = _g().pal
    c7, wh = pal.ramp("CANDY", 7), pal.WHITE
    m = (a == c7) | (a == wh)
    lab, n = ndimage.label(m)
    out = []
    if n == 0:
        return out
    objs = ndimage.find_objects(lab)
    for k, sl in enumerate(objs, 1):
        sub = lab[sl] == k
        cnt = int(sub.sum())
        if cnt < 2 or cnt > 48:
            continue
        vals = a[sl][sub]
        if not ((vals == c7).any() and (vals == wh).any()):
            continue
        ys, xs = np.nonzero(sub)
        out.append((ys + sl[0].start, xs + sl[1].start, vals.copy()))
    return out


class Amb:
    def __init__(self, st):
        self.st, self.id, self.kind = st, st.id, st.get("kind")
        self.a = st.args

    def get(self, k, d=None):
        return self.st.get(k, d)


class SetAnim:
    def __init__(self, lib, sd, var=None, crowd_cache=None):
        self.lib, self.sd = lib, sd
        self.var = var or C.variants_of(sd)[0]
        self.px = sd.px
        g = _g()
        self.pal = g.pal
        self.ambs = [Amb(s) for s in getattr(sd, "ambs", [])]
        self.ambs = [a for a in self.ambs if not self._hidden(a)]
        self.by_plane = {}
        for a in self.ambs:
            self.by_plane.setdefault(a.get("plane", "__front__" if a.get("order") == "front" else None), []).append(a)
        self.planes = sorted(enumerate(sd.planes), key=lambda t: (C.ORDER.get(t[1].get("order", "back"), 0), t[0]))
        self.img = {}
        for _, st in self.planes:
            src = st.get("image") or st.get("tilemap") or st.get("tile")
            if src:
                self.img[st.id] = C.bake(self.var.over.get("image." + st.id, src))
        self.sway = {}
        for a in self.ambs:
            if a.kind == "sway":
                ids = split_list(a.get("props"))
                for k, pid in enumerate(ids):
                    self.sway[pid] = (a, k)
        self.lamps = {}
        self.crowd_cache = crowd_cache if crowd_cache is not None else {}
        for a in self.ambs:
            if a.kind == "lamps":
                self._prep_lamps(a)
            if a.kind == "crowd":
                self._prep_crowd(a)

    # ---- helpers -------------------------------------------------------------
    def _hidden(self, a):
        if a.id in self.var.show:
            return False
        if a.id in self.var.hide:
            return True
        return a.get("hidden", "no") == "yes"

    def _prep_lamps(self, a):
        c5, c7, c6 = self.pal.ramp("CANDY", 5), self.pal.ramp("CANDY", 7), self.pal.ramp("CANDY", 6)
        groups = []
        if a.get("plane"):
            img = self.img.get(a.get("plane"))
            if img is not None:
                for comp in lamp_components(img.a):
                    groups.append(("plane", a.get("plane"), comp))
        for pid in split_list(a.get("props")):
            pr = next((p for p in self.sd.props if p.id == pid), None)
            if pr is None or C.plane_hidden(pr, self.var):
                continue
            if self.var.states.get(pid) not in (None, "lit"):
                continue
            for comp in lamp_components(C.bake(pr.get("art")).a):
                groups.append(("prop", pr, comp))
        self.lamps[a.id] = (groups, c5, c7, c6)

    def _prep_crowd(self, a):
        from ..cast.lookdata import Bible
        from ..cast import host
        key = a.get("kset")
        if key in self.crowd_cache:
            return
        b = self.crowd_cache.get("__bible__") or Bible()
        self.crowd_cache["__bible__"] = b
        looks = [lb for _lbl, lb in b.kset_looks(key)]
        jobs = [host.job(lb, "chibi", a=d, b=f) for lb in looks for d in range(4) for f in range(4)]
        _pal, out = host.render(jobs)
        sprites = []
        k = 0
        for _ in looks:
            per = {}
            for d in range(4):
                for f in range(4):
                    w, h, px = out[k]
                    k += 1
                    per[(d, f)] = np.frombuffer(px, np.uint8).reshape(h, w)
            sprites.append(per)
        self.crowd_cache[key] = sprites

    def _lut(self, st):
        return C._lut_for(st, self.var)

    def _cycle_lut(self, st, base, t):
        cyc = [a for a in self.by_plane.get(st.id, []) if a.kind == "pcycle"]
        if not cyc:
            return base
        lut = np.arange(256, dtype=np.uint8) if base is None else base.copy()
        src = lut.copy()
        for a in cyc:
            idx = [parse_idx(x) for x in split_list(a.get("idx"))]
            s = A.pcycle_step(t, int(a.get("every", 6)), len(idx))
            for k, i in enumerate(idx):
                lut[i] = src[idx[(k + s) % len(idx)]]
        return lut

    # ---- one frame ------------------------------------------------------------
    def frame(self, t, cam=(0.0, 0.0), actors=(), base=1):
        out = np.full((FRAME_H, FRAME_W), base, np.uint8)
        drew_actors = False
        for _, st in self.planes:
            order = st.get("order", "back")
            if not drew_actors and C.ORDER.get(order, 0) > 0:
                self._actors(out, t, cam, actors)
                drew_actors = True
            if C.plane_hidden(st, self.var):
                continue
            self._plane(out, st, t, cam)
        if not drew_actors:
            self._actors(out, t, cam, actors)
        for a in self.by_plane.get("__front__", []):
            self._amb(out, a, t, cam, None)
        return out

    def _actors(self, out, t, cam, actors):
        for a in self.by_plane.get(None, []):       # no plane: crowds, motes behind the cast
            self._amb(out, a, t, cam, None)
        for ac in actors:
            ac.draw(out, t, cam)

    def _plane(self, out, st, t, cam):
        parx, pary = C.par_of(st)
        lut = self._cycle_lut(st, self._lut(st), t)
        ppx = int(st.get("px", self.px))
        ambs = self.by_plane.get(st.id, [])
        dx = dy = 0
        wave = None
        for a in ambs:
            if a.kind == "drift":
                dx += A.drift_off(A.q8(a.get("vx", 0)), t)
                dy += A.drift_off(A.q8(a.get("vy", 0)), t)
            elif a.kind == "wave":
                wave = a
        if st.kw == "strokes":
            C.draw_strokes(self.lib, self.sd, st, out, cam, parx, pary, lut)
            return
        if "bands" in st.args:
            spec = self.var.over.get("bands." + st.id, st.get("bands"))
            rows = C.band_rows(self.lib, spec)
            for y in range(FRAME_H):
                yy = int(y + cam[1] * pary)
                out[y, :] = rows[max(0, min(len(rows) - 1, yy))]
            if lut is not None:
                out[:] = lut[out]
            return
        img = self.img.get(st.id)
        if img is None:
            return
        tiled = "tile" in st.args or any(a.kind == "drift" for a in ambs)
        if tiled:
            tw, th = img.w * ppx, img.h * ppx
            if "tile" in st.args:
                ox = -((cam[0] * parx - dx) % tw)
                oy = -((cam[1] * pary - dy) % th)
                ys = []
                yy = oy
                while yy < FRAME_H:
                    ys.append(yy)
                    yy += th
            else:
                x0, y0 = C.plane_origin(st, img.w, img.h, ppx)
                ox = (x0 - cam[0] * parx + dx) % tw - tw
                ys = [y0 - cam[1] * pary + dy]
            for yy in ys:
                xx = ox
                while xx < FRAME_W:
                    self._put_plane(out, img, xx, yy, ppx, lut, wave, t)
                    xx += tw
        else:
            x0, y0 = C.plane_origin(st, img.w, img.h, ppx)
            self._put_plane(out, img, x0 - cam[0] * parx + dx, y0 - cam[1] * pary + dy, ppx, lut, wave, t)
        # props, then the plane's ambient sprites
        for pr in self.sd.props:
            if pr.get("plane") != st.id or C.plane_hidden(pr, self.var):
                continue
            art = pr.get("art")
            state = self.var.states.get(pr.id)
            if state:
                art = pr.get("art." + state, art)
            pimg = C.bake(art)
            plut = None if pr.get("light") == "fixed" else lut
            x, y = C.prop_xy(pr, pimg, ppx)
            sx = sy = 0
            if pr.id in self.sway:
                a, k = self.sway[pr.id]
                sx = A.sway_dx(int(a.get("amp", 1)), int(a.get("period", 90)), t, k, int(a.get("lag", 7)))
                if a.get("dy"):
                    sy = -abs(A.sway_dx(int(a.get("dy")), int(a.get("period", 90)) // 2, t, k, int(a.get("lag", 7))))
            C._put(out, pimg, x - cam[0] * parx + sx, y - cam[1] * pary + sy, ppx, plut)
            self._prop_lamps(out, pr, x - cam[0] * parx + sx, y - cam[1] * pary + sy, ppx, plut, t)
        for a in ambs:
            if a.kind in ("lamps", "flip", "motes", "crowd"):
                self._amb(out, a, t, cam, st)

    def _put_plane(self, out, img, x, y, ppx, lut, wave, t):
        if wave is None:
            C._put(out, img, x, y, ppx, lut)
            return
        amp, per, ln = int(wave.get("amp", 1)), int(wave.get("period", 120)), int(wave.get("len", 64))
        strip = int(wave.get("strip", 2))
        r0, r1 = A.rng2(wave.get("rows", "0..%d" % img.h))
        a = img.a
        for ry in range(0, img.h, strip):
            sub = a[ry:ry + strip]
            ddx = A.wave_dx(amp, per, ln, ry, t) if r0 <= ry < r1 else 0
            C._put(out, sub, x + ddx * ppx, y + ry * ppx, ppx, lut)

    def _lamp_draw(self, out, ys, xs, vals, ox, oy, ppx, lut, state, c5, c7, c6):
        if state == 0:
            return
        h, w = out.shape
        if state == 1:
            new = np.where(vals == self.pal.WHITE, c7, c5).astype(np.uint8)
            for y, x, v in zip(ys, xs, new):
                X, Y = int(round(ox + x * ppx)), int(round(oy + y * ppx))
                if 0 <= X < w and 0 <= Y < h:
                    out[Y, X] = lut[v] if lut is not None else v
        else:
            pts = set(zip(ys.tolist(), xs.tolist()))
            ring = set()
            for y, x in pts:
                for ddy, ddx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    q = (y + ddy, x + ddx)
                    if q not in pts:
                        ring.add(q)
            for y, x in ring:
                X, Y = int(round(ox + x * ppx)), int(round(oy + y * ppx))
                if 0 <= X < w and 0 <= Y < h:
                    out[Y, X] = c6

    def _prop_lamps(self, out, pr, x, y, ppx, lut, t):
        for aid, (groups, c5, c7, c6) in self.lamps.items():
            a = next(m for m in self.ambs if m.id == aid)
            hold = A.rng2(a.get("hold", "4..12"))
            for i, (kind, ref, comp) in enumerate(groups):
                if kind != "prop" or ref is not pr:
                    continue
                s = A.lamp_state(int(a.get("seed", 7)), i, t, hold, int(a.get("dim", 22)), int(a.get("bloom", 12)))
                self._lamp_draw(out, comp[0], comp[1], comp[2], x, y, ppx, lut, s, c5, c7, c6)

    def _amb(self, out, a, t, cam, st):
        parx, pary = C.par_of(st) if st is not None else (float(a.get("par", 1.0)), float(a.get("par", 1.0)))
        lut = self._cycle_lut(st, self._lut(st), t) if st is not None else None
        if a.kind == "lamps":
            if st is None:
                return                                   # prop lamps: drawn with their props
            groups, c5, c7, c6 = self.lamps[a.id]
            hold = A.rng2(a.get("hold", "4..12"))
            img = self.img.get(st.id)
            if img is None:
                return
            ppx = int(st.get("px", self.px))
            x0, y0 = C.plane_origin(st, img.w, img.h, ppx)
            for i, (kind, ref, comp) in enumerate(groups):
                if kind != "plane":
                    continue
                s = A.lamp_state(int(a.get("seed", 7)), i, t, hold, int(a.get("dim", 22)), int(a.get("bloom", 12)))
                self._lamp_draw(out, comp[0], comp[1], comp[2], x0 - cam[0] * parx, y0 - cam[1] * pary,
                                ppx, lut, s, c5, c7, c6)
        elif a.kind == "flip":
            fb = C.bake(a.get("art"))
            frames, holds = fb.meta.get("frames", [fb]), fb.meta.get("holds", [1])
            k = A.flip_frame(holds, t, A.q8(a.get("speed", 1.0)), int(a.get("phase", 0)))
            img = frames[k]
            ppx = int(st.get("px", self.px)) if st is not None else 1
            x, y = C.prop_xy(a.st, img, ppx)
            flut = None if a.get("light") == "fixed" else lut
            C._put(out, img, x - cam[0] * parx, y - cam[1] * pary, ppx, flut)
        elif a.kind == "motes":
            typ = a.get("type", "firefly")
            area = [int(v) for v in a.get("area", "0,0,320,180").split(",")]
            cols = {r: parse_idx(n) for r, n in A.MOTE_COLOURS[typ].items()}
            seed = int(a.get("seed", 1))
            sp = A.q8(a.get("speed", 1.0))
            for i in range(int(a.get("n", 12))):
                m = A.mote(typ, seed, i, t, area, sp)
                if m is None:
                    continue
                x, y, size, role = m
                X = int(round(x - cam[0] * parx))
                Y = int(round(y + MATTE - cam[1] * pary))
                c = cols[role]
                if typ == "firefly" and size == 1:
                    for ddx, ddy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                        self._dot(out, X + ddx, Y + ddy, cols["dim"])
                if size == 3:
                    for k in range(-3, 4):
                        self._dot(out, X + k, Y, c if abs(k) < 3 else cols["dim"])
                        self._dot(out, X, Y + k, c if abs(k) < 3 else cols["dim"])
                elif size == 1:
                    for ddx, ddy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                        self._dot(out, X + ddx, Y + ddy, c)
                elif size == 2:
                    for ddx, ddy in ((1, 0), (0, 1), (1, 1)):
                        self._dot(out, X + ddx, Y + ddy, cols["dim"])
                self._dot(out, X, Y, c)
        elif a.kind == "crowd":
            sprites = self.crowd_cache[a.get("kset")]
            x0, x1 = A.rng2(a.get("x", "0..320"))
            y = int(a.get("y", 150))
            seed = int(a.get("seed", 1))
            n = int(a.get("n", 8))
            members = []
            for i in range(n):
                if a.get("form", "walk") == "walk":
                    x, yy, d, f = A.walker(i, t, seed, x0, x1, y, A.q8(a.get("speed", 0.5)), int(a.get("step", 5)), n=n)
                    bob = 1 if f in (1, 3) else 0
                else:
                    x, yy, d, f, bob = A.idler(i, t, seed, x0, x1, y, int(a.get("depth", 10)))
                members.append((yy, x, d, f, bob, i))
            members.sort()
            px = int(a.get("px", 1))
            for yy, x, d, f, bob, i in members:
                spr = sprites[A.hash3(seed, i, 5) % len(sprites)][(d, f)]
                if px > 1:
                    spr = np.repeat(np.repeat(spr, px, 0), px, 1)
                h, w = spr.shape
                X = int(round(x - w / 2 - cam[0] * parx))
                Y = int(round(yy + MATTE - h - bob - cam[1] * pary))
                C._put(out, spr, X, Y, 1, lut)

    @staticmethod
    def _dot(out, x, y, c):
        if 0 <= x < out.shape[1] and 0 <= y < out.shape[0]:
            out[y, x] = c
