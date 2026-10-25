"""strokes.py - STROKES planes -> ShowStrokeSet payloads (show_strokes.h), and
the player's rasteriser mirrored (path pixels, reveal curves) for the foley,
`show timing` and the tests.

In a set file:
    strokes <plane> [par=P] [boil=E] [at=X,Y]
      stroke <id> nib=<colour> [w=2] path=<gen:...|shape:...> [gap=9,1] [tooth=<colour>]
             [hidden=on] [boil=off]
    end
In a shot:
    +A reveal <stroke> over=+A..+B      (100 % on frame B; nothing before A)
    +T stroke <stroke> show|hide

Points are show px; the plane draws them on its own pixel grid (px), so a
px=2 stroke is made of 2x2 blocks like the plate under it.
"""
from . import bake, colors, fmt
from .fmt import SK_ALWAYS, SK_PEN_UP, SK_HIDDEN, SK_NOBOIL


class StrokeError(ValueError):
    pass


class Stroke:
    def __init__(self, sid, nib, path, w=2, gap=(9, 1), tooth=None, hidden=False, boil=True,
                 where=None):
        self.id, self.nib, self.path, self.w = sid, nib, path, int(w)
        self.gap, self.tooth, self.hidden, self.boil, self.where = gap, tooth, hidden, boil, where
        if not 1 <= self.w <= 3:
            raise StrokeError("stroke %s: w=%d (1..3)" % (sid, self.w))

    def polylines(self):
        return bake.path(self.path) if isinstance(self.path, str) else self.path


def plane_points(polys, px, at=(0, 0)):
    """show px polylines -> plane px points with pen-ups (ShowStrokePt)."""
    pts = []
    for poly in polys:
        cur = []
        for x, y in poly:
            p = (int(round((x - at[0]) / px)), int(round((y - at[1]) / px)))
            if not cur or cur[-1] != p:
                cur.append(p)
        if not cur:
            continue
        if pts:
            pts.append((SK_PEN_UP, 0))
        pts.extend(cur)
    return pts


def compile_plane(strokes, px=1, plane_id=1, boil=0, at=(0, 0), reveals=None, where=None):
    """-> (payload bytes, {stroke id: index})."""
    reveals = reveals or {}
    recs, allpts, index = [], [], {}
    for i, s in enumerate(strokes):
        pts = plane_points(s.polylines(), px, at)
        t0, t1 = reveals.get(s.id, (SK_ALWAYS, SK_ALWAYS))
        flags = (SK_HIDDEN if s.hidden else 0) | (0 if s.boil else SK_NOBOIL)
        on, off = (s.gap or (0, 0))
        recs.append(fmt.STROKE.pack(len(allpts), len(pts), t0, t1, colors.index(s.nib),
                                    colors.index(s.tooth) if s.tooth else 0, s.w, on, off, flags, 0))
        allpts.extend(pts)
        index[s.id] = i
    if len(allpts) > 65535:
        raise StrokeError("a STROKES plane holds at most 65,535 points")
    body = fmt.STROKESET.pack(len(strokes), len(allpts), plane_id & 0xFFFFFFFF, boil, 0)
    body += b"".join(recs) + b"".join(fmt.STROKEPT.pack(x, y) for x, y in allpts)
    return fmt.pad4(body), index


# ---- the rasteriser, as show_strokes.c walks it (no boil) -------------------------
def path_pixels(pts):
    """The plane-px pixels a stroke visits, in order (shared endpoints once)."""
    out = []
    have, first = False, True
    px = py = 0
    for i, (x, y) in enumerate(pts):
        if x == SK_PEN_UP:
            have, first = False, True
            continue
        if have:
            out.extend(_segment(px, py, x, y, first))
            first = False
        elif i + 1 >= len(pts) or pts[i + 1][0] == SK_PEN_UP:
            out.append((x, y))
        px, py, have = x, y, True
    return out


def _segment(x0, y0, x1, y1, first):
    dx, dy = abs(x1 - x0), abs(y1 - y0)
    sx, sy = (1 if x1 >= x0 else -1), (1 if y1 >= y0 else -1)
    err = dx - dy
    out = [(x0, y0)] if first else []
    while (x0, y0) != (x1, y1):
        e2 = 2 * err
        if e2 > -dy:
            err -= dy
            x0 += sx
        if e2 < dx:
            err += dx
            y0 += sy
        out.append((x0, y0))
    return out


def reveal_limit(t, t0, t1, n):
    if t0 == SK_ALWAYS:
        return n
    if t < t0:
        return 0
    if t >= t1:
        return n
    span = t1 - t0 + 1
    return (n * (t - t0 + 1) + span - 1) // span


def reveal_curve(stroke, px, t0, t1, frames, at=(0, 0)):
    """Path pixels drawn on each frame 0..frames-1 (the foley's input)."""
    n = len(path_pixels(plane_points(stroke.polylines(), px, at)))
    return [reveal_limit(t, t0, t1, n) for t in range(frames)]
