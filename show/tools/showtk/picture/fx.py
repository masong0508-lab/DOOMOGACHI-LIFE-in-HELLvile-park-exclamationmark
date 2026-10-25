"""fx.py - the shot's effects -> beat events and ShowFxSetup/ShowWipe records.

    look <register> [boil=E] [lerp=L] [grain=D]  setup: crayon|pixel|sepia|lcd|hellify
    +T iris_in|iris_out at=prop:<id>|X,Y over=F [shape=circle|heart] [r=R] [hold=end]
    +T fade in|out over=F                        (fade in at +0: the shot starts black)
    +T flash <colour> <level16> <F>              (engine-native; TIME-FLASH lints it)
    +T shake SHK_*  /  +T punch DX DY  /  +T zoom Q8 F    (engine-native, via camfx)
    +T pal PAL_SEPIA|PAL_CRAYON|PAL_HELLIFY|PAL_LILAC <level16> over=F
    +T boil on|off [every=2]
    +T wipe brick [dir=R|L] [stop=half] [hold=F|end] [out=on]    (a full wipe / the Split)
    +T split [hold=60] [side=right|left] [out=on] [line=160]     (the Split down the Wall line)
    +T dissolve over=13                          (the plate dissolve, credits)

Engine-native ops become (at, op, a, b) beat events for textpack's compile_beat;
irises with holds over 63 f, the boil and frame-0 states become lane-2 records.
"""
from . import colors, fmt

# beat.h op numbers (BOP_*), as compile_beat emits them
BOP = {"END": 0, "SHAKE": 13, "PUNCH": 14, "ZOOM": 15, "FLASH": 16, "FADE": 17,
       "IRIS_OUT": 18, "IRIS_IN": 19, "PAL_LERP": 33}
SHK = ["SHK_TAP", "SHK_BRAKE", "SHK_POW", "SHK_IMPACT", "SHK_STAMP", "SHK_STAMP3", "SHK_SLAM",
       "SHK_IGNITE", "SHK_SHOTGUN", "SHK_FWOOMP", "SHK_CRACK", "SHK_RUMBLE", "SHK_ROAR", "SHK_GUNFIRE"]
# camfx.c's shake profiles: frames and the largest |x|, |y| (240x160 px)
SHAKE_FRAMES = [2, 3, 3, 3, 3, 5, 5, 5, 7, 8, 8, 12, 14, 1]
SHAKE_AMP = [(0, 3), (2, 0), (2, 1), (3, 2), (3, 2), (3, 2), (3, 1), (3, 2), (7, 3), (4, 2), (3, 1),
             (3, 2), (4, 2), (1, 1)]
PAL = {"PAL_SEPIA": 1, "PAL_CRAYON": 2, "PAL_HELLIFY": 3, "PAL_LILAC": 4}
IRIS_SNAP, IRIS_CLOSE = 3, 5


class FxError(ValueError):
    pass


def s16(v):
    """a BeatEv int16 field from a packed value (frames << 8 may pass 32767)."""
    v &= 0xFFFF
    return v - 65536 if v > 32767 else v


OVERSCAN = 8                         # show_cam.c clamps the shake to it


def shake_px(profile, w=320, h=240):
    """The largest offset (show px) a shake profile moves the planes by."""
    ax, ay = SHAKE_AMP[profile]
    return min(OVERSCAN, -(-ax * w // 240)), min(OVERSCAN, -(-ay * h // 160))


def iris(op, over, slot=None, xy=None, shape="circle", r=0, hold_end=False):
    """`iris_in|iris_out ... over=F` -> an engine-native event (a, b) when it
    fits beat.h's packing (hold <= 63, a prop/actor slot), else a SOP_IRIS.
    camfx runs R3 + hold + R5 (in) or R5 + hold + R3 (out), so over=F is
    hold = F - 8 (spec 6.4: over=13 -> hold 5; under 8 is refused)."""
    if over < IRIS_SNAP + IRIS_CLOSE:
        raise FxError("an iris needs over >= 8 (it snaps R3 and moves R5); got %d" % over)
    hold = over - IRIS_SNAP - IRIS_CLOSE
    heart = 1 if shape == "heart" else 0
    if hold_end:
        hold = 255
    if slot is not None and hold <= 63 and r <= 1022:
        b = hold | (heart << 6) | ((r // 2) << 7)
        if b > 32767:
            b -= 65536
        return ("beat", BOP["IRIS_IN" if op == "iris_in" else "IRIS_OUT"], slot, b)
    x, y = xy if xy else (160, 90)
    return ("sop", fmt.SOP_IRIS, [slot if slot is not None else -1, x, y, r, heart,
                                  0 if op == "iris_in" else 1, min(hold, 255)])


def fade(direction, over, t):
    """fade in|out over=F -> (setup fade0, beat event). BOP_FADE b = level | frames << 8."""
    if direction == "in":
        return (16 if t == 0 else None), (BOP["FADE"], 0, s16(over << 8))
    return None, (BOP["FADE"], 0, s16(16 | (over << 8)))


def flash(color, level, frames):
    c = colors.index(color)
    return BOP["FLASH"], c, s16(int(level) | (int(frames) << 8))


def pal(name, level, over):
    if name not in PAL:
        raise FxError("pal %s: use PAL_SEPIA, PAL_CRAYON, PAL_HELLIFY or PAL_LILAC" % name)
    return BOP["PAL_LERP"], PAL[name], s16(int(level) | (int(over) << 8))


def setup(look="pixel", boil=0, matte="open", fade0=0, pal_id=0, pal_level=0, seed=0,
          nwipes=0, dissolve=(0, 0)):
    return fmt.FXSETUP.pack(fmt.LOOKS[look], boil, fmt.MATTE[matte], fade0, pal_id, pal_level,
                            nwipes, 0, seed & 0xFFFFFFFF, dissolve[0], dissolve[1])


def wipe(t0, kind="stack", side="right", hold=60, out=True, line=160, courses=5, step=2,
         brick=(16, 8), color="RAMP_WOOD:3", hilite="RAMP_WOOD:5", mortar="RAMP_STONE:2", px=1):
    """A ShowWipe. kind stack = the Split down the Wall line (GAG_BIBLE 8.2: 5
    courses x 2 f); kind sweep = the full brick wipe, the edge crossing the frame."""
    k = fmt.WIPE_STACK if kind == "stack" else fmt.WIPE_SWEEP
    sd = fmt.WIPE_B_LEFT if side == "left" else fmt.WIPE_B_RIGHT
    hd = fmt.WIPE_HOLD_END if hold in ("end", None) else int(hold)
    return fmt.WIPE.pack(t0, hd, k, sd, courses, step, line, 1 if out else 0, brick[0], brick[1],
                         colors.index(color), colors.index(hilite), colors.index(mortar), px)


def wipe_span(t0, hold, out, courses=5, step=2):
    """(first frame, last frame side B shows) for lints and timing."""
    n = courses * step
    if hold in ("end", None):
        return t0, None
    end = t0 + n + int(hold) + (n if out else 0) - 1
    return t0, end
