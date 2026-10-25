"""showtk.art.cast.sheet - look-dev model sheets from the game's own portrait
engine (ART track; SHOW_ENGINE_SPEC.md sec.9.1 `show art sheet`, AT6.1 style).

Every pixel of a character comes from core/src/portrait*.c (through the
native host, showtk.art.cast.host); this module only lays the images out on a
sheet and writes the labels.  Review material, never master pixels: once
show.exe exists the same sheets render on the PS1.

    python3 -m showtk.art.cast.sheet mayor nana ...  [--out DIR] [--scale 2]
    python3 -m showtk.art.cast.sheet --all            # every char + CAST_LINEUP.png
    python3 -m showtk.art.cast.sheet --lineup         # CAST_LINEUP.png only

A character sheet holds:
    figure px=1   x 11 expressions (EXPR_*), then x 8 visemes
    bust   px=2   x 11 expressions, then x 8 visemes (the mouth override,
                  hunk (d)), then a blink frame and the possessed/ghost/dead
                  states
    chibi  px=1 and px=2: 4 directions x 4 walk frames
Visemes follow show/series/visemes.shw when the AUDIO track has written it
(`viseme <V> MOUTH_*`, optionally `family=<SPECIES_*>`), else the spec's
sec.9.3 defaults.
"""
import os
import re
import sys

from PIL import Image, ImageDraw, ImageFont

from . import host
from .catalog import ROOT, catalog
from .lookdata import Bible, SERIES

DEFAULT_OUT = os.environ.get("SHOW_ART_OUT") or os.path.join(
    "/tmp/claude-0/-home-user-DOOMOGACHI-LIFE-in-HELLvile-park-exclamationmark/"
    "ea792815-11f0-5f9f-bdfe-5997cdfc4994/scratchpad/show_out/art/cast")

EXPRS = ["EXPR_NEUTRAL", "EXPR_HAPPY", "EXPR_SAD", "EXPR_ANGRY", "EXPR_LOVE", "EXPR_SHOCK",
         "EXPR_SLEEPY", "EXPR_SICK", "EXPR_POSSESSED", "EXPR_DEAD", "EXPR_TALK"]
VISEMES = "XMCEAOUF"
# SHOW_ENGINE_SPEC.md sec.9.3 defaults (rest = the look's own mouth style)
VISEME_DEFAULT = {"X": None, "M": "MOUTH_FLAT", "C": "MOUTH_TEETH", "E": "MOUTH_GRIN",
                  "A": "MOUTH_OPEN", "O": "MOUTH_O", "U": "MOUTH_POUT", "F": "MOUTH_TEETH"}
BG = (0x3a, 0x38, 0x52)
PANEL = (0x8c, 0x88, 0xa8)
INK = (0xf4, 0xee, 0xe0)
DIM = (0xb8, 0xb0, 0xc8)


def visemes(species):
    """{viseme: MOUTH_* or None} for a species, from series/visemes.shw if present"""
    table = dict(VISEME_DEFAULT)
    fam = {}
    path = os.path.join(SERIES, "visemes.shw")
    if os.path.exists(path):
        for line in open(path):
            line = line.split("#", 1)[0]
            m = re.match(r"\s*viseme\s+([XMCEAOUF])\s+(MOUTH_\w+)(?:\s+family=(SPECIES_\w+))?", line)
            if m:
                if m.group(3):
                    fam.setdefault(m.group(3), {})[m.group(1)] = m.group(2)
                else:
                    table[m.group(1)] = m.group(2)
    table.update(fam.get(catalog().name_of("SPECIES_", species), {}))
    return table


def mouth_value(name):
    return -1 if name is None else catalog().value(name)


def _font():
    try:
        return ImageFont.load_default()
    except Exception:  # pragma: no cover
        return None


class Canvas:
    def __init__(self, w, h, pal):
        self.im = Image.new("RGB", (w, h), BG)
        self.d = ImageDraw.Draw(self.im)
        self.pal = pal
        self.font = _font()

    def img(self, rec, x, y, z, panel=True):
        w, h, px = rec
        if panel:
            self.d.rectangle([x, y, x + w * z - 1, y + h * z - 1], fill=PANEL)
        t = Image.new("RGBA", (w, h))
        t.putdata([(*self.pal[p], 255) if p else (0, 0, 0, 0) for p in px])
        if z != 1:
            t = t.resize((w * z, h * z), Image.NEAREST)
        self.im.paste(t, (x, y), t)

    def text(self, x, y, s, fill=INK):
        self.d.text((x, y), s, fill=fill, font=self.font)


def char_jobs(look, species):
    vis = visemes(species)
    jobs, labels = [], []

    def add(label, *a, **k):
        jobs.append(host.job(look, *a, **k))
        labels.append(label)
    cat = catalog()
    for e in EXPRS:
        add(("fig", e), "figure", expr=cat.value(e), anim=4)
    for v in VISEMES:
        add(("figv", v), "figure", expr=0, mouth=mouth_value(vis[v]), anim=4)
    for e in EXPRS:
        add(("bust", e), "bust", expr=cat.value(e), anim=4)
    for v in VISEMES:
        add(("bustv", v), "bust", expr=0, mouth=mouth_value(vis[v]), anim=4)
    add(("bust", "blink"), "bust", expr=0, anim=93)
    for st in ("RES_POSSESSED", "RES_GHOST", "RES_DEAD"):
        add(("bust", st), "bust", status=cat.value(st), expr=0, anim=4)
    for d in range(4):
        for f in range(4):
            add(("chibi", d, f), "chibi", a=d, b=f)
    return jobs, labels, vis


def short(e):
    return e.replace("EXPR_", "").replace("RES_", "").lower()


def char_sheet(bible, cid, out_dir, scale=2):
    c = bible.chars[cid]
    look = c.look.pack()
    species = c.look.values["species"]
    jobs, labels, vis = char_jobs(look, species)
    pal, imgs = host.render(jobs)
    R = dict(zip(labels, imgs))
    gap = 6
    W = max(11 * (96 + gap), 19 * (48 + gap)) + 16
    H = 30 + (80 + 22) + (112 + 22) * 2 + (24 * 2 + 30) + 10
    cv = Canvas(W, H, pal)
    name = c.opts.get("name", (cid, None))[0]
    cat = catalog()
    lv = c.look.values
    head = "%s  (%s)  %s  eyes=%s  mouth=%s  age=%s" % (
        name, cid, cat.name_of("SPECIES_", species), cat.name_of("EYE_", lv["eyes"]),
        cat.name_of("MOUTH_", lv["mouth"]), c.opts.get("age", ("?", None))[0])
    cv.text(8, 6, head)
    src = c.opts.get("look", ("", None))[0]
    cv.text(8, 18, "look=%s  squash=%s  voice=%s   [game portrait engine, native host; review only]" % (
        src, c.opts.get("squash", ("-", None))[0], c.opts.get("voice", ("-", None))[0]), DIM)
    y = 32
    x = 8
    for e in EXPRS:
        cv.img(R[("fig", e)], x, y, 1)
        cv.text(x, y + 81, short(e)[:8])
        x += 48 + gap
    x += 8
    for v in VISEMES:
        cv.img(R[("figv", v)], x, y, 1)
        cv.text(x, y + 81, "%s %s" % (v, (vis[v] or "rest").replace("MOUTH_", "").lower()[:6]))
        x += 48 + gap
    y += 80 + 22
    x = 8
    for e in EXPRS:
        cv.img(R[("bust", e)], x, y, 2)
        cv.text(x, y + 113, short(e))
        x += 96 + gap
    y += 112 + 22
    x = 8
    for v in VISEMES:
        cv.img(R[("bustv", v)], x, y, 2)
        cv.text(x, y + 113, "%s  %s" % (v, (vis[v] or "rest").replace("MOUTH_", "").lower()))
        x += 96 + gap
    for k in ("blink", "RES_POSSESSED", "RES_GHOST", "RES_DEAD"):
        cv.img(R[("bust", k)], x, y, 2 if k == "blink" else 1)
        cv.text(x, y + (113 if k == "blink" else 57), short(k))
        x += (96 if k == "blink" else 48) + gap
    y += 112 + 22
    x = 8
    for z in (1, 2):
        for d in range(4):
            for f in range(4):
                cv.img(R[("chibi", d, f)], x, y, z)
                x += 16 * z + 2
            x += 6
        x += 10
    cv.text(8, y + 50, "chibi px=1 and px=2: down, left, right, up x walk 0-3")
    os.makedirs(out_dir, exist_ok=True)
    im = cv.im
    if scale != 1:
        im = im.resize((im.width * scale, im.height * scale), Image.NEAREST)
    path = os.path.join(out_dir, "%s_sheet.png" % cid)
    im.save(path)
    return path


def seen(c, episode="ep101"):
    """a portrait cast member who is on screen in `episode`"""
    tags = c.opts.get("tags", ("", None))[0].split(",")
    first = c.opts.get("first", ("ep000", None))[0]
    return c.look is not None and "VOICE_ONLY" not in tags and first <= episode


def lineup(bible, out_dir, scale=2, episode="ep101"):
    """every char's figure (px=1) and chibi, then every kset's figures"""
    chars = [cid for cid in bible.order if seen(bible.chars[cid], episode)]
    jobs, labels = [], []
    for cid in chars:
        lb = bible.chars[cid].look.pack()
        jobs.append(host.job(lb, "figure", anim=4))
        labels.append(("fig", cid))
        jobs.append(host.job(lb, "chibi"))
        labels.append(("chibi", cid))
    kl = {}
    for kid in sorted(bible.ksets):
        kl[kid] = bible.kset_looks(kid)
        for n, (_lab, lb) in enumerate(kl[kid]):
            jobs.append(host.job(lb, "figure", anim=4 + n * 7))
            labels.append(("kfig", kid, n))
    pal, imgs = host.render(jobs)
    R = dict(zip(labels, imgs))
    per_row = 12
    cell = 76
    rows = (len(chars) + per_row - 1) // per_row
    krows = sum((len(v) + 23) // 24 for v in kl.values())
    W = per_row * cell + 16
    W = max(W, 24 * 40 + 16)
    H = 24 + rows * 124 + len(kl) * 16 + krows * 86 + 20
    cv = Canvas(W, H, pal)
    cv.text(8, 6, "DOOMOGACHI LIFE in HELLvile Park! 101 - cast lineup (figures px=1 + chibi), "
                  "the game's portrait engine")
    y = 24
    for i, cid in enumerate(chars):
        r, col = divmod(i, per_row)
        x = 8 + col * cell
        yy = y + r * 124
        cv.img(R[("fig", cid)], x, yy, 1)
        cv.img(R[("chibi", cid)], x + 52, yy + 56, 1)
        c = bible.chars[cid]
        cv.text(x, yy + 82, cid[:12])
        cv.text(x, yy + 94, c.opts.get("age", ("?", 0))[0], DIM)
    y += rows * 124
    for kid, looks in kl.items():
        ks = bible.ksets[kid]
        cv.text(8, y, "kset %s  (K=%d, age=%s) %s" % (kid, len(looks), ks.age, ks.note[:60]), DIM)
        y += 14
        for n in range(len(looks)):
            r, col = divmod(n, 24)
            cv.img(R[("kfig", kid, n)], 8 + col * 40, y + r * 86, 1, panel=False)
        y += ((len(looks) + 23) // 24) * 86 + 2
    im = cv.im.crop((0, 0, W, min(H, y + 8)))
    if scale != 1:
        im = im.resize((im.width * scale, im.height * scale), Image.NEAREST)
    os.makedirs(out_dir, exist_ok=True)
    path = os.path.join(out_dir, "CAST_LINEUP.png")
    im.save(path)
    return path


def main(argv):
    out = DEFAULT_OUT
    scale = 2
    args = []
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == "--out":
            out = argv[i + 1]
            i += 1
        elif a == "--scale":
            scale = int(argv[i + 1])
            i += 1
        else:
            args.append(a)
        i += 1
    b = Bible()
    if b.errors:
        for e in b.errors:
            print("ERROR", e)
        return 1
    want = [a for a in args if not a.startswith("--")]
    if "--all" in args:
        want = [cid for cid in b.order if seen(b.chars[cid])]
    for cid in want:
        if cid not in b.chars or not b.chars[cid].look:
            print("no portrait cast member %r" % cid)
            return 1
        print(char_sheet(b, cid, out, scale))
    if "--all" in args or "--lineup" in args:
        print(lineup(b, out, scale))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
