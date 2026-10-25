"""showtk.edl - the deliver EDL and its segments (WP12, SHOW_ENGINE_SPEC.md sec.12.2).

An EDL is the list of (shot master, in, out) pieces that make an episode range,
in episode frames.  It is cut into segments, one per beat (H.264, closed GOP),
each keyed by a hash of exactly what it is made of, so a one-shot change
re-encodes one segment and the rest are reused from build/<ep>/segments/.

Masters are found, in order:
  1. --masters DIR:   DIR/<shot>.mkv or DIR/<shot>/master.mkv
  2. the shot's cache key from timeline.json:  build/cache/shots/<key>/master.mkv
  3. build/<ep>/render_index.json  {"co01": "<cache dir or key>", ...}
"""
import hashlib
import json
import os
from pathlib import Path

from .sidecars import SHOW, build_dir


class MissingMasters(RuntimeError):
    def __init__(self, ep, shots):
        self.shots = shots
        super().__init__("%s: no rendered master for %d shot(s): %s; run `show render %s %s`" % (
            ep, len(shots), ", ".join(shots[:12]) + (" ..." if len(shots) > 12 else ""), ep,
            shots[0] + (".." + shots[-1] if len(shots) > 1 else "")))


def cache_root():
    root = os.environ.get("SHOW_CACHE")
    return Path(root) if root else SHOW / "build" / "cache" / "shots"


class Masters:
    """Shot id -> master.mkv (+ its frames.sha256, badge.json when the capture wrote them)."""

    def __init__(self, tl, masters_dir=None):
        self.tl = tl
        self.dir = Path(masters_dir) if masters_dir else None
        self.index = {}
        idx = build_dir(tl.episode) / "render_index.json"
        if idx.exists():
            data = json.loads(idx.read_text())
            self.index = data.get("shots", data) if isinstance(data, dict) else {}

    def path(self, sid):
        cands = []
        if self.dir:
            cands += [self.dir / (sid + ".mkv"), self.dir / sid / "master.mkv"]
        shot = self.tl.shot(sid)
        if shot.key:
            cands.append(cache_root() / str(shot.key) / "master.mkv")
        ent = self.index.get(sid)
        if isinstance(ent, dict):
            ent = ent.get("dir") or ent.get("key") or ent.get("path")
        if ent:
            p = Path(ent)
            if not p.is_absolute():
                p = cache_root() / ent if not (SHOW / ent).exists() else SHOW / ent
            cands += [p if p.suffix == ".mkv" else p / "master.mkv"]
        for c in cands:
            if c.exists():
                return c
        return None

    def require(self, sids):
        missing = [s for s in sids if self.path(s) is None]
        if missing:
            raise MissingMasters(self.tl.episode, missing)
        return {s: self.path(s) for s in sids}

    def content_hash(self, sid):
        """The capture's frames.sha256 (the frame hashes) when present, else the file's."""
        p = self.path(sid)
        fh = p.parent / "frames.sha256"
        if p.name == "master.mkv" and fh.exists():
            return hashlib.sha256(fh.read_bytes()).hexdigest()
        side = p.with_suffix(".frames.sha256")
        if side.exists():
            return hashlib.sha256(side.read_bytes()).hexdigest()
        h = hashlib.sha256()
        with open(p, "rb") as f:
            for chunk in iter(lambda: f.read(1 << 20), b""):
                h.update(chunk)
        return h.hexdigest()

    def badge(self, sid):
        p = self.path(sid)
        for b in (p.parent / "badge.json", p.with_suffix(".badge.json")):
            if p and b.exists():
                try:
                    d = json.loads(b.read_text())
                    return d.get("badge", "") if isinstance(d, dict) else str(d)
                except ValueError:
                    pass
        return self.tl.shot(sid).badge or ""


class Piece:
    """Frames [a, b) of one shot's master, at episode frames [f0, f0 + b - a)."""

    def __init__(self, shot, a, b, master):
        self.shot = shot
        self.a, self.b = a, b
        self.f0 = shot.start + a
        self.master = master

    @property
    def n(self):
        return self.b - self.a

    def entry(self):
        return (self.master, self.a, self.b)


class Segment:
    def __init__(self, sid, pieces):
        self.id = sid
        self.pieces = pieces
        self.f0 = pieces[0].f0
        self.f1 = pieces[-1].f0 + pieces[-1].n

    @property
    def n(self):
        return self.f1 - self.f0

    def key(self, recipe, masters, extra=""):
        h = hashlib.sha256()
        h.update(("seg1|%s|%s|%d|%d|" % (recipe, self.id, self.f0, self.f1)).encode())
        for p in self.pieces:
            h.update(("%s:%d:%d:%s|" % (p.shot.id, p.a, p.b, masters.content_hash(p.shot.id))).encode())
        h.update(extra.encode())
        return h.hexdigest()[:24]


class EDL:
    def __init__(self, tl, f0, f1, masters):
        self.tl, self.f0, self.f1, self.masters = tl, f0, f1, masters
        shots = tl.shots_in(f0, f1)
        paths = masters.require([s.id for s in shots])
        self.pieces = []
        for s in shots:
            a = max(f0, s.start) - s.start
            b = min(f1, s.end) - s.start
            self.pieces.append(Piece(s, a, b, paths[s.id]))
        if not self.pieces:
            raise ValueError("empty range %d..%d" % (f0, f1))
        gaps = [(p.f0, q.f0) for p, q in zip(self.pieces, self.pieces[1:]) if p.f0 + p.n != q.f0]
        if gaps:
            raise ValueError("the timeline has holes between shots at %s" % gaps[:3])

    @property
    def n(self):
        return self.f1 - self.f0

    def segments(self, by="beat"):
        segs, cur, cur_id = [], [], None
        for p in self.pieces:
            sid = (p.shot.beat or p.shot.id) if by == "beat" else p.shot.id
            if cur and sid != cur_id:
                segs.append(Segment(cur_id, cur))
                cur = []
            cur.append(p)
            cur_id = sid
        if cur:
            segs.append(Segment(cur_id, cur))
        return segs

    def write(self, path):
        from .ffmpeg import write_ffconcat
        return write_ffconcat([p.entry() for p in self.pieces], path)

    def table(self):
        """Human-readable EDL (also written next to the deliverable)."""
        from .sidecars import tc
        rows = ["# EDL %s %d..%d (%d f)" % (self.tl.episode, self.f0, self.f1 - 1, self.n),
                "# rec_in       rec_out      shot     beat               src_in src_out master"]
        for p in self.pieces:
            rows.append("%s  %s  %-8s %-18s %6d %7d %s" % (
                tc(p.f0 - self.f0), tc(p.f0 - self.f0 + p.n), p.shot.id, p.shot.beat or "-",
                p.a, p.b, p.master))
        return "\n".join(rows) + "\n"
