"""budget.py - the VRAM page budget of every set WITH its ambient layers.

The set pool is 5 8bpp texture pages (256x256 texels each, setkit.check). An
ambient layer costs:
  drift / wave / sway / pcycle   0 texels (an offset, row strips, a CLUT rewrite)
  plane (new clouds/fog/haze)    its opaque bbox (counted by setkit.check)
  flip                           every drawing's bbox (all resident; no streaming)
  lamps                          per lamp: dim + bloom overlays (bbox x 2)
  motes                          0 texels (flat TILE prims, <= 4 per mote)
  crowd                          0 set-pool texels: chibis live in the portrait
                                 pool (384 B blocks), counted separately here
Prims per frame are estimated too (the 2D layer's budget is ~2,000 SPRTs).

    python3 -m showtk.art.motion.budget [--md]
"""
import sys

from ..setkit.setfile import Library, split_list
from ..setkit import check as CK
from ..setkit import compose as C
from .render import lamp_components

PAGE, POOL = CK.PAGE, CK.POOL_PAGES


def amb_cost(lib, sd):
    tex = prims = chibi = 0
    for a in getattr(sd, "ambs", []):
        k = a.get("kind")
        if k == "flip":
            fb = C.bake(a.get("art"))
            for f in fb.meta.get("frames", [fb]):
                x0, y0, x1, y1 = f.bbox()
                tex += (x1 - x0) * (y1 - y0)
            prims += 1
        elif k == "lamps":
            n = 0
            if a.get("plane"):
                pl = next((p for p in sd.planes if p.id == a.get("plane")), None)
                src = pl and (pl.get("image") or pl.get("tilemap") or pl.get("tile"))
                if src:
                    n += len(lamp_components(C.bake(src).a))
            for pid in split_list(a.get("props")):
                pr = next((p for p in sd.props if p.id == pid), None)
                if pr is not None:
                    n += len(lamp_components(C.bake(pr.get("art")).a))
            tex += n * 2 * 24
            prims += n
        elif k == "motes":
            prims += int(a.get("n", 12)) * 4
        elif k == "crowd":
            prims += int(a.get("n", 8))
            chibi += min(int(a.get("n", 8)), 48) * 4      # looks x the dirs in use
        elif k == "wave":
            prims += 240 // int(a.get("strip", 2))
    return tex, prims, chibi


def table(lib):
    rows = []
    for sid, sd in sorted(lib.sets.items()):
        if sd.kind != "2d":
            rows.append((sid, sd.kind, 0, 0, 0, 0, len(getattr(sd, "ambs", []))))
            continue
        base = CK.vram_bytes(sd)
        tex, prims, chibi = amb_cost(lib, sd)
        rows.append((sid, sd.kind, base, tex, prims, chibi, len(sd.ambs)))
    return rows


def main(argv=None):
    argv = argv or sys.argv[1:]
    lib = Library().load_dir()
    rows = table(lib)
    md = "--md" in argv
    if md:
        print("| set | kind | plates (pages) | ambient texels | total pages | amb prims/f | chibi blocks | amb layers |")
        print("|---|---|---|---|---|---|---|---|")
    worst = 0
    for sid, kind, base, tex, prims, chibi, n in rows:
        pages = (base + tex) / PAGE
        worst = max(worst, pages)
        if md:
            print("| %s | %s | %.2f | %d | **%.2f** | %d | %d | %d |" % (sid, kind, base / PAGE, tex, pages, prims, chibi, n))
        else:
            flag = "OVER" if pages > POOL else ("warn" if pages > 4.5 else "ok")
            print("%-22s %-9s plates %.2f pg + amb %6d tx = %.2f/%d pages %-4s prims+%-4d chibi %-3d layers %d" % (
                sid, kind, base / PAGE, tex, pages, POOL, flag, prims, chibi, n))
    print("worst set: %.2f of %d pages" % (worst, POOL))
    return 0 if worst <= POOL else 1


if __name__ == "__main__":
    sys.exit(main())
