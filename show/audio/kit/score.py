"""Score format: plain Python data (tracks of notes, times in beats).

    s = Score(bpm=150, key="Ab", mode="major", bars=16)
    lead = s.track("lead", inst="supersaw_lead", role="lead", program=81)
    lead.seq("C5:1 C5:.5 Eb5:.5 C5:1 Ab5:1", start=16)       # beats
    lead.add(20, 1.0, "Eb5", v=0.9, art="accent", bend=[(0.0, -1.0), (0.12, 0.0)])
    kick = s.track("kick", inst="kick", role="drums", drum=True)
    kick.pattern("x...x...x...x...", note=36, start=0, bars=4)
    s.humanize(seed=7); s.swing(0.12, grid=0.25)
    s.export_midi("out/")                                    # per-track + combined

Articulations (`Note.art`, space-separated flags): stac, legato, accent, ghost,
mute (palm mute), slide (glide into the note), bend, open, roll, flam, choke.
`Note.bend`: [(beat offset from note start, semitones), ...] pitch-bend curve.
`Note.glide`: portamento time in beats from the previous pitch (mono voices).
"""
from __future__ import annotations

import copy
import dataclasses as dc
import os
import re
from typing import Any

import mido
import numpy as np

from .core import SR, note as note_num, rng


@dc.dataclass
class Note:
    t: float                 # start (beats from song start)
    d: float                 # duration (beats)
    p: int                   # MIDI pitch (drums: GM drum map key)
    v: float = 0.8           # velocity 0..1
    art: str = ""            # articulation flags
    glide: float = 0.0       # portamento from previous note (beats)
    bend: list | None = None  # [(beat_offset, semitones)]
    meta: dict = dc.field(default_factory=dict)

    def has(self, flag: str) -> bool:
        return flag in self.art.split()

    @property
    def end(self) -> float:
        return self.t + self.d


@dc.dataclass
class Track:
    name: str
    inst: Any = None          # instrument key (str) or Instrument object
    role: str = "music"       # drums | bass | harmony | lead | fx | vox | music
    drum: bool = False
    program: int = 0          # GM program for MIDI export
    channel: int | None = None
    notes: list = dc.field(default_factory=list)
    params: dict = dc.field(default_factory=dict)   # instrument params
    chromatic_ok: bool = False  # tuning verifier: allow out-of-scale notes
    mute: bool = False

    # ---- authoring helpers -------------------------------------------------
    def add(self, t, d, p, v=0.8, art="", glide=0.0, bend=None, **meta):
        p = note_num(p) if isinstance(p, str) else int(p)
        n = Note(float(t), float(d), p, float(v), art, float(glide), bend, dict(meta))
        self.notes.append(n)
        return n

    def chord(self, t, d, pitches, v=0.75, art="", strum=0.0, **meta):
        """Add simultaneous notes; strum (beats) spreads them low->high."""
        ps = [note_num(p) if isinstance(p, str) else int(p) for p in pitches]
        for i, p in enumerate(sorted(ps)):
            self.add(t + i * strum, d - i * strum, p, v, art, **meta)

    def seq(self, text: str, start: float = 0.0, v: float = 0.8, transpose: int = 0,
            gate: float = 1.0, art: str = ""):
        """Parse 'C5:1 Eb5:.5 r:.5 [C4,Eb4,G4]:2 G4:1!' (durations in beats).

        '!' suffix = accent, '~' suffix = slide/glide in, '.' prefix flag = stac,
        'r' = rest, [a,b,c] = chord. Returns the beat after the last event.
        """
        t = float(start)
        for tok in text.split():
            name, _, dur = tok.partition(":")
            flags = art
            while dur and dur[-1] in "!~^":
                flags += {"!": " accent", "~": " slide", "^": " stac"}[dur[-1]]
                dur = dur[:-1]
            d = float(dur) if dur else 1.0
            if name.lower() == "r":
                t += d
                continue
            if name.startswith("["):
                ps = [note_num(x) + transpose for x in name.strip("[]").split(",")]
                for p in ps:
                    self.add(t, d * gate, p, v * (1.15 if "accent" in flags else 1.0),
                             flags.strip())
            else:
                g = 0.06 if "slide" in flags else 0.0
                self.add(t, d * gate, note_num(name) + transpose,
                         min(1.0, v * (1.15 if "accent" in flags else 1.0)), flags.strip(),
                         glide=g)
            t += d
        return t

    def pattern(self, pat: str, note=36, start=0.0, step=0.25, bars=1, v=0.85,
                beats_per_bar=4, accent_v=1.0, ghost_v=0.45, art=""):
        """Drum-grid pattern. 'x' hit, 'X' accent, 'g' ghost, 'o' open, 'f' flam, '.' rest.

        The pattern repeats to fill `bars` bars.
        """
        steps = [c for c in pat if c not in " |"]
        n_steps = int(round(bars * beats_per_bar / step))
        p = note_num(note) if isinstance(note, str) else note
        for i in range(n_steps):
            c = steps[i % len(steps)]
            if c == ".":
                continue
            vel = {"x": v, "X": accent_v, "g": ghost_v, "o": v, "f": v}.get(c, v)
            a = art + (" open" if c == "o" else "") + (" flam" if c == "f" else "") + \
                (" ghost" if c == "g" else "") + (" accent" if c == "X" else "")
            self.add(start + i * step, step, p, vel, a.strip())

    def transpose(self, semis: int):
        for n in self.notes:
            n.p += semis
        return self

    def shift(self, beats: float):
        for n in self.notes:
            n.t += beats
        return self

    def copy_range(self, t0, t1, dest, reps=1):
        """Copy notes in [t0, t1) to start at `dest` (repeated `reps` times)."""
        src = [n for n in self.notes if t0 <= n.t < t1]
        L = t1 - t0
        for r in range(reps):
            for n in src:
                m = copy.deepcopy(n)
                m.t = dest + r * L + (n.t - t0)
                self.notes.append(m)

    def sorted(self):
        return sorted(self.notes, key=lambda n: (n.t, n.p))

    @property
    def end(self) -> float:
        return max((n.end for n in self.notes), default=0.0)


@dc.dataclass
class Section:
    name: str
    bar: int        # first bar (0-based)
    bars: int


class Score:
    def __init__(self, bpm=150.0, key="Ab", mode="major", bars=16, beats_per_bar=4,
                 title="untitled", tail_beats=8.0):
        self.bpm = float(bpm)
        self.key = key
        self.mode = mode
        self.bars = bars
        self.beats_per_bar = beats_per_bar
        self.title = title
        self.tail_beats = tail_beats
        self.tracks: dict[str, Track] = {}
        self.sections: list[Section] = []
        self.chords: list[tuple[float, float, str]] = []  # (beat, dur, symbol)

    # time
    @property
    def spb(self) -> float:
        return 60.0 / self.bpm

    def b2s(self, beats) -> float:
        return np.asarray(beats, dtype=float) * self.spb

    def b2n(self, beats, sr=SR) -> int:
        return int(round(float(beats) * self.spb * sr))

    def bar(self, b: float) -> float:
        return b * self.beats_per_bar

    @property
    def length_beats(self) -> float:
        return self.bars * self.beats_per_bar

    def n_samples(self, sr=SR, tail=True) -> int:
        return self.b2n(self.length_beats + (self.tail_beats if tail else 0), sr)

    # authoring
    def track(self, name, **kw) -> Track:
        if name not in self.tracks:
            self.tracks[name] = Track(name, **kw)
        return self.tracks[name]

    def section(self, name, bar, bars):
        self.sections.append(Section(name, bar, bars))

    def chord_at(self, beat, dur, symbol):
        self.chords.append((float(beat), float(dur), symbol))

    def section_at(self, beat) -> str:
        for s in self.sections:
            if self.bar(s.bar) <= beat < self.bar(s.bar + s.bars):
                return s.name
        return ""

    # performance
    def humanize(self, timing_ms=6.0, vel=0.06, seed=7, tracks=None, keep_downbeats=True):
        """Gaussian timing (ms) and velocity jitter; drums get tighter timing."""
        for name, tr in self.tracks.items():
            if tracks and name not in tracks:
                continue
            r = rng("humanize", name, seed=seed)
            tms = timing_ms * (0.6 if tr.drum else 1.0)
            for n in tr.notes:
                on_grid = abs(n.t - round(n.t)) < 1e-6
                jit = r.normal(0, tms) / 1000.0 / self.spb
                if keep_downbeats and on_grid and abs(n.t % self.beats_per_bar) < 1e-6:
                    jit *= 0.35
                n.t = max(0.0, n.t + jit)
                n.v = float(np.clip(n.v * (1.0 + r.normal(0, vel)), 0.05, 1.0))
        return self

    def swing(self, amount=0.1, grid=0.25, tracks=None):
        """Delay every off-grid step (odd 16ths when grid=.25) by amount*grid beats."""
        for name, tr in self.tracks.items():
            if tracks and name not in tracks:
                continue
            for n in tr.notes:
                k = n.t / grid
                if abs(k - round(k)) < 0.02 and int(round(k)) % 2 == 1:
                    n.t += amount * grid
        return self

    # MIDI
    def export_midi(self, out_dir, ppq=480, combined_name=None) -> list[str]:
        os.makedirs(out_dir, exist_ok=True)
        paths = []
        tracks = [t for t in self.tracks.values() if t.notes]
        for i, tr in enumerate(tracks):
            mf = mido.MidiFile(ticks_per_beat=ppq, type=1)
            mf.tracks.append(self._meta_track(ppq))
            mf.tracks.append(self._midi_track(tr, ppq, i))
            p = os.path.join(out_dir, f"{_safe(tr.name)}.mid")
            mf.save(p)
            paths.append(p)
        mf = mido.MidiFile(ticks_per_beat=ppq, type=1)
        mf.tracks.append(self._meta_track(ppq))
        for i, tr in enumerate(tracks):
            mf.tracks.append(self._midi_track(tr, ppq, i))
        p = os.path.join(out_dir, (combined_name or _safe(self.title)) + "_full.mid")
        mf.save(p)
        paths.append(p)
        return paths

    def _meta_track(self, ppq):
        t = mido.MidiTrack()
        t.append(mido.MetaMessage("track_name", name=self.title, time=0))
        t.append(mido.MetaMessage("set_tempo", tempo=mido.bpm2tempo(self.bpm), time=0))
        t.append(mido.MetaMessage("time_signature", numerator=self.beats_per_bar,
                                  denominator=4, time=0))
        try:
            ks = self.key + ("m" if self.mode == "minor" else "")
            t.append(mido.MetaMessage("key_signature", key=ks, time=0))
        except Exception:
            pass
        events = [(self.bar(s.bar), s.name) for s in self.sections]
        last = 0
        for beat, name in sorted(events):
            tick = int(round(beat * ppq))
            t.append(mido.MetaMessage("marker", text=name, time=tick - last))
            last = tick
        return t

    def _midi_track(self, tr: Track, ppq, idx):
        ch = 9 if tr.drum else (tr.channel if tr.channel is not None else
                                [c for c in range(16) if c != 9][idx % 15])
        ev = []
        for n in tr.notes:
            on = int(round(n.t * ppq))
            off = max(on + 1, int(round(n.end * ppq)))
            vel = int(np.clip(round(n.v * 127), 1, 127))
            ev.append((on, 2, mido.Message("note_on", channel=ch, note=int(n.p), velocity=vel)))
            ev.append((off, 0, mido.Message("note_off", channel=ch, note=int(n.p), velocity=0)))
            if n.bend:
                for (bo, semis) in n.bend:
                    tick = int(round((n.t + bo) * ppq))
                    val = int(np.clip(semis / 2.0 * 8191, -8192, 8191))  # +-2 st range
                    ev.append((tick, 1, mido.Message("pitchwheel", channel=ch, pitch=val)))
                ev.append((off, 1, mido.Message("pitchwheel", channel=ch, pitch=0)))
        ev.sort(key=lambda e: (e[0], e[1]))
        t = mido.MidiTrack()
        t.append(mido.MetaMessage("track_name", name=tr.name, time=0))
        if not tr.drum:
            t.append(mido.Message("program_change", channel=ch, program=int(tr.program) % 128,
                                  time=0))
        last = 0
        for tick, _, msg in ev:
            msg.time = tick - last
            t.append(msg)
            last = tick
        return t


def _safe(s):
    return re.sub(r"[^A-Za-z0-9_.-]+", "_", s)


def import_midi(path, track_name=None, inst=None, role="music", tempo_override=None):
    """Read a MIDI file into a Track (beats from the file's own PPQ)."""
    mf = mido.MidiFile(path)
    ppq = mf.ticks_per_beat
    tr = Track(track_name or os.path.splitext(os.path.basename(path))[0], inst=inst, role=role)
    for mt in mf.tracks:
        t = 0
        on = {}
        for msg in mt:
            t += msg.time
            if msg.type == "note_on" and msg.velocity > 0:
                on.setdefault(msg.note, []).append((t, msg.velocity))
            elif msg.type in ("note_off", "note_on"):
                if on.get(msg.note):
                    t0, v = on[msg.note].pop(0)
                    tr.add(t0 / ppq, max(1, t - t0) / ppq, msg.note, v / 127.0)
    tr.notes.sort(key=lambda n: (n.t, n.p))
    return tr
