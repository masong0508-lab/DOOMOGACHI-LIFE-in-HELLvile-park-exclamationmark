"""shots.py - the lane-2 test shots, built with showtk.picture the way showc
will build them. Each returns (blob bytes, info dict). Plates are drawn here
with simple palette-exact patterns when ART's generators are not on the
branch, so every test runs on its own.
"""
import numpy as np

from pictest import SHOW                                             # noqa: F401  (sys.path)
from showtk.picture import blob, camera, colors, fmt, fx, strokes, text, tilemap

C = colors.index


def plate(w, h, seed, base, accents):
    """A palette-exact test plate: bands of `base` shades with accent blocks."""
    rng = np.random.RandomState(seed)
    a = np.zeros((h, w), np.uint8)
    for y in range(h):
        a[y, :] = C(base[(y * len(base)) // h])
    for _ in range(max(1, w * h // 900)):
        x, y = rng.randint(0, w - 8), rng.randint(0, h - 8)
        a[y:y + rng.randint(3, 9), x:x + rng.randint(3, 9)] = C(accents[rng.randint(len(accents))])
    for x in range(0, w, 32):                       # a ruler every 32 px: motion is visible
        a[:, x] = C("WHITE")
    return a


def drift_shot(frames=180, draw=False):
    """co02's camera: drift doc:1.0 px/f, x = 0 at +90; planes at 0.05/0.10/0.20."""
    sh = blob.Shot(frames)
    cam = camera.Camera()
    cam.drift(dx="doc:1.0", zero=90)
    setup, keys = cam.pack()
    sh.camera(setup, keys)
    sh.fx(fx.setup())
    for w, par, base in ((352, "0.05", ["RAMP_SKY:1", "RAMP_SKY:2", "RAMP_SKY:3"]),
                         (368, "0.10", ["RAMP_GRASS:2", "RAMP_GRASS:3"]),
                         (400, "0.20", ["RAMP_WOOD:2"])):
        a = plate(w, 240, w, base, ["PAINT_LEMON:1", "PAINT_ROSE:1", "PAINT_MINT:1"])
        if par == "0.20":
            a[:150] = 0                             # the near plane: only its bottom
        sh.image_plane(a, fmt.planecam(par=camera.par_frac(par), at=(-16, -30)))
    return sh.pack(), {"cam": cam, "pars": [(1, 20), (1, 10), (1, 5)], "at": -16}


def calendar_shot():
    """co01: a crayon circle revealed +0..+24, a scribble +30..+60, boil on twos, px=2."""
    sh = blob.Shot(90)
    cam = camera.Camera()
    setup, keys = cam.pack()
    sh.camera(setup, keys)
    sh.fx(fx.setup(look="crayon", boil=2, fade0=16, seed=0xC0101))
    sh.event(0, fx.BOP["FADE"], 0, fx.s16(8 << 8))
    paper = np.full((120, 160), C("UI_CREAM"), np.uint8)
    for y in range(0, 120, 9):
        paper[y, 8:152] = C("RAMP_STONE:5")
    paper[30:34, 40:120] = C("RAMP_STONE:3")
    sh.image_plane(paper, fmt.planecam(at=(0, -30)), px=2)
    circle = strokes.Stroke("circle_midsummer", "PAINT_CHERRY:1", "shape:ellipse(160,96,48,18,1.15)", w=2,
                            tooth="PAINT_ROSE:2")
    bursts = [[(160 + dx, 96 + dy), (160 + dx * 2, 96 + dy * 2)]
              for dx, dy in ((-60, -20), (60, -22), (-50, 25), (55, 24), (0, -40))]
    scribble = strokes.Stroke("fireworks_scribble", "PAINT_BUBBLEGUM:1", bursts, w=2)
    payload, idx = strokes.compile_plane([circle, scribble], px=2, plane_id=11,
                                         reveals={"circle_midsummer": (0, 24),
                                                  "fireworks_scribble": (30, 60)})
    sh.plane(blob.PLANE_STROKES, payload, fmt.planecam(), px=2)
    return sh.pack(), {"strokes": [circle, scribble]}


CO02_TEXT = "Every Midsummer night, the whole park had one party."


def super_shot():
    """co02's SUPER (type over +30..+132, child's hand) and co03's (+13 until +63)."""
    sh = blob.Shot(180)
    sh.camera(*camera.Camera().pack())
    sh.fx(fx.setup())
    sh.fill_plane(C("RAMP_SKY:1"))
    items = [text.super_(CO02_TEXT, "type", 30, 180, at="lower", over=(30, 132), font="child_hand",
                         sid="s_co02"),
             text.super_("It was called the Mingle.", "type", 13, 180, at="top", until=63,
                         font="child_hand", sid="s_co03"),
             text.super_("Now there's a different party.", "pop", 100, 180, at="title", until=170,
                         font="game", scale=2),
             text.super_("Nobody talks about why.", "deadpan", 140, 180, at=(24, 40), font="game")]
    sh.text(text.pack(items))
    return sh.pack(), {"items": items}


def iris_shot(heart=False):
    """co02's iris_in on the bandstand (prop slot 0 at 160,92) over=13; co21's heart."""
    sh = blob.Shot(60)
    sh.camera(*camera.Camera().pack())
    sh.fx(fx.setup())
    sh.image_plane(plate(336, 240, 5, ["RAMP_SKY:2", "RAMP_GRASS:3"], ["PAINT_LEMON:1"]),
                   fmt.planecam(at=(-8, -30)))
    slot = sh.slot(160, 92)
    kind, *ev = fx.iris("iris_in", 13, slot=slot)
    sh.event(0, ev[0], ev[1], ev[2])
    if heart:
        k2 = fx.iris("iris_out", 20, slot=slot, shape="heart", r=40, hold_end=True)
        sh.event(30, k2[1], *k2[2])
    return sh.pack(), {"ev": (kind, ev)}


def shake_shot(w=336, profile="SHK_SHOTGUN", matte="open"):
    """A shake on planes with an 8 px overscan: no edge may show (AT8.3)."""
    sh = blob.Shot(40)
    sh.camera(*camera.Camera().pack())
    sh.fx(fx.setup(matte=matte))
    sh.fill_plane(C("PAINT_MAGENTA:1"))              # anything uncovered shows magenta
    sh.image_plane(plate(w, 196, 9, ["RAMP_PEACH:3", "RAMP_PEACH:4"], ["PAINT_TEAL:1"]),
                   fmt.planecam(at=(-(w - 320) // 2, -8)))
    p = fx.SHK.index(profile)
    for t in (2, 14, 26):
        sh.event(t, fx.BOP["SHAKE"], p)
    return sh.pack(), {"profile": p, "w": w}


def tilemap_shot(scale_to=None, frames=30):
    """A pixel plate as a TILEMAP plane; optionally a scale move 1.10 -> 1.00 (co12)."""
    a = plate(384, 240, 3, ["RAMP_PEACH:4", "RAMP_PEACH:5", "RAMP_CANDY:6"], ["PAINT_ROSE:1", "PAINT_SKY:1"])
    a[100:140, 140:244] = C("RAMP_WOOD:3")          # a bandstand-ish block (repeats tiles)
    payload, info = tilemap.compile_plane(a, plane_id=7)
    sh = blob.Shot(frames)
    cam = camera.Camera()
    if scale_to:
        cam.setup_at(scale="1.10")
        cam.to(0, frames, scale=scale_to, ease="OUT")
    sh.camera(*cam.pack())
    sh.fx(fx.setup())
    sh.fill_plane(C("PAINT_MAGENTA:1"))
    sh.plane(blob.PLANE_TILEMAP, payload, fmt.planecam(at=(-32, -30)))
    return sh.pack(), {"plate": a, "info": info, "cam": cam}


def split_shot():
    """The Split down the Wall line: 5 courses x 2 f in at +10, 60 f held, out."""
    sh = blob.Shot(110)
    sh.camera(*camera.Camera().pack())
    sh.fx(fx.setup(nwipes=1), [fx.wipe(10, "stack", hold=60, out=True)])
    sh.image_plane(plate(336, 240, 1, ["RAMP_PASTEL_PINK:5", "RAMP_PASTEL_PINK:6"], ["PAINT_LEMON:1"]),
                   fmt.planecam(at=(-8, -30)))
    sh.image_plane(plate(336, 240, 2, ["RAMP_BLOOD:1", "RAMP_LAVA:2"], ["PAINT_JET:1"]),
                   fmt.planecam(at=(-8, -30)), side=1)
    return sh.pack(), {}


def camera_moves_shot():
    """pan, crane, creep, whip and a scale move in one shot (Python == C check)."""
    sh = blob.Shot(200)
    cam = camera.Camera()
    cam.setup_at(x=0, y=0)
    cam.pan(10, dx="0.5")
    cam.crane(40, dy=-12, over=30)
    cam.stop(80)
    cam.creep(90, dx="0.25", ramp=13)
    cam.whip(140, "R", over=6)
    cam.to(160, 30, scale="1.08", ease="INOUT")
    sh.camera(*cam.pack())
    sh.fx(fx.setup())
    sh.image_plane(plate(800, 260, 4, ["RAMP_MINT:3", "RAMP_MINT:4"], ["PAINT_ROSE:1"]),
                   fmt.planecam(par=(1, 1), at=(-16, -40)))
    return sh.pack(), {"cam": cam}
