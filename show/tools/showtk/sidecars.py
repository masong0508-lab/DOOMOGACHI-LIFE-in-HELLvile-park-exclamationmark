"""showtk.sidecars - the read side of the compiler's sidecars (WP12/WP13/WP17).

The compiler (showc, emit.py) writes build/<ep>/timeline.json,
audio_events.json and captions.json (SHOW_ENGINE_SPEC.md sec.3, sec.6.4-6.6).
Every delivery, review and standards tool reads them through this module, so
a change in the compiler's field names is absorbed in one place.

    tl = load_timeline("ep101")             # or load_timeline(path=...)
    tl.shot("co02").start, tl.range("co01..co03"), tl.scene_of("co02")

Field names accepted (the first one found wins), so the reader keeps working
while the emitter settles:
    frame   f | frame | at | ep_frame          (episode frame)
    start   start | f0 | in | start_frame
    len     len | frames | length
    end     end | f1 | out | end_frame          (exclusive unless noted)
"""
import json
import os
import re
from pathlib import Path

FPS = 30
RATE = 48000
SPF = RATE // FPS          # 1,600 samples per frame (sec.10.1)

SHOW = Path(__file__).resolve().parents[2]          # show/
REPO = SHOW.parent

RUNGS = (2, 3, 5, 8, 13, 21, 26)                    # the comic-timing ladder


def build_dir(ep):
    """show/build/<ep> (SHOW_BUILD overrides the build root)."""
    root = os.environ.get("SHOW_BUILD")
    return Path(root) / ep if root else SHOW / "build" / ep


def ep_dir(ep):
    """show/<ep> (the committed episode sources)."""
    root = os.environ.get("SHOW_EPISODES")
    return Path(root) / ep if root else SHOW / ep


def tc(frame, fps=FPS):
    """Episode frame -> HH:MM:SS:FF."""
    frame = int(frame)
    ff = frame % fps
    s = frame // fps
    return "%02d:%02d:%02d:%02d" % (s // 3600, s // 60 % 60, s % 60, ff)


def parse_tc(text, fps=FPS):
    m = re.fullmatch(r"(\d+):(\d\d):(\d\d)[:;](\d\d)", text.strip())
    if not m:
        raise ValueError("not a timecode: %r" % text)
    h, mi, s, f = (int(x) for x in m.groups())
    return ((h * 60 + mi) * 60 + s) * fps + f


def rung(gap):
    """A gap in frames -> its ladder name: 'R8', or '~R8+1' when off the ladder."""
    if gap <= 0:
        return "same"
    best = min(RUNGS, key=lambda r: (abs(r - gap), r))
    d = gap - best
    return "R%d" % best if d == 0 else "~R%d%+d" % (best, d)


def _get(d, *names, default=None):
    for n in names:
        if isinstance(d, dict) and n in d and d[n] is not None:
            return d[n]
    return default


def _tags(v):
    """Tags as a list of dicts {name, to, consent, ...} from a string or list."""
    out = []
    if v is None:
        return out
    if isinstance(v, str):
        v = [t for t in re.split(r"[,\s]+", v) if t]
    for t in v:
        if isinstance(t, dict):
            d = dict(t)
            d.setdefault("name", d.get("tag", ""))
            out.append(d)
            continue
        t = str(t)
        name, _, rest = t.partition(":")
        d = {"name": name}
        if rest:
            d["to"] = rest
        out.append(d)
    return out


class Event:
    """One timed event of a shot, in episode frames."""

    def __init__(self, d, shot):
        self.raw = d
        self.shot = shot
        t = _get(d, "t", "rel", "shot_frame")
        f = _get(d, "f", "frame", "at", "ep_frame")
        if f is None and t is not None:
            f = shot.start + int(t)
        if t is None and f is not None:
            t = int(f) - shot.start
        self.f = int(f if f is not None else shot.start)
        self.t = int(t if t is not None else 0)
        self.op = str(_get(d, "op", "kind", "name", default="?"))
        self.args = _get(d, "args", "arg", default="")
        span = _get(d, "span", "over")
        if isinstance(span, (list, tuple)) and len(span) == 2:
            self.span = (int(span[0]), int(span[1]))
        else:
            self.span = None
        self.src = _get(d, "src", "where", default="")
        self.line = _get(d, "line", "line_id")
        self.text = _get(d, "text")
        self.k = _get(d, "k")
        self.visual = bool(_get(d, "visual", default=self.op not in AUDIO_OPS))
        self.tags = _tags(_get(d, "tags"))

    def __repr__(self):
        return "Event(%s %s @%d)" % (self.shot.id, self.op, self.f)


AUDIO_OPS = {"sfx", "fol", "amb", "mx", "walla", "say", "hum", "cc", "stop", "tapestop", "ad",
             "label", "punch", "button", "nobutton", "tag", "scene", "audio"}


class Shot:
    def __init__(self, d, tl):
        self.raw = d
        self.tl = tl
        self.id = str(_get(d, "id", "shot"))
        self.beat = _get(d, "beat")
        self.act = _get(d, "act")
        self.start = int(_get(d, "start", "f0", "in", "start_frame", default=0))
        ln = _get(d, "len", "frames", "length")
        if ln is None:
            ln = int(_get(d, "end", "f1", "out", "end_frame")) - self.start
        self.len = int(ln)
        self.px = int(_get(d, "px", default=1))
        self.look = _get(d, "look", default="pixel")
        self.set = _get(d, "set")
        self.set_kind = str(_get(d, "set_kind", "kind", default="2d")).lower()
        self.scene = _get(d, "scene")
        self.cut = _get(d, "cut", default="CUT")
        self.gore = str(_get(d, "gore", default="CUTE")).upper()
        self.tags = _tags(_get(d, "tags"))
        self.insert = bool(_get(d, "insert", default=False))
        self.badge = _get(d, "badge", "badge_est", default="")
        self.key = _get(d, "key", "cache_key")
        self.src = _get(d, "src", "where", default="")
        self.actors = list(_get(d, "actors", default=[]) or [])
        self.crowds = list(_get(d, "crowds", default=[]) or [])
        self.assets = list(_get(d, "assets", default=[]) or [])
        self.fp = bool(_get(d, "fp", "first_person", default=self.set_kind in ("view3d", "fp")))
        self.thumb = _get(d, "thumb")
        self.events = [Event(e, self) for e in (_get(d, "events", default=[]) or [])]
        self.events.sort(key=lambda e: (e.f, e.op))

    @property
    def end(self):
        return self.start + self.len

    def __repr__(self):
        return "Shot(%s %d+%d)" % (self.id, self.start, self.len)


class Line:
    def __init__(self, d, tl):
        self.raw = d
        self.id = str(_get(d, "id", "line"))
        self.who = _get(d, "who", "speaker", "char")
        self.text = _get(d, "text", default="")
        self.shot = _get(d, "shot")
        self.start = _get(d, "start", "f0", "in", "frame", "f")
        self.end = _get(d, "end", "f1", "out")
        if self.start is not None:
            self.start = int(self.start)
        if self.end is None and self.start is not None:
            ln = _get(d, "len", "frames")
            if ln is not None:
                self.end = self.start + int(ln)
        if self.end is not None:
            self.end = int(self.end)
        self.tags = _tags(_get(d, "tags"))
        self.ref = _get(d, "ref")
        self.to = _get(d, "to")
        self.consent = _get(d, "consent")
        self.take = str(_get(d, "take", "src", default="tts"))
        self.provisional = bool(_get(d, "provisional", default=False))
        self.sdh = _get(d, "sdh", "cc")
        self.mouth = _get(d, "mouth")
        self.walla = bool(_get(d, "walla", default=str(self.who or "").startswith("walla:")))
        self.voice = _get(d, "voice")
        self.src = _get(d, "where", "file", default="")
        self.scene = _get(d, "scene")

    @property
    def tts(self):
        return self.take.startswith("tts")

    def tag_names(self):
        return {t["name"] for t in self.tags}

    def __repr__(self):
        return "Line(%s %s)" % (self.id, self.who)


class Timeline:
    def __init__(self, data, path=None):
        self.raw = data
        self.path = path
        self.episode = str(_get(data, "episode", "ep", "id", default="ep101"))
        self.title = _get(data, "title", default="")
        self.fps = int(_get(data, "fps", default=FPS))
        self.rate = int(_get(data, "audio_rate", "rate", "audio", default=RATE))
        canvas = _get(data, "canvas", default={}) or {}
        if isinstance(canvas, str):          # "320x180@320x240"
            m = re.match(r"(\d+)x(\d+)(?:@(\d+)x(\d+))?", canvas)
            w, h = int(m.group(1)), int(m.group(2))
            fh = int(m.group(4) or h)
            canvas = {"w": w, "h": h, "y_off": (fh - h) // 2, "full_h": fh}
        self.canvas = {"w": int(canvas.get("w", 320)), "h": int(canvas.get("h", 180)),
                       "y_off": int(canvas.get("y_off", 30)),
                       "full_h": int(canvas.get("full_h", 240))}
        self.shots = [Shot(s, self) for s in (_get(data, "shots", default=[]) or [])]
        self.shots.sort(key=lambda s: s.start)
        self._by_id = {s.id: s for s in self.shots}
        self.beats = list(_get(data, "beats", default=[]) or [])
        self.acts = list(_get(data, "acts", default=[]) or [])
        self.lines = [Line(x, self) for x in (_get(data, "lines", default=[]) or [])]
        self.strings = list(_get(data, "strings", "supers", default=[]) or [])
        self.cast = dict(_get(data, "cast", default={}) or {})
        self.voices = dict(_get(data, "voices", default={}) or {})
        self.chapters = list(_get(data, "chapters", default=[]) or [])
        self.release = dict(_get(data, "release", default={}) or {})
        # beats and acts given only on the shots
        if not self.beats:
            seen = {}
            for s in self.shots:
                if s.beat and s.beat not in seen:
                    seen[s.beat] = {"id": s.beat, "act": s.act, "start": s.start, "len": 0}
                    self.beats.append(seen[s.beat])
                if s.beat:
                    b = seen[s.beat]
                    b["len"] = s.end - b["start"]
        for b in self.beats:
            b.setdefault("start", min((s.start for s in self.shots if s.beat == b["id"]), default=0))
            if "len" not in b:
                b["len"] = max((s.end for s in self.shots if s.beat == b["id"]), default=0) - b["start"]
        for s in self.shots:
            if not s.act:
                b = self.beat(s.beat)
                s.act = b.get("act") if b else None
        for ln in self.lines:          # place lines on shots by frame when no shot is named
            if ln.shot is None and ln.start is not None:
                s = self.shot_at(ln.start)
                ln.shot = s.id if s else None
            if ln.start is None and ln.shot in self._by_id:
                ln.start = self._by_id[ln.shot].start
            if ln.end is None and ln.start is not None:
                ln.end = ln.start + max(30, 9 * len(str(ln.text).split()) + 15)
        # scenes: a shot without scene= shares the previous shot's (sec.5.5)
        cur = None
        for s in self.shots:
            if s.scene:
                cur = s.scene
            else:
                s.scene = cur or s.id
        for ln in self.lines:
            if not ln.scene:
                ln.scene = self._by_id[ln.shot].scene if ln.shot in self._by_id else None

    # ---- lookups ---------------------------------------------------------------
    @property
    def length(self):
        return max((s.end for s in self.shots), default=0)

    def shot(self, sid):
        return self._by_id[sid]

    def has_shot(self, sid):
        return sid in self._by_id

    def beat(self, bid):
        for b in self.beats:
            if b.get("id") == bid:
                return b
        return None

    def act(self, aid):
        for a in self.acts:
            if a.get("id") == aid:
                return a
        return None

    def shot_at(self, frame):
        for s in self.shots:
            if s.start <= frame < s.end:
                return s
        return None

    def lines_in(self, f0, f1):
        return [ln for ln in self.lines
                if ln.start is not None and ln.start < f1 and (ln.end or ln.start + 1) > f0]

    def scenes(self):
        """{scene id: [shots]} in episode order."""
        out = {}
        for s in self.shots:
            out.setdefault(s.scene, []).append(s)
        return out

    def scene_of(self, sid):
        return self._by_id[sid].scene

    def cast_entry(self, who):
        who = str(who or "")
        if who.startswith("walla:"):
            return self.voices.get(who[6:], {})
        return self.cast.get(who, {})

    # ---- ranges -----------------------------------------------------------------
    def _endpoint(self, tok, is_end):
        tok = tok.strip()
        if tok.lstrip("+").isdigit():
            return int(tok) + (1 if is_end else 0)
        if re.fullmatch(r"\d+:\d\d:\d\d[:;]\d\d", tok):
            return parse_tc(tok) + (1 if is_end else 0)
        if tok in self._by_id:
            s = self._by_id[tok]
            return s.end if is_end else s.start
        b = self.beat(tok)
        if b:
            return b["start"] + b["len"] if is_end else b["start"]
        a = self.act(tok)
        if a:
            shots = [s for s in self.shots if s.act == tok]
            if shots:
                return shots[-1].end if is_end else shots[0].start
        acts = [s for s in self.shots if s.act == tok]
        if acts:
            return acts[-1].end if is_end else acts[0].start
        raise KeyError("unknown shot, beat, act or frame: %r" % tok)

    def range(self, spec=None):
        """'co01..co03' | 'co02' | 'CO' | '0..389' | 'all' -> (f0, f1) with f1 exclusive."""
        if spec in (None, "", "all"):
            return 0, self.length
        if ".." in spec:
            a, b = spec.split("..", 1)
            return self._endpoint(a, False), self._endpoint(b, True)
        return self._endpoint(spec, False), self._endpoint(spec, True)

    def shots_in(self, f0, f1):
        return [s for s in self.shots if s.start < f1 and s.end > f0]

    def badges(self):
        return {s.id: s.badge for s in self.shots}


def load_json(path):
    with open(path, encoding="utf-8") as fh:
        return json.load(fh)


def load_timeline(ep=None, path=None):
    path = Path(path) if path else build_dir(ep) / "timeline.json"
    return Timeline(load_json(path), path)


def load_audio_events(ep=None, path=None):
    """audio_events.json as a list of dicts with an added episode frame 'f'."""
    path = Path(path) if path else build_dir(ep) / "audio_events.json"
    if not path.exists():
        return []
    data = load_json(path)
    if isinstance(data, dict):
        data = data.get("events", [])
    for e in data:
        e.setdefault("f", int(e.get("smp", 0)) // SPF)
    return data


def load_captions_json(ep=None, path=None):
    path = Path(path) if path else build_dir(ep) / "captions.json"
    if not path.exists():
        return []
    data = load_json(path)
    if isinstance(data, dict):
        data = data.get("captions", data.get("cues", []))
    return data
