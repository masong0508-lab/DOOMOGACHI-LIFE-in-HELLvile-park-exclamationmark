"""text.py - SUPERs, signs and cards -> ShowTextItem records (show_text.h), with
the player's own measurement and glyph schedule.

    +T super <string> style=type|pop|deadpan at=lower|title|top|X,Y
          [over=+A..+B | cps=N] [until=<time>|end] [font=game|child_hand]
          [scale=N] [align=left|center|right] [color=] [outline=] [shadow=] [hop=on|off]
    +T sign <string> on=<prop> [dx= dy=] [until=] [font=] [scale=] [color=] [outline=]
    +T card <template> k=v ... [until=]      templates: tvma, act, title, text, black

<string> is a string id from strings.shw (its font= is the default font) or a
quoted literal. Lines wrap at the title-safe width (288 px) unless the text
has its own '\\n'. A SUPER is on screen from its first frame (type: its first
glyph) until `until` (exclusive), the shot's end by default.
"""
import re

from . import colors, fmt, pxt
from .fmt import TS, TF, TA, TX_HOP, TX_SLOT, TX_END

CANVAS_W, CANVAS_H, SAFE_X, SAFE_Y = 320, 180, 16, 9
GAME_ADV, GAME_LINE = 6, 9

_child = None


def child_font():
    global _child
    if _child is None:
        _child = pxt.child_font()
    return _child


class TextError(ValueError):
    pass


# ---- measurement (show_text_line_w / _line_h) -----------------------------------
def line_w(font, s):
    if font == TF["game"]:
        return len(s) * GAME_ADV - 1 if s else 0
    return child_font().line_w(s)


def line_h(font):
    return GAME_LINE if font == TF["game"] else child_font().line


def wrap(text, font, scale, max_w=CANVAS_W - 2 * SAFE_X):
    out = []
    for para in text.split("\n"):
        cur = ""
        for word in para.split(" "):
            trial = word if not cur else cur + " " + word
            if cur and line_w(font, trial) * scale > max_w:
                out.append(cur)
                cur = word
            else:
                cur = trial
        out.append(cur)
    return out


def check_glyphs(text, font):
    if font == TF["game"]:
        bad = [c for c in text if c != "\n" and not 0x20 <= ord(c) <= 0x9F]
    else:
        f = child_font()
        bad = [c for c in text if c not in "\n " and ord(c) not in f.glyphs]
    if bad:
        raise TextError("no glyph for %s in the %s font" % (", ".join(repr(c) for c in sorted(set(bad))),
                                                             "game" if font == 0 else "child_hand"))


# ---- the type schedule (show_text.c glyph_frame) ----------------------------------
def glyph_frame(t0, t1, k, n):
    span = t1 - t0
    if n <= 1 or span <= 0:
        return t0
    return t0 + (2 * k * span + (n - 1)) // (2 * (n - 1))


def glyph_frames(item):
    """Every glyph's first frame (type) - what `show timing` lists."""
    n = sum(1 for c in item.text if c != "\n")
    if item.style != TS["type"]:
        return [item.t_on] * n
    return [glyph_frame(item.t0, item.t1, k, n) for k in range(n)]


class Item:
    def __init__(self, text, style, x, y, font=0, scale=1, align=TA["center"], color=2,
                 outline=0, shadow=0, flags=0, t_on=0, t_off=TX_END, t0=0, t1=0, w=0, h=0,
                 where=None, sid=None):
        self.text, self.style, self.x, self.y = text, style, x, y
        self.font, self.scale, self.align = font, scale, align
        self.color, self.outline, self.shadow, self.flags = color, outline, shadow, flags
        self.t_on, self.t_off, self.t0, self.t1, self.w, self.h = t_on, t_off, t0, t1, w, h
        self.where, self.sid = where, sid

    def words(self):
        return len(self.text.split())

    def frames_on(self, shot_len):
        end = shot_len if self.t_off == TX_END else self.t_off
        return end - self.t_on


def pack(items):
    pool = b""
    recs = []
    for it in items:
        raw = it.text.encode("latin-1")
        recs.append(fmt.TEXTITEM.pack(it.t_on, it.t_off, it.t0, it.t1, len(pool), len(raw),
                                      it.x, it.y, it.w, it.h, it.style, it.font, it.scale, it.align,
                                      it.color, it.outline, it.shadow, it.flags))
        pool += raw
    body = fmt.TEXTSET.pack(len(items), len(pool), 0) + b"".join(recs) + pool
    return fmt.pad4(body)


# ---- statements ---------------------------------------------------------------------
DEFAULTS = {          # per font: colour, outline, shadow
    TF["game"]: ("WHITE", "BLACK", "GRAY_D"),
    TF["child_hand"]: ("WHITE", "PAINT_NAVY:0", "none"),
}


def _place(at, lines, font, scale):
    h = len(lines) * line_h(font) * scale - (line_h(font) - (8 if font == 0 else child_font().h)) * scale
    if at in (None, "lower"):
        return CANVAS_W // 2, CANVAS_H - SAFE_Y - 2 - h, TA["center"]
    if at == "title":
        return CANVAS_W // 2, (CANVAS_H - h) // 2, TA["center"]
    if at == "top":
        return CANVAS_W // 2, SAFE_Y + 2, TA["center"]
    if isinstance(at, (tuple, list)):
        return int(at[0]), int(at[1]), TA["left"]
    m = re.fullmatch(r"(-?\d+),(-?\d+)", str(at))
    if not m:
        raise TextError("at=%s: use lower, title, top or X,Y" % (at,))
    return int(m.group(1)), int(m.group(2)), TA["left"]


def super_(text, style, t_on, shot_len, at="lower", over=None, cps=None, until=None,
           font="game", scale=1, align=None, color=None, outline=None, shadow=None, hop=True,
           where=None, sid=None):
    """One SUPER. over=(a, b) are shot frames (type: first and last glyph)."""
    f = TF[font] if isinstance(font, str) else font
    st = TS[style]
    if st not in (TS["type"], TS["pop"], TS["deadpan"]):
        raise TextError("style=%s: a SUPER is type, pop or deadpan" % style)
    check_glyphs(text, f)
    lines = wrap(text, f, scale) if "\n" not in text else text.split("\n")
    body = "\n".join(lines)
    x, y, al = _place(at, lines, f, scale)
    if align:
        al = TA[align]
    dc, do, ds = DEFAULTS[f]
    n = sum(1 for c in body if c != "\n")
    t0 = t1 = t_on
    if st == TS["type"]:
        if over:
            t0, t1 = over
        elif cps:
            t1 = t_on + round((n - 1) * 30 / float(cps))
        else:
            t1 = t_on + (n - 1) * 2                    # 15 cps by default
        t_on = t0
    t_off = TX_END if until in (None, "end") else int(until)
    if t_off != TX_END and t_off <= t_on:
        raise TextError("until=%d is not after the SUPER's first frame %d" % (t_off, t_on))
    if t1 >= shot_len:
        raise TextError("the type-on ends at +%d, after the shot (%d f)" % (t1, shot_len))
    return Item(body, st, x, y, f, scale, al, colors.index(color or dc), colors.index(outline or do),
                colors.index(shadow or ds), TX_HOP if (hop and st == TS["type"]) else 0,
                t_on, t_off, t0, t1, where=where, sid=sid)


def sign(text, t_on, slot, dx=0, dy=0, until=None, font="game", scale=1, color=None, outline=None,
         align="center", where=None):
    f = TF[font] if isinstance(font, str) else font
    check_glyphs(text, f)
    dc, do, _ = DEFAULTS[f]
    return Item(text, TS["sign"], int(dx), int(dy), f, scale, TA[align], colors.index(color or dc),
                colors.index(outline or do), 0, TX_SLOT, t_on, TX_END if until in (None, "end") else int(until),
                t_on, t_on, w=int(slot), where=where)


def box(x, y, w, h, color, border=0, t_on=0, t_off=TX_END):
    return Item("", TS["box"], x, y, 0, 1, 0, colors.index(color), colors.index(border) if border else 0,
                0, 0, t_on, t_off, t_on, t_on, w, h)


# ---- card templates ---------------------------------------------------------------
def _txt(text, y, scale, color="WHITE", outline="none", font="game", t_on=0, t_off=TX_END):
    f = TF[font]
    check_glyphs(text, f)
    return Item(text, TS["deadpan"], CANVAS_W // 2, y, f, scale, TA["center"], colors.index(color),
                colors.index(outline), 0, 0, t_on, t_off, t_on, t_on)


def card(template, t_on=0, until=None, **kv):
    """The cards the pilot needs. Every key has a default; see the README."""
    t_off = TX_END if until in (None, "end") else int(until)
    bg = kv.get("bg", "BLACK")
    full = box(0, -30, CANVAS_W, 240, bg, t_on=t_on, t_off=t_off)
    if template == "black":
        return [full]
    if template == "tvma":
        desc = kv.get("desc", "V")
        items = [full,
                 box(100, 28, 120, 82, "WHITE", "GRAY_L", t_on, t_off),
                 box(104, 32, 112, 74, "BLACK", 0, t_on, t_off),
                 box(106, 34, 108, 70, "WHITE", 0, t_on, t_off),
                 _txt("TV-MA", 46, 3, "BLACK", t_on=t_on, t_off=t_off),
                 _txt(desc, 80, 2, "BLACK", t_on=t_on, t_off=t_off),
                 _txt(kv.get("line1", "THIS PROGRAM IS INTENDED FOR MATURE AUDIENCES"), 126, 1,
                      t_on=t_on, t_off=t_off),
                 _txt(kv.get("line2", "AND MAY BE UNSUITABLE FOR CHILDREN UNDER 17."), 138, 1,
                      t_on=t_on, t_off=t_off)]
        if desc == "V":
            items.append(_txt(kv.get("line3", "V: FANTASY VIOLENCE ON DEMONS AND MONSTERS"), 154, 1,
                              "GRAY_L", t_on=t_on, t_off=t_off))
        return items
    if template == "act":
        n = kv.get("n", "ONE")
        items = [full, _txt("ACT " + n.upper(), 70, 3, kv.get("color", "WHITE"),
                            kv.get("outline", "PAINT_ROSE:0"), t_on=t_on, t_off=t_off)]
        if kv.get("title"):
            items.append(_txt(kv["title"], 104, 1, "GRAY_L", t_on=t_on, t_off=t_off))
        return items
    if template == "title":
        items = [full, _txt(kv.get("text", "MINGLE NIGHT"), 72, int(kv.get("scale", 3)),
                            kv.get("color", "WHITE"), kv.get("outline", "PAINT_ROSE:0"),
                            font=kv.get("font", "game"), t_on=t_on, t_off=t_off)]
        if kv.get("sub"):
            items.append(_txt(kv["sub"], 110, 1, "GRAY_L", t_on=t_on, t_off=t_off))
        return items
    if template == "text":
        sc = int(kv.get("scale", 1))
        f = TF[kv.get("font", "game")]
        lines = wrap(kv.get("text", ""), f, sc)
        y0 = (CANVAS_H - len(lines) * line_h(f) * sc) // 2
        it = _txt("\n".join(lines), y0, sc, kv.get("color", "WHITE"), kv.get("outline", "none"),
                  font=kv.get("font", "game"), t_on=t_on, t_off=t_off)
        return [full, it] if kv.get("bg") else [it]
    raise TextError("unknown card template '%s' (tvma, act, title, text, black)" % template)
