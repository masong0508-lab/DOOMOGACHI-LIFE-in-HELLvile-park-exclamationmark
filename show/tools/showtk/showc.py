"""showc.py - the show compiler (spec 7): .shw text -> EP###.SHW + sidecars.

    from showtk import showc
    res = showc.compile_episode("ep101")                       # everything
    res = showc.compile_episode("ep101", shots="co01..co03")   # a range
    res = showc.compile_episode("ep101", slug=True)            # the rundown alone
    res.diags, res.timeline, res.audio, res.shots, res.write(out_dir)

Phases (spec 7.1): parse + macros (parse.py, macros.py) -> rundown layout
(beats, shot lengths, slug fill) -> time solve per shot (timeexpr.py) ->
stage resolve (stage.py, looks.py) -> ops: engine-native lines through the
game's compile_beat (textpack_bridge.py), show ops on BOP_SHOT, audio events
-> event indices k -> emit (emit.py) and plan (plan.py).

Every diagnostic is a showtk.errors.Diag in the spec 13.4 format. A compile
with errors still returns what it could build (res.ok is False).
"""
import hashlib
import json
import math
import re
from fractions import Fraction
from pathlib import Path

from . import docunits, macros as macro_mod, parse, paths, stage, textpack_bridge, timeexpr
from . import shw_ir as F
from .errors import Diag, ShowError

FPS, SPF = docunits.FPS, docunits.SPF
TAGS = {"innuendo", "implied", "offscreen", "suggestive", "advance", "contact", "touch", "yes", "no",
        "stepback", "gore_truth", "rear", "satire", "explicit", "consent_gag", "anatomy"}
NO_WAIVE = {"STD-EXPLICIT", "STD-CONSENT", "STD-AGE", "STD-KIDS", "STD-HATE"}
ENGINE_OPS = {"shake", "punch", "zoom", "flash", "pal", "tint", "hitstop", "expr", "freeze", "thaw",
              "panic", "still", "twitch", "pop", "headturn", "nobutton", "fade", "iris_in", "iris_out",
              "label", "button"}
FORBIDDEN = {"waita": "SHW-WAITA", "flag": "SHW-OPS", "music": "SHW-OPS", "mute": "SHW-OPS",
             "duck": "SHW-OPS", "insert": "SHW-OPS", "cut": "SHW-OPS", "act": "SHW-OPS",
             "caption": "SHW-OPS", "hold": "SHW-OPS", "npc": "SHW-OPS"}
AUDIO_OPS = {"sfx", "fol", "amb", "mx", "walla", "tapestop", "cc", "hum", "stop"}
LOOKS = {"pixel": 0, "crayon": 1, "sepia": 2, "lcd": 3, "hellify": 4}
CUTS = {"CUT": 0, "SMASH": 1, "MATCH": 2, "FADE": 3, "IRIS": 4, "WIPE": 5}
KINDS = {"bust": 0, "figure": 1, "chibi": 2, "fighter": 3, "model3d": 4, "sprite": 5}
IDLES = {"on": 0, "still": 1, "frozen": 2, "none": 3}
STATES = {"normal": 0, "possessed": 1, "ghost": 2, "dead": 3}
AGES = {"adult": 0, "teen": 1, "child": 2}
TXT_STYLES = {"type": 0, "pop": 1, "deadpan": 2}
EASES = {"LINEAR": 0, "IN": 1, "OUT": 2, "INOUT": 3}
VISEME_DEFAULT = {"X": None, "M": "MOUTH_FLAT", "C": "MOUTH_TEETH", "E": "MOUTH_GRIN", "A": "MOUTH_OPEN",
                  "O": "MOUTH_O", "U": "MOUTH_POUT", "F": "MOUTH_TEETH"}
P4_MAX = 14            # ShowMbox.p4_k[14]: audio events one frame can publish
TITLE_SAFE = (16, 9)
SHOT_RE = re.compile(r"^[A-Za-z]\w*$")


def _w(st):
    return {"file": st.file, "line": st.line}


# =============================================================================
# the project's files
# =============================================================================
class Project:
    def __init__(self, ep="ep101", root=None, series=None):
        self.root = Path(root) if root else paths.SHOW
        self.series = Path(series) if series else self.root / "series"
        self.ep = ep
        self.ep_dir = self.root / ep
        self.files = []             # every input read (for the inputs hash)
        self.diags = []
        self.included = set()

    def read(self, path, required=False):
        p = Path(path)
        if not p.exists():
            if required:
                self.diags.append(Diag("FILE", "missing: %s" % paths.rel(p)))
            return []
        self.files.append(p)
        try:
            return parse.parse_file(p)
        except ShowError as e:
            self.diags += e.diags
            return []

    def read_tree(self, path, required=False, _seen=None):
        """parse a file and every `include <path>` in it (relative to the including file),
        spliced in place; acts flattened. -> [Stmt]"""
        p = Path(path).resolve() if Path(path).exists() else Path(path)
        _seen = _seen if _seen is not None else set()
        if p in _seen:
            self.diags.append(Diag("FILE", "%s is included twice (or includes itself)" % paths.rel(p)))
            return []
        _seen.add(p)
        out = []
        for st in parse.flatten(self.read(p, required)):
            if st.kw == "include":
                if not st.words:
                    self.diags.append(st.diag("FILE", "`include <path>`"))
                    continue
                q = (p.parent / st.words[0])
                if not q.exists():
                    self.diags.append(st.diag("FILE", "include %s: no such file (%s)" % (st.words[0], paths.rel(q))))
                    continue
                self.included.add(q.resolve())
                out += self.read_tree(q, True, _seen)
            else:
                out.append(st)
        return out

    def read_glob(self, d, pat="*.shw"):
        d = Path(d)
        return [(p, self.read(p)) for p in sorted(d.glob(pat))] if d.is_dir() else []


# =============================================================================
# libraries: sfx, music, voices, lines, strings, cues, show settings
# =============================================================================
class Library:
    def __init__(self, prj, episode_stmts=()):
        self.prj = prj
        self.settings = {"seed": 0xD00D}
        for st in prj.read(prj.series / "show.shw"):
            if st.kw == "show":
                if st.get("seed"):
                    self.settings["seed"] = int(st.get("seed"), 0)
        self.sfx = {}
        for st in prj.read(prj.series / "sfx.shw"):
            if st.kw in ("sfx", "fol", "amb") and st.words:
                self.sfx[st.words[0]] = {"kind": st.kw, "gain_db": docunits.gain_db(st.get("gain", "0"), _w(st)),
                                         "pan": st.get("pan"), "cc": st.get("cc"), "src": st.get("src"),
                                         "bus": st.get("bus", "FX"), "where": _w(st)}
        self.cues = {}
        for st in prj.read(prj.series / "music.shw"):
            if st.kw == "cue" and st.words:
                self.cues[st.words[0]] = {"bpm": Fraction(st.get("bpm", "120")), "meter": st.get("meter", "4/4"),
                                          "where": _w(st), "stmt": st}
        self.voices = {}
        for st in prj.read(prj.series / "voices.shw"):
            if st.kw == "voice" and st.words:
                self.voices[st.words[0]] = {"age": st.get("age"), "speed": st.get("speed", "1"),
                                            "engine": st.get("engine"), "where": _w(st)}
        self.visemes = dict(VISEME_DEFAULT)
        for st in prj.read(prj.series / "visemes.shw"):
            # `viseme <X> MOUTH_*` (the AUDIO track's table; per-family tables are its business)
            if st.kw == "viseme" and len(st.words) >= 2 and st.words[0] in self.visemes:
                self.visemes[st.words[0]] = st.words[1] if st.words[1] not in ("rest", "none") else None
        self.macros = {}
        acting = prj.read(prj.series / "acting.shw")
        try:
            self.macros = macro_mod.load(acting)
        except ShowError as e:
            prj.diags += e.diags
        set_files = [s for _p, s in prj.read_glob(prj.series / "sets")]
        self.sets, d = stage.load_sets(set_files)
        prj.diags += d
        # episode files
        self.lines, self.wallas, self.strings = {}, {}, {}
        for st in prj.read(prj.ep_dir / "lines.shw"):
            if st.kw == "line" and len(st.words) >= 2:
                if not st.strs:
                    prj.diags.append(st.diag("LINE", "line %s has no quoted text" % st.words[0]))
                    continue
                lid, spk = st.words[0], st.words[1]
                if lid in self.lines:
                    prj.diags.append(st.diag("LINE", "line %s is already defined at line %d" % (
                        lid, self.lines[lid]["where"]["line"])))
                self.lines[lid] = {"id": lid, "speaker": spk, "text": st.strs[0], "dir": st.get("dir"),
                                   "tags": [t for t in (st.get("tags") or "").split(",") if t],
                                   "to": st.get("to"), "ref": st.get("ref"), "consent": st.get("consent"),
                                   "take": st.get("take"), "where": _w(st)}
            elif st.kw == "walla" and st.words:
                self.wallas[st.words[0]] = {"lines": st.get("lines"), "where": _w(st), "pan": st.get("pan")}
        seen_str = set()
        string_stmts = list(prj.read(prj.ep_dir / "strings.shw"))
        for _p, ss in prj.read_glob(prj.ep_dir / "strings"):
            string_stmts += ss
        string_stmts += [x for x in episode_stmts if x.kw == "string"]
        for st in string_stmts:
            if st.kw != "string" or not st.words or not st.strs:
                continue
            key = (str(Path(st.file).resolve()), st.line)
            if key in seen_str:
                continue
            seen_str.add(key)
            sid = st.words[0]
            if sid in self.strings and self.strings[sid]["text"] != st.strs[0]:
                prj.diags.append(st.diag("STRING", "string %s is already defined at %s:%d with other text" % (
                    sid, paths.rel(self.strings[sid]["where"]["file"]), self.strings[sid]["where"]["line"])))
                continue
            self.strings[sid] = {"id": sid, "text": st.strs[0], "font": st.get("font"),
                                 "tags": [t for t in (st.get("tags") or "").split(",") if t], "where": _w(st)}
        self.spots, self.cue_ops = {}, []
        for st in prj.read(prj.ep_dir / "cues.shw"):
            if st.kw == "spot" and st.words:
                self.spots[st.words[0]] = {"from": st.get("from"), "bus": st.get("bus", "MX"),
                                           "gain_db": docunits.gain_db(st.get("gain", "0"), _w(st))}
            elif st.time is not None and st.kw == "mx":
                self.cue_ops.append(st)
        self.voice_lock = self._voice_lock()

    def _voice_lock(self):
        """show/epNNN/voice.lock (WP9, generated): JSON or TOML, one table per line id with
        len_smp, onset_smp, mouth (one viseme letter per frame), words [[start_smp, end_smp, w]]"""
        p = self.prj.ep_dir / "voice.lock"
        if not p.exists():
            return {}
        self.prj.files.append(p)
        txt = p.read_text()
        try:
            d = json.loads(txt)
        except ValueError:
            try:
                import tomllib
                d = tomllib.loads(txt)
            except Exception as e:
                self.prj.diags.append(Diag("VOICE-LOCK", "cannot read %s: %s" % (paths.rel(p), e), file=p,
                                           severity="warning"))
                return {}
        return d.get("lines", d) if isinstance(d, dict) else {}

    # --- line timing (voice.lock, else the provisional per-voice estimate) ----------
    def line_timing(self, lid, cast):
        """-> {'len': frames, 'words': [frame offsets], 'mouth': str|None, 'provisional': bool}"""
        ln = self.lines.get(lid)
        lock = self.voice_lock.get(lid) if isinstance(self.voice_lock, dict) else None
        if lock and "len_smp" in lock:
            n = int(math.ceil(int(lock["len_smp"]) / SPF))
            words = [int(w[0]) // SPF for w in lock.get("words", [])]
            return {"len": n, "words": words, "mouth": lock.get("mouth"), "provisional": False}
        text = ln["text"] if ln else ""
        speed = 1.0
        spk = (ln or {}).get("speaker", "")
        voice = None
        if spk.startswith("walla:"):
            voice = spk[6:]
        elif spk in cast.chars:
            voice = cast.chars[spk].get("voice")
        if voice in self.voices:
            sp = str(self.voices[voice]["speed"])
            try:
                parts = [float(x) for x in sp.split("..")]
                speed = sum(parts) / len(parts)
            except ValueError:
                speed = 1.0
        return estimate_line(text, speed)


def estimate_line(text, speed=1.0):
    """a provisional length before a take exists: 18 f + 2.4 f per letter, a comma
    4 f, an ellipsis or {..} 13 f (the R13 pause), divided by the voice's speed"""
    vis = re.sub(r"\{[^}]*\}", "", text)
    words, f, pos = [], 0.0, 0.0
    tokens = re.findall(r"\.\.\.|[A-Za-z']+|[,;:]|\S", vis)
    pause = text.count("{..}") * 13
    for t in tokens:
        if t == "...":
            pos += 13
        elif t in ",;:":
            pos += 4
        elif re.match(r"[A-Za-z']+", t):
            words.append(pos)
            pos += 2.4 * len(t.replace("'", "")) + 2
    f = (18 + pos + pause) / max(speed, 0.25)
    return {"len": int(math.ceil(f)), "words": [int(w / max(speed, 0.25)) + 3 for w in words], "mouth": None,
            "provisional": True}


def flap(text, n):
    """a provisional viseme track of n frames from the letters (one letter per frame)"""
    vis = re.sub(r"\{[^}]*\}", " ", text).lower()
    seq = []
    for ch in vis:
        if ch in "a":
            seq.append("A")
        elif ch in "ei":
            seq.append("E")
        elif ch in "o":
            seq.append("O")
        elif ch in "uw":
            seq.append("U")
        elif ch in "mbp":
            seq.append("M")
        elif ch in "fv":
            seq.append("F")
        elif ch.isalpha():
            seq.append("C")
        elif ch in " ,.;:!?-":
            seq.append("X")
    # collapse repeats, then stretch to n with every mouth held >= 2 f
    col = []
    for v in seq:
        if not col or col[-1] != v:
            col.append(v)
    while col and col[-1] == "X":
        col.pop()
    while col and col[0] == "X":
        col.pop(0)
    if not col or n <= 0:
        return "X" * max(n, 0)
    per = max(2, n // len(col))
    out = "".join(v * per for v in col)[:n]
    return out + "X" * (n - len(out))


# =============================================================================
# the rundown
# =============================================================================
class Beat:
    def __init__(self, st, act):
        self.id = st.words[0]
        self.stmt = st
        self.act = act
        self.len = docunits.frames(st.get("len", "0"), _w(st))
        self.title = st.get("title") or (st.strs[0] if st.strs else "")
        self.tc = st.get("tc")
        self.elastic = st.get("elastic")
        self.protected = st.get("protected") in ("yes", "1", "on", "true")
        self.start = 0
        self.shots = []


class Rundown:
    def __init__(self, prj, with_episode=True):
        self.prj = prj
        self.id, self.title, self.seed = prj.ep, "", 0
        self.acts, self.beats = [], []
        src = prj.ep_dir / "rundown.shw"
        alt = prj.ep_dir / "episode.shw"
        stmts = parse.flatten(prj.read(src)) if src.exists() else []
        # episode.shw (STAGING): the include list in running order; it may also carry beats
        self.episode_stmts = prj.read_tree(alt) if alt.exists() and (with_episode or not src.exists()) else []
        ep_beats = [s for s in self.episode_stmts if s.kw == "beat"]
        if not any(s.kw == "beat" for s in stmts):
            stmts = [s for s in self.episode_stmts if s.kw in ("@episode", "act", "beat")]
            ep_beats = []
        elif not any(s.kw == "@episode" for s in stmts):
            stmts = [s for s in self.episode_stmts if s.kw == "@episode"] + stmts
        if not stmts:
            prj.diags.append(Diag("FMT-RUNTIME", "no rundown: write %s (act/beat lines)" % paths.rel(src)))
        act = None
        ids = set()
        for st in stmts:
            if st.kw == "@episode":
                self.id = st.words[0] if st.words else prj.ep
                self.title = st.strs[0] if st.strs else ""
                self.seed = int(st.get("seed", "0"), 0)
            elif st.kw == "act":
                act = {"id": st.words[0] if st.words else "?", "title": st.strs[0] if st.strs else "",
                       "beats": [], "where": _w(st)}
                self.acts.append(act)
            elif st.kw == "beat":
                if not st.words:
                    prj.diags.append(st.diag("FMT-RUNTIME", "`beat <id> len=<frames>`"))
                    continue
                if act is None:
                    act = {"id": "-", "title": "", "beats": [], "where": _w(st)}
                    self.acts.append(act)
                try:
                    b = Beat(st, act["id"])
                except ShowError as e:
                    prj.diags += e.diags
                    continue
                if b.id in ids:
                    prj.diags.append(st.diag("FMT-RUNTIME", "beat %s is listed twice" % b.id))
                    continue
                ids.add(b.id)
                act["beats"].append(b.id)
                self.beats.append(b)
        f = 0
        for b in self.beats:
            b.start = f
            if b.tc:
                want = b.tc if b.tc.count(":") == 3 else "00:" + b.tc + (":00" if b.tc.count(":") == 1 else "")
                try:
                    wf = docunits.parse_tc(want, _w(b.stmt))
                except ShowError as e:
                    prj.diags += e.diags
                    wf = f
                if wf != f:
                    prj.diags.append(b.stmt.diag("FMT-RUNTIME", "beat %s: tc=%s but the lengths before it put it "
                                                 "at %s (frame %d)" % (b.id, b.tc, docunits.tc(f), f)))
            f += b.len
        self.total = f
        self.by_id = {b.id: b for b in self.beats}
        for st in ep_beats:       # beats repeated in the staging files must agree with the rundown
            bid = st.words[0] if st.words else None
            b = self.by_id.get(bid)
            if b is None:
                prj.diags.append(st.diag("FMT-RUNTIME", "beat %s is not in the rundown (%s)" % (bid, paths.rel(src))))
                continue
            try:
                n = docunits.frames(st.get("len", str(b.len)), _w(st))
            except ShowError as e:
                prj.diags += e.diags
                continue
            if n != b.len:
                prj.diags.append(st.diag("FMT-RUNTIME", "beat %s len=%d here but %d in the rundown (%s:%d)" % (
                    bid, n, b.len, paths.rel(b.stmt.file), b.stmt.line)))
            if st.get("shots"):
                b.claimed = (st.get("shots"), _w(st))


# =============================================================================
# shots
# =============================================================================
class Shot:
    """one shot header + body; or a slug card (stmt None)"""

    def __init__(self, st=None, beat=None, slug_len=0):
        self.stmt = st
        self.slug = st is None
        self.diags = []
        if self.slug:
            self.id = "slug:%s" % beat.id
            self.beat = beat.id
            self.len = slug_len
            self.kv = {}
            self.file, self.line = beat.stmt.file, beat.stmt.line
        else:
            self.id = st.words[0] if st.words else "?"
            self.kv = st.kv
            self.beat = st.get("beat")
            self.file, self.line = st.file, st.line
            self.len = None
        self.start = 0
        self.index = 0
        self.compiled = False
        self.scene = None
        self.px = int(self.kv.get("px", "1") or 1)

    def where(self):
        return {"file": self.file, "line": self.line}


def _shot_range(order, spec, diags):
    """'co01..co03' | 'co02' | 'co01,co05' -> set of shot ids"""
    if not spec:
        return None
    ids = [s.id for s in order]
    want = set()
    for part in spec.split(","):
        part = part.strip()
        if ".." in part:
            a, b = part.split("..", 1)
            if a not in ids or b not in ids:
                diags.append(Diag("SHOTS", "--shots %s: unknown shot %s" % (part, a if a not in ids else b)))
                continue
            i, j = ids.index(a), ids.index(b)
            want.update(ids[min(i, j):max(i, j) + 1])
        elif part in ids:
            want.add(part)
        elif part in {s.beat for s in order}:
            want.update(s.id for s in order if s.beat == part)
        else:
            diags.append(Diag("SHOTS", "--shots: no shot or beat called %s" % part))
    return want


# =============================================================================
# per-shot compile
# =============================================================================
class Item:
    __slots__ = ("stmt", "time_ast", "where", "text", "frame", "chain_head", "idx")

    def __init__(self, stmt, time_ast, chain_head):
        self.stmt = stmt
        self.time_ast = time_ast
        self.where = _w(stmt)
        self.text = "%s %s" % (stmt.time, stmt.kw) if stmt.time else stmt.kw
        self.chain_head = chain_head     # index of the first item on the same source line
        self.frame = None


class ShotCompiler:
    def __init__(self, ep, shot):
        self.ep = ep
        self.lib = ep.lib
        self.cast = ep.cast
        self.shot = shot
        self.st = shot.stmt
        self.diags = shot.diags
        self.engine_lines = []          # (where, '+f op args')
        self.show_ops = []              # (f, sop name, [words], where, order)
        self.audio = []                 # dicts (absolute smp) + '_order', '_where', '_f'
        self.events_tl = []             # timeline events
        self.captions = []
        self.args = []
        self.actors, self.actor_ix = [], {}
        self.props, self.prop_ix = [], {}
        self.planes, self.plane_ix = [], {}
        self.strokes, self.stroke_ix = [], {}
        self.crowds, self.crowd_ix = [], {}
        self.looks = []
        self.camkeys = []
        self.strings = bytearray()
        self.str_at = {}
        self.tracks = {}
        self.supers = 0
        self.tags, self.waivers, self.asserts, self.notes = [], [], [], []
        self.lines_said = []
        self.flags = 0
        self.provisional = False
        self.look_name = shot.kv.get("look", "pixel")
        self.texpages, self.cluts = [], []
        self.ad = []
        self.vis = []                   # (f, prop|plane|stroke, id, state, on)  -> SOP_PLANE_SHOW/HIDE
        self.wipes = []                 # (f, wipe|split|dissolve, kv, words)    -> PIC1 FX
        self.pcam = None                # picture.camera.Camera (lane 2's exact camera)
        self.text_items = []            # picture.text.Item list               -> PIC1 TEXT
        self.pic_events = []            # (t, op, args)                         -> PIC1 EVTS
        self.mouth_raw = []             # (actor index, shot frame, viseme letters) -> WP6 TRAK
        self.host = None                # hostbridge.HostShot on an overworld/room/view3d set

    # -- helpers -------------------------------------------------------------
    def d(self, rule, msg, st=None, severity="error", frame=None):
        w = _w(st) if st is not None else self.shot.where()
        self.diags.append(Diag(rule, msg, shot=self.shot.id, frame=frame, severity=severity, **w))

    def s(self, text):
        if text is None:
            return F.NONE
        if text not in self.str_at:
            self.str_at[text] = len(self.strings)
            self.strings += text.encode("utf-8") + b"\0"
        return self.str_at[text]

    def snap(self, v, st, what):
        px = self.shot.px
        iv = int(math.floor(Fraction(v) + Fraction(1, 2)))
        if px > 1 and iv % px:
            sn = int(round(iv / px)) * px
            self.d("PIC-SNAP", "%s %d is off the px=%d grid; snapped to %d" % (what, iv, px, sn), st, "warning")
            return sn
        return iv

    def xy(self, txt, st, what="at"):
        if txt is None:
            return 0, 0
        an = self.anchor_xy(txt, st)
        if an is not None:
            return an
        parts = str(txt).split(",")
        if len(parts) != 2:
            st.fail("STAGE", "%s=%s: x,y" % (what, txt))
        return (self.snap(docunits.num(parts[0], _w(st)), st, what + " x"),
                self.snap(docunits.num(parts[1], _w(st)), st, what + " y"))

    def anchor_xy(self, txt, st, f=0):
        """room:/mon:/follow:/prop: anchors and VIEW3D world cells -> show px (None: not an anchor)"""
        t = str(txt)
        kind = t.partition(":")[0]
        if kind == "prop" and t[5:] in self.prop_ix:
            p = self.props[self.prop_ix[t[5:]]]
            return p["x"], p["y"]
        if kind == "prop" and self.host is not None:
            return self.host.anchor("room:" + t[5:].replace("shellL", "window_sill_l").replace(
                "shellR", "window_sill_r"), f)
        if self.host is not None:
            if kind in ("room", "mon", "follow"):
                xy = self.host.anchor(t, f or getattr(self.host, "cur_f", 0))
                return xy if xy is not None else (160, 120)
            if self.set_kind == "view3d" and re.fullmatch(r"-?\d+(\.\d+)?,-?\d+(\.\d+)?", t):
                x, y = [float(v) for v in t.split(",")]
                if x < 128 and y < 128 and ("." in t):
                    return self.host.project(x, y, f, 0.4)
        elif kind == "follow":
            who, _, part = t[7:].partition(".")
            ai = self.actor_ix.get(who)
            if ai is None and who in self.prop_ix:
                p = self.props[self.prop_ix[who]]
                return p["x"], p["y"]
            if ai is not None:
                from .hostbridge import PARTS
                dx, dy = PARTS.get(part or "feet", (0, -16))
                return self.actors[ai]["x"] + dx, self.actors[ai]["y"] + dy
        return None

    def actor(self, name, st):
        if name in self.actor_ix:
            return self.actor_ix[name]
        hits = [i for i, a in enumerate(self.actors) if a["who"] == name]
        if len(hits) == 1:
            return hits[0]
        st.fail("ACTOR", "no actor %s in shot %s (actors: %s)" % (
            name, self.shot.id, ", ".join(a["id"] for a in self.actors) or "none"))

    def sop(self, f, name, words, st):
        self.show_ops.append((f, name, [int(w) for w in words], _w(st), len(self.show_ops)))

    def tl(self, f, st, kind, op, end=None, k=None, extra=None):
        e = {"f": self.shot.start + f, "shot_f": f, "end": (self.shot.start + end) if end is not None else None,
             "op": op, "kind": kind, "text": st.text(), "k": k, "file": paths.rel(st.file), "line": st.line}
        if getattr(st, "from_macro", None):
            e["macro"] = st.from_macro
        if extra:
            e.update(extra)
        self.events_tl.append(e)
        return e

    # -- the compile -----------------------------------------------------------
    def run(self):
        sh = self.shot
        hdr = sh.kv
        if sh.px not in (1, 2, 3):
            self.d("PIC-SNAP", "px=%s: 1, 2 or 3" % hdr.get("px"))
            sh.px = 1
        if self.look_name not in LOOKS:
            self.d("STAGE", "look=%s (one of %s)" % (self.look_name, ", ".join(LOOKS)))
            self.look_name = "pixel"
        cut = (hdr.get("cut") or "CUT").split(":")[0]
        if cut not in CUTS:
            self.d("STAGE", "cut=%s (one of CUT SMASH MATCH:<shot> FADE IRIS WIPE:brick)" % hdr.get("cut"))
            cut = "CUT"
        self.cut = cut
        self.flags |= CUTS[cut] << 8
        if hdr.get("matte") == "black":
            self.flags |= F.C["SHW_SHOT_MATTE_BLACK"]
        if hdr.get("text") == "off":
            self.flags |= F.C["SHW_SHOT_TEXT_OFF"]
        self.gore = hdr.get("gore", "CUTE")
        if self.gore not in ("CUTE", "BRUTAL"):
            self.d("STAGE", "gore=%s: CUTE or BRUTAL" % self.gore)
            self.gore = "CUTE"
        if self.gore == "BRUTAL":
            self.flags |= F.C["SHW_SHOT_GORE_BRUTAL"]
        if "insert" in self.st.words[1:]:
            self.flags |= F.C["SHW_SHOT_INSERT"]
        self.set_stage()
        setup, timed = [], []
        for c in self.st.children:
            (timed if c.time is not None else setup).append(c)
        own = {c.pos[0] for c in setup if c.kw in ("actor", "crowd") and c.pos}
        setup = [c for c in self.inherited(own) if not (c.pos and c.pos[0] in own)] + setup
        for c in setup:
            try:
                self.setup(c)
            except ShowError as e:
                self.diags += e.diags
            except (ValueError, KeyError, IndexError, TypeError, ZeroDivisionError) as e:
                self.d("SHW", "cannot read `%s`: %s: %s" % (c.text(), type(e).__name__, e), c)
        self.solve_and_run(timed)
        sh.compiled = True

    def inherited(self, own):
        """base=<shot>@f / replay=<shot>@f: the source shot's cast (actor/crowd setup lines, in its
        slot order, recursively), because the player re-derives the state by running the source
        silently (spec 5.3, 8.1): the same slots must exist here. A cycle is an error."""
        out, seen, sh = [], {self.shot.id}, self.shot
        while True:
            src = sh.kv.get("base") or sh.kv.get("replay")
            if not src:
                return out
            sid = src.split("@", 1)[0]
            if sid in seen:
                self.d("STAGE", "base=/replay= sources form a cycle: %s" % " -> ".join(list(seen) + [sid]))
                return out
            seen.add(sid)
            sh = self.ep.shot_by_id.get(sid)
            if sh is None or sh.stmt is None:
                self.d("STAGE", "base=/replay=%s: no shot %s" % (src, sid))
                return out
            got = [c for c in sh.stmt.children if c.time is None and c.kw in ("actor", "crowd") and c.pos
                   and c.pos[0] not in own and all(c.pos[0] != o.pos[0] for o in out)]
            out = got + out

    # -- stage ---------------------------------------------------------------------
    def set_stage(self):
        sh = self.shot
        sname = (sh.kv.get("set") or "").split(":")[0]
        self.set_name = sname or None
        self.set_kind = "2d"
        self.setdef = None
        if not sname:
            self.d("STAGE", "shot %s has no set= (a set from series/sets/*.shw)" % sh.id)
            return
        sd = self.lib.sets.get(sname)
        if sd is None:
            self.d("STAGE", "unknown set %s (series/sets/ has: %s)" % (
                sname, ", ".join(sorted(self.lib.sets)[:16]) or "no sets yet"))
            return
        self.setdef = sd
        self.set_kind = sd.kind
        if "px" not in sh.kv:
            sh.px = sd.px
        elif sd.px != sh.px and sd.kind == "2d":
            self.d("STAGE", "shot px=%d but set %s is authored at px=%d" % (sh.px, sname, sd.px),
                   severity="warning")
        if sd.kind != "2d" and sh.px != 1:
            self.d("PIC-SNAP", "3D set kinds are px=1 only (spec 2.3)")
        if sd.kind in ("overworld", "room", "view3d"):
            from . import hostbridge
            try:
                self.host = hostbridge.HostShot(self)       # the shot's one host plane (emit.Stage)
            except Exception as e:                          # noqa: BLE001
                self.d("SHW-HOST", "set %s: %s: %s" % (sname, type(e).__name__, e))
        for p in sd.planes:
            try:
                self.plane(p, sd)
            except ShowError as e:
                self.diags += e.diags
            except (ValueError, KeyError, IndexError, TypeError) as e:
                self.d("SET", "cannot read `%s`: %s: %s" % (p.text(), type(e).__name__, e), p)
        for p in sd.props:
            try:
                self.prop(p)
            except ShowError as e:
                self.diags += e.diags
            except (ValueError, KeyError, IndexError, TypeError) as e:
                self.d("SET", "cannot read `%s`: %s: %s" % (p.text(), type(e).__name__, e), p)

    def plane(self, p, sd):
        pid = p.pos[0] if p.pos else "plane%d" % len(self.planes)
        w = _w(p)
        if p.kw == "strokes":
            kind = "strokes"
        elif p.get("tile"):
            kind = "grain" if "grain" in p.get("tile") else "tilemap"
        elif p.get("bands"):
            kind = "bands"
        elif p.get("kind"):
            kind = p.get("kind")
        else:
            kind = "image"
        if kind not in stage.PLANE_KINDS:
            p.fail("STAGE", "plane %s: kind %s" % (pid, kind))
        pw, ph = stage.size_of(p, sd)
        src = p.get("image") or p.get("tile") or p.get("bands")
        order = p.get("order", "back")
        if order not in stage.ORDERS:
            p.fail("STAGE", "plane %s: order=%s (back|front|top)" % (pid, order))
        rec = {"kind": stage.PLANE_KINDS[kind], "order": stage.ORDERS[order], "flags": 0,
               "boil": int(p.get("boil", "0")), "par_q16": stage.par_q16(p.get("par", "0"), w),
               "w": pw, "h": ph, "tex": F.NONE, "clut": F.NONE, "x": 0, "y": 0, "arg": 0}
        if p.get("tile"):
            rec["flags"] |= 1
        m = re.search(r"density=(\d+)", src or "")
        if kind == "grain" and m:
            rec["arg"] = int(m.group(1))
        if p.get("at"):
            rec["x"], rec["y"] = self.xy(p.get("at"), p)
        if src and src.startswith("gen:") or (src and kind in ("image", "tilemap", "grain")):
            baked = stage.bake(src, pw, ph, self.shot.px, 0)
            if baked:
                rec["tex"] = len(self.texpages)
                self.texpages.append({"w": baked["w"], "h": baked["h"], "bpp": 8, "comp": baked.get("comp", 0),
                                      "name": self.ep.strtab.add(src), "data": baked["data"]})
                if baked.get("clut"):
                    rec["clut"] = len(self.cluts)
                    self.cluts.append(list(baked["clut"]))
        if kind == "strokes":
            first = len(self.strokes)
            for c in p.children:
                if c.kw != "stroke":
                    continue
                sid = c.pos[0] if c.pos else "stroke%d" % len(self.strokes)
                pts, dg = stage.path_points(c.get("path", ""), _w(c))
                if dg:
                    dg.shot = self.shot.id
                    self.diags.append(dg)
                nib, dg = stage.nib_index(c.get("nib", "0"), _w(c))
                if dg:
                    dg.shot = self.shot.id
                    self.diags.append(dg)
                self.stroke_ix[sid] = len(self.strokes)
                self.strokes.append({"nib": nib, "width": int(c.get("w", "2")),
                                     "flags": 2 if c.get("outside") in ("on", "yes", "1") else 0,
                                     "boil": int(p.get("boil", "0")), "reveal_from": 0, "reveal_to": 0,
                                     "name": self.ep.strtab.add(sid), "pts": pts, "id": sid, "revealed": False})
            rec["arg"] = first | ((len(self.strokes) - first) << 16)
        self.plane_ix[pid] = len(self.planes)
        rec["id"] = pid
        self.planes.append(rec)

    def prop(self, p):
        pid = p.pos[0] if p.pos else p.fail("STAGE", "`prop <id> art= at=x,y`")
        if pid in self.prop_ix:
            p.fail("STAGE", "prop %s twice in shot %s" % (pid, self.shot.id))
        plane = p.get("plane")
        if plane and plane not in self.plane_ix:
            p.fail("STAGE", "prop %s: no plane %s in set %s" % (pid, plane, self.set_name))
        x, y = self.xy(p.get("at", "0,0"), p)
        face = p.get("face", "front")
        if face not in stage.FACES:
            p.fail("STAGE", "prop %s: face=%s (front|L|R|wall)" % (pid, face))
        self.prop_ix[pid] = len(self.props)
        self.props.append({"plane": self.plane_ix.get(plane, 0), "face": stage.FACES[face],
                           "anchor": 1 if p.get("anchor") == "center" else 0, "flags": 0, "x": x, "y": y,
                           "art": self.ep.strtab.add(p.get("art")), "tex": F.NONE, "id": pid,
                           "par": self.planes[self.plane_ix[plane]]["par_q16"] if plane else 0,
                           "plane_id": plane, "src": p.get("art"),
                           "states": {k[4:]: v for k, v in p.kv.items() if k.startswith("art.")}})

    # -- setup statements -----------------------------------------------------------
    def setup(self, c):
        kw = c.kw
        if kw == "look":
            reg = c.words[0] if c.words else None
            if reg and reg not in LOOKS:
                c.fail("STAGE", "look %s (one of %s)" % (reg, ", ".join(LOOKS)))
            if reg and "look" in self.shot.kv and self.shot.kv["look"] != reg:
                self.d("STAGE", "look %s differs from the header's look=%s" % (reg, self.shot.kv["look"]), c,
                       "warning")
            if reg:
                self.look_name = reg
            self.look_params = dict(c.kv)
        elif kw == "cam":
            self.cam_setup(c)
        elif kw == "actor":
            self.add_actor(c)
        elif kw == "prop":
            self.prop(c)
        elif kw == "crowd":
            self.add_crowd(c)
        elif kw == "change":
            self.change(c)
        elif kw == "assert":
            self.asserts.append(c)
        elif kw == "waive":
            if not c.words:
                c.fail("WAIVE", '`waive <RULE> "<reason>"`')
            rule = c.words[0]
            if rule in NO_WAIVE:
                self.d("STD-WAIVE", "%s is a hard limit and can never be waived (spec 13.5)" % rule, c)
            elif not c.strs:
                self.d("WAIVE", "waive %s needs a quoted reason" % rule, c)
            else:
                self.waivers.append({"rule": rule, "reason": c.strs[0], "file": paths.rel(c.file),
                                     "line": c.line})
        elif kw == "note":
            self.notes.append(c.strs[0] if c.strs else c.text())
        elif kw == "scene":
            if c.words:
                self.shot.scene = c.words[0]
        elif kw == "tag":
            self.tag(c, None)
        elif kw == "ad":
            self.ad.append({"f": self.shot.start, "text": c.strs[0] if c.strs else " ".join(c.words)})
        elif kw in ("label",):
            c.fail("TIME", "`label` needs a time (+N label NAME)")
        elif kw in ("show", "hide", "prop.set", "stroke"):
            self.vis_op(0, c)                    # a setup visibility = its state at frame 0
        else:
            c.fail("SHW-OPS", "`%s` is not a setup statement (spec 5.5: look cam actor prop crowd change assert "
                              "waive note scene tag ad); timed ops need a time: +N %s ..." % (kw, kw))

    def pic_cam(self, c, fn):
        """mirror a camera statement into lane 2's exact camera (picture.camera)"""
        from .picture import camera as pcam
        if self.pcam is None:
            self.pcam = pcam.Camera()
        try:
            fn(self.pcam)
        except (pcam.CamError, ValueError, ZeroDivisionError) as e:
            self.d("CAM", "the stage camera cannot take `%s`: %s" % (c.text(), e), c, "warning")

    def cam_setup(self, c):
        w = _w(c)
        if c.words and c.words[0] == "drift":
            z = 0
            if c.get("zero"):
                za = timeexpr.parse(c.get("zero"), w)
                z = za[1] if za[0] == "rel" else 0
            self.pic_cam(c, lambda k: k.drift(c.get("dx"), c.get("dy"), z))
        elif c.get("at"):
            at = c.get("at").split(",")
            self.pic_cam(c, lambda k: k.setup_at(at[0], at[1] if len(at) > 1 else None, c.get("scale"),
                                                 c.get("about").split(",") if c.get("about") else None))
        if c.words and c.words[0] == "drift":
            dx = docunits.num(c.get("dx", "0"), w)
            dy = docunits.num(c.get("dy", "0"), w)
            zero = 0
            if c.get("zero"):
                a = timeexpr.parse(c.get("zero"), w)
                if a[0] != "rel":
                    c.fail("TIME", "cam drift zero= takes +N")
                zero = a[1]
            vx, vy = docunits.q16(dx), docunits.q16(dy)
            base = self.camkeys[0] if self.camkeys else None
            x0 = base["x_q16"] if base else 0
            y0 = base["y_q16"] if base else 0
            key = {"frame": 0, "mode": 1, "ease": 0, "x_q16": x0 - vx * zero, "y_q16": y0 - vy * zero,
                   "vx_q16": vx, "vy_q16": vy, "scale_q8": base["scale_q8"] if base else 256, "over": 0}
            if base and base["mode"] == 0 and base["frame"] == 0:
                self.camkeys[0] = key
            else:
                self.camkeys.append(key)
            self.cam_drift = {"vx": dx, "vy": dy, "zero": zero}
            return
        at = c.get("at")
        if at is None:
            c.fail("CAM", "setup `cam at=x,y [scale=s]` or `cam drift dx= [dy=] [zero=+N]`")
        x, y = self.xy(at, c, "cam at")
        sc = int(round(float(docunits.num(c.get("scale", "1"), w)) * 256))
        self.camkeys.insert(0, {"frame": 0, "mode": 0, "ease": 0, "x_q16": x << 16, "y_q16": y << 16,
                                "vx_q16": 0, "vy_q16": 0, "scale_q8": sc, "over": 0})

    def cam_x(self, f):
        """the camera x (Fraction px) at shot frame f from the keys (drift and set only)"""
        x = Fraction(0)
        for k in self.camkeys:
            if k["frame"] <= f:
                x = Fraction(k["x_q16"] + k["vx_q16"] * (f - k["frame"]), 65536)
        return x

    def add_actor(self, c):
        if not c.pos:
            c.fail("ACTOR", "`actor <id> who=<char> kind=bust|figure|chibi|fighter|model3d at=x,y`")
        aid = c.pos[0]
        who = c.get("who", aid)
        if aid in self.actor_ix:
            c.fail("ACTOR", "actor %s twice in shot %s" % (aid, self.shot.id))
        if len(self.actors) >= 8:
            c.fail("ACTOR", "more than 8 actors in a shot (BEAT_ACTORS clocks); make the rest a crowd")
        ch = self.cast.chars.get(who)
        if ch is None:
            c.fail("ACTOR", "unknown character %s (series/cast.shw has: %s)" % (
                who, ", ".join(sorted(self.cast.chars)[:20]) or "no cast yet"))
        kind = c.get("kind", "figure")
        if kind not in KINDS:
            c.fail("ACTOR", "kind=%s (one of %s)" % (kind, ", ".join(KINDS)))
        if ch.get("kinds") and kind not in ch["kinds"] and kind != "sprite":
            self.d("ACTOR", "%s has no %s image (cast.shw kinds=%s)" % (who, kind, ",".join(ch["kinds"])), c)
        x, y = self.xy(c.get("at", "160,90"), c)
        face = c.get("face", "R")
        idle = c.get("idle", ch.get("idle") or "on")
        if idle not in IDLES:
            c.fail("ACTOR", "idle=%s (on|still|frozen|none)" % idle)
        state = c.get("state", ch.get("state") or "normal")
        if state not in STATES:
            c.fail("ACTOR", "state=%s (%s)" % (state, "|".join(STATES)))
        if state != "normal" and ch.get("age") == "child":
            self.d("STD-KIDS", "a child (%s) is never shown %s" % (who, state), c)
        expr = textpack_bridge.sym(c.get("expr", "EXPR_NEUTRAL"), _w(c), "EXPR_")
        look_b = self.cast.looks.get(who)
        li = 0xFF
        if look_b:
            li = len(self.looks)
            self.looks.append(look_b)
        px = int(c.get("px", str(self.shot.px)))
        self.actor_ix[aid] = len(self.actors)
        self.actors.append({"kind": KINDS[kind], "look": li, "face": 1 if face == "L" else 0, "expr": expr,
                            "idle": IDLES[idle], "phase": int(ch.get("phase") or 0) & 0xFF, "px": px,
                            "state": STATES[state], "x": x, "y": y, "name": self.ep.strtab.add(who),
                            "roster": ch["roster"] if ch.get("roster") is not None else F.NONE,
                            "id": aid, "who": who, "kind_name": kind, "age": ch.get("age"),
                            "tags": ch.get("tags", []), "voice": ch.get("voice"), "face_name": face,
                            "state_name": state, "idle_name": idle, "expr_name": c.get("expr", "EXPR_NEUTRAL")})

    def add_crowd(self, c):
        if not c.pos:
            c.fail("CROWD", "`crowd <id> gen=randomize seed= n= ... | looks=kset:<id> ... end`")
        cid = c.pos[0]
        w = _w(c)
        n = int(c.get("n", "1"))
        age = c.get("age")
        cast_looks = None
        if c.get("gen") == "cast":
            cast_looks, age, n = self.cast_crowd(c, cid, n, age)
        if age not in AGES:
            self.d("STD-AGE", "crowd %s must declare age=adult|teen|child" % cid, c)
            age = "adult"
        tags = [t for t in (c.get("tags") or "").split(",") if t]
        if age == "child" and "KID" not in tags:
            tags.append("KID")
        kind = c.get("kind", "chibi")
        if kind not in KINDS:
            c.fail("CROWD", "kind=%s" % kind)
        rec = {"look_base": len(self.looks), "n": n, "lane": 0xFF, "dir": 1 if c.get("dir") == "L" else 0,
               "kind": KINDS[kind], "ripple_lo": 0, "ripple_hi": 0, "age": AGES[age], "spacing_q8": 0,
               "from": 0, "y": 0, "cycle": self.ep.strtab.add(c.get("cycle")), "speed_q16": 0,
               "seed": int(c.get("seed", "0"), 0), "clock_mask": (1 << min(n, 32)) - 1 if n < 32 else 0xFFFFFFFF,
               "gen": 0, "species": 0xFF, "look_k": 0, "flags": 1 if age == "child" else 0,
               "id": cid, "age_name": age, "kind_name": kind, "species_name": c.get("species"), "tags": tags,
               "kv": dict(c.kv)}
        looks = c.get("looks")
        if cast_looks is not None:
            rec["gen"] = 2
            rec["cast_looks"] = cast_looks
            rec["look_k"] = len(cast_looks)
            rec["kv"]["n"] = str(n)
        elif c.get("gen") == "randomize":
            rec["gen"] = 1
        elif looks and looks.startswith("kset:"):
            kid = looks[5:]
            lb = self.cast.kset(kid)
            if lb is None:
                c.fail("CROWD", "unknown kset %s (series/looks/crowds.shw)" % kid)
            rec["look_k"] = len(lb)
            self.looks += lb
        else:
            c.fail("CROWD", "crowd %s: gen=randomize seed=N or looks=kset:<id>" % cid)
        if c.get("species"):
            rec["species"] = textpack_bridge.sym(c.get("species"), w, "SPECIES_")
        lane = c.get("lane")
        y = 0
        if lane:
            if not self.setdef or lane not in self.setdef.paths:
                c.fail("CROWD", "crowd %s: lane=%s is not a path of set %s" % (cid, lane, self.set_name))
                return
            rec["lane"] = list(self.setdef.paths).index(lane)
            y = int(docunits.num(self.setdef.paths[lane].get("y", "0"), w))
        if c.get("dy"):
            y += int(docunits.num(c.get("dy").lstrip("+"), w)) * (-1 if c.get("dy").startswith("-") else 1)
        rec["y"] = self.snap(y, c, "crowd y")
        rec["from"] = self.snap(docunits.num(c.get("from", "0"), w), c, "crowd from")
        rec["spacing_q8"] = self.snap(docunits.num(c.get("spacing", "16"), w), c, "crowd spacing") << 8
        rec["speed_q16"] = docunits.q16(docunits.num(c.get("speed", "0"), w))
        rp = c.get("ripple", "0")
        lo, _, hi = rp.partition("..")
        rec["ripple_lo"], rec["ripple_hi"] = int(lo), int(hi or lo)
        if rec["ripple_hi"] > 3:
            self.d("TIME-RIPPLE", "crowd %s ripple %s: ripples are 0-3 f" % (cid, rp), c, "warning")
        self.crowd_ix[cid] = len(self.crowds)
        self.crowds.append(rec)

    def kid_safe(self):
        """STD-KIDS: may a child be in this shot? Not with gore=BRUTAL, on a first-person (VIEW3D)
        set, or in a scene tagged adult/suggestive/innuendo/romance"""
        if self.gore == "BRUTAL" or self.set_kind == "view3d":
            return False
        tags = " ".join(str(self.shot.kv.get(k, "")) for k in ("tags", "rating", "scene", "tone")).lower()
        tags += " " + " ".join(str(c.text()).lower() for c in self.st.children if c.kw in ("tag", "note"))
        return not any(t in tags for t in ("adult", "suggestive", "innuendo", "romance", "sexy", "flirt",
                                           "mingle", "brutal", "gore"))

    def cast_crowd(self, c, cid, n, age):
        """`crowd <id> gen=cast who=a,b,random:N`: the named cast as crowd members (their own
        looks); children are dropped from any shot that is not kid-safe (STD-KIDS)"""
        who = [x for x in (c.get("who") or "").split(",") if x]
        safe = self.kid_safe()
        looks, dropped, kid = [], [], False
        for wid in who:
            if wid.startswith("random:"):
                continue
            ch = self.cast.chars.get(wid)
            if ch is None:
                self.d("CROWD", "crowd %s: gen=cast who=%s: no such character" % (cid, wid), c)
                continue
            if ch.get("age") == "child":
                if not safe:
                    dropped.append(wid)
                    continue
                kid = True
            lb = self.cast.looks.get(wid)
            if lb:
                looks.append((wid, bytes(lb)))
        if dropped:
            self.d("STD-KIDS", "crowd %s: %s left out of the crowd (a child is never in this shot's context)"
                   % (cid, ", ".join(dropped)), c, "info")
        if not looks:
            c.fail("CROWD", "crowd %s: gen=cast found no cast looks (who=%s)" % (cid, c.get("who")))
        if age not in AGES:
            age = "child" if kid else "adult"
        return looks, age, max(len(looks), min(n, len(looks)))

    def change(self, c):
        if not self.shot.kv.get("base"):
            self.d("COMP-ONECHANGE", "`change` is for a shot with base=<shot>@<frame>", c)
        m = re.fullmatch(r"(\w+)\.(\w+)=(\S+)", " ".join(c.pos) if c.pos else "")
        if not m:
            kv = c.toks
            m = None
            if c.pos and c.toks and c.toks[-1].kind == "kv":
                m = re.fullmatch(r"(\w+)\.(\w+)", c.toks[-1].key)
                if m:
                    obj, field, val = m.group(1), m.group(2), c.toks[-1].value
            if not m:
                c.fail("COMP-ONECHANGE", "`change <obj>.<field>=<value>`")
        else:
            obj, field, val = m.groups()
        self.changes = getattr(self, "changes", []) + [(obj, field, val, c)]
        if obj in self.prop_ix and field == "state":
            if val not in self.props[self.prop_ix[obj]]["states"]:
                c.fail("COMP-ONECHANGE", "%s.state=%s: the prop has no art.%s" % (obj, val, val))
            self.vis.append((0, "prop", obj, val, True))
            return
        if obj in self.prop_ix and field == "hide":
            self.vis.append((0, "prop", obj, None, val in ("0", "no", "off")))
            return
        if obj in self.prop_ix:
            pf = {"face": 0, "x": 1, "y": 2, "hide": 3}.get(field)
            if pf is None:
                c.fail("COMP-ONECHANGE", "prop field %s (face|x|y|hide|state)" % field)
            v = stage.FACES.get(val, None) if field == "face" else int(val) if val.lstrip("-").isdigit() else 1
            if v is None:
                c.fail("COMP-ONECHANGE", "face=%s" % val)
            self.sop(0, "SOP_PROP", [self.prop_ix[obj], pf, v], c)

    # -- tags ---------------------------------------------------------------------
    def tag(self, c, f, span=None):
        if not c.words:
            c.fail("STD-TAG", "`tag shot|line <id>|scene <tags>`")
        on = c.words[0]
        rest = c.words[1:]
        target = None
        if on == "line":
            if not rest:
                c.fail("STD-TAG", "`tag line <line id> <tags>`")
            target, rest = rest[0], rest[1:]
            if target not in self.lib.lines:
                self.d("STD-TAG", "tag line %s: no such line in lines.shw" % target, c)
        elif on not in ("shot", "scene"):
            c.fail("STD-TAG", "tag %s: the target is shot, line <id> or scene" % on)
        tags = []
        for r in rest:
            tags += [t for t in r.split(",") if t]
        if c.get("tags"):
            tags += [t for t in c.get("tags").split(",") if t]
        bad = [t for t in tags if t not in TAGS]
        if bad:
            self.d("STD-TAG", "unknown tag %s (the closed list: %s)" % (", ".join(bad), " ".join(sorted(TAGS))), c)
        if not tags:
            self.d("STD-TAG", "tag with no tags", c)
        start = self.shot.start + (f or 0)
        end = self.shot.start + self.shot.len
        if span:
            start, end = self.shot.start + span[0], self.shot.start + span[1]
        self.tags.append({"on": on, "id": target or (self.shot.id if on == "shot" else None),
                          "shot": self.shot.id, "tags": tags, "to": c.get("to"), "ref": c.get("ref"),
                          "consent": c.get("consent"), "start": start, "end": end,
                          "file": paths.rel(c.file), "line": c.line})

    # -- timed statements ------------------------------------------------------------
    def solve_and_run(self, timed):
        items = []
        heads = {}
        for c in timed:
            key = (c.file, c.line)
            try:
                ast = timeexpr.parse(c.time, _w(c))
            except ShowError as e:
                self.diags += e.diags
                continue
            heads.setdefault(key, len(items))
            items.append(Item(c, ast, heads[key]))
        labels = {}
        says = {}
        for i, it in enumerate(items):
            c = it.stmt
            if c.kw == "label" and c.words:
                labels[c.words[0]] = i
            elif c.kw == "punch":
                labels["L_PUNCH"] = i
            elif c.kw == "button":
                labels["L_BUTTON"] = i
            elif c.kw == "say" and c.words:
                says[c.words[0]] = i
        ep = self.ep

        def line_fn(name, field):
            if name not in says:
                return None
            t = self.lib.line_timing(name, self.cast)
            if field == "start":
                return says[name], 0, t["provisional"]
            if field == "end":
                return says[name], t["len"], t["provisional"]
            k = int(field[1:])
            if field[0] == "w":
                if k < 1 or k > len(t["words"]):
                    raise ShowError(Diag("TIME-ANCHOR", "line %s has %d words; .w%d is past them" % (
                        name, len(t["words"]), k), shot=self.shot.id, **items[says[name]].where))
                return says[name], t["words"][k - 1], t["provisional"]
            return says[name], 0, True      # phonemes: only with a voice.lock

        def shot_fn(name, field):
            s = ep.shot_by_id.get(name)
            if s is None or s.slug:
                return None
            return s.start + (s.len if field == "end" else 0)

        def bar_fn(B, b, it):
            return self.bar_frame(B, b, items, it)

        solver = timeexpr.Solver(items, labels.get, line_fn, shot_fn, bar_fn, self.shot.start, self.shot.len,
                                 self.shot.id)
        for i in range(len(items)):
            try:
                items[i].frame = solver.frame_of(i, [])
            except ShowError as e:
                self.diags += e.diags
                solver.frames[i] = -1
                solver._busy.discard(i)
        self.provisional = any(solver.prov)
        if self.provisional:
            self.flags |= F.C["SHW_SHOT_PROVISIONAL"]
        # macros expand at their take's frame
        run = []
        for it in items:
            if it.frame is None or it.frame < 0:
                continue
            if it.stmt.kw == "take":
                try:
                    for f, ns in macro_mod.expand(it.stmt, self.lib.macros, it.frame):
                        run.append((f, ns, it.chain_head, it.time_ast))
                except ShowError as e:
                    self.diags += e.diags
            else:
                run.append((it.frame, it.stmt, it.chain_head, it.time_ast))
        self.last_audio_on_line = {}
        for f, c, head, ast in run:
            if f > self.shot.len:
                self.d("TIME-ANCHOR", "%s lands at +%d, after the shot's end (+%d)" % (c.text(), f, self.shot.len),
                       c)
                continue
            try:
                self.op(f, c, head, ast)
            except (ValueError, KeyError, IndexError, TypeError, ZeroDivisionError) as e:
                self.d("SHW", "cannot read `%s`: %s: %s" % (c.text(), type(e).__name__, e), c, frame=f)
            except ShowError as e:
                for dg in e.diags:
                    if dg.shot is None:
                        dg.shot = self.shot.id
                    if dg.frame is None:
                        dg.frame = f
                self.diags += e.diags
        self.check_asserts()

    def bar_frame(self, B, b, items, it):
        cue_hdr = self.shot.kv.get("cue")
        cue, bar0, f0 = None, None, 0
        if cue_hdr:
            m = re.fullmatch(r"(\w+)@bar:(\d+)(?:\.(\d+))?", cue_hdr)
            if m:
                cue, bar0 = m.group(1), (int(m.group(2)), int(m.group(3) or 1))
        if cue is None:
            for j in items:
                c = j.stmt
                if c.kw == "mx" and len(c.words) >= 2 and c.words[1] == "start" and j.time_ast[0] in ("rel",):
                    cue = c.words[0]
                    fr = (self.lib.spots.get(cue, {}).get("from") or "bar:1.1")
                    m = re.fullmatch(r"bar:(\d+)(?:\.(\d+))?", c.get("from") or fr)
                    bar0 = (int(m.group(1)), int(m.group(2) or 1)) if m else (1, 1)
                    f0 = j.time_ast[1]
                    break
        if cue is None or cue not in self.lib.cues:
            return None
        info = self.lib.cues[cue]
        num = int(info["meter"].split("/")[0])
        beats = (B - bar0[0]) * num + (b - bar0[1])
        fpb = Fraction(60 * FPS) / info["bpm"]          # frames per beat (96 bpm: 18.75)
        return f0 + int(math.floor(beats * fpb + Fraction(1, 2)))

    # -- one op --------------------------------------------------------------------
    def op(self, f, c, head, ast):
        kw = c.kw
        w = _w(c)
        if kw in FORBIDDEN:
            c.fail(FORBIDDEN[kw], "`%s` is not allowed in a show (spec 5.5%s)" % (
                kw, ": a show never waits for a button" if kw == "waita" else
                "; sound is a show op: sfx/mx/walla" if kw in ("music", "mute", "duck") else ""))
        if self.host is not None:
            from .hostbridge import HOST_KWS
            self.host.cur_f = f
            if kw in HOST_KWS and self.host.op(f, c):
                self.tl(f, c, "show", kw)
                return
            if kw == "sfx":
                self.host.sfx(f, c)
            if kw == "eyes" and self.host.eyes(f, c):
                return
        if self.crowd_op(f, c):
            return
        if kw == "prop" and c.pos and c.pos[0] not in self.prop_ix:
            self.prop(c)                    # a timed prop: declared where the line is (props have no hide yet)
            self.tl(f, c, "show", kw)
            return
        if kw in ENGINE_OPS:
            return self.engine(f, c)
        if kw in AUDIO_OPS:
            return self.audio_op(f, c, head, ast)
        if kw == "say":
            return self.say(f, c)
        if kw == "tag":
            span = ast[1:3] if ast[0] == "span" else None
            if c.get("over"):
                a = timeexpr.parse(c.get("over"), w)
                if a[0] == "span":
                    span = (a[1], a[2])
            return self.tag(c, f, span)
        if kw == "super":
            return self.super(f, c)
        if kw == "cam":
            return self.cam_op(f, c)
        if kw == "reveal":
            if not c.words or c.words[0] not in self.stroke_ix:
                c.fail("STROKE", "reveal: no stroke %s in set %s (strokes: %s)" % (
                    c.words[0] if c.words else "?", self.set_name, ", ".join(self.stroke_ix) or "none"))
            a, b = self.span(c.get("over"), f, c)
            si = self.stroke_ix[c.words[0]]
            self.strokes[si]["reveal_from"], self.strokes[si]["reveal_to"] = a, b
            self.strokes[si]["revealed"] = True
            self.sop(f, "SOP_REVEAL", [si, a, b], c)
            self.tl(f, c, "show", "reveal", end=b + 1, extra={"stroke": c.words[0]})   # 100 % on frame b
            return
        if kw in ("move", "walk", "squash", "face", "pose", "blink", "state", "eyes"):
            return self.actor_op(f, c)
        if kw in ("sign", "card", "fx", "boil", "cam3d"):
            return self.misc_op(f, c)
        if kw == "ad":
            self.ad.append({"f": self.shot.start + f, "text": c.strs[0] if c.strs else " ".join(c.words)})
            return
        if kw == "note":
            self.notes.append(c.strs[0] if c.strs else c.text())
            return
        if kw == "punch":
            return self.engine(f, c)
        if kw in ("show", "hide", "prop.set", "stroke"):
            return self.vis_op(f, c)
        if kw in ("wipe", "split", "dissolve"):
            return self.wipe_op(f, c)
        c.fail("SHW-OPS", "unknown op `%s` (spec 5.5)" % kw)

    # -- crowds as targets (show_crowd.c SHCO_*: one SOP_CROWD row [crowd, op, a]) ------------
    CROWD_KWS = {"still", "freeze", "thaw", "panic", "walk", "move", "pose", "face", "headturn", "ripple",
                 "flinch", "hide", "show", "say", "take", "expr", "squash", "blink", "pop", "eyes"}
    SHCO = {"freeze": 0, "thaw": 1, "pop": 2, "face": 3, "speed": 4, "show": 5}
    CROWD_POP = {"clap": "TWT_CLAP", "pileup": "TWT_CLUNK", "laugh": "TWT_HOP2", "hat": "TWT_HOP1"}

    def crowd_ref(self, tok):
        """'crowd:<id>[.N]' or a bare crowd id[.N] (not an actor) -> (crowd index, member) or None"""
        if not tok:
            return None
        t = tok[6:] if tok.startswith("crowd:") else tok
        name, _, mem = t.partition(".")
        if name in self.crowd_ix and (tok.startswith("crowd:") or name not in self.actor_ix):
            return self.crowd_ix[name], mem or None
        return None

    def crowd_row(self, f, ci, op, a, c):
        self.sop(f, "SOP_CROWDOP", [ci, self.SHCO[op], a], c)

    def crowd_twt(self, name):
        for k, v in self.CROWD_POP.items():
            if name and name.startswith(k):
                return textpack_bridge.sym(v, _w(self.st), "TWT_")
        return textpack_bridge.sym("TWT_HOP1", _w(self.st), "TWT_")

    def crowd_op(self, f, c):
        kw = c.kw
        if kw not in self.CROWD_KWS:
            return False
        w = list(c.words)
        if kw in ("ripple", "flinch") and w and (w[0] == "crowd" or w[0].startswith("crowd:") or not self.crowds) \
                and (not self.crowds or self.crowd_ref(w[0]) is None and w[0] != "crowd"):
            self.d("CROWD", "%s %s: no such crowd in this shot (nothing to ripple)" % (kw, w[0]), c, "info")
            return True
        if kw == "ripple" and w and w[0] not in self.crowd_ix and w[0] not in self.actor_ix:
            self.d("CROWD", "ripple %s: no crowd %s in this shot" % (w[0], w[0]), c, "info")
            return True
        if not self.crowds:
            return False
        if kw == "flinch" and w and w[0] == "crowd":
            for ci in range(len(self.crowds)):
                self.crowd_row(f, ci, "pop", self.crowd_twt("hat"), c)
            self.tl(f, c, "show", kw)
            return True
        ti = 1 if kw in ("say", "take") else 0
        ref = self.crowd_ref(w[ti]) if len(w) > ti else None
        if ref is None:
            return False
        ci, mem = ref
        rec = self.crowds[ci]
        if kw == "say":                       # a crowd line: the walla's voice, the crowd hops on it
            rest = [t for t in c.toks if not (t.kind == "word" and t.value == w[1])]
            from .parse import Stmt
            self.say(f, Stmt("say", rest, c.file, c.line, c.time))
            self.crowd_row(f, ci, "pop", self.crowd_twt("laugh"), c)
            return True
        if kw in ("still", "freeze"):
            self.crowd_row(f, ci, "freeze", 0, c)
            ov = c.get("over")
            if ov and ".." in ov:
                end = self.span(ov, f, c)[1]
                if end < self.shot.len:
                    self.crowd_row(end, ci, "thaw", 0, c)
        elif kw == "thaw":
            self.crowd_row(f, ci, "thaw", 0, c)
        elif kw == "panic":
            self.crowd_row(f, ci, "thaw", 0, c)
            self.crowd_row(f, ci, "speed", -768 if rec["dir"] else 768, c)
            self.crowd_row(f, ci, "pop", self.crowd_twt("hat"), c)
        elif kw in ("walk", "move"):
            ov = max(1, docunits.frames(c.get("over", "30"), _w(c)))
            to = str(c.get("to", "0"))
            if "," in to or to.startswith("+") or to.startswith("-"):
                dx = float(docunits.num(to.split(",")[0].lstrip("+"), _w(c)) or 0)
                if to.startswith("-"):
                    dx = -abs(dx)
            else:
                dx = float(docunits.num(to, _w(c))) - rec["from"]
            self.crowd_row(f, ci, "speed", int(round(dx * 256 / ov)), c)
            if f + ov < self.shot.len:
                self.crowd_row(f + ov, ci, "speed", 0, c)
        elif kw in ("pose", "take", "pop", "squash", "expr", "blink"):
            self.crowd_row(f, ci, "pop", self.crowd_twt(w[1] if len(w) > 1 else "hat"), c)
        elif kw == "ripple":
            what = w[1] if len(w) > 1 else "laugh"
            if what == "headturn":
                b = int(w[2]) if len(w) > 2 and re.fullmatch(r"-?\d+", w[2]) else 64
                self.crowd_row(f, ci, "face", 2 if 0 < b % 256 < 128 else (1 if b % 256 >= 128 else 0), c)
            else:
                self.crowd_row(f, ci, "pop", self.crowd_twt(what), c)
        elif kw == "headturn":
            b = int(w[1]) if len(w) > 1 and re.fullmatch(r"-?\d+", w[1]) else 64
            self.crowd_row(f, ci, "face", 2 if 0 < b % 256 < 128 else (1 if b % 256 >= 128 else 0), c)
        elif kw == "eyes":
            d = (w[1] if len(w) > 1 else "D").upper()
            self.crowd_row(f, ci, "face", {"L": 1, "R": 2, "U": 3}.get(d, 0), c)
        elif kw == "face":
            d = (w[1] if len(w) > 1 else "front")
            self.crowd_row(f, ci, "face", {"L": 1, "R": 2, "back": 3, "wall": 3}.get(d, 0), c)
        elif kw in ("hide", "show"):
            if mem is not None:              # one member: show_crowd.c hides whole crowds only
                self.d("CROWD", "%s %s: one crowd member cannot be hidden on its own yet; the crowd stays"
                       % (kw, w[0]), c, "info")
            else:
                self.crowd_row(f, ci, "show", 1 if kw == "show" else 0, c)
        self.tl(f, c, "show", kw, extra={"crowd": rec["id"]})
        return True

    def until_arg(self, c):
        u = c.get("until")
        if u in (None, "end"):
            return None
        a = timeexpr.parse(u, _w(c))
        return a[1] if a[0] == "rel" else None

    def pic_text(self, c, fn):
        """mirror a SUPER/sign/card into lane 2's text records (picture.text)"""
        from .picture import text as T
        try:
            self.text_items += fn(T)
        except (T.TextError, ValueError, KeyError) as e:
            self.d("SUPER", "the text module cannot set `%s`: %s" % (c.text(), e), c, "warning")

    def vis_op(self, f, c):
        """`+T show <prop>[:<state>]` / `hide <prop>` / `prop.set <prop> state=<s>` /
        `stroke <id> show|hide`: plane visibility, resolved to plane ids by emit."""
        kw = c.kw
        if kw == "stroke":
            sid = c.words[0] if c.words else c.fail("STROKE", "`stroke <id> show|hide`")
            if sid not in self.stroke_ix:
                c.fail("STROKE", "no stroke %s in set %s" % (sid, self.set_name))
            on = (c.words[1] if len(c.words) > 1 else "show") == "show"
            self.vis.append((f, "stroke", sid, None, on))
            self.tl(f, c, "show", "stroke " + ("show" if on else "hide"))
            return
        if kw == "prop.set":
            tgt = c.words[0] if c.words else c.fail("STAGE", "`prop.set <prop> state=<state>`")
            state, on = c.get("state"), True
        else:
            tgt = c.words[0] if c.words else c.fail("STAGE", "`%s <prop>[:<state>]`" % kw)
            tgt, _, state = tgt.partition(":")
            state, on = state or None, kw == "show"
        if tgt in self.actor_ix and not state:
            self.vis.append((f, "actor", tgt, None, kw == "show"))
            self.tl(f, c, "show", "%s %s" % (kw, tgt))
            return
        if tgt not in self.prop_ix and tgt not in self.plane_ix:
            c.fail("STAGE", "%s %s: no prop or plane %s in set %s (props: %s)" % (
                kw, tgt, tgt, self.set_name, ", ".join(self.prop_ix) or "none"))
        if state and tgt in self.prop_ix:
            arts = self.props[self.prop_ix[tgt]].get("states", {})
            if state not in arts:
                c.fail("STAGE", "%s %s:%s: the prop has no art.%s (states: %s)" % (
                    kw, tgt, state, state, ", ".join(sorted(arts)) or "none"))
        self.vis.append((f, "prop" if tgt in self.prop_ix else "plane", tgt, state, on))
        self.tl(f, c, "show", "%s %s%s" % (kw, tgt, ":" + state if state else ""))

    def wipe_op(self, f, c):
        """lane-2 wipes (the brick wipe, the Split, the dissolve): kept as statements for emit."""
        self.wipes.append((f, c.kw, dict(c.kv), list(c.words)))
        self.tl(f, c, "show", c.kw)

    def span(self, txt, f, c, default=None):
        if txt is None:
            if default is None:
                c.fail("TIME", "`%s` needs over=" % c.kw)
            return f, f + default
        a = timeexpr.parse(txt, _w(c))
        if a[0] == "span":
            return a[1], a[2]
        if a[0] == "rel":
            return f, f + a[1]
        c.fail("TIME", "over=%s: +a..+b or a frame count" % txt)

    def frames_arg(self, txt, c):
        if txt is None:
            return None
        t = str(txt)
        if t.startswith("+") and ".." in t:
            a = timeexpr.parse(t, _w(c))
            return a[2] - a[1]
        return docunits.frames(t.lstrip("+"), _w(c))

    def engine(self, f, c):
        kw = c.kw
        w = _w(c)
        words = list(c.words)
        line = None
        if kw in ("label",):
            name = words[0] if words else c.fail("TIME", "`label NAME`")
            if name in ("L_PUNCH", "L_BUTTON"):
                lid = {"L_PUNCH": 30, "L_BUTTON": 31}[name]
            else:
                if name not in self.label_ids:
                    if len(self.label_ids) >= 29:
                        c.fail("TIME", "more than 29 labels in a shot")
                    self.label_ids[name] = len(self.label_ids) + 1
                lid = self.label_ids[name]
            self.ev_direct.append((f, textpack_bridge.symbols()["BOP_LABEL"], lid, 0, w))
            self.tl(f, c, "mark", "label", extra={"label": name})
            return
        if kw == "punch" and not words:
            self.ev_direct.append((f, textpack_bridge.symbols()["BOP_LABEL"], 30, 0, w))
            self.tl(f, c, "mark", "punch")
            return
        if kw == "button":
            self.ev_direct.append((f, textpack_bridge.symbols()["BOP_LABEL"], 31, 0, w))
            self.tl(f, c, "mark", "button")
            return
        end = None
        if kw == "fade":
            if not words or words[0] not in ("in", "out"):
                c.fail("FADE", "`fade in|out over=F`")
            n = self.frames_arg(c.get("over"), c) or 8
            if words[0] == "in":
                if f == 0:
                    self.flags |= F.C["SHW_SHOT_FADE_IN"]    # camfx_fade(16) at setup (spec 6.4)
                line = "fade 0 %d" % n
            else:
                line = "fade 16 %d" % n
            end = f + n
        elif kw in ("iris_in", "iris_out"):
            at = c.get("at", "center")
            over = self.frames_arg(c.get("over"), c)
            hold = c.get("hold")
            shape = c.get("shape", "circle")
            if shape not in ("circle", "heart"):
                c.fail("TIME-IRIS", "shape=%s (circle|heart)" % shape)
            if over is not None:
                if over < 8:
                    c.fail("TIME-IRIS", "over=%d: an iris is R3 snap + hold + R5 open, so at least 8 f" % over)
                h = over - 8
                if hold is not None and docunits.frames(hold, w) != h:
                    c.fail("TIME-IRIS", "over=%d means hold %d (3 + hold + 5); hold=%s disagrees" % (over, h, hold))
            else:
                h = docunits.frames(hold, w) if hold is not None else 5
            if h > 63:
                c.fail("TIME-IRIS", "hold %d > 63 (6 bits in BOP_IRIS b)" % h)
            radius = int(c.get("radius", "0"))
            b = h | ((1 if shape == "heart" else 0) << 6) | ((radius // 2) << 7)
            if at.startswith("prop:"):
                pid = at[5:]
                if pid not in self.prop_ix:
                    c.fail("TIME-IRIS", "iris at=prop:%s: no such prop (props: %s)" % (
                        pid, ", ".join(self.prop_ix) or "none"))
                centre = self.prop_ix[pid] + 1
            elif at.startswith("actor:") or at in self.actor_ix:
                centre = 64 + self.actor(at.split(":", 1)[-1], c)
            elif at == "center":
                centre = 0
            else:
                x, y = self.xy(at, c)
                self.sop(f, "SOP_IRIS", [1 if kw == "iris_in" else 0, x, y, 1 if shape == "heart" else 0,
                                         8 + h, h], c)
                self.pic_events.append((f, 0x0100, [x, y, radius, 1 if shape == "heart" else 0,
                                                    0 if kw == "iris_in" else 1, h]))
                self.tl(f, c, "show", kw, end=f + 8 + h)
                return
            line = "%s %d %d" % (kw, centre, b)
            end = f + 8 + h
        elif kw in ("expr", "freeze", "thaw", "panic", "still", "twitch", "headturn"):
            if words and not re.fullmatch(r"-?\d+", words[0]):
                if words[0] == "all":
                    words[0] = "-1"
                else:
                    words[0] = str(self.actor(words[0], c))
            if kw == "twitch" and len(words) > 1:
                words[1] = {"on": "1", "off": "0"}.get(words[1], words[1])
            if kw == "headturn" and len(words) >= 3:
                end = f + docunits.frames(words[2], w)
                words[2] = str(docunits.frames(words[2], w))
            line = "%s %s" % (kw, " ".join(words))
            if kw == "expr" and len(words) > 1:
                textpack_bridge.sym(words[1], w, "EXPR_")
        elif kw == "pop":
            if words and not re.fullmatch(r"-?\d+", words[0]):
                if words[0] in self.prop_ix:
                    words[0] = str(64 + self.prop_ix[words[0]])
                else:
                    words[0] = str(self.actor(words[0], c))
            line = "pop %s" % " ".join(words)
        elif kw == "pal":
            n = self.frames_arg(c.get("over"), c) or 0
            line = "pal %s %s" % (" ".join(words[:2]), n)
            end = f + n
        elif kw == "flash":
            if len(words) >= 3:
                n = docunits.frames(words[2], w)
                words[2] = str(n)
                end = f + n
                if n > 3:
                    self.d("TIME-FLASH", "flash %d f: flashes are <= 3 f" % n, c, "warning", frame=f)
            line = "flash %s" % " ".join(words)
        elif kw in ("hitstop", "zoom"):
            ws = [str(docunits.frames(x, w)) if re.fullmatch(r"R\d+|\d+(\.\d+)?s", x) else x for x in words]
            line = "%s %s" % (kw, " ".join(ws))
        else:
            line = "%s %s" % (kw, " ".join(words))
        evs = textpack_bridge.compile_beat([(w, "+%d %s" % (f, line.strip())), (w, "+%d end" % f)],
                                           self.shot.id, c.file)
        endop = textpack_bridge.symbols()["BOP_END"]
        for at, anchor, op, a, b in evs:
            if op == endop:
                continue
            self.ev_direct.append((at, op, a, b, w))
        self.tl(f, c, "engine", kw, end=end)

    # -- audio ---------------------------------------------------------------------
    def audio_op(self, f, c, head, ast):
        kw = c.kw
        w = _w(c)
        smp = (self.shot.start + f) * SPF
        ev = None
        key = (c.file, c.line)
        if kw == "cc":
            text = c.strs[0] if c.strs else " ".join(c.words)
            prev = self.last_audio_on_line.get(key)
            if prev is not None:
                prev["cc"] = text
            self.captions.append({"in": self.shot.start + f, "out": self.shot.start + min(self.shot.len, f + 90),
                                  "text": text, "kind": "sdh", "shot": self.shot.id})
            self.tl(f, c, "audio", "cc")
            return
        if kw in ("sfx", "fol", "amb"):
            if not c.words:
                c.fail("AUD-LIB", "`%s <id>`" % kw)
            sid = c.words[0]
            lib = self.lib.sfx.get(sid)
            if lib is None:
                c.fail("AUD-LIB", "unknown sound %s (series/sfx.shw has: %s)" % (
                    sid, ", ".join(sorted(self.lib.sfx)[:16]) or "no sounds yet"))
            ev = {"smp": smp, "src": sid}
            if kw == "fol":
                if c.get("follow"):
                    if c.get("follow") not in self.stroke_ix:
                        c.fail("AUD-LIB", "fol follow=%s: no stroke of that name" % c.get("follow"))
                    ev["follow"] = "%s:%s" % (self.shot.id, c.get("follow"))
                if c.get("from"):
                    fr = c.get("from")
                    if fr.startswith("crowd:"):
                        ids = fr[6:].split(",")
                        for i in ids:
                            if i not in self.crowd_ix:
                                c.fail("AUD-LIB", "fol from=crowd:%s: no crowd %s" % (fr[6:], i))
                        fr = "crowd:" + ",".join("%s.%s" % (self.shot.id, i) for i in ids)
                    ev["from"] = fr
            if kw == "amb":
                ev["op"] = c.words[1] if len(c.words) > 1 else "on"
            ev["bus"] = lib.get("bus") or "FX"
            ev["gain_db"] = _add_db(lib["gain_db"], docunits.gain_db(c.get("gain", "0"), w))
            pan = c.get("pan", lib.get("pan"))
            if pan is not None and pan != "auto":
                ev["pan"] = {"center": 0.0, "centre": 0.0, "L": -1.0, "R": 1.0}.get(pan)
                if ev["pan"] is None:
                    try:
                        ev["pan"] = float(pan)
                    except ValueError:
                        c.fail("AUD-LIB", "pan=%s (auto|center|L|R|-1..1)" % pan)
        elif kw == "mx":
            if len(c.words) < 2:
                c.fail("AUD-MX", "`mx <cue> start|stop|mute F|duck <g> F|bend <a>-><b> over=F|to <g> over=F`")
            cue, opn = c.words[0], c.words[1]
            if cue not in self.lib.cues:
                c.fail("AUD-MX", "unknown cue %s (series/music.shw has: %s)" % (
                    cue, ", ".join(sorted(self.lib.cues)) or "no cues yet"))
            spot = self.lib.spots.get(cue, {})
            ev = {"smp": smp, "src": "cue:%s" % cue}
            if opn == "start":
                ev["from"] = c.get("from", spot.get("from") or "bar:1.1")
                ev["bus"] = spot.get("bus", "MX")
                ev["gain_db"] = docunits.gain_db(c.get("gain"), w) if c.get("gain") else spot.get("gain_db", 0)
            elif opn in ("stop", "mute", "duck", "bend", "to"):
                ev["op"] = opn
                rest = c.words[2:]
                if opn == "mute" and rest:
                    ev["over_smp"] = docunits.frames(rest[0], w) * SPF
                elif opn == "duck" and len(rest) >= 2:
                    ev["gain_db"] = docunits.gain_db(rest[0], w)
                    ev["over_smp"] = docunits.frames(rest[1], w) * SPF
                elif opn == "to" and rest:
                    ev["gain_db"] = docunits.gain_db(rest[0], w)
                elif opn == "bend" and rest:
                    ev["bend"] = rest[0]
                if c.get("over"):
                    ev["over_smp"] = self.frames_arg(c.get("over"), c) * SPF
                if c.get("gain"):
                    ev["gain_db"] = docunits.gain_db(c.get("gain"), w)
            else:
                c.fail("AUD-MX", "mx %s %s: start|stop|mute|duck|bend|to" % (cue, opn))
        elif kw == "walla":
            if len(c.words) < 2:
                c.fail("AUD-WALLA", "`walla <bed> on|off|to gain= [over=]`")
            bed, opn = c.words[0], c.words[1]
            if bed not in self.lib.wallas:
                c.fail("AUD-WALLA", "unknown walla bed %s (ep lines.shw `walla` lines: %s)" % (
                    bed, ", ".join(sorted(self.lib.wallas)) or "none yet"))
            if opn not in ("on", "off", "to"):
                c.fail("AUD-WALLA", "walla %s %s: on|off|to" % (bed, opn))
            ev = {"smp": smp, "src": "walla:%s" % bed, "op": opn}
            if opn == "on":
                ev["bus"] = "DX"
            if c.get("gain"):
                ev["gain_db"] = docunits.gain_db(c.get("gain"), w)
            if c.get("over"):
                ev["over_smp"] = self.frames_arg(c.get("over"), c) * SPF
        elif kw == "tapestop":
            n = docunits.frames(c.words[0], w) if c.words else 5
            ev = {"smp": smp, "src": "tapestop", "op": "tapestop", "over_smp": n * SPF}
        elif kw == "hum":
            if len(c.words) < 2:
                c.fail("AUD-HUM", "`hum <actor> on|off note=<cue>.<note>`")
            ai = self.actor(c.words[0], c)
            on = c.words[1] == "on"
            self.sop(f, "SOP_HUM", [ai, 1 if on else 0], c)
            ev = {"smp": smp, "src": "hum:%s" % self.actors[ai]["who"], "op": "on" if on else "off", "bus": "DX"}
            if c.get("note"):
                ev["note"] = c.get("note")
        elif kw == "stop":
            if not c.words:
                c.fail("AUD-STOP", "`stop <line>`")
            ev = {"smp": smp, "src": "line:%s" % c.words[0], "op": "stop"}
            for i, a in enumerate(self.actors):
                pass
        ev["_order"] = (self.shot.index, c.line, len(self.audio))
        ev["_f"] = self.shot.start + f
        ev["_shot"] = self.shot.id
        ev["_where"] = w
        self.audio.append(ev)
        self.last_audio_on_line[key] = ev
        ev["_tl"] = self.tl(f, c, "audio", kw, end=f + (ev.get("over_smp", 0) // SPF if ev.get("over_smp") else 0)
                            if ev.get("over_smp") else None)

    def say(self, f, c):
        if not c.words:
            c.fail("LINE", "`say <line> [actor] [mouth=auto|none|\"<track>\"]`")
        lid = c.words[0]
        ln = self.lib.lines.get(lid)
        if ln is None:
            c.fail("LINE", "unknown line %s (ep lines.shw)" % lid)
        t = self.lib.line_timing(lid, self.cast)
        ai = -1
        who = ln["speaker"]
        if len(c.words) > 1 and c.words[1].startswith("crowd:") and self.crowd_ref(c.words[1]) is None:
            pass                                # an off-screen crowd's line: the voice only
        elif len(c.words) > 1:
            ai = self.actor(c.words[1], c)
        else:
            hits = [i for i, a in enumerate(self.actors) if a["who"] == who]
            if hits:
                ai = hits[0]
        end = f + t["len"]
        if end > self.shot.len:
            self.d("AUD-OVERRUN", "line %s (%d f%s) ends at +%d, after the shot's end (+%d). Move it to +%d, "
                   "use a shorter take, or set retime=\"take\" in takes.toml" % (
                       lid, t["len"], ", provisional" if t["provisional"] else "", end, self.shot.len,
                       max(0, self.shot.len - t["len"])), c, "warning" if t["provisional"] else "error", frame=f)
        mouth = c.get("mouth", "auto")
        track = None
        if ai >= 0 and mouth != "none":
            if mouth == "auto":
                track = t["mouth"] or flap(ln["text"], t["len"])
            else:
                track = mouth
            if self.actors[ai]["kind_name"] == "chibi":
                track = "".join("X" if ch in "XM" else "A" for ch in track)
            self.mouth_track(ai, max(0, f - 1), track, c)
        if t["provisional"]:
            self.provisional = True
            self.flags |= F.C["SHW_SHOT_PROVISIONAL"]
        self.sop(f, "SOP_SAY", [ai, self.s(ln["text"]), min(end, self.shot.len), self.track_index(ai)], c)
        smp = (self.shot.start + f) * SPF
        ev = {"smp": smp, "src": "line:%s" % lid, "speaker": who, "bus": "DX", "len_smp": t["len"] * SPF}
        if t["provisional"]:
            ev["provisional"] = True
        ev.update(_order=(self.shot.index, c.line, len(self.audio)), _f=self.shot.start + f, _shot=self.shot.id,
                  _where=_w(c))
        self.audio.append(ev)
        self.last_audio_on_line[(c.file, c.line)] = ev
        ev["_tl"] = self.tl(f, c, "audio", "say", end=end, extra={"line": lid, "mouth": track})
        self.lines_said.append({"id": lid, "start": self.shot.start + f, "end": self.shot.start + end,
                                "provisional": t["provisional"], "actor": self.actors[ai]["id"] if ai >= 0 else None})
        self.captions.append({"in": self.shot.start + f, "out": self.shot.start + end, "text": ln["text"],
                              "kind": "line", "shot": self.shot.id, "line": lid})

    def track_index(self, ai):
        if ai < 0 or (ai, 0) not in self.tracks:
            return -1
        return list(self.tracks).index((ai, 0))

    def mouth_track(self, ai, f0, track, c):
        self.mouth_raw.append((ai, f0, track))
        key = (ai, 0)
        arr = self.tracks.get(key) or [0] * (self.shot.len + 1)
        for i, ch in enumerate(track):
            fr = f0 + i
            if fr > self.shot.len:
                break
            if ch not in self.lib.visemes:
                c.fail("MOUTH", "mouth track letter %r (visemes: %s)" % (ch, "".join(self.lib.visemes)))
            style = self.lib.visemes[ch]
            arr[fr] = 0 if style is None else textpack_bridge.sym(style, _w(c), "MOUTH_") + 1
        self.tracks[key] = arr

    # -- text ----------------------------------------------------------------------
    def super(self, f, c):
        if not c.words:
            c.fail("SUPER", "`super <string> style=type|pop|deadpan at=title|lower|x,y ...`")
        sid = c.words[0]
        s = self.lib.strings.get(sid)
        if s is None:
            c.fail("SUPER", "unknown string %s (ep strings.shw has: %s)" % (
                sid, ", ".join(sorted(self.lib.strings)[:16]) or "none yet"))
        style = c.get("style", "type")
        if style not in TXT_STYLES:
            c.fail("SUPER", "style=%s (type|pop|deadpan)" % style)
        at = c.get("at", "lower")
        if at == "lower":
            x, y = -1, 180 - TITLE_SAFE[1] - 24
        elif at == "title":
            x, y = -1, 84
        else:
            x, y = self.xy(at, c)
        glyphs = len(re.sub(r"\{[^}]*\}", "", s["text"]))
        a, b = f, f
        if style == "type":
            if c.get("over"):
                a, b = self.span(c.get("over"), f, c)
            elif c.get("cps"):
                cps = Fraction(c.get("cps"))
                a, b = f, f + int(math.ceil((glyphs - 1) * FPS / cps))
            else:
                a, b = f, f + 2 * (glyphs - 1)            # the default type rate: 15 cps
        until = c.get("until", "end")
        if until == "end":
            u = self.shot.len
        else:
            ua = timeexpr.parse(until, _w(c))
            if ua[0] != "rel":
                c.fail("SUPER", "until=%s: +N or end" % until)
            u = ua[1]
        if u > self.shot.len:
            self.d("SUPER", "until=+%d is past the shot's end (+%d)" % (u, self.shot.len), c)
            u = self.shot.len
        self.pic_text(c, lambda T: [T.super_(
            re.sub(r"\{[^}]*\}", "", s["text"]), style, f, self.shot.len, at=at,
            over=(a, b) if style == "type" else None, until=None if u >= self.shot.len else u,
            font=c.get("font") or s.get("font") or "game", scale=int(c.get("scale", "1")),
            align=c.get("align"), color=c.get("color"), outline=c.get("outline"), shadow=c.get("shadow"),
            hop=c.get("hop", "on") != "off", where=_w(c), sid=sid)])
        idx = self.supers
        self.supers += 1
        self.sop(f, "SOP_SUPER", [self.s(s["text"]), TXT_STYLES[style], x, y, a, b, u, glyphs], c)
        steps = []
        if style == "type" and glyphs > 1:
            last = None
            for g in range(1, glyphs + 1):
                fr = a + int(math.floor(Fraction((g - 1) * (b - a), glyphs - 1) + Fraction(1, 2)))
                if last is not None and fr == last[0]:
                    last[1] = g
                else:
                    last = [fr, g]
                    steps.append(last)
            for fr, g in steps:
                self.sop(fr, "SOP_TEXT_STEP", [idx, g], c)
        if u < self.shot.len:
            self.sop(u, "SOP_TEXT_CLEAR", [idx], c)
        self.tl(f, c, "text", "super", end=u, extra={
            "string": sid, "style": style, "glyphs": glyphs,
            "glyph_frames": [self.shot.start + fr for fr, _g in steps] if steps else None,
            "cps": round(float(Fraction(glyphs * FPS, b - a)), 1) if b > a else None})
        self.strings_shown = getattr(self, "strings_shown", []) + [{
            "id": sid, "text": s["text"], "shot": self.shot.id, "start": self.shot.start + f,
            "end": self.shot.start + u, "style": style, "tags": s.get("tags", [])}]
        self.captions.append({"in": self.shot.start + f, "out": self.shot.start + u, "text": s["text"],
                              "kind": "super", "shot": self.shot.id})

    # -- camera, actors, misc ----------------------------------------------------------
    def cam_op(self, f, c):
        w = _w(c)
        sub = c.words[0] if c.words else None
        prev = self.camkeys[-1] if self.camkeys else {"x_q16": 0, "y_q16": 0, "scale_q8": 256}
        over_n = self.frames_arg(c.get("over"), c) if c.get("over") else None
        if sub == "to":
            self.pic_cam(c, lambda k: k.to(f, over_n or 1, c.get("x"), c.get("y"), c.get("scale"),
                                           c.get("ease", "INOUT")))
        elif sub == "pan" and over_n:
            self.pic_cam(c, lambda k: k.by(f, over_n, c.get("dx"), c.get("dy"), c.get("ease", "INOUT")))
        elif sub == "pan":
            self.pic_cam(c, lambda k: k.pan(f, c.get("dx"), c.get("dy")))
        elif sub == "creep":
            self.pic_cam(c, lambda k: k.creep(f, c.get("dx"), c.get("dy"), int(c.get("ramp", "13"))))
        elif sub == "whip":
            self.pic_cam(c, lambda k: k.whip(f, c.words[1] if len(c.words) > 1 else "R", over_n or 6,
                                             c.get("dist", "320")))
        elif sub == "crane":
            self.pic_cam(c, lambda k: k.crane(f, c.get("dy", "0"), over_n or 30, c.get("ease", "INOUT")))
        elif sub == "stop":
            self.pic_cam(c, lambda k: k.stop(f))
            self.tl(f, c, "show", "cam stop")
            return
        if sub == "to":
            over = self.frames_arg(c.get("over"), c) or 0
            x = docunits.q16(docunits.num(c.get("x"), w)) if c.get("x") else prev["x_q16"]
            y = docunits.q16(docunits.num(c.get("y"), w)) if c.get("y") else prev["y_q16"]
            sc = int(round(float(docunits.num(c.get("scale"), w)) * 256)) if c.get("scale") else prev["scale_q8"]
            ease = c.get("ease", "INOUT")
            if ease not in EASES:
                c.fail("CAM", "ease=%s (IN|OUT|INOUT|LINEAR)" % ease)
            key = {"frame": f, "mode": 2, "ease": EASES[ease], "x_q16": x, "y_q16": y, "vx_q16": 0, "vy_q16": 0,
                   "scale_q8": sc, "over": over}
            if sc != 256 or prev["scale_q8"] != 256:
                self.tl(f, c, "show", "CAM-SCALE", end=f + over, extra={"from": prev["scale_q8"], "to": sc})
        elif sub in ("pan", "creep", "crane"):
            over = self.frames_arg(c.get("over"), c)
            if sub == "creep":
                v = docunits.q16(docunits.num(c.get("dx", "0"), w))
                key = {"frame": f, "mode": 1, "ease": 0, "x_q16": prev["x_q16"], "y_q16": prev["y_q16"],
                       "vx_q16": v, "vy_q16": 0, "scale_q8": prev["scale_q8"], "over": 0}
            else:
                dx = docunits.q16(docunits.num(c.get("dx", "0"), w))
                dy = docunits.q16(docunits.num(c.get("dy", "0"), w))
                key = {"frame": f, "mode": 2, "ease": 3, "x_q16": prev["x_q16"] + dx, "y_q16": prev["y_q16"] + dy,
                       "vx_q16": 0, "vy_q16": 0, "scale_q8": prev["scale_q8"], "over": over or 30}
        elif sub == "whip":
            d = c.words[1] if len(c.words) > 1 else "R"
            over = self.frames_arg(c.get("over"), c) or 4
            key = {"frame": f, "mode": 2, "ease": 1, "x_q16": prev["x_q16"] + ((320 if d == "R" else -320) << 16),
                   "y_q16": prev["y_q16"], "vx_q16": 0, "vy_q16": 0, "scale_q8": prev["scale_q8"], "over": over}
        else:
            c.fail("CAM", "cam to|pan|creep|whip|crane (setup: cam at= / cam drift)")
        self.camkeys.append(key)
        self.sop(f, "SOP_CAM", [len(self.camkeys) - 1], c)
        self.tl(f, c, "show", "cam " + sub, end=f + key.get("over", 0))

    def actor_op(self, f, c):
        kw = c.kw
        w = _w(c)
        if not c.words:
            c.fail("ACTOR", "`%s <actor> ...`" % kw)
        ai = self.actor(c.words[0], c)
        end = None
        if kw == "move":
            x, y = self.xy(c.get("to"), c, "to")
            over = self.frames_arg(c.get("over"), c) or 0
            tw = textpack_bridge.sym(c.get("tw"), w, "TW_") if c.get("tw") else -1
            self.sop(f, "SOP_MOVE", [ai, x, y, over, tw], c)
            end = f + over
        elif kw == "walk":
            an = self.anchor_xy(c.get("to") or "0", c)
            x = an[0] if an else self.snap(docunits.num((c.get("to") or "0").split(",")[0], w), c, "to")
            over = self.frames_arg(c.get("over"), c) or 0
            self.sop(f, "SOP_WALK", [ai, x, over], c)
            end = f + over
        elif kw == "squash":
            q = int(c.words[1]) if len(c.words) > 1 else 256
            over = self.frames_arg(c.get("over"), c) or 0
            self.sop(f, "SOP_SQUASH", [ai, q, over], c)
            end = f + over
        elif kw == "face":
            d = c.words[1] if len(c.words) > 1 else "R"
            if d not in ("L", "R"):
                c.fail("ACTOR", "face %s L|R" % c.words[0])
            self.sop(f, "SOP_FACE", [ai, 1 if d == "L" else 0], c)
        elif kw == "pose":
            fb = c.words[1] if len(c.words) > 1 else c.fail("ACTOR", "pose <actor> <flipbook>")
            sp = c.get("speed")
            self.sop(f, "SOP_POSE", [ai, self.s(fb), int(round(float(sp) * 256)) if sp else 256], c)
        elif kw == "blink":
            n = docunits.frames(c.words[1], w) if len(c.words) > 1 else 2
            self.sop(f, "SOP_BLINK", [ai, n], c)
            end = f + n
        elif kw == "state":
            s = c.words[1] if len(c.words) > 1 else "normal"
            if s not in STATES:
                c.fail("ACTOR", "state %s (%s)" % (s, "|".join(STATES)))
            if s != "normal" and self.actors[ai]["age"] == "child":
                self.d("STD-KIDS", "a child (%s) is never shown %s" % (self.actors[ai]["who"], s), c)
            self.sop(f, "SOP_STATE", [ai, STATES[s]], c)
        elif kw == "eyes":
            self.d("SHW-OPS", "eyes: gaze control is deferred to 2027 (spec 4.3); stage the glance with "
                   "headturn, expr and cutaways", c, "warning")
            return
        self.tl(f, c, "show", kw, end=end, extra={"actor": self.actors[ai]["id"]})

    def misc_op(self, f, c):
        kw = c.kw
        w = _w(c)
        if kw == "sign":
            sid = c.words[0] if c.words else c.fail("SIGN", "`sign <string> on=<prop>`")
            s = self.lib.strings.get(sid)
            if s is None:
                c.fail("SIGN", "unknown string %s" % sid)
            on = c.get("on")
            if on and on.startswith("mon:") and on not in self.prop_ix:
                x, y = self.xy(on, c)
                self.prop_ix[on] = len(self.props)          # an art-less anchor where the monster stands
                self.props.append({"plane": 0, "face": 0, "anchor": 1, "flags": 0, "x": x, "y": y - 24,
                                   "art": self.ep.strtab.add(None), "tex": F.NONE, "id": on, "par": 0,
                                   "plane_id": None, "src": None, "states": {}})
            if on not in self.prop_ix:
                c.fail("SIGN", "sign %s on=%s: no such prop" % (sid, on))
            self.sop(f, "SOP_SIGN", [self.s(s["text"]), self.prop_ix[on]], c)
            until = self.until_arg(c)
            self.pic_text(c, lambda T: [T.sign(re.sub(r"\{[^}]*\}", "", s["text"]), f, self.prop_ix[on] + 1,
                                               c.get("dx", "0"), c.get("dy", "0"), until,
                                               c.get("font") or s.get("font") or "game",
                                               int(c.get("scale", "1")), c.get("color"), c.get("outline"),
                                               where=_w(c))])
        elif kw == "card":
            tpl = c.words[0] if c.words else c.fail("CARD", "`card <template> k=v...`")
            kv = " ".join("%s=%s" % (k, v) for k, v in c.kv.items())
            until = self.until_arg(c)
            ckv = {k: v for k, v in c.kv.items() if k != "until"}
            self.pic_text(c, lambda T: T.card(tpl, f, until, **ckv))
            self.sop(f, "SOP_CARD", [self.s(tpl), self.s(kv)], c)
        elif kw == "fx":
            kind = c.words[0] if c.words else c.fail("FX", "`fx <kind> at=x,y n= seed=`")
            x, y = self.xy(c.get("at", "160,90"), c)
            self.sop(f, "SOP_FX", [self.s(kind), x, y, int(c.get("n", "8")), int(c.get("seed", "0"), 0)], c)
        elif kw == "boil":
            on = (c.words[0] if c.words else "on") == "on"
            self.sop(f, "SOP_BOIL", [1 if on else 0, int(c.get("every", "2"))], c)
            self.pic_events.append((f, 0x0101, [int(c.get("every", "2")) if on else 0]))
        elif kw == "cam3d":
            rail = c.get("rail") or c.fail("CAM", "`cam3d rail=<path> from= to=`")
            self.sop(f, "SOP_CAM3D", [self.s(rail), int(c.get("from", "0")), int(c.get("to", "0"))], c)
        self.tl(f, c, "show", kw)

    # -- asserts (COMP-LOCK) ----------------------------------------------------------------
    def check_asserts(self):
        for c in self.asserts:
            pos = c.pos
            try:
                if len(pos) < 3 or pos[1] != "==":
                    c.fail("COMP-LOCK", "`assert <obj>.x|y == <value> [@+N]`")
                obj, field = pos[0].split(".", 1) if "." in pos[0] else (pos[0], "x")
                want = docunits.num(pos[2], _w(c))
                at = 0
                if len(pos) > 3:
                    a = timeexpr.parse(pos[3][1:] if pos[3].startswith("@") else pos[3], _w(c))
                    at = a[1]
                if obj in self.prop_ix:
                    p = self.props[self.prop_ix[obj]]
                    par = Fraction(p["par"], 65536)
                    base = p["x"] if field == "x" else p["y"]
                    cam = self.cam_x(at) if field == "x" else Fraction(0)
                    val = base - cam * par
                elif obj in self.actor_ix:
                    a_ = self.actors[self.actor_ix[obj]]
                    val = Fraction(a_["x"] if field == "x" else a_["y"]) - (self.cam_x(at) if field == "x" else 0)
                else:
                    c.fail("COMP-LOCK", "assert: no prop or actor %s" % obj)
                ok = val == want
                rec = {"expr": "%s.%s == %s" % (obj, field, pos[2]), "shot_f": at, "ok": ok,
                       "value": float(val), "file": paths.rel(c.file), "line": c.line}
                self.asserts_done = getattr(self, "asserts_done", []) + [rec]
                if ok:
                    self.diags.append(Diag("COMP-LOCK", "ok %s.%s == %s at +%d (episode frame %d)" % (
                        obj, field, pos[2], at, self.shot.start + at), file=c.file, line=c.line, shot=self.shot.id,
                        severity="info"))
                else:
                    self.d("COMP-LOCK", "%s.%s is %s at +%d, not %s" % (obj, field, float(val), at, pos[2]), c,
                           frame=at)
            except ShowError as e:
                self.diags += e.diags


def _add_db(a, b):
    v = a + b
    return int(v) if float(v) == int(v) else v


# =============================================================================
# the episode
# =============================================================================
class Result:
    def __init__(self):
        self.diags = []
        self.timeline = None
        self.audio = None
        self.captions = None
        self.shw = None
        self.plan = ""
        self.shots = []
        self.slug = False

    @property
    def errors(self):
        return [d for d in self.diags if d.severity == "error"]

    @property
    def ok(self):
        return not self.errors

    def write(self, out_dir, name=None):
        from . import emit
        return emit.write(self, Path(out_dir), name)


class EpisodeCompiler:
    def __init__(self, ep="ep101", root=None, series=None, shots=None, slug=False, bake=True):
        self.prj = Project(ep, root, series)
        self.bake = bake
        self.want = shots
        self.slug_only = slug
        self.strtab = F.StrTab()

    def run(self):
        res = Result()
        res.slug = self.slug_only
        prj = self.prj
        parse.WARNINGS.clear()
        self.rundown = Rundown(prj, with_episode=not self.slug_only)
        if self.slug_only:
            self.lib = None
            from . import looks
            self.cast = looks.Cast({}, {}, {}, [], prj.series)
            shot_stmts = []
        else:
            self.lib = Library(prj, self.rundown.episode_stmts)
            from . import looks
            try:
                self.cast = looks.cast(prj.series)
            except Exception as e:
                prj.diags.append(Diag("CAST", "series/cast.shw could not be read: %s" % e, severity="warning"))
                self.cast = looks.Cast({}, {}, {}, [], prj.series)
            for e in self.cast.errors:
                prj.diags.append(Diag("CAST", e, severity="warning"))
            # shots: the episode.shw include tree first (STAGING's running order), then any
            # shots/*.shw file it does not include (the worked example co.shw, a new act in progress)
            shot_stmts = [st for st in self.rundown.episode_stmts if st.kw == "shot"]
            inc = {str(Path(st.file).resolve()) for st in shot_stmts}
            for p, stmts in prj.read_glob(prj.ep_dir / "shots"):
                if str(p.resolve()) in inc or p.resolve() in prj.included:
                    continue
                for st in parse.flatten(stmts):
                    if st.kw == "shot":
                        st.orphan = True
                        shot_stmts.append(st)
                    elif st.kw in ("act", "beat", "include", "string"):
                        pass
                    else:
                        prj.diags.append(st.diag("SHOT", "a shots file holds `shot ... end` blocks (got `%s`)"
                                                 % st.kw))
        self.layout(shot_stmts)
        sel = _shot_range([s for s in self.order if not s.slug], self.want, prj.diags) if self.want else None
        self.shot_by_id = {s.id: s for s in self.order}
        compilers = []
        for s in self.order:
            if s.slug:
                if sel is None:
                    compilers.append(self.slug_shot(s))
                continue
            if sel is not None and s.id not in sel:
                continue
            sc = ShotCompiler(self, s)
            sc.label_ids = {}
            sc.ev_direct = []
            try:
                sc.run()
            except ShowError as e:
                s.diags += e.diags
            compilers.append(sc)
        self.compilers = compilers
        self.assign_scenes()
        if not self.slug_only:
            self.cue_ops(compilers, sel)
        self.assign_k(compilers)
        diags = list(parse.WARNINGS) + list(prj.diags)
        for s in self.order:
            diags += s.diags
        diags += self.lints(compilers, sel)
        res.diags = diags
        from . import emit, plan
        emit.build(self, res, compilers, bake=self.bake)
        plan.plan(self, res, compilers)
        res.timeline["diagnostics"] = [{"rule": d.rule, "severity": d.severity, "text": d.format()}
                                       for d in res.diags]
        return res

    # -- layout: beats, shot lengths, slug fill --------------------------------------
    def layout(self, shot_stmts):
        prj = self.prj
        by_beat = {}
        seen = {}
        for st in shot_stmts:
            s = Shot(st)
            if not SHOT_RE.match(s.id):
                prj.diags.append(st.diag("SHOT", "shot id %r: letters, digits, _" % s.id))
                continue
            if s.id in seen:
                if getattr(st, "orphan", False):
                    prj.diags.append(st.diag("SHOT", "shot %s is also defined at %s:%d, which episode.shw includes; "
                                             "that one is used" % (s.id, paths.rel(seen[s.id].file), seen[s.id].line),
                                             severity="warning"))
                else:
                    prj.diags.append(st.diag("SHOT", "shot %s is already defined at %s:%d" % (
                        s.id, paths.rel(seen[s.id].file), seen[s.id].line)))
                continue
            seen[s.id] = s
            if not s.beat or s.beat not in self.rundown.by_id:
                prj.diags.append(st.diag("FMT-RUNTIME", "shot %s: beat=%s is not in the rundown" % (s.id, s.beat)))
                continue
            try:
                s.len = self.shot_len(s)
            except ShowError as e:
                prj.diags += e.diags
                s.len = 0
            by_beat.setdefault(s.beat, []).append(s)
        order = []
        for b in self.rundown.beats:
            f = b.start
            shots = by_beat.get(b.id, [])
            for s in shots:
                s.start = f
                f += s.len
                order.append(s)
            used = f - b.start
            b.shots = [s.id for s in shots]
            b.slug_frames = 0
            if used > b.len:
                prj.diags.append(Diag("FMT-RUNTIME", "beat %s is %d f but its shots (%s) are %d f: %d f over" % (
                    b.id, b.len, ", ".join(b.shots), used, used - b.len), file=b.stmt.file, line=b.stmt.line))
            elif used < b.len:
                slug = Shot(None, b, b.len - used)
                slug.start = f
                b.slug_frames = b.len - used
                order.append(slug)
                if shots and not self.slug_only:
                    prj.diags.append(Diag("FMT-RUNTIME", "beat %s: shots %s cover %d of %d f; the last %d f are a "
                                          "slug card" % (b.id, ",".join(b.shots), used, b.len, b.len - used),
                                          file=b.stmt.file, line=b.stmt.line, severity="warning"))
        for i, s in enumerate(order):
            s.index = i
        self.order = order
        total = sum(s.len for s in order)
        self.total = total
        if not (20 * 60 * FPS <= total <= 23 * 60 * FPS) and self.rundown.beats:
            prj.diags.append(Diag("FMT-RUNTIME", "the episode is %d f (%s); the web window is 20:00-23:00" % (
                total, docunits.tc(total))))

    def shot_len(self, s):
        st = s.stmt
        w = _w(st)
        v = st.get("len")
        if v is None:
            st.fail("FMT-RUNTIME", "shot %s has no len= (N | bar:N | @<line>.end+N | auto)" % s.id)
        if v.startswith("bar:"):
            cue_hdr = st.get("cue")
            cue = cue_hdr.split("@")[0] if cue_hdr else None
            if not cue or cue not in self.lib.cues:
                st.fail("FMT-RUNTIME", "len=%s needs cue=<cue>@bar:B in the header" % v)
            info = self.lib.cues[cue]
            num = int(info["meter"].split("/")[0])
            fr = Fraction(int(v[4:]) * num * 60 * FPS) / info["bpm"]
            return int(math.floor(fr + Fraction(1, 2)))
        if v.startswith("@") or v == "auto":
            # the line's length (voice.lock, else the estimate) from where it is said
            says = {}
            for c in st.children:
                if c.kw == "say" and c.words and c.time:
                    a = timeexpr.parse(c.time, _w(c))
                    if a[0] == "rel":
                        says[c.words[0]] = a[1]
            if v == "auto":
                if not says:
                    st.fail("FMT-RUNTIME", "len=auto needs a `say` at +N")
                ends = [f + self.lib.line_timing(l, self.cast)["len"] for l, f in says.items()]
                return max(ends) + 13
            a = timeexpr.parse(v, w)
            if a[0] != "ref" or a[2] not in ("end", "start") or a[1] not in says:
                st.fail("FMT-RUNTIME", "len=%s: @<line>.end+N with that line said at +N in the shot" % v)
            t = self.lib.line_timing(a[1], self.cast)
            return says[a[1]] + (t["len"] if a[2] == "end" else 0) + a[3]
        n = docunits.frames(v, w)
        if n <= 0 or n > 0xFFFF:
            st.fail("FMT-RUNTIME", "len=%s: 1..65535 frames" % v)
        return n

    def slug_shot(self, s):
        class Slug:
            pass
        sc = Slug()
        sc.shot = s
        s.compiled = True
        sc.slug = True
        return sc

    # -- scenes: a shot continues the previous one's scene unless it names one -----------
    def assign_scenes(self):
        cur = None
        for s in self.order:
            if s.slug:
                continue
            sc = s.kv.get("scene")
            if s.compiled:
                pass
            if sc:
                cur = sc
            elif s.scene:
                cur = s.scene
            elif cur is None:
                cur = s.beat
            s.scene = s.scene or cur
            cur = s.scene

    # -- cues.shw timed music ops -> the shot covering their frame ------------------------
    def cue_ops(self, compilers, sel):
        by_shot = {c.shot.id: c for c in compilers if not getattr(c, "slug", False)}
        slug_by = {c.shot.id: c for c in compilers if getattr(c, "slug", False)}
        for st in self.lib.cue_ops:
            w = _w(st)
            try:
                a = timeexpr.parse(st.time, w)
                if a[0] == "tc":
                    f = a[1] + a[2]
                elif a[0] == "ref" and a[1] in self.rundown.by_id:
                    f = self.rundown.by_id[a[1]].start + a[3]
                elif a[0] == "ref" and a[1] in self.shot_by_id:
                    s = self.shot_by_id[a[1]]
                    f = s.start + (s.len if a[2] == "end" else 0) + a[3]
                elif a[0] == "cuebar":
                    cue = a[1]
                    starts = sorted(e["_f"] for c in compilers for e in getattr(c, "audio", [])
                                    if e.get("src") == "cue:%s" % cue and "op" not in e)
                    if not starts or cue not in self.lib.cues:
                        continue        # its cue does not start inside what is compiled
                    info = self.lib.cues[cue]
                    num = int(info["meter"].split("/")[0])
                    fpb = Fraction(60 * FPS) / info["bpm"]
                    f = starts[0] + int(math.floor(((a[2] - 1) * num + a[3] - 1) * fpb + Fraction(1, 2))) + a[4]
                else:
                    raise ShowError(Diag("TIME", "cues.shw: @<beat>+N, @tc:HH:MM:SS:FF or @<cue>:bar:B.b", **w))
            except ShowError as e:
                self.prj.diags += e.diags
                continue
            cover = None
            for s in self.order:
                if s.start <= f < s.start + s.len or (f == s.start + s.len and s is self.order[-1]):
                    cover = s
                    break
            if cover is None:
                self.prj.diags.append(st.diag("TIME", "cues.shw op at frame %d is outside the episode" % f))
                continue
            sc = by_shot.get(cover.id) or slug_by.get(cover.id)
            if sc is None:
                continue
            if getattr(sc, "slug", False):
                sc = self._slug_compiler(sc)
            # same op already in the shot? (STAGING moved it in) -> merge
            src = "cue:%s" % st.words[0] if st.words else None
            opn = st.words[1] if len(st.words) > 1 else None
            if any(e.get("src") == src and e.get("op", "start") == opn and e["_f"] == f for e in sc.audio):
                continue
            st2 = parse.Stmt(st.kw, st.toks, st.file, st.line, time="+%d" % (f - cover.start))
            try:
                sc.audio_op(f - cover.start, st2, (st.file, st.line), ("rel", f - cover.start))
                sc.audio[-1]["_order"] = (cover.index, 1 << 20, st.line)
            except ShowError as e:
                self.prj.diags += e.diags
        # put converted slug compilers back
        for i, c in enumerate(compilers):
            if getattr(c, "slug", False) and getattr(c, "_full", None):
                compilers[i] = c._full

    def _slug_compiler(self, slug):
        if getattr(slug, "_full", None):
            return slug._full
        sc = ShotCompiler(self, slug.shot)
        sc.label_ids, sc.ev_direct = {}, []
        sc.last_audio_on_line = {}
        sc.set_name, sc.set_kind, sc.setdef = None, "slug", None
        sc.is_slug = True
        slug._full = sc
        return sc

    def assign_k(self, compilers):
        evs = [e for c in compilers for e in getattr(c, "audio", [])]
        evs.sort(key=lambda e: (e["smp"], e["_order"]))
        for k, e in enumerate(evs):
            e["k"] = k
            if e.get("_tl") is not None:
                e["_tl"]["k"] = k
        self.audio_events = evs

    # -- compiler lints (the rest live in showtk/lint/) -------------------------------------
    def lints(self, compilers, sel):
        out = []
        frame0 = []
        for c in compilers:
            if getattr(c, "slug", False) or getattr(c, "is_slug", False):
                continue
            fr = {}
            for e in c.audio:
                fr.setdefault(e["_f"], []).append(e)
            for f, lst in sorted(fr.items()):
                if len(lst) > P4_MAX:
                    out.append(Diag("AUD-P4", "%d audio events on one frame; the mailbox publishes %d (p4_k)" % (
                        len(lst), P4_MAX), shot=c.shot.id, frame=f - c.shot.start, **lst[0]["_where"]))
            hit = [e for e in c.audio if e["_f"] == c.shot.start]
            if hit:
                frame0.append(c.shot.start * SPF)
            else:
                out.append(Diag("AUD-FRAME0", "no sound on frame 0 of the cut into %s" % c.shot.id,
                                shot=c.shot.id, frame=0, severity="warning", **c.shot.where()))
        if frame0 and len(frame0) == len([c for c in compilers if not getattr(c, "slug", False)
                                          and not getattr(c, "is_slug", False)]):
            names = [c.shot.id for c in compilers if not getattr(c, "slug", False)]
            out.append(Diag("AUD-FRAME0", "ok: every cut has a sound on frame 0 (smp %s)" % ", ".join(
                str(x) for x in frame0), shot="%s..%s" % (names[0], names[-1]) if len(names) > 1 else names[0],
                severity="info"))
        return out


def compile_episode(ep="ep101", root=None, series=None, shots=None, slug=False, bake=True):
    return EpisodeCompiler(ep, root, series, shots, slug, bake).run()


def inputs_hash(files):
    h = hashlib.sha256()
    for p in sorted(set(Path(f) for f in files)):
        h.update(paths.rel(p).encode() + b"\0")
        h.update(p.read_bytes())
    return h.hexdigest()
