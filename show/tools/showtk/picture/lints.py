"""lints.py - the picture's lints (spec 13.5), each on compiled data so it sees
exactly what the player will draw:

  TIME-READ   a SUPER, sign or card text is on screen >= 9 f per word + 15 (warning)
  TIME-FLASH  a full-screen flash lasts <= 3 f, flashes start >= 10 f apart (< 3/s);
              a flash brighter than 8/16 needs a luminance ramp: its level must
              fall over >= 8 f, or it is an error (Harding-style, not certified)
  TIME-IRIS   an iris over >= 8 f (R3 + hold + R5) inside the shot
  TIME-WIPE   a brick wipe is 5 courses x 2 f (GAG_BIBLE 8.2)
  TIME-SPLIT  a Split holds >= 48 f (2 s at 24; the docs' 60 f default) on x = 160
  PIC-COVER   every opaque plane covers the picture at every frame, with room for the
              shot's largest shake (the planes move with the shake; spec 2.3)
Each returns a list of showtk.errors.Diag.
"""
from ..errors import Diag
from . import fx as fxm

READ_PER_WORD, READ_BASE = 9, 15
FLASH_MAX, FLASH_GAP, FLASH_RAMP = 3, 10, 8


def time_read(items, shot_len, shot=None, file=None):
    out = []
    for it in items:
        if not it.text.strip():
            continue
        on = it.frames_on(shot_len)
        words = it.words()
        need = READ_PER_WORD * words + READ_BASE
        if on < need:
            end = shot_len if it.t_off == 0xFFFF else it.t_off
            out.append(Diag("TIME-READ", "SUPER %s\"%s\" is on screen %d f (+%d..+%d); %d words need %d f "
                            "(%d f/word + %d). Keep it (`waive TIME-READ \"...\"`) or end it at +%d."
                            % ((it.sid + " ") if it.sid else "", it.text.replace("\n", " "), on, it.t_on,
                               end, words, need, READ_PER_WORD, READ_BASE, it.t_on + need),
                            file=file, line=(it.where or {}).get("line") if isinstance(it.where, dict) else None,
                            shot=shot, frame=it.t_on, severity="warning"))
    return out


def time_flash(flashes, shot=None, file=None):
    """flashes: [(t, colour, level16, frames, line)] - the shot's `flash` ops."""
    out = []
    last = None
    for t, color, level, frames, line in sorted(flashes):
        if frames > FLASH_MAX:
            out.append(Diag("TIME-FLASH", "a full-screen flash of %d f; the limit is %d f (spec 13.5). "
                            "Shorten it, or make it a fade (`fade out over=8`)." % (frames, FLASH_MAX),
                            file=file, line=line, shot=shot, frame=t))
        if last is not None and t - last < FLASH_GAP:
            out.append(Diag("TIME-FLASH", "this flash starts %d f after the last one; flashes need %d f "
                            "between starts (under 3 a second). camfx would drop it." % (t - last, FLASH_GAP),
                            file=file, line=line, shot=shot, frame=t))
        if level > 8 and frames < FLASH_RAMP and frames > 1:
            out.append(Diag("TIME-FLASH", "a %d/16 flash decays over %d f; anything brighter than 8/16 "
                            "ramps over >= %d f (the firework rule) or stays at 8/16 or less."
                            % (level, frames, FLASH_RAMP), file=file, line=line, shot=shot, frame=t,
                            severity="warning"))
        last = t
    return out


def time_iris(irises, shot_len, shot=None, file=None):
    """irises: [(t, over, line)]"""
    out = []
    for t, over, line in irises:
        if over < 8:
            out.append(Diag("TIME-IRIS", "an iris over %d f; it needs >= 8 (it snaps R3 and moves R5)" % over,
                            file=file, line=line, shot=shot, frame=t))
        elif t + over > shot_len:
            out.append(Diag("TIME-IRIS", "the iris runs to +%d, past the shot's end (+%d)" % (t + over, shot_len),
                            file=file, line=line, shot=shot, frame=t, severity="warning"))
    return out


def time_wipe(wipes, shot=None, file=None):
    """wipes: [(t, kind, courses, step, hold, line_x, line)]"""
    out = []
    for t, kind, courses, step, hold, lx, line in wipes:
        if courses != 5 or step != 2:
            out.append(Diag("TIME-WIPE", "a brick wipe is 5 courses x 2 f (GAG_BIBLE 8.2); this one is %d x %d"
                            % (courses, step), file=file, line=line, shot=shot, frame=t))
        if kind == "stack":
            if hold not in ("end", None) and int(hold) < 48:
                out.append(Diag("TIME-SPLIT", "the Split holds %s f; it holds at least 48 (2 s)" % hold,
                                file=file, line=line, shot=shot, frame=t))
            if lx != 160:
                out.append(Diag("TIME-SPLIT", "the Split runs on x = %d; the Wall line is x = 160" % lx,
                                file=file, line=line, shot=shot, frame=t))
    return out


def pic_cover(planes, camera, shot_len, shakes=(), shot=None, file=None, step=1):
    """planes: [(name, w_px, h_px, px, par, at, opaque, line)] (par a fraction pair);
    camera: a showtk.picture.camera.Camera; shakes: SHK_* profile numbers used."""
    from .camera import plane_origin
    from .fmt import CAM_CU
    out = []
    mx = max([fxm.shake_px(p)[0] for p in shakes] + [0])
    my = max([fxm.shake_px(p)[1] for p in shakes] + [0])
    for name, w, h, px, par, at, opaque, line in planes:
        if not opaque:
            continue
        for t in range(0, shot_len, step):
            cx, cy, s = camera.at(t)
            if s != 65536:
                continue                         # scaled spans: CAM-SCALE logs them
            x0 = plane_origin(cx, par, at[0])
            y0 = plane_origin(cy, par, at[1])
            x1, y1 = x0 + w * px, y0 + h * px
            if x0 > -mx or x1 < 320 + mx or y0 > -my or y1 < 180 + my:
                out.append(Diag("PIC-COVER", "plane %s covers x %d..%d, y %d..%d at +%d; the picture "
                                "needs 0..320 x 0..180 plus the shake's %d,%d px"
                                % (name, x0, x1, y0, y1, t, mx, my), file=file, line=line, shot=shot,
                                frame=t))
                break
    return out
