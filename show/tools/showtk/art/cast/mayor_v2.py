"""showtk.art.cast.mayor_v2 - review sheets for Mayor Giggs v2, the turkey
(ART track; review only).  Every pixel comes from the portrait engine plus
the show's part set (show/art/parts/show_parts.c) through the native host.

    python3 -m showtk.art.cast.mayor_v2 [--out DIR]

writes mayor_v2_variants.png (numbered options, 1 = the default in
series/looks/cast.shw) and mayor_v2_lineup.png (figure + chibi turnaround
next to Nana and Pip, px=2, for scale).
"""
import os
import sys

from . import host
from .catalog import catalog
from .lookdata import Bible, Look, parse_value, field_key
from .sheet import Canvas, DEFAULT_OUT

# LOCKED (creator, Oct 25): variant 1 "with no paper number on the hat" is
# the only mayor look; the alternates were dropped.
VARIANTS = [
    ("1 LOCKED lavender hat, peach head, teal coat, no card", {}),
]


def look_with(bible, over):
    vals = dict(bible.resolve_look("mayor", ()).values)
    for k, v in over.items():
        vals[field_key(k)] = parse_value(k, v, "mayor_v2")
    L = Look("v", "mayor_v2")
    L.values = vals
    return L.pack()


def main(argv):
    out = DEFAULT_OUT.replace("/cast", "/mayor_v2")
    if "--out" in argv:
        out = argv[argv.index("--out") + 1]
    os.makedirs(out, exist_ok=True)
    b = Bible()
    cat = catalog()
    # variants: figure px=2, bust px=2 (neutral, happy, A-open)
    jobs = []
    for _, over in VARIANTS:
        lb = look_with(b, over)
        jobs += [host.job(lb, "figure", anim=4), host.job(lb, "bust", anim=4),
                 host.job(lb, "bust", expr=cat.value("EXPR_HAPPY"), anim=12),
                 host.job(lb, "bust", mouth=cat.value("MOUTH_OPEN"), anim=20)]
    pal, recs = host.render(jobs)
    c = Canvas(len(VARIANTS) * 420, 200, pal)
    for i, (name, _) in enumerate(VARIANTS):
        x = 8 + i * 420
        c.text(x, 4, name)
        c.img(recs[i * 4], x, 18, 2)
        for j in range(3):
            c.img(recs[i * 4 + 1 + j], x + 104 + j * 100, 18, 2)
    p1 = os.path.join(out, "mayor_v2_variants.png")
    c.im.save(p1)
    # lineup + turnaround
    lj, labels = [], []
    for cid in ("nana", "mayor", "pip"):
        lj.append(host.job(b.char_look_bytes(cid), "figure", anim=4))
        labels.append(cid)
    m = b.char_look_bytes("mayor")
    for d in range(4):
        lj.append(host.job(m, "chibi", a=d, b=0))
    for f in range(4):
        lj.append(host.job(m, "chibi", a=1, b=f))
    pal, recs = host.render(lj)
    c = Canvas(3 * 104 + 8 * 40 + 40, 200, pal)
    for i in range(3):
        c.img(recs[i], 8 + i * 104, 18, 2)
        c.text(8 + i * 104, 4, labels[i])
    c.text(330, 4, "chibi: down left right up | walk left 0-3")
    for i in range(8):
        c.img(recs[3 + i], 330 + i * 40 + (8 if i >= 4 else 0), 30, 2)
    p2 = os.path.join(out, "mayor_v2_lineup.png")
    c.im.save(p2)
    # final: bust px=2 + figure px=2 per expression, chibi px=2 4 directions
    exprs = ["EXPR_NEUTRAL", "EXPR_HAPPY", "EXPR_ANGRY", "EXPR_SHOCK", "EXPR_SAD"]
    fj = []
    for e in exprs:
        v = cat.value(e)
        fj += [host.job(m, "bust", expr=v, anim=8), host.job(m, "figure", expr=v, anim=8)]
    for d in range(4):
        fj.append(host.job(m, "chibi", a=d, b=1))
    pal, recs = host.render(fj)
    bh = max(recs[2 * i][1] for i in range(len(exprs))) * 2
    fh = max(recs[2 * i + 1][1] for i in range(len(exprs))) * 2
    cw = max(r[0] for r in recs) * 2 + 8
    ch = max(r[1] for r in recs[2 * len(exprs):]) * 2
    y1, y2 = 32, 32 + bh + 8
    y3 = y2 + fh + 22
    c = Canvas(max(len(exprs) * cw + 16, 560), y3 + ch + 8, pal)
    c.text(8, 4, "MAYOR GIGGS (locked v1, no hat card): bust + figure per expression")
    for i, e in enumerate(exprs):
        x = 8 + i * cw
        c.text(x, 18, e[5:].lower())
        c.img(recs[2 * i], x, y1, 2)
        c.img(recs[2 * i + 1], x, y2, 2)
    c.text(8, y3 - 14, "chibi: down left right up")
    for d in range(4):
        r = recs[2 * len(exprs) + d]
        c.img(r, 8 + d * (r[0] * 2 + 8), y3, 2)
    p3 = os.path.join(out, "mayor_final.png")
    c.im.save(p3)
    print(p1)
    print(p2)
    print(p3)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
