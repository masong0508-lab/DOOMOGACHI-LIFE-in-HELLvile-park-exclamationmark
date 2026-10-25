"""showtk.actors - WP6: a shot's cast, actors, crowds and tracks as player records.

The compiler side of show/engine/show_cast.c, show_actor.c and show_crowd.c
(SHOW_ENGINE_SPEC.md sec.7.3, 8.4, 8.5, 9.3). Every record here is the byte
image of a struct in show/include/show/show_actor.h (little-endian, sizes
static-asserted on both sides; show/tests/engine/wp6 checks them against the
PS1's own reading).

    from showtk.actors import ShotCast
    sc = ShotCast()                       # one per shot
    sc.actor("mayor", who="mayor", kind="figure", at=(196, 96), face="L", idle="on")
    sc.crowd("plaza", looks="kset:giggle_adults", n=150, form="scatter", area=(0, 100, 320, 60))
    sc.mouth("mayor", start=40, track="XXMMEEEFFEECCCXX")
    sc.op("mayor", "headturn", 64, 2)     # -> (SHOW_SOP_ACTOR args) for the BOP_SHOT arg table
    secs = sc.pack()                      # {'LOOK': b, 'ACTR': b, 'CRWD': b, 'TRAK': b, 'FLIP': b}

Statement syntax (README "Staging reference: WP6"): `actor`, `crowd ... end`,
and the timed ops expr / blink / face / move / walk / pop / squash /
headturn / pose / state / show / hide / freeze / thaw / still / panic, plus
`crowd <id> freeze|thaw|pop|face|speed|show|hide`. parse_actor() and
parse_crowd() turn a statement's key=value words into these calls.
"""
import struct

# ---- enums mirrored from show_actor.h (checked by tests/engine/wp6) ---------------
KIND = {"bust": 0, "figure": 1, "chibi": 2, "fighter": 3}
IDLE = {"on": 0, "still": 1, "frozen": 2, "none": 3}
FACE = {"R": 0, "L": 1, "front": 2, "back": 3}
VISEMES = "XMCEAOUF"                       # VIS_X .. VIS_F
NO_MOUTH = 0xFF
CHAN = {"mouth": 0, "expr": 1, "blink": 2}
KEEP = 0xFF
FORM = {"lane": 0, "grid": 1, "scatter": 2}
CDIR = {"front": 0, "L": 1, "R": 2, "back": 3}
CF_FRONT, CF_FLIPMIX, CF_BOB, CF_NOCLOCK = 1, 2, 4, 8
SHOP = {"expr": 0, "blink": 1, "face": 2, "move": 3, "walk": 4, "pop": 5, "squash": 6,
        "headturn": 7, "pose": 8, "state": 9, "twitch": 10, "show": 11}
EASE = {"LINEAR": 0, "SMOOTH": 1, "IN": 2, "OUT": 3, "BOUNCE": 4, "INOUT": 1}
SHCO = {"freeze": 0, "thaw": 1, "pop": 2, "face": 3, "speed": 4, "show": 5}
STATE = {"normal": 1, "possessed": 2, "dead": 4, "ghost": 5}       # RES_*
FLIP_MODE = {"replace": 0, "over": 1}
SHOW_SOP_ACTOR, SHOW_SOP_CROWD = 0x0A0, 0x0C0   # show op numbers (BOP_SHOT a = 0x4000 | op)

ACTOR_SIZE, CROWD_SIZE, TRACK_HDR, FLIP_HDR, LOOK_SIZE = 32, 48, 8, 12, 48
MAX_LOOKS, MAX_ACTORS, MAX_CROWDS, MAX_K, MAX_N, BEAT_ACTORS = 256, 16, 8, 48, 256, 8


class CastError(Exception):
    pass


def _cat():
    from .art.cast.catalog import catalog
    return catalog()


def sym(name):
    """an engine constant by its repo name (EXPR_HAPPY, MOUTH_FLAT, TWT_HOP2, ...)"""
    if isinstance(name, int):
        return name
    v = _cat().value(name)
    if v is None:
        raise CastError("unknown engine constant %s" % name)
    return v


def default_visemes():
    """sec.9.3: X rest (the look's own mouth), M FLAT, C TEETH, E GRIN, A OPEN,
    O O, U POUT, F TEETH. series/visemes.shw (AUDIO) overrides per look family."""
    return [NO_MOUTH] + [sym(m) for m in ("MOUTH_FLAT", "MOUTH_TEETH", "MOUTH_GRIN", "MOUTH_OPEN",
                                          "MOUTH_O", "MOUTH_POUT", "MOUTH_TEETH")]


def beak_visemes():
    """beaked species keep MOUTH_BEAK and flap open / closed only"""
    beak, opn = sym("MOUTH_BEAK"), sym("MOUTH_OPEN")
    return [NO_MOUTH, beak, beak, opn, opn, opn, beak, beak]


# ---- mouth tracks ------------------------------------------------------------------
def mouth_values(track):
    """'XXMMEE..' -> [0, 0, 1, 1, 3, 3, ...]; '.' = keep (no override that frame)"""
    out = []
    for ch in track:
        if ch in " \t\n":
            continue
        if ch == ".":
            out.append(KEEP)
            continue
        i = VISEMES.find(ch.upper())
        if i < 0:
            raise CastError("mouth track: %r is not a viseme (%s or .)" % (ch, VISEMES))
        out.append(i)
    return out


def runs(values):
    """run-length code: [(len 1..255, value)]"""
    out = []
    for v in values:
        if out and out[-1][1] == v and out[-1][0] < 255:
            out[-1] = (out[-1][0] + 1, v)
        else:
            out.append((1, v))
    return out


def pack_track(actor, chan, start, values):
    rl = runs(values)
    b = struct.pack("<BBHHH", actor, CHAN[chan] if isinstance(chan, str) else chan, start,
                    len(rl), len(values))
    b += b"".join(struct.pack("<BB", n, v) for n, v in rl)
    return b + b"\0" * (-len(b) % 4)


# ---- records -----------------------------------------------------------------------
def pack_actor(kind, look, x, y, face="R", expr=0, idle="on", phase=0, px=1, status=1,
               clock=0xFF, flags=0, view=0, wframe=0xFF, vis=None):
    vis = list(vis if vis is not None else default_visemes())
    if len(vis) != 8:
        raise CastError("a viseme map has 8 entries (%s)" % VISEMES)
    return struct.pack("<BBBBhhBBBBBBBB8B8x", KIND[kind] if isinstance(kind, str) else kind, look,
                       FACE[face] if isinstance(face, str) else face, sym(expr), int(x), int(y),
                       IDLE[idle] if isinstance(idle, str) else idle, phase & 255, px, status,
                       clock, flags, view, wframe, *vis)


def pack_crowd(form, look_base, k, n, seed=0, x=0, y=0, w=0, h=0, spacing=16.0, row=0.0,
               speed=0.0, dir="front", ripple=0, step=4, clock=0xFF, flags=0, px=1, special=()):
    """speed in px/f (signed: + walks right), spacing/row in px (Q4 in the record)"""
    sp = list(special)[:4]
    idx = [s[0] for s in sp] + [0] * (4 - len(sp))
    lk = [s[1] for s in sp] + [0] * (4 - len(sp))
    fl = [s[2] for s in sp] + [0] * (4 - len(sp))
    return struct.pack("<BBBBHHhhhhhhiBBBBBB2x4H4B4B", FORM[form] if isinstance(form, str) else form,
                       look_base, k, CDIR[dir] if isinstance(dir, str) else dir, n, seed & 0xFFFF,
                       int(x), int(y), int(w), int(h), int(round(spacing * 16)), int(round(row * 16)),
                       int(round(speed * 65536)), ripple, step, clock, flags, px, len(sp),
                       *idx, *lk, *fl)


def pack_flip(w, h, frames, holds, mode="replace", loop=False, dx=0, dy=0):
    """frames: [bytes w*h] (palette indices, 0 transparent); holds: frames per drawing"""
    if len(frames) != len(holds) or not frames:
        raise CastError("a flipbook needs one hold per drawing")
    b = struct.pack("<HHBBBxbb2x", w, h, len(frames), FLIP_MODE[mode] if isinstance(mode, str) else mode,
                    1 if loop else 0, dx, dy)
    hb = bytes(holds)
    b += hb + b"\0" * (-len(hb) % 4)
    pix = b"".join(bytes(f) for f in frames)
    if len(pix) != w * h * len(frames):
        raise CastError("flipbook drawings must be %dx%d" % (w, h))
    return b + pix + b"\0" * (-len(pix) % 4)


# ---- one shot ------------------------------------------------------------------------
class ShotCast:
    """Collects one shot's LOOK slots, actors, crowds, tracks and flipbooks."""

    def __init__(self, bible=None):
        self._bible = bible
        self.looks = []          # [48-byte look]
        self.look_ids = {}       # key -> slot
        self.actors = []         # [(id, dict)]
        self.crowds = []         # [(id, dict)]
        self.tracks = []         # [bytes]
        self.flips = []          # [(id, bytes)]

    @property
    def bible(self):
        if self._bible is None:
            from .art.cast.lookdata import Bible
            self._bible = Bible()
        return self._bible

    # LOOK slots
    def slot(self, key, look_bytes):
        if key in self.look_ids:
            return self.look_ids[key]
        if len(look_bytes) != LOOK_SIZE:
            raise CastError("%s: a DoomoLook is 48 bytes" % key)
        if len(self.looks) >= MAX_LOOKS:
            raise CastError("more than %d looks in one shot" % MAX_LOOKS)
        self.looks.append(bytes(look_bytes))
        self.look_ids[key] = len(self.looks) - 1
        return self.look_ids[key]

    def char_slot(self, who):
        b = self.bible.char_look_bytes(who)
        if b is None:
            raise CastError("char %s has no DoomoLook (a sprite or voice-only cast member)" % who)
        return self.slot("char:" + who, b)

    def char_phase(self, who):
        c = self.bible.chars.get(who)
        if c and "phase" in c.opts:
            return int(c.opts["phase"][0])
        return 0

    def actor_index(self, aid):
        for i, (a, _) in enumerate(self.actors):
            if a == aid:
                return i
        raise CastError("no actor %s in this shot" % aid)

    def crowd_index(self, cid):
        for i, (c, _) in enumerate(self.crowds):
            if c == cid:
                return i
        raise CastError("no crowd %s in this shot" % cid)

    def actor(self, aid, who, kind="figure", at=(160, 150), face="R", expr="EXPR_NEUTRAL", idle=None,
              px=1, state="normal", look=None, phase=None, vis=None, hidden=False, view=0, wframe=None):
        if len(self.actors) >= MAX_ACTORS:
            raise CastError("more than %d actors in one shot" % MAX_ACTORS)
        slot = self.slot("look:" + look[0], look[1]) if look else self.char_slot(who)
        if idle is None:
            c = self.bible.chars.get(who)
            idle = c.opts["idle"][0] if c and "idle" in c.opts else "on"
        idx = len(self.actors)
        rec = dict(kind=kind, look=slot, x=at[0], y=at[1], face=face, expr=expr, idle=idle,
                   phase=self.char_phase(who) if phase is None else phase, px=px, status=STATE[state],
                   clock=idx if idx < BEAT_ACTORS else 0xFF, flags=1 if hidden else 0, view=view,
                   wframe=0xFF if wframe is None else wframe, vis=vis)
        self.actors.append((aid, rec))
        return idx

    def crowd(self, cid, looks, n, form="lane", seed=0, at=(0, 0), area=(0, 0, 0, 0), spacing=16.0,
              row=12.0, speed=0.0, dir="front", ripple=0, cycle="walk4", clock=None, front=False,
              flipmix=False, bob=False, still=False, px=1, special=()):
        """looks: 'kset:<id>' | [(label, 48 bytes)]; special: [(member, char id)] never moving"""
        if len(self.crowds) >= MAX_CROWDS:
            raise CastError("more than %d crowds in one shot" % MAX_CROWDS)
        if isinstance(looks, str):
            if not looks.startswith("kset:"):
                raise CastError("crowd %s: looks=kset:<id>" % cid)
            looks = self.bible.kset_looks(looks[5:])
        if not 1 <= len(looks) <= MAX_K:
            raise CastError("crowd %s: %d looks (K is 1..%d, spec 8.5)" % (cid, len(looks), MAX_K))
        if not 1 <= n <= MAX_N:
            raise CastError("crowd %s: n=%d (1..%d)" % (cid, n, MAX_N))
        slots = [self.slot("member:" + lab, b) for lab, b in looks]      # shared by every crowd
        base = slots[0]
        if slots != list(range(base, base + len(slots))):
            raise CastError("crowd %s: its looks must take consecutive LOOK slots (declare it before "
                            "anything that shares them)" % cid)
        sp = [(m, self.char_slot(who), CF_NOCLOCK) for m, who in special]
        if isinstance(cycle, str):
            step = int(cycle[4:] or 4) if cycle.startswith("walk") else 4
        else:
            step = int(cycle)
        flags = (CF_FRONT if front else 0) | (CF_FLIPMIX if flipmix else 0) | (CF_BOB if bob else 0) | \
                (CF_NOCLOCK if still else 0)
        x, y = at
        if form != "lane":
            x, y = area[0], area[1]
        rec = dict(form=form, look_base=base, k=len(looks), n=n, seed=seed, x=x, y=y,
                   w=area[2], h=area[3], spacing=spacing, row=row, speed=speed, dir=dir,
                   ripple=ripple, step=step, clock=0xFF if clock is None else clock, flags=flags,
                   px=px, special=sp)
        self.crowds.append((cid, rec))
        return len(self.crowds) - 1

    def mouth(self, aid, start, track):
        self.tracks.append(pack_track(self.actor_index(aid), "mouth", start, mouth_values(track)))

    def expr_track(self, aid, start, values):
        self.tracks.append(pack_track(self.actor_index(aid), "expr", start, [sym(v) for v in values]))

    def blink_track(self, aid, start, text):
        """'..11..' : 1 shut, 0 open, . the clock's own blink"""
        vals = [KEEP if c == "." else int(c) for c in text if c in "01."]
        self.tracks.append(pack_track(self.actor_index(aid), "blink", start, vals))

    def flipbook(self, fid, w, h, frames, holds, mode="replace", loop=False, dx=0, dy=0):
        self.flips.append((fid, pack_flip(w, h, frames, holds, mode, loop, dx, dy)))
        return len(self.flips) - 1

    def flip_index(self, fid):
        for i, (f, _) in enumerate(self.flips):
            if f == fid:
                return i
        raise CastError("no flipbook %s in this shot" % fid)

    # timed ops -> the args of one BOP_SHOT show op: (SHOW_SOP_*, [args])
    def op(self, aid, name, *args, **kw):
        a = self.actor_index(aid)
        o = SHOP[name]
        v = [0, 0, 0, 0]
        if name == "expr":
            v[0] = sym(args[0])
        elif name == "blink":
            v[0] = int(args[0]) if args else 0
        elif name == "face":
            v[0] = FACE[args[0]]
        elif name in ("move", "walk"):
            v[0] = int(kw.get("x", args[0] if args else 0))
            v[1] = int(kw.get("y", args[1] if len(args) > 1 and name == "move" else 0))
            v[2] = int(kw.get("over", 1))
            v[3] = EASE[kw.get("ease", "SMOOTH" if name == "move" else "LINEAR")]
        elif name == "pop":
            v[0] = sym(args[0])
        elif name == "squash":
            v[0] = int(args[0])
            v[2] = int(kw.get("over", 1))
        elif name == "headturn":
            v[0] = int(args[0])
            v[2] = int(args[1]) if len(args) > 1 else 2
        elif name == "pose":
            v[0] = -1 if args[0] in ("off", None) else self.flip_index(args[0])
            v[1] = int(round(float(kw.get("speed", 1.0)) * 256))
        elif name == "state":
            v[0] = STATE[args[0]]
        elif name in ("show", "twitch"):
            v[0] = 1 if (not args or args[0] in ("on", 1, True)) else 0
        return SHOW_SOP_ACTOR, [a, o] + v

    def crowd_op(self, cid, name, *args):
        c = self.crowd_index(cid)
        a = 0
        if name == "pop":
            a = sym(args[0])
        elif name == "face":
            a = CDIR[args[0]]
        elif name == "speed":
            a = int(round(float(args[0]) * 256))
        elif name == "show":
            a = 1 if not args or args[0] in ("on", 1, True) else 0
        return SHOW_SOP_CROWD, [c, SHCO[name], a]

    def pack(self):
        actr = b"".join(pack_actor(**r) for _, r in self.actors)
        crwd = b"".join(pack_crowd(**r) for _, r in self.crowds)
        return {"LOOK": b"".join(self.looks), "ACTR": actr, "CRWD": crwd,
                "TRAK": b"".join(self.tracks), "FLIP": b"".join(b for _, b in self.flips)}

    # the .SHW records (shw_format.h): LOOK 1, ACTOR 32, TRACK 33, CROWD 34, FLIP private 0x160
    SHWR_WP6_FLIP = 0x100 + 0x60

    def to_chunk(self, chunk, ops=()):
        """add this shot's records to a shw_format.Chunk; ops: [(at, (sop, args)), ...]
        from op()/crowd_op() become BOP_SHOT events with their argument rows.
        Returns the beat events (at, anchor, op, a, b) for the caller's add_beat()."""
        from . import shw_format as F
        s = self.pack()
        if s["LOOK"]:
            chunk.add(F.SHWR_LOOK, len(self.looks), s["LOOK"])
        if s["ACTR"]:
            chunk.add(F.SHWR_ACTOR, len(self.actors), s["ACTR"])
        if s["TRAK"]:
            chunk.add(F.SHWR_TRACK, len(self.tracks), s["TRAK"])
        if s["CRWD"]:
            chunk.add(F.SHWR_CROWD, len(self.crowds), s["CRWD"])
        if s["FLIP"]:
            chunk.add(self.SHWR_WP6_FLIP, len(self.flips), s["FLIP"])
        words, events = [], []
        for at, (sop, args) in ops:
            events.append(F.show_op(sop, len(words), at))
            words += list(args) + [0] * (6 - len(args))
        if words:
            chunk.add_args(words)
        return events

    def blocks_estimate(self):
        """portrait-pool blocks this shot keeps live (the planner's PLAN-POOL count, spec 8.4)"""
        per = {"bust": 7, "figure": 10, "chibi": 1, "fighter": 4}
        n = 0
        for _, r in self.actors:
            # rest + 6 mouths, x2 blink, x2 breath (busts and figures)
            n += per[r["kind"]] * (28 if r["kind"] in ("bust", "figure") else 4)
        for _, r in self.crowds:
            frames = 4 if r["speed"] else 1
            n += r["k"] * frames
        return n


# ---- statement words -> calls (the compiler's stage pass) -------------------------------
def _xy(v):
    a, b = v.split(",")
    return int(a), int(b)


def parse_actor(sc, aid, kv):
    """`actor <id> who=<char> kind=bust|figure|chibi|fighter at=x,y [face=L|R|front|back]
    [expr=EXPR_*] [idle=on|still|frozen|none] [px=1|2|3] [state=normal|possessed|ghost|dead]
    [hidden]`"""
    return sc.actor(aid, kv["who"], kind=kv.get("kind", "figure"), at=_xy(kv.get("at", "160,150")),
                    face=kv.get("face", "R"), expr=kv.get("expr", "EXPR_NEUTRAL"), idle=kv.get("idle"),
                    px=int(kv.get("px", 1)), state=kv.get("state", "normal"), hidden="hidden" in kv)


def parse_crowd(sc, cid, kv):
    """`crowd <id> looks=kset:<id> n=N [form=lane|grid|scatter] ... end` (README)"""
    area = tuple(int(v) for v in kv["area"].split(",")) if "area" in kv else (0, 0, 0, 0)
    special = []
    if "still" in kv and "=" not in kv["still"]:
        pass
    for part in kv.get("special", "").split(",") if kv.get("special") else []:
        m, who = part.split(":")
        special.append((int(m), who))
    rip = kv.get("ripple", "0")
    rip = int(rip.split("..")[-1])
    return sc.crowd(cid, kv["looks"], int(kv["n"]), form=kv.get("form", "lane"), seed=int(kv.get("seed", 0)),
                    at=_xy(kv.get("from", kv.get("at", "0,0"))), area=area,
                    spacing=float(kv.get("spacing", 16)), row=float(kv.get("row", 12)),
                    speed=float(kv.get("speed", 0)), dir=kv.get("dir", "front"), ripple=rip,
                    cycle=kv.get("cycle", "walk4"), clock=int(kv["clock"]) if "clock" in kv else None,
                    front=kv.get("layer") == "front", flipmix=kv.get("flip") == "mix",
                    bob=kv.get("bob") == "on", px=int(kv.get("px", 1)), special=special)
