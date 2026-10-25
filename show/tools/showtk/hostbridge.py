"""hostbridge.py - staged host shots -> one lane-3 host plane (owner: show, ENGINE).

A shot whose set is kind=overworld|room|view3d (series/sets/world3d.shw) gets ONE
host plane (SHWP_OVERWORLD / SHWP_ROOM / SHWP_VIEW3D) whose payload is the record
showtk/hosts.py packs. This module is the glue the compiler (showc.py) calls:

  * the series set block (`set ... kind=view3d map=MAPID_X`, `obj ... furn=`,
    `cam3d <rail> from= to= look= z=eye`, `cam3d room yaw= zoom=`, `variant`)
    becomes the host's own set syntax (`set`, `furn`, `path`);
  * the staged host ops of a shot become `world ...` statements:
      hud on|off|slam / hud set hearts= shells= plate= / hud face <f> / hud sticker
      on|blow / hud spin to=<weapon> / hud heart pop
      mon <id> spawn sprite=<kind> at=x,y [scale=] / die gore= [fall=flat] /
      lunge|swing|attack / flinch|damage
      lens <gib|splat|paint|clear|drip> [over=]      gore CUTE|BRUTAL [over=]
      cam3d rail=<path> from=+a to=+b [part=i/n]     cam3d turn yaw=+N over= / tilt up=N
      cam3d room yaw= zoom= [at=room:<anchor>]       door open|close|wobble
      world <any hosts.py statement>                 (passed through)
    an `sfx FX_BLAM` in a VIEW3D shot also fires the weapon (muzzle flash, kick);
  * screen anchors: `room:<anchor>` / `room:x,y` (ROOM, the cottage's floor in
    screen px for the show actors drawn over the room), `mon:<id>` (VIEW3D, the
    monster projected through the camera at that frame) and world-cell `at=` on
    a VIEW3D prop (projected at its first frame).

The show actors of a ROOM shot stay lane-2 actors drawn over the host plane
(their poses, mouths and blinks keep working); see show/DECISIONS.md.
"""
import math
import re
import zlib

from . import hosts as H

HOST_KWS = {"hud", "mon", "lens", "gore", "door", "world", "cam3d"}

MON_SPR = {"puff_demon": ("SPR_PUFF_DEMON", 0), "big_puff": ("SPR_PUFF_DEMON", 0),
           "cardboard_ghost": ("SPR_VAPE_WRAITH", 0), "impling": ("SPR_IMPLING", 0),
           "wraith": ("SPR_VAPE_WRAITH", 0)}
HUD_FACE = {"glance": "left", "look_left": "left", "look_right": "right", "look_down": "idle", "idle": "idle",
            "grin": "grin", "wince": "wince", "glare": "glare", "left": "left", "right": "right"}
LENS = {"gib": (9, 2), "splat": (11, 2), "paint": (14, 8), "drip": (12, 4)}

# ROOM: the cottage living room as the ROOM host frames it (320x180, yaw 0 = looking
# at the door wall, yaw 512 = the window wall); show px of a standing actor's feet.
ROOM_ANCHORS = {
    "door": (160, 112), "door_out": (160, 118), "door_in": (160, 136), "door_lock": (176, 96),
    "door_side": (196, 140), "window": (112, 92), "window_sill_l": (100, 100), "window_sill_r": (124, 100),
    "centre": (160, 132), "center": (160, 132),
}
PARTS = {"head": (0, -30), "hand": (10, -14), "arm": (8, -18), "feet": (0, 0)}


def _map_name(v):
    v = (v or "").strip()
    if v.upper().startswith("MAPID_"):
        v = v[6:]
    return v.lower()


class HostShot:
    def __init__(self, sc):
        self.sc = sc
        self.kind = sc.set_kind
        self.lines = []          # hosts.Line (world statements, setup first)
        self.setup = []
        self.mons = {}           # id -> dict(at=(x, y), shown=f or None)
        self.rails = {}          # name -> dict(points, look)
        self.cam_init = None
        self.set_block = self.build_set()
        self.rail_spans = []     # (f0, f1, rail, part)

    # -- the set block --------------------------------------------------------------
    def build_set(self):
        sd = self.sc.setdef
        st = sd.stmt
        variant = (self.sc.shot.kv.get("set") or "").partition(":")[2] or None
        head = {k: v for k, v in st.kv.items() if k not in ("desc", "px", "kind", "home", "preset", "map")}
        head["kind"] = self.kind
        if self.kind != "room":
            head["map"] = _map_name(st.get("map"))
        body = []
        for c in st.children:
            if c.kw == "variant" and c.pos and c.pos[0] == variant:
                for k, v in c.kv.items():
                    if k in ("daylight", "tint", "tint_level"):
                        head[k] = v
            elif c.kw == "obj" and c.get("furn") and self.kind == "room":
                body.append("furn %s at=%s rot=%s" % (c.get("furn"), c.get("at", "0,0"), c.get("rot", "0")))
            elif c.kw == "cam3d" and c.pos:
                name = c.pos[0]
                if name == "room":
                    self.cam_init = dict(c.kv)
                    if "daylight" in c.kv and "daylight" not in head:
                        head["daylight"] = c.kv["daylight"]
                    continue
                if c.get("from") and c.get("to"):
                    pts = [c.get("from"), c.get("to")]
                    self.rails[name] = dict(points=[[float(v) for v in p.split(",")] for p in pts],
                                            look=c.get("look"))
                    if self.kind == "view3d":
                        body.append("path %s %s %s eye=%d%s" % (name, pts[0], pts[1], H.EYE,
                                                                (" look=%s" % c.get("look")) if c.get("look") else ""))
                    elif self.kind == "overworld" and self.cam_init is None:
                        self.cam_init = dict(ow_from=pts[0], ow_to=pts[1], ease=c.get("ease", "INOUT"))
        if "tint" in head and head["tint"].upper() == "TINT_NONE":
            head.pop("tint")
            head.pop("tint_level", None)
        text = "set %s %s" % (sd.id, " ".join("%s=%s" % kv for kv in head.items()))
        out = [H.Line(st.file, st.line, text)] + [H.Line(st.file, st.line, b) for b in body]
        out.append(H.Line(st.file, st.line, "end"))
        return out

    # -- helpers ----------------------------------------------------------------------
    def add(self, f, text, c):
        self.lines.append(H.Line(c.file, c.line, ("+%d " % f if f is not None else "") + text))

    def cam_at(self, f):
        """VIEW3D camera (x, y, yaw radians) at shot frame f, from the rail spans"""
        span = None
        for s in self.rail_spans:
            if s[0] <= f:
                span = s
        if span is None and self.rail_spans:
            span = self.rail_spans[0]
        if span is None or span[2] not in self.rails:
            r = next(iter(self.rails.values()), None)
            if r is None:
                return 0.0, 0.0, 0.0
            span = (0, max(1, self.sc.shot.len), None, (0.0, 1.0))
        else:
            r = self.rails[span[2]]
        f0, f1, _, part = span
        u = max(0.0, min(1.0, (f - f0) / float(max(1, f1 - f0))))
        u = part[0] + (part[1] - part[0]) * u
        (ax, ay), (bx, by) = r["points"][0][:2], r["points"][-1][:2]
        x, y = ax + (bx - ax) * u, ay + (by - ay) * u
        if r.get("look"):
            lx, ly = [float(v) for v in r["look"].split(",")[:2]]
            yaw = math.atan2(ly - y, lx - x)
        else:
            yaw = math.atan2(by - ay, bx - ax)
        return x, y, yaw

    def project(self, wx, wy, f, z=0.0):
        cx, cy, yaw = self.cam_at(f)
        dx, dy = wx - cx, wy - cy
        fwd = dx * math.cos(yaw) + dy * math.sin(yaw)
        lat = -dx * math.sin(yaw) + dy * math.cos(yaw)
        fwd = max(0.6, fwd)
        sx = 160 + lat / fwd * 160
        sy = 90 + (H.EYE / 64.0 - z) / fwd * 160 * 0.5
        return int(round(max(-40, min(360, sx)))), int(round(max(0, min(200, sy))))

    def anchor(self, txt, f=0):
        """'room:door' / 'room:2,1' / 'mon:big' / 'follow:sunny.head' -> (x, y) show px, or None"""
        kind, _, name = str(txt).partition(":")
        if kind == "room":
            if name in ROOM_ANCHORS:
                return ROOM_ANCHORS[name]
            m = re.fullmatch(r"(-?[\d.]+),(-?[\d.]+)", name)
            if m:                                   # tile -> the floor's screen px (iso-ish)
                tx, ty = float(m.group(1)), float(m.group(2))
                return int(round(160 + (tx - ty) * 22)), int(round(118 + (tx + ty) * 8))
            return ROOM_ANCHORS["centre"]
        if kind == "mon":
            mn = self.mons.get(name)
            if mn is None:
                return 160, 96
            return self.project(mn["at"][0], mn["at"][1], f, 0.5)
        if kind == "follow":
            who, _, part = name.partition(".")
            sc = self.sc
            ai = sc.actor_ix.get(who)
            if ai is None:
                hits = [i for i, a in enumerate(sc.actors) if a["who"] == who]
                ai = hits[0] if hits else None
            if ai is None:
                return None
            a = sc.actors[ai]
            dx, dy = PARTS.get(part or "feet", (0, -16))
            return a["x"] + dx, a["y"] + dy
        return None

    def mon(self, name, f, c, at=None, sprite="puff_demon", scale=None):
        if name in self.mons:
            return self.mons[name]
        spr, _ = MON_SPR.get(sprite, ("SPR_PUFF_DEMON", 0))
        if at is None:                              # undeclared: it stands in front of the camera
            cx, cy, yaw = self.cam_at(f)
            k = len(self.mons)
            d = 2.5 + (k % 3) * 0.6
            off = ((k % 5) - 2) * 0.45
            at = (cx + math.cos(yaw) * d - math.sin(yaw) * off, cy + math.sin(yaw) * d + math.cos(yaw) * off)
        m = dict(at=at, spr=spr)
        self.mons[name] = m
        sc_ = (" scale=%s" % scale) if scale else ""
        self.setup.append(H.Line(c.file, c.line, "world actor %s kind=monster spr=%s at=%.2f,%.2f hidden%s"
                                 % (name, spr, at[0], at[1], sc_)))
        return m

    # -- ops ----------------------------------------------------------------------------
    def op(self, f, c):
        kw, w, kv = c.kw, list(c.words), c.kv
        if kw == "world":
            self.add(f, "world " + " ".join(c.text().split()[1 + (1 if c.time else 0):]), c)
            return True
        if kw == "cam3d":
            return self.cam3d(f, c, w, kv)
        if kw == "hud":
            return self.hud(f, c, w, kv)
        if kw == "mon":
            return self.mon_op(f, c, w, kv)
        if kw == "lens":
            if self.kind != "view3d":
                return True
            what = w[0] if w else "clear"
            over = int(re.sub(r"\D", "", kv.get("over", "8")) or 8)
            if what == "clear":
                self.add(f, "world lens 0 over=%d" % over, c)
            elif what in LENS:
                lvl, ov = LENS[what]
                self.add(f, "world lens %d over=%d tint=TINT_RED" % (lvl, ov), c)
            return True
        if kw == "gore":
            g = (w[0] if w else "CUTE").upper()
            if self.kind == "view3d":
                self.add(f, "world gore %s" % g, c)
            return True
        if kw == "door":
            # the ROOM host has no door hinge yet: the jelly door reads as a soft camera wobble
            amp = int(re.sub(r"\D", "", kv.get("amp", "20")) or 20)
            self.add(f, "world shake %s" % ("SHK_POW" if amp >= 30 else "SHK_TAP"), c)
            return True
        return False

    def cam3d(self, f, c, w, kv):
        if "rail" in kv:
            if self.kind != "view3d":
                return True
            f0 = int(str(kv.get("from", "+%d" % f)).lstrip("+") or f)
            f1 = int(str(kv.get("to", "+%d" % self.sc.shot.len)).lstrip("+"))
            f1 = min(f1, self.sc.shot.len)
            if f1 <= f0:
                f1 = min(self.sc.shot.len, f0 + 1)
            part = (0.0, 1.0)
            ex = ""
            if "part" in kv:
                i, n = [int(v) for v in kv["part"].split("/")]
                part = ((i - 1) / n, i / n)
                ex = " part=%s" % kv["part"]
            self.rail_spans.append((f0, f1, kv["rail"], part))
            self.add(f0, "cam3d rail=%s to=+%d%s" % (kv["rail"], f1, ex), c)
            return True
        sub = w[0] if w else ""
        if sub in ("turn", "tilt"):
            if self.kind == "view3d":
                o = " ".join("%s=%s" % (k, v.lstrip("+") if k == "yaw" else v) for k, v in kv.items())
                self.add(f, "world %s %s" % (sub, o), c)
            return True
        if sub == "room":
            if self.kind == "room":
                yaw = kv.get("yaw", "0")
                zoom = 1 if int(kv.get("zoom", "1")) >= 2 else 0
                punch = 256 + 24 * max(0, int(kv.get("zoom", "1")) - 1)
                pan = "0,0"
                at = kv.get("at")
                if at:
                    p = self.anchor(at, f)
                    if p:
                        pan = "%d,%d" % (max(-100, min(100, (p[0] - 160) // 2)), max(-100, min(100, (p[1] - 120) // 2)))
                self.add(f, "world cam yaw=%s zoom=%d punch=%d pan=%s" % (yaw, zoom, punch, pan), c)
            return True
        return True                                 # other cam3d forms: the set's own rails

    def hud(self, f, c, w, kv):
        if self.kind != "view3d":
            return True
        sub = w[0] if w else ""
        if sub in ("on", "off", "slam"):
            self.add(f, "world hud %s" % ("slam" if "slam" in w else sub), c)
        elif sub == "set":
            o = []
            for k in ("hearts", "shells"):
                if k in kv:
                    o.append("%s=%s" % (k, kv[k]))
            if "plate" in kv:
                o.append("plate=%s" % ("brutal" if "slaughter" in kv["plate"] or "saw" in kv["plate"] else "cute"))
            if o:
                self.add(f, "world hud " + " ".join(o), c)
        elif sub == "face":
            fc = w[1] if len(w) > 1 else "idle"
            if fc == "horn":
                self.add(f, "world hud horn", c)
            else:
                self.add(f, "world hud face=%s" % HUD_FACE.get(fc, "idle"), c)
        elif sub == "sticker":
            st = w[1] if len(w) > 1 else "on"
            self.add(f, "world hud sticker=%s" % ("blow" if st == "blow" else "on"), c)
        elif sub == "spin":
            to = kv.get("to", "pump")
            self.add(f, "world weapon %s" % (to if to in H.WEAPONS else "saw"), c)
        elif sub == "heart":
            self.add(f, "world hurt", c)
        return True

    def mon_op(self, f, c, w, kv):
        if self.kind != "view3d" or not w:
            return True
        name, verb = w[0], (w[1] if len(w) > 1 else "spawn")
        if verb == "spawn":
            at = None
            if "at" in kv:
                at = tuple(float(v) for v in kv["at"].split(",")[:2])
            new = name not in self.mons
            self.mon(name, f, c, at, kv.get("sprite", "puff_demon"), kv.get("scale"))
            if new:
                self.add(f, "world show %s" % name, c)
            return True
        if name not in self.mons:
            self.mon(name, f, c)
            self.add(max(0, f - 20), "world show %s" % name, c)
        if verb == "die":
            g = kv.get("gore", "")
            if kv.get("fall") == "flat" or g == "none":
                self.add(f, "world fall %s" % name, c)
            else:
                self.add(f, "world kill %s%s" % (name, " overkill" if g not in ("smear", "mist") else ""), c)
        elif verb in ("lunge", "swing", "attack"):
            self.add(f, "world attack %s" % name, c)
        elif verb in ("flinch", "damage", "pain"):
            self.add(f, "world pain %s frames=%d" % (name, 10 if verb == "damage" else 6), c)
        elif verb in ("hide",):
            self.add(f, "world hide %s" % name, c)
        return True

    def sfx(self, f, c):
        if self.kind == "view3d" and c.words and c.words[0] in ("FX_BLAM", "FX_SHOTGUN"):
            self.add(f, "world fire", c)

    def eyes(self, f, c):
        """`eyes <mon> L|R|D|U`: the monster turns to face that way"""
        if self.kind == "view3d" and c.words and c.words[0] in self.mons:
            d = c.words[1] if len(c.words) > 1 else "D"
            self.add(f, "world face %s %s" % (c.words[0], d if d.upper() in H.DIRS else "D"), c)
            return True
        return False

    # -- the plane ------------------------------------------------------------------------
    def pack(self):
        sc = self.sc
        lines = list(self.setup)
        if self.kind == "room" and self.cam_init is not None and not any(
                l.text.split()[1:3] == ["world", "cam"] and l.text.startswith("+0 ") for l in self.lines):
            ci = self.cam_init
            z = 1 if int(ci.get("zoom", "1")) >= 2 else 0
            lines.append(H.Line(self.set_block[0].file, self.set_block[0].line,
                                "world cam yaw=%s zoom=%d" % (ci.get("yaw", "0"), z)))
        if self.kind == "overworld" and not any("world cam" in l.text for l in self.lines):
            ci = self.cam_init or {}
            if ci.get("ow_from"):
                lines.append(H.Line(self.set_block[0].file, self.set_block[0].line, "world cam at=%s" % ci["ow_from"]))
                lines.append(H.Line(self.set_block[0].file, self.set_block[0].line,
                                    "+0 world cam to %s over=%d ease=%s" % (ci["ow_to"], sc.shot.len,
                                                                          ci.get("ease", "INOUT").upper())))
        if self.kind == "view3d" and not self.rail_spans and self.rails:
            name = next(iter(self.rails))
            lines.append(H.Line(self.set_block[0].file, self.set_block[0].line,
                                "+0 cam3d rail=%s to=+%d" % (name, sc.shot.len)))
        # room cam keys: `world cam to` keys must follow a setup key at +0
        ordered = sorted(self.lines, key=lambda l: int(l.text.split()[0][1:]) if l.text.startswith("+") else -1)
        lines += ordered
        kids = any(a.get("age") == "child" for a in sc.actors) or any(r.get("age_name") == "child" for r in sc.crowds)
        return H.pack_plane(self.set_block, lines, sc.shot.len, cast={}, seed=sc.shot.kv.get("seed") and
                            int(sc.shot.kv.get("seed"), 0) or (zlib.crc32(sc.shot.id.encode()) & 0xFFFF),
                            gore=sc.gore, kids=kids, shot_id=sc.shot.id)
