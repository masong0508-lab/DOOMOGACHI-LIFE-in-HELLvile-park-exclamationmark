"""amb.py - a set's `amb` layers (SETS.md sec.6) -> the SHWR_AMB record (show_amb.c).

Record SHWR_AMB = 36: n items, each
    u8 kind, u8 flags, u16 bytes (4-aligned, this header included), u16 plane id, i16 params[...]
kind (AMB_K_*):
    1 DRIFT  vx_q8, vy_q8                                   the plane slides, wrapping (TILE_X)
    2 WAVE   amp, period, len, strip, row0, row1            rows shear (clipped bands)
    3 SWAY   amp, period, k, lag, dy                        a prop plane swings
    4 FLIP   img0, n, x, y, speed_q8, phase, hold[n]        a looping drawing on a plane
    5 MOTES  type, n, x, y, w, h, speed_q8, seed_lo, seed_hi, c_core, c_hi, c_dim
    6 LAMPS  seed_lo, seed_hi, hold_lo, hold_hi, dim, bloom, ring colour, nlamps,
             per lamp: x, y, w, h, nruns, nruns * (dx, dy, len, colour)   (dim = the runs; bloom = a ring)
    7 PCYCLE (deferred: DEFERRED.md)
flags: 1 FRONT (motes after the actors), 2 HIDDEN (a variant's show= turns it on: not yet).
x/y/w/h are in the plane image's own pixels (the plate's px), or show px when plane = 0xFFFF.
Crowds (`kind=crowd`) become CRWD records (lane + SHCF_WRAP walkers, scatter + bob idlers).
"""
import struct

SHWR_AMB = 36
K = {"drift": 1, "wave": 2, "sway": 3, "flip": 4, "motes": 5, "lamps": 6, "pcycle": 7}
MOTE = {"firefly": 0, "sparkle": 1, "ember": 2, "dust": 3, "petal": 4, "snow": 5, "bubble": 6, "leaf": 7}
NOPLANE = 0xFFFF
SHCF_WRAP = 16
AMB_K = 3                      # looks per ambient crowd (each look = 4 dirs x 4 frames of chibi in VRAM)


def _num(v, d=0.0):
    try:
        return float(str(v).replace("doc:", ""))
    except (TypeError, ValueError):
        return d


def _xy(v):
    a = str(v or "0,0").split(",")
    return [int(round(_num(x))) for x in a]


def _rng(v, d):
    if not v:
        return d
    a, _, b = str(v).partition("..")
    return int(_num(a)), int(_num(b or a))


def item(kind, plane, params, flags=0):
    body = struct.pack("<%dh" % len(params), *[max(-32768, min(32767, int(p))) for p in params])
    n = 6 + len(body)
    n += -n % 4
    return struct.pack("<BBHH", kind, flags, n, plane) + body + b"\0" * (n - 6 - len(body))


def _colours():
    from .art.motion import ambient as A
    from .art.motion import render as R
    out = {}
    for typ, roles in A.MOTE_COLOURS.items():
        out[typ] = [int(R.parse_idx(roles[r])) for r in ("core", "hi", "dim")]
    return out


def lamp_runs(img):
    """lantern cores in a plane image -> [(x, y, w, h, [(dx, dy, len, colour)])] (dim recolour runs)"""
    from .art.motion import render as R
    from .art.setkit import compose as C
    pal = C._g().pal
    c7, c5, wh = pal.ramp("CANDY", 7), pal.ramp("CANDY", 5), pal.WHITE
    out = []
    for ys, xs, vals in R.lamp_components(img):
        x0, y0 = int(xs.min()), int(ys.min())
        w, h = int(xs.max()) - x0 + 1, int(ys.max()) - y0 + 1
        runs = []
        pix = {(int(y), int(x)): int(v) for y, x, v in zip(ys, xs, vals)}
        for yy in range(y0, y0 + h):
            x = x0
            while x < x0 + w:
                v = pix.get((yy, x))
                if v is None:
                    x += 1
                    continue
                col = c7 if v == wh else c5
                n = 1
                while pix.get((yy, x + n)) == v:
                    n += 1
                runs.append((x - x0, yy - y0, n, col))
                x += n
        out.append((x0, y0, w, h, runs[:24]))
    return out


def compile_set(stage, sd, var, planes):
    """stage: emit.Stage; sd: ART's set; var: compose.Variant; planes: {plane/prop id: info}
    info = dict(pid, at=(x, y) show px, px, img, kind). Returns (items, n, crowds, tile_x_pids)."""
    from .art.setkit import compose as C
    items, crowds, tile_x = [], [], set()
    cols = None

    def local(info, x, y):
        px = max(1, info["px"])
        return (int(round((x - info["at"][0]) / px)), int(round((y - info["at"][1]) / px)))

    for st in getattr(sd, "ambs", []):
        kind = st.get("kind")
        if C.plane_hidden(st, var):
            continue
        try:
            pinfo = planes.get(st.get("plane")) if st.get("plane") else None
            if kind in ("drift", "wave", "lamps", "pcycle", "flip") and st.get("plane") and pinfo is None:
                stage.note("warning", "amb %s: no plane %s drawn in this shot" % (st.id, st.get("plane")))
                continue
            if kind == "drift":
                vx = int(round(_num(st.get("vx", 0)) * 256))
                vy = int(round(_num(st.get("vy", 0)) * 256))
                items.append(item(1, pinfo["pid"], [vx, vy]))
                tile_x.add(pinfo["pid"])
            elif kind == "wave":
                img = pinfo.get("img")
                hgt = img.shape[0] if img is not None else 180
                r0, r1 = _rng(st.get("rows"), (0, hgt))
                items.append(item(2, pinfo["pid"], [int(_num(st.get("amp", 1))), int(_num(st.get("period", 120))),
                                                    int(_num(st.get("len", 90))), max(1, int(_num(st.get("strip", 2)))),
                                                    r0, min(r1, hgt)]))
            elif kind == "sway":
                ids = [p for p in str(st.get("props", "")).split(",") if p]
                for k, prop in enumerate(ids):
                    for key, info in planes.items():
                        if info.get("prop") == prop:
                            items.append(item(3, info["pid"], [int(_num(st.get("amp", 1))),
                                                               int(_num(st.get("period", 60))), k,
                                                               int(_num(st.get("lag", 0))),
                                                               int(_num(st.get("dy", 0)))]))
            elif kind == "flip":
                import numpy as np
                if stage.c_images_left() < 2:
                    stage.note("warning", "amb %s: no image slots left" % st.id)
                    continue
                from . import flips as _fl
                import sys as _sys
                if _fl.SHOW not in _sys.path:
                    _sys.path.insert(0, _fl.SHOW)
                from art.gen import bake as _gbake
                cv = _gbake(st.get("art"))
                frames = [np.asarray(f.a, np.uint8) for f in cv.meta.get("frames", [cv])]
                holds = list(cv.meta.get("holds", [8] * len(frames)))
                x, y = _xy(st.get("at"))
                h0, w0 = frames[0].shape
                if st.get("anchor", "bottom") == "bottom":
                    x, y = x - w0 // 2, y - h0
                lx, ly = local(pinfo, x, y)
                img0 = stage.add_images(frames)
                if img0 < 0:
                    continue
                items.append(item(4, pinfo["pid"], [img0, len(frames), lx, ly,
                                                    int(round(_num(st.get("speed", 1)) * 256)),
                                                    int(_num(st.get("phase", 0)))] + holds))
            elif kind == "motes":
                if cols is None:
                    cols = _colours()
                typ = st.get("type", "dust")
                x, y, w, h = (_xy(st.get("area")) + [0, 0, 64, 64])[:4]
                seed = int(_num(st.get("seed", 1)))
                front = 1 if st.get("order") == "front" else 0
                if pinfo is not None:
                    lx, ly = local(pinfo, x, y)
                    pid = pinfo["pid"]
                    px = max(1, pinfo["px"])
                    w, h = max(1, w // px), max(1, h // px)
                else:
                    lx, ly, pid = x, y, NOPLANE
                c = cols.get(typ, cols["dust"])
                items.append(item(5, pid, [MOTE.get(typ, 3), int(_num(st.get("n", 8))), lx, ly, max(1, w), max(1, h),
                                           int(round(_num(st.get("speed", 1)) * 256)), seed & 0xFFFF, seed >> 16,
                                           c[0], c[1], c[2]], flags=front))
            elif kind == "lamps":
                srcs = []
                if pinfo is not None and pinfo.get("img") is not None:
                    srcs.append(pinfo)
                for prop in [p for p in str(st.get("props", "")).split(",") if p]:
                    srcs += [i for i in planes.values() if i.get("prop") == prop and i.get("img") is not None]
                lo, hi = _rng(st.get("hold"), (4, 14))
                seed = int(_num(st.get("seed", 1)))
                for info in srcs:
                    lamps = lamp_runs(info["img"])[:40]
                    if not lamps:
                        continue
                    from .art.setkit import compose as C2
                    pr = [seed & 0xFFFF, seed >> 16, lo, hi, int(_num(st.get("dim", 15))),
                          int(_num(st.get("bloom", 12))), C2._g().pal.ramp("CANDY", 6), len(lamps)]
                    for x, y, w, h, runs in lamps:
                        pr += [x, y, w, h, len(runs)]
                        for r in runs:
                            pr += list(r)
                    items.append(item(6, info["pid"], pr))
            elif kind == "crowd":
                crowds.append(st)
            elif kind == "pcycle":
                pass                                    # DEFERRED.md: CLUT rotation
        except Exception as e:                          # noqa: BLE001 - an amb never sinks a shot
            stage.note("warning", "amb %s (%s): %s: %s" % (st.id, kind, type(e).__name__, e))
    return items, crowds, tile_x


def add_crowds(sc, crowds, warn):
    """`amb kind=crowd` -> CRWD records on a ShotCast (walkers wrap; idlers bob)"""
    for st in crowds:
        try:
            n = max(1, int(_num(st.get("n", 4))))
            ks = sc.bible.kset_looks(st.get("kset"))[:max(1, min(n, AMB_K))]   # VRAM: few looks, reused
            n = max(1, int(_num(st.get("n", 4))))
            x0, x1 = _rng(st.get("x"), (0, 320))
            y = int(_num(st.get("y", 170)))
            seed = int(_num(st.get("seed", 1)))
            if st.get("form", "walk") == "walk":
                spd = _num(st.get("speed", 0.4))
                span = (x1 - x0) + 40
                nr = (n + 1) // 2
                for half, cnt, v in ((0, nr, spd), (1, n - nr, -spd)):
                    if cnt <= 0:
                        continue
                    i = sc.crowd("%s_%d" % (st.id, half), ks, cnt, form="lane", seed=seed + half,
                                 at=(x0 - 20 + (span // (2 * cnt)) * half, y + 2 * half), spacing=span / cnt,
                                 speed=v, dir="R" if v > 0 else "L")
                    sc.crowds[i][1]["flags"] |= SHCF_WRAP
                    sc.crowds[i][1]["w"] = span
                    sc.crowds[i][1]["x"] = x0 - 20
            else:
                depth = int(_num(st.get("depth", 6)))
                sc.crowd(st.id, ks, n, form="scatter", seed=seed, area=(x0, y - depth, max(1, x1 - x0), max(1, depth)),
                         flipmix=True, bob=True)
        except Exception as e:                          # noqa: BLE001
            warn("amb crowd %s: %s" % (st.id, e))
