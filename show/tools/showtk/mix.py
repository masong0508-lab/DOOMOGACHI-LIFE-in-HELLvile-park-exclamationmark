"""showtk.mix - the kit mixer driven by audio_events.json (WP11; SHOW_ENGINE_SPEC.md sec.10.6).

    show mix ep101                       build/ep101/{audio_events,timeline}.json -> build/ep101/audio/
    show mix ep101 --range co01..co03    stop the mix at the end of co03 (it always starts at sample 0)
    show mix dxp --build DIR --lock show/tests/audio/dx_proof/voice.lock
    show mix ep101 --p4 [PLOG]           + the P4 cross-check against a render's plog.tsv (AT11.2)

One 48 kHz clock (frame f = sample 1,600 f). Every event k of audio_events.json is
placed on its own sample and lands on one of three buses (the AD bus is deferred):

    DX  dialogue lines (`line:<id>`, the locked take; take sample `onset` on the event's
        sample, panned by the speaker's slot), hums, walla beds (`walla:<bed>` on/to/off);
    MX  cues (`cue:<id>` start + mute/duck/to/bend/stop, global `tapestop`), rendered by
        music.automation over each cue's continuous render;
    FX  sfx / foley / ambience (`FX_*`, `FOL_*`, `AMB_*` from the SFX library, peak -1 dBFS
        files at the event's gain_db; FOL_crayon_scratch follow=<stroke> and FOL_paw_patter
        from=crowd:<ids> are synthesised from the picture's own data in timeline.json).

Ducking: MX ducks 7 dB and FX/walla 3 dB under every dialogue line (40 ms attack, 350 ms
release), sidechained from the lines' spans, so the speech sits on top of the score.

Outputs (build/<ep>/audio/, not git):
    mix_web.wav        the stereo web master, 48 kHz / 24-bit, -16 LUFS, <= -1 dBTP (master.py)
    stem_DX/MX/FX.flac the buses at the master's gain, before its limiter
    mix_sources.json   every sample source in the mix (AUD-LIC keys; release.credits reads it)
    mix.json           QA, loudness, P4, licences, credit lines, hashes
and the audio side of the captions merged into build/<ep>/captions.json (the file
captions.py reads first): `[music]` while a cue sounds, each SFX's library caption,
the walla beds' `cc` (kind "sdh", "src": "mix"; re-running replaces them).
"""
from __future__ import annotations

import hashlib
import json
import os
import re
import tomllib
from pathlib import Path

import numpy as np
import soundfile as sf

from . import paths
from .music import audiopaths as AP
from .music import automation as A
from .music import barmap as BM
from .music import cuelib

SR, SPF, FPS = 48000, 1600, 30
BUSES = ("DX", "MX", "FX")
DUCK = {"MX": -7.0, "FX": -3.0, "WALLA": -3.0}
DUCK_ATT, DUCK_REL = 0.040, 0.350
MUSIC_CC_GAP = 2 * FPS          # [music] cues closer than this merge into one


class MixError(Exception):
    pass


def db(g):
    return 10.0 ** (float(g) / 20.0)


def pan2(x, p):
    """Mono or stereo -> stereo with a constant-power pan p in -1..1."""
    x = np.asarray(x, dtype=np.float64)
    th = (np.clip(float(p or 0.0), -1, 1) + 1) * np.pi / 4
    if x.ndim == 1:
        return np.stack([x * np.cos(th), x * np.sin(th)], 1) * np.sqrt(2)
    if p:
        return x * np.array([np.cos(th), np.sin(th)]) * np.sqrt(2)
    return x


def add_at(buf, x, at):
    """buf[at:at+len(x)] += x, clipped to the buffer."""
    a, b = max(0, at), min(len(buf), at + len(x))
    if b > a:
        buf[a:b] += x[a - at: b - at]


# ---- inputs ------------------------------------------------------------------------
def load_json(p):
    return json.loads(Path(p).read_text(encoding="utf-8"))


def load_locks(ep, extra=()):
    """Every voice.lock row the mix may need: the episode's own plus --lock files."""
    from .voice import env as venv
    rows = {}
    cands = [Path(x) for x in extra] + [Path(venv.ep_dir(ep)) / "voice.lock"]   # --lock wins
    for p in cands:
        if p.exists():
            for k, v in load_json(p).get("lines", {}).items():
                rows.setdefault(k, v)
    return rows


class Timeline:
    def __init__(self, tl):
        self.tl = tl
        self.shots = [s for s in tl.get("shots", [])]

    def shot_at(self, f):
        for s in self.shots:
            if s["start"] <= f < s["end"]:
                return s
        return None

    def shot(self, sid):
        return next((s for s in self.shots if s["id"] == sid), None)

    def end_frame(self, upto=None):
        done = [s for s in self.shots if s.get("compiled", True)]
        if upto:
            s = self.shot(upto)
            if s is None:
                raise MixError("no shot %s in the timeline" % upto)
            return s["end"]
        return max(s["end"] for s in done) if done else 0


# ---- per-kind renderers -------------------------------------------------------------
class Mixer:
    def __init__(self, ep, events, timeline, n, locks):
        self.ep, self.events, self.tl, self.n, self.locks = ep, events, Timeline(timeline), n, locks
        self.bus = {b: np.zeros((n, 2)) for b in BUSES}
        self.walla = np.zeros((n, 2))
        self.sources, self.captions, self.warnings = set(), [], []
        self.speech = []             # [(smp0, smp1)] dialogue spans (the duck sidechain)
        self.placed = []             # [(k, smp)] every event the mix placed (P4 / QA)
        self._sfx_lib = None
        self.spans = []              # [(smp0, smp1, id, file has its own edges)] (QA)
        self._edgy = {}

    # library -----------------------------------------------------------------------
    def sfx_lib(self):
        if self._sfx_lib is None:
            p = AP.LIB_SFX / "library.json"
            if not p.exists():
                raise MixError("%s missing: run `show sfx build`" % p)
            self._sfx_lib = load_json(p)
        return self._sfx_lib

    def sfx_audio(self, sid):
        e = self.sfx_lib().get(sid)
        if e is None:
            raise MixError("sound %s is not in the rendered SFX library" % sid)
        x, sr = sf.read(str(AP.LIB / e["file"]), dtype="float64")
        if sr != SR:
            raise MixError("%s is %d Hz" % (e["file"], sr))
        self.sources.update(e.get("sources", []))
        return x, e

    # events --------------------------------------------------------------------------
    def run(self):
        evs = sorted(self.events, key=lambda e: (e["smp"], e["k"]))
        cue_evs, walla_evs, amb = [], {}, {}
        for e in evs:
            if e["smp"] >= self.n:
                continue
            src = str(e.get("src", ""))
            kind = src.split(":", 1)[0] if ":" in src else src
            self.placed.append((e["k"], e["smp"]))
            if kind == "cue" or src == "tapestop":
                cue_evs.append(e)
            elif kind == "walla":
                walla_evs.setdefault(src, []).append(e)
            elif kind == "line":
                self.line(e)
            elif kind == "hum":
                self.warnings.append("k%d %s: hums play as their locked line takes; the hum op adds no audio"
                                     % (e["k"], src))
            elif src.startswith("AMB_"):
                amb.setdefault(src, []).append(e)
            else:
                self.sfx(e)
        for src, lst in amb.items():
            self.ambience(src, lst)
        for src, lst in walla_evs.items():
            self.walla_bed(src, lst)
        self.music(cue_evs)
        self.duck()

    def sfx(self, e):
        sid = e["src"]
        x, lib = None, self.sfx_lib().get(sid, {})
        src = str(lib.get("src", ""))
        f = e["smp"] // SPF
        if sid == "FOL_crayon_scratch" or src.startswith("foley:crayon"):
            x = self.crayon(e)
            self.sources.add("foley:crayon")
        elif sid == "FOL_paw_patter" or src.startswith("foley:paws"):
            x = self.paws(e)
            self.sources.add("foley:paws")
        if x is None:
            x, lib = self.sfx_audio(sid)
        y = _declick(pan2(x, e.get("pan", 0.0)) * db(e.get("gain_db", 0.0)))
        add_at(self.bus[e.get("bus", "FX")], y, e["smp"])
        if sid not in self._edgy:
            from . import master
            self._edgy[sid] = master.clicks(pan2(_peak(x), 0.0)) > 0
        self.spans.append((e["smp"], e["smp"] + len(y), sid, self._edgy[sid]))
        cc = lib.get("cc") or e.get("cc")
        if cc:
            self.caption(f, f + max(MUSIC_CC_GAP, int(np.ceil(len(x) / SPF))), cc, e["k"])

    def _shot_events(self, e, op):
        sid = str(e.get("follow", "")).split(":")[0] or (self.tl.shot_at(e["smp"] // SPF) or {}).get("id")
        s = self.tl.shot(sid) or {}
        return s, [x for x in s.get("events", []) if x.get("op") == op]

    def crayon(self, e):
        from .sfx import foley
        sid, _, stroke = str(e.get("follow", "")).partition(":")
        s, revs = self._shot_events(e, "reveal")
        f0 = e["smp"] // SPF
        n = 24
        for r in revs:
            if r.get("stroke") == stroke:
                n = max(2, int((r.get("end") or r["f"] + 24) - r["f"]))
        y = foley.crayon_span(n, seed=int(hashlib.sha1(str(e.get("follow", f0)).encode()).hexdigest()[:6], 16))
        return _peak(y)

    def paws(self, e):
        from .sfx import foley
        s = self.tl.shot_at(e["smp"] // SPF) or {}
        ids = str(e.get("from", "")).replace("crowd:", "").split(",")
        want = {i.split(".")[-1] for i in ids if i}
        n_w = sum(int(c.get("n", 6)) for c in s.get("crowds", []) if c.get("id") in want) or 12
        n_f = max(1, int(s.get("end", e["smp"] // SPF + 90) - e["smp"] // SPF))
        seed = int(hashlib.sha1(str(e.get("from")).encode()).hexdigest()[:6], 16)
        return _peak(foley.paws(foley.crowd_contacts(n_w, n_f, seed=seed), n_f, seed=seed))

    def ambience(self, src, lst):
        x, lib = self.sfx_audio(src)
        x = pan2(x, 0.0)
        on = None
        for e in lst + [{"smp": self.n, "op": "off", "k": -1}]:
            if e.get("op", "on") == "on" and on is None:
                on = e
            elif e.get("op") == "off" and on is not None:
                a, b = on["smp"], min(self.n, e["smp"])
                reps = int(np.ceil((b - a) / len(x))) + 1
                y = np.tile(x, (reps, 1))[: b - a] * db(on.get("gain_db", 0.0))
                fade = min(len(y) // 2, 4800)
                if fade:
                    y[:fade] *= np.linspace(0, 1, fade)[:, None]
                    y[-fade:] *= np.linspace(1, 0, fade)[:, None]
                add_at(self.bus[on.get("bus", "FX")], y, a)
                if lib.get("cc"):
                    self.caption(a // SPF, b // SPF, lib["cc"], on["k"])
                on = None

    def walla_bed(self, src, lst):
        from .voice import walla
        bed_id = src.split(":", 1)[1]
        on = [e for e in lst if e.get("op") == "on"]
        if not on:
            self.warnings.append("%s has no `on` event" % src)
            return
        a = on[0]["smp"]
        off = [e for e in lst if e.get("op") == "off" and e["smp"] > a]
        b = min([self.n] + [e["smp"] + max(A.DECLICK, int(e.get("over_smp", 0) or 4800)) for e in off])
        y = walla.bed(self.ep_for_lines(), bed_id, b - a).astype(np.float64)
        if y.ndim == 1:
            y = pan2(y, 0.0)
        ops = []
        for e in lst:
            if e.get("op") == "to":
                ops.append({"smp": e["smp"], "op": "to", "gain_db": e.get("gain_db", 0.0),
                            "over_smp": int(e.get("over_smp", A.DECLICK))})
            elif e.get("op") == "off":
                ops.append({"smp": e["smp"], "op": "stop", "over_smp": int(e.get("over_smp", 0) or 4800)})
        env = A.gain_envelope(len(y), ops, on[0].get("gain_db", 0.0), a)
        add_at(self.walla, y * env[:, None], a)
        self._walla_rows(bed_id)
        if on[0].get("cc"):
            self.caption(a // SPF, b // SPF, on[0]["cc"], on[0]["k"], from_event=True)

    def _walla_rows(self, bed_id):
        from .voice import shwio, env as venv
        _lines, beds = shwio.read_lines(os.path.join(venv.ep_dir(self.ep_for_lines()), "lines.shw"))
        for lid in getattr(beds.get(bed_id), "lines", []):
            r = self.locks.get(lid)
            if r:
                self.sources.add(_voice_src(r))

    def ep_for_lines(self):
        return self.ep if self.ep.startswith("ep") else "ep101"

    def line(self, e):
        from .voice import env as venv, takes
        lid = e["src"].split(":", 1)[1]
        if e.get("op") == "stop":
            return
        r = self.locks.get(lid)
        if not r or not r.get("take"):
            self.warnings.append("k%d line %s is not locked: silent in the mix" % (e["k"], lid))
            return
        if r.get("kind") == "pending":
            self.warnings.append("k%d line %s is locked as pending (no audio yet)" % (e["k"], lid))
            return
        x = takes.load(venv.unrel(r["take"])).astype(np.float64)
        at = e["smp"] - int(r.get("onset", 1440))
        p = self.speaker_pan(e)
        add_at(self.bus["DX"], _declick(pan2(x, p) * db(e.get("gain_db", 0.0))), at)
        self.sources.add(_voice_src(r))
        on = e["smp"]
        self.speech.append((on, on + int(r.get("dur") or r.get("len") or e.get("len_smp", 0))))

    def speaker_pan(self, e):
        s = self.tl.shot_at(e["smp"] // SPF) or {}
        cast = s.get("cast", [])
        who = e.get("speaker")
        me = [c for c in cast if c.get("who") == who]
        if len(cast) < 2 or not me:
            return 0.0
        slot = me[0].get("slot", 0)
        return float(np.interp(slot, [0, len(cast) - 1], [-0.35, 0.35]))

    # music -----------------------------------------------------------------------------
    def music(self, evs):
        if not evs:
            return
        metas, inst, live = {}, {}, {}
        out = []
        for e in evs:
            if e["src"] == "tapestop":
                for name, it in inst.items():
                    if it["end"] > e["smp"] and not it.get("stopped"):
                        it["ops"].append({"smp": e["smp"], "op": "tapestop", "over_smp": int(e["over_smp"])})
                continue
            cue = e["src"][4:]
            if cue not in metas:
                try:
                    metas[cue] = cuelib.load_meta(cue)
                except FileNotFoundError:
                    cuelib.build(cuelib.load_defs()[cue])
                    metas[cue] = cuelib.load_meta(cue)
                self.sources.update(metas[cue].get("sources", []))
            if "op" not in e:
                live[cue] = live.get(cue, 0) + 1
                name = "%s#%d" % (cue, live[cue])
                bm = BM.BarMap.from_json(metas[cue]["barmap"])
                off = bm.at(e.get("from", "bar:1.1"))
                inst[name] = {"start": e, "ops": [], "cue": cue, "off": off,
                              "end": e["smp"] + metas[cue]["length_smp"] - off}
                continue
            name = "%s#%d" % (cue, live.get(cue, 0))
            if name not in inst:
                self.warnings.append("k%d %s %s before any start" % (e["k"], e["src"], e["op"]))
                continue
            op = dict(e)
            if op["op"] == "bend" and "bend" in op:
                a, _, b = str(op["bend"]).partition("->")
                op["from_st"], op["to_st"] = float(a), float(b)
            if op["op"] == "stop":
                inst[name]["stopped"] = True
            inst[name]["ops"].append(op)
        rows = []
        for name, it in inst.items():
            st, cue = it["start"], it["cue"]
            ev = dict(st, inst=name)
            rows.append(ev)
            rows += [dict(o, inst=name, src=st["src"]) for o in it["ops"]]
        self.bus["MX"] += _render_mx(rows, metas, self.n)
        # [music] captions: one per stretch of sounding score
        spans = []
        for name, it in inst.items():
            a = it["start"]["smp"]
            stops = [o["smp"] + int(o.get("over_smp", 0)) for o in it["ops"] if o["op"] in ("stop", "tapestop")]
            b = min([it["end"], self.n] + stops)
            spans.append([a // SPF, b // SPF, it["start"]["k"]])
        spans.sort()
        merged = []
        for a, b, k in spans:
            if merged and a <= merged[-1][1] + MUSIC_CC_GAP:
                merged[-1][1] = max(merged[-1][1], b)
            else:
                merged.append([a, b, k])
        for a, b, k in merged:
            self.caption(a, max(a + MUSIC_CC_GAP, b), "[music]", k)

    # ducking ---------------------------------------------------------------------------
    def duck_gain(self, depth_db):
        g = np.zeros(self.n)
        for a, b in self.speech:
            g[max(0, a): max(0, min(self.n, b))] = 1.0
        if not g.any():
            return np.ones(self.n)
        # attack / release as one-pole smoothing, run on a 1 ms grid then held per sample
        step = SR // 1000
        m = g[: len(g) // step * step].reshape(-1, step).max(axis=1) if len(g) >= step else g
        ka, kr = 1 - np.exp(-1.0 / (DUCK_ATT * 1000)), 1 - np.exp(-1.0 / (DUCK_REL * 1000))
        y, v = np.empty(len(m)), 0.0
        look = int(DUCK_ATT * 1000)          # start the duck one attack early (look-ahead)
        mm = np.concatenate([m[look:], np.zeros(look)])
        for i, t in enumerate(np.maximum(m, mm)):
            v += (ka if t > v else kr) * (t - v)
            y[i] = v
        y = np.repeat(y, step)
        y = np.concatenate([y, np.full(self.n - len(y), y[-1] if len(y) else 0.0)])[: self.n]
        return 10 ** (depth_db * y / 20.0)

    def duck(self):
        for b in ("MX", "FX"):
            self.bus[b] *= self.duck_gain(DUCK[b])[:, None]
        self.bus["DX"] += self.walla * self.duck_gain(DUCK["WALLA"])[:, None]

    # captions --------------------------------------------------------------------------
    def caption(self, f0, f1, text, k, from_event=False):
        t = str(text).strip()
        if not t:
            return
        if not t.startswith("["):
            t = "[%s]" % t.strip("[]")
        self.captions.append({"in": int(f0), "out": int(max(f1, f0 + 20)), "text": t, "kind": "sdh",
                              "k": k, "src": "mix", "from_event": from_event})


def _declick(y, n=A.DECLICK):
    """A 2 ms fade on the last samples, so a one-shot that ends mid-wave does not click."""
    y = np.array(y, dtype=np.float64)
    if len(y) > 2 * n:
        y[-n:] *= np.linspace(1.0, 0.0, n)[:, None] if y.ndim == 2 else np.linspace(1.0, 0.0, n)
    return y


def _peak(y, peak_db=-1.0):
    m = float(np.max(np.abs(y))) if len(y) else 0.0
    return y * (db(peak_db) / m) if m > 0 else y


def _voice_src(r):
    s = str(r.get("src", ""))
    if s.startswith("tts:kokoro"):
        return "tts:kokoro"
    if s.startswith("rec:") or r.get("kind") == "rec":
        return "rec:" + s[4:] if s.startswith("rec:") else "rec:creator"
    if s.startswith("lib:") or r.get("kind") == "lib":
        return "lib:" + s.split(":", 1)[-1]
    return s or "tts:kokoro"


def _render_mx(rows, metas, n):
    """music.spotting.render_mx over a mix of n samples (same automation code as WP10)."""
    from .music import spotting
    defs = None
    if any(r.get("op") == "bend" for r in rows):
        defs = cuelib.load_defs()
    return spotting.render_mx(rows, metas, n, defs).astype(np.float64)


# ---- AUD-LIC -------------------------------------------------------------------------
def licences(sources):
    """{source: (row, credit) | None}; None = no row or `in_mix = false` (AUD-LIC fails)."""
    return cuelib.licence_rows(sorted(sources), AP.LIB_SRC / "licences" / "licences.toml")


def licence_files_ok():
    """Every `file =` named in licences.toml exists in show/lib/licences/."""
    d = AP.LIB_SRC / "licences"
    rows = tomllib.loads((d / "licences.toml").read_text(encoding="utf-8"))
    return [r["file"] for r in rows.values() if isinstance(r, dict) and r.get("file")
            and not (d / r["file"]).exists()]


# ---- P4 cross-check ------------------------------------------------------------------
def read_plog(path):
    """plog.tsv -> {ep_frame: [k, ...]} (capture.py's PLOG_HEADER)."""
    rows = Path(path).read_text(encoding="utf-8").splitlines()
    head = rows[0].split("\t")
    i_f, i_p4 = head.index("ep_frame"), head.index("p4")
    out = {}
    for r in rows[1:]:
        c = r.split("\t")
        if len(c) <= i_p4:
            continue
        ks = [int(x) for x in c[i_p4].split(",") if x.strip()]
        out[int(c[i_f])] = ks
    return out


def p4_check(events, plog, f_range=None):
    """AT11.2 / AUD-P4: every event k fired on its scheduled frame and nowhere else.
    Returns {"ok", "checked", "missing", "late", "extra", "frames"}."""
    fired = {}
    for f, ks in plog.items():
        for k in ks:
            fired.setdefault(k, []).append(f)
    frames = set(plog)
    lo, hi = (min(frames), max(frames) + 1) if frames else (0, 0)
    if f_range:
        lo, hi = f_range
    missing, late, extra, checked = [], [], [], 0
    want = {}
    for e in events:
        f = int(e["smp"]) // SPF
        fr = int(round(int(e["smp"]) / SPF))
        if not (lo <= f < hi):
            continue
        checked += 1
        want[e["k"]] = (f, fr)
        got = fired.get(e["k"], [])
        if not got:
            missing.append({"k": e["k"], "frame": f, "src": e.get("src")})
        elif got[0] not in (f, fr) or len(got) > 1:
            late.append({"k": e["k"], "want": f, "got": got, "src": e.get("src")})
    for k, fs in fired.items():
        if k not in want:
            extra.append({"k": k, "frames": fs})
    return {"ok": not (missing or late or extra), "checked": checked, "missing": missing,
            "late": late, "extra": extra, "frames": [lo, hi]}


def find_plog(ep):
    p = paths.episode_build(ep) / "render" / "plog.tsv"
    return p if p.exists() else None


# ---- the whole thing -------------------------------------------------------------------
def pcm_hash(y):
    """sha256 of the 24-bit PCM the WAV holds (AT11.3)."""
    if isinstance(y, (str, Path)):
        q, _ = sf.read(str(y), dtype="int32")          # 24-bit PCM, left-justified in int32
        return hashlib.sha256((q >> 8).astype("<i4").tobytes()).hexdigest()
    q = np.clip(np.round(np.asarray(y) * 8388608.0), -8388608, 8388607).astype("<i4")
    return hashlib.sha256(q.tobytes()).hexdigest()


def mix(ep="ep101", build=None, upto=None, end_frame=None, locks=(), out=None, p4=None,
        merge_captions=True, verbose=True):
    from . import master
    bdir = Path(build) if build else paths.episode_build(ep)
    ae = load_json(bdir / "audio_events.json")
    events = ae["events"] if isinstance(ae, dict) else ae
    tl = load_json(bdir / "timeline.json")
    T = Timeline(tl)
    f_end = end_frame or T.end_frame(upto)
    n = f_end * SPF
    if n <= 0:
        raise MixError("nothing to mix: the timeline has no compiled shots")
    m = Mixer(tl.get("episode", ep) if ep.startswith("ep") else ep, events, tl, n,
              load_locks(ep if ep.startswith("ep") else "ep101", locks))
    m.ep = ep if ep.startswith("ep") else "ep101"
    m.run()
    stems = {b: m.bus[b] for b in BUSES}
    y, rep, gain = master.web(sum(stems.values()))
    odir = Path(out) if out else bdir / "audio"
    odir.mkdir(parents=True, exist_ok=True)
    wav = odir / "mix_web.wav"
    sf.write(str(wav), y, SR, subtype="PCM_24")
    for b, x in stems.items():
        sf.write(str(odir / ("stem_%s.flac" % b)), np.clip(x * db(gain), -1, 1), SR, subtype="PCM_24")
    lic = licences(m.sources)
    bad = sorted(k for k, v in lic.items() if v is None)
    credits = sorted({v[1] for v in lic.values() if v and v[1]})
    (odir / "mix_sources.json").write_text(json.dumps(sorted(m.sources), indent=1) + "\n")
    qa = master.qa(y, stems, gain, tl, n, m.spans)
    res = {"format": "show-mix/1", "episode": ep, "frames": [0, f_end], "samples": n,
           "events_in_mix": len(m.placed), "events_total": len(events),
           "master": rep, "qa": qa, "sha256_pcm": pcm_hash(wav), "wav": str(wav),
           "sources": sorted(m.sources), "aud_lic": {"ok": not bad and not licence_files_ok(),
                                                     "unlicensed": bad,
                                                     "missing_files": licence_files_ok()},
           "credits": credits, "captions": merge_same(m.captions), "warnings": m.warnings}
    plog = Path(p4) if p4 and p4 is not True else (find_plog(ep) if p4 else None)
    if p4:
        if plog is None or not plog.exists():
            res["p4"] = {"ok": False, "error": "no plog.tsv (run `show render`)"}
        else:
            res["p4"] = p4_check(events, read_plog(plog), (0, f_end))
            res["p4"]["plog"] = str(plog)
    (odir / "mix.json").write_text(json.dumps(res, indent=1) + "\n", encoding="utf-8")
    if merge_captions:
        merge_into_captions(bdir / "captions.json", m.captions, ep)
    if verbose:
        print(report(res))
    return res


def merge_into_captions(path, cues, ep):
    """Add the mix's SDH cues to captions.json (show-captions/1), replacing earlier mix
    cues; a cue the compiler already wrote (same text starting on the same frame) is kept once."""
    path = Path(path)
    data = load_json(path) if path.exists() else {"format": "show-captions/1", "episode": ep, "cues": []}
    have = [c for c in data.get("cues", []) if c.get("src") != "mix"]
    cues = merge_same(cues)
    seen = {(c.get("in"), c.get("text")) for c in have}
    add = [{k: v for k, v in c.items() if k != "from_event"} for c in cues
           if (c["in"], c["text"]) not in seen]
    data["cues"] = sorted(have + add, key=lambda c: (int(c.get("in", 0)), c.get("kind") != "sdh"))
    path.write_text(json.dumps(data, indent=1) + "\n", encoding="utf-8")
    return len(add)


def merge_same(cues):
    """One SDH cue for a run of the same caption (two crayon strokes 1 s apart read as one)."""
    out = []
    for c in sorted(cues, key=lambda c: (c["text"], c["in"])):
        if out and out[-1]["text"] == c["text"] and c["in"] <= out[-1]["out"]:
            out[-1]["out"] = max(out[-1]["out"], c["out"])
        else:
            out.append(dict(c))
    return sorted(out, key=lambda c: c["in"])


def report(res):
    mr, qa = res["master"], res["qa"]
    rows = ["show mix %s: %d f (%d smp), %d/%d events placed" % (
        res["episode"], res["frames"][1], res["samples"], res["events_in_mix"], res["events_total"]),
        "  web master  %.2f LUFS  %.2f dBTP  gain %+.2f dB  %s" % (
            mr["lufs"], mr["tp"], mr["gain_db"], "PASS" if mr["ok"] else "FAIL"),
        "  stems       " + "  ".join("%s %.1f LUFS" % (b, qa["stems"][b]["lufs"]) for b in BUSES),
        "  QA          clipped %d  clicks %d (+%d inside sounds that carry their own edges)  unscored %.1f %%" % (
            qa["clipped"], qa["clicks"], qa.get("source_clicks", 0), qa["unscored_pct"]),
        "  AUD-LIC     %s  (%d sources; credits: %s)" % (
            "ok" if res["aud_lic"]["ok"] else "FAIL %s %s" % (res["aud_lic"]["unlicensed"],
                                                              res["aud_lic"]["missing_files"]),
            len(res["sources"]), "; ".join(res["credits"]) or "none"),
        "  captions    %d SDH (%s)" % (len(res["captions"]), ", ".join(sorted({c["text"] for c in res["captions"]}))),
        "  sha256      %s" % res["sha256_pcm"]]
    if "p4" in res:
        p = res["p4"]
        rows.append("  P4          %s" % ("ok: %d events on their frames" % p["checked"] if p.get("ok") else
                                          "FAIL %s" % {k: p.get(k) for k in ("error", "missing", "late", "extra")
                                                       if p.get(k)}))
    rows += ["  warning: " + w for w in res["warnings"]]
    rows.append("  -> " + res["wav"])
    return "\n".join(rows)
