"""Bar maps: where every bar and beat of a cue falls, in samples and frames
(SHOW_ENGINE_SPEC.md sec.5.3 `@bar:B.b`, sec.10.1, sec.10.4).

A cue's bar map is counted from the cue's own sample 0 (its bar 1, beat 1).
Bars land on whole samples; a picture event at `@bar:B.b` snaps to the nearest
frame of the exact sample, so the error is at most half a frame and never
accumulates (it is counted from the bar line, not added up beat by beat).

    150 bpm (the theme):  a beat is 12 f exactly, a bar 48 f = 76,800 smp
     96 bpm (MINGLE):     a beat is 18.75 f, a bar 75 f = 120,000 smp
    140 bpm (the HOOK):   a beat is 12.857 f, a bar 51.43 f

`import_midi(path)` reads a master's tempo map and section markers (the kit
writes them into every `*_full.mid` it exports), which is how the theme's bar
map comes in (`show music import-barmap <master>`).
"""
from __future__ import annotations

import dataclasses as dc
import json
import math
import re
from fractions import Fraction

SR = 48000
FPS = 30
SPF = SR // FPS


@dc.dataclass
class BarMap:
    bpm: float
    meter: int = 4
    bars: int = 0
    sections: list = dc.field(default_factory=list)   # [(name, bar 1-based, n_bars)]
    offset_smp: int = 0                                 # sample of bar 1 beat 1 in the file
    source: str = ""

    # exact arithmetic: samples per beat as a fraction (96 bpm -> 30000, 140 -> 144000/7)
    @property
    def spb_smp(self) -> Fraction:
        return Fraction(SR * 60) / Fraction(self.bpm).limit_denominator(1000)

    @property
    def frames_per_bar(self) -> float:
        return float(self.spb_smp * self.meter / SPF)

    def smp(self, bar: int, beat: float = 1.0) -> int:
        """1-based bar and beat (beat may be fractional) -> sample from the cue start."""
        beats = Fraction((bar - 1) * self.meter) + Fraction(beat - 1).limit_denominator(960)
        return self.offset_smp + int(round(beats * self.spb_smp))

    def frame(self, bar: int, beat: float = 1.0) -> int:
        """Nearest frame (from the cue start) of bar.beat, counted from the bar line."""
        bar_smp = self.smp(bar, 1.0)
        f_bar = bar_smp / SPF
        within = self.smp(bar, beat) - bar_smp
        return int(math.floor(f_bar + within / SPF + 0.5))

    def at(self, text: str) -> int:
        """'bar:17.1' / '17.3' / '17' -> sample."""
        s = text.split(":", 1)[-1]
        if s and not s[0].isdigit():
            return self.smp(self.section_bar(s), 1.0)
        b, _, bt = s.partition(".")
        return self.smp(int(b), float(bt) if bt else 1.0)

    def section_bar(self, name: str) -> int:
        """A section by name ('chorus', 'pre-chorus' = 'prechorus'), or '<section>_lastN' =
        the section's last N bars; returns the bar it starts on."""
        import re as _re
        key = name.lower()
        last = None
        m = _re.match(r"^(.*?)_last(\d+)$", key)
        if m:
            key, last = m.group(1), int(m.group(2))
        norm = lambda t: _re.sub(r"[^a-z0-9]", "", t.lower())
        for sec, b0, n in self.sections:
            if norm(sec) == norm(key):
                return b0 + max(0, n - last) if last else b0
        raise ValueError("no section %r in this cue's bar map (%s)" % (
            name, ", ".join(s[0] for s in self.sections) or "none"))

    def bar_of_smp(self, smp: int) -> tuple:
        """sample -> (bar 1-based, beat 1-based float)."""
        beats = Fraction(smp - self.offset_smp) / self.spb_smp
        bar = int(beats // self.meter) + 1
        return bar, float(beats - (bar - 1) * self.meter) + 1.0

    def section_at(self, bar: int) -> str:
        for name, b0, n in self.sections:
            if b0 <= bar < b0 + n:
                return name
        return ""

    def table(self, bars=None):
        """[(bar, sample, frame, section)] for the cue sheet."""
        out = []
        for b in range(1, (bars or self.bars) + 1):
            out.append((b, self.smp(b), self.frame(b), self.section_at(b)))
        return out

    def to_json(self) -> dict:
        return dict(bpm=self.bpm, meter=self.meter, bars=self.bars, sections=self.sections,
                    offset_smp=self.offset_smp, frames_per_bar=self.frames_per_bar,
                    smp_per_bar=float(self.spb_smp * self.meter), source=self.source,
                    bar_smp=[self.smp(b) for b in range(1, self.bars + 2)],
                    bar_frame=[self.frame(b) for b in range(1, self.bars + 2)])

    @classmethod
    def from_json(cls, d: dict) -> "BarMap":
        return cls(bpm=d["bpm"], meter=d.get("meter", 4), bars=d.get("bars", 0),
                   sections=[tuple(s) for s in d.get("sections", [])],
                   offset_smp=d.get("offset_smp", 0), source=d.get("source", ""))


def import_midi(path, offset_smp: int = 0) -> BarMap:
    """A master's `*_full.mid`: tempo, metre and `marker` sections -> BarMap."""
    import mido
    mf = mido.MidiFile(str(path))
    ppq = mf.ticks_per_beat
    tempo, num = 500000, 4
    marks, t, last_tick = [], 0, 0
    tempos = []
    for msg in mf.tracks[0]:
        t += msg.time
        last_tick = max(last_tick, t)
        if msg.type == "set_tempo":
            tempos.append((t, msg.tempo))
            tempo = msg.tempo if t == 0 else tempo
        elif msg.type == "time_signature" and t == 0:
            num = msg.numerator
        elif msg.type == "marker":
            marks.append((t / ppq, msg.text))
    if len({tp for _, tp in tempos}) > 1:
        raise ValueError(f"{path}: tempo changes inside a master are not supported")
    bpm = round(mido.tempo2bpm(tempo), 6)
    end_beats = max([t / ppq for t in [last_tick]] + [0.0])
    # bars: up to the last marker's end (the song's length is the last section's end)
    total_beats = 0.0
    for tr in mf.tracks[1:]:
        tt = 0
        for msg in tr:
            tt += msg.time
        total_beats = max(total_beats, tt / ppq)
    secs = []
    for i, (beat, name) in enumerate(marks):
        nxt = marks[i + 1][0] if i + 1 < len(marks) else None
        b0 = int(round(beat / num)) + 1
        n = int(round((nxt - beat) / num)) if nxt is not None else None
        secs.append([name, b0, n])
    bars = int(math.ceil(max(total_beats, end_beats) / num - 1e-9))
    if secs and secs[-1][2] is None:
        secs[-1][2] = max(1, bars - secs[-1][1] + 1)
    return BarMap(bpm=bpm, meter=num, bars=bars, sections=[tuple(s) for s in secs],
                  offset_smp=offset_smp, source=str(path))


def save(bm: BarMap, path) -> None:
    with open(path, "w", encoding="utf-8") as f:
        json.dump(bm.to_json(), f, indent=1)


def load(path) -> BarMap:
    with open(path, encoding="utf-8") as f:
        return BarMap.from_json(json.load(f))


_BAR = re.compile(r"^@?bar:(\d+)(?:\.(\d+(?:\.\d+)?))?$")


def parse_bar(text: str):
    """'@bar:17.3' -> (17, 3.0)."""
    m = _BAR.match(text.strip())
    if not m:
        raise ValueError(f"not a bar time: {text!r}")
    return int(m.group(1)), float(m.group(2)) if m.group(2) else 1.0
