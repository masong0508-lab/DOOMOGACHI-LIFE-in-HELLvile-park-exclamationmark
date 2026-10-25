"""showtk.review.frame - `show frame <shot> +N --overlay` (WP13, sec.13.1).

One master frame at x3 (the whole 320x240 display, so the matte shows) with,
when --overlay is given:
  - the 16:9 picture (lines 30-209) in grey, title-safe (16,9) in yellow and
    action-safe (8,5) in green;
  - the Wall line x = 160 in cyan (the docs' doc:192);
  - every actor and prop anchor the timeline gives (a cross and its id);
  - off-palette pixels in magenta (the capture LUT, sec.2.3), with their count.
"""
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

from ..sidecars import build_dir, tc
from .pictures import Pictures, off_palette_mask
from .sheet import _font

SAFE_TITLE = (16, 9)
SAFE_ACTION = (8, 5)
WALL_X = 160
SCALE = 3


def frame(tl, sid, t, masters_dir=None, overlay=True, out=None, scale=SCALE):
    s = tl.shot(sid)
    if not 0 <= t < s.len:
        raise ValueError("%s +%d is outside the shot (0..%d)" % (sid, t, s.len - 1))
    f = s.start + t
    img = Pictures(tl, masters_dir).frames([f], full=True)[f]
    y0, ph, pw = tl.canvas["y_off"], tl.canvas["h"], tl.canvas["w"]
    off = off_palette_mask(img)
    n_off = int(off[y0:y0 + ph].sum())
    big = np.repeat(np.repeat(img, scale, 0), scale, 1)
    if overlay:
        m = np.repeat(np.repeat(off, scale, 0), scale, 1)
        big[m] = (255, 0, 255)
    im = Image.fromarray(big)
    if overlay:
        d = ImageDraw.Draw(im)

        def rect(x0, y0_, x1, y1, col, w=1):
            d.rectangle([x0 * scale, y0_ * scale, x1 * scale - 1, y1 * scale - 1], outline=col, width=w)
        rect(0, y0, pw, y0 + ph, (150, 150, 150))
        rect(SAFE_ACTION[0], y0 + SAFE_ACTION[1], pw - SAFE_ACTION[0], y0 + ph - SAFE_ACTION[1], (60, 220, 90))
        rect(SAFE_TITLE[0], y0 + SAFE_TITLE[1], pw - SAFE_TITLE[0], y0 + ph - SAFE_TITLE[1], (250, 210, 40))
        for yy in range(y0 * scale, (y0 + ph) * scale, 12):
            d.line([WALL_X * scale, yy, WALL_X * scale, yy + 6], fill=(40, 220, 240), width=2)
        fnt = _font(13)
        for a in list(s.actors) + list(s.raw.get("props", [])):
            x, y = a.get("x"), a.get("y")
            if x is None or y is None:
                continue
            cx, cy = int(x * scale), int((y + y0) * scale)
            d.line([cx - 9, cy, cx + 9, cy], fill=(255, 80, 80), width=2)
            d.line([cx, cy - 9, cx, cy + 9], fill=(255, 80, 80), width=2)
            d.text((cx + 6, cy + 4), str(a.get("id", a.get("who", "?"))), fill=(255, 255, 255), font=fnt)
        d.text((6, 4), "%s %s +%d  f%d  %s   off-palette %d px   %s" % (
            tl.episode, sid, t, f, tc(f), n_off, s.badge or ""), fill=(255, 255, 255), font=fnt)
    out = Path(out) if out else build_dir(tl.episode) / "review" / ("frame_%s_%03d.png" % (sid, t))
    out.parent.mkdir(parents=True, exist_ok=True)
    im.save(out)
    return out, {"frame": f, "off_palette": n_off}
