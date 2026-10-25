"""emit.py - the compiler's outputs (spec 7.1 phase 8; formats in show/README.md
"Compiler outputs"): EP###.SHW, timeline.json, audio_events.json,
captions.json, plan.txt, lint.txt.

The .SHW is the player's format (shw_format.h / shw_format.py, owned by the
player package). Per shot chunk, the compiler writes:

  SHWR_PLANE + SHWR_IMAGE  the set: BANDS, IMAGE (plates and every prop / prop
                           state as its own plane, ids 1..), lane 2's TILEMAP and
                           STROKES planes (their payloads from showtk.picture)
  SHWR_BEAT + SHWR_ARGS    engine ops (beat.h) and show ops, sorted (anchor, at)
  SHWR_STRINGS             the shot's strings (line text, flipbook names...)
  SHWR_AUDIO_K             the audio events k due in the shot (P4)
  SHWR_LOOK                48-B DoomoLooks of the cast in the shot
  SHWR_ACTOR/TRACK/CROWD   the compiler's proposed payloads (README), for lane 2's actors
  SHWR_PRIV_L2 (0x100)     lane 2's "PIC1" blob: CAM, FX, TEXT, EVTS, SLOT sections
                           (show_l2_bind); planes travel in SHWR_PLANE instead
  SHWR_SLUG                for a slug card
"""
import hashlib
import json
import re
import struct
from pathlib import Path

from . import docunits, paths, textpack_bridge
from . import shw_format as F
from . import shw_ir as IR

TOOL_FILES = ("showc.py", "emit.py", "plan.py", "parse.py", "macros.py", "timeexpr.py", "stage.py", "looks.py",
              "textpack_bridge.py", "docunits.py", "shw_format.py", "shw_ir.py")
AUDIO_KEYS = ("k", "smp", "src", "speaker", "follow", "from", "op", "bus", "gain_db", "pan", "note", "bend", "cc",
              "over_smp", "len_smp", "provisional")

# show ops the compiler puts in the beat stream (BeatEv{BOP_SHOT, 0x4000|op, b=args row}).
# Lane 1's own (shw_format.h) plus the compiler's proposal for the actor and crowd/prop
# ranges, which lane 2's actor modules own (README "Compiler outputs": ADD-ONLY there).
SOP_ACTOR = {"SOP_MOVE": 0, "SOP_WALK": 1, "SOP_SQUASH": 2, "SOP_FACE": 3, "SOP_POSE": 4, "SOP_BLINK": 5,
             "SOP_STATE": 6, "SOP_SAY": 7, "SOP_STOP": 8, "SOP_HUM": 9, "SOP_EYES": 10}
SOP_CROWD = {"SOP_PROP": 0, "SOP_FX": 1}
SOP_HOST = {"SOP_CAM3D": 0x40}
BAKED = {"SOP_SUPER", "SOP_TEXT_STEP", "SOP_TEXT_CLEAR", "SOP_SIGN", "SOP_CARD", "SOP_REVEAL", "SOP_CAM",
         "SOP_CAM_DRIFT", "SOP_BOIL", "SOP_IRIS", "SOP_AUDIO"}

# ShwActor / ShwTrack / ShwCrowd: the compiler's proposed record payloads (README)
ACTOR = struct.Struct("<BBBBBBBBhhHH")                       # 16
TRACK = struct.Struct("<BBHI")                               # 8, then (value u8, count u8) runs
CROWD = struct.Struct("<BBBBBBBBHhhHiIIBBBB")                # 36


def toolchain_hash():
    h = hashlib.sha256()
    for name in TOOL_FILES:
        p = paths.SHOWTK / name
        if p.exists():
            h.update(p.read_bytes())
    hdr = paths.SHOW / "include" / "show" / "shw_format.h"
    if hdr.exists():
        h.update(hdr.read_bytes())
    return h.digest()[:8]


_IMG_CACHE = {}


def image_bytes(a, id=0):
    """shw_format.image() for a numpy plate, byte for byte, with numpy runs (fast) and a cache"""
    import numpy as np
    a = np.ascontiguousarray(a, np.uint8)
    key = (a.shape, hashlib.sha1(a.tobytes()).digest(), id)
    if key in _IMG_CACHE:
        return _IMG_CACHE[key]
    flat = a.ravel()
    h, w = a.shape
    if flat.size:
        edges = np.flatnonzero(np.diff(flat)) + 1
        starts = np.concatenate(([0], edges))
        lens = np.diff(np.concatenate((starts, [flat.size])))
        vals = flat[starts]
        reps = (lens + 254) // 255
        rv = np.repeat(vals, reps)
        rl = np.full(rv.size, 255, np.int64)
        last = np.cumsum(reps) - 1
        rl[last] = lens - (reps - 1) * 255
        rle = np.empty(rv.size * 2, np.uint8)
        rle[0::2], rle[1::2] = rl, rv
        rle = rle.tobytes()
    else:
        rle = b""
    if len(rle) < flat.size:
        data, comp = rle, F.SHWI_RLE
    else:
        data, comp = flat.tobytes(), F.SHWI_RAW
    data = data + b"\0" * (-len(data) % 4)
    out = F.IMAGE.pack(w, h, comp, 0, id, F.IMAGE.size + len(data)) + data
    _IMG_CACHE[key] = out
    return out


def _beat_no(ep, s):
    return next((i for i, b in enumerate(ep.rundown.beats) if b.id == s.beat), 0)


def _act_no(ep, s):
    b = ep.rundown.by_id.get(s.beat)
    acts = [a["id"] for a in ep.rundown.acts]
    return acts.index(b.act) if b is not None and b.act in acts else 0


# ---- the set's planes -------------------------------------------------------------
class Stage:
    """The shot's planes (lane-1 records), plane ids, and slot positions."""

    def __init__(self, c, chunk, diags):
        self.c, self.ch, self.diags = c, chunk, diags
        self.ids = {}                    # plane id / prop id / 'prop:state' / 'stroke:<id>' -> ShwPlane.id
        self.prop_planes = {}            # prop id -> [(state, plane id)]
        self.next_id = 1

    def note(self, sev, msg):
        from .errors import Diag
        s = self.c.shot
        self.diags.append(Diag("PLAN", msg, shot=s.id, severity=sev, **s.where()))

    def build(self, bake=True):
        from . import stage
        c = self.c
        sname = (c.shot.kv.get("set") or "").partition(":")
        if not sname[0] or c.set_kind != "2d":
            return
        planes, notes = stage.resolve_set(sname[0], sname[2] or None, bake_images=bake,
                                          sets_dir=c.ep.prj.series / "sets")
        for sev, msg in notes:
            self.note(sev, msg)
        if planes is None:
            return
        for order_i, p in enumerate(planes):
            pid = self.next_id
            self.next_id += 1
            flags = F.SHWPF_HIDDEN if p["hidden"] else 0
            kw = dict(id=pid, order=order_i + 100 * p["order"], flags=flags, par=p["par"], at=p["at"])
            k = p["kind"]
            try:
                if k == "bands":
                    self.ch.planes.append(F.plane_bands(p["bands"], **kw))
                elif k in ("image", "tile"):
                    if p["img"] is None:
                        continue
                    if k == "tile":
                        kw["flags"] |= F.SHWPF_TILE_X | F.SHWPF_TILE_Y
                    a = p["img"]
                    self.ch.images.append(image_bytes(a, id=len(self.ch.images)))
                    self.ch.planes.append(F.plane_image(len(self.ch.images) - 1, scale=p["px"], **kw))
                elif k == "tilemap":
                    if p["img"] is None:
                        continue
                    from .picture import tilemap
                    payload, info = tilemap.compile_plane(p["img"], plane_id=pid)
                    if len(payload) + F.PLANE.size > 0xFFFC:
                        self.note("warning", "TILEMAP %s is %d KB, over a plane's 64 KB: drawn as an image"
                                  % (p["id"], len(payload) // 1024))
                        a = p["img"]
                        self.ch.images.append(image_bytes(a, id=len(self.ch.images)))
                        self.ch.planes.append(F.plane_image(len(self.ch.images) - 1, scale=p["px"], **kw))
                    else:
                        self.ch.planes.append(F.plane(F.SHWP_TILEMAP, payload, **kw))
                elif k == "strokes":
                    self.ch.planes.append(F.plane(F.SHWP_STROKES, self.strokes(p, pid), **kw))
            except Exception as e:
                self.note("warning", "plane %s: %s: %s" % (p["id"], type(e).__name__, e))
                continue
            self.ids[p["id"]] = pid
            if p.get("prop"):
                self.prop_planes.setdefault(p["prop"], []).append((p["state"], pid))

    def strokes(self, p, pid):
        from .picture import strokes as S
        c = self.c
        reveals = {}
        for s in c.strokes:
            if s.get("revealed"):
                reveals[s["id"]] = (s["reveal_from"], s["reveal_to"])
        objs = []
        for sid, nib, w, path, hidden in p["strokes"]:
            objs.append(S.Stroke(sid, nib, path, w=w, hidden=hidden))
            self.ids["stroke:" + sid] = pid
        try:
            payload, _ix = S.compile_plane(objs, px=p["px"], plane_id=pid, boil=p["boil"], at=p["at"],
                                           reveals=reveals)
        except Exception as e:
            self.note("warning", "strokes %s: %s: %s" % (p["id"], type(e).__name__, e))
            payload, _ix = S.compile_plane([], px=p["px"], plane_id=pid)
        return payload

    def vis_events(self):
        """(f, op, [plane id]) for `show`/`hide`/`prop.set`/`change x.state=`"""
        out = []
        for f, kind, tgt, state, on in self.c.vis:
            if kind == "prop" and tgt in self.prop_planes:
                for st, pid in self.prop_planes[tgt]:
                    if state is None:
                        want = on and st is None
                    else:
                        want = on and st == state
                    if state is None and not on:
                        want = False
                    out.append((f, F.SOP_PLANE_SHOW if want else F.SOP_PLANE_HIDE, [pid]))
            elif kind == "plane" and tgt in self.ids:
                out.append((f, F.SOP_PLANE_SHOW if on else F.SOP_PLANE_HIDE, [self.ids[tgt]]))
            elif kind == "stroke":
                pass                                     # strokes show/hide: lane 2 (reveal spans)
        return out


# ---- WP6 records: the compiler's actors/crowds/tracks -> lane 2's structs ----------------
# show_actor.h (ShowActorRec 32 B, ShowCrowdRec 48 B, the TRAK blob) via showtk.actors;
# the timed ops are one SHOW_SOP_ACTOR row [actor, SHOP_*, v0..v3] each.
WP6_SHOP = {"expr": 0, "blink": 1, "face": 2, "move": 3, "walk": 4, "pop": 5, "squash": 6,
            "headturn": 7, "pose": 8, "state": 9, "twitch": 10, "show": 11}


def _wp6_actor_op(name, w):
    w = list(w) + [0] * 6
    S = WP6_SHOP
    if name == "SOP_MOVE":
        return [w[0], S["move"], w[1], w[2], max(1, w[3]), 1]
    if name == "SOP_WALK":
        return [w[0], S["walk"], w[1], 0, max(1, w[2]), 0]
    if name == "SOP_SQUASH":
        return [w[0], S["squash"], w[1], 0, max(1, w[2]), 0]
    if name == "SOP_FACE":
        return [w[0], S["face"], 1 if w[1] else 0, 0, 0, 0]
    if name == "SOP_BLINK":
        return [w[0], S["blink"], w[1], 0, 0, 0]
    if name == "SOP_STATE":
        return [w[0], S["state"], w[1], 0, 0, 0]
    return None         # SAY/STOP/HUM (the mouth TRAK carries them), POSE (no flipbook records yet), EYES


def _look_ids(spec):
    """'a_1..a_6' -> [a_1, ..., a_6]; 'a,b' -> [a, b]"""
    import re as _re
    out = []
    for part in (spec or "").split(","):
        m = _re.fullmatch(r"(.*?)(\d+)\.\.(?:\1)?(\d+)", part)
        if m:
            out += ["%s%d" % (m.group(1), i) for i in range(int(m.group(2)), int(m.group(3)) + 1)]
        elif part:
            out.append(part)
    return out


def _wp6_records(ch, c, diags):
    from . import actors as A
    from .errors import Diag
    sc = A.ShotCast()

    def warn(msg):
        diags.append(Diag("WP6", msg, shot=c.shot.id, severity="warning", **c.shot.where()))

    amap = {}
    for i, a in enumerate(c.actors):
        try:
            lb = c.looks[a["look"]] if a["look"] != 0xFF and a["look"] < len(c.looks) else None
            look = (a["who"], bytes(lb)) if lb else None
            st = a.get("state_name", "normal")
            amap[i] = sc.actor(a["id"], a["who"], kind=a.get("kind_name", "figure"), at=(a["x"], a["y"]),
                               face=a.get("face_name", "R") if a.get("face_name") in A.FACE else "R",
                               expr=a.get("expr_name", "EXPR_NEUTRAL"), idle=a.get("idle_name", "on"),
                               px=max(1, min(4, int(a.get("px", 1)))),
                               state=st if st in A.STATE else "normal", look=look)
        except Exception as e:                      # noqa: BLE001 - one bad actor never sinks the shot
            warn("actor %s: %s" % (a.get("id"), e))
    for ai, f0, track in getattr(c, "mouth_raw", []):
        if ai in amap:
            try:
                sc.tracks.append(A.pack_track(amap[ai], "mouth", f0, A.mouth_values(track)))
            except Exception as e:                  # noqa: BLE001
                warn("mouth track: %s" % e)
    for r in c.crowds:
        kv = dict(r.get("kv") or {})
        try:
            looks = kv.get("looks", "")
            if not looks.startswith("kset:"):
                ids = _look_ids(looks)
                if not ids:
                    raise A.CastError("crowd %s: no looks (gen=randomize needs looks=<a_1..a_N> or kset:)"
                                      % r["id"])
                looks = [(lid, sc.bible.resolve_look(lid, ()).pack()) for lid in ids]
            n = int(kv.get("n", r["n"]))
            spd = float(kv.get("speed", "0").replace("doc:", "") or 0)
            d = kv.get("dir", "front")
            if d in ("L", "R"):
                spd = -abs(spd) if d == "L" else abs(spd)
            rip = kv.get("ripple", "0").split("..")[-1]
            x0 = int(r.get("from", 0))
            sc.crowd(r["id"], looks, n, form=kv.get("form", "lane"), seed=int(kv.get("seed", "0"), 0),
                     at=(x0, int(r.get("y", 0))),
                     area=tuple(int(v) for v in kv["area"].split(",")) if "area" in kv else (0, 0, 0, 0),
                     spacing=float(kv.get("spacing", "16").replace("doc:", "")), speed=spd,
                     dir=d if d in A.CDIR else "front", ripple=int(rip), cycle=kv.get("cycle", "walk4"),
                     front=kv.get("layer") == "front", px=max(1, min(2, int(kv.get("px", c.shot.px or 1)))))
        except Exception as e:                      # noqa: BLE001
            warn("crowd %s: %s" % (r.get("id"), e))
    sc.to_chunk(ch)
    return len(sc.looks), len(sc.crowds)


def _slots(c):
    """slot 0 = the frame centre, 1.. the props (their anchor, show px), 64.. the actors"""
    slots = [(160, 90)]
    for p in c.props:
        slots.append((p["x"], p["y"]))
    while len(slots) < 64:
        slots.append((160, 90))
    for a in c.actors:
        slots.append((a["x"], a["y"] - 24))
    return slots


def pic1(c, shot_len):
    """lane 2's blob: camera, effects, text, its events, the slot table"""
    from .picture import blob, fmt, fx
    b = blob.Shot(shot_len)
    if c.pcam is not None:
        setup, keys = c.pcam.pack()
    else:
        setup, keys = fmt.camsetup(), []
    b.camera(setup, keys)
    wipes, dissolve = [], (0, 0)
    for f, kw, kv, words in c.wipes:
        if kw == "dissolve":
            n = int(str(kv.get("over", "13")).lstrip("+"))
            dissolve = (f, n)
            continue
        hold = kv.get("hold", "end" if kw == "wipe" else "60")
        wipes.append(fx.wipe(f, kind="sweep" if kw == "wipe" else "stack",
                             side=kv.get("side", "left" if kv.get("dir") == "L" else "right"),
                             hold="end" if hold == "end" else int(hold), out=kv.get("out") in ("on", "yes"),
                             line=int(kv.get("line", "160")), px=c.shot.px))
    boil = int(c.look_params.get("boil", "0") or 0) if getattr(c, "look_params", None) else 0
    fade0 = 16 if c.flags & IR.C["SHW_SHOT_FADE_IN"] else 0
    b.fx(fx.setup(look=c.look_name, boil=boil, matte="black" if c.flags & IR.C["SHW_SHOT_MATTE_BLACK"] else "open",
                  fade0=fade0, seed=getattr(c, "seed", 0), nwipes=len(wipes), dissolve=dissolve), wipes)
    if c.text_items and not c.flags & IR.C["SHW_SHOT_TEXT_OFF"]:
        from .picture import text
        b.text(text.pack(c.text_items))
    for t, op, args in c.pic_events:
        b.event(t, op, *args)
    for x, y in _slots(c):
        b.slot(x, y)
    return b.pack()


def _strings_of(ep, c, ch, off):
    s = ep.strtab.get(off) if off not in (None, IR.NONE) else None
    return ch.str(s) if s is not None else 0xFFFF


def chunk_for(ep, c, diags, bake=True):
    """one shot -> (shw_format.Chunk, beat events)"""
    s = c.shot
    S = textpack_bridge.symbols()
    ch = F.Chunk(bg=F.PAL_BLACK)
    if getattr(c, "slug", False) or getattr(c, "is_slug", False):
        b = ep.rundown.by_id[s.beat]
        act = next((a for a in ep.rundown.acts if a["id"] == b.act), {"title": ""})
        # M0-T3 card: the segment's BANDS sky + boiling grain under the text (as slug.py's)
        import re
        import zlib
        from . import slug as slugmod
        seg = re.sub(r"^A(?=\d)", "", str(b.act or "")).upper()
        actno, ramp = slugmod._SEG.get({"TAG": "T"}.get(seg, seg), (0, 0))
        ch.planes.append(F.plane_bands([(0, F.pal_ramp(ramp, 1)), (60, F.pal_ramp(ramp, 2)),
                                        (120, F.pal_ramp(ramp, 3))], order=0))
        ch.planes.append(F.plane_grain(F.pal_ramp(ramp, 4), 8, zlib.crc32(b.id.encode()), boil=2,
                                       order=1))
        title = re.sub(r"\s*\(.*?\)", "", b.title or "").replace("\u201c", '"').replace("\u201d", '"')
        title = "%s  %s" % (b.id, title)
        if len(title) > 44:
            title = title[:41] + "..."
        l1 = re.sub(r"\s*\(.*?\)", "", act.get("title") or "")[:40]
        ch.add_slug(title, l1, docunits.tc(b.start), beat=_beat_no(ep, s) + 1, act=actno)
        ch.add_beat([(s.len, 0, F.BOP_END, 0, 0)])
        return ch, []
    ch.strings = bytearray(c.strings)
    ch.set_kind = {"2d": F.SHWP_IMAGE, "view3d": F.SHWP_VIEW3D, "overworld": F.SHWP_OVERWORLD,
                   "room": F.SHWP_ROOM}.get(c.set_kind, F.SHWP_IMAGE)
    stg = Stage(c, ch, diags)
    stg.build(bake)
    args = []
    evs = []                               # (at, line, seq, (at, anchor, op, a, b))
    seq = 0
    for f, op, a, b, w in c.ev_direct:
        evs.append((f, w.get("line", 0), seq, (f, 0, op, a, b)))
        seq += 1

    def show_op(f, op, words, line=0):
        nonlocal seq
        row = len(args)
        args.extend(int(x) for x in words)
        evs.append((f, line, seq, (f, 0, F.BOP_SHOT, F.SHOW_OP_FLAG | op, row)))
        seq += 1

    for f, name, words, w, _o in c.show_ops:
        if name in BAKED:
            continue
        if name in SOP_ACTOR:
            row = _wp6_actor_op(name, words)          # lane 2's ShowActorOp row, or None
            if row is not None:
                show_op(f, F.SOP_ACTOR_BASE, row, w.get("line", 0))
        elif name in SOP_CROWD:
            pass        # SOP_PROP / SOP_FX: no player module reads them (0x0C0 is the crowd op)
        elif name in SOP_HOST:
            show_op(f, F.SOP_HOST_BASE + SOP_HOST[name], words, w.get("line", 0))
    for f, op, words in stg.vis_events():
        show_op(f, op, words)
    for f, kind, tgt, _st, on in c.vis:                 # show/hide <actor>
        if kind == "actor" and tgt in getattr(c, "actor_ix", {}):
            show_op(f, F.SOP_ACTOR_BASE, [c.actor_ix[tgt], WP6_SHOP["show"], 1 if on else 0, 0, 0, 0])
    evs.sort(key=lambda e: (e[0], e[1], e[2]))
    events = [e[3] for e in evs] + [(s.len, 0, F.BOP_END, 0, 0)]
    ch.add_beat(events, nlabels=sum(1 for e in events if e[2] == S["BOP_LABEL"]))
    if args:
        ch.add_args(args)
    ak = sorted({(e["_f"] - s.start, e["k"]) for e in c.audio})
    if ak:
        ch.add_audio_k(ak)
    nlooks, ncrowds = _wp6_records(ch, c, diags)
    ch.add(F.SHWR_PRIV_L2, 1, pic1(c, s.len))
    ch.vram_plan = (0, min(nlooks, 255), ncrowds, len(ch.images))
    return ch, events


def _build_chunk(ep, c, ch, diags):
    """Chunk.build(), dropping the biggest images to fit 160 KB (spec 7.3) with a warning"""
    from .errors import Diag
    while True:
        try:
            return ch.build()
        except ValueError as e:
            if not ch.images:
                raise
            big = max(range(len(ch.images)), key=lambda i: len(ch.images[i]))
            ch.images[big] = F.image(1, 1, b"\0", id=big)
            diags.append(Diag("PLAN-CHUNK", "%s: plate %d dropped to fit the chunk (needs a texture chunk, "
                                            "DEFERRED.md)" % (e, big), shot=c.shot.id, severity="warning",
                              **c.shot.where()))


def build(ep, res, compilers, bake=True):
    """fill res.shw (bytes), res.timeline, res.audio, res.captions"""
    m = __import__("re").search(r"\d+", ep.prj.ep)
    epi = F.Episode(episode=int(m.group(0)) if m else 0, seed=ep.rundown.seed & 0xFFFFFFFF,
                    toolchain=toolchain_hash())
    shot_ix = {c.shot.id: i for i, c in enumerate(compilers)}
    for c in compilers:
        s = c.shot
        try:
            ch, events = chunk_for(ep, c, res.diags, bake=bake)
            ch.shot = len(epi.shots)
            chunk = _build_chunk(ep, c, ch, res.diags)
        except Exception as e:      # an encoding limit: report, keep going
            from .errors import Diag
            res.diags.append(Diag("EMIT", "%s: %s" % (type(e).__name__, e), shot=s.id, **s.where()))
            ch = F.Chunk(shot=len(epi.shots))
            ch.add_beat([(s.len, 0, F.BOP_END, 0, 0)])
            chunk, events = ch.build(), []
        c.chunk_bytes = len(chunk)
        c.ev_n = len(events)
        flags = 0
        if getattr(c, "slug", False) or getattr(c, "is_slug", False):
            flags |= F.SHWF_SLUG
        if getattr(c, "set_kind", "2d") in ("view3d", "overworld", "room"):
            flags |= F.SHWF_3D
        base, replay, preroll = (0xFFFF, 0), (0xFFFF, 0), 0
        for key in ("base", "replay"):
            v = s.kv.get(key)
            if v and "@" in v:
                sid, fr = v.split("@", 1)
                tgt = shot_ix.get(sid)
                n = int(fr.lstrip("+") or 0)
                if tgt is None:
                    if sid not in ep.shot_by_id:
                        from .errors import Diag
                        res.diags.append(Diag("COMP-ONECHANGE", "%s=%s: no shot %s" % (key, v, sid), shot=s.id,
                                              **s.where()))
                    continue
                if key == "base":
                    base = (tgt, n)
                else:
                    replay = (tgt, n)
        look = {"pixel": 0, "crayon": 1, "sepia": 2, "lcd": 3, "hellify": 4}.get(getattr(c, "look_name", "pixel"), 0)
        seed = (ep.rundown.seed * 0x9E3779B1 + int(s.kv.get("seed", s.index)) * 2654435761) & 0xFFFFFFFF
        c.seed = seed
        sid = s.id
        if getattr(c, "slug", False):         # 8-byte .SHW id: the beat code ("co_1", "1_12")
            sid = re.sub(r"^e\d+_", "", s.beat or s.id)[:8]
        epi.add_shot(sid, s.len, chunk, px=s.px, seed=seed, flags=flags, preroll=preroll,
                     beat=_beat_no(ep, s), look=look, base=base, replay=replay)
    res.shw = epi.build()
    res.compilers = compilers
    res.timeline = timeline(ep, res, compilers)
    res.audio = {"format": "show-audio/1", "episode": ep.prj.ep, "rate": docunits.RATE, "spf": docunits.SPF,
                 "total_smp": ep.total * docunits.SPF,
                 "events": [{k: e[k] for k in AUDIO_KEYS if k in e} for e in ep.audio_events]}
    caps = []
    for c in compilers:
        caps += getattr(c, "captions", [])
    caps.sort(key=lambda x: (x["in"], x["out"]))
    res.captions = {"format": "show-captions/1", "episode": ep.prj.ep, "cues": caps}


def timeline(ep, res, compilers):
    from . import showc
    rd = ep.rundown
    by_id = {c.shot.id: c for c in compilers}
    shots = []
    scenes = {}
    tags = []
    lines = []
    strings = []
    for s in ep.order:
        c = by_id.get(s.id)
        rec = {"id": s.id, "index": s.index, "beat": s.beat, "scene": s.scene, "start": s.start, "len": s.len,
               "end": s.start + s.len, "tc_in": docunits.tc(s.start), "slug": s.slug,
               "compiled": c is not None, "file": paths.rel(s.file), "line": s.line}
        if c is not None and not s.slug:
            rec.update({
                "set": c.set_name, "set_kind": c.set_kind, "px": s.px, "look": c.look_name, "cut": c.cut,
                "matte": s.kv.get("matte", "open"), "gore": c.gore, "text": s.kv.get("text", "on"),
                "insert": "insert" in (s.stmt.words[1:] if s.stmt else []),
                "base": s.kv.get("base"), "replay": s.kv.get("replay"), "seed": getattr(c, "seed", 0),
                "cast": [{"id": a["id"], "who": a["who"], "slot": i, "kind": a["kind_name"], "age": a["age"],
                          "tags": a["tags"], "voice": a["voice"]} for i, a in enumerate(c.actors)],
                "crowds": [{"id": x["id"], "n": x["n"], "age": x["age_name"], "kind": x["kind_name"],
                            "species": x["species_name"], "tags": x["tags"]} for x in c.crowds],
                "lines": [l["id"] for l in c.lines_said],
                "strings": [x["id"] for x in getattr(c, "strings_shown", [])],
                "audio_k": [e["k"] for e in c.audio],
                "waivers": c.waivers, "asserts": getattr(c, "asserts_done", []), "notes": c.notes,
                "ad": c.ad, "provisional": c.provisional, "chunk_bytes": getattr(c, "chunk_bytes", 0),
                "badge": getattr(c, "badge", "PS1-LEGAL"),
                "events": sorted(c.events_tl, key=lambda e: (e["f"], e["line"] if isinstance(e["line"], int) else -1))})
            if getattr(c, "cam_drift", None):
                d = c.cam_drift
                rec["camera"] = {"drift_px_per_f": [float(d["vx"]), float(d["vy"])], "zero_shot_f": d["zero"],
                                 "zero_f": s.start + d["zero"]}
            tags += c.tags
            for l in c.lines_said:
                ln = ep.lib.lines.get(l["id"], {})
                spk = ln.get("speaker")
                ch = ep.cast.chars.get(spk, {}) if spk else {}
                lines.append({"id": l["id"], "speaker": spk, "text": ln.get("text"), "dir": ln.get("dir"),
                              "shot": s.id, "start": l["start"], "end": l["end"], "provisional": l["provisional"],
                              "tags": ln.get("tags", []), "to": ln.get("to"), "ref": ln.get("ref"),
                              "consent": ln.get("consent"), "voice": ch.get("voice"), "age": ch.get("age"),
                              "file": paths.rel(ln["where"]["file"]) if ln.get("where") else None,
                              "line": ln.get("where", {}).get("line")})
                if ln.get("tags"):
                    tags.append({"on": "line", "id": l["id"], "shot": s.id, "tags": ln["tags"], "to": ln.get("to"),
                                 "ref": ln.get("ref"), "consent": ln.get("consent"), "start": l["start"],
                                 "end": l["end"], "file": paths.rel(ln["where"]["file"]),
                                 "line": ln["where"]["line"]})
            for x in getattr(c, "strings_shown", []):
                strings.append({k: x[k] for k in ("id", "text", "shot", "start", "end", "style")})
                if x.get("tags"):
                    tags.append({"on": "string", "id": x["id"], "shot": s.id, "tags": x["tags"], "to": None,
                                 "ref": None, "consent": None, "start": x["start"], "end": x["end"],
                                 "file": None, "line": None})
            # scene membership and who is in it
            sc = scenes.setdefault(s.scene, {"id": s.scene, "shots": [], "start": s.start, "end": s.start + s.len,
                                             "children": False, "child_sources": [], "gore": "CUTE"})
            sc["shots"].append(s.id)
            sc["end"] = s.start + s.len
            if c.gore == "BRUTAL":
                sc["gore"] = "BRUTAL"
            for a in c.actors:
                if a["age"] in ("child", "teen") or "KID" in a["tags"]:
                    sc["children"] = True
                    sc["child_sources"].append("actor %s.%s (%s, age=%s)" % (s.id, a["id"], a["who"], a["age"]))
            for x in c.crowds:
                if x["age_name"] in ("child", "teen") or "KID" in x["tags"]:
                    sc["children"] = True
                    sc["child_sources"].append("crowd %s.%s (age=%s)" % (s.id, x["id"], x["age_name"]))
            for e in c.audio:
                src = e.get("src", "")
                if src.startswith("walla:"):
                    bed = src[6:]
                    for lid, ln in ep.lib.lines.items():
                        spk = ln["speaker"]
                        if spk.startswith("walla:"):
                            v = ep.lib.voices.get(spk[6:], {})
                            if v.get("age") in ("child", "teen") or "kid" in spk:
                                if "walla %s" % bed not in " ".join(sc["child_sources"]):
                                    sc["children"] = True
                                    sc["child_sources"].append("walla %s (voice %s, age=%s)" % (
                                        bed, spk[6:], v.get("age") or "unset"))
                                break
                if src.startswith("line:"):
                    ln = ep.lib.lines.get(src[5:], {})
                    ch = ep.cast.chars.get(ln.get("speaker"), {})
                    if ch.get("age") in ("child", "teen"):
                        sc["children"] = True
                        sc["child_sources"].append("line %s (%s, age=%s)" % (src[5:], ln.get("speaker"), ch["age"]))
        shots.append(rec)
    for t in tags:
        sh = next((x for x in ep.order if x.id == t["shot"]), None)
        t["scene"] = sh.scene if sh else None
    acts = []
    for a in rd.acts:
        bs = [rd.by_id[b] for b in a["beats"]]
        acts.append({"id": a["id"], "title": a["title"], "start": bs[0].start if bs else 0,
                     "len": sum(b.len for b in bs), "beats": a["beats"]})
    beats = [{"id": b.id, "act": b.act, "title": b.title, "start": b.start, "len": b.len,
              "tc_in": docunits.tc(b.start), "elastic": int(b.elastic) if b.elastic not in (None, "") else None,
              "protected": b.protected, "shots": b.shots, "slug_frames": getattr(b, "slug_frames", 0),
              "file": paths.rel(b.stmt.file), "line": b.stmt.line} for b in rd.beats]
    diags = [{"rule": d.rule, "severity": d.severity, "text": d.format()} for d in res.diags]
    return {"format": "show-timeline/1", "episode": ep.prj.ep, "id": rd.id, "title": rd.title,
            "fps": docunits.FPS, "rate": docunits.RATE, "spf": docunits.SPF, "total_frames": ep.total,
            "slug": res.slug, "inputs_hash": showc.inputs_hash(ep.prj.files),
            "toolchain_hash": toolchain_hash().hex(), "acts": acts, "beats": beats,
            "scenes": list(scenes.values()), "shots": shots, "lines": lines, "strings": strings, "tags": tags,
            "diagnostics": diags}


def write(res, out_dir, name=None):
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    ep = res.timeline["episode"]
    name = name or ("SLUG.SHW" if res.slug else "%s.SHW" % ep.upper())
    written = []

    def put(fn, data, binary=False):
        p = out_dir / fn
        tmp = p.with_suffix(p.suffix + ".tmp")
        if binary:
            tmp.write_bytes(data)
        else:
            tmp.write_text(data)
        tmp.replace(p)
        written.append(p)

    put(name, res.shw, True)
    put("timeline.json", json.dumps(res.timeline, indent=1) + "\n")
    put("audio_events.json", json.dumps(res.audio, indent=1) + "\n")
    put("captions.json", json.dumps(res.captions, indent=1) + "\n")
    put("plan.txt", res.plan)
    put("lint.txt", "".join(d.format() + "\n" for d in res.diags))
    return written
