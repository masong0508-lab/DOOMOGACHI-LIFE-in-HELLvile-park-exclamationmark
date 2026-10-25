"""Helpers for writing the show's score cues with the kit (WP10, sec.10.4).

A score cue (show/lib/music/<family>.py) builds a `Cue`: a kit Score plus a
small mix spec. `render(cue)` turns it into stereo 48 kHz audio the same way
for every cue, so the cue files stay about music:

    c = Cue("its_fine", bpm=140, key="C", bars=2)
    bell = c.track("bell", "celesta")
    place(bell, [(0, 1, P("C6"))], at=0)
    c.mix["bell"] = dict(gain=-3, sends={"room": -14})
    y, info = render(c)

The game's musical language comes from the game itself: leitmotifs are read
from core/music/common/motifs.dms through mml_legacy.motif() (tools/mmlc.py),
and moved between keys and modes diatonically (mode_map), the way the game's
runtime quotes them (music SPEC sec.8.4).
"""
from __future__ import annotations

import dataclasses as dc

import numpy as np

from . import mml_legacy
from .kitenv import SPF, SR, kit
from kit import dsp, theory  # noqa: E402

_PC = theory.PC


def P(x) -> int:
    """'E5' / 'Bb3' / 64 -> MIDI (C4 = 60)."""
    return kit.note(x) if isinstance(x, str) else int(x)


def scale(key: str, mode: str):
    m = {"major": "major", "minor": "minor", "harmonic_minor": "harmonic_minor",
         "mixolydian": "mixolydian", "dorian": "dorian"}[mode]
    return theory.SCALES[m]


def mode_map(midi: int, src_key="C", src_mode="major", dst_key="C", dst_mode="major") -> int:
    """Move a note between keys/modes by scale degree (chromatic notes keep their
    offset from the nearest lower degree)."""
    s_sc, d_sc = scale(src_key, src_mode), scale(dst_key, dst_mode)
    rel = midi - (_PC[src_key] + 60)
    octv, pc = divmod(rel, 12)
    deg = max(i for i, v in enumerate(s_sc) if v <= pc)
    off = pc - s_sc[deg]
    return 60 + _PC[dst_key] + 12 * octv + d_sc[deg] + off


def motif(name: str, key: str | None = None, mode: str | None = None, octave: int = 0,
          stretch: float = 1.0):
    """A game leitmotif -> [(t, d, midi)], moved to key/mode (diatonically).
    `fixed` motifs (WALL, RIFF) keep their own mode, as in the game."""
    notes, meta = mml_legacy.motif(name)
    sk, _, sm = meta.get("key", "C:major").partition(":")
    sm = sm or "major"
    dk = key or sk
    dm = sm if ("fixed" in meta or mode is None) else mode
    out = []
    for t, d, m in notes:
        mm = m if (dk == sk and dm == sm) else mode_map(m, sk, sm, dk, dm)
        # keep the motif's register: choose the octave nearest the source pitch
        while mm - m > 6:
            mm -= 12
        while m - mm > 6:
            mm += 12
        out.append((t * stretch, d * stretch, mm + 12 * octave))
    return out


def place(track, notes, at=0.0, v=0.8, gate=1.0, art="", transpose=0, vel_curve=None, **meta):
    """Add [(t, d, midi)] notes at beat `at`."""
    for i, (t, d, m) in enumerate(notes):
        vv = v if vel_curve is None else v * vel_curve(i, len(notes))
        track.add(at + t, d * gate, int(m) + transpose, min(1.0, vv), art, **meta)
    return track


def seq(track, text: str, at=0.0, v=0.8, gate=1.0, transpose=0, art=""):
    return track.seq(text, start=at, v=v, gate=gate, transpose=transpose, art=art)


def chord(track, at, dur, sym, center=64, n=4, v=0.6, strum=0.0, art=""):
    track.chord(at, dur, theory.voice(sym, center=center, n=n), v=v, strum=strum, art=art)


def chords(track, chart, beats_per_bar=4, start_bar=0, center=64, n=4, v=0.6, strum=0.0,
           rhythm=None, art=""):
    """chart: 'C | Am | F G | C' (one bar per |, chords split the bar evenly).
    rhythm: None (held) or a list of (beat_offset, dur) hits inside each chord."""
    for bi, bar in enumerate([b.strip() for b in chart.split("|")]):
        syms = bar.split()
        if not syms:
            continue
        span = beats_per_bar / len(syms)
        for si, sym in enumerate(syms):
            if sym in (".", "-"):
                continue
            t0 = (start_bar + bi) * beats_per_bar + si * span
            if rhythm is None:
                chord(track, t0, span, sym, center, n, v, strum, art)
            else:
                for off, d in rhythm:
                    if off < span:
                        chord(track, t0 + off, min(d, span - off), sym, center, n, v, strum, art)


def bassline(track, chart, beats_per_bar=4, start_bar=0, octave=2, pattern=((0, 1.0),),
             v=0.8, fifth=False, art=""):
    """Root (and optional fifth on the 2nd hit) per chord."""
    for bi, bar in enumerate([b.strip() for b in chart.split("|")]):
        syms = bar.split()
        if not syms:
            continue
        span = beats_per_bar / len(syms)
        for si, sym in enumerate(syms):
            if sym in (".", "-"):
                continue
            root = theory.bass_note(sym, octave)
            t0 = (start_bar + bi) * beats_per_bar + si * span
            for k, (off, d) in enumerate(pattern):
                if off < span:
                    p = root + (7 if fifth and k % 2 == 1 else 0)
                    track.add(t0 + off, min(d, span - off), p, v, art)


def drums(track, pattern: str, note, bars, start_bar=0, step=0.25, v=0.8, beats_per_bar=4, **kw):
    track.pattern(pattern, note, start=start_bar * beats_per_bar, step=step, bars=bars, v=v,
                  beats_per_bar=beats_per_bar, **kw)


# ---- the cue -------------------------------------------------------------------
@dc.dataclass
class Cue:
    name: str
    bpm: float = 140.0
    key: str = "C"
    mode: str = "major"
    bars: int = 4
    meter: int = 4
    tail_beats: float = 6.0
    desc: str = ""
    score: object = None
    mix: dict = dc.field(default_factory=dict)        # track -> strip
    verbs: dict = dc.field(default_factory=dict)      # return -> reverb params
    post: list = dc.field(default_factory=list)       # whole-mix filters: "radio", ...
    notes: dict = dc.field(default_factory=dict)      # named notes (bar.beat, pitch)
    markers: dict = dc.field(default_factory=dict)    # named times (bar.beat) for the sheet
    length_s: float | None = None                     # hard length (else bars + tail)
    fade_out_s: float = 0.0

    def __post_init__(self):
        if self.score is None:
            self.score = kit.Score(bpm=self.bpm, key=self.key, mode=self.mode, bars=self.bars,
                                   beats_per_bar=self.meter, title=self.name,
                                   tail_beats=self.tail_beats)
        if not self.verbs:
            self.verbs = {"room": dict(kind="room", rt60=1.1, hp=220, lp=9000)}

    def track(self, name, inst, role="music", drum=False, **params):
        return self.score.track(name, inst=inst, role=role, drum=drum, params=params)

    def b(self, bar, beat=1.0) -> float:
        """1-based bar.beat -> beats from the start."""
        return (bar - 1) * self.meter + (beat - 1)

    def section(self, name, bar, bars):
        self.score.section(name, bar - 1, bars)


# ---- whole-mix filters -----------------------------------------------------------
def _bp(y, lo, hi, order=2):
    y = dsp.butter(y, "high", lo, order, SR)
    return dsp.butter(y, "low", hi, order, SR)


def post_filter(y, kind: str):
    """Diegetic perspectives: radio (the bus), speaker (fair speakers), distant
    (heard across the park), tent (inside a tent), memory (sepia flashback)."""
    y = np.asarray(y, dtype=float)
    if kind == "radio":
        z = _bp(y, 420.0, 3600.0, 3)
        z = np.tanh(z * 2.2) / 2.2
        return z * 1.6
    if kind == "speaker":
        z = _bp(y, 180.0, 6500.0, 2)
        z = kit.eq(z, [("peak", 2400.0, 3.0, 1.2)])
        return np.tanh(z * 1.4) / 1.4
    if kind == "distant":
        z = dsp.butter(y, "low", 2600.0, 2, SR)
        z = dsp.butter(z, "high", 120.0, 1, SR)
        ir = kit.reverb.make_ir("hall", 2.2, 30, SR, 3)
        wet = kit.reverb.convolve(z, ir, SR)[: len(z)]
        return 0.55 * z + 0.6 * wet
    if kind == "tent":
        z = dsp.butter(y, "low", 3800.0, 2, SR)
        return z * 0.5
    if kind == "memory":
        z = _bp(y, 250.0, 5000.0, 2)
        return z
    raise ValueError(f"unknown post filter {kind}")


# ---- rendering -----------------------------------------------------------------
def render(cue: Cue, verbose=False):
    """Cue -> (stereo float64 (n, 2) at 48 kHz, info). Deterministic."""
    sc = cue.score
    stems = kit.render_score(sc, verbose=verbose)
    mx = kit.Mixer(sc)
    for rname, rp in cue.verbs.items():
        rp = dict(rp)
        if rp.pop("type", "reverb") == "delay":
            mx.delay(rname, **rp)
        else:
            mx.reverb(rname, **rp)
    for name, st in stems.items():
        strip = dict(cue.mix.get(name, {}))
        gain = strip.pop("gain", 0.0)
        strip.setdefault("sends", {"room": -16})
        mx.channel(name, st, gain_db=gain, **strip)
    y = mx.render()
    for k in cue.post:
        y = post_filter(y, k)
    n = int(round(cue.length_s * SR)) if cue.length_s else sc.n_samples()
    if len(y) < n:
        y = np.pad(y, ((0, n - len(y)), (0, 0)))
    y = y[:n]
    if cue.fade_out_s > 0:
        y = kit.core.fade(y, 0, int(cue.fade_out_s * SR))
    # 2 ms de-click at the very end only (the start must stay sample exact)
    y = kit.core.fade(y, 0, 96)
    info = dict(bpm=sc.bpm, meter=sc.beats_per_bar, bars=sc.bars, key=cue.key, mode=cue.mode,
                sections=[(s.name, s.bar + 1, s.bars) for s in sc.sections],
                notes=cue.notes, markers=cue.markers, desc=cue.desc,
                n_notes=sum(len(t.notes) for t in sc.tracks.values()))
    return y, info


def unity(y, target_lufs=-16.0, tp_max=-1.0):
    """The library's reference level ("unity"): integrated loudness at the web
    target (-16 LUFS) unless the true peak would pass -1 dBTP, then the peak
    sets it. Spot gains in cues.shw are relative to this."""
    y = np.asarray(y, dtype=float)
    try:
        L = kit.lufs(y)
    except Exception:
        L = float("-inf")
    tp = kit.true_peak(y)
    if not np.isfinite(L):
        g = tp_max - tp
    else:
        g = min(target_lufs - L, tp_max - tp)
    y2 = y * 10 ** (g / 20.0)
    return y2, dict(gain_db=round(g, 2), lufs=round(L + g, 2) if np.isfinite(L) else None,
                    tp=round(tp + g, 2))


def frames_per_bar(bpm, meter=4):
    return 30.0 * 60.0 * meter / bpm


__all__ = ["P", "mode_map", "motif", "place", "seq", "chord", "chords", "bassline", "drums",
           "Cue", "render", "unity", "post_filter", "frames_per_bar", "kit", "SR", "SPF", "theory"]
